// ddnet-insta config variables
#ifndef GAME_VARIABLES_INSTA_H
#define GAME_VARIABLES_INSTA_H
#undef GAME_VARIABLES_INSTA_H // this file will be included several times

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

MACRO_CONFIG_INT(SvSpectatorVotes, sv_spectator_votes, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow spectators to vote")

MACRO_CONFIG_INT(SvCountdownUnpause, sv_countdown_unpause, 0, -1, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of seconds to freeze the game in a countdown before match continues after pause")
MACRO_CONFIG_INT(SvCountdownRoundStart, sv_countdown_round_start, 0, -1, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of seconds to freeze the game in a countdown before match starts (0 enables only for survival gamemodes, -1 disables)")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 600, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state")

MACRO_CONFIG_INT(SvGrenadeAmmoRegen, sv_grenade_ammo_regen, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Activate or deactivate grenade ammo regeneration in general")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenTime, sv_grenade_ammo_regen_time, 128, 1, 9000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Grenade ammo regeneration time in miliseconds")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenNum, sv_grenade_ammo_regen_num, 6, 1, 10, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of grenades if ammo regeneration on")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenSpeedNade, sv_grenade_ammo_regen_speed, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Give grenades back that push own player")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenOnKill, sv_grenade_ammo_regen_on_kill, 2, 0, 2, CFGFLAG_SAVE | CFGFLAG_SERVER, "Refill nades on kill (0=off, 1=1, 2=all)")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenResetOnFire, sv_grenade_ammo_regen_reset_on_fire, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Reset regen time if shot is fired")
MACRO_CONFIG_INT(SvSprayprotection, sv_sprayprotection, 0, 0, 1, CFGFLAG_SERVER, "Spray protection")
MACRO_CONFIG_INT(SvOnlyHookKills, sv_only_hook_kills, 0, 0, 1, CFGFLAG_SERVER, "Only count kills when enemy is hooked")
MACRO_CONFIG_INT(SvKillHook, sv_kill_hook, 0, 0, 1, CFGFLAG_SERVER, "Hook kills")

MACRO_CONFIG_INT(SvFastcap, sv_fastcap, 0, 0, 1, CFGFLAG_SERVER, "Insert flag captures into ddrace rank database")

MACRO_CONFIG_INT(SvShowSettingsMotd, sv_show_settings_motd, 1, 0, 1, CFGFLAG_SERVER, "Show insta game settings in motd on join")

#endif
