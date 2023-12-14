// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("hammer", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConHammer, this, "Gives a hammer to you")
CONSOLE_COMMAND("gun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGun, this, "Gives a gun to you")
CONSOLE_COMMAND("unhammer", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnHammer, this, "Removes a hammer from you")
CONSOLE_COMMAND("ungun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnGun, this, "Removes a gun from you")
CONSOLE_COMMAND("godmode", "?v[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConGodmode, this, "Removes damage")

CONSOLE_COMMAND("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams")
CONSOLE_COMMAND("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams")
CONSOLE_COMMAND("swap_teams_random", "", CFGFLAG_SERVER, ConSwapTeamsRandom, this, "Swap the current teams or not (random chance)")

#undef CONSOLE_COMMAND
