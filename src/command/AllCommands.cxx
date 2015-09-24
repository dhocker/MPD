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

#include "config.h"
#include "AllCommands.hxx"
#include "Request.hxx"
#include "QueueCommands.hxx"
#include "TagCommands.hxx"
#include "PlayerCommands.hxx"
#include "PlaylistCommands.hxx"
#include "StorageCommands.hxx"
#include "DatabaseCommands.hxx"
#include "FileCommands.hxx"
#include "OutputCommands.hxx"
#include "MessageCommands.hxx"
#include "NeighborCommands.hxx"
#include "OtherCommands.hxx"
#include "Permission.hxx"
#include "tag/TagType.h"
#include "Partition.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/Macros.hxx"
#include "util/Tokenizer.hxx"
#include "util/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringAPI.hxx"

#ifdef ENABLE_SQLITE
#include "StickerCommands.hxx"
#include "sticker/StickerDatabase.hxx"
#endif

#include <assert.h>
#include <string>
#include <map>

/*
 * The most we ever use is for search/find, and that limits it to the
 * number of tags we can have.  Add one for the command, and one extra
 * to catch errors clients may send us
 */
#define COMMAND_ARGV_MAX	(2+(TAG_NUM_OF_ITEM_TYPES*2))

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct command {
	const char *cmd;
	unsigned permission;
	int min;
	int max;
	CommandResult (*handler)(Client &client, Request request, Response &response);
};

/* don't be fooled, this is the command handler for "commands" command */
static CommandResult
handle_commands(Client &client, Request request, Response &response);

static CommandResult
handle_not_commands(Client &client, Request request, Response &response);

#ifdef no_map_use	
/**
 * The command registry.
 *
 * This array must be sorted!
 * If the std::map change is used, ordering is not required. However, it
 * does make the list easier to manage.
 */
