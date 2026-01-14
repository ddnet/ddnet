/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_PROTOCOL_H
#define ENGINE_SHARED_PROTOCOL_H

#include <engine/shared/protocol7.h>

#include <bitset>

/*
	Connection diagram - How the initialization works.

	Client -> INFO -> Server
		Contains version info, name, and some other info.

	Client <- MAP <- Server
		Contains current map.

	Client -> READY -> Server
		The client has loaded the map and is ready to go,
		but the mod needs to send it's information as well.
		modc_connected is called on the client and
		mods_connected is called on the server.
		The client should call client_entergame when the
		mod has done it's initialization.

	Client -> ENTERGAME -> Server
		Tells the server to start sending snapshots.
		client_entergame and server_client_enter is called.
*/

enum
{
	NETMSG_EX = 0,

	// the first thing sent by the client
	// contains the version info for the client
	NETMSG_INFO,

	// sent by server
	NETMSG_MAP_CHANGE, // sent when client should switch map
	NETMSG_MAP_DATA, // map transfer, contains a chunk of the map file
	NETMSG_CON_READY, // connection is ready, client should send start info
	NETMSG_SNAP, // normal snapshot, multiple parts
	NETMSG_SNAPEMPTY, // empty snapshot
	NETMSG_SNAPSINGLE, // ?
	NETMSG_SNAPSMALL, //
	NETMSG_INPUTTIMING, // reports how off the input was
	NETMSG_RCON_AUTH_STATUS, // result of the authentication
	NETMSG_RCON_LINE, // line that should be printed to the remote console

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

	// sent by server (todo: move it up)
	NETMSG_RCON_CMD_ADD,
	NETMSG_RCON_CMD_REM,

	NUM_NETMSGS,
};

// this should be revised
enum
{
	SERVER_TICK_SPEED = 50,
	SERVER_FLAG_PASSWORD = 1 << 0,
	SERVER_FLAG_TIMESCORE = 1 << 1,
	SERVERINFO_LEVEL_MIN = 0,
	SERVERINFO_LEVEL_MAX = 2,

	MAX_SERVER_ADDRESSES = 16,
	SERVERINFO_MAX_CLIENTS = 128,
	MAX_CLIENTS = 128,
	VANILLA_MAX_CLIENTS = 16,
	SERVER_MAX_CLIENTS = 64,
	MAX_CHECKPOINTS = 25,
	MIN_TICK = 0,
	MAX_TICK = 0x6FFFFFFF,

	MAX_INPUT_SIZE = 128,
	MAX_SNAPSHOT_PACKSIZE = 900,

	MAX_NAME_LENGTH = 16,
	MAX_CLAN_LENGTH = 12,
	MAX_SKIN_LENGTH = 24,

	// message packing
	/**
	 * Guaranteed to be delivered, resent on packet loss.
	 */
	MSGFLAG_VITAL = 1 << 0,
	/**
	 * Makes the message be sent immediately. Without this flag the message will be delayed until the next flush.
	 */
	MSGFLAG_FLUSH = 1 << 1,
	/**
	 * Don't write message to demo recorders. This flag is server-side only, where sent messages are recorded by default.
	 */
	MSGFLAG_NORECORD = 1 << 2,
	/**
	 * Write message to demo recorders. This flag is client-side only, where sent messages are not recorded by default.
	 */
	MSGFLAG_RECORD = 1 << 3,
	/**
	 * Don't send the message to client/server. Useful combined with @link MSGFLAG_RECORD @endlink to record a message without sending it.
	 */
	MSGFLAG_NOSEND = 1 << 4,
};

enum
{
	VERSION_NONE = -1,
	VERSION_VANILLA = 0,
	VERSION_DDRACE = 1,
	VERSION_DDNET_OLD = 2,
	VERSION_DDNET_WHISPER = 217,
	VERSION_DDNET_ANTIPING_PROJECTILE = 604,
	VERSION_DDNET_UPDATER_FIXED = 707,
	VERSION_DDNET_GAMETICK = 10042,
	VERSION_DDNET_EARLY_VERSION = 13020,
	VERSION_DDNET_MSG_LEGACY = 15040,
	VERSION_DDNET_INDEPENDENT_SPECTATORS_TEAM = 16000,
	VERSION_DDNET_WEAPON_SHIELDS = 16010,
	VERSION_DDNET_NEW_HUD = 16020,
	VERSION_DDNET_MULTI_LASER = 16040,
	VERSION_DDNET_ENTITY_NETOBJS = 16200,
	VERSION_DDNET_REDIRECT = 17020,
	VERSION_DDNET_PLAYERFLAG_SPEC_CAM = 18090,
	VERSION_DDNET_RECONNECT = 18090,
	VERSION_DDNET_128_PLAYERS = 19000,
	VERSION_DDNET_PREINPUT = 19040,
	VERSION_DDNET_SAVE_CODE = 19060,
	VERSION_DDNET_IMPORTANT_ALERT = 19060,
	VERSION_DDNET_MAP_BESTTIME = 19070,
};

namespace TuneZone
{
	inline constexpr int OVERRIDE_NONE = -1;
	inline constexpr int NUM = 256;
};

namespace FinishTime
{
	inline constexpr int NOT_FINISHED_TIMESCORE = -9999;
	inline constexpr int NOT_FINISHED_MILLIS = -1;
	inline constexpr int UNSET = -2;
}

typedef std::bitset<MAX_CLIENTS> CClientMask;

#endif
