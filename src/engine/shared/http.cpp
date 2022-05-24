#include "http.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/version.h>

#if !defined(CONF_FAMILY_WINDOWS)
#include <csignal>
#endif

#define WIN32_LEAN_AND_MEAN
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#ifdef CONF_FAMILY_WINDOWS
#undef min
#undef max
#endif

static const char g_Wakeup = 'w';

int CHttp::CurlDebug(CURL *pHandle, unsigned int Type, char *pData, size_t DataSize, void *pUser)
{
	curl_infotype InfoType = static_cast<curl_infotype>(Type);
	char TypeChar;
	switch(InfoType)
	{
	case CURLINFO_TEXT:
		TypeChar = '*';
		break;
	case CURLINFO_HEADER_OUT:
		TypeChar = '<';
		break;
	case CURLINFO_HEADER_IN:
		TypeChar = '>';
		break;
	default:
		return 0;
	}

	while(const char *pLineEnd = (const char *)memchr(pData, '\n', DataSize))
	{
		int LineLength = pLineEnd - pData;
		log_debug("curl", "%c %.*s", TypeChar, LineLength, pData);
		pData += LineLength + 1;
		DataSize -= LineLength + 1;
	}
	return 0;
}

void CHttp::EscapeUrl(char *pBuf, int Size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, Size);
	curl_free(pEsc);
}

CHttpRequest::CHttpRequest(const char *pUrl)
{
	str_copy(m_aUrl, pUrl, sizeof(m_aUrl));
}

CHttpRequest::~CHttpRequest()
{
	if(!m_WriteToFile)
	{
		m_BufferSize = 0;
		m_BufferLength = 0;
		free(m_pBuffer);
		m_pBuffer = nullptr;
	}
	curl_slist_free_all((curl_slist *)m_pHeaders);
	m_pHeaders = nullptr;
	if(m_pBody)
	{
		m_BodyLength = 0;
		free(m_pBody);
		m_pBody = nullptr;
	}
}

bool CHttpRequest::BeforeInit()
{
	if(m_WriteToFile)
	{
		if(fs_makedir_rec_for(m_aDestAbsolute) < 0)
		{
			dbg_msg("http", "i/o error, cannot create folder for: %s", m_aDest);
			return false;
		}

		m_File = io_open(m_aDestAbsolute, IOFLAG_WRITE);
		if(!m_File)
		{
			dbg_msg("http", "i/o error, cannot open file: %s", m_aDest);
			return false;
		}
	}
	return true;
}

bool CHttpRequest::ConfigureHandle(CURL *pHandle, CURLSH *pShare)
{
	if(!BeforeInit())
		return false;

	if(g_Config.m_DbgCurl)
	{
		curl_easy_setopt(pHandle, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(pHandle, CURLOPT_DEBUGFUNCTION, CHttp::CurlDebug);
	}

	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, m_aErr);

	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.ConnectTimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.LowSpeedLimit);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, m_Timeout.LowSpeedTime);

	curl_easy_setopt(pHandle, CURLOPT_SHARE, pShare);
	curl_easy_setopt(pHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pHandle, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");
	curl_easy_setopt(pHandle, CURLOPT_ACCEPT_ENCODING, ""); // Use any compression algorithm supported by libcurl.

	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
	curl_easy_setopt(pHandle, CURLOPT_IPRESOLVE, m_IpResolve == IPRESOLVE::V4 ? CURL_IPRESOLVE_V4 : m_IpResolve == IPRESOLVE::V6 ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_WHATEVER);

	if(curl_version_info(CURLVERSION_NOW)->version_num < 0x074400)
	{
		// Causes crashes, see https://github.com/ddnet/ddnet/issues/4342.
		// No longer a problem in curl 7.68 and above, and 0x44 = 68.
		curl_easy_setopt(pHandle, CURLOPT_FORBID_REUSE, 1L);
	}

#ifdef CONF_PLATFORM_ANDROID
	curl_easy_setopt(pHandle, CURLOPT_CAINFO, "data/cacert.pem");
#endif

	switch(m_Type)
	{
	case REQUEST::GET:
		break;
	case REQUEST::HEAD:
		curl_easy_setopt(pHandle, CURLOPT_NOBODY, 1L);
		break;
	case REQUEST::POST:
	case REQUEST::POST_JSON:
		if(m_Type == REQUEST::POST_JSON)
		{
			Header("Content-Type: application/json");
		}
		else
		{
			Header("Content-Type:");
		}
		curl_easy_setopt(pHandle, CURLOPT_POSTFIELDS, m_pBody);
		curl_easy_setopt(pHandle, CURLOPT_POSTFIELDSIZE, m_BodyLength);
		break;
	}

	curl_easy_setopt(pHandle, CURLOPT_HTTPHEADER, m_pHeaders);

	return true;
}

