/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "SocketMonitor.hxx"
#include "Loop.hxx"

#include <assert.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

void
SocketMonitor::Dispatch(unsigned flags)
{
	flags &= GetScheduledFlags();

	if (flags != 0 && !OnSocketReady(flags) && IsDefined())
		Cancel();
}

SocketMonitor::~SocketMonitor()
{
	if (IsDefined())
		Cancel();
}

void
SocketMonitor::Open(SocketDescriptor _fd)
{
	assert(!fd.IsDefined());
	assert(_fd.IsDefined());

	fd = _fd;
}

SocketDescriptor
SocketMonitor::Steal()
{
	assert(IsDefined());

	Cancel();

	return std::exchange(fd, SocketDescriptor::Undefined());
}

void
SocketMonitor::Abandon()
{
	assert(IsDefined());

	loop.Abandon(std::exchange(fd, SocketDescriptor::Undefined()).Get(),
		     *this);
}

void
SocketMonitor::Close()
{
	Steal().Close();
}

void
SocketMonitor::Schedule(unsigned flags)
{
	assert(IsDefined());

	if (flags == GetScheduledFlags())
		return;

	if (scheduled_flags == 0)
		loop.AddFD(fd.Get(), flags, *this);
	else if (flags == 0)
		loop.RemoveFD(fd.Get(), *this);
	else
		loop.ModifyFD(fd.Get(), flags, *this);

	scheduled_flags = flags;
}

SocketMonitor::ssize_t
SocketMonitor::Read(void *data, size_t length)
{
	assert(IsDefined());

	int flags = 0;
#ifdef MSG_DONTWAIT
	flags |= MSG_DONTWAIT;
#endif

	return recv(Get().Get(), (char *)data, length, flags);
}

SocketMonitor::ssize_t
SocketMonitor::Write(const void *data, size_t length)
{
	assert(IsDefined());

	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
	flags |= MSG_DONTWAIT;
#endif

	return send(Get().Get(), (const char *)data, length, flags);
}
