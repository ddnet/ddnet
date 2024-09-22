// UUID(name_in_code, name)
//
// When adding your own extended net messages, choose the name (third
// parameter) as `<name>@<domain>` where `<name>` is a name you can choose
// freely and `<domain>` is a domain you own. If you don't own a domain, try
// choosing a string that is not a domain and uniquely identifies you, e.g. use
// the name of the client/server you develop.
//
// Example:
//
// 1) `i-unfreeze-you@ddnet.org`
// 2) `creeper@minetee`
//
// The first example applies if you own the `ddnet.org` domain, that is, if you
// are adding this message on behalf of the DDNet team.
//
// The second example shows how you could add a message if you don't own a
// domain, but need a message for your minetee client/server.

// This file can be included several times.

UUID(NETMSG_WHATIS, "what-is@ddnet.org")
UUID(NETMSG_ITIS, "it-is@ddnet.org")
UUID(NETMSG_IDONTKNOW, "i-dont-know@ddnet.org")

UUID(NETMSG_RCONTYPE, "rcon-type@ddnet.org")
UUID(NETMSG_MAP_DETAILS, "map-details@ddnet.org")
UUID(NETMSG_CAPABILITIES, "capabilities@ddnet.org")
UUID(NETMSG_CLIENTVER, "clientver@ddnet.org")
UUID(NETMSG_PINGEX, "ping@ddnet.org")
UUID(NETMSG_PONGEX, "pong@ddnet.org")
UUID(NETMSG_CHECKSUM_REQUEST, "checksum-request@ddnet.org")
UUID(NETMSG_CHECKSUM_RESPONSE, "checksum-response@ddnet.org")
UUID(NETMSG_CHECKSUM_ERROR, "checksum-error@ddnet.org")
UUID(NETMSG_REDIRECT, "redirect@ddnet.org")
UUID(NETMSG_RCON_CMD_GROUP_START, "rcon-cmd-group-start@ddnet.org")
UUID(NETMSG_RCON_CMD_GROUP_END, "rcon-cmd-group-end@ddnet.org")
UUID(NETMSG_MAP_RELOAD, "map-reload@ddnet.org")
