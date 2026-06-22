#ifndef ENGINE_SERVER_UPNP_H
#define ENGINE_SERVER_UPNP_H

#include <base/types.h>
class CUPnP
{
	NETADDR m_Addr;
	struct UPNPUrls *m_pUPnPUrls;
	struct IGDdatas *m_pUPnPData;
	struct UPNPDev *m_pUPnPDevice;
	bool m_Enabled;

public:
	void Open(NETADDR Address);
	void Shutdown();
};

#endif
