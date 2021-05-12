#include "serverbrowser_http.h"

#include "http.h"

#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/serverbrowser.h>
#include <engine/shared/serverinfo.h>

#include <memory>

class CChooseMaster
{
public:
	typedef bool (*VALIDATOR)(json_value *pJson);

	enum
	{
		MAX_URLS = 16,
	};
	CChooseMaster(IEngine *pEngine, VALIDATOR pfnValidator, const char **ppUrls, int NumUrls, int PreviousBestIndex);
	virtual ~CChooseMaster() {}

	const char *GetBestUrl() const;
	int GetBestIndex() const;
	void Refresh();

private:
	class CData
	{
	public:
		std::atomic_int m_BestIndex{-1};
		// Constant after construction.
		VALIDATOR m_pfnValidator;
		int m_NumUrls;
		char m_aaUrls[MAX_URLS][256];
	};
	class CJob : public IJob
	{
		std::shared_ptr<CData> m_pData;
		virtual void Run();

	public:
		CJob(std::shared_ptr<CData> pData) :
			m_pData(pData) {}
		virtual ~CJob() {}
	};

	IEngine *m_pEngine;
	int m_PreviousBestIndex;
	std::shared_ptr<CData> m_pData;
};

CChooseMaster::CChooseMaster(IEngine *pEngine, VALIDATOR pfnValidator, const char **ppUrls, int NumUrls, int PreviousBestIndex) :
	m_pEngine(pEngine),
	m_PreviousBestIndex(PreviousBestIndex)
{
	dbg_assert(NumUrls >= 0, "no master URLs");
	dbg_assert(NumUrls <= MAX_URLS, "too many master URLs");
	dbg_assert(PreviousBestIndex >= -1, "previous best index negative and not -1");
	dbg_assert(PreviousBestIndex < NumUrls, "previous best index too high");
	m_pData = std::make_shared<CData>();
	m_pData->m_pfnValidator = pfnValidator;
	m_pData->m_NumUrls = NumUrls;
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		str_copy(m_pData->m_aaUrls[i], ppUrls[i], sizeof(m_pData->m_aaUrls[i]));
	}
	if(m_PreviousBestIndex < 0)
	{
		m_PreviousBestIndex = secure_rand_below(NumUrls);
	}
}

int CChooseMaster::GetBestIndex() const
{
	int BestIndex = m_pData->m_BestIndex.load();
	if(BestIndex >= 0)
	{
		return BestIndex;
	}
	else
	{
		return m_PreviousBestIndex;
	}
}

const char *CChooseMaster::GetBestUrl() const
{
	return m_pData->m_aaUrls[GetBestIndex()];
}

void CChooseMaster::Refresh()
{
	m_pEngine->AddJob(std::make_shared<CJob>(m_pData));
}

