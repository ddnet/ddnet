/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <engine/shared/network.h>
#include <engine/console.h>
#include <engine/storage.h>

#include "banmaster.h"

enum 
{
	MAX_BANS=1024,
	BAN_REREAD_TIME=300,
	CFGFLAG_BANMASTER=32
};

static const char BANMASTER_BANFILE[] = "bans.cfg";

struct CBan
{
	NETADDR m_Address;
	char m_aReason[256];
	int64 m_Expire;
};

static CBan m_aBans[MAX_BANS];
static int m_NumBans = 0;
static CNetClient m_Net;
static IConsole *m_pConsole;
static char m_aBindAddr[64] = "";

CBan* CheckBan(NETADDR *pCheck)
{
	for(int i = 0; i < m_NumBans; i++)
		if(net_addr_comp(&m_aBans[i].m_Address, pCheck) == 0)
			return &m_aBans[i];

	return 0;
}

int SendResponse(NETADDR *pAddr, NETADDR *pCheck)
{
	static char aIpBan[sizeof(BANMASTER_IPBAN) + NETADDR_MAXSTRSIZE + 256] = { 0 };
	static char *pIpBanContent = aIpBan + sizeof(BANMASTER_IPBAN);
	if (!aIpBan[0])
		mem_copy(aIpBan, BANMASTER_IPBAN, sizeof(BANMASTER_IPBAN));
	
	static CNetChunk p;
	
	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;

	CBan* pBan = CheckBan(pCheck);
	if(pBan)
	{
		net_addr_str(pCheck, pIpBanContent, NETADDR_MAXSTRSIZE);
		char *pIpBanReason = pIpBanContent + (str_length(pIpBanContent) + 1);
		str_copy(pIpBanReason, pBan->m_aReason, 256);
		
		p.m_pData = aIpBan;
		p.m_DataSize = sizeof(BANMASTER_IPBAN) + str_length(pIpBanContent) + 1 + str_length(pIpBanReason) + 1;
		m_Net.Send(&p);
		return 1;
	}
	
	return 0;
	/*else
	{
		p.m_DataSize = sizeof(BANMASTER_IPOK);
		p.m_pData = BANMASTER_IPOK;
		m_Net.Send(&p);
		return 0;
	}*/
}

void AddBan(NETADDR *pAddr, const char *pReason)
{
	pAddr->port = 0;

	CBan *pBan = CheckBan(pAddr);
	char aAddressStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pAddr, aAddressStr, sizeof(aAddressStr));

	if(pBan)
	{
		char aAddressStr[NETADDR_MAXSTRSIZE];
		net_addr_str(pAddr, aAddressStr, sizeof(aAddressStr));
		dbg_msg("banmaster", "updated ban, ip=%s oldreason='%s' reason='%s'", aAddressStr, pBan->m_aReason, pReason);
	
		str_copy(pBan->m_aReason, pReason, sizeof(m_aBans[m_NumBans].m_aReason));
		pBan->m_Expire = -1;
	}
	else
	{	
		if(m_NumBans == MAX_BANS)
		{
			dbg_msg("banmaster", "error: banmaster is full");
			return;
		}

		m_aBans[m_NumBans].m_Address = *pAddr;
		str_copy(m_aBans[m_NumBans].m_aReason, pReason, sizeof(m_aBans[m_NumBans].m_aReason));
		m_aBans[m_NumBans].m_Expire = -1;

		dbg_msg("banmaster", "added ban, ip=%s reason='%s'", aAddressStr, m_aBans[m_NumBans].m_aReason);
	
		m_NumBans++;
	}
}

void ClearBans()
{
	m_NumBans = 0;
}

void PurgeBans()
{
	int64 Now = time_get();
	int i = 0;
	while(i < m_NumBans)
	{
		if(m_aBans[i].m_Expire != -1 && m_aBans[i].m_Expire < Now)
		{
			// remove ban
			char aBuf[NETADDR_MAXSTRSIZE];
			net_addr_str(&m_aBans[i].m_Address, aBuf, sizeof(aBuf));
			dbg_msg("banmaster", "ban expired, ip=%s reason='%s'", aBuf, m_aBans[i].m_aReason);
			m_aBans[i] = m_aBans[m_NumBans - 1];
			m_NumBans--;
		}
		else
			i++;
	}
}

