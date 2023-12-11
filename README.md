[![DDraceNetwork](other/ddnet-insta.png)](https://ddnet.tw) [![](https://github.com/ZillyInsta/ddnet-insta/workflows/Build/badge.svg)](https://github.com/ZillyInsta/ddnet-insta/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

DDNet-insta based on DDRaceNetwork, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

For build instructions visit the [ddnet repo](https://github.com/ddnet/ddnet).

# Configs

+ `sv_spectator_votes` - Allow spectators to vote
+ `sv_countdown_unpause` - Number of seconds to freeze the game in a countdown before match continues after pause
+ `sv_countdown_round_start` - Number of seconds to freeze the game in a countdown before match starts (0 enables only for survival gamemodes, -1 disables)
+ `sv_scorelimit` Score limit (0 disables)
+ `sv_timelimit` Time limit in minutes (0 disables)
+ `sv_gametype` Game type (gctf, ictf)
+ `sv_player_ready_mode` When enabled, players can pause/unpause the game and start the game on warmup via their ready state
+ `sv_grenade_ammo_regen` Activate or deactivate grenade ammo regeneration in general
+ `sv_grenade_ammo_regen_time` Grenade ammo regeneration time in miliseconds
+ `sv_grenade_ammo_regen_num` Maximum number of grenades if ammo regeneration on
+ `sv_grenade_ammo_regen_speed` Give grenades back that push own player
+ `sv_grenade_ammo_regen_on_kill` Give grenades back on kill (0=0ff, 1=one nade back, 2=all nades back)
+ `sv_grenade_ammo_regen_reset_on_fire` Reset regen time if shot is fired
+ `sv_sprayprotection` Spray protection
+ `sv_only_hook_kills` Only count kills when enemy is hooked
+ `sv_kill_hook` Hooking kills
+ `sv_killingspree_kills` How many kills are needed to be on a killing-spree (0=off)
+ `sv_chat_ratelimit_long_messages` Needs sv_spamprotection 0 (0=off, 1=only messages longer than 12 chars are limited)
+ `sv_chat_ratelimit_spectators` Needs sv_spamprotection 0 (0=off, 1=specs have slow chat)
+ `sv_chat_ratelimit_public_chat` Needs sv_spamprotection 0 (0=off, 1=non team chat is slow)
+ `sv_chat_ratelimit_non_calls` Needs sv_spamprotection 0 (0=off, 1=ratelimit all but call binds such as 'help')
+ `sv_chat_ratelimit_spam` Needs sv_spamprotection 0 (0=off, 1=ratelimit chat detected as spam)
+ `sv_chat_ratelimit_debug` Logs which of the ratelimits kicked in
+ `sv_fastcap` Insert flag captures into ddrace rank database
+ `sv_vote_checkboxes` Fill [ ] checkbox in vote name if the config is already set
+ `sv_hide_admins` Only send admin status to other authed players
+ `sv_show_settings_motd` Show insta game settings in motd on join
+ `sv_unstack_chat` Revert ddnet clients duplicated chat message stacking
+ `sv_tournament` Print messages saying tournament is running. No other effects.
+ `sv_tournament_welcome_chat` Chat message shown in chat on join when sv_tournament is 1
+ `sv_tournament_chat` 0=off, 1=Spectators can not public chat, 2=Nobody can public chat
+ `sv_tournament_chat_smart` s sv_tournament_chat on on restart and off on round end (1=specs,2=all)
+ `sv_tournament_join_msgs` Hide join/leave of spectators in chat (0=off,1=hidden,2=shown for specs)

# Rcon commmands

+ `shuffle_teams` Shuffle the current teams
+ `swap_teams` Swap the current teams
+ `swap_teams_random` Swap the current teams or not (random chance)
+ `godmode ?v[id]` Removes damage


# Chat commands

Most ddnet slash chat commands were inherited and are still functional.
But /pause and /spec got removed since it is conflicting with pausing games and usually not wanted for pvp games.

ddnet-insta then added a bunch of own slash chat commands and also bang (!) chat commands

+ `!ready` `!pause` `/pause` `/ready` to pause the game. Needs `sv_player_ready_mode 1` and 0.7 clients can also send the 0.7 ready change message
+ `!shuffle` `/shuffle` call vote to shuffle teams
+ `!swap` `/swap` call vote to swap teams
+ `!swap_random` `/swap_random` call vote to swap teams to random sides
+ `!settings` show current game settings in the message of the day. It will show if spray protection is on or off and similar game relevant settings.
+ `!1v1` `!2v2` `!v1` `!v2` `!1on1` ... call vote to change in game slots
+ `!restart ?(seconds)` call vote to restart game with optional parameter of warmup seconds (default: 10)