void CChooseMaster::CJob::Run()
{
	// Check masters in a random order.
	int aRandomized[MAX_URLS];
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		aRandomized[i] = i;
	}
	// https://en.wikipedia.org/w/index.php?title=Fisher%E2%80%93Yates_shuffle&oldid=1002922479#The_modern_algorithm
	// The equivalent version.
	for(int i = 0; i <= m_pData->m_NumUrls - 2; i++)
	{
		int j = i + secure_rand_below(m_pData->m_NumUrls - i);
		std::swap(aRandomized[i], aRandomized[j]);
	}
	// Do a HEAD request to ensure that a connection is established and
	// then do a GET request to check how fast we can get the server list.
	//
	// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to
	// fail.
	CTimeout Timeout{10000, 8000, 10};
	int aTimeMs[MAX_URLS];
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		aTimeMs[i] = -1;
		const char *pUrl = m_pData->m_aaUrls[aRandomized[i]];
		CHead Head(pUrl, Timeout);
		IEngine::RunJobBlocking(&Head);
		if(Head.State() != HTTP_DONE)
		{
			continue;
		}
		int64 StartTime = time_get();
		CGet Get(pUrl, Timeout);
		IEngine::RunJobBlocking(&Get);
		int Time = (time_get() - StartTime) * 1000 / time_freq();
		if(Get.State() != HTTP_DONE)
		{
			continue;
		}
		json_value *pJson = Get.ResultJson();
		if(!pJson)
		{
			continue;
		}
		bool ParseFailure = m_pData->m_pfnValidator(pJson);
		json_value_free(pJson);
		if(ParseFailure)
		{
			continue;
		}
		dbg_msg("serverbrowse_http", "found master, url='%s' time=%dms", pUrl, Time);
		aTimeMs[i] = Time;
	}
	// Determine index of the minimum time.
	int BestIndex = -1;
	int BestTime = 0;
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		if(aTimeMs[i] < 0)
		{
			continue;
		}
		if(BestIndex == -1 || aTimeMs[i] < BestTime)
		{
			BestTime = aTimeMs[i];
			BestIndex = aRandomized[i];
		}
	}
	if(BestIndex == -1)
	{
		dbg_msg("serverbrowse_http", "WARNING: no usable masters found");
		return;
	}
	dbg_msg("serverbrowse_http", "determined best master, url='%s' time=%dms", m_pData->m_aaUrls[BestIndex], BestTime);
	m_pData->m_BestIndex.store(BestIndex);
}

class CServerBrowserHttp : public IServerBrowserHttp
{
public:
	CServerBrowserHttp(IEngine *pEngine);
	virtual ~CServerBrowserHttp() {}
	void Update();
	bool IsRefreshing() { return (bool)m_pGetServers; }
	void Refresh();

	int NumServers() const
	{
		return m_aServers.size();
	}
	const NETADDR &ServerAddress(int Index) const
	{
		return m_aServers[Index].m_Addr;
	}
	void Server(int Index, NETADDR *pAddr, CServerInfo *pInfo) const
	{
		const CEntry &Entry = m_aServers[Index];
		*pAddr = Entry.m_Addr;
		*pInfo = Entry.m_Info;
	}
	int NumLegacyServers() const
	{
		return m_aLegacyServers.size();
	}
	const NETADDR &LegacyServer(int Index) const
	{
		return m_aLegacyServers[Index];
	}

private:
	class CEntry
	{
	public:
		NETADDR m_Addr;
		CServerInfo m_Info;
	};

	static bool Validate(json_value *pJson);
	static bool Parse(json_value *pJson, std::vector<CEntry> *paServers, std::vector<NETADDR> *paLegacyServers);

	IEngine *m_pEngine;
	std::shared_ptr<CGet> m_pGetServers;
	std::unique_ptr<CChooseMaster> m_pChooseMaster;

	std::vector<CEntry> m_aServers;
	std::vector<NETADDR> m_aLegacyServers;
};

static const char *MASTERSERVER_URLS[] = {
	"https://heinrich5991.de/teeworlds/temp/xyz.json",
	"https://heinrich5991.de/teeworlds/temp/servers.json",
};