static constexpr struct command mpd_commands[] = {
	{ "add", PERMISSION_ADD, 1, 1, handle_add },
	{ "addid", PERMISSION_ADD, 1, 2, handle_addid },
	{ "addtagid", PERMISSION_ADD, 3, 3, handle_addtagid },
	{ "channels", PERMISSION_READ, 0, 0, handle_channels },
	{ "clear", PERMISSION_CONTROL, 0, 0, handle_clear },
	{ "clearerror", PERMISSION_CONTROL, 0, 0, handle_clearerror },
	{ "cleartagid", PERMISSION_ADD, 1, 2, handle_cleartagid },
	{ "close", PERMISSION_NONE, -1, -1, handle_close },
	{ "commands", PERMISSION_NONE, 0, 0, handle_commands },
	{ "config", PERMISSION_ADMIN, 0, 0, handle_config },
	{ "consume", PERMISSION_CONTROL, 1, 1, handle_consume },
#ifdef ENABLE_DATABASE
	{ "count", PERMISSION_READ, 2, -1, handle_count },
#endif
	{ "crossfade", PERMISSION_CONTROL, 1, 1, handle_crossfade },
	{ "currentsong", PERMISSION_READ, 0, 0, handle_currentsong },
	{ "decoders", PERMISSION_READ, 0, 0, handle_decoders },
	{ "delete", PERMISSION_CONTROL, 1, 1, handle_delete },
	{ "deleteid", PERMISSION_CONTROL, 1, 1, handle_deleteid },
	{ "disableoutput", PERMISSION_ADMIN, 1, 1, handle_disableoutput },
	{ "enableoutput", PERMISSION_ADMIN, 1, 1, handle_enableoutput },
#ifdef ENABLE_DATABASE
	{ "find", PERMISSION_READ, 2, -1, handle_find },
	{ "findadd", PERMISSION_ADD, 2, -1, handle_findadd},
#endif
	{ "idle", PERMISSION_READ, 0, -1, handle_idle },
	{ "kill", PERMISSION_ADMIN, -1, -1, handle_kill },
#ifdef ENABLE_DATABASE
	{ "list", PERMISSION_READ, 1, -1, handle_list },
	{ "listall", PERMISSION_READ, 0, 1, handle_listall },
	{ "listallinfo", PERMISSION_READ, 0, 1, handle_listallinfo },
#endif
	{ "listfiles", PERMISSION_READ, 0, 1, handle_listfiles },
#ifdef ENABLE_DATABASE
	{ "listmounts", PERMISSION_READ, 0, 0, handle_listmounts },
#endif
#ifdef ENABLE_NEIGHBOR_PLUGINS
	{ "listneighbors", PERMISSION_READ, 0, 0, handle_listneighbors },
#endif
	{ "listplaylist", PERMISSION_READ, 1, 1, handle_listplaylist },
	{ "listplaylistinfo", PERMISSION_READ, 1, 1, handle_listplaylistinfo },
	{ "listplaylists", PERMISSION_READ, 0, 0, handle_listplaylists },
	{ "load", PERMISSION_ADD, 1, 2, handle_load },
	{ "lsinfo", PERMISSION_READ, 0, 1, handle_lsinfo },
	{ "mixrampdb", PERMISSION_CONTROL, 1, 1, handle_mixrampdb },
	{ "mixrampdelay", PERMISSION_CONTROL, 1, 1, handle_mixrampdelay },
#ifdef ENABLE_DATABASE
	{ "mount", PERMISSION_ADMIN, 2, 2, handle_mount },
#endif
	{ "move", PERMISSION_CONTROL, 2, 2, handle_move },
	{ "moveid", PERMISSION_CONTROL, 2, 2, handle_moveid },
	{ "next", PERMISSION_CONTROL, 0, 0, handle_next },
	{ "notcommands", PERMISSION_NONE, 0, 0, handle_not_commands },
	{ "outputs", PERMISSION_READ, 0, 0, handle_devices },
	{ "password", PERMISSION_NONE, 1, 1, handle_password },
	{ "pause", PERMISSION_CONTROL, 0, 1, handle_pause },
	{ "ping", PERMISSION_NONE, 0, 0, handle_ping },
	{ "play", PERMISSION_CONTROL, 0, 1, handle_play },
	{ "playid", PERMISSION_CONTROL, 0, 1, handle_playid },
	{ "playlist", PERMISSION_READ, 0, 0, handle_playlist },
	{ "playlistadd", PERMISSION_CONTROL, 2, 2, handle_playlistadd },
	{ "playlistclear", PERMISSION_CONTROL, 1, 1, handle_playlistclear },
	{ "playlistdelete", PERMISSION_CONTROL, 2, 2, handle_playlistdelete },
	{ "playlistfind", PERMISSION_READ, 2, -1, handle_playlistfind },
	{ "playlistid", PERMISSION_READ, 0, 1, handle_playlistid },
	{ "playlistinfo", PERMISSION_READ, 0, 1, handle_playlistinfo },
	{ "playlistmove", PERMISSION_CONTROL, 3, 3, handle_playlistmove },
	{ "playlistsearch", PERMISSION_READ, 2, -1, handle_playlistsearch },
	{ "plchanges", PERMISSION_READ, 1, 1, handle_plchanges },
	{ "plchangesposid", PERMISSION_READ, 1, 1, handle_plchangesposid },
	{ "previous", PERMISSION_CONTROL, 0, 0, handle_previous },
	{ "prio", PERMISSION_CONTROL, 2, -1, handle_prio },
	{ "prioid", PERMISSION_CONTROL, 2, -1, handle_prioid },
	{ "random", PERMISSION_CONTROL, 1, 1, handle_random },
	{ "rangeid", PERMISSION_ADD, 2, 2, handle_rangeid },
	{ "readcomments", PERMISSION_READ, 1, 1, handle_read_comments },
	{ "readmessages", PERMISSION_READ, 0, 0, handle_read_messages },
	{ "rename", PERMISSION_CONTROL, 2, 2, handle_rename },
	{ "repeat", PERMISSION_CONTROL, 1, 1, handle_repeat },
	{ "replay_gain_mode", PERMISSION_CONTROL, 1, 1,
	  handle_replay_gain_mode },
	{ "replay_gain_status", PERMISSION_READ, 0, 0,
	  handle_replay_gain_status },
	{ "rescan", PERMISSION_CONTROL, 0, 1, handle_rescan },
	{ "rm", PERMISSION_CONTROL, 1, 1, handle_rm },
	{ "save", PERMISSION_CONTROL, 1, 1, handle_save },
#ifdef ENABLE_DATABASE
	{ "search", PERMISSION_READ, 2, -1, handle_search },
	{ "searchadd", PERMISSION_ADD, 2, -1, handle_searchadd },
	{ "searchaddpl", PERMISSION_CONTROL, 3, -1, handle_searchaddpl },
#endif
	{ "seek", PERMISSION_CONTROL, 2, 2, handle_seek },
	{ "seekcur", PERMISSION_CONTROL, 1, 1, handle_seekcur },
	{ "seekid", PERMISSION_CONTROL, 2, 2, handle_seekid },
	{ "sendmessage", PERMISSION_CONTROL, 2, 2, handle_send_message },
	{ "setvol", PERMISSION_CONTROL, 1, 1, handle_setvol },
	{ "shuffle", PERMISSION_CONTROL, 0, 1, handle_shuffle },
	{ "single", PERMISSION_CONTROL, 1, 1, handle_single },
	{ "stats", PERMISSION_READ, 0, 0, handle_stats },
	{ "status", PERMISSION_READ, 0, 0, handle_status },
#ifdef ENABLE_SQLITE
	{ "sticker", PERMISSION_ADMIN, 3, -1, handle_sticker },
#endif
	{ "stop", PERMISSION_CONTROL, 0, 0, handle_stop },
	{ "subscribe", PERMISSION_READ, 1, 1, handle_subscribe },
	{ "swap", PERMISSION_CONTROL, 2, 2, handle_swap },
	{ "swapid", PERMISSION_CONTROL, 2, 2, handle_swapid },
	{ "tagtypes", PERMISSION_READ, 0, 0, handle_tagtypes },
	{ "toggleoutput", PERMISSION_ADMIN, 1, 1, handle_toggleoutput },
#ifdef ENABLE_DATABASE
	{ "unmount", PERMISSION_ADMIN, 1, 1, handle_unmount },
#endif
	{ "unsubscribe", PERMISSION_READ, 1, 1, handle_unsubscribe },
	{ "update", PERMISSION_CONTROL, 0, 1, handle_update },
	{ "urlhandlers", PERMISSION_READ, 0, 0, handle_urlhandlers },
	{ "volume", PERMISSION_CONTROL, 1, 1, handle_volume },
};

