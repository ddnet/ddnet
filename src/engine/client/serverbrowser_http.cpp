#include "serverbrowser_http.h"

#include "http.h"

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/serverbrowser.h>
#include <engine/shared/linereader.h>
#include <engine/shared/serverinfo.h>
#include <engine/storage.h>

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

	bool GetBestUrl(const char **pBestUrl) const;
	void Reset();
	bool IsRefreshing() const { return m_pJob && m_pJob->Status() != IJob::STATE_DONE; }
	void Refresh();

private:
	int GetBestIndex() const;

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
			m_pData(std::move(pData)) {}
		virtual ~CJob() {}
	};

	IEngine *m_pEngine;
	int m_PreviousBestIndex;
	std::shared_ptr<CData> m_pData;
	std::shared_ptr<CJob> m_pJob;
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

bool CChooseMaster::GetBestUrl(const char **ppBestUrl) const
{
	int Index = GetBestIndex();
	if(Index < 0)
	{
		*ppBestUrl = nullptr;
		return true;
	}
	*ppBestUrl = m_pData->m_aaUrls[Index];
	return false;
}

void CChooseMaster::Reset()
{
	m_PreviousBestIndex = -1;
	m_pData->m_BestIndex.store(-1);
}

void CChooseMaster::Refresh()
{
	m_pEngine->AddJob(m_pJob = std::make_shared<CJob>(m_pData));
}

void CChooseMaster::CJob::Run()
{
	// Check masters in a random order.
	int aRandomized[MAX_URLS] = {0};
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
		CHead Head(pUrl, Timeout, HTTPLOG::FAILURE);
		IEngine::RunJobBlocking(&Head);
		if(Head.State() != HTTP_DONE)
		{
			continue;
		}
		int64_t StartTime = time_get_microseconds();
		CGet Get(pUrl, Timeout, HTTPLOG::FAILURE);
		IEngine::RunJobBlocking(&Get);
		int Time = (time_get_microseconds() - StartTime) / 1000;
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
	CServerBrowserHttp(IEngine *pEngine, IConsole *pConsole, const char **ppUrls, int NumUrls, int PreviousBestIndex);
	virtual ~CServerBrowserHttp() {}
	void Update();
	bool IsRefreshing() { return m_State != STATE_DONE; }
	void Refresh();
	bool GetBestUrl(const char **pBestUrl) const { return m_pChooseMaster->GetBestUrl(pBestUrl); }

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
	enum
	{
		STATE_DONE,
		STATE_WANTREFRESH,
		STATE_REFRESHING,
	};

	class CEntry
	{
	public:
		NETADDR m_Addr;
		CServerInfo m_Info;
	};

	static bool Validate(json_value *pJson);
	static bool Parse(json_value *pJson, std::vector<CEntry> *paServers, std::vector<NETADDR> *paLegacyServers);

	IEngine *m_pEngine;
	IConsole *m_pConsole;

	int m_State = STATE_DONE;
	std::shared_ptr<CGet> m_pGetServers;
	std::unique_ptr<CChooseMaster> m_pChooseMaster;

	std::vector<CEntry> m_aServers;
	std::vector<NETADDR> m_aLegacyServers;
};

CServerBrowserHttp::CServerBrowserHttp(IEngine *pEngine, IConsole *pConsole, const char **ppUrls, int NumUrls, int PreviousBestIndex) :
	m_pEngine(pEngine),
	m_pConsole(pConsole),
	m_pChooseMaster(new CChooseMaster(pEngine, Validate, ppUrls, NumUrls, PreviousBestIndex))
{
	m_pChooseMaster->Refresh();
}
void CServerBrowserHttp::Update()
{
	if(m_State == STATE_WANTREFRESH)
	{
		const char *pBestUrl;
		if(m_pChooseMaster->GetBestUrl(&pBestUrl))
		{
			if(!m_pChooseMaster->IsRefreshing())
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowse_http", "no working serverlist URL found");
				m_State = STATE_DONE;
			}
			return;
		}
		// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to fail.
		CTimeout Timeout{10000, 8000, 10};
		m_pEngine->AddJob(m_pGetServers = std::make_shared<CGet>(pBestUrl, Timeout));
		m_State = STATE_REFRESHING;
	}
	else if(m_State == STATE_REFRESHING)
	{
		if(m_pGetServers->State() == HTTP_QUEUED || m_pGetServers->State() == HTTP_RUNNING)
		{
			return;
		}
		m_State = STATE_DONE;
		std::shared_ptr<CGet> pGetServers = nullptr;
		std::swap(m_pGetServers, pGetServers);

		bool Success = true;
		json_value *pJson = pGetServers->ResultJson();
		Success = Success && pJson;
		Success = Success && !Parse(pJson, &m_aServers, &m_aLegacyServers);
		json_value_free(pJson);
		if(!Success)
		{
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowse_http", "failed getting serverlist, trying to find best URL");
			m_pChooseMaster->Reset();
			m_pChooseMaster->Refresh();
		}
	}
}
void CServerBrowserHttp::Refresh()
{
	if(m_State == STATE_WANTREFRESH)
	{
		m_pChooseMaster->Refresh();
	}
	if(m_State == STATE_DONE)
		m_State = STATE_WANTREFRESH;
	Update();
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
			//dbg_msg("dbg/serverbrowser", "skipped due to info, i=%d", i);
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
				//dbg_msg("dbg/serverbrowser", "unknown address, i=%d a=%d", i, a);
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

static const char *DEFAULT_SERVERLIST_URLS[] = {
	"https://master1.ddnet.tw/ddnet/15/servers.json",
	"https://master2.ddnet.tw/ddnet/15/servers.json",
	"https://master3.ddnet.tw/ddnet/15/servers.json",
	"https://master4.ddnet.tw/ddnet/15/servers.json",
};

IServerBrowserHttp *CreateServerBrowserHttp(IEngine *pEngine, IConsole *pConsole, IStorage *pStorage, const char *pPreviousBestUrl)
{
	char aaUrls[CChooseMaster::MAX_URLS][256];
	const char *apUrls[CChooseMaster::MAX_URLS] = {0};
	const char **ppUrls = apUrls;
	int NumUrls = 0;
	IOHANDLE File = pStorage->OpenFile("ddnet-serverlist-urls.cfg", IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		CLineReader Lines;
		Lines.Init(File);
		while(NumUrls < CChooseMaster::MAX_URLS)
		{
			const char *pLine = Lines.Get();
			if(!pLine)
			{
				break;
			}
			str_copy(aaUrls[NumUrls], pLine, sizeof(aaUrls[NumUrls]));
			apUrls[NumUrls] = aaUrls[NumUrls];
			NumUrls += 1;
		}
	}
	if(NumUrls == 0)
	{
		ppUrls = DEFAULT_SERVERLIST_URLS;
		NumUrls = sizeof(DEFAULT_SERVERLIST_URLS) / sizeof(DEFAULT_SERVERLIST_URLS[0]);
	}
	int PreviousBestIndex = -1;
	for(int i = 0; i < NumUrls; i++)
	{
		if(str_comp(ppUrls[i], pPreviousBestUrl) == 0)
		{
			PreviousBestIndex = i;
			break;
		}
	}
	return new CServerBrowserHttp(pEngine, pConsole, ppUrls, NumUrls, PreviousBestIndex);
}