size_t CHttpRequest::OnData(char *pData, size_t DataSize)
{
	if(!m_WriteToFile)
	{
		if(DataSize == 0)
		{
			return DataSize;
		}
		size_t NewBufferSize = maximum((size_t)1024, m_BufferSize);
		while(m_BufferLength + DataSize > NewBufferSize)
		{
			NewBufferSize *= 2;
		}
		if(NewBufferSize != m_BufferSize)
		{
			m_pBuffer = (unsigned char *)realloc(m_pBuffer, NewBufferSize);
			m_BufferSize = NewBufferSize;
		}
		mem_copy(m_pBuffer + m_BufferLength, pData, DataSize);
		m_BufferLength += DataSize;
		return DataSize;
	}
	else
	{
		return io_write(m_File, pData, DataSize);
	}
}

size_t CHttpRequest::WriteCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	return ((CHttpRequest *)pUser)->OnData(pData, Size * Number);
}

int CHttpRequest::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CHttpRequest *pTask = (CHttpRequest *)pUser;
	pTask->m_Current.store(DlCurr, std::memory_order_relaxed);
	pTask->m_Size.store(DlTotal, std::memory_order_relaxed);
	pTask->m_Progress.store((100 * DlCurr) / (DlTotal ? DlTotal : 1), std::memory_order_relaxed);
	pTask->OnProgress();
	return pTask->m_Abort ? -1 : 0;
}

void CHttpRequest::OnStart()
{
	if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
		dbg_msg("http", "fetching %s", m_aUrl);
}

void CHttpRequest::OnCompletionInternal(unsigned int Result)
{
	CURLcode Code = static_cast<CURLcode>(Result);
	int State;
	if(Code != CURLE_OK)
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
			dbg_msg("http", "%s failed. libcurl error: %s", m_aUrl, m_aErr);
		State = (Code == CURLE_ABORTED_BY_CALLBACK) ? HTTP_ABORTED : HTTP_ERROR;
	}
	else
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
			dbg_msg("http", "task done %s", m_aUrl);
		State = HTTP_DONE;
	}

	if(m_WriteToFile)
	{
		if(m_File && io_close(m_File) != 0)
		{
			dbg_msg("http", "i/o error, cannot close file: %s", m_aDest);
			State = HTTP_ERROR;
		}

		if(State == HTTP_ERROR || State == HTTP_ABORTED)
		{
			fs_remove(m_aDestAbsolute);
		}
	}

	m_State = State;
	OnCompletion();

	std::lock_guard l(m_DoneLock);
	m_Done = true;
	m_DoneCV.notify_one();
}

