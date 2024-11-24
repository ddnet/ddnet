# pylint: skip-file
# See https://github.com/ddnet/ddnet/issues/3507

from datatypes import Enum, Flags, NetArray, NetBool, NetEvent, NetEventEx, NetIntAny, NetIntRange, NetMessage, NetMessageEx, NetObject, NetObjectEx, NetString, NetStringHalfStrict, NetStringStrict, NetTick

Emotes = ["NORMAL", "PAIN", "HAPPY", "SURPRISE", "ANGRY", "BLINK"]
PlayerFlags = ["PLAYING", "IN_MENU", "CHATTING", "SCOREBOARD", "AIM"]
GameFlags = ["TEAMS", "FLAGS"]
GameStateFlags = ["GAMEOVER", "SUDDENDEATH", "PAUSED", "RACETIME"]
CharacterFlags = ["SOLO", "JETPACK", "COLLISION_DISABLED", "ENDLESS_HOOK", "ENDLESS_JUMP", "SUPER",
                  "HAMMER_HIT_DISABLED", "SHOTGUN_HIT_DISABLED", "GRENADE_HIT_DISABLED", "LASER_HIT_DISABLED", "HOOK_HIT_DISABLED",
                  "TELEGUN_GUN", "TELEGUN_GRENADE", "TELEGUN_LASER",
                  "WEAPON_HAMMER", "WEAPON_GUN", "WEAPON_SHOTGUN", "WEAPON_GRENADE", "WEAPON_LASER", "WEAPON_NINJA",
				  "MOVEMENTS_DISABLED", "IN_FREEZE", "PRACTICE_MODE", "LOCK_MODE", "TEAM0_MODE", "INVINCIBLE"]
GameInfoFlags = [
	"TIMESCORE", "GAMETYPE_RACE", "GAMETYPE_FASTCAP", "GAMETYPE_FNG",
	"GAMETYPE_DDRACE", "GAMETYPE_DDNET", "GAMETYPE_BLOCK_WORLDS",
	"GAMETYPE_VANILLA", "GAMETYPE_PLUS", "FLAG_STARTS_RACE", "RACE",
	"UNLIMITED_AMMO", "DDRACE_RECORD_MESSAGE", "RACE_RECORD_MESSAGE",
	"ALLOW_EYE_WHEEL", "ALLOW_HOOK_COLL", "ALLOW_ZOOM", "BUG_DDRACE_GHOST",
	"BUG_DDRACE_INPUT", "BUG_FNG_LASER_RANGE", "BUG_VANILLA_BOUNCE",
	"PREDICT_FNG", "PREDICT_DDRACE", "PREDICT_DDRACE_TILES", "PREDICT_VANILLA",
	"ENTITIES_DDNET", "ENTITIES_DDRACE", "ENTITIES_RACE", "ENTITIES_FNG",
	"ENTITIES_VANILLA", "DONT_MASK_ENTITIES", "ENTITIES_BW"
	# Full, use GameInfoFlags2 for more flags
]
GameInfoFlags2 = [
	"ALLOW_X_SKINS", "GAMETYPE_CITY", "GAMETYPE_FDDRACE", "ENTITIES_FDDRACE", "HUD_HEALTH_ARMOR", "HUD_AMMO",
	"HUD_DDRACE", "NO_WEAK_HOOK", "NO_SKIN_CHANGE_FOR_FROZEN"
]
ExPlayerFlags = ["AFK", "PAUSED", "SPEC"]
LegacyProjectileFlags = [f"CLIENTID_BIT{i}" for i in range(8)] + [
	"NO_OWNER", "IS_DDNET", "BOUNCE_HORIZONTAL", "BOUNCE_VERTICAL",
	"EXPLOSIVE", "FREEZE",
]
ProjectileFlags = [
	"BOUNCE_HORIZONTAL", "BOUNCE_VERTICAL", "EXPLOSIVE", "FREEZE", "NORMALIZE_VEL",
]
LaserFlags = [
	"NO_PREDICT",
]

