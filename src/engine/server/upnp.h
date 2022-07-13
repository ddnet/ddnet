#ifndef ENGINE_SERVER_UPNP_H
#define ENGINE_SERVER_UPNP_H

#include <base/system.h>
class CUPnP
{
	NETADDR m_Addr;
	struct UPNPUrls *m_pUPnPUrls = nullptr;
	struct IGDdatas *m_pUPnPData = nullptr;
	struct UPNPDev *m_pUPnPDevice = nullptr;
	bool m_Enabled = false;

public:
	void Open(NETADDR Address);
	void Shutdown();
};

#endif
