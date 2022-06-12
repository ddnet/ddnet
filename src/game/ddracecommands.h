/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill")
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToTeleporter, this, "Teleports you to teleporter v")
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v")
CONSOLE_COMMAND("tele", "?i[id] ?i[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConTeleport, this, "Teleports player i (or you) to player i (or you to where you look at)")
CONSOLE_COMMAND("addweapon", "i[weapon-id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)")
CONSOLE_COMMAND("removeweapon", "i[weapon-id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)")
CONSOLE_COMMAND("shotgun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConShotgun, this, "Gives a shotgun to you")
CONSOLE_COMMAND("grenade", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGrenade, this, "Gives a grenade launcher to you")
CONSOLE_COMMAND("laser", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLaser, this, "Gives a laser to you")
CONSOLE_COMMAND("rifle", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLaser, this, "Gives a laser to you")
CONSOLE_COMMAND("jetpack", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConJetpack, this, "Gives jetpack to you")
CONSOLE_COMMAND("weapons", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConWeapons, this, "Gives all weapons to you")
CONSOLE_COMMAND("unshotgun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnShotgun, this, "Removes the shotgun from you")
CONSOLE_COMMAND("ungrenade", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnGrenade, this, "Removes the grenade launcher from you")
CONSOLE_COMMAND("unlaser", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLaser, this, "Removes the laser from you")
CONSOLE_COMMAND("unrifle", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLaser, this, "Removes the laser from you")
CONSOLE_COMMAND("unjetpack", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnJetpack, this, "Removes the jetpack from you")
CONSOLE_COMMAND("unweapons", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnWeapons, this, "Removes all weapons from you")
CONSOLE_COMMAND("ninja", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConNinja, this, "Makes you a ninja")
CONSOLE_COMMAND("super", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConSuper, this, "Makes you super")
CONSOLE_COMMAND("unsuper", "", CFGFLAG_SERVER, ConUnSuper, this, "Removes super from you")
CONSOLE_COMMAND("endless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConEndlessHook, this, "Gives you endless hook")
CONSOLE_COMMAND("unendless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnEndlessHook, this, "Removes endless hook from you")
CONSOLE_COMMAND("unsolo", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnSolo, this, "Puts you out of solo part")
CONSOLE_COMMAND("undeep", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnDeep, this, "Puts you out of deep freeze")
CONSOLE_COMMAND("livefreeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLiveFreeze, this, "Makes you live frozen")
CONSOLE_COMMAND("unlivefreeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLiveFreeze, this, "Puts you out of live freeze")
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left")
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right")
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up")
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down")

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii")
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii")
CONSOLE_COMMAND("force_pause", "v[id] i[seconds]", CFGFLAG_SERVER, ConForcePause, this, "Force i to pause for i seconds")
CONSOLE_COMMAND("force_unpause", "v[id]", CFGFLAG_SERVER, ConForcePause, this, "Set force-pause timer of i to 0.")

CONSOLE_COMMAND("set_team_ddr", "v[id] ?i[team]", CFGFLAG_SERVER, ConSetDDRTeam, this, "Set ddrace team of a player")
CONSOLE_COMMAND("uninvite", "v[id] ?i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team")

CONSOLE_COMMAND("vote_mute", "v[id] i[seconds]", CFGFLAG_SERVER, ConVoteMute, this, "Remove v's right to vote for i seconds")
CONSOLE_COMMAND("vote_unmute", "v[id]", CFGFLAG_SERVER, ConVoteUnmute, this, "Give back v's right to vote.")
CONSOLE_COMMAND("vote_mutes", "", CFGFLAG_SERVER, ConVoteMutes, this, "List the current active vote mutes.")
CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "")
CONSOLE_COMMAND("muteid", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteID, this, "Mute player with id")
CONSOLE_COMMAND("muteip", "s[ip] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteIP, this, "Mute player with IP address")
CONSOLE_COMMAND("unmute", "i[muteid]", CFGFLAG_SERVER, ConUnmute, this, "Unmute mute with number from \"mutes\"")
CONSOLE_COMMAND("unmuteid", "v[id]", CFGFLAG_SERVER, ConUnmuteID, this, "Unmute player with id")
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "")
CONSOLE_COMMAND("moderate", "", CFGFLAG_SERVER, ConModerate, this, "Enables/disables active moderator mode for the player")
CONSOLE_COMMAND("vote_no", "", CFGFLAG_SERVER, ConVoteNo, this, "Same as \"vote no\"")
CONSOLE_COMMAND("save_dry", "", CFGFLAG_SERVER, ConDrySave, this, "Dump the current savestring")

CONSOLE_COMMAND("freezehammer", "v[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConFreezeHammer, this, "Gives a player Freeze Hammer")
CONSOLE_COMMAND("unfreezehammer", "v[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnFreezeHammer, this, "Removes Freeze Hammer from a player")
#undef CONSOLE_COMMAND
