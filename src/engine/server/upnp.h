#ifndef ENGINE_SERVER_UPNP_H
#define ENGINE_SERVER_UPNP_H

#include <base/system.h>
class CUPnP
{
	NETADDR m_Addr;
	struct UPNPUrls *m_UPnPUrls;
	struct IGDdatas *m_UPnPData;
	struct UPNPDev *m_UPnPDevice;
	bool m_Enabled;

public:
	void Open(NETADDR Address);
	void Shutdown();
};

#endif
