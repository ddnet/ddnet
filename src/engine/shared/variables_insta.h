// ddnet-insta config variables
#ifndef ENGINE_SHARED_VARIABLES_INSTA_H
#define ENGINE_SHARED_VARIABLES_INSTA_H
#undef ENGINE_SHARED_VARIABLES_INSTA_H // this file will be included several times

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

MACRO_CONFIG_INT(SvSpectatorVotes, sv_spectator_votes, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow spectators to vote")
MACRO_CONFIG_INT(SvSpectatorVotesSixup, sv_spectator_votes_sixup, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow 0.7 players to vote as spec if sv_spectator_vote is 1 (hacky dead spec)")

MACRO_CONFIG_INT(SvCountdownUnpause, sv_countdown_unpause, 0, -1, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of seconds to freeze the game in a countdown before match continues after pause")
MACRO_CONFIG_INT(SvCountdownRoundStart, sv_countdown_round_start, 0, -1, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of seconds to freeze the game in a countdown before match starts (0 enables only for survival gamemodes, -1 disables)")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 600, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state")
MACRO_CONFIG_INT(SvForceReadyAll, sv_force_ready_all, 0, 0, 60, CFGFLAG_SAVE | CFGFLAG_SERVER, "minutes after which a game will be force unpaused (0=off) related to sv_player_ready_mode")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow powerups like ninja")

MACRO_CONFIG_INT(SvGrenadeAmmoRegen, sv_grenade_ammo_regen, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Activate or deactivate grenade ammo regeneration in general")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenTime, sv_grenade_ammo_regen_time, 128, 1, 9000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Grenade ammo regeneration time in miliseconds")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenNum, sv_grenade_ammo_regen_num, 6, 1, 10, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of grenades if ammo regeneration on")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenSpeedNade, sv_grenade_ammo_regen_speed, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Give grenades back that push own player")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenOnKill, sv_grenade_ammo_regen_on_kill, 2, 0, 2, CFGFLAG_SAVE | CFGFLAG_SERVER, "Refill nades on kill (0=off, 1=1, 2=all)")
MACRO_CONFIG_INT(SvGrenadeAmmoRegenResetOnFire, sv_grenade_ammo_regen_reset_on_fire, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Reset regen time if shot is fired")
MACRO_CONFIG_INT(SvSprayprotection, sv_sprayprotection, 0, 0, 1, CFGFLAG_SERVER, "Spray protection")
MACRO_CONFIG_INT(SvOnlyHookKills, sv_only_hook_kills, 0, 0, 1, CFGFLAG_SERVER, "Only count kills when enemy is hooked")
MACRO_CONFIG_INT(SvKillHook, sv_kill_hook, 0, 0, 1, CFGFLAG_SERVER, "Hook kills")
MACRO_CONFIG_INT(SvKillingspreeKills, sv_killingspree_kills, 0, 0, 20, CFGFLAG_SERVER, "How many kills are needed to be on a killing-spree (0=off)")
MACRO_CONFIG_INT(SvDamageNeededForKill, sv_damage_needed_for_kill, 4, 0, 5, CFGFLAG_SERVER, "Damage needed to kill")
MACRO_CONFIG_INT(SvAllowZoom, sv_allow_zoom, 0, 0, 1, CFGFLAG_SERVER, "allow ddnet clients to use the client side zoom feature")
MACRO_CONFIG_STR(SvSpawnWeapons, sv_spawn_weapons, 900, "grenade", CFGFLAG_SERVER, "possible values: grenade, laser")
MACRO_CONFIG_INT(SvAnticamper, sv_anticamper, 0, 0, 1, CFGFLAG_SERVER, "Toggle to enable/disable Anticamper")
MACRO_CONFIG_INT(SvAnticamperFreeze, sv_anticamper_freeze, 7, 0, 15, CFGFLAG_SERVER, "If a player should freeze on camping (and how long) or die")
MACRO_CONFIG_INT(SvAnticamperTime, sv_anticamper_time, 10, 5, 120, CFGFLAG_SERVER, "How long to wait till the player dies/freezes")
MACRO_CONFIG_INT(SvAnticamperRange, sv_anticamper_range, 200, 0, 1000, CFGFLAG_SERVER, "Distance how far away the player must move to escape anticamper")
MACRO_CONFIG_INT(SvZcatchMinPlayers, sv_zcatch_min_players, 3, 0, MAX_CLIENTS, CFGFLAG_SERVER, "How many active players (not spectators) are required to start a round")
MACRO_CONFIG_INT(SvReleaseGame, sv_release_game, 0, 0, 1, CFGFLAG_SERVER, "auto release on kill (only affects sv_gametype zCatch)")
MACRO_CONFIG_STR(SvZcatchColors, sv_zcatch_colors, 512, "teetime", CFGFLAG_SERVER, "Color scheme for zCatch options: teetime, savander")
MACRO_CONFIG_INT(SvDropFlagOnSelfkill, sv_drop_flag_on_selfkill, 0, 0, 1, CFGFLAG_SERVER, "drop flag on selfkill (activates chat cmd '/drop flag')")
MACRO_CONFIG_INT(SvDropFlagOnVote, sv_drop_flag_on_vote, 0, 0, 1, CFGFLAG_SERVER, "drop flag on vote yes (activates chat cmd '/drop flag')")
MACRO_CONFIG_INT(SvOnFireMode, sv_on_fire_mode, 0, 0, 1, CFGFLAG_SERVER, "no reload delay after hitting an enemy with rifle")
MACRO_CONFIG_INT(SvHammerScaleX, sv_hammer_scale_x, 320, 1, 1000, CFGFLAG_SERVER, "(fng) linearly scale up hammer x power, percentage, for hammering enemies and unfrozen teammates")
MACRO_CONFIG_INT(SvHammerScaleY, sv_hammer_scale_y, 120, 1, 1000, CFGFLAG_SERVER, "(fng) linearly scale up hammer y power, percentage, for hammering enemies and unfrozen teammates")
MACRO_CONFIG_INT(SvMeltHammerScaleX, sv_melt_hammer_scale_x, 50, 1, 1000, CFGFLAG_SERVER, "(fng) linearly scale up hammer x power, percentage, for hammering frozen teammates")
MACRO_CONFIG_INT(SvMeltHammerScaleY, sv_melt_hammer_scale_y, 50, 1, 1000, CFGFLAG_SERVER, "(fng) linearly scale up hammer y power, percentage, for hammering frozen teammates")
MACRO_CONFIG_INT(SvFngHammer, sv_fng_hammer, 1, 0, 1, CFGFLAG_SERVER, "(fng only) use sv_hammer_scale_x/y and sv_melt_hammer_scale_x/y tuning for hammer")

/*

sv_chat_ratelimit_long_messages

12 is the magic max size of team call binds
TODO: count characters not size because 🅷🅴🅻🅿 is longer than 12 chars

         0123456789
1234567891111111111
           |
top        |
bottom     |
help!      |
come base  |
need help  |

*/
MACRO_CONFIG_INT(SvChatRatelimitLongMessages, sv_chat_ratelimit_long_messages, 1, 0, 1, CFGFLAG_SERVER, "Needs sv_spamprotection 0 (0=off, 1=only messages longer than 12 chars are limited)")
MACRO_CONFIG_INT(SvChatRatelimitSpectators, sv_chat_ratelimit_spectators, 1, 0, 1, CFGFLAG_SERVER, "Needs sv_spamprotection 0 (0=off, 1=specs have slow chat)")
MACRO_CONFIG_INT(SvChatRatelimitPublicChat, sv_chat_ratelimit_public_chat, 1, 0, 1, CFGFLAG_SERVER, "Needs sv_spamprotection 0 (0=off, 1=non team chat is slow)")
MACRO_CONFIG_INT(SvChatRatelimitNonCalls, sv_chat_ratelimit_non_calls, 1, 0, 1, CFGFLAG_SERVER, "Needs sv_spamprotection 0 (0=off, 1=ratelimit all but call binds such as 'help')")
MACRO_CONFIG_INT(SvChatRatelimitSpam, sv_chat_ratelimit_spam, 1, 0, 1, CFGFLAG_SERVER, "Needs sv_spamprotection 0 (0=off, 1=ratelimit chat detected as spam)")
MACRO_CONFIG_INT(SvChatRatelimitDebug, sv_chat_ratelimit_debug, 0, 0, 1, CFGFLAG_SERVER, "Logs which of the ratelimits kicked in")

MACRO_CONFIG_INT(SvFastcap, sv_fastcap, 0, 0, 1, CFGFLAG_SERVER, "Insert flag captures into ddrace rank database")

MACRO_CONFIG_INT(SvVoteCheckboxes, sv_vote_checkboxes, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Fill [ ] checkbox in vote name if the config is already set")
MACRO_CONFIG_INT(SvHideAdmins, sv_hide_admins, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Only send admin status to other authed players")
MACRO_CONFIG_INT(SvShowSettingsMotd, sv_show_settings_motd, 1, 0, 1, CFGFLAG_SERVER, "Show insta game settings in motd on join")
MACRO_CONFIG_INT(SvUnstackChat, sv_unstack_chat, 1, 0, 1, CFGFLAG_SERVER, "Revert ddnet clients duplicated chat message stacking")
// MACRO_CONFIG_INT(SvCasualRounds, sv_casual_rounds, 1, 0, 1, CFGFLAG_SERVER, "1=start rounds automatically, 2=require restart vote to properly start game")
MACRO_CONFIG_INT(SvTournament, sv_tournament, 0, 0, 1, CFGFLAG_SERVER, "Print messages saying tournament is running. No other effects.")
MACRO_CONFIG_STR(SvTournamentWelcomeChat, sv_tournament_welcome_chat, 900, "", CFGFLAG_SERVER, "Chat message shown in chat on join when sv_tournament is 1")
MACRO_CONFIG_INT(SvTournamentChat, sv_tournament_chat, 0, 0, 2, CFGFLAG_SERVER, "0=off, 1=Spectators can not public chat, 2=Nobody can public chat")
MACRO_CONFIG_INT(SvTournamentChatSmart, sv_tournament_chat_smart, 0, 0, 2, CFGFLAG_SERVER, "Turns sv_tournament_chat on on restart and off on round end (1=specs,2=all)")
MACRO_CONFIG_INT(SvTournamentJoinMsgs, sv_tournament_join_msgs, 0, 0, 2, CFGFLAG_SERVER, "Hide join/leave of spectators in chat !0.6 only for now! (0=off,1=hidden,2=shown for specs)")
MACRO_CONFIG_STR(SvRoundStatsDiscordWebhook, sv_round_stats_discord_webhook, 512, "", CFGFLAG_SERVER, "If set will post score stats there on round end")
MACRO_CONFIG_STR(SvRoundStatsHttpEndpoint, sv_round_stats_http_endpoint, 512, "", CFGFLAG_SERVER, "If set will post score stats there on round end")
MACRO_CONFIG_STR(SvRoundStatsOutputFile, sv_round_stats_output_file, 512, "", CFGFLAG_SERVER, "If set will write score stats there on round end")
MACRO_CONFIG_INT(SvRoundStatsFormatDiscord, sv_round_stats_format_discord, 1, 0, 4, CFGFLAG_SERVER, "0=csv 1=psv 2=ascii table 3=markdown table 4=json")
MACRO_CONFIG_INT(SvRoundStatsFormatHttp, sv_round_stats_format_http, 4, 0, 4, CFGFLAG_SERVER, "0=csv 1=psv 2=ascii table 3=markdown table 4=json")
MACRO_CONFIG_INT(SvRoundStatsFormatFile, sv_round_stats_format_file, 1, 0, 4, CFGFLAG_SERVER, "0=csv 1=psv 2=ascii table 3=markdown table 4=json")

#endif
