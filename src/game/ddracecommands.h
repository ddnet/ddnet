/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_DDRACECOMMANDS_H
#define GAME_SERVER_DDRACECOMMANDS_H
#undef GAME_SERVER_DDRACECOMMANDS_H // this file can be included several times

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help, level)
#endif

CONSOLE_COMMAND("kill_pl", "v", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill", IConsole::CONSOLELEVEL_MODERATOR)
CONSOLE_COMMAND("logout", "", CFGFLAG_SERVER, ConLogOut, this, "Logs out from the console", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("moder", "v", CFGFLAG_SERVER, ConSetlvl1, this, "Authenticates player v to the level of 1", IConsole::CONSOLELEVEL_ADMIN)
CONSOLE_COMMAND("admin", "v", CFGFLAG_SERVER, ConSetlvl2, this, "Authenticates player v to the level of 2", IConsole::CONSOLELEVEL_ADMIN)
CONSOLE_COMMAND("tele", "i", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleport, this, "Teleports you to player i", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("addweapon", "i", CFGFLAG_SERVER|CMDFLAG_TEST, ConAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, rifle = 4, ninja = 5)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("removeweapon", "i", CFGFLAG_SERVER|CMDFLAG_TEST, ConRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, rifle = 4)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("shotgun", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConShotgun, this, "Gives a shotgun to you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("grenade", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGrenade, this, "Gives a grenade launcher to you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("rifle", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConRifle, this, "Gives a rifle to you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("weapons", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConWeapons, this, "Gives all weapons to you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("unshotgun", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnShotgun, this, "Takes the shotgun from you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("ungrenade", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnGrenade, this, "Takes the grenade launcher you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("unrifle", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnRifle, this, "Takes the rifle from you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("unweapons", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnWeapons, this, "Takes all weapons from you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("ninja", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConNinja, this, "Makes you a ninja", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("super", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConSuper, this, "Makes you super", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("unsuper", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnSuper, this, "Removes super from you", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("move", "ii", CFGFLAG_SERVER|CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("move_raw", "ii", CFGFLAG_SERVER|CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("credits", "", CFGFLAG_SERVER, ConCredits, this, "Shows the credits of the DDRace mod", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("emote", "?si", CFGFLAG_SERVER, ConEyeEmote, this, "Sets your tee's eye emote", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("broadmsg", "", CFGFLAG_SERVER, ConToggleBroadcast, this, "Toggles showing the server's broadcast message during race on/off", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("eyeemote", "", CFGFLAG_SERVER, ConEyeEmote, this, "Toggles use of standard eye-emotes on/off", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("settings", "?s", CFGFLAG_SERVER, ConSettings, this, "Shows gameplay information for this server", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("help", "?r", CFGFLAG_SERVER, ConHelp, this, "Shows help to command r, general help if left blank", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("info", "", CFGFLAG_SERVER, ConInfo, this, "Shows info about this server", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("me", "r", CFGFLAG_SERVER, ConMe, this, "Like the famous irc command '/me says hi' will display '<yourname> says hi'", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("pause", "", CFGFLAG_SERVER, ConTogglePause, this, "Toggles pause on/off (if activated on server)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("rank", "?r", CFGFLAG_SERVER, ConRank, this, "Shows the rank of player with name r (your rank by default)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("rules", "", CFGFLAG_SERVER, ConRules, this, "Shows the server rules", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("team", "?i", CFGFLAG_SERVER, ConJoinTeam, this, "Lets you join team i (shows your team if left blank)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("top5", "?i", CFGFLAG_SERVER, ConTop5, this, "Shows five ranks of the ladder beginning with rank i (1 by default)", IConsole::CONSOLELEVEL_USER)
CONSOLE_COMMAND("showothers", "?i", CFGFLAG_SERVER, ConShowOthers, this, "Whether to showplayers from other teams or not (off by default), optional i = 0 for off else for on", IConsole::CONSOLELEVEL_USER)

CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "", IConsole::CONSOLELEVEL_MODERATOR);
CONSOLE_COMMAND("muteid", "vi", CFGFLAG_SERVER, ConMuteID, this, "", IConsole::CONSOLELEVEL_MODERATOR);
CONSOLE_COMMAND("muteip", "si", CFGFLAG_SERVER, ConMuteIP, this, "", IConsole::CONSOLELEVEL_MODERATOR);
CONSOLE_COMMAND("unmute", "v", CFGFLAG_SERVER, ConUnmute, this, "", IConsole::CONSOLELEVEL_MODERATOR);
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "", IConsole::CONSOLELEVEL_MODERATOR);

#if defined(CONF_SQL)
CONSOLE_COMMAND("times", "?s?i", CFGFLAG_SERVER, ConTimes, this, "/times ?s?i shows last 5 times of the server or of a player beginning with name s starting with time i (i = 1 by default)", IConsole::CONSOLELEVEL_USER)
#endif
#undef CONSOLE_COMMAND

#endif
