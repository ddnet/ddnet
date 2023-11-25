[![DDraceNetwork](other/ddnet-insta.png)](https://ddnet.tw) [![](https://github.com/ZillyInsta/ddnet-insta/workflows/Build/badge.svg)](https://github.com/ZillyInsta/ddnet-insta/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

DDNet-insta based on DDRaceNetwork, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

For build instructions visit the [ddnet repo](https://github.com/ddnet/ddnet).

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
+ `sv_damage_needed_for_kill` Damage needed to kill
+ `sv_show_settings_motd` Show insta game settings in motd on join
+ `sv_unstack_chat` Revert ddnet clients duplicated chat message stacking