static constexpr unsigned num_commands = ARRAY_SIZE(mpd_commands);
#endif

// Map of all commands. Maps are maitained in binary tree format,
// so we do not have to worry about keeping the list in order.
// Using a map for the command registry allows new commands to
// be added or removed at run time. One possible use of this
// dynamic ability might be to allow command handler plugins to be defined in
// the config file. 
static std::map<std::string, const command&> command_map;
typedef std::map<std::string, const command&>::iterator command_map_iterator;

static void 
insert_command(
	const char *cmd,
	unsigned permission,
	int min,
	int max,
	CommandResult (*handler)(Client &client, Request request, Response &response))
{
	// This instance is never freed. It is held in the map forever.
	// As a result, it could appear to be a memory leak when the app is shutdown.
	command *c = new command;
	c->cmd = cmd;
	c->permission = permission;
	c->min = min;
	c->max = max;
	c->handler = handler;
	std::string key(cmd);
	command_map.insert(std::pair<std::string, const command&>(key, *c));
}

static void
build_command_map() 
{
	// This sequence builds the command map - the registry of all MPD commands.
	// Each call adds one MPD command to the map. The sequence does not have
	// to be in any particular order. However, keeping in alphabetical order
	// is an aid to the reader/maintainer.
	insert_command("add", PERMISSION_ADD, 1, 1, handle_add);
	insert_command("addid", PERMISSION_ADD, 1, 2, handle_addid);
	insert_command("addtagid", PERMISSION_ADD, 3, 3, handle_addtagid);
	insert_command("channels", PERMISSION_READ, 0, 0, handle_channels);
	insert_command("clear", PERMISSION_CONTROL, 0, 0, handle_clear);
	insert_command("clearerror", PERMISSION_CONTROL, 0, 0, handle_clearerror);
	insert_command("cleartagid", PERMISSION_ADD, 1, 2, handle_cleartagid);
	insert_command("close", PERMISSION_NONE, -1, -1, handle_close);
	insert_command("commands", PERMISSION_NONE, 0, 0, handle_commands);
	insert_command("config", PERMISSION_ADMIN, 0, 0, handle_config);
	insert_command("consume", PERMISSION_CONTROL, 1, 1, handle_consume);
#ifdef ENABLE_DATABASE
	insert_command("count", PERMISSION_READ, 2, -1, handle_count);
#endif
	insert_command("crossfade", PERMISSION_CONTROL, 1, 1, handle_crossfade);
	insert_command("currentsong", PERMISSION_READ, 0, 0, handle_currentsong);
	insert_command("decoders", PERMISSION_READ, 0, 0, handle_decoders);
	insert_command("delete", PERMISSION_CONTROL, 1, 1, handle_delete);
	insert_command("deleteid", PERMISSION_CONTROL, 1, 1, handle_deleteid);
	insert_command("disableoutput", PERMISSION_ADMIN, 1, 1, handle_disableoutput);
	insert_command("enableoutput", PERMISSION_ADMIN, 1, 1, handle_enableoutput);
#ifdef ENABLE_DATABASE
	insert_command("find", PERMISSION_READ, 2, -1, handle_find);
	insert_command("findadd", PERMISSION_ADD, 2, -1, handle_findadd);
#endif
	insert_command("idle", PERMISSION_READ, 0, -1, handle_idle);
	insert_command("kill", PERMISSION_ADMIN, -1, -1, handle_kill);
#ifdef ENABLE_DATABASE
	insert_command("list", PERMISSION_READ, 1, -1, handle_list);
	insert_command("listall", PERMISSION_READ, 0, 1, handle_listall);
	insert_command("listallinfo", PERMISSION_READ, 0, 1, handle_listallinfo);
#endif
	insert_command("listfiles", PERMISSION_READ, 0, 1, handle_listfiles);
#ifdef ENABLE_DATABASE
	insert_command("listmounts", PERMISSION_READ, 0, 0, handle_listmounts);
#endif
#ifdef ENABLE_NEIGHBOR_PLUGINS
	insert_command("listneighbors", PERMISSION_READ, 0, 0, handle_listneighbors);
#endif
	insert_command("listplaylist", PERMISSION_READ, 1, 1, handle_listplaylist);
	insert_command("listplaylistinfo", PERMISSION_READ, 1, 1, handle_listplaylistinfo);
	insert_command("listplaylists", PERMISSION_READ, 0, 0, handle_listplaylists);
	insert_command("load", PERMISSION_ADD, 1, 2, handle_load);
	insert_command("lsinfo", PERMISSION_READ, 0, 1, handle_lsinfo);
	insert_command("mixrampdb", PERMISSION_CONTROL, 1, 1, handle_mixrampdb);
	insert_command("mixrampdelay", PERMISSION_CONTROL, 1, 1, handle_mixrampdelay);
#ifdef ENABLE_DATABASE
	insert_command("mount", PERMISSION_ADMIN, 2, 2, handle_mount);
#endif
	insert_command("move", PERMISSION_CONTROL, 2, 2, handle_move);
	insert_command("moveid", PERMISSION_CONTROL, 2, 2, handle_moveid);
	insert_command("next", PERMISSION_CONTROL, 0, 0, handle_next);
	insert_command("notcommands", PERMISSION_NONE, 0, 0, handle_not_commands);
	insert_command("outputs", PERMISSION_READ, 0, 0, handle_devices);
	insert_command("password", PERMISSION_NONE, 1, 1, handle_password);
	insert_command("pause", PERMISSION_CONTROL, 0, 1, handle_pause);
	insert_command("ping", PERMISSION_NONE, 0, 0, handle_ping);
	insert_command("play", PERMISSION_CONTROL, 0, 1, handle_play);
	insert_command("playid", PERMISSION_CONTROL, 0, 1, handle_playid);
	insert_command("playlist", PERMISSION_READ, 0, 0, handle_playlist);
	insert_command("playlistadd", PERMISSION_CONTROL, 2, 2, handle_playlistadd);
	insert_command("playlistclear", PERMISSION_CONTROL, 1, 1, handle_playlistclear);
	insert_command("playlistdelete", PERMISSION_CONTROL, 2, 2, handle_playlistdelete);
	insert_command("playlistfind", PERMISSION_READ, 2, -1, handle_playlistfind);
	insert_command("playlistid", PERMISSION_READ, 0, 1, handle_playlistid);
	insert_command("playlistinfo", PERMISSION_READ, 0, 1, handle_playlistinfo);
	insert_command("playlistmove", PERMISSION_CONTROL, 3, 3, handle_playlistmove);
	insert_command("playlistsearch", PERMISSION_READ, 2, -1, handle_playlistsearch);
	insert_command("plchanges", PERMISSION_READ, 1, 1, handle_plchanges);
	insert_command("plchangesposid", PERMISSION_READ, 1, 1, handle_plchangesposid);
	insert_command("previous", PERMISSION_CONTROL, 0, 0, handle_previous);
	insert_command("prio", PERMISSION_CONTROL, 2, -1, handle_prio);
	insert_command("prioid", PERMISSION_CONTROL, 2, -1, handle_prioid);
	insert_command("random", PERMISSION_CONTROL, 1, 1, handle_random);
	insert_command("rangeid", PERMISSION_ADD, 2, 2, handle_rangeid);
	insert_command("readcomments", PERMISSION_READ, 1, 1, handle_read_comments);
	insert_command("readmessages", PERMISSION_READ, 0, 0, handle_read_messages);
	insert_command("rename", PERMISSION_CONTROL, 2, 2, handle_rename);
	insert_command("repeat", PERMISSION_CONTROL, 1, 1, handle_repeat);
	insert_command("replay_gain_mode", PERMISSION_CONTROL, 1, 1,
	  handle_replay_gain_mode);
	insert_command("replay_gain_status", PERMISSION_READ, 0, 0,
	  handle_replay_gain_status);
	insert_command("rescan", PERMISSION_CONTROL, 0, 1, handle_rescan);
	insert_command("rm", PERMISSION_CONTROL, 1, 1, handle_rm);
	insert_command("save", PERMISSION_CONTROL, 1, 1, handle_save);
#ifdef ENABLE_DATABASE
	insert_command("search", PERMISSION_READ, 2, -1, handle_search);
	insert_command("searchadd", PERMISSION_ADD, 2, -1, handle_searchadd);
	insert_command("searchaddpl", PERMISSION_CONTROL, 3, -1, handle_searchaddpl);
#endif
	insert_command("seek", PERMISSION_CONTROL, 2, 2, handle_seek);
	insert_command("seekcur", PERMISSION_CONTROL, 1, 1, handle_seekcur);
	insert_command("seekid", PERMISSION_CONTROL, 2, 2, handle_seekid);
	insert_command("sendmessage", PERMISSION_CONTROL, 2, 2, handle_send_message);
	insert_command("setvol", PERMISSION_CONTROL, 1, 1, handle_setvol);
	insert_command("shuffle", PERMISSION_CONTROL, 0, 1, handle_shuffle);
	insert_command("single", PERMISSION_CONTROL, 1, 1, handle_single);
	insert_command("stats", PERMISSION_READ, 0, 0, handle_stats);
	insert_command("status", PERMISSION_READ, 0, 0, handle_status);
#ifdef ENABLE_SQLITE
	insert_command("sticker", PERMISSION_ADMIN, 3, -1, handle_sticker);
#endif
	insert_command("stop", PERMISSION_CONTROL, 0, 0, handle_stop);
	insert_command("subscribe", PERMISSION_READ, 1, 1, handle_subscribe);
	insert_command("swap", PERMISSION_CONTROL, 2, 2, handle_swap);
	insert_command("swapid", PERMISSION_CONTROL, 2, 2, handle_swapid);
	insert_command("tagtypes", PERMISSION_READ, 0, 0, handle_tagtypes);
	insert_command("toggleoutput", PERMISSION_ADMIN, 1, 1, handle_toggleoutput);
#ifdef ENABLE_DATABASE
	insert_command("unmount", PERMISSION_ADMIN, 1, 1, handle_unmount);
#endif
	insert_command("unsubscribe", PERMISSION_READ, 1, 1, handle_unsubscribe);
	insert_command("update", PERMISSION_CONTROL, 0, 1, handle_update);
	insert_command("urlhandlers", PERMISSION_READ, 0, 0, handle_urlhandlers);
	insert_command("volume", PERMISSION_CONTROL, 1, 1, handle_volume);
}

