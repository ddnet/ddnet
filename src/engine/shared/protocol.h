/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_PROTOCOL_H
#define ENGINE_SHARED_PROTOCOL_H

#include <base/system.h>

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
	NETMSG_INFO = 1,

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

	NETMSG_AUTH_CHALLANGE, //
	NETMSG_AUTH_RESULT, //

	// sent by client
	NETMSG_READY, //
	NETMSG_ENTERGAME,
	NETMSG_INPUT, // contains the inputdata from the client
	NETMSG_RCON_CMD, //
	NETMSG_RCON_AUTH, //
	NETMSG_REQUEST_MAP_DATA, //

	NETMSG_AUTH_START, //
	NETMSG_AUTH_RESPONSE, //

	// sent by both
	NETMSG_PING,
	NETMSG_PING_REPLY,
	NETMSG_ERROR,

	// sent by server (todo: move it up)
	NETMSG_RCON_CMD_ADD,
	NETMSG_RCON_CMD_REM,

	NUM_NETMSGS,
};

// this should be revised
static const int SERVER_TICK_SPEED = 50;
static const int SERVER_FLAG_PASSWORD = 1 << 0;
static const int SERVER_FLAG_TIMESCORE = 1 << 1;
static const int SERVERINFO_LEVEL_MIN = 0;
static const int SERVERINFO_LEVEL_MAX = 2;

static const int SERVERINFO_MAX_CLIENTS = 128;
static const int MAX_CLIENTS = 64;
static const int VANILLA_MAX_CLIENTS = 16;
static const int MAX_SERVER_ADDRESSES = 16;
static const int MAX_CHECKPOINTS = 25;

static const int MAX_INPUT_SIZE = 128;
static const int MAX_SNAPSHOT_PACKSIZE = 900;

static const int MAX_NAME_LENGTH = 16;
static const int MAX_CLAN_LENGTH = 12;

// message packing
static const int MSGFLAG_VITAL = 1;
static const int MSGFLAG_FLUSH = 2;
static const int MSGFLAG_NORECORD = 4;
static const int MSGFLAG_RECORD = 8;
static const int MSGFLAG_NOSEND = 16;

static const int VERSION_NONE = -1;
static const int VERSION_VANILLA = 0;
static const int VERSION_DDRACE = 1;
static const int VERSION_DDNET_OLD = 2;
static const int VERSION_DDNET_WHISPER = 217;
static const int VERSION_DDNET_GOODHOOK = 221;
static const int VERSION_DDNET_RCONPROTECT = 408;
static const int VERSION_DDNET_ANTIPING_PROJECTILE = 604;
static const int VERSION_DDNET_UPDATER_FIXED = 707;
static const int VERSION_DDNET_GAMETICK = 10042;
static const int VERSION_DDNET_EARLY_VERSION = 13020;
static const int VERSION_DDNET_MSG_LEGACY = 15040;
static const int VERSION_DDNET_SWITCH = 15060;
static const int VERSION_DDNET_INDEPENDENT_SPECTATORS_TEAM = 16000;
static const int VERSION_DDNET_WEAPON_SHIELDS = 16010;
static const int VERSION_DDNET_NEW_HUD = 16020;
static const int VERSION_DDNET_MULTI_LASER = 16040;

#endif
