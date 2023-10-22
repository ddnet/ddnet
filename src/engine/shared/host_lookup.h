/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_HOST_LOOKUP_H
#define ENGINE_SHARED_HOST_LOOKUP_H

#include <base/system.h>

#include <engine/shared/jobs.h>

class CHostLookup : public IJob
{
private:
	void Run() override;

public:
	CHostLookup();
	CHostLookup(const char *pHostname, int Nettype);

	int m_Result;
	char m_aHostname[128];
	int m_Nettype;
	NETADDR m_Addr;
};

#endif
