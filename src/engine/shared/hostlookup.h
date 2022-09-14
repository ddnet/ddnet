#ifndef ENGINE_SHARED_HOSTLOOKUP_H
#define ENGINE_SHARED_HOSTLOOKUP_H

#include <engine/shared/jobs.h>

class CHostLookup : public IJob
{
private:
	void Run() override;

public:
	CHostLookup() = default;
	CHostLookup(const char *pHostname, int Nettype);

	int m_Result;
	char m_aHostname[128];
	int m_Nettype;
	NETADDR m_Addr;
};

#endif //ENGINE_SHARED_HOSTLOOKUP_H
