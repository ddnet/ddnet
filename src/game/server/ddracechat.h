/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_DDRACECOMMANDS_H
#define GAME_SERVER_DDRACECOMMANDS_H
#undef GAME_SERVER_DDRACECOMMANDS_H // this file can be included several times

#ifndef CHAT_COMMAND
#define CHAT_COMMAND(name, params, flags, callback, userdata, help)
#endif

CHAT_COMMAND("credits", "", CFGFLAG_CHAT, ConCredits, this, "Shows the credits of the DDRace mod")
CHAT_COMMAND("emote", "?si", CFGFLAG_CHAT, ConEyeEmote, this, "Sets your tee's eye emote")
CHAT_COMMAND("eyeemote", "", CFGFLAG_CHAT, ConEyeEmote, this, "Toggles use of standard eye-emotes on/off")
CHAT_COMMAND("settings", "?s", CFGFLAG_CHAT, ConSettings, this, "Shows gameplay information for this server")
CHAT_COMMAND("help", "?r", CFGFLAG_CHAT, ConHelp, this, "Shows help to command r, general help if left blank")
CHAT_COMMAND("info", "", CFGFLAG_CHAT, ConInfo, this, "Shows info about this server")
CHAT_COMMAND("me", "r", CFGFLAG_CHAT, ConMe, this, "Like the famous irc command '/me says hi' will display '<yourname> says hi'")
CHAT_COMMAND("pause", "", CFGFLAG_CHAT, ConTogglePause, this, "Toggles pause on/off (if activated on server)")
CHAT_COMMAND("rank", "?r", CFGFLAG_CHAT, ConRank, this, "Shows the rank of player with name r (your rank by default)")
CHAT_COMMAND("rules", "", CFGFLAG_CHAT, ConRules, this, "Shows the server rules")
CHAT_COMMAND("team", "?i", CFGFLAG_CHAT, ConJoinTeam, this, "Lets you join team i (shows your team if left blank)")
CHAT_COMMAND("top5", "?i", CFGFLAG_CHAT, ConTop5, this, "Shows five ranks of the ladder beginning with rank i (1 by default)")
CHAT_COMMAND("showothers", "?i", CFGFLAG_CHAT, ConShowOthersChat, this, "Whether to showplayers from other teams or not (off by default), optional i = 0 for off else for on")

#if defined(CONF_SQL)
CHAT_COMMAND("times", "?s?i", CFGFLAG_CHAT, ConTimes, this, "/times ?s?i shows last 5 times of the server or of a player beginning with name s starting with time i (i = 1 by default)")
#endif
#undef CHAT_COMMAND

#endif
