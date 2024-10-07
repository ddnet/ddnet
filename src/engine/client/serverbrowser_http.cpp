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

#include <base/lock.h>
#include <base/log.h>
#include <base/system.h>

#include <memory>
#include <vector>

#include <chrono>

using namespace std::chrono_literals;

static int SanitizeAge(std::optional<int64_t> Age)
{
	// A year is of course pi*10**7 seconds.
	if(!(Age && 0 <= *Age && *Age < 31415927))
	{
		return 31415927;
	}
	return *Age;
}

// Classify HTTP responses into buckets, treat 15 seconds as fresh, 1 minute as
// less fresh, etc. This ensures that differences in the order of seconds do
// not affect master choice.
static int ClassifyAge(int AgeSeconds)
{
	return 0 //
	       + (AgeSeconds >= 15) // 15 seconds
	       + (AgeSeconds >= 60) // 1 minute
	       + (AgeSeconds >= 300) // 5 minutes
	       + (AgeSeconds / 3600); // 1 hour
}

class CChooseMaster
{
public:
	typedef bool (*VALIDATOR)(json_value *pJson);

	enum
	{
		MAX_URLS = 16,
	};
	CChooseMaster(IEngine *pEngine, IHttp *pHttp, VALIDATOR pfnValidator, const char **ppUrls, int NumUrls, int PreviousBestIndex);
	virtual ~CChooseMaster();

	bool GetBestUrl(const char **pBestUrl) const;
	void Reset();
	bool IsRefreshing() const { return m_pJob && !m_pJob->Done(); }
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
		CChooseMaster *m_pParent;
		CLock m_Lock;
		std::shared_ptr<CData> m_pData;
		std::shared_ptr<CHttpRequest> m_pHead;
		std::shared_ptr<CHttpRequest> m_pGet;
		void Run() override REQUIRES(!m_Lock);

	public:
		CJob(CChooseMaster *pParent, std::shared_ptr<CData> pData) :
			m_pParent(pParent),
			m_pData(std::move(pData))
		{
			Abortable(true);
		}
		bool Abort() override REQUIRES(!m_Lock);
	};

	IEngine *m_pEngine;
	IHttp *m_pHttp;
	int m_PreviousBestIndex;
	std::shared_ptr<CData> m_pData;
	std::shared_ptr<CJob> m_pJob;
};

CChooseMaster::CChooseMaster(IEngine *pEngine, IHttp *pHttp, VALIDATOR pfnValidator, const char **ppUrls, int NumUrls, int PreviousBestIndex) :
	m_pEngine(pEngine),
	m_pHttp(pHttp),
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
	if(m_pJob == nullptr || m_pJob->State() == IJob::STATE_DONE)
	{
		m_pJob = std::make_shared<CJob>(this, m_pData);
		m_pEngine->AddJob(m_pJob);
	}
}

bool CChooseMaster::CJob::Abort()
{
	if(!IJob::Abort())
	{
		return false;
	}

	CLockScope ls(m_Lock);
	if(m_pHead != nullptr)
	{
		m_pHead->Abort();
	}

	if(m_pGet != nullptr)
	{
		m_pGet->Abort();
	}

	return true;
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
	int aAgeS[MAX_URLS];
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		aTimeMs[i] = -1;
		aAgeS[i] = SanitizeAge({});
		const char *pUrl = m_pData->m_aaUrls[aRandomized[i]];
		std::shared_ptr<CHttpRequest> pHead = HttpHead(pUrl);
		pHead->Timeout(Timeout);
		pHead->LogProgress(HTTPLOG::FAILURE);
		{
			CLockScope ls(m_Lock);
			m_pHead = pHead;
		}

		m_pParent->m_pHttp->Run(pHead);
		pHead->Wait();
		if(pHead->State() == EHttpState::ABORTED || State() == IJob::STATE_ABORTED)
		{
			log_debug("serverbrowser_http", "master chooser aborted");
			return;
		}
		if(pHead->State() != EHttpState::DONE)
		{
			continue;
		}

		auto StartTime = time_get_nanoseconds();
		std::shared_ptr<CHttpRequest> pGet = HttpGet(pUrl);
		pGet->Timeout(Timeout);
		pGet->LogProgress(HTTPLOG::FAILURE);
		{
			CLockScope ls(m_Lock);
			m_pGet = pGet;
		}

		m_pParent->m_pHttp->Run(pGet);
		pGet->Wait();

		auto Time = std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - StartTime);
		if(pGet->State() == EHttpState::ABORTED || State() == IJob::STATE_ABORTED)
		{
			log_debug("serverbrowser_http", "master chooser aborted");
			return;
		}
		if(pGet->State() != EHttpState::DONE)
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
		int AgeS = SanitizeAge(pGet->ResultAgeSeconds());
		log_info("serverbrowser_http", "found master, url='%s' time=%dms age=%ds", pUrl, (int)Time.count(), AgeS);

		aTimeMs[i] = Time.count();
		aAgeS[i] = AgeS;
	}

	// Determine index of the minimum time.
	int BestIndex = -1;
	int BestTime = 0;
	int BestAge = 0;
	for(int i = 0; i < m_pData->m_NumUrls; i++)
	{
		if(aTimeMs[i] < 0)
		{
			continue;
		}
		if(BestIndex == -1 || std::tuple(ClassifyAge(aAgeS[i]), aTimeMs[i]) < std::tuple(ClassifyAge(BestAge), BestTime))
		{
			BestTime = aTimeMs[i];
			BestAge = aAgeS[i];
			BestIndex = aRandomized[i];
		}
	}
	if(BestIndex == -1)
	{
		log_error("serverbrowser_http", "WARNING: no usable masters found");
		return;
	}

	log_info("serverbrowser_http", "determined best master, url='%s' time=%dms age=%ds", m_pData->m_aaUrls[BestIndex], BestTime, BestAge);
	m_pData->m_BestIndex.store(BestIndex);
}

