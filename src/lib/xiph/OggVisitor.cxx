/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "OggVisitor.hxx"

#include <stdexcept>

void
OggVisitor::EndStream()
{
	if (!has_stream)
		return;

	has_stream = false;
	OnOggEnd();
}

inline bool
OggVisitor::ReadNextPage()
{
	ogg_page page;
	if (!sync.ExpectPage(page))
		return false;

	const auto page_serialno = ogg_page_serialno(&page);
	if (page_serialno != stream.GetSerialNo()) {
		EndStream();
		stream.Reinitialize(page_serialno);
	}

	stream.PageIn(page);
	return true;
}

inline void
OggVisitor::HandlePacket(const ogg_packet &packet)
{
	if (packet.b_o_s) {
		EndStream();
		has_stream = true;
		OnOggBeginning(packet);
		return;
	}

	if (!has_stream)
		/* fail if BOS is missing */
		throw std::runtime_error("BOS packet expected");

	if (packet.e_o_s) {
		EndStream();
		return;
	}

	OnOggPacket(packet);
}

inline void
OggVisitor::HandlePackets()
{
	ogg_packet packet;
	while (stream.PacketOut(packet) == 1)
		HandlePacket(packet);
}

void
OggVisitor::Visit()
{
	do {
		HandlePackets();
	} while (ReadNextPage());
}

void
OggVisitor::PostSeek()
{
	sync.Reset();

	/* reset the stream to clear any previous partial packet
	   data */
	stream.Reset();

	/* find the next Ogg page and feed it into the stream */
	sync.ExpectPageSeekIn(stream);
}
