#include "http_curl.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>
#include <base/thread.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <cstring>
#include <limits>

#if !defined(CONF_FAMILY_WINDOWS)
#include <csignal>
#endif

#include <curl/curl.h>

static int CurlDebug(CURL *pHandle, curl_infotype Type, char *pData, size_t DataSize, void *pUser)
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

CHttpRequestCurl::CHttpRequestCurl(const char *pUrl) :
	IHttpRequest(pUrl)
{
}

CHttpRequestCurl::~CHttpRequestCurl()
{
	dbg_assert(m_File == nullptr, "HTTP request file was not closed");
	curl_slist_free_all(m_pRequestHeaders);
}

void CHttpRequestCurl::Header(const char *pNameColonValue)
{
	m_pRequestHeaders = curl_slist_append(m_pRequestHeaders, pNameColonValue);
}

bool CHttpRequestCurl::ConfigureHandle(CURL *pHandle)
{
	if(!BeforeInit())
	{
		return false;
	}

	if(g_Config.m_DbgHttp)
	{
		curl_easy_setopt(pHandle, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(pHandle, CURLOPT_DEBUGFUNCTION, CurlDebug);
	}
	long Protocols = CURLPROTO_HTTPS;
	if(g_Config.m_HttpAllowInsecure)
	{
		Protocols |= CURLPROTO_HTTP;
	}

	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, m_aErr);

	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.m_ConnectTimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_TIMEOUT_MS, m_Timeout.m_TimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.m_LowSpeedLimit);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, m_Timeout.m_LowSpeedTime);
	if(m_MaxResponseSize >= 0)
	{
		curl_easy_setopt(pHandle, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)m_MaxResponseSize);
	}
	if(m_IfModifiedSince >= 0)
	{
		curl_easy_setopt(pHandle, CURLOPT_TIMEVALUE_LARGE, (curl_off_t)m_IfModifiedSince);
		curl_easy_setopt(pHandle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
	}

	// ‘CURLOPT_PROTOCOLS’ is deprecated: since 7.85.0. Use CURLOPT_PROTOCOLS_STR
	// Wait until all platforms have 7.85.0
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	curl_easy_setopt(pHandle, CURLOPT_PROTOCOLS, Protocols);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	if(m_FailOnErrorStatus)
	{
		curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	}
	curl_easy_setopt(pHandle, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, USER_AGENT_STRING);
	curl_easy_setopt(pHandle, CURLOPT_ACCEPT_ENCODING, ""); // Use any compression algorithm supported by libcurl.

	curl_easy_setopt(pHandle, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_HEADERFUNCTION, HeaderCallback);
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	// ‘CURLOPT_PROGRESSFUNCTION’ is deprecated: since 7.32.0. Use CURLOPT_XFERINFOFUNCTION
	// See problems with curl_off_t type in header file in https://github.com/ddnet/ddnet/pull/6185/
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	curl_easy_setopt(pHandle, CURLOPT_IPRESOLVE, m_IpResolve == IPRESOLVE::V4 ? CURL_IPRESOLVE_V4 : (m_IpResolve == IPRESOLVE::V6 ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_WHATEVER));
	if(g_Config.m_Bindaddr[0] != '\0')
	{
		curl_easy_setopt(pHandle, CURLOPT_INTERFACE, g_Config.m_Bindaddr);
	}

	if(curl_version_info(CURLVERSION_NOW)->version_num < 0x074400)
	{
		// Causes crashes, see https://github.com/ddnet/ddnet/issues/4342.
		// No longer a problem in curl 7.68 and above, and 0x44 = 68.
		curl_easy_setopt(pHandle, CURLOPT_FORBID_REUSE, 1L);
	}

#ifdef CONF_PLATFORM_ANDROID
	curl_easy_setopt(pHandle, CURLOPT_CAPATH, "/system/etc/security/cacerts");
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

	curl_easy_setopt(pHandle, CURLOPT_HTTPHEADER, m_pRequestHeaders);

	return true;
}

