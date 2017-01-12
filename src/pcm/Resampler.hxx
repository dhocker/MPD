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

#ifndef MPD_PCM_RESAMPLER_HXX
#define MPD_PCM_RESAMPLER_HXX

#include "util/ConstBuffer.hxx"
#include "Compiler.h"

struct AudioFormat;

/**
 * This is an interface for plugins that convert PCM data to a
 * specific sample rate.
 */
class PcmResampler {
public:
	virtual ~PcmResampler() {}

	/**
	 * Opens the resampler, preparing it for Resample().
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param af the audio format of incoming data; the plugin may
	 * modify the object to enforce another input format (however,
	 * it may not request a different input sample rate)
	 * @param new_sample_rate the requested output sample rate
	 * @param error location to store the error
	 * @return the format of outgoing data
	 */
	virtual AudioFormat Open(AudioFormat &af,
				 unsigned new_sample_rate) = 0;

	/**
	 * Closes the resampler.  After that, you may call Open()
	 * again.
	 */
	virtual void Close() = 0;

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	virtual void Reset() {
	}

	/**
	 * Resamples a block of PCM data.
	 *
	 * @param src the input buffer
	 * @return the destination buffer (will be invalidated by
	 * filter_close() or filter_filter())
	 */
	gcc_pure
	virtual ConstBuffer<void> Resample(ConstBuffer<void> src) = 0;
};

#endif