LaserTypes = ["RIFLE", "SHOTGUN", "DOOR", "FREEZE", "DRAGGER", "GUN", "PLASMA"]
DraggerTypes = ["WEAK", "WEAK_NW", "NORMAL", "NORMAL_NW", "STRONG", "STRONG_NW"]
GunTypes = ["UNFREEZE", "EXPLOSIVE", "FREEZE", "EXPFREEZE"]

Emoticons = ["OOP", "EXCLAMATION", "HEARTS", "DROP", "DOTDOT", "MUSIC", "SORRY", "GHOST", "SUSHI", "SPLATTEE", "DEVILTEE", "ZOMG", "ZZZ", "WTF", "EYES", "QUESTION"]

Powerups = ["HEALTH", "ARMOR", "WEAPON", "NINJA", "ARMOR_SHOTGUN", "ARMOR_GRENADE", "ARMOR_NINJA", "ARMOR_LASER"]
Authed = ["NO", "HELPER", "MOD", "ADMIN"]
EntityClasses = ["PROJECTILE", "DOOR", "DRAGGER_WEAK", "DRAGGER_NORMAL", "DRAGGER_STRONG", "GUN_NORMAL", "GUN_EXPLOSIVE", "GUN_FREEZE", "GUN_UNFREEZE", "LIGHT", "PICKUP"]
Teams = ["ALL", "SPECTATORS", "RED", "BLUE", "WHISPER_SEND", "WHISPER_RECV"]

RawHeader = '''
#include <engine/shared/teehistorian_ex.h>

enum
{
	INPUT_STATE_MASK=0x3f
};

enum
{
	FLAG_MISSING=-3,
	FLAG_ATSTAND,
	FLAG_TAKEN,

	SPEC_FREEVIEW=-1,
	SPEC_FOLLOW=-2,
};

enum
{
	GAMEINFO_CURVERSION=9,
};
'''

Enums = [
	Enum("EMOTE", Emotes),
	Enum("POWERUP", Powerups),
	Enum("EMOTICON", Emoticons),
	Enum("AUTHED", Authed),
	Enum("ENTITYCLASS", EntityClasses),
	Enum("LASERTYPE", LaserTypes),
	Enum("LASERDRAGGERTYPE", DraggerTypes),
	Enum("LASERGUNTYPE", GunTypes),
	Enum("TEAM", Teams, -2),
]

Flags = [
	Flags("PLAYERFLAG", PlayerFlags),
	Flags("GAMEFLAG", GameFlags),
	Flags("GAMESTATEFLAG", GameStateFlags),
	Flags("CHARACTERFLAG", CharacterFlags),
	Flags("GAMEINFOFLAG", GameInfoFlags),
	Flags("GAMEINFOFLAG2", GameInfoFlags2),
	Flags("EXPLAYERFLAG", ExPlayerFlags),
	Flags("LEGACYPROJECTILEFLAG", LegacyProjectileFlags),
	Flags("PROJECTILEFLAG", ProjectileFlags),
	Flags("LASERFLAG", LaserFlags),
]

