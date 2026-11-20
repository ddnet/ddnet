#include "ddnet_info.h"

#include <base/str.h>

bool StunServerFromInfoJson(const json_value &InfoJson, int NetType, NETADDR *pAddr)
{
	const char *pKey = nullptr;
	if(NetType == NETTYPE_IPV4)
	{
		pKey = "stun-servers-ipv4";
	}
	else if(NetType == NETTYPE_IPV6)
	{
		pKey = "stun-servers-ipv6";
	}
	else
	{
		return false;
	}

	const json_value &StunServers = InfoJson[pKey];
	if(StunServers.type != json_array)
	{
		return false;
	}

	for(int i = 0; i < json_array_length(&StunServers); i++)
	{
		const json_value &StunServer = *json_array_get(&StunServers, i);
		if(StunServer.type != json_string)
		{
			continue;
		}

		NETADDR Addr;
		if(net_addr_from_str(&Addr, StunServer) != 0 || Addr.port == 0 || !(Addr.type & NetType))
		{
			continue;
		}

		*pAddr = Addr;
		return true;
	}
	return false;
}
