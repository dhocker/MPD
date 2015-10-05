/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_APE_LOADER_HXX
#define MPD_APE_LOADER_HXX

#include "check.h"

#include <functional>

#include <stddef.h>

struct StringView;
class Path;

typedef std::function<bool(unsigned long flags, const char *key,
			   StringView value)> ApeTagCallback;

/**
 * Scans the APE tag values from a file.
 *
 * @param path_fs the path of the file in filesystem encoding
 * @return false if the file could not be opened or if no APE tag is
 * present
 */
bool
tag_ape_scan(Path path_fs, ApeTagCallback callback);

#endif