size_t CHttpRequestCurl::OnHeader(char *pHeader, size_t HeaderSize)
{
	// `pHeader` is NOT null-terminated.
	// `pHeader` has a trailing newline.

	if(HeaderSize <= 1)
	{
		m_ResponseHeadersEnded = true;
		return HeaderSize;
	}
	if(m_ResponseHeadersEnded)
	{
		// redirect, clear old headers
		m_ResponseHeadersEnded = false;
		m_ResultDate = {};
		m_ResultLastModified = {};
	}

	static const char DATE[] = "Date: ";
	static const char LAST_MODIFIED[] = "Last-Modified: ";

	// Trailing newline and null termination evens out.
	if(HeaderSize - 1 >= sizeof(DATE) - 1 && str_startswith_nocase(pHeader, DATE))
	{
		char aValue[128];
		str_truncate(aValue, sizeof(aValue), pHeader + (sizeof(DATE) - 1), HeaderSize - (sizeof(DATE) - 1) - 1);
		int64_t Value = curl_getdate(aValue, nullptr);
		if(Value != -1)
		{
			m_ResultDate = Value;
		}
	}
	if(HeaderSize - 1 >= sizeof(LAST_MODIFIED) - 1 && str_startswith_nocase(pHeader, LAST_MODIFIED))
	{
		char aValue[128];
		str_truncate(aValue, sizeof(aValue), pHeader + (sizeof(LAST_MODIFIED) - 1), HeaderSize - (sizeof(LAST_MODIFIED) - 1) - 1);
		int64_t Value = curl_getdate(aValue, nullptr);
		if(Value != -1)
		{
			m_ResultLastModified = Value;
		}
	}

	return HeaderSize;
}

void CHttpRequestCurl::OnCompletionInternal(CURL *pHandle, CURLcode Code)
{
	if(pHandle)
	{
		long StatusCode;
		curl_easy_getinfo(pHandle, CURLINFO_RESPONSE_CODE, &StatusCode);
		m_StatusCode = StatusCode;
	}

	EHttpState State;
	if(Code != CURLE_OK)
	{
		if(g_Config.m_DbgHttp || m_LogProgress >= HTTPLOG::FAILURE)
		{
			log_error("http", "%s failed. libcurl error (%u): %s", m_aUrl, Code, m_aErr[0] != '\0' ? m_aErr : curl_easy_strerror(Code));
		}
		State = (Code == CURLE_ABORTED_BY_CALLBACK) ? EHttpState::ABORTED : EHttpState::ERROR;
	}
	else
	{
		if(g_Config.m_DbgHttp || m_LogProgress >= HTTPLOG::ALL)
		{
			log_info("http", "task done: %s", m_aUrl);
		}
		State = EHttpState::DONE;
	}

	IHttpRequest::OnCompletionInternal(State);
}

size_t CHttpRequestCurl::HeaderCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	dbg_assert(Size == 1, "invalid size parameter passed to header callback");
	return ((CHttpRequestCurl *)pUser)->OnHeader(pData, Number);
}

size_t CHttpRequestCurl::WriteCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	return ((CHttpRequestCurl *)pUser)->OnData(pData, Size * Number);
}

int CHttpRequestCurl::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CHttpRequestCurl *pTask = (CHttpRequestCurl *)pUser;
	pTask->m_Current.store(DlCurr, std::memory_order_relaxed);
	pTask->m_Size.store(DlTotal, std::memory_order_relaxed);
	pTask->m_Progress.store(DlTotal == 0.0 ? 0 : (100 * DlCurr) / DlTotal, std::memory_order_relaxed);
	if(pTask->m_pProgressCallback != nullptr)
	{
		pTask->m_pProgressCallback->OnProgress();
	}
	return pTask->m_Abort ? -1 : 0;
}

std::unique_ptr<IHttpRequest> CreateHttpRequest(const char *pUrl)
{
	return std::make_unique<CHttpRequestCurl>(pUrl);
}

void EscapeUrl(char *pBuffer, size_t BufferSize, const char *pUrl)
{
	char *pEscapedUrl = curl_easy_escape(nullptr, pUrl, 0);
	str_copy(pBuffer, pEscapedUrl, BufferSize);
	curl_free(pEscapedUrl);
}