static bool
command_available(gcc_unused const Partition &partition,
		  gcc_unused const struct command &cmd)
{
#ifdef ENABLE_SQLITE
	if (StringIsEqual(cmd.cmd, "sticker"))
		return sticker_enabled();
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (StringIsEqual(cmd.cmd, "listneighbors"))
		return neighbor_commands_available(partition.instance);
#endif

	if (StringIsEqual(cmd.cmd, "save") ||
	    StringIsEqual(cmd.cmd, "rm") ||
	    StringIsEqual(cmd.cmd, "rename") ||
	    StringIsEqual(cmd.cmd, "playlistdelete") ||
	    StringIsEqual(cmd.cmd, "playlistmove") ||
	    StringIsEqual(cmd.cmd, "playlistclear") ||
	    StringIsEqual(cmd.cmd, "playlistadd") ||
	    StringIsEqual(cmd.cmd, "listplaylists"))
		return playlist_commands_available();

	return true;
}

static CommandResult
PrintAvailableCommands(Response &r, const Partition &partition,
		     unsigned permission)
{
#ifdef no_map_use
	for (unsigned i = 0; i < command_map.size(); ++i) {
		const struct command *cmd = &commands[i];

		if (cmd->permission == (permission & cmd->permission) &&
		    command_available(partition, cmd))
			r.Format("command: %s\n", cmd->cmd);
	}
#else
	for (command_map_iterator it = command_map.begin(); it != command_map.end(); it++) {
		if (it->second.permission == (permission & it->second.permission) &&
			command_available(partition, it->second)) {
			r.Format("command: %s\n", it->second.cmd);
		}
	}
#endif

	return CommandResult::OK;
}

