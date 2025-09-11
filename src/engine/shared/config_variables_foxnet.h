/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
// This helps IDEs properly syntax highlight the uses of the macro below.
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc)
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc)
#endif

MACRO_CONFIG_INT(FoxExampleInt, fox_example_int, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_SAVE, "Example integer config variable")
MACRO_CONFIG_STR(FoxExampleStr, fox_example_str, 100, "FoxNet", CFGFLAG_SERVER | CFGFLAG_SAVE, "Example string config variable")

MACRO_CONFIG_INT(SvRandomMapVoteOnStart, sv_random_map_vote_on_start, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Call a random map vote on server startup")
MACRO_CONFIG_INT(SvVoteMenuFlags, sv_vote_menu_flags, 0, 0, 32768, CFGFLAG_SERVER | CFGFLAG_SAVE, "Flags for what Pages to show in the vote menu")
MACRO_CONFIG_INT(SvVoteSkipPrefix, sv_vote_skip_prefix, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Skips Prefixes for the vote message when calling a vote")

MACRO_CONFIG_INT(SvAccounts, sv_accounts, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_SAVE, "Enable optional player accounts")

MACRO_CONFIG_STR(SvCurrencyName, sv_currency_name, 16, "FoxCoins", CFGFLAG_SERVER, "Whatever you want your currency name to be")
MACRO_CONFIG_INT(SvLevelUpMoney, sv_levelup_money, 500, 0, 5000, CFGFLAG_SERVER | CFGFLAG_GAME, "How much money a player should get if they level up")
MACRO_CONFIG_INT(SvPlaytimeMoney, sv_playtime_money, 250, 0, 5000, CFGFLAG_SERVER | CFGFLAG_GAME, "How much money a player should everytime their playtime increased by 100 (divisble by 100: 100, 200..)")

// Bot Detection
MACRO_CONFIG_INT(SvAntiAdBot, sv_anti_ad_bot, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Anti chat ad bot")
MACRO_CONFIG_INT(SvAutoBanJSClient, sv_auto_ban_jsclient, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "JS Client is a bot client commonly used for chat spamming")

MACRO_CONFIG_INT(SvReversePrediction, sv_prediction_test, 14, 1, 200, CFGFLAG_SERVER, "Reverse prediction margin")

// Ban Syncing
MACRO_CONFIG_INT(SvBanSyncing, sv_ban_syncing, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to Sync bans every fs_ban_syncing_delay mins across servers")
MACRO_CONFIG_INT(SvBanSyncingDelay, sv_ban_syncing_delay, 15, 1, 500, CFGFLAG_SERVER | CFGFLAG_GAME, "How long the server waits between syncs")

// snake
MACRO_CONFIG_INT(SvSnakeAutoMove, sv_snake_auto_move, 0, 0, 1, CFGFLAG_SERVER, "Whether snake keeps last input or can stand still if no inputs applied")
MACRO_CONFIG_INT(SvSnakeSpeed, sv_snake_speed, 6, 1, 50, CFGFLAG_SERVER, "Snake blocks per second speed")
MACRO_CONFIG_INT(SvSnakeDiagonal, sv_snake_diagonal, 1, 0, 1, CFGFLAG_SERVER, "Whether snake can move diagonally")
MACRO_CONFIG_INT(SvSnakeSmooth, sv_snake_smooth, 1, 0, 1, CFGFLAG_SERVER, "Whether snake moves smoothly")
MACRO_CONFIG_INT(SvSnakeTeePickup, sv_snake_tee_pickup, 1, 0, 1, CFGFLAG_SERVER, "Whether to add tees to the snake on touch or not")
MACRO_CONFIG_INT(SvSnakeCollision, sv_snake_collision, 1, 0, 1, CFGFLAG_SERVER, "Whether to have collision with blocks or not")

// Ufo
MACRO_CONFIG_INT(SvUfoMaxSpeed, sv_ufo_max_speed, 16, 1, 50, CFGFLAG_SERVER, "Ufos maximum speed")
MACRO_CONFIG_INT(SvUfoFriction, sv_ufo_friction, 90, 0, 100, CFGFLAG_SERVER, "Ufos friction (how fast it slows down when theres no movement)")
MACRO_CONFIG_INT(SvUfoAccel, sv_ufo_accel, 12, 1, 200, CFGFLAG_SERVER, "Ufos acceleration in any direction")
MACRO_CONFIG_INT(SvUfoTranslateVel, sv_ufo_translate_vel, 1, 0, 1, CFGFLAG_SERVER, "Whether to use normal chararcter velocity aswell as UFOs")
MACRO_CONFIG_INT(SvUfoDisableFreeze, sv_ufo_disable_freeze, 1, 0, 1, CFGFLAG_SERVER, "Whether the character gets affected by freeze (cant move)")
MACRO_CONFIG_INT(SvAutoUfo, sv_auto_ufo, 0, 0, 1, CFGFLAG_SERVER, "Automatically gives every player an UFO (always)")
MACRO_CONFIG_INT(SvUfoLaserType, sv_ufo_laser_type, 0, 0, 6, CFGFLAG_SERVER, "Ufos laser type")
MACRO_CONFIG_INT(SvUfoHideHookColl, sv_ufo_hide_hook_coll, 2, 0, 2, CFGFLAG_SERVER, "Whether other people see UFO players hookcoll when flying down")
MACRO_CONFIG_INT(SvUfoBrakes, sv_ufo_brakes, 0, 0, 1, CFGFLAG_SERVER, "Allows the UFO to instantly stop and stay still if player is flying up, down, and holding Fire")

// Quiet Join
MACRO_CONFIG_INT(SvQuietJoin, sv_quiet_join, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to disable the join message for players with the right password")
MACRO_CONFIG_STR(SvQuietJoinPassword, sv_quiet_join_password, 128, "", CFGFLAG_SERVER | CFGFLAG_GAME, "Password if QuietJoin is enabled")

// Quads
MACRO_CONFIG_INT(SvInstantCoreUpdate, sv_instant_core_update, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Sends Info about a player every tick, even if not doing anything")
MACRO_CONFIG_INT(SvDebugQuadPos, sv_debug_quad_pos, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Shows interactive quad positions using lasers")
MACRO_CONFIG_INT(SvQStopaGivesDj, sv_qstopa_gives_dj, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether the QStopa quad should give dj back")

MACRO_CONFIG_INT(SvDebugIdPool, sv_debug_id_pool, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Debug Id allocation")

// Abilities
MACRO_CONFIG_INT(SvNoAuthCooldown, sv_no_auth_cooldown, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "whether theres a cooldown for abilities on authed players")

// Weapon Drops
MACRO_CONFIG_INT(SvAllowWeaponDrops, sv_allow_weapon_drops, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Turns on functionality for /weapondrop")
MACRO_CONFIG_INT(SvDropWeaponVoteNo, sv_drop_weapon_vote_no, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "requires sv_allow_weapon_drops, drop weapons using f4 (vote no)")
MACRO_CONFIG_INT(SvResetDropsOnLeave, sv_reset_drops_on_leave, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "If a player leaves while he was weapons dropped, they get reset")
MACRO_CONFIG_INT(SvDropWeaponOnDeath, sv_drop_weapon_on_death, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Requires sv_allow_weapon_drops")
MACRO_CONFIG_INT(SvDropsInFreezeFloat, sv_drops_in_freeze_float, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Make Weapon Drops in freeze float up")

// PowerUps
MACRO_CONFIG_INT(SvSpawnPowerUps, sv_spawn_powerups, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to spawn powerups randomly in the map")

// Solo on Spawn
MACRO_CONFIG_INT(SvSoloOnSpawn, sv_solo_on_spawn, 5, 0, 15, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether Players Should be solod on spawn + how long in seconds")

// Flags
MACRO_CONFIG_INT(SvAllowZoom, sv_allow_zoom, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to allow zoom or not")
MACRO_CONFIG_INT(SvAllowHookColl, sv_allow_hook_coll, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to allow hook coll or not")
MACRO_CONFIG_INT(SvAllowEyeWheel, sv_allow_eye_wheel, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to allow eye wheel or not")

// Cosmetics
MACRO_CONFIG_INT(SvCosmeticLimit, sv_cosmetic_limit, 5, 0, 25, CFGFLAG_SERVER | CFGFLAG_GAME, "How many cosmetics a player can have at a time")