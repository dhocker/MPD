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
#include "MpcdecDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "pcm/Traits.hxx"
#include "tag/TagHandler.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "util/Clamp.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

#include <mpc/mpcdec.h>

#include <stdexcept>

#include <math.h>

struct mpc_decoder_data {
	InputStream &is;
	DecoderClient *client;

	mpc_decoder_data(InputStream &_is, DecoderClient *_client)
		:is(_is), client(_client) {}
};

static constexpr Domain mpcdec_domain("mpcdec");

static constexpr SampleFormat mpcdec_sample_format = SampleFormat::S24_P32;
typedef SampleTraits<mpcdec_sample_format> MpcdecSampleTraits;

static mpc_int32_t
mpc_read_cb(mpc_reader *reader, void *ptr, mpc_int32_t size)
{
	struct mpc_decoder_data *data =
		(struct mpc_decoder_data *)reader->data;

	return decoder_read(data->client, data->is, ptr, size);
}

static mpc_bool_t
mpc_seek_cb(mpc_reader *reader, mpc_int32_t offset)
{
	struct mpc_decoder_data *data =
		(struct mpc_decoder_data *)reader->data;

	try {
		data->is.LockSeek(offset);
		return true;
	} catch (const std::runtime_error &) {
		return false;
	}
}

static mpc_int32_t
mpc_tell_cb(mpc_reader *reader)
{
	struct mpc_decoder_data *data =
		(struct mpc_decoder_data *)reader->data;

	return (long)data->is.GetOffset();
}

static mpc_bool_t
mpc_canseek_cb(mpc_reader *reader)
{
	struct mpc_decoder_data *data =
		(struct mpc_decoder_data *)reader->data;

	return data->is.IsSeekable();
}

static mpc_int32_t
mpc_getsize_cb(mpc_reader *reader)
{
	struct mpc_decoder_data *data =
		(struct mpc_decoder_data *)reader->data;

	if (!data->is.KnownSize())
		return -1;

	return data->is.GetSize();
}

/* this _looks_ performance-critical, don't de-inline -- eric */
static inline MpcdecSampleTraits::value_type
mpc_to_mpd_sample(MPC_SAMPLE_FORMAT sample)
{
	/* only doing 16-bit audio for now */
	MpcdecSampleTraits::value_type val;

	constexpr int bits = MpcdecSampleTraits::BITS;
	constexpr auto clip_min = MpcdecSampleTraits::MIN;
	constexpr auto clip_max = MpcdecSampleTraits::MAX;

#ifdef MPC_FIXED_POINT
	const int shift = bits - MPC_FIXED_POINT_SCALE_SHIFT;

	if (shift < 0)
		val = sample >> -shift;
	else
		val = sample << shift;
#else
	const int float_scale = 1 << (bits - 1);

	val = sample * float_scale;
#endif

	return Clamp(val, clip_min, clip_max);
}

static void
mpc_to_mpd_buffer(MpcdecSampleTraits::pointer_type dest,
		  const MPC_SAMPLE_FORMAT *src,
		  unsigned num_samples)
{
	while (num_samples-- > 0)
		*dest++ = mpc_to_mpd_sample(*src++);
}

static void
mpcdec_decode(DecoderClient &client, InputStream &is)
{
	mpc_decoder_data data(is, &client);

	mpc_reader reader;
	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_demux *demux = mpc_demux_init(&reader);
	if (demux == nullptr) {
		if (client.GetCommand() != DecoderCommand::STOP)
			LogWarning(mpcdec_domain,
				   "Not a valid musepack stream");
		return;
	}

	AtScopeExit(demux) { mpc_demux_exit(demux); };

	mpc_streaminfo info;
	mpc_demux_get_info(demux, &info);

	auto audio_format = CheckAudioFormat(info.sample_freq,
					     mpcdec_sample_format,
					     info.channels);

	ReplayGainInfo rgi;
	rgi.Clear();
	rgi.album.gain = MPC_OLD_GAIN_REF  - (info.gain_album  / 256.);
	rgi.album.peak = pow(10, info.peak_album / 256. / 20) / 32767;
	rgi.track.gain = MPC_OLD_GAIN_REF  - (info.gain_title  / 256.);
	rgi.track.peak = pow(10, info.peak_title / 256. / 20) / 32767;

	client.SubmitReplayGain(&rgi);

	client.Ready(audio_format, is.IsSeekable(),
		     SongTime::FromS(mpc_streaminfo_get_length(&info)));

	DecoderCommand cmd = DecoderCommand::NONE;
	do {
		if (cmd == DecoderCommand::SEEK) {
			mpc_int64_t where = client.GetSeekFrame();
			bool success;

			success = mpc_demux_seek_sample(demux, where)
				== MPC_STATUS_OK;
			if (success)
				client.CommandFinished();
			else
				client.SeekError();
		}

		MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];
		mpc_frame_info frame;
		frame.buffer = (MPC_SAMPLE_FORMAT *)sample_buffer;
		mpc_status status = mpc_demux_decode(demux, &frame);
		if (status != MPC_STATUS_OK) {
			LogWarning(mpcdec_domain,
				   "Failed to decode sample");
			break;
		}

		if (frame.bits == -1)
			break;

		mpc_uint32_t ret = frame.samples;
		ret *= info.channels;

		MpcdecSampleTraits::value_type chunk[ARRAY_SIZE(sample_buffer)];
		mpc_to_mpd_buffer(chunk, sample_buffer, ret);

		long bit_rate = unsigned(frame.bits) * audio_format.sample_rate
			/ (1000 * frame.samples);

		cmd = client.SubmitData(is,
					chunk, ret * sizeof(chunk[0]),
					bit_rate);
	} while (cmd != DecoderCommand::STOP);
}

static SignedSongTime
mpcdec_get_file_duration(InputStream &is)
{
	mpc_decoder_data data(is, nullptr);

	mpc_reader reader;
	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_demux *demux = mpc_demux_init(&reader);
	if (demux == nullptr)
		return SignedSongTime::Negative();

	AtScopeExit(demux) { mpc_demux_exit(demux); };

	mpc_streaminfo info;
	mpc_demux_get_info(demux, &info);

	return SongTime::FromS(mpc_streaminfo_get_length(&info));
}

static bool
mpcdec_scan_stream(InputStream &is,
		   const TagHandler &handler, void *handler_ctx)
{
	const auto duration = mpcdec_get_file_duration(is);
	if (duration.IsNegative())
		return false;

	tag_handler_invoke_duration(handler, handler_ctx, SongTime(duration));
	return true;
}

static const char *const mpcdec_suffixes[] = { "mpc", nullptr };

const struct DecoderPlugin mpcdec_decoder_plugin = {
	"mpcdec",
	nullptr,
	nullptr,
	mpcdec_decode,
	nullptr,
	nullptr,
	mpcdec_scan_stream,
	nullptr,
	mpcdec_suffixes,
	nullptr,
};