static CommandResult
PrintUnavailableCommands(Response &r, unsigned permission)
{
#ifdef no_map_use
	for (unsigned i = 0; i < command_map.size(); ++i) {
		const struct command *cmd = &commands[i];

		if (cmd->permission != (permission & cmd->permission))
			r.Format("command: %s\n", cmd->cmd);
	}
#else
	for (command_map_iterator it = command_map.begin(); it != command_map.end(); it++) {
		if (it->second.permission != (permission & it->second.permission)) {
			r.Format("command: %s\n", it->second.cmd);
		}
	}
#endif

	return CommandResult::OK;
}

/* don't be fooled, this is the command handler for "commands" command */
static CommandResult
handle_commands(Client &client, gcc_unused Request request, Response &r)
{
	return PrintAvailableCommands(r, client.partition,
				      client.GetPermission());
}

static CommandResult
handle_not_commands(Client &client, gcc_unused Request request, Response &r)
{
	return PrintUnavailableCommands(r, client.GetPermission());
}

void
command_init()
{
	// Build a map of the commands. The map is automatically sorted by key.
	build_command_map();

#ifdef no_map_use	
#ifndef NDEBUG
	/* ensure that the command list is sorted */
	for (unsigned i = 0; i < command_map.size() - 1; ++i)
		assert(strcmp(mpd_commands[i].cmd, mpd_commands[i + 1].cmd) < 0);
#endif
#endif
}

