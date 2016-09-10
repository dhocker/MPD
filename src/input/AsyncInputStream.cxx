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
#include "AsyncInputStream.hxx"
#include "Domain.hxx"
#include "tag/Tag.hxx"
#include "thread/Cond.hxx"
#include "IOThread.hxx"

#include <assert.h>
#include <string.h>

AsyncInputStream::AsyncInputStream(const char *_url,
				   Mutex &_mutex, Cond &_cond,
				   size_t _buffer_size,
				   size_t _resume_at)
	:InputStream(_url, _mutex, _cond),
	 deferred_resume(io_thread_get(), BIND_THIS_METHOD(DeferredResume)),
	 deferred_seek(io_thread_get(), BIND_THIS_METHOD(DeferredSeek)),
	 allocation(_buffer_size),
	 buffer((uint8_t *)allocation.get(), _buffer_size),
	 resume_at(_resume_at),
	 open(true),
	 paused(false),
	 seek_state(SeekState::NONE),
	 tag(nullptr) {}

AsyncInputStream::~AsyncInputStream()
{
	delete tag;

	buffer.Clear();
}

void
AsyncInputStream::SetTag(Tag *_tag)
{
	delete tag;
	tag = _tag;
}

void
AsyncInputStream::Pause()
{
	assert(io_thread_inside());

	paused = true;
}

void
AsyncInputStream::PostponeError(Error &&error)
{
	assert(io_thread_inside());

	seek_state = SeekState::NONE;
	postponed_error = std::move(error);
	cond.broadcast();
}

inline void
AsyncInputStream::Resume()
{
	assert(io_thread_inside());

	if (paused) {
		paused = false;

		DoResume();
	}
}

bool
AsyncInputStream::Check(Error &error)
{
	if (postponed_exception) {
		auto e = std::move(postponed_exception);
		postponed_exception = std::exception_ptr();
		std::rethrow_exception(e);
	}

	bool success = !postponed_error.IsDefined();
	if (!success) {
		error = std::move(postponed_error);
		postponed_error.Clear();
	}

	return success;
}

bool
AsyncInputStream::IsEOF()
{
	return (KnownSize() && offset >= size) ||
		(!open && buffer.IsEmpty());
}

bool
AsyncInputStream::Seek(offset_type new_offset, Error &error)
{
	assert(IsReady());
	assert(seek_state == SeekState::NONE);

	if (new_offset == offset)
		/* no-op */
		return true;

	if (!IsSeekable()) {
		error.Set(input_domain, "Not seekable");
		return false;
	}

	/* check if we can fast-forward the buffer */

	while (new_offset > offset) {
		auto r = buffer.Read();
		if (r.IsEmpty())
			break;

		const size_t nbytes =
			new_offset - offset < (offset_type)r.size
					       ? new_offset - offset
					       : r.size;

		buffer.Consume(nbytes);
		offset += nbytes;
	}

	if (new_offset == offset)
		return true;

	/* no: ask the implementation to seek */

	seek_offset = new_offset;
	seek_state = SeekState::SCHEDULED;

	deferred_seek.Schedule();

	while (seek_state != SeekState::NONE)
		cond.wait(mutex);

	if (!Check(error))
		return false;

	return true;
}

void
AsyncInputStream::SeekDone()
{
	assert(io_thread_inside());
	assert(IsSeekPending());

	/* we may have reached end-of-file previously, and the
	   connection may have been closed already; however after
	   seeking successfully, the connection must be alive again */
	open = true;

	seek_state = SeekState::NONE;
	cond.broadcast();
}

Tag *
AsyncInputStream::ReadTag()
{
	Tag *result = tag;
	tag = nullptr;
	return result;
}

bool
AsyncInputStream::IsAvailable()
{
	return postponed_error.IsDefined() ||
		postponed_exception ||
		IsEOF() ||
		!buffer.IsEmpty();
}

size_t
AsyncInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	assert(!io_thread_inside());

	/* wait for data */
	CircularBuffer<uint8_t>::Range r;
	while (true) {
		if (!Check(error))
			return 0;

		r = buffer.Read();
		if (!r.IsEmpty() || IsEOF())
			break;

		cond.wait(mutex);
	}

	const size_t nbytes = std::min(read_size, r.size);
	memcpy(ptr, r.data, nbytes);
	buffer.Consume(nbytes);

	offset += (offset_type)nbytes;

	if (paused && buffer.GetSize() < resume_at)
		deferred_resume.Schedule();

	return nbytes;
}

void
AsyncInputStream::CommitWriteBuffer(size_t nbytes)
{
	buffer.Append(nbytes);

	if (!IsReady())
		SetReady();
	else
		cond.broadcast();
}

void
AsyncInputStream::AppendToBuffer(const void *data, size_t append_size)
{
	auto w = buffer.Write();
	assert(!w.IsEmpty());

	size_t nbytes = std::min(w.size, append_size);
	memcpy(w.data, data, nbytes);
	buffer.Append(nbytes);

	const size_t remaining = append_size - nbytes;
	if (remaining > 0) {
		w = buffer.Write();
		assert(!w.IsEmpty());
		assert(w.size >= remaining);

		memcpy(w.data, (const uint8_t *)data + nbytes, remaining);
		buffer.Append(remaining);
	}

	if (!IsReady())
		SetReady();
	else
		cond.broadcast();
}

void
AsyncInputStream::DeferredResume()
{
	const ScopeLock protect(mutex);

	try {
		Resume();
	} catch (...) {
		postponed_exception = std::current_exception();
		cond.broadcast();
	}
}

void
AsyncInputStream::DeferredSeek()
{
	const ScopeLock protect(mutex);
	if (seek_state != SeekState::SCHEDULED)
		return;

	try {
		Resume();

		seek_state = SeekState::PENDING;
		buffer.Clear();
		paused = false;

		DoSeek(seek_offset);
	} catch (...) {
		postponed_exception = std::current_exception();
		cond.broadcast();
	}
}
