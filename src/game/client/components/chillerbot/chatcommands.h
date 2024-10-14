// ChillerDragon 2023 - chillerbot ux

// This file can be included several times.

#ifndef CHILLERBOT_CHAT_CMD
#define CHILLERBOT_CHAT_CMD(name, params, flags, callback, userdata, help)
#endif

// ux commands

CHILLERBOT_CHAT_CMD("addwar", "s[folder]r[name]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("addteam", "s[folder]r[name]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("peace", "s[folder]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("war", "s[folder]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("team", "s[folder]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("unfriend", "s[folder]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("addreason", "s[folder]r[reason]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("search", "r[name]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")
CHILLERBOT_CHAT_CMD("create", "s[war|team|neutral|traitor]s[folder]?r[name]", CFGFLAG_CHAT | CFGFLAG_CLIENT, NULL, this, "War list command")

// zx commands

#undef CHILLERBOT_CHAT_CMD
