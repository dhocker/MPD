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
#include "Param.hxx"
#include "ConfigPath.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Error.hxx"

#include <stdexcept>

ConfigParam::ConfigParam(const char *_value, int _line)
	:next(nullptr), value(_value), line(_line), used(false) {}

ConfigParam::~ConfigParam()
{
	delete next;
}

AllocatedPath
ConfigParam::GetPath(Error &error) const
{
	auto path = ParsePath(value.c_str(), error);
	if (gcc_unlikely(path.IsNull()))
		error.FormatPrefix("Invalid path at line %i: ", line);

	return path;

}

AllocatedPath
ConfigParam::GetPath() const
{
	Error error;
	auto path = ParsePath(value.c_str(), error);
	if (gcc_unlikely(path.IsNull()))
		throw std::runtime_error(error.GetMessage());

	return path;

}
