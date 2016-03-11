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

#ifndef MPD_EVENT_MASK_MONITOR_HXX
#define MPD_EVENT_MASK_MONITOR_HXX

#include "check.h"
#include "DeferredMonitor.hxx"
#include "util/BoundMethod.hxx"

#include <atomic>

/**
 * Manage a bit mask of events that have occurred.  Every time the
 * mask becomes non-zero, OnMask() is called in #EventLoop's thread.
 *
 * This class is thread-safe.
 */
class MaskMonitor : DeferredMonitor {
	std::atomic_uint pending_mask;

public:
	explicit MaskMonitor(EventLoop &_loop)
		:DeferredMonitor(_loop), pending_mask(0) {}

	using DeferredMonitor::GetEventLoop;
	using DeferredMonitor::Cancel;

	void OrMask(unsigned new_mask);

protected:
	virtual void HandleMask(unsigned mask) = 0;

	/* virtual methode from class DeferredMonitor */
	void RunDeferred() override;
};

/**
 * A variant of #MaskMonitor which invokes a bound method.
 */
template<typename T>
class CallbackMaskMonitor final : public MaskMonitor {
	BoundMethod<T, void, unsigned> callback;

public:
	template<typename... Args>
	explicit CallbackMaskMonitor(EventLoop &_loop, Args&&... args)
		:MaskMonitor(_loop), callback(std::forward<Args>(args)...) {}

protected:
	void HandleMask(unsigned mask) override {
		callback(mask);
	}
};

#endif
