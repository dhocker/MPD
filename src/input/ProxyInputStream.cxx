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
#include "ProxyInputStream.hxx"
#include "tag/Tag.hxx"

#include <stdexcept>

ProxyInputStream::ProxyInputStream(InputStreamPtr _input) noexcept
	:InputStream(_input->GetURI(), _input->mutex),
	 input(std::move(_input))
{
	assert(input);

	input->SetHandler(this);
}

ProxyInputStream::~ProxyInputStream() noexcept = default;

void
ProxyInputStream::SetInput(InputStreamPtr _input) noexcept
{
	assert(!input);
	assert(_input);

	input = std::move(_input);
	input->SetHandler(this);

	/* this call wakes up client threads if the new input is
	   ready */
	CopyAttributes();

	set_input_cond.signal();
}

void
ProxyInputStream::CopyAttributes()
{
	assert(input);

	if (input->IsReady()) {
		if (!IsReady()) {
			if (input->HasMimeType())
				SetMimeType(input->GetMimeType());

			size = input->KnownSize()
				? input->GetSize()
				: UNKNOWN_SIZE;

			seekable = input->IsSeekable();
			SetReady();
		}

		offset = input->GetOffset();
	}
}

void
ProxyInputStream::Check()
{
	if (input)
		input->Check();
}

void
ProxyInputStream::Update() noexcept
{
	if (!input)
		return;

	input->Update();
	CopyAttributes();
}

void
ProxyInputStream::Seek(offset_type new_offset)
{
	while (!input)
		set_input_cond.wait(mutex);

	input->Seek(new_offset);
	CopyAttributes();
}

bool
ProxyInputStream::IsEOF() noexcept
{
	return input && input->IsEOF();
}

std::unique_ptr<Tag>
ProxyInputStream::ReadTag()
{
	if (!input)
		return nullptr;

	return input->ReadTag();
}

bool
ProxyInputStream::IsAvailable() noexcept
{
	return input && input->IsAvailable();
}

size_t
ProxyInputStream::Read(void *ptr, size_t read_size)
{
	while (!input)
		set_input_cond.wait(mutex);

	size_t nbytes = input->Read(ptr, read_size);
	CopyAttributes();
	return nbytes;
}
