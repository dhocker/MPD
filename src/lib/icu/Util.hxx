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

#ifndef MPD_ICU_UTIL_HXX
#define MPD_ICU_UTIL_HXX

#include "check.h"

#include <unicode/utypes.h>

template<typename T> struct ConstBuffer;
template<typename T> class AllocatedArray;
template<typename T> class AllocatedString;

/**
 * Wrapper for u_strFromUTF8().
 */
AllocatedArray<UChar>
UCharFromUTF8(const char *src);

/**
 * Wrapper for u_strToUTF8().
 */
AllocatedString<char>
UCharToUTF8(ConstBuffer<UChar> src);

/**
 * Wrapper for u_strToUTF8().
 *
 * Throws std::runtime_error on error.
 */
AllocatedString<char>
UCharToUTF8Throw(ConstBuffer<UChar> src);

#endif