class CServerBrowserHttp : public IServerBrowserHttp
{
public:
	CServerBrowserHttp(IEngine *pEngine, IHttp *pHttp, const char **ppUrls, int NumUrls, int PreviousBestIndex);
	~CServerBrowserHttp() override;
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

private:
	enum
	{
		STATE_DONE,
		STATE_WANTREFRESH,
		STATE_REFRESHING,
		STATE_NO_MASTER,
	};

	static bool Validate(json_value *pJson);
	static bool Parse(json_value *pJson, std::vector<CServerInfo> *pvServers);

	IHttp *m_pHttp;

	int m_State = STATE_DONE;
	std::shared_ptr<CHttpRequest> m_pGetServers;
	std::unique_ptr<CChooseMaster> m_pChooseMaster;

	std::vector<CServerInfo> m_vServers;
};

CServerBrowserHttp::CServerBrowserHttp(IEngine *pEngine, IHttp *pHttp, const char **ppUrls, int NumUrls, int PreviousBestIndex) :
	m_pHttp(pHttp),
	m_pChooseMaster(new CChooseMaster(pEngine, pHttp, Validate, ppUrls, NumUrls, PreviousBestIndex))
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
				log_error("serverbrowser_http", "no working serverlist URL found");
				m_State = STATE_NO_MASTER;
			}
			return;
		}
		m_pGetServers = HttpGet(pBestUrl);
		// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to fail.
		m_pGetServers->Timeout(CTimeout{10000, 0, 8000, 10});
		m_pHttp->Run(m_pGetServers);
		m_State = STATE_REFRESHING;
	}
	else if(m_State == STATE_REFRESHING)
	{
		if(!m_pGetServers->Done())
		{
			return;
		}
		m_State = STATE_DONE;
		std::shared_ptr<CHttpRequest> pGetServers = nullptr;
		std::swap(m_pGetServers, pGetServers);

		bool Success = true;
		json_value *pJson = pGetServers->State() == EHttpState::DONE ? pGetServers->ResultJson() : nullptr;
		Success = Success && pJson;
		Success = Success && !Parse(pJson, &m_vServers);
		json_value_free(pJson);
		if(!Success)
		{
			log_error("serverbrowser_http", "failed getting serverlist, trying to find best URL");
			m_pChooseMaster->Reset();
			m_pChooseMaster->Refresh();
		}
		else
		{
			// Try to find new master if the current one returns
			// results that are 5 minutes old.
			int Age = SanitizeAge(pGetServers->ResultAgeSeconds());
			if(Age > 300)
			{
				log_info("serverbrowser_http", "got stale serverlist, age=%ds, trying to find best URL", Age);
				m_pChooseMaster->Refresh();
			}
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
	int Failure = net_addr_from_url(pOut, pUrl, nullptr, 0);
	if(Failure || pOut->port == 0)
		return true;
	return false;
}
bool CServerBrowserHttp::Validate(json_value *pJson)
{
	std::vector<CServerInfo> vServers;
	return Parse(pJson, &vServers);
}
bool CServerBrowserHttp::Parse(json_value *pJson, std::vector<CServerInfo> *pvServers)
{
	std::vector<CServerInfo> vServers;

	const json_value &Json = *pJson;
	const json_value &Servers = Json["servers"];
	if(Servers.type != json_array)
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
			log_debug("serverbrowser_http", "skipped due to info, i=%d", i);
			// Only skip the current server on parsing
			// failure; the server info is "user input" by
			// the game server and can be set to arbitrary
			// values.
			continue;
		}
		CServerInfo SetInfo = ParsedInfo;
		SetInfo.m_Location = ParsedLocation;
		SetInfo.m_NumAddresses = 0;
		bool GotVersion6 = false;
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			if(str_startswith(Addresses[a], "tw-0.6+udp://"))
			{
				GotVersion6 = true;
				break;
			}
		}
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			if(GotVersion6 && str_startswith(Addresses[a], "tw-0.7+udp://"))
			{
				continue;
			}
			NETADDR ParsedAddr;
			if(ServerbrowserParseUrl(&ParsedAddr, Addresses[a]))
			{
				// log_debug("serverbrowser_http", "unknown address, i=%d a=%d", i, a);
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
	*pvServers = vServers;
	return false;
}

static const char *DEFAULT_SERVERLIST_URLS[] = {
	"https://master1.ddnet.org/ddnet/15/servers.json",
	"https://master2.ddnet.org/ddnet/15/servers.json",
	"https://master3.ddnet.org/ddnet/15/servers.json",
	"https://master4.ddnet.org/ddnet/15/servers.json",
};

IServerBrowserHttp *CreateServerBrowserHttp(IEngine *pEngine, IStorage *pStorage, IHttp *pHttp, const char *pPreviousBestUrl)
{
	char aaUrls[CChooseMaster::MAX_URLS][256];
	const char *apUrls[CChooseMaster::MAX_URLS] = {0};
	const char **ppUrls = apUrls;
	int NumUrls = 0;
	CLineReader LineReader;
	if(LineReader.OpenFile(pStorage->OpenFile("ddnet-serverlist-urls.cfg", IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		while(const char *pLine = LineReader.Get())
		{
			if(NumUrls == CChooseMaster::MAX_URLS)
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
	return new CServerBrowserHttp(pEngine, pHttp, ppUrls, NumUrls, PreviousBestIndex);
}
