#include "http.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/version.h>

#include <limits>
#include <thread>

#if !defined(CONF_FAMILY_WINDOWS)
#include <csignal>
#endif

#define WIN32_LEAN_AND_MEAN
#include <curl/curl.h>

// There is a stray constant on Windows/MSVC...
#ifdef ERROR
#undef ERROR
#endif

int CurlDebug(CURL *pHandle, curl_infotype Type, char *pData, size_t DataSize, void *pUser)
{
	char TypeChar;
	switch(Type)
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

void EscapeUrl(char *pBuf, int Size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, Size);
	curl_free(pEsc);
}

bool HttpHasIpresolveBug()
{
	// curl < 7.77.0 doesn't use CURLOPT_IPRESOLVE correctly wrt.
	// connection caches.
	return curl_version_info(CURLVERSION_NOW)->version_num < 0x074d00;
}

CHttpRequest::CHttpRequest(const char *pUrl)
{
	str_copy(m_aUrl, pUrl);
	sha256_init(&m_ActualSha256);
}

CHttpRequest::~CHttpRequest()
{
	m_ResponseLength = 0;
	if(!m_WriteToFile)
	{
		m_BufferSize = 0;
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

bool CHttpRequest::ConfigureHandle(void *pHandle)
{
	CURL *pH = (CURL *)pHandle;
	if(!BeforeInit())
	{
		return false;
	}

	if(g_Config.m_DbgCurl)
	{
		curl_easy_setopt(pH, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(pH, CURLOPT_DEBUGFUNCTION, CurlDebug);
	}
	long Protocols = CURLPROTO_HTTPS;
	if(g_Config.m_HttpAllowInsecure)
	{
		Protocols |= CURLPROTO_HTTP;
	}

	curl_easy_setopt(pH, CURLOPT_ERRORBUFFER, m_aErr);

	curl_easy_setopt(pH, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.ConnectTimeoutMs);
	curl_easy_setopt(pH, CURLOPT_TIMEOUT_MS, m_Timeout.TimeoutMs);
	curl_easy_setopt(pH, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.LowSpeedLimit);
	curl_easy_setopt(pH, CURLOPT_LOW_SPEED_TIME, m_Timeout.LowSpeedTime);
	if(m_MaxResponseSize >= 0)
	{
		curl_easy_setopt(pH, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)m_MaxResponseSize);
	}

	// ‘CURLOPT_PROTOCOLS’ is deprecated: since 7.85.0. Use CURLOPT_PROTOCOLS_STR
	// Wait until all platforms have 7.85.0
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	curl_easy_setopt(pH, CURLOPT_PROTOCOLS, Protocols);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	curl_easy_setopt(pH, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pH, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pH, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pH, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pH, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pH, CURLOPT_USERAGENT, GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");
	curl_easy_setopt(pH, CURLOPT_ACCEPT_ENCODING, ""); // Use any compression algorithm supported by libcurl.

	curl_easy_setopt(pH, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pH, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pH, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pH, CURLOPT_PROGRESSDATA, this);
	// ‘CURLOPT_PROGRESSFUNCTION’ is deprecated: since 7.32.0. Use CURLOPT_XFERINFOFUNCTION
	// See problems with curl_off_t type in header file in https://github.com/ddnet/ddnet/pull/6185/
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	curl_easy_setopt(pH, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	curl_easy_setopt(pH, CURLOPT_IPRESOLVE, m_IpResolve == IPRESOLVE::V4 ? CURL_IPRESOLVE_V4 : m_IpResolve == IPRESOLVE::V6 ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_WHATEVER);
	if(g_Config.m_Bindaddr[0] != '\0')
	{
		curl_easy_setopt(pH, CURLOPT_INTERFACE, g_Config.m_Bindaddr);
	}

	if(curl_version_info(CURLVERSION_NOW)->version_num < 0x074400)
	{
		// Causes crashes, see https://github.com/ddnet/ddnet/issues/4342.
		// No longer a problem in curl 7.68 and above, and 0x44 = 68.
		curl_easy_setopt(pH, CURLOPT_FORBID_REUSE, 1L);
	}

#ifdef CONF_PLATFORM_ANDROID
	curl_easy_setopt(pHandle, CURLOPT_CAINFO, "data/cacert.pem");
#endif

	switch(m_Type)
	{
	case REQUEST::GET:
		break;
	case REQUEST::HEAD:
		curl_easy_setopt(pH, CURLOPT_NOBODY, 1L);
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
		curl_easy_setopt(pH, CURLOPT_POSTFIELDS, m_pBody);
		curl_easy_setopt(pH, CURLOPT_POSTFIELDSIZE, m_BodyLength);
		break;
	}

	curl_easy_setopt(pH, CURLOPT_HTTPHEADER, m_pHeaders);

	return true;
}

size_t CHttpRequest::OnData(char *pData, size_t DataSize)
{
	// Need to check for the maximum response size here as curl can only
	// guarantee it if the server sets a Content-Length header.
	if(m_MaxResponseSize >= 0 && m_ResponseLength + DataSize > (uint64_t)m_MaxResponseSize)
	{
		return 0;
	}

	sha256_update(&m_ActualSha256, pData, DataSize);

	if(!m_WriteToFile)
	{
		if(DataSize == 0)
		{
			return DataSize;
		}
		size_t NewBufferSize = maximum((size_t)1024, m_BufferSize);
		while(m_ResponseLength + DataSize > NewBufferSize)
		{
			NewBufferSize *= 2;
		}
		if(NewBufferSize != m_BufferSize)
		{
			m_pBuffer = (unsigned char *)realloc(m_pBuffer, NewBufferSize);
			m_BufferSize = NewBufferSize;
		}
		mem_copy(m_pBuffer + m_ResponseLength, pData, DataSize);
		m_ResponseLength += DataSize;
		return DataSize;
	}
	else
	{
		m_ResponseLength += DataSize;
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

void CHttpRequest::OnCompletionInternal(std::optional<unsigned int> Result)
{
	EHttpState State;
	if(Result.has_value())
	{
		CURLcode Code = static_cast<CURLcode>(Result.value());
		if(Code != CURLE_OK)
		{
			if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
				dbg_msg("http", "%s failed. libcurl error (%u): %s", m_aUrl, Code, m_aErr);
			State = (Code == CURLE_ABORTED_BY_CALLBACK) ? EHttpState::ABORTED : EHttpState::ERROR;
		}
		else
		{
			if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
				dbg_msg("http", "task done: %s", m_aUrl);
			State = EHttpState::DONE;
		}
	}
	else
	{
		dbg_msg("http", "%s failed. internal error: %s", m_aUrl, m_aErr);
		State = EHttpState::ERROR;
	}

	if(State == EHttpState::DONE && m_ExpectedSha256 != SHA256_ZEROED)
	{
		const SHA256_DIGEST ActualSha256 = sha256_finish(&m_ActualSha256);
		if(ActualSha256 != m_ExpectedSha256)
		{
			if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
			{
				char aActualSha256[SHA256_MAXSTRSIZE];
				sha256_str(ActualSha256, aActualSha256, sizeof(aActualSha256));
				char aExpectedSha256[SHA256_MAXSTRSIZE];
				sha256_str(m_ExpectedSha256, aExpectedSha256, sizeof(aExpectedSha256));
				dbg_msg("http", "SHA256 mismatch: got=%s, expected=%s, url=%s", aActualSha256, aExpectedSha256, m_aUrl);
			}
			State = EHttpState::ERROR;
		}
	}

	if(m_WriteToFile)
	{
		if(m_File && io_close(m_File) != 0)
		{
			dbg_msg("http", "i/o error, cannot close file: %s", m_aDest);
			State = EHttpState::ERROR;
		}

		if(State == EHttpState::ERROR || State == EHttpState::ABORTED)
		{
			fs_remove(m_aDestAbsolute);
		}
	}

	// The globally visible state must be updated after OnCompletion has finished,
	// or other threads may try to access the result of a completed HTTP request,
	// before the result has been initialized/updated in OnCompletion.
	OnCompletion(State);
	m_State = State;
}

void CHttpRequest::WriteToFile(IStorage *pStorage, const char *pDest, int StorageType)
{
	m_WriteToFile = true;
	str_copy(m_aDest, pDest);
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
	using namespace std::chrono_literals;

	// This is so uncommon that polling just might work
	for(;;)
	{
		EHttpState State = m_State.load(std::memory_order_seq_cst);
		if(State != EHttpState::QUEUED && State != EHttpState::RUNNING)
		{
			return;
		}
		std::this_thread::sleep_for(10ms);
	}
}

void CHttpRequest::Result(unsigned char **ppResult, size_t *pResultLength) const
{
	if(m_WriteToFile || State() != EHttpState::DONE)
	{
		*ppResult = nullptr;
		*pResultLength = 0;
		return;
	}
	*ppResult = m_pBuffer;
	*pResultLength = m_ResponseLength;
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

bool CHttp::Init(std::chrono::milliseconds ShutdownDelay)
{
	m_ShutdownDelay = ShutdownDelay;

#if !defined(CONF_FAMILY_WINDOWS)
	// As a multithreaded application we have to tell curl to not install signal
	// handlers and instead ignore SIGPIPE from OpenSSL ourselves.
	signal(SIGPIPE, SIG_IGN);
#endif
	m_pThread = thread_init(CHttp::ThreadMain, this, "http");

	std::unique_lock Lock(m_Lock);
	m_Cv.wait(Lock, [this]() { return m_State != CHttp::UNINITIALIZED; });
	if(m_State != CHttp::RUNNING)
	{
		return false;
	}

	return true;
}

void CHttp::ThreadMain(void *pUser)
{
	CHttp *pHttp = static_cast<CHttp *>(pUser);
	pHttp->RunLoop();
}

void CHttp::RunLoop()
{
	std::unique_lock Lock(m_Lock);
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		dbg_msg("http", "curl_global_init failed");
		m_State = CHttp::ERROR;
		m_Cv.notify_all();
		return;
	}

	m_pMultiH = curl_multi_init();
	if(!m_pMultiH)
	{
		dbg_msg("http", "curl_multi_init failed");
		m_State = CHttp::ERROR;
		m_Cv.notify_all();
		return;
	}

	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		dbg_msg("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	m_State = CHttp::RUNNING;
	m_Cv.notify_all();
	dbg_msg("http", "running");
	Lock.unlock();

	while(m_State == CHttp::RUNNING)
	{
		static int NextTimeout = std::numeric_limits<int>::max();
		int Events = 0;
		CURLMcode mc = curl_multi_poll(m_pMultiH, NULL, 0, NextTimeout, &Events);

		// We may have been woken up for a shutdown
		if(m_Shutdown)
		{
			auto Now = std::chrono::steady_clock::now();
			if(!m_ShutdownTime.has_value())
			{
				m_ShutdownTime = Now + m_ShutdownDelay;
				NextTimeout = m_ShutdownDelay.count();
			}
			else if(m_ShutdownTime < Now || m_RunningRequests.empty())
			{
				break;
			}
		}

		if(mc != CURLM_OK)
		{
			Lock.lock();
			dbg_msg("http", "Failed multi wait: %s", curl_multi_strerror(mc));
			m_State = CHttp::ERROR;
			break;
		}

		mc = curl_multi_perform(m_pMultiH, &Events);
		if(mc != CURLM_OK)
		{
			Lock.lock();
			dbg_msg("http", "Failed multi perform: %s", curl_multi_strerror(mc));
			m_State = CHttp::ERROR;
			break;
		}

		struct CURLMsg *m;
		while((m = curl_multi_info_read(m_pMultiH, &Events)))
		{
			if(m->msg == CURLMSG_DONE)
			{
				auto RequestIt = m_RunningRequests.find(m->easy_handle);
				dbg_assert(RequestIt != m_RunningRequests.end(), "Running handle not added to map");
				auto pRequest = std::move(RequestIt->second);
				m_RunningRequests.erase(RequestIt);

				pRequest->OnCompletionInternal(m->data.result);
				curl_multi_remove_handle(m_pMultiH, m->easy_handle);
				curl_easy_cleanup(m->easy_handle);
			}
		}

		decltype(m_PendingRequests) NewRequests = {};
		Lock.lock();
		std::swap(m_PendingRequests, NewRequests);
		Lock.unlock();

		while(!NewRequests.empty())
		{
			auto &pRequest = NewRequests.front();
			if(g_Config.m_DbgCurl)
				dbg_msg("http", "task: %s %s", CHttpRequest::GetRequestType(pRequest->m_Type), pRequest->m_aUrl);

			CURL *pEH = curl_easy_init();
			if(!pEH)
				goto error_init;

			if(!pRequest->ConfigureHandle(pEH))
				goto error_configure;

			mc = curl_multi_add_handle(m_pMultiH, pEH);
			if(mc != CURLM_OK)
				goto error_configure;

			m_RunningRequests.emplace(pEH, std::move(pRequest));
			NewRequests.pop_front();

			continue;

		error_configure:
			curl_easy_cleanup(pEH);
		error_init:
			dbg_msg("http", "failed to start new request");
			Lock.lock();
			m_State = CHttp::ERROR;
			break;
		}

		// Only happens if m_State == ERROR, thus we already hold the lock
		if(!NewRequests.empty())
		{
			m_PendingRequests.insert(m_PendingRequests.end(), std::make_move_iterator(NewRequests.begin()), std::make_move_iterator(NewRequests.end()));
			break;
		}
	}

	if(!Lock.owns_lock())
		Lock.lock();

	bool Cleanup = m_State != CHttp::ERROR;
	for(auto &pRequest : m_PendingRequests)
	{
		str_copy(pRequest->m_aErr, "Shutting down");
		pRequest->OnCompletionInternal(std::nullopt);
	}

	for(auto &ReqPair : m_RunningRequests)
	{
		auto &[pHandle, pRequest] = ReqPair;
		if(Cleanup)
		{
			curl_multi_remove_handle(m_pMultiH, pHandle);
			curl_easy_cleanup(pHandle);
		}

		str_copy(pRequest->m_aErr, "Shutting down");
		pRequest->OnCompletionInternal(std::nullopt);
	}

	if(Cleanup)
	{
		curl_multi_cleanup(m_pMultiH);
		curl_global_cleanup();
	}
}

void CHttp::Run(std::shared_ptr<IHttpRequest> pRequest)
{
	std::unique_lock Lock(m_Lock);
	m_Cv.wait(Lock, [this]() { return m_State != CHttp::UNINITIALIZED; });
	m_PendingRequests.emplace_back(std::static_pointer_cast<CHttpRequest>(pRequest));
	curl_multi_wakeup(m_pMultiH);
}

void CHttp::Shutdown()
{
	std::unique_lock Lock(m_Lock);
	if(m_Shutdown || m_State != CHttp::RUNNING)
		return;

	m_Shutdown = true;
	curl_multi_wakeup(m_pMultiH);
}

CHttp::~CHttp()
{
	if(!m_pThread)
		return;

	Shutdown();
	thread_wait(m_pThread);
}