void CHttpRequest::WriteToFile(IStorage *pStorage, const char *pDest, int StorageType)
{
	m_WriteToFile = true;
	str_copy(m_aDest, pDest, sizeof(m_aDest));
	if(StorageType == -2)
	{
		pStorage->GetBinaryPath(m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
	else
	{
		pStorage->GetCompletePath(StorageType, m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
}

void CHttpRequest::Header(const char *pNameColonValue)
{
	m_pHeaders = curl_slist_append((curl_slist *)m_pHeaders, pNameColonValue);
}

void CHttpRequest::Wait()
{
	std::unique_lock l(m_DoneLock);
	m_DoneCV.wait(l, [this] { return m_Done.load(); });
}

void CHttpRequest::Result(unsigned char **ppResult, size_t *pResultLength) const
{
	if(m_WriteToFile || State() != HTTP_DONE)
	{
		*ppResult = nullptr;
		*pResultLength = 0;
		return;
	}
	*ppResult = m_pBuffer;
	*pResultLength = m_BufferLength;
}

json_value *CHttpRequest::ResultJson() const
{
	unsigned char *pResult;
	size_t ResultLength;
	Result(&pResult, &ResultLength);
	if(!pResult)
	{
		return nullptr;
	}
	return json_parse((char *)pResult, ResultLength);
}

bool CHttp::Init()
{
#if !defined(CONF_FAMILY_WINDOWS)
	// As a multithreaded application we have to tell curl to not install signal
	// handlers and instead ignore SIGPIPE from OpenSSL ourselves.
	signal(SIGPIPE, SIG_IGN);
#endif

	NETSOCKET SockPair[2];
	if(!net_tcp_socketpair(SockPair))
		return false;

	if(net_set_non_blocking(SockPair[0]) || net_set_non_blocking(SockPair[1]))
		goto error;

	m_WaitSocket = SockPair[0];
	m_InterruptSocket = SockPair[1];

	m_pThread = thread_init(CHttp::ThreadMain, this, "http");
	if(!m_pThread)
		goto error;

	return true;

error:
	// Should probably mark CHttp as inop, unless CHttp::Init failing should be fatal
	net_tcp_close(SockPair[0]);
	net_tcp_close(SockPair[1]);
	return false;
}

// Only call when holding m_Lock
void CHttp::WakeUp()
{
	net_tcp_send(m_InterruptSocket, &g_Wakeup, sizeof(g_Wakeup));
}

void CHttp::AddRequest(std::shared_ptr<CHttpRequest> pRequest)
{
	std::lock_guard l(m_Lock);
	if(m_Shutdown)
		return;

	m_PendingRequests.emplace(std::move(pRequest));
	WakeUp();
}

void CHttp::ThreadMain(void *pUser)
{
	CHttp *pSelf = static_cast<CHttp *>(pUser);
	pSelf->Run();
}

void CHttp::Run()
{
	static_assert(CURL_ERROR_SIZE == 256); // CHttpRequest::m_aErr
	// CHttpRequest::OnCompletionInternal
	static_assert(std::numeric_limits<std::underlying_type_t<CURLcode>>::min() >= std::numeric_limits<unsigned int>::min() && // NOLINT(misc-redundant-expression)
		      std::numeric_limits<std::underlying_type_t<CURLcode>>::max() <= std::numeric_limits<unsigned int>::max()); // NOLINT(misc-redundant-expression)

	// CHttp::CurlDebug
	static_assert(std::numeric_limits<std::underlying_type_t<curl_infotype>>::min() >= std::numeric_limits<unsigned int>::min() && // NOLINT(misc-redundant-expression)
		      std::numeric_limits<std::underlying_type_t<curl_infotype>>::max() <= std::numeric_limits<unsigned int>::max()); // NOLINT(misc-redundant-expression)

	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		return; //TODO: Report the error somehow
	}

	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		dbg_msg("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	m_pShare = curl_share_init();
	if(!m_pShare)
	{
		return; //TODO: Report the error somehow
	}
	curl_share_setopt(m_pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);

	if(!(m_pHandle = curl_multi_init()))
	{
		return; //TODO: Report the error somehow
	}

	curl_waitfd ExtraFds[] = {{static_cast<curl_socket_t>(net_socket_v4_getraw(m_WaitSocket)), CURL_POLL_IN, 0}};
	while(!m_Shutdown.load(std::memory_order_seq_cst))
	{
		int Running = 0;
		CURLMcode mc = curl_multi_wait(m_pHandle, ExtraFds, sizeof(ExtraFds) / sizeof(ExtraFds[0]), 100, NULL);

		// Discard any data on m_WaitSocket so it doesn't wake us up again
		char aBuf[64];
		while(net_tcp_recv(m_WaitSocket, aBuf, sizeof(aBuf)) > 0)
			;

		if(m_Shutdown.load(std::memory_order_seq_cst))
			break;

		if(mc != CURLM_OK)
			break; //TOOD: Report the error somehow

		mc = curl_multi_perform(m_pHandle, &Running);
		if(mc != CURLM_OK)
			break; //TOOD: Report the error somehow

		{
			struct CURLMsg *m;
			int Remaining = 0;
			while((m = curl_multi_info_read(m_pHandle, &Remaining)))
			{
				if(m->msg == CURLMSG_DONE)
				{
					auto RequestIt = m_RunningRequests.find(m->easy_handle);
					dbg_assert(RequestIt != m_RunningRequests.end(), "Running handle not added to map");
					auto Request = RequestIt->second;

					Request->OnCompletionInternal(m->data.result);

					curl_multi_remove_handle(m_pHandle, m->easy_handle);
					curl_easy_cleanup(m->easy_handle);

					m_RunningRequests.erase(RequestIt);
				}
			}
		}

		std::unique_lock l(m_Lock);
		decltype(m_PendingRequests) NewRequests;
		std::swap(m_PendingRequests, NewRequests);
		l.unlock();

		while(!NewRequests.empty())
		{
			auto pRequest = std::move(NewRequests.front());
			NewRequests.pop();

			CURL *pHandle = curl_easy_init();
			if(!pHandle)
			{
				m_Shutdown = true;
				break;
			}

			if(!pRequest->ConfigureHandle(pHandle, m_pShare))
			{
				pRequest->m_State = HTTP_ERROR;
				dbg_msg("http", "Invalid request"); //TODO: More details?
				continue;
			}

			pRequest->m_State = HTTP_RUNNING;
			pRequest->OnStart();
			m_RunningRequests.emplace(std::make_pair(pHandle, std::move(pRequest)));
			curl_multi_add_handle(m_pHandle, pHandle);
		}
	}

	std::unique_lock l(m_Lock);
	m_Shutdown = true;

	for(auto &ReqPair : m_RunningRequests)
	{
		auto &[pHandle, pRequest] = ReqPair;
		curl_multi_remove_handle(m_pHandle, pHandle);
		curl_easy_cleanup(pHandle);

		// Emulate CURLE_ABORTED_BY_CALLBACK
		str_copy(pRequest->m_aErr, "Shutting down", sizeof(pRequest->m_aErr));
		pRequest->OnCompletionInternal(CURLE_ABORTED_BY_CALLBACK);
	}

	m_RunningRequests.clear();

	while(!m_PendingRequests.empty())
	{
		auto pRequest = std::move(m_PendingRequests.front());
		m_PendingRequests.pop();

		// Emulate CURLE_ABORTED_BY_CALLBACK
		str_copy(pRequest->m_aErr, "Shutting down", sizeof(pRequest->m_aErr));
		pRequest->OnCompletionInternal(CURLE_ABORTED_BY_CALLBACK);
	}

	curl_share_cleanup(m_pShare);
	curl_multi_cleanup(m_pHandle);
	curl_global_cleanup();
}

CHttp::~CHttp()
{
	if(m_pThread)
	{
		std::unique_lock l(m_Lock);
		m_Shutdown = true;
		WakeUp();
		l.unlock();
		thread_wait(m_pThread);
	}

	net_tcp_close(m_WaitSocket);
	net_tcp_close(m_InterruptSocket);
}
