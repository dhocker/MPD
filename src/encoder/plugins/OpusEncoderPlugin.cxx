/*
 * Copyright 2003-2016 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "OpusEncoderPlugin.hxx"
#include "OggEncoder.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/Alloc.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"

#include <opus.h>
#include <ogg/ogg.h>

#include <assert.h>
#include <stdlib.h>

namespace {

class OpusEncoder final : public OggEncoder {
	const AudioFormat audio_format;

	const size_t frame_size;

	const size_t buffer_frames, buffer_size;
	size_t buffer_position = 0;
	uint8_t *const buffer;

	::OpusEncoder *const enc;

	unsigned char buffer2[1275 * 3 + 7];

	int lookahead;

	ogg_int64_t packetno = 0;

	ogg_int64_t granulepos;

public:
	OpusEncoder(AudioFormat &_audio_format, ::OpusEncoder *_enc);
	~OpusEncoder() override;

	/* virtual methods from class Encoder */
	bool End(Error &) override;
	bool Write(const void *data, size_t length, Error &) override;

	size_t Read(void *dest, size_t length) override;

private:
	bool DoEncode(bool eos, Error &error);
	bool WriteSilence(unsigned fill_frames, Error &error);

	void GenerateHead();
	void GenerateTags();
};

class PreparedOpusEncoder final : public PreparedEncoder {
	opus_int32 bitrate;
	int complexity;
	int signal;

public:
	bool Configure(const ConfigBlock &block, Error &error);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format, Error &) override;

	const char *GetMimeType() const override {
		return "audio/ogg";
	}
};

static constexpr Domain opus_encoder_domain("opus_encoder");

bool
PreparedOpusEncoder::Configure(const ConfigBlock &block, Error &error)
{
	const char *value = block.GetBlockValue("bitrate", "auto");
	if (strcmp(value, "auto") == 0)
		bitrate = OPUS_AUTO;
	else if (strcmp(value, "max") == 0)
		bitrate = OPUS_BITRATE_MAX;
	else {
		char *endptr;
		bitrate = strtoul(value, &endptr, 10);
		if (endptr == value || *endptr != 0 ||
		    bitrate < 500 || bitrate > 512000) {
			error.Set(config_domain, "Invalid bit rate");
			return false;
		}
	}

	complexity = block.GetBlockValue("complexity", 10u);
	if (complexity > 10) {
		error.Format(config_domain, "Invalid complexity");
		return false;
	}

	value = block.GetBlockValue("signal", "auto");
	if (strcmp(value, "auto") == 0)
		signal = OPUS_AUTO;
	else if (strcmp(value, "voice") == 0)
		signal = OPUS_SIGNAL_VOICE;
	else if (strcmp(value, "music") == 0)
		signal = OPUS_SIGNAL_MUSIC;
	else {
		error.Format(config_domain, "Invalid signal");
		return false;
	}

	return true;
}

static PreparedEncoder *
opus_encoder_init(const ConfigBlock &block, Error &error)
{
	auto *encoder = new PreparedOpusEncoder();

	/* load configuration from "block" */
	if (!encoder->Configure(block, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return encoder;
}

OpusEncoder::OpusEncoder(AudioFormat &_audio_format, ::OpusEncoder *_enc)
	:OggEncoder(false),
	 audio_format(_audio_format),
	 frame_size(_audio_format.GetFrameSize()),
	 buffer_frames(_audio_format.sample_rate / 50),
	 buffer_size(frame_size * buffer_frames),
	 buffer((unsigned char *)xalloc(buffer_size)),
	 enc(_enc)
{
	opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&lookahead));
}

Encoder *
PreparedOpusEncoder::Open(AudioFormat &audio_format, Error &error)
{
	/* libopus supports only 48 kHz */
	audio_format.sample_rate = 48000;

	if (audio_format.channels > 2)
		audio_format.channels = 1;

	switch (audio_format.format) {
	case SampleFormat::S16:
	case SampleFormat::FLOAT:
		break;

	case SampleFormat::S8:
		audio_format.format = SampleFormat::S16;
		break;

	default:
		audio_format.format = SampleFormat::FLOAT;
		break;
	}

	int error_code;
	auto *enc = opus_encoder_create(audio_format.sample_rate,
					audio_format.channels,
					OPUS_APPLICATION_AUDIO,
					&error_code);
	if (enc == nullptr) {
		error.Set(opus_encoder_domain, error_code,
			  opus_strerror(error_code));
		return nullptr;
	}

	opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
	opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
	opus_encoder_ctl(enc, OPUS_SET_SIGNAL(signal));

	return new OpusEncoder(audio_format, enc);
}