void ConBan(IConsole::IResult *pResult, void *pUser, int ClientID)
{
	NETADDR Addr;
	const char *pStr = pResult->GetString(0);
	const char *pReason = "";
	
	if(pResult->NumArguments() > 1)
		pReason = pResult->GetString(1);
	
	if(!net_addr_from_str(&Addr, pStr))
		AddBan(&Addr, pReason);
	else
		dbg_msg("banmaster", "invalid network address to ban, str='%s'", pStr);
}

void ConUnbanAll(IConsole::IResult *pResult, void *pUser, int ClientID)
{
	ClearBans();
}

void ConSetBindAddr(IConsole::IResult *pResult, void *pUser, int ClientID)
{
	if(m_aBindAddr[0])
		return;
	str_copy(m_aBindAddr, pResult->GetString(0), sizeof(m_aBindAddr));
	dbg_msg("banmaster/network", "bound to %s", m_aBindAddr);
}

void StandardOutput(const char *pLine, void *pUser)
{
}

int main(int argc, const char **argv) // ignore_convention
{
	int64 LastUpdate = time_get();

	dbg_logger_stdout();
	net_init();

	IKernel *pKernel = IKernel::Create();
	IStorage *pStorage = CreateStorage("Teeworlds", argc, argv); // ignore_convention

	m_pConsole = CreateConsole(CFGFLAG_BANMASTER);
	m_pConsole->RegisterPrintCallback(StandardOutput, 0);
	m_pConsole->Register("ban", "s?r", CFGFLAG_BANMASTER, ConBan, 0, "Bans the specified ip", IConsole::CONSOLELEVEL_USER);
	m_pConsole->Register("unban_all", "", CFGFLAG_BANMASTER, ConUnbanAll, 0, "Unbans all ips", IConsole::CONSOLELEVEL_USER);
	m_pConsole->Register("bind", "s", CFGFLAG_BANMASTER, ConSetBindAddr, 0, "Binds to the specified address", IConsole::CONSOLELEVEL_USER);

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(m_pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		
		if(RegisterFail)
			return -1;
	}

	m_pConsole->ExecuteFile(BANMASTER_BANFILE, -1, IConsole::CONSOLELEVEL_CONFIG, 0, 0);
	
	NETADDR BindAddr;
	if(m_aBindAddr[0] && net_host_lookup(m_aBindAddr, &BindAddr, NETTYPE_IPV4) == 0)
	{
		if(BindAddr.port == 0)
			BindAddr.port = BANMASTER_PORT;
	}
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.port = BANMASTER_PORT;
	}
		
	m_Net.Open(BindAddr, 0);
	// TODO: check socket for errors

	dbg_msg("banmaster", "started");
	
	while(1)
	{
		m_Net.Update();
		
		// process m_aPackets
		CNetChunk Packet;
		while(m_Net.Recv(&Packet))
		{
			char aAddressStr[NETADDR_MAXSTRSIZE];
			net_addr_str(&Packet.m_Address, aAddressStr, sizeof(aAddressStr));

			if(Packet.m_DataSize >= sizeof(BANMASTER_IPCHECK) && mem_comp(Packet.m_pData, BANMASTER_IPCHECK, sizeof(BANMASTER_IPCHECK)) == 0)
			{
				char *pAddr = (char *)Packet.m_pData + sizeof(BANMASTER_IPCHECK);
				NETADDR CheckAddr;
				if(net_addr_from_str(&CheckAddr, pAddr))
				{
					dbg_msg("banmaster", "dropped weird message, ip=%s checkaddr='%s'", aAddressStr, pAddr);
				}
				else
				{
					CheckAddr.port = 0;

					int Banned = SendResponse(&Packet.m_Address, &CheckAddr);

					char aBuf[NETADDR_MAXSTRSIZE];
					net_addr_str(&CheckAddr, aBuf, sizeof(aBuf));
					dbg_msg("banmaster", "responded to checkmsg, ip=%s checkaddr=%s result=%s", aAddressStr, aBuf, (Banned) ? "ban" : "ok");
				}
			}
			else
				dbg_msg("banmaster", "dropped weird packet, ip=%s", aAddressStr, (char *)Packet.m_pData);
		}
		
		if(time_get() - LastUpdate > time_freq() * BAN_REREAD_TIME)
		{
			ClearBans();
			LastUpdate = time_get();
			m_pConsole->ExecuteFile(BANMASTER_BANFILE, -1, IConsole::CONSOLELEVEL_CONFIG, 0, 0);
		}
		
		// be nice to the CPU
		thread_sleep(1);
	}
	
	return 0;
}

