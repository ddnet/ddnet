/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_PROTOCOL7_H
#define ENGINE_SHARED_PROTOCOL7_H

#include <base/types.h>

namespace protocol7 {

enum
{
	NETMSG_NULL = 0,

	// the first thing sent by the client
	// contains the version info for the client
	NETMSG_INFO = 1,

	// sent by server
	NETMSG_MAP_CHANGE, // sent when client should switch map
	NETMSG_MAP_DATA, // map transfer, contains a chunk of the map file
	NETMSG_SERVERINFO,
	NETMSG_CON_READY, // connection is ready, client should send start info
	NETMSG_SNAP, // normal snapshot, multiple parts
	NETMSG_SNAPEMPTY, // empty snapshot
	NETMSG_SNAPSINGLE, // ?
	NETMSG_SNAPSMALL, //
	NETMSG_INPUTTIMING, // reports how off the input was
	NETMSG_RCON_AUTH_ON, // rcon authentication enabled
	NETMSG_RCON_AUTH_OFF, // rcon authentication disabled
	NETMSG_RCON_LINE, // line that should be printed to the remote console
	NETMSG_RCON_CMD_ADD,
	NETMSG_RCON_CMD_REM,

	NETMSG_UNUSED1,
	NETMSG_UNUSED2,

	// sent by client
	NETMSG_READY, //
	NETMSG_ENTERGAME,
	NETMSG_INPUT, // contains the inputdata from the client
	NETMSG_RCON_CMD, //
	NETMSG_RCON_AUTH, //
	NETMSG_REQUEST_MAP_DATA, //

	NETMSG_UNUSED3,
	NETMSG_UNUSED4,

	// sent by both
	NETMSG_PING,
	NETMSG_PING_REPLY,
	NETMSG_UNUSED5,

	NETMSG_MAPLIST_ENTRY_ADD, // todo 0.8: move up
	NETMSG_MAPLIST_ENTRY_REM,
};

enum
{
	MAX_NAME_LENGTH = 16,
	MAX_NAME_ARRAY_SIZE = MAX_NAME_LENGTH * UTF8_BYTE_LENGTH + 1,
	MAX_CLAN_LENGTH = 12,
	MAX_CLAN_ARRAY_SIZE = MAX_CLAN_LENGTH * UTF8_BYTE_LENGTH + 1,
	MAX_SKIN_LENGTH = 24,
	MAX_SKIN_ARRAY_SIZE = MAX_SKIN_LENGTH * UTF8_BYTE_LENGTH + 1,
};

} // namespace protocol7

#endif
