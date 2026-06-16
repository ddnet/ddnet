#ifndef ENGINE_SERVER_UPNP_H
#define ENGINE_SERVER_UPNP_H

#include <base/types.h>

// NOLINTBEGIN(readability-identifier-naming)
struct IGDdatas;
struct UPNPDev;
struct UPNPUrls;
// NOLINTEND(readability-identifier-naming)

class CUPnP
{
	NETADDR m_Addr;
	UPNPUrls *m_pUpnpUrls;
	IGDdatas *m_pUpnpData;
	UPNPDev *m_pUpnpDevice;
	bool m_Enabled;

public:
	void Open(NETADDR Address);
	void Shutdown();
};

#endif
