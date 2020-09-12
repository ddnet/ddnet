/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill")
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConToTeleporter, this, "Teleports you to teleporter v")
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v")
CONSOLE_COMMAND("tele", "?i[id] ?i[id]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConTeleport, this, "Teleports player i (or you) to player i (or you to where you look at)")
CONSOLE_COMMAND("addweapon", "i[weapon-id]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)")
CONSOLE_COMMAND("removeweapon", "i[weapon-id]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)")
CONSOLE_COMMAND("shotgun", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConShotgun, this, "Gives a shotgun to you")
CONSOLE_COMMAND("grenade", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConGrenade, this, "Gives a grenade launcher to you")
CONSOLE_COMMAND("laser", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConLaser, this, "Gives a laser to you")
CONSOLE_COMMAND("rifle", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConLaser, this, "Gives a laser to you")
CONSOLE_COMMAND("jetpack","", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConJetpack, this, "Gives jetpack to you")
CONSOLE_COMMAND("weapons", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConWeapons, this, "Gives all weapons to you")
CONSOLE_COMMAND("unshotgun", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnShotgun, this, "Removes the shotgun from you")
CONSOLE_COMMAND("ungrenade", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnGrenade, this, "Removes the grenade launcher from you")
CONSOLE_COMMAND("unlaser", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnLaser, this, "Removes the laser from you")
CONSOLE_COMMAND("unrifle", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnLaser, this, "Removes the laser from you")
CONSOLE_COMMAND("unjetpack", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnJetpack, this, "Removes the jetpack from you")
CONSOLE_COMMAND("unweapons", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnWeapons, this, "Removes all weapons from you")
CONSOLE_COMMAND("ninja", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConNinja, this, "Makes you a ninja")
CONSOLE_COMMAND("super", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConSuper, this, "Makes you super")
CONSOLE_COMMAND("unsuper", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnSuper, this, "Removes super from you")
CONSOLE_COMMAND("unsolo", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnSolo, this, "Puts you out of solo part")
CONSOLE_COMMAND("undeep", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnDeep, this, "Puts you out of deep freeze")
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConGoLeft, this, "Makes you move 1 tile left")
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConGoRight, this, "Makes you move 1 tile right")
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConGoUp, this, "Makes you move 1 tile up")
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConGoDown, this, "Makes you move 1 tile down")

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConMove, this, "Moves to the tile with x/y-number ii")
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii")
CONSOLE_COMMAND("force_pause", "v[id] i[seconds]", CFGFLAG_SERVER, ConForcePause, this, "Force i to pause for i seconds")
CONSOLE_COMMAND("force_unpause", "v[id]", CFGFLAG_SERVER, ConForcePause, this, "Set force-pause timer of i to 0.")

CONSOLE_COMMAND("set_team_ddr", "v[id] ?i[team]", CFGFLAG_SERVER, ConSetDDRTeam, this, "Set ddrace team of a player")
CONSOLE_COMMAND("uninvite", "v[id] ?i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team")

CONSOLE_COMMAND("vote_mute", "v[id] i[seconds]", CFGFLAG_SERVER, ConVoteMute, this, "Remove v's right to vote for i seconds")
CONSOLE_COMMAND("vote_unmute", "v[id]", CFGFLAG_SERVER, ConVoteUnmute, this, "Give back v's right to vote.")
CONSOLE_COMMAND("vote_mutes", "", CFGFLAG_SERVER, ConVoteMutes, this, "List the current active vote mutes.")
CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "")
CONSOLE_COMMAND("muteid", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteID, this, "")
CONSOLE_COMMAND("muteip", "s[ip] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteIP, this, "")
CONSOLE_COMMAND("unmute", "v[id]", CFGFLAG_SERVER, ConUnmute, this, "")
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "")
CONSOLE_COMMAND("moderate", "", CFGFLAG_SERVER, ConModerate, this, "Enables/disables active moderator mode for the player")
CONSOLE_COMMAND("vote_no", "", CFGFLAG_SERVER, ConVoteNo, this, "Same as \"vote no\"")
CONSOLE_COMMAND("save_dry", "", CFGFLAG_SERVER, ConDrySave, this, "Dump the current savestring")

CONSOLE_COMMAND("freezehammer", "v[id]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConFreezeHammer, this, "Gives a player Freeze Hammer")
CONSOLE_COMMAND("unfreezehammer", "v[id]", CFGFLAG_SERVER|CFGFLAG_GAME|CFGFLAG_NOMAPCFG, ConUnFreezeHammer, this, "Removes Freeze Hammer from a player")
#undef CONSOLE_COMMAND
