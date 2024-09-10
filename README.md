[![DDraceNetwork](other/ddnet-insta.png)](https://ddnet.tw) [![](https://github.com/ddnet-insta/ddnet-insta/workflows/Build/badge.svg)](https://github.com/ddnet-insta/ddnet-insta/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

DDNet-insta based on DDRaceNetwork, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

For build instructions visit the [ddnet repo](https://github.com/ddnet/ddnet).

---

A ddnet based pvp mod. With the focus on correct 0.6 and 0.7 support and staying close to and up to date with ddnet.
While being highly configurable and feature rich.

Implementing most of the relevant pvp gametypes: gctf, ictf, gdm, idm, gtdm, itdm, zCatch


Planned gametypes are: fng, ctf, dm

# Features

## checkbox votes

If a vote is added starting with a ``[ ]`` in the display name. It will be used as a checkbox.
If the underlying config is currently set that checkbox will be ticked and users see ``[x]`` in the vote menu.
This feature is optional and if you do not put any  ``[ ]`` in your config it will not be using any checkboxes.
It is only applied for ddnet-insta settings not for all ddnet configs.
It is recommended to set ``sv_vote_checkboxes 0`` at the start of your autoexec and ``sv_vote_checkboxes 1``
at the end so it does not update all votes for every setting it loads.

![checkbox votes](https://raw.githubusercontent.com/ddnet-insta/images/c6c3e871a844fa06b460b8be61ba0ff01d0a82f6/checkbox_votes.png)

## unstack chat for ddnet clients

Newer DDNet clients do not show duplicated messages multiple times. This is not always wanted when using call binds for team communication during pvp games. So there is ``sv_unstack_chat`` to revert that ddnet feature and ensure every message is sent properly in chat.

![unstack_chat](https://raw.githubusercontent.com/ddnet-insta/images/3c437acdea599788fb245518e9c25de7c0e63795/unstack_chat.png)

## 0.6 and 0.7 support including ready change

ddnet-insta uses the 0.6/0.7 server side version bridge from ddnet. So all gametypes are playable by latest teeworlds clients and ddnet clients at the same time.

In 0.7 there was a ready change added which allows users to pause the game. It only continues when everyone presses the ready bind.
This feature is now also possible for 0.6 clients using the /pause chat command. This feature should be turned off on public servers ``sv_player_ready_mode 0`` because it will be used by trolls.

![pause game](https://raw.githubusercontent.com/ddnet-insta/images/1a2d10c893605d704aeea8320cf0e65f8e0c2aa3/ready_change.png)

## 0.7 dead players in zCatch

In 0.6 dead players join the spectators team in the zCatch gamemode.

![zCatch 0.6](https://raw.githubusercontent.com/ddnet-insta/images/master/zCatch_teetime_06.png)

In 0.7 they are marked as dead players and are separate from spectators.

![zCatch 0.7](https://raw.githubusercontent.com/ddnet-insta/images/master/zCatch_teetime_07.png)

## Allow spectator votes for 0.7

The official teeworlds 0.7 client does block voting on the client side for spectators.
To make `sv_spectator_votes` portable and fair for both 0.6 and 0.7 players there is an option to allow
0.7 clients to vote as spectators. It is a bit hacky so it is hidden behind then config `sv_spectator_votes_sixup 1`
when that is set it will make the 0.7 clients believe they are in game and unlock the call vote menu.
But this also means that to join the game the users have to press the "spectate" button.

## gametype support

### iCTF

``sv_gametype iCTF``

Instagib capture the flag. Is a team based mode where every player only has a laser rifle.
It kills with one shot and capturing the enemy flag scores your team 100 points.

### gCTF

``sv_gametype gCTF``

Grenade capture the flag. Is a team based mode where every player only has a rocket launcher.
It kills with one shot and capturing the enemy flag scores your team 100 points.

### iDM

``sv_gametype iDM``

Laser death match. One shot kills. First to reach the scorelimit wins.

### gDM

``sv_gametype gDM``

Grenade death match. One shot kills. First to reach the scorelimit wins.

### iTDM

``sv_gametype iTDM``

Laser team death match. One shot kills. First team to reach the scorelimit wins.

### gTDM

``sv_gametype gTDM``

Grenade team death match. One shot kills. First team to reach the scorelimit wins.

### zCatch

``sv_gametype zCatch``

If you get killed you stay dead until your killer dies. Last man standing wins.
It is an instagib gametype so one shot kills. You can choose the weapon with
`sv_spawn_weapons` the options are `grenade` or `laser`.

# Configs

+ `sv_gametype` Game type (gctf, ictf, gdm, idm, gtdm, itdm, zcatch)
+ `sv_spectator_votes` Allow spectators to vote
+ `sv_spectator_votes_sixup` Allow 0.7 players to vote as spec if sv_spectator_vote is 1 (hacky dead spec)
+ `sv_countdown_unpause` Number of seconds to freeze the game in a countdown before match continues after pause
+ `sv_countdown_round_start` Number of seconds to freeze the game in a countdown before match starts (0 enables only for survival gamemodes, -1 disables)
+ `sv_scorelimit` Score limit (0 disables)
+ `sv_timelimit` Time limit in minutes (0 disables)
+ `sv_player_ready_mode` When enabled, players can pause/unpause the game and start the game on warmup via their ready state
+ `sv_force_ready_all` minutes after which a game will be force unpaused (0=off) related to sv_player_ready_mode
+ `sv_powerups` Allow powerups like ninja
+ `sv_grenade_ammo_regen` Activate or deactivate grenade ammo regeneration in general
+ `sv_grenade_ammo_regen_time` Grenade ammo regeneration time in miliseconds
+ `sv_grenade_ammo_regen_num` Maximum number of grenades if ammo regeneration on
+ `sv_grenade_ammo_regen_speed` Give grenades back that push own player
+ `sv_grenade_ammo_regen_on_kill` Refill nades on kill (0=off, 1=1, 2=all)
+ `sv_grenade_ammo_regen_reset_on_fire` Reset regen time if shot is fired
+ `sv_sprayprotection` Spray protection
+ `sv_only_hook_kills` Only count kills when enemy is hooked
+ `sv_kill_hook` Hook kills
+ `sv_killingspree_kills` How many kills are needed to be on a killing-spree (0=off)
+ `sv_damage_needed_for_kill` Damage needed to kill
+ `sv_allow_zoom` allow ddnet clients to use the client side zoom feature
+ `sv_anticamper` Toggle to enable/disable Anticamper
+ `sv_anticamper_freeze` If a player should freeze on camping (and how long) or die
+ `sv_anticamper_time` How long to wait till the player dies/freezes
+ `sv_anticamper_range` Distance how far away the player must move to escape anticamper
+ `sv_zcatch_min_players` How many active players (not spectators) are required to start a round
+ `sv_release_game` auto release on kill (only affects sv_gametype zCatch)
+ `sv_drop_flag_on_selfkill` drop flag on selfkill (activates chat cmd '/drop flag')
+ `sv_drop_flag_on_vote` drop flag on vote yes (activates chat cmd '/drop flag')
+ `sv_on_fire_mode` no reload delay after hitting an enemy with rifle
+ `sv_hammer_scale_x` (fng) linearly scale up hammer x power, percentage, for hammering enemies and unfrozen teammates
+ `sv_hammer_scale_y` (fng) linearly scale up hammer y power, percentage, for hammering enemies and unfrozen teammates
+ `sv_melt_hammer_scale_x` (fng) linearly scale up hammer x power, percentage, for hammering frozen teammates
+ `sv_melt_hammer_scale_y` (fng) linearly scale up hammer y power, percentage, for hammering frozen teammates
+ `sv_fng_hammer` (fng only) use sv_hammer_scale_x/y and sv_melt_hammer_scale_x/y tuning for hammer
+ `sv_punish_freeze_disconnect` (fng) 0=off otherwise bantime in minutes when leaving server while being frozen
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
+ `sv_tournament_chat` 0=off, 1=Spectators can not public chat, 2=Nobody can public chat
+ `sv_tournament_chat_smart` Turns sv_tournament_chat on on restart and off on round end (1=specs,2=all)
+ `sv_tournament_join_msgs` Hide join/leave of spectators in chat !0.6 only for now! (0=off,1=hidden,2=shown for specs)
+ `sv_round_stats_format_discord` 0=csv 1=psv 2=ascii table 3=markdown table 4=json
+ `sv_round_stats_format_http` 0=csv 1=psv 2=ascii table 3=markdown table 4=json
+ `sv_round_stats_format_file` 0=csv 1=psv 2=ascii table 3=markdown table 4=json
+ `sv_spawn_weapons` possible values: grenade, laser
+ `sv_zcatch_colors` Color scheme for zCatch options: teetime, savander
+ `sv_tournament_welcome_chat` Chat message shown in chat on join when sv_tournament is 1
+ `sv_round_stats_discord_webhook` If set will post score stats there on round end
+ `sv_round_stats_http_endpoint` If set will post score stats there on round end
+ `sv_round_stats_output_file` If set will write score stats there on round end

# Rcon commmands

+ `hammer` Gives a hammer to you
+ `gun` Gives a gun to you
+ `unhammer` Removes a hammer from you
+ `ungun` Removes a gun from you
+ `godmode` Removes damage
+ `force_ready` Sets a player to ready (when the game is paused)
+ `shuffle_teams` Shuffle the current teams
+ `swap_teams` Swap the current teams
+ `swap_teams_random` Swap the current teams or not (random chance)

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
+ `/drop flag` if it is a CTF gametype the flagger can drop the flag without dieing if either `sv_drop_flag_on_selfkill` or `sv_drop_flag_on_vote` is set

# Publish round stats

At the end of every round the stats about every players score can be published to discord (and other destinations).

The following configs determine which format the stats will be represented in.

+ `sv_round_stats_format_discord` 0=csv 1=psv 2=ascii table 3=markdown table 4=json
+ `sv_round_stats_format_http` 0=csv 1=psv 2=ascii table 3=markdown table 4=json
+ `sv_round_stats_format_file` 0=csv 1=psv 2=ascii table 3=markdown table 4=json

And these configs determin where the stats will be sent to.

+ `sv_round_stats_discord_webhook` Will do a discord webhook POST request to that url. The url has to look like this: `https://discord.com/api/webhooks/1232962289217568799/8i_a89XXXXXXXXXXXXXXXXXXXXXXX`
  If you don't know how to setup a discord webhook, don't worry its quite simple. You need to have admin access to a discord server and then you can follow this [1 minute youtube tutorial](https://www.youtube.com/watch?v=fKksxz2Gdnc).
+ `sv_round_stats_http_endpoint` It will do a http POST request to that url with the round stats as payload. You can set this to your custom api endpoint that collect stats. Example: `https://api.zillyhuhn.com/insta/round_stats`
+ `sv_round_stats_output_file` **NOT IMPLEMENTED YET** It will write the round stats to a file located at that path. You could then read that file with another tool or expose it with an http server. Example value: `stats.json`

## csv - comma separated values (format 0)

NOT IMPLEMENTED YET

## psv - pipe separated values (format 1)

```
---> Server: unnamed server, Map: tmp/maps-07/ctf5_spikes, Gametype: gctf.
(Length: 0 min 17 sec, Scorelimit: 1, Timelimit: 0)

**Red Team:**                                       
Clan: **|*KoG*|**                                   
Id: 0 | Name: ChillerDragon | Score: 2 | Kills: 1 | Deaths: 0 | Ratio: 1.00
**Blue Team:**                                      
Clan: **|*KoG*|**                                   
Id: 1 | Name: ChillerDragon.* | Score: 0 | Kills: 0 | Deaths: 1 | Ratio: 0.00
---------------------                               
**Red: 1 | Blue 0**  
```

Here is how it would display when posted on discord:

![psv on discord](https://raw.githubusercontent.com/ddnet-insta/images/5fafe03ed60153096facf4cc5d56c5df9ff20a5c/psv_discord.png)

## ascii table (format 2)

NOT IMPLEMENTED YET

## markdown (format 3)

NOT IMPLEMENTED YET

## json - javascript object notation (format 4)

```json
{
        "server": "unnamed server",
        "map": "tmp/maps-07/ctf5_spikes",
        "game_type": "gctf",
        "game_duration_seconds": 67,
        "score_limit": 200,
        "time_limit": 0,
        "score_red": 203,
        "score_blue": 0,
        "players": [
                {
                        "id": 0,
                        "team": "red",
                        "name": "ChillerDragon",
                        "score": 15,
                        "kills": 3,
                        "deaths": 1,
                        "ratio": 3,
                        "flag_grabs": 3,
                        "flag_captures": 2
                },
                {
                        "id": 1,
                        "team": "blue",
                        "name": "ChillerDragon.*",
                        "score": 0,
                        "kills": 0,
                        "deaths": 3,
                        "ratio": 0,
                        "flag_grabs": 0,
                        "flag_captures": 0
                }
        ]
}
```