CServerBrowserHttp::CServerBrowserHttp(IEngine *pEngine) :
	m_pEngine(pEngine),
	m_pChooseMaster(new CChooseMaster(pEngine, Validate, MASTERSERVER_URLS, sizeof(MASTERSERVER_URLS) / sizeof(MASTERSERVER_URLS[0]), -1))
{
	m_pChooseMaster->Refresh();
}
void CServerBrowserHttp::Update()
{
	if(m_pGetServers && m_pGetServers->State() != HTTP_QUEUED && m_pGetServers->State() != HTTP_RUNNING)
	{
		std::shared_ptr<CGet> pGetServers = nullptr;
		std::swap(m_pGetServers, pGetServers);

		json_value *pJson = pGetServers->ResultJson();
		if(!pJson)
		{
			return;
		}
		bool ParseFailure = Parse(pJson, &m_aServers, &m_aLegacyServers);
		json_value_free(pJson);
		if(ParseFailure)
		{
			return;
		}
	}
}
void CServerBrowserHttp::Refresh()
{
	m_pEngine->AddJob(m_pGetServers = std::make_shared<CGet>(m_pChooseMaster->GetBestUrl(), CTimeout{0, 0, 0}));
}
bool ServerbrowserParseUrl(NETADDR *pOut, const char *pUrl)
{
	char aHost[128];
	const char *pRest = str_startswith(pUrl, "tw-0.6+udp://");
	if(!pRest)
	{
		return true;
	}
	int Length = str_length(pRest);
	int Start = 0;
	int End = Length;
	for(int i = 0; i < Length; i++)
	{
		if(pRest[i] == '@')
		{
			if(Start != 0)
			{
				// Two at signs.
				return true;
			}
			Start = i + 1;
		}
		else if(pRest[i] == '/' || pRest[i] == '?' || pRest[i] == '#')
		{
			End = i;
			break;
		}
	}
	str_truncate(aHost, sizeof(aHost), pRest + Start, End - Start);
	if(net_addr_from_str(pOut, aHost))
	{
		return true;
	}
	return false;
}
bool CServerBrowserHttp::Validate(json_value *pJson)
{
	std::vector<CEntry> aServers;
	std::vector<NETADDR> aLegacyServers;
	return Parse(pJson, &aServers, &aLegacyServers);
}
bool CServerBrowserHttp::Parse(json_value *pJson, std::vector<CEntry> *paServers, std::vector<NETADDR> *paLegacyServers)
{
	std::vector<CEntry> aServers;
	std::vector<NETADDR> aLegacyServers;

	const json_value &Json = *pJson;
	const json_value &Servers = Json["servers"];
	const json_value &LegacyServers = Json["servers_legacy"];
	if(Servers.type != json_array || (LegacyServers.type != json_array && LegacyServers.type != json_none))
	{
		return true;
	}
	for(unsigned int i = 0; i < Servers.u.array.length; i++)
	{
		const json_value &Server = Servers[i];
		const json_value &Addresses = Server["addresses"];
		const json_value &Info = Server["info"];
		const json_value &Location = Server["location"];
		int ParsedLocation = CServerInfo::LOC_UNKNOWN;
		CServerInfo2 ParsedInfo;
		if(Addresses.type != json_array || (Location.type != json_string && Location.type != json_none))
		{
			return true;
		}
		if(Location.type == json_string)
		{
			if(CServerInfo::ParseLocation(&ParsedLocation, Location))
			{
				return true;
			}
		}
		if(CServerInfo2::FromJson(&ParsedInfo, &Info))
		{
			dbg_msg("dbg/serverbrowser", "skipped due to info, i=%d", i);
			// Only skip the current server on parsing
			// failure; the server info is "user input" by
			// the game server and can be set to arbitrary
			// values.
			continue;
		}
		CServerInfo SetInfo = ParsedInfo;
		SetInfo.m_Location = ParsedLocation;
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			// TODO: Address address handling :P
			NETADDR ParsedAddr;
			if(ServerbrowserParseUrl(&ParsedAddr, Addresses[a]))
			{
				dbg_msg("dbg/serverbrowser", "unknown address, i=%d a=%d", i, a);
				// Skip unknown addresses.
				continue;
			}
			aServers.push_back({ParsedAddr, SetInfo});
		}
	}
	if(LegacyServers.type == json_array)
	{
		for(unsigned int i = 0; i < LegacyServers.u.array.length; i++)
		{
			const json_value &Address = LegacyServers[i];
			NETADDR ParsedAddr;
			if(Address.type != json_string || net_addr_from_str(&ParsedAddr, Address))
			{
				return true;
			}
			aLegacyServers.push_back(ParsedAddr);
		}
	}
	*paServers = aServers;
	*paLegacyServers = aLegacyServers;
	return false;
}
IServerBrowserHttp *CreateServerBrowserHttp(IEngine *pEngine)
{
	return new CServerBrowserHttp(pEngine);
}