Objects = [

	NetObject("PlayerInput", [
		NetIntAny("m_Direction"),
		NetIntAny("m_TargetX"),
		NetIntAny("m_TargetY"),

		NetIntAny("m_Jump"),
		NetIntAny("m_Fire"),
		NetIntAny("m_Hook"),

		NetIntRange("m_PlayerFlags", 0, 256),

		NetIntAny("m_WantedWeapon"),
		NetIntAny("m_NextWeapon"),
		NetIntAny("m_PrevWeapon"),
	]),

	NetObject("Projectile", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),

		NetIntRange("m_Type", 0, 'NUM_WEAPONS-1'),
		NetTick("m_StartTick"),
	]),

	NetObject("Laser", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_FromX"),
		NetIntAny("m_FromY"),

		NetTick("m_StartTick"),
	]),

	NetObject("Pickup", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),

		NetIntRange("m_Type", 0, 'max_int'),
		NetIntRange("m_Subtype", 0, 'max_int'),
	]),

	NetObject("Flag", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),

		NetIntRange("m_Team", 'TEAM_RED', 'TEAM_BLUE')
	]),

	NetObject("GameInfo", [
		NetIntRange("m_GameFlags", 0, 256),
		NetIntRange("m_GameStateFlags", 0, 256),
		NetTick("m_RoundStartTick"),
		NetIntRange("m_WarmupTimer", 'min_int', 'max_int'),

		NetIntRange("m_ScoreLimit", 0, 'max_int'),
		NetIntRange("m_TimeLimit", 0, 'max_int'),

		NetIntRange("m_RoundNum", 0, 'max_int'),
		NetIntRange("m_RoundCurrent", 0, 'max_int'),
	]),

	NetObject("GameData", [
		NetIntAny("m_TeamscoreRed"),
		NetIntAny("m_TeamscoreBlue"),

		NetIntRange("m_FlagCarrierRed", 'FLAG_MISSING', 'MAX_CLIENTS-1'),
		NetIntRange("m_FlagCarrierBlue", 'FLAG_MISSING', 'MAX_CLIENTS-1'),
	]),

	NetObject("CharacterCore", [
		NetIntAny("m_Tick"),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),

		NetIntAny("m_Angle"),
		NetIntRange("m_Direction", -1, 1),

		NetIntRange("m_Jumped", 0, 3),
		NetIntRange("m_HookedPlayer", -1, 'MAX_CLIENTS-1'),
		NetIntRange("m_HookState", -1, 5),
		NetIntAny("m_HookTick"),

		NetIntAny("m_HookX"),
		NetIntAny("m_HookY"),
		NetIntAny("m_HookDx"),
		NetIntAny("m_HookDy"),
	]),

	NetObject("Character:CharacterCore", [
		NetIntRange("m_PlayerFlags", 0, 256),
		NetIntRange("m_Health", 0, 10),
		NetIntRange("m_Armor", 0, 10),
		NetIntRange("m_AmmoCount", 0, 10),
		NetIntRange("m_Weapon", -1, 'NUM_WEAPONS-1'),
		NetIntRange("m_Emote", 0, len(Emotes)),
		NetIntRange("m_AttackTick", 0, 'max_int'),
	]),

	NetObject("PlayerInfo", [
		NetIntRange("m_Local", 0, 1),
		NetIntRange("m_ClientId", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Team", 'TEAM_SPECTATORS', 'TEAM_BLUE'),

		NetIntAny("m_Score"),
		NetIntAny("m_Latency"),
	]),

	NetObject("ClientInfo", [
		# 4*4 = 16 characters
		NetIntAny("m_Name0"), NetIntAny("m_Name1"), NetIntAny("m_Name2"),
		NetIntAny("m_Name3"),

		# 4*3 = 12 characters
		NetIntAny("m_Clan0"), NetIntAny("m_Clan1"), NetIntAny("m_Clan2"),

		NetIntAny("m_Country"),

		# 4*6 = 24 characters
		NetIntAny("m_Skin0"), NetIntAny("m_Skin1"), NetIntAny("m_Skin2"),
		NetIntAny("m_Skin3"), NetIntAny("m_Skin4"), NetIntAny("m_Skin5"),

		NetIntRange("m_UseCustomColor", 0, 1),

		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
	]),

	NetObject("SpectatorInfo", [
		NetIntRange("m_SpectatorId", 'SPEC_FREEVIEW', 'MAX_CLIENTS-1'),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),

	NetObjectEx("MyOwnObject", "my-own-object@heinrich5991.de", [
		NetIntAny("m_Test"),
	]),

	NetObjectEx("DDNetCharacter", "character@netobj.ddnet.tw", [
		NetIntAny("m_Flags", 0),
		NetTick("m_FreezeEnd", 0),
		NetIntRange("m_Jumps", -1, 255, 2),
		NetIntAny("m_TeleCheckpoint", -1),
		NetIntRange("m_StrongWeakId", 0, 'MAX_CLIENTS-1', 0),

		# New data fields for jump display, freeze bar and ninja bar
		# Default values indicate that these values should not be used
		NetIntRange("m_JumpedTotal", -1, 255, -1),
		NetTick("m_NinjaActivationTick", -1),
		NetTick("m_FreezeStart", -1),
		# New data fields for improved target accuracy
		NetIntAny("m_TargetX", 0),
		NetIntAny("m_TargetY", 0),
	], validate_size=False),

	NetObjectEx("DDNetPlayer", "player@netobj.ddnet.tw", [
		NetIntAny("m_Flags"),
		NetIntRange("m_AuthLevel", "AUTHED_NO", "AUTHED_ADMIN"),
	]),

	NetObjectEx("GameInfoEx", "gameinfo@netobj.ddnet.tw", [
		NetIntAny("m_Flags", 0),
		NetIntAny("m_Version", 0),
		NetIntAny("m_Flags2", 0),
	], validate_size=False),

	# The code assumes that this has the same in-memory representation as
	# the Projectile net object.
	NetObjectEx("DDRaceProjectile", "projectile@netobj.ddnet.tw", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_Angle"),
		NetIntAny("m_Data"),
		NetIntRange("m_Type", 0, 'NUM_WEAPONS-1'),
		NetTick("m_StartTick"),
	]),

	NetObjectEx("DDNetLaser", "laser@netobj.ddnet.tw", [
		NetIntAny("m_ToX"),
		NetIntAny("m_ToY"),
		NetIntAny("m_FromX"),
		NetIntAny("m_FromY"),
		NetTick("m_StartTick"),
		NetIntRange("m_Owner", -1, 'MAX_CLIENTS-1'),
		NetIntAny("m_Type"),
		NetIntAny("m_SwitchNumber", -1),
		NetIntAny("m_Subtype", -1),
		NetIntAny("m_Flags", 0),
	]),

	NetObjectEx("DDNetProjectile", "ddnet-projectile@netobj.ddnet.tw", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntRange("m_Type", 0, 'NUM_WEAPONS-1'),
		NetTick("m_StartTick"),
		NetIntRange("m_Owner", -1, 'MAX_CLIENTS-1'),
		NetIntAny("m_SwitchNumber"),
		NetIntAny("m_TuneZone"),
		NetIntAny("m_Flags"),
	]),

	NetObjectEx("DDNetPickup", "pickup@netobj.ddnet.tw", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntRange("m_Type", 0, 'max_int'),
		NetIntRange("m_Subtype", 0, 'max_int'),
		NetIntAny("m_SwitchNumber"),
	]),

	## Events

	NetEvent("Common", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),


	NetEvent("Explosion:Common", []),
	NetEvent("Spawn:Common", []),
	NetEvent("HammerHit:Common", []),


	NetEvent("Death:Common", [
		NetIntRange("m_ClientId", 0, 'MAX_CLIENTS-1'),
	]),

	NetEvent("SoundGlobal:Common", [ #TODO 0.7: remove me
		NetIntRange("m_SoundId", 0, 'NUM_SOUNDS-1'),
	]),

	NetEvent("SoundWorld:Common", [
		NetIntRange("m_SoundId", 0, 'NUM_SOUNDS-1'),
	]),

	NetEvent("DamageInd:Common", [
		NetIntAny("m_Angle"),
	]),

	NetEventEx("Birthday:Common", "birthday@netevent.ddnet.org", []),

	NetEventEx("Finish:Common", "finish@netevent.ddnet.org", []),

	NetObjectEx("MyOwnEvent", "my-own-event@heinrich5991.de", [
		NetIntAny("m_Test"),
	]),

	NetObjectEx("SpecChar", "spec-char@netobj.ddnet.tw", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),

	# Switch state for a player team.
	NetObjectEx("SwitchState", "switch-state@netobj.ddnet.tw", [
		NetIntAny("m_HighestSwitchNumber", 0),
		# 256 switches / 32 bits = 8 int32
		NetArray(NetIntAny("m_aStatus", 0), 8),
		# send the endtick of up to 4 timed switchers
		NetArray(NetIntAny("m_aSwitchNumbers", 0), 4),
		NetArray(NetIntAny("m_aEndTicks", 0), 4),
	], validate_size=False),

	# Switch info for map items
	NetObjectEx("EntityEx", "entity-ex@netobj.ddnet.tw", [
		NetIntAny("m_SwitchNumber"),
		NetIntAny("m_Layer"),
		NetIntAny("m_EntityClass"),
	]),

	NetEventEx("MapSoundWorld:Common", "map-sound-world@netevent.ddnet.org", [
		NetIntAny("m_SoundId"),
	]),
]

