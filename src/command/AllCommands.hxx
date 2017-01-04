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

#ifndef MPD_ALL_COMMANDS_HXX
#define MPD_ALL_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

void
command_init();

void
command_finish();

CommandResult
command_process(Client &client, unsigned num, char *line);

void 
insert_command(
	const char *cmd,
	unsigned permission,
	int min,
	int max,
	CommandResult (*handler)(Client &client, Request request, Response &response));

bool
	remove_command(const char *cmd);

#endif
