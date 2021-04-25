// ChillerDragon 2021 - chillerbot ux

// This file can be included several times.

#ifndef CHILLERBOT_CHAT_CMD
#define CHILLERBOT_CHAT_CMD(name, params, flags, callback, userdata, help)
#endif

CHILLERBOT_CHAT_CMD("war", "s[name]r[name]", CFGFLAG_CHAT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("peace", "s[name]r[name]", CFGFLAG_CHAT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("traitor", "s[name]r[name]", CFGFLAG_CHAT, NULL, this, "War list command")

#undef CHILLERBOT_CHAT_CMD
