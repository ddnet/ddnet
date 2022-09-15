#include "http.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/version.h>

#if !defined(CONF_FAMILY_WINDOWS)
#include <csignal>
#include <fcntl.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <curl/curl.h>

CHttpRunner gs_Runner;

bool CHttpRunner::Init()
{
#if !defined(CONF_FAMILY_WINDOWS)
	// As a multithreaded application we have to tell curl to not install signal
	// handlers and instead ignore SIGPIPE from OpenSSL ourselves.
	signal(SIGPIPE, SIG_IGN);
#endif

	// Create wakeup pair
#if defined(CONF_FAMILY_UNIX)
	pipe2(m_WakeUpPair, O_NONBLOCK);
#elif defined(CONF_FAMILY_WINDOWS)
	m_WakeUpPair[0] = m_WakeUpPair[1] = net_loop_create();
#endif

	write(m_WakeUpPair[1], "t", 1);
	char aT[10];
	read(m_WakeUpPair[0], aT, sizeof(aT));
	dbg_assert(aT[0] == 't', "self-test failed");

	thread_init(CHttpRunner::ThreadMain, this, "http_runner");

	std::unique_lock l(m_Lock);
	m_Cv.wait(l, [this]() { return m_State != UNINITIALIZED; });
	if(m_State != RUNNING)
	{
#if defined(CONF_FAMILY_UNIX)
		close(m_WakeUpPair[0]);
		close(m_WakeUpPair[1]);
#elif defined(CONF_FAMILY_WINDOWS)
		net_loop_close(m_WakeUpPair[0]);
#endif

		return true;
	}

	return false;
}

void CHttpRunner::WakeUp()
{
//TODO: Check with TSan to make sure we hold m_Lock
#if defined(CONF_FAMILY_UNIX)
	write(m_WakeUpPair[1], "w", 1);
#elif defined(CONF_FAMILY_WINDOWS)
	net_loop_send(m_WakeUpPair[1], "w", 1);
#endif
}

void CHttpRunner::Run(std::shared_ptr<IEngineRunnable> pRunnable)
{
	std::unique_lock l(m_Lock);
	if(m_Shutdown)
		return;

	m_Cv.wait(l, [this]() { return m_State != UNINITIALIZED; });
	if(m_State == ERROR)
		return; //TODO: Report and handle this properly

	auto pHttpRunnable = std::static_pointer_cast<CHttpRunnable>(pRunnable);
	if(auto pRequest = std::dynamic_pointer_cast<CHttpRequest>(pHttpRunnable))
	{
		pRequest->SetStatus(IEngineRunnable::RUNNING);
		m_PendingRequests.emplace(std::move(pRequest));
		WakeUp();
	}
}

void CHttpRunner::ThreadMain(void *pUser)
{
	CHttpRunner *pSelf = static_cast<CHttpRunner *>(pUser);
	pSelf->RunLoop();
}

void Discard(int fd)
{
	char aBuf[32];
	while(
#if defined(CONF_FAMILY_UNIX)
		read(fd, aBuf, sizeof(aBuf)) >= 0
#elif defined(CONF_FAMILY_WINDOWS)
		net_loop_recv(fd, aBuf, sizeof(aBuf))
#endif
	)
		;
}