bool CHttpCurl::Init(std::chrono::milliseconds ShutdownDelay)
{
	m_ShutdownDelay = ShutdownDelay;

#if !defined(CONF_FAMILY_WINDOWS)
	// As a multithreaded application we have to tell curl to not install signal
	// handlers and instead ignore SIGPIPE from OpenSSL ourselves.
	signal(SIGPIPE, SIG_IGN);
#endif
	m_pThread = thread_init(CHttpCurl::ThreadMain, this, "http");

	std::unique_lock Lock(m_Lock);
	m_ConditionVariableInit.wait(Lock, [this]() { return m_State != CHttpCurl::UNINITIALIZED; });
	if(m_State != CHttpCurl::RUNNING)
	{
		return false;
	}

	return true;
}

void CHttpCurl::Shutdown()
{
	std::unique_lock Lock(m_Lock);
	if(m_Shutdown || m_State != CHttpCurl::RUNNING)
		return;

	m_Shutdown = true;
	curl_multi_wakeup(m_pMultiH);
}

CHttpCurl::~CHttpCurl()
{
	if(!m_pThread)
		return;

	Shutdown();
	thread_wait(m_pThread);
}

void CHttpCurl::Run(std::shared_ptr<IHttpRequest> pRequest)
{
	std::shared_ptr<CHttpRequestCurl> pRequestImpl = std::static_pointer_cast<CHttpRequestCurl>(pRequest);
	std::unique_lock Lock(m_Lock);
	if(m_Shutdown || m_State == CHttpCurl::ERROR)
	{
		str_copy(pRequestImpl->m_aErr, "Shutting down");
		pRequestImpl->OnCompletionInternal(nullptr, CURLE_ABORTED_BY_CALLBACK);
		return;
	}
	m_ConditionVariableInit.wait(Lock, [this]() { return m_State != CHttpCurl::UNINITIALIZED; });
	m_PendingRequests.emplace_back(pRequestImpl);
	curl_multi_wakeup(m_pMultiH);
}

bool CHttpCurl::HasIpresolveBug() const
{
	// curl < 7.77.0 doesn't use CURLOPT_IPRESOLVE correctly wrt.
	// connection caches.
	return curl_version_info(CURLVERSION_NOW)->version_num < 0x074d00;
}

void CHttpCurl::ThreadMain(void *pUser)
{
	static_cast<CHttpCurl *>(pUser)->RunLoop();
}

