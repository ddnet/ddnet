#ifndef ENGINE_SHARED_DDNET_INFO_H
#define ENGINE_SHARED_DDNET_INFO_H

#include <base/net.h>

#include <engine/shared/json.h>

static constexpr const char *DDNET_INFO_FILE = "ddnet-info.json";
static constexpr const char *DDNET_INFO_URL = "https://info.ddnet.org/info";

// Extracts the STUN server for the given net type (`NETTYPE_IPV4` or
// `NETTYPE_IPV6`) from the DDNet info file, returning whether one was found.
bool StunServerFromInfoJson(const json_value &InfoJson, int NetType, NETADDR *pAddr);

#endif // ENGINE_SHARED_DDNET_INFO_H
