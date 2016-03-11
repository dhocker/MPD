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
#include "Partition.hxx"
#include "Instance.hxx"
#include "DetachedSong.hxx"
#include "mixer/Volume.hxx"
#include "IdleFlags.hxx"

Partition::Partition(Instance &_instance,
		     unsigned max_length,
		     unsigned buffer_chunks,
		     unsigned buffered_before_play)
	:instance(_instance),
	 global_events(instance.event_loop, *this, &Partition::OnGlobalEvent),
	 playlist(max_length, *this),
	 outputs(*this),
	 pc(*this, outputs, buffer_chunks, buffered_before_play)
{
}

void
Partition::EmitIdle(unsigned mask)
{
	instance.EmitIdle(mask);
}

#ifdef ENABLE_DATABASE

const Database *
Partition::GetDatabase(Error &error) const
{
	return instance.GetDatabase(error);
}

void
Partition::DatabaseModified(const Database &db)
{
	playlist.DatabaseModified(db);
	EmitIdle(IDLE_DATABASE);
}

#endif

void
Partition::TagModified()
{
	DetachedSong *song = pc.LockReadTaggedSong();
	if (song != nullptr) {
		playlist.TagModified(std::move(*song));
		delete song;
	}
}

void
Partition::SyncWithPlayer()
{
	playlist.SyncWithPlayer(pc);
}

void
Partition::OnQueueModified()
{
	EmitIdle(IDLE_PLAYLIST);
}

void
Partition::OnQueueOptionsChanged()
{
	EmitIdle(IDLE_OPTIONS);
}

void
Partition::OnQueueSongStarted()
{
	EmitIdle(IDLE_PLAYER);
}

void
Partition::OnPlayerSync()
{
	EmitGlobalEvent(SYNC_WITH_PLAYER);
}

void
Partition::OnPlayerTagModified()
{
	EmitGlobalEvent(TAG_MODIFIED);
}

void
Partition::OnMixerVolumeChanged(gcc_unused Mixer &mixer, gcc_unused int volume)
{
	InvalidateHardwareVolume();

	/* notify clients */
	EmitIdle(IDLE_MIXER);
}

void
Partition::OnGlobalEvent(unsigned mask)
{
	if ((mask & TAG_MODIFIED) != 0)
		TagModified();

	if ((mask & SYNC_WITH_PLAYER) != 0)
		SyncWithPlayer();
}