void CHttpCurl::RunLoop()
{
	std::unique_lock Lock(m_Lock);
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		log_error("http", "curl_global_init failed");
		m_State = CHttpCurl::ERROR;
		m_ConditionVariableInit.notify_all();
		return;
	}

	m_pMultiH = curl_multi_init();
	if(!m_pMultiH)
	{
		log_error("http", "curl_multi_init failed");
		m_State = CHttpCurl::ERROR;
		m_ConditionVariableInit.notify_all();
		return;
	}

	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		log_info("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	m_State = CHttpCurl::RUNNING;
	m_ConditionVariableInit.notify_all();
	Lock.unlock();
	m_NextTimeout = std::numeric_limits<int>::max();

	while(m_State == CHttpCurl::RUNNING)
	{
		int Events = 0;
		const CURLMcode PollCode = curl_multi_poll(m_pMultiH, nullptr, 0, m_NextTimeout, &Events);

		// We may have been woken up for a shutdown
		if(m_Shutdown)
		{
			if(m_RunningRequests.empty() && m_PendingRequests.empty())
				break;

			auto Now = std::chrono::steady_clock::now();
			if(!m_ShutdownTime.has_value())
			{
				m_ShutdownTime = Now + m_ShutdownDelay;
				m_NextTimeout = m_ShutdownDelay.count();
			}
			else if(m_ShutdownTime < Now)
			{
				break;
			}
		}

		if(PollCode != CURLM_OK)
		{
			Lock.lock();
			log_error("http", "curl_multi_poll failed: %s", curl_multi_strerror(PollCode));
			m_State = CHttpCurl::ERROR;
			break;
		}

		const CURLMcode PerformCode = curl_multi_perform(m_pMultiH, &Events);
		if(PerformCode != CURLM_OK)
		{
			Lock.lock();
			log_error("http", "curl_multi_perform failed: %s", curl_multi_strerror(PerformCode));
			m_State = CHttpCurl::ERROR;
			break;
		}

		struct CURLMsg *pMsg;
		while((pMsg = curl_multi_info_read(m_pMultiH, &Events)))
		{
			if(pMsg->msg == CURLMSG_DONE)
			{
				auto RequestIt = m_RunningRequests.find(pMsg->easy_handle);
				dbg_assert(RequestIt != m_RunningRequests.end(), "Running handle not added to map");
				auto pRequest = std::move(RequestIt->second);
				m_RunningRequests.erase(RequestIt);

				pRequest->OnCompletionInternal(pMsg->easy_handle, pMsg->data.result);
				curl_multi_remove_handle(m_pMultiH, pMsg->easy_handle);
				curl_easy_cleanup(pMsg->easy_handle);
			}
		}

		decltype(m_PendingRequests) NewRequests = {};
		Lock.lock();
		std::swap(m_PendingRequests, NewRequests);
		Lock.unlock();

		while(!NewRequests.empty())
		{
			auto &pRequest = NewRequests.front();
			if(g_Config.m_DbgHttp)
				log_debug("http", "task: %s %s", CHttpRequestCurl::GetRequestType(pRequest->m_Type), pRequest->m_aUrl);

			if(pRequest->ShouldSkipRequest())
			{
				if(pRequest->m_pProgressCallback != nullptr)
				{
					pRequest->m_pProgressCallback->OnCompletion(EHttpState::DONE);
				}
				{
					std::unique_lock WaitLock(pRequest->m_WaitMutex);
					pRequest->m_State = EHttpState::DONE;
				}
				pRequest->m_WaitCondition.notify_all();
				NewRequests.pop_front();
				continue;
			}

			CURL *pEH = curl_easy_init();
			if(!pEH)
			{
				log_error("http", "curl_easy_init failed");
				goto error_init;
			}

			if(!pRequest->ConfigureHandle(pEH))
			{
				curl_easy_cleanup(pEH);
				str_copy(pRequest->m_aErr, "Failed to initialize request");
				pRequest->OnCompletionInternal(nullptr, CURLE_ABORTED_BY_CALLBACK);
				NewRequests.pop_front();
				continue;
			}

			if(curl_multi_add_handle(m_pMultiH, pEH) != CURLM_OK)
			{
				log_error("http", "curl_multi_add_handle failed");
				goto error_configure;
			}

			{
				std::unique_lock WaitLock(pRequest->m_WaitMutex);
				pRequest->m_State = EHttpState::RUNNING;
			}
			m_RunningRequests.emplace(pEH, std::move(pRequest));
			NewRequests.pop_front();
			continue;

		error_configure:
			curl_easy_cleanup(pEH);
		error_init:
			Lock.lock();
			m_State = CHttpCurl::ERROR;
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

	bool Cleanup = m_State != CHttpCurl::ERROR;
	for(auto &pRequest : m_PendingRequests)
	{
		str_copy(pRequest->m_aErr, "Shutting down");
		pRequest->OnCompletionInternal(nullptr, CURLE_ABORTED_BY_CALLBACK);
	}
	m_PendingRequests.clear();

	for(auto &ReqPair : m_RunningRequests)
	{
		auto &[pHandle, pRequest] = ReqPair;

		str_copy(pRequest->m_aErr, "Shutting down");
		pRequest->OnCompletionInternal(pHandle, CURLE_ABORTED_BY_CALLBACK);

		if(Cleanup)
		{
			curl_multi_remove_handle(m_pMultiH, pHandle);
			curl_easy_cleanup(pHandle);
		}
	}
	m_RunningRequests.clear();

	if(Cleanup)
	{
		curl_multi_cleanup(m_pMultiH);
		curl_global_cleanup();
	}
}

IEngineHttp *CreateEngineHttp()
{
	return new CHttpCurl;
}