OpusEncoder::~OpusEncoder()
{
	free(buffer);
	opus_encoder_destroy(enc);
}

bool
OpusEncoder::DoEncode(bool eos, Error &error)
{
	assert(buffer_position == buffer_size);

	opus_int32 result =
		audio_format.format == SampleFormat::S16
		? opus_encode(enc,
			      (const opus_int16 *)buffer,
			      buffer_frames,
			      buffer2,
			      sizeof(buffer2))
		: opus_encode_float(enc,
				    (const float *)buffer,
				    buffer_frames,
				    buffer2,
				    sizeof(buffer2));
	if (result < 0) {
		error.Set(opus_encoder_domain, "Opus encoder error");
		return false;
	}

	granulepos += buffer_frames;

	ogg_packet packet;
	packet.packet = buffer2;
	packet.bytes = result;
	packet.b_o_s = false;
	packet.e_o_s = eos;
	packet.granulepos = granulepos;
	packet.packetno = packetno++;
	stream.PacketIn(packet);

	buffer_position = 0;

	return true;
}

bool
OpusEncoder::End(Error &error)
{
	Flush();

	memset(buffer + buffer_position, 0,
	       buffer_size - buffer_position);
	buffer_position = buffer_size;

	return DoEncode(true, error);
}

bool
OpusEncoder::WriteSilence(unsigned fill_frames, Error &error)
{
	size_t fill_bytes = fill_frames * frame_size;

	while (fill_bytes > 0) {
		size_t nbytes = buffer_size - buffer_position;
		if (nbytes > fill_bytes)
			nbytes = fill_bytes;

		memset(buffer + buffer_position, 0, nbytes);
		buffer_position += nbytes;
		fill_bytes -= nbytes;

		if (buffer_position == buffer_size &&
		    !DoEncode(false, error))
			return false;
	}

	return true;
}

bool
OpusEncoder::Write(const void *_data, size_t length, Error &error)
{
	const uint8_t *data = (const uint8_t *)_data;

	if (lookahead > 0) {
		/* generate some silence at the beginning of the
		   stream */

		assert(buffer_position == 0);

		if (!WriteSilence(lookahead, error))
			return false;

		lookahead = 0;
	}

	while (length > 0) {
		size_t nbytes = buffer_size - buffer_position;
		if (nbytes > length)
			nbytes = length;

		memcpy(buffer + buffer_position, data, nbytes);
		data += nbytes;
		length -= nbytes;
		buffer_position += nbytes;

		if (buffer_position == buffer_size &&
		    !DoEncode(false, error))
			return false;
	}

	return true;
}

void
OpusEncoder::GenerateHead()
{
	unsigned char header[19];
	memcpy(header, "OpusHead", 8);
	header[8] = 1;
	header[9] = audio_format.channels;
	*(uint16_t *)(header + 10) = ToLE16(lookahead);
	*(uint32_t *)(header + 12) = ToLE32(audio_format.sample_rate);
	header[16] = 0;
	header[17] = 0;
	header[18] = 0;

	ogg_packet packet;
	packet.packet = header;
	packet.bytes = 19;
	packet.b_o_s = true;
	packet.e_o_s = false;
	packet.granulepos = 0;
	packet.packetno = packetno++;
	stream.PacketIn(packet);
	Flush();
}

void
OpusEncoder::GenerateTags()
{
	const char *version = opus_get_version_string();
	size_t version_length = strlen(version);

	size_t comments_size = 8 + 4 + version_length + 4;
	unsigned char *comments = (unsigned char *)xalloc(comments_size);
	memcpy(comments, "OpusTags", 8);
	*(uint32_t *)(comments + 8) = ToLE32(version_length);
	memcpy(comments + 12, version, version_length);
	*(uint32_t *)(comments + 12 + version_length) = ToLE32(0);

	ogg_packet packet;
	packet.packet = comments;
	packet.bytes = comments_size;
	packet.b_o_s = false;
	packet.e_o_s = false;
	packet.granulepos = 0;
	packet.packetno = packetno++;
	stream.PacketIn(packet);
	Flush();

	free(comments);
}

size_t
OpusEncoder::Read(void *dest, size_t length)
{
	if (packetno == 0)
		GenerateHead();
	else if (packetno == 1)
		GenerateTags();

	return OggEncoder::Read(dest, length);
}

}

const EncoderPlugin opus_encoder_plugin = {
	"opus",
	opus_encoder_init,
};
