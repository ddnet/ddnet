#include "serverbrowser_http.h"

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/serverbrowser.h>
#include <engine/shared/http.h>
#include <engine/shared/jobs.h>
#include <engine/shared/linereader.h>
#include <engine/shared/serverinfo.h>
#include <engine/storage.h>

#include <base/lock_scope.h>
#include <base/system.h>

#include <memory>
#include <vector>

#include <chrono>

using namespace std::chrono_literals;

class CChooseMaster
{
public:
	typedef bool (*VALIDATOR)(json_value *pJson);

	enum
	{
		MAX_URLS = 16,
	};
	CChooseMaster(IEngine *pEngine, VALIDATOR pfnValidator, const char **ppUrls, int NumUrls, int PreviousBestIndex);
	virtual ~CChooseMaster();

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
		LOCK m_Lock;
		std::shared_ptr<CData> m_pData;
		std::unique_ptr<CHttpRequest> m_pHead PT_GUARDED_BY(m_Lock);
		std::unique_ptr<CHttpRequest> m_pGet PT_GUARDED_BY(m_Lock);
		void Run() override REQUIRES(!m_Lock);

	public:
		CJob(std::shared_ptr<CData> pData) :
			m_pData(std::move(pData)) { m_Lock = lock_create(); }
		virtual ~CJob() { lock_destroy(m_Lock); }
		void Abort() REQUIRES(!m_Lock);
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
		str_copy(m_pData->m_aaUrls[i], ppUrls[i]);
	}
}

CChooseMaster::~CChooseMaster()
{
	if(m_pJob)
	{
		m_pJob->Abort();
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
	if(m_pJob == nullptr || m_pJob->Status() == IJob::STATE_DONE)
		m_pEngine->AddJob(m_pJob = std::make_shared<CJob>(m_pData));
}

void CChooseMaster::CJob::Abort()
{
	CLockScope ls(m_Lock);
	if(m_pHead != nullptr)
	{
		m_pHead->Abort();
	}

	if(m_pGet != nullptr)
	{
		m_pGet->Abort();
	}
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
	CTimeout Timeout{10000, 0, 8000, 10};
	int aTimeMs[MAX_URLS];
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		aTimeMs[i] = -1;
		const char *pUrl = m_pData->m_aaUrls[aRandomized[i]];
		CHttpRequest *pHead = HttpHead(pUrl).release();
		pHead->Timeout(Timeout);
		pHead->LogProgress(HTTPLOG::FAILURE);
		{
			CLockScope ls(m_Lock);
			m_pHead = std::unique_ptr<CHttpRequest>(pHead);
		}
		IEngine::RunJobBlocking(pHead);
		if(pHead->State() == HTTP_ABORTED)
		{
			dbg_msg("serverbrowse_http", "master chooser aborted");
			return;
		}
		if(pHead->State() != HTTP_DONE)
		{
			continue;
		}
		auto StartTime = time_get_nanoseconds();
		CHttpRequest *pGet = HttpGet(pUrl).release();
		pGet->Timeout(Timeout);
		pGet->LogProgress(HTTPLOG::FAILURE);
		{
			CLockScope ls(m_Lock);
			m_pGet = std::unique_ptr<CHttpRequest>(pGet);
		}
		IEngine::RunJobBlocking(pGet);
		auto Time = std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - StartTime);
		if(pHead->State() == HTTP_ABORTED)
		{
			dbg_msg("serverbrowse_http", "master chooser aborted");
			return;
		}
		if(pGet->State() != HTTP_DONE)
		{
			continue;
		}
		json_value *pJson = pGet->ResultJson();
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
		dbg_msg("serverbrowse_http", "found master, url='%s' time=%dms", pUrl, (int)Time.count());
		aTimeMs[i] = Time.count();
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
	virtual ~CServerBrowserHttp();
	void Update() override;
	bool IsRefreshing() override { return m_State != STATE_DONE; }
	void Refresh() override;
	bool GetBestUrl(const char **pBestUrl) const override { return m_pChooseMaster->GetBestUrl(pBestUrl); }

	int NumServers() const override
	{
		return m_vServers.size();
	}
	const CServerInfo &Server(int Index) const override
	{
		return m_vServers[Index];
	}
	int NumLegacyServers() const override
	{
		return m_vLegacyServers.size();
	}
	const NETADDR &LegacyServer(int Index) const override
	{
		return m_vLegacyServers[Index];
	}

private:
	enum
	{
		STATE_DONE,
		STATE_WANTREFRESH,
		STATE_REFRESHING,
		STATE_NO_MASTER,
	};

	static bool Validate(json_value *pJson);
	static bool Parse(json_value *pJson, std::vector<CServerInfo> *pvServers, std::vector<NETADDR> *pvLegacyServers);

	IEngine *m_pEngine;
	IConsole *m_pConsole;

	int m_State = STATE_DONE;
	std::shared_ptr<CHttpRequest> m_pGetServers;
	std::unique_ptr<CChooseMaster> m_pChooseMaster;

	std::vector<CServerInfo> m_vServers;
	std::vector<NETADDR> m_vLegacyServers;
};