void CHttpRunner::RunLoop()
{
	std::unique_lock InitL(m_Lock);
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		dbg_msg("http", "curl_global_init failed");
		m_State = ERROR;
		m_Cv.notify_one();
		return;
	}

	CURLM *MultiH = curl_multi_init();
	if(!MultiH)
	{
		dbg_msg("http", "curl_multi_init failed");
		m_State = ERROR;
		m_Cv.notify_one();
		return;
	}

	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		dbg_msg("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	m_State = RUNNING;
	m_Cv.notify_one();
	InitL.unlock();
	InitL.release();

	curl_waitfd ExtraFds[] = {{static_cast<curl_socket_t>(m_WakeUpPair[0]), CURL_POLL_IN, 0}};
	while(!m_Shutdown)
	{
		int Events = 0;
		CURLMcode mc = curl_multi_wait(MultiH, ExtraFds, sizeof(ExtraFds) / sizeof(ExtraFds[0]), 1000000, &Events);

		std::unique_lock LoopL(m_Lock);
		if(m_Shutdown)
			break;
		LoopL.unlock();

		if(mc != CURLM_OK)
			break; //TODO: Report the error

		// Discard data on the wakeup pair
		Discard(m_WakeUpPair[0]);

		mc = curl_multi_perform(MultiH, &Events);
		if(mc != CURLM_OK)
			break; //TODO: Report the error

		struct CURLMsg *m;
		while((m = curl_multi_info_read(MultiH, &Events)))
		{
			if(m->msg == CURLMSG_DONE)
			{
				CHttpRequest *pRequest;
				curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &pRequest);

				pRequest->OnCompletionInternal(m->data.result);
				curl_multi_remove_handle(MultiH, m->easy_handle);
				curl_easy_cleanup(m->easy_handle);

				if(pRequest->m_pPrev)
					pRequest->m_pPrev->m_pNext = pRequest->m_pNext;
				else
					m_pRunningRequestsHead = pRequest->m_pNext;

				if(pRequest->m_pNext)
					pRequest->m_pNext->m_pPrev = pRequest->m_pPrev;
			}
		}

		decltype(m_PendingRequests) NewRequests = {};
		LoopL.lock();
		std::swap(m_PendingRequests, NewRequests);
		LoopL.unlock();

		while(!NewRequests.empty())
		{
			auto pRequest = std::move(NewRequests.front());
			NewRequests.pop();

			dbg_msg("http", "task: %s", pRequest->m_aUrl);

			CURL *EH = curl_easy_init();
			if(!EH)
				goto bail; //TODO: Report the error

			if(!pRequest->ConfigureHandle(EH))
				goto bail; //TODO: Report the error

			curl_easy_setopt(EH, CURLOPT_PRIVATE, pRequest.get());

			// Linked list keeps the requests alive
			pRequest->m_pPrev = nullptr;
			pRequest->m_pNext = m_pRunningRequestsHead;
			if(m_pRunningRequestsHead)
				m_pRunningRequestsHead->m_pPrev = pRequest;
			m_pRunningRequestsHead = std::move(pRequest);

			mc = curl_multi_add_handle(MultiH, EH);
			if(mc != CURLM_OK)
				goto bail; //TODO: Report the error
		}
	}
bail:;

	std::lock_guard FinalL(m_Lock);
	m_Shutdown = true;

	while(!m_PendingRequests.empty())
	{
		auto pRequest = std::move(m_PendingRequests.front());
		m_PendingRequests.pop();

		// Emulate CURLE_ABORTED_BY_CALLBACK
		str_copy(pRequest->m_aErr, "Shutting down", sizeof(pRequest->m_aErr));
		pRequest->OnCompletionInternal(CURLE_ABORTED_BY_CALLBACK);
	}

	if(m_pRunningRequestsHead)
	{
		for(std::shared_ptr<CHttpRequest> pRequest = std::move(m_pRunningRequestsHead); pRequest;)
		{
			curl_easy_cleanup(pRequest->m_pHandle);
			pRequest->m_pPrev = nullptr;

			// Emulate CURLE_ABORTED_BY_CALLBACK
			str_copy(pRequest->m_aErr, "Shutting down", sizeof(pRequest->m_aErr));
			pRequest->OnCompletionInternal(CURLE_ABORTED_BY_CALLBACK);

			pRequest = std::move(pRequest->m_pNext);
		}
	}

	curl_multi_cleanup(MultiH);
	curl_global_cleanup();
}

void CHttpRunner::Shutdown()
{
	std::lock_guard l(m_Lock);
	m_Shutdown = true;
	WakeUp();
}

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

bool CHttpRequest::ConfigureHandle(CURL *pUser)
{
	if(!BeforeInit())
		return false;

	CURL *pHandle = m_pHandle = (CURL *)pUser;
	if(g_Config.m_DbgCurl)
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

	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.ConnectTimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_TIMEOUT_MS, m_Timeout.TimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.LowSpeedLimit);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, m_Timeout.LowSpeedTime);
	if(m_MaxResponseSize >= 0)
	{
		curl_easy_setopt(pHandle, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)m_MaxResponseSize);
	}

	curl_easy_setopt(pHandle, CURLOPT_PROTOCOLS, Protocols);
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

void CHttpRequest::OnCompletionInternal(unsigned int Result)
{
	CURLcode Code = static_cast<CURLcode>(Result);
	int State;
	if(Code != CURLE_OK)
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
			dbg_msg("http", "%s failed. libcurl error (%u): %s", m_aUrl, Result, m_aErr);
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
	SetStatus(IEngineRunnable::DONE);
}

size_t CHttpRequest::OnData(char *pData, size_t DataSize)
{
	// Need to check for the maximum response size here as curl can only
	// guarantee it if the server sets a Content-Length header.
	if(m_MaxResponseSize >= 0 && m_ResponseLength + DataSize > (uint64_t)m_MaxResponseSize)
	{
		return 0;
	}
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

void CHttpRequest::Result(unsigned char **ppResult, size_t *pResultLength) const
{
	if(m_WriteToFile || State() != HTTP_DONE)
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

bool HttpInit(IEngine *pEngine, IStorage *pStorage)
{
	if(gs_Runner.Init())
		return true;

	CHttpRunnable::m_sRunner = pEngine->RegisterRunner(&gs_Runner);

	return false;
}