Messages = [

	### Server messages
	NetMessage("Sv_Motd", [
		NetString("m_pMessage"),
	]),

	NetMessage("Sv_Broadcast", [
		NetString("m_pMessage"),
	]),

	NetMessage("Sv_Chat", [
		NetIntRange("m_Team", -2, 3),
		NetIntRange("m_ClientId", -1, 'MAX_CLIENTS-1'),
		NetStringHalfStrict("m_pMessage"),
	]),

	NetMessage("Sv_KillMsg", [
		NetIntRange("m_Killer", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Victim", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Weapon", -3, 'NUM_WEAPONS-1'),
		NetIntAny("m_ModeSpecial"),
	]),

	NetMessage("Sv_SoundGlobal", [
		NetIntRange("m_SoundId", 0, 'NUM_SOUNDS-1'),
	]),

	NetMessage("Sv_TuneParams", []),
	NetMessage("Unused", []),
	NetMessage("Sv_ReadyToEnter", []),

	NetMessage("Sv_WeaponPickup", [
		NetIntRange("m_Weapon", 0, 'NUM_WEAPONS-1'),
	]),

	NetMessage("Sv_Emoticon", [
		NetIntRange("m_ClientId", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Emoticon", 0, 'NUM_EMOTICONS-1'),
	]),

	NetMessage("Sv_VoteClearOptions", [
	]),

	NetMessage("Sv_VoteOptionListAdd", [
		NetIntRange("m_NumOptions", 1, 15),
		NetStringStrict("m_pDescription0"), NetStringStrict("m_pDescription1"),	NetStringStrict("m_pDescription2"),
		NetStringStrict("m_pDescription3"),	NetStringStrict("m_pDescription4"),	NetStringStrict("m_pDescription5"),
		NetStringStrict("m_pDescription6"), NetStringStrict("m_pDescription7"), NetStringStrict("m_pDescription8"),
		NetStringStrict("m_pDescription9"), NetStringStrict("m_pDescription10"), NetStringStrict("m_pDescription11"),
		NetStringStrict("m_pDescription12"), NetStringStrict("m_pDescription13"), NetStringStrict("m_pDescription14"),
	]),

	NetMessage("Sv_VoteOptionAdd", [
		NetStringStrict("m_pDescription"),
	]),

	NetMessage("Sv_VoteOptionRemove", [
		NetStringStrict("m_pDescription"),
	]),

	NetMessage("Sv_VoteSet", [
		NetIntRange("m_Timeout", 0, 'max_int'),
		NetStringStrict("m_pDescription"),
		NetStringStrict("m_pReason"),
	]),

	NetMessage("Sv_VoteStatus", [
		NetIntRange("m_Yes", 0, 'MAX_CLIENTS'),
		NetIntRange("m_No", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Pass", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Total", 0, 'MAX_CLIENTS'),
	]),

	### Client messages
	NetMessage("Cl_Say", [
		NetBool("m_Team"),
		NetStringHalfStrict("m_pMessage"),
	], teehistorian=False),

	NetMessage("Cl_SetTeam", [
		NetIntRange("m_Team", 'TEAM_SPECTATORS', 'TEAM_BLUE'),
	]),

	NetMessage("Cl_SetSpectatorMode", [
		NetIntRange("m_SpectatorId", 'SPEC_FREEVIEW', 'MAX_CLIENTS-1'),
	]),

	NetMessage("Cl_StartInfo", [
		NetStringStrict("m_pName"),
		NetStringStrict("m_pClan"),
		NetIntAny("m_Country"),
		NetStringStrict("m_pSkin"),
		NetBool("m_UseCustomColor"),
		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
	]),

	NetMessage("Cl_ChangeInfo", [
		NetStringStrict("m_pName"),
		NetStringStrict("m_pClan"),
		NetIntAny("m_Country"),
		NetStringStrict("m_pSkin"),
		NetBool("m_UseCustomColor"),
		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
	]),

	NetMessage("Cl_Kill", []),

	NetMessage("Cl_Emoticon", [
		NetIntRange("m_Emoticon", 0, 'NUM_EMOTICONS-1'),
	]),

	NetMessage("Cl_Vote", [
		NetIntRange("m_Vote", -1, 1),
	], teehistorian=False),

	NetMessage("Cl_CallVote", [
		NetStringStrict("m_pType"),
		NetStringStrict("m_pValue"),
		NetStringStrict("m_pReason"),
	], teehistorian=False),

	NetMessage("Cl_IsDDNetLegacy", []),

	NetMessage("Sv_DDRaceTimeLegacy", [
		NetIntAny("m_Time"),
		NetIntAny("m_Check"),
		NetIntRange("m_Finish", 0, 1),
	]),

	NetMessage("Sv_RecordLegacy", [
		NetIntAny("m_ServerTimeBest"),
		NetIntAny("m_PlayerTimeBest"),
	]),

	NetMessage("Unused2", []),

	NetMessage("Sv_TeamsStateLegacy", []),

	# deprecated, use showothers@netmsg.ddnet.tw instead
	NetMessage("Cl_ShowOthersLegacy", [
		NetBool("m_Show"),
	]),
# Can't add any NetMessages here!

	NetMessageEx("Sv_MyOwnMessage", "my-own-message@heinrich5991.de", [
		NetIntAny("m_Test"),
	]),

	NetMessageEx("Cl_ShowDistance", "show-distance@netmsg.ddnet.tw", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),

	NetMessageEx("Cl_ShowOthers", "showothers@netmsg.ddnet.tw", [
		NetIntRange("m_Show", 0, 2),
	]),

	NetMessageEx("Sv_TeamsState", "teamsstate@netmsg.ddnet.tw", []),

	NetMessageEx("Sv_DDRaceTime", "ddrace-time@netmsg.ddnet.tw", [
		NetIntAny("m_Time"),
		NetIntAny("m_Check"),
		NetIntRange("m_Finish", 0, 1),
	]),

	NetMessageEx("Sv_Record", "record@netmsg.ddnet.tw", [
		NetIntAny("m_ServerTimeBest"),
		NetIntAny("m_PlayerTimeBest"),
	]),
    
	NetMessageEx("Sv_KillMsgTeam", "killmsgteam@netmsg.ddnet.tw", [
		NetIntRange("m_Team", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_First", -1, 'MAX_CLIENTS-1'),
	]),

	NetMessageEx("Sv_YourVote", "yourvote@netmsg.ddnet.org", [
		NetIntRange("m_Voted", -1, 1),
	]),

	NetMessageEx("Sv_RaceFinish", "racefinish@netmsg.ddnet.org", [
		NetIntRange("m_ClientId", 0, 'MAX_CLIENTS-1'),
		NetIntAny("m_Time"),
		NetIntAny("m_Diff"),
		NetBool("m_RecordPersonal"),
		NetBool("m_RecordServer", default=False),
	]),

	NetMessageEx("Sv_CommandInfo", "commandinfo@netmsg.ddnet.org", [
			NetStringStrict("m_pName"),
			NetStringStrict("m_pArgsFormat"),
			NetStringStrict("m_pHelpText")
	]),

	NetMessageEx("Sv_CommandInfoRemove", "commandinfo-remove@netmsg.ddnet.org", [
			NetStringStrict("m_pName")
	]),

	NetMessageEx("Sv_VoteOptionGroupStart", "sv-vote-option-group-start@netmsg.ddnet.org", []),
	NetMessageEx("Sv_VoteOptionGroupEnd", "sv-vote-option-group-end@netmsg.ddnet.org", []),

	NetMessageEx("Sv_CommandInfoGroupStart", "sv-commandinfo-group-start@netmsg.ddnet.org", []),
	NetMessageEx("Sv_CommandInfoGroupEnd", "sv-commandinfo-group-end@netmsg.ddnet.org", []),

	NetMessageEx("Sv_ChangeInfoCooldown", "change-info-cooldown@netmsg.ddnet.org", [
		NetTick("m_WaitUntil")
	]),

	NetMessageEx("Sv_MapSoundGlobal", "map-sound-global@netmsg.ddnet.org", [
		NetIntAny("m_SoundId"),
	]),
]