CServerBrowserHttp::CServerBrowserHttp(IEngine *pEngine, IConsole *pConsole, const char **ppUrls, int NumUrls, int PreviousBestIndex) :
	m_pEngine(pEngine),
	m_pConsole(pConsole),
	m_pChooseMaster(new CChooseMaster(pEngine, Validate, ppUrls, NumUrls, PreviousBestIndex))
{
	m_pChooseMaster->Refresh();
}

CServerBrowserHttp::~CServerBrowserHttp()
{
	if(m_pGetServers != nullptr)
	{
		m_pGetServers->Abort();
	}
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
				m_State = STATE_NO_MASTER;
			}
			return;
		}
		m_pGetServers = HttpGet(pBestUrl);
		// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to fail.
		m_pGetServers->Timeout(CTimeout{10000, 0, 8000, 10});
		m_pEngine->AddJob(m_pGetServers);
		m_State = STATE_REFRESHING;
	}
	else if(m_State == STATE_REFRESHING)
	{
		if(m_pGetServers->State() == HTTP_QUEUED || m_pGetServers->State() == HTTP_RUNNING)
		{
			return;
		}
		m_State = STATE_DONE;
		std::shared_ptr<CHttpRequest> pGetServers = nullptr;
		std::swap(m_pGetServers, pGetServers);

		bool Success = true;
		json_value *pJson = pGetServers->ResultJson();
		Success = Success && pJson;
		Success = Success && !Parse(pJson, &m_vServers, &m_vLegacyServers);
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
	if(m_State == STATE_WANTREFRESH || m_State == STATE_REFRESHING || m_State == STATE_NO_MASTER)
	{
		if(m_State == STATE_NO_MASTER)
			m_State = STATE_WANTREFRESH;
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
	return net_addr_from_str(pOut, aHost) != 0;
}
bool CServerBrowserHttp::Validate(json_value *pJson)
{
	std::vector<CServerInfo> vServers;
	std::vector<NETADDR> vLegacyServers;
	return Parse(pJson, &vServers, &vLegacyServers);
}
bool CServerBrowserHttp::Parse(json_value *pJson, std::vector<CServerInfo> *pvServers, std::vector<NETADDR> *pvLegacyServers)
{
	std::vector<CServerInfo> vServers;
	std::vector<NETADDR> vLegacyServers;

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
		SetInfo.m_NumAddresses = 0;
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			NETADDR ParsedAddr;
			if(ServerbrowserParseUrl(&ParsedAddr, Addresses[a]))
			{
				//dbg_msg("dbg/serverbrowser", "unknown address, i=%d a=%d", i, a);
				// Skip unknown addresses.
				continue;
			}
			if(SetInfo.m_NumAddresses < (int)std::size(SetInfo.m_aAddresses))
			{
				SetInfo.m_aAddresses[SetInfo.m_NumAddresses] = ParsedAddr;
				SetInfo.m_NumAddresses += 1;
			}
		}
		if(SetInfo.m_NumAddresses > 0)
		{
			vServers.push_back(SetInfo);
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
			vLegacyServers.push_back(ParsedAddr);
		}
	}
	*pvServers = vServers;
	*pvLegacyServers = vLegacyServers;
	return false;
}

static const char *DEFAULT_SERVERLIST_URLS[] = {
	"https://master1.ddnet.org/ddnet/15/servers.json",
	"https://master2.ddnet.org/ddnet/15/servers.json",
	"https://master3.ddnet.org/ddnet/15/servers.json",
	"https://master4.ddnet.org/ddnet/15/servers.json",
};

IServerBrowserHttp *CreateServerBrowserHttp(IEngine *pEngine, IConsole *pConsole, IStorage *pStorage, const char *pPreviousBestUrl)
{
	char aaUrls[CChooseMaster::MAX_URLS][256];
	const char *apUrls[CChooseMaster::MAX_URLS] = {0};
	const char **ppUrls = apUrls;
	int NumUrls = 0;
	IOHANDLE File = pStorage->OpenFile("ddnet-serverlist-urls.cfg", IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
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
			str_copy(aaUrls[NumUrls], pLine);
			apUrls[NumUrls] = aaUrls[NumUrls];
			NumUrls += 1;
		}
	}
	if(NumUrls == 0)
	{
		ppUrls = DEFAULT_SERVERLIST_URLS;
		NumUrls = std::size(DEFAULT_SERVERLIST_URLS);
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