void
command_finish()
{
}

static const struct command *
command_lookup(const char *name)
{
#ifdef no_map_use	
	unsigned a = 0, b = command_map.size(), i;

	/* binary search */
	do {
		i = (a + b) / 2;

		const auto cmp = strcmp(name, commands[i].cmd);
		if (cmp == 0)
			return &commands[i];
		else if (cmp < 0)
			b = i;
		else if (cmp > 0)
			a = i + 1;
	} while (a < b);
#else
	std::string key(name);
	command_map_iterator it = command_map.find(key);
	if (it != command_map.end()) {
		return &(it->second);
	}
#endif

	return nullptr;
}

static bool
command_check_request(const struct command *cmd, Response &r,
		      unsigned permission, Request args)
{
	if (cmd->permission != (permission & cmd->permission)) {
		r.FormatError(ACK_ERROR_PERMISSION,
			      "you don't have permission for \"%s\"",
			      cmd->cmd);
		return false;
	}

	const int min = cmd->min;
	const int max = cmd->max;

	if (min < 0)
		return true;

	if (min == max && unsigned(max) != args.size) {
		r.FormatError(ACK_ERROR_ARG,
			      "wrong number of arguments for \"%s\"",
			      cmd->cmd);
		return false;
	} else if (args.size < unsigned(min)) {
		r.FormatError(ACK_ERROR_ARG,
			      "too few arguments for \"%s\"", cmd->cmd);
		return false;
	} else if (max >= 0 && args.size > unsigned(max)) {
		r.FormatError(ACK_ERROR_ARG,
			      "too many arguments for \"%s\"", cmd->cmd);
		return false;
	} else
		return true;
}

static const struct command *
command_checked_lookup(Response &r, unsigned permission,
		       const char *cmd_name, Request args)
{
	const struct command *cmd = command_lookup(cmd_name);
	if (cmd == nullptr) {
		r.FormatError(ACK_ERROR_UNKNOWN,
			      "unknown command \"%s\"", cmd_name);
		return nullptr;
	}

	r.SetCommand(cmd->cmd);

	if (!command_check_request(cmd, r, permission, args))
		return nullptr;

	return cmd;
}

CommandResult
command_process(Client &client, unsigned num, char *line)
{
	Response r(client, num);
	Error error;

	/* get the command name (first word on the line) */
	/* we have to set current_command because Response::Error()
	   expects it to be set */

	Tokenizer tokenizer(line);

	const char *const cmd_name = tokenizer.NextWord(error);
	if (cmd_name == nullptr) {
		if (tokenizer.IsEnd())
			r.FormatError(ACK_ERROR_UNKNOWN, "No command given");
		else
			r.Error(ACK_ERROR_UNKNOWN, error.GetMessage());

		/* this client does not speak the MPD protocol; kick
		   the connection */
		return CommandResult::FINISH;
	}

	char *argv[COMMAND_ARGV_MAX];
	Request args(argv, 0);

	/* now parse the arguments (quoted or unquoted) */

	while (true) {
		if (args.size == COMMAND_ARGV_MAX) {
			r.Error(ACK_ERROR_ARG, "Too many arguments");
			return CommandResult::ERROR;
		}

		char *a = tokenizer.NextParam(error);
		if (a == nullptr) {
			if (tokenizer.IsEnd())
				break;

			r.Error(ACK_ERROR_UNKNOWN, error.GetMessage());
			return CommandResult::ERROR;
		}

		argv[args.size++] = a;
	}

	/* look up and invoke the command handler */

	const struct command *cmd =
		command_checked_lookup(r, client.GetPermission(),
				       cmd_name, args);

	CommandResult ret = cmd
		? cmd->handler(client, args, r)
		: CommandResult::ERROR;

	return ret;
}
