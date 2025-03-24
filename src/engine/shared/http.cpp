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
	char *pEsc = curl_easy_escape(nullptr, pStr, 0);
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
	sha256_init(&m_ActualSha256Ctx);
}

CHttpRequest::~CHttpRequest()
{
	dbg_assert(m_File == nullptr, "HTTP request file was not closed");
	free(m_pBuffer);
	curl_slist_free_all((curl_slist *)m_pHeaders);
	free(m_pBody);
	if(m_State == EHttpState::DONE && m_ValidateBeforeOverwrite)
	{
		OnValidation(false);
	}
}

static bool CalculateSha256(const char *pAbsoluteFilename, SHA256_DIGEST *pSha256)
{
	IOHANDLE File = io_open(pAbsoluteFilename, IOFLAG_READ);
	if(!File)
	{
		return false;
	}
	SHA256_CTX Sha256Ctxt;
	sha256_init(&Sha256Ctxt);
	unsigned char aBuffer[64 * 1024];
	while(true)
	{
		unsigned Bytes = io_read(File, aBuffer, sizeof(aBuffer));
		if(Bytes == 0)
			break;
		sha256_update(&Sha256Ctxt, aBuffer, Bytes);
	}
	io_close(File);
	*pSha256 = sha256_finish(&Sha256Ctxt);
	return true;
}

bool CHttpRequest::ShouldSkipRequest()
{
	if(m_WriteToFile && m_ExpectedSha256 != SHA256_ZEROED)
	{
		SHA256_DIGEST Sha256;
		if(CalculateSha256(m_aDestAbsolute, &Sha256) && Sha256 == m_ExpectedSha256)
		{
			log_debug("http", "skipping download because expected file already exists: %s", m_aDest);
			return true;
		}
	}
	return false;
}

bool CHttpRequest::BeforeInit()
{
	if(m_WriteToFile)
	{
		if(m_SkipByFileTime)
		{
			time_t FileCreatedTime, FileModifiedTime;
			if(fs_file_time(m_aDestAbsolute, &FileCreatedTime, &FileModifiedTime) == 0)
			{
				m_IfModifiedSince = FileModifiedTime;
			}
		}

		if(fs_makedir_rec_for(m_aDestAbsoluteTmp) < 0)
		{
			log_error("http", "i/o error, cannot create folder for: %s", m_aDest);
			return false;
		}

		m_File = io_open(m_aDestAbsoluteTmp, IOFLAG_WRITE);
		if(!m_File)
		{
			log_error("http", "i/o error, cannot open file: %s", m_aDest);
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
	if(m_IfModifiedSince >= 0)
	{
		curl_easy_setopt(pH, CURLOPT_TIMEVALUE_LARGE, (curl_off_t)m_IfModifiedSince);
		curl_easy_setopt(pH, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
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
	if(m_FailOnErrorStatus)
	{
		curl_easy_setopt(pH, CURLOPT_FAILONERROR, 1L);
	}
	curl_easy_setopt(pH, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pH, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pH, CURLOPT_USERAGENT, GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");
	curl_easy_setopt(pH, CURLOPT_ACCEPT_ENCODING, ""); // Use any compression algorithm supported by libcurl.

	curl_easy_setopt(pH, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(pH, CURLOPT_HEADERFUNCTION, HeaderCallback);
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
	curl_easy_setopt(pH, CURLOPT_CAINFO, "data/cacert.pem");
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

size_t CHttpRequest::OnHeader(char *pHeader, size_t HeaderSize)
{
	// `pHeader` is NOT null-terminated.
	// `pHeader` has a trailing newline.

	if(HeaderSize <= 1)
	{
		m_HeadersEnded = true;
		return HeaderSize;
	}
	if(m_HeadersEnded)
	{
		// redirect, clear old headers
		m_HeadersEnded = false;
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

size_t CHttpRequest::OnData(char *pData, size_t DataSize)
{
	// Need to check for the maximum response size here as curl can only
	// guarantee it if the server sets a Content-Length header.
	if(m_MaxResponseSize >= 0 && m_ResponseLength + DataSize > (uint64_t)m_MaxResponseSize)
	{
		return 0;
	}

	if(DataSize == 0)
	{
		return DataSize;
	}

	sha256_update(&m_ActualSha256Ctx, pData, DataSize);

	size_t Result = DataSize;

	if(m_WriteToMemory)
	{
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
	}
	if(m_WriteToFile)
	{
		Result = io_write(m_File, pData, DataSize);
	}
	m_ResponseLength += DataSize;
	return Result;
}

size_t CHttpRequest::HeaderCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	dbg_assert(Size == 1, "invalid size parameter passed to header callback");
	return ((CHttpRequest *)pUser)->OnHeader(pData, Number);
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
	pTask->m_Progress.store(DlTotal == 0.0 ? 0 : (100 * DlCurr) / DlTotal, std::memory_order_relaxed);
	pTask->OnProgress();
	return pTask->m_Abort ? -1 : 0;
}

void CHttpRequest::OnCompletionInternal(void *pHandle, unsigned int Result)
{
	if(pHandle)
	{
		CURL *pH = (CURL *)pHandle;
		long StatusCode;
		curl_easy_getinfo(pH, CURLINFO_RESPONSE_CODE, &StatusCode);
		m_StatusCode = StatusCode;
	}

	EHttpState State;
	const CURLcode Code = static_cast<CURLcode>(Result);
	if(Code != CURLE_OK)
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
		{
			log_error("http", "%s failed. libcurl error (%u): %s", m_aUrl, Code, m_aErr);
		}
		State = (Code == CURLE_ABORTED_BY_CALLBACK) ? EHttpState::ABORTED : EHttpState::ERROR;
	}
	else
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
		{
			log_info("http", "task done: %s", m_aUrl);
		}
		State = EHttpState::DONE;
	}

	if(State == EHttpState::DONE)
	{
		m_ActualSha256 = sha256_finish(&m_ActualSha256Ctx);
		if(m_ExpectedSha256 != SHA256_ZEROED && m_ActualSha256 != m_ExpectedSha256)
		{
			if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
			{
				char aActualSha256[SHA256_MAXSTRSIZE];
				sha256_str(m_ActualSha256, aActualSha256, sizeof(aActualSha256));
				char aExpectedSha256[SHA256_MAXSTRSIZE];
				sha256_str(m_ExpectedSha256, aExpectedSha256, sizeof(aExpectedSha256));
				log_error("http", "SHA256 mismatch: got=%s, expected=%s, url=%s", aActualSha256, aExpectedSha256, m_aUrl);
			}
			State = EHttpState::ERROR;
		}
	}

	if(m_WriteToFile)
	{
		if(m_File && io_close(m_File) != 0)
		{
			log_error("http", "i/o error, cannot close file: %s", m_aDest);
			State = EHttpState::ERROR;
		}
		m_File = nullptr;

		if(State == EHttpState::ERROR || State == EHttpState::ABORTED)
		{
			fs_remove(m_aDestAbsoluteTmp);
		}
		else if(m_IfModifiedSince >= 0 && m_StatusCode == 304) // 304 Not Modified
		{
			fs_remove(m_aDestAbsoluteTmp);
			if(m_WriteToMemory)
			{
				free(m_pBuffer);
				m_pBuffer = nullptr;
				m_ResponseLength = 0;
				void *pBuffer;
				unsigned Length;
				IOHANDLE File = io_open(m_aDestAbsolute, IOFLAG_READ);
				bool Success = File && io_read_all(File, &pBuffer, &Length);
				if(File)
				{
					io_close(File);
				}
				if(Success)
				{
					m_pBuffer = (unsigned char *)pBuffer;
					m_ResponseLength = Length;
				}
				else
				{
					log_error("http", "i/o error, cannot read existing file: %s", m_aDest);
					State = EHttpState::ERROR;
				}
			}
		}
		else if(!m_ValidateBeforeOverwrite)
		{
			if(fs_rename(m_aDestAbsoluteTmp, m_aDestAbsolute))
			{
				log_error("http", "i/o error, cannot move file: %s", m_aDest);
				State = EHttpState::ERROR;
				fs_remove(m_aDestAbsoluteTmp);
			}
		}
	}

	// The globally visible state must be updated after OnCompletion has finished,
	// or other threads may try to access the result of a completed HTTP request,
	// before the result has been initialized/updated in OnCompletion.
	OnCompletion(State);
	{
		std::unique_lock WaitLock(m_WaitMutex);
		m_State = State;
	}
	m_WaitCondition.notify_all();
}

void CHttpRequest::OnValidation(bool Success)
{
	dbg_assert(m_ValidateBeforeOverwrite, "this function is illegal to call without having set ValidateBeforeOverwrite");
	m_ValidateBeforeOverwrite = false;
	if(Success)
	{
		if(m_IfModifiedSince >= 0 && m_StatusCode == 304) // 304 Not Modified
		{
			fs_remove(m_aDestAbsoluteTmp);
			return;
		}
		if(fs_rename(m_aDestAbsoluteTmp, m_aDestAbsolute))
		{
			log_error("http", "i/o error, cannot move file: %s", m_aDest);
			m_State = EHttpState::ERROR;
			fs_remove(m_aDestAbsoluteTmp);
		}
	}
	else
	{
		m_State = EHttpState::ERROR;
		fs_remove(m_aDestAbsoluteTmp);
	}
}

void CHttpRequest::WriteToFile(IStorage *pStorage, const char *pDest, int StorageType)
{
	m_WriteToMemory = false;
	m_WriteToFile = true;
	str_copy(m_aDest, pDest);
	m_StorageType = StorageType;
	if(StorageType == -2)
	{
		pStorage->GetBinaryPath(m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
	else
	{
		pStorage->GetCompletePath(StorageType, m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
	IStorage::FormatTmpPath(m_aDestAbsoluteTmp, sizeof(m_aDestAbsoluteTmp), m_aDestAbsolute);
}

void CHttpRequest::WriteToFileAndMemory(IStorage *pStorage, const char *pDest, int StorageType)
{
	WriteToFile(pStorage, pDest, StorageType);
	m_WriteToMemory = true;
}

void CHttpRequest::Header(const char *pNameColonValue)
{
	m_pHeaders = curl_slist_append((curl_slist *)m_pHeaders, pNameColonValue);
}

void CHttpRequest::Wait()
{
	std::unique_lock Lock(m_WaitMutex);
	m_WaitCondition.wait(Lock, [this]() {
		EHttpState State = m_State.load(std::memory_order_seq_cst);
		return State != EHttpState::QUEUED && State != EHttpState::RUNNING;
	});
}

void CHttpRequest::Result(unsigned char **ppResult, size_t *pResultLength) const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	dbg_assert(m_WriteToMemory, "Result only usable when written to memory");
	*ppResult = m_pBuffer;
	*pResultLength = m_ResponseLength;
}

json_value *CHttpRequest::ResultJson() const
{
	unsigned char *pResult;
	size_t ResultLength;
	Result(&pResult, &ResultLength);
	return json_parse((char *)pResult, ResultLength);
}

const SHA256_DIGEST &CHttpRequest::ResultSha256() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	return m_ActualSha256;
}

int CHttpRequest::StatusCode() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	return m_StatusCode;
}

std::optional<int64_t> CHttpRequest::ResultAgeSeconds() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	if(!m_ResultDate || !m_ResultLastModified)
	{
		return {};
	}
	return *m_ResultDate - *m_ResultLastModified;
}

std::optional<int64_t> CHttpRequest::ResultLastModified() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	return m_ResultLastModified;
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
		log_error("http", "curl_global_init failed");
		m_State = CHttp::ERROR;
		m_Cv.notify_all();
		return;
	}

	m_pMultiH = curl_multi_init();
	if(!m_pMultiH)
	{
		log_error("http", "curl_multi_init failed");
		m_State = CHttp::ERROR;
		m_Cv.notify_all();
		return;
	}

	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		log_info("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	m_State = CHttp::RUNNING;
	m_Cv.notify_all();
	Lock.unlock();

	while(m_State == CHttp::RUNNING)
	{
		static int s_NextTimeout = std::numeric_limits<int>::max();
		int Events = 0;
		const CURLMcode PollCode = curl_multi_poll(m_pMultiH, nullptr, 0, s_NextTimeout, &Events);

		// We may have been woken up for a shutdown
		if(m_Shutdown)
		{
			auto Now = std::chrono::steady_clock::now();
			if(!m_ShutdownTime.has_value())
			{
				m_ShutdownTime = Now + m_ShutdownDelay;
				s_NextTimeout = m_ShutdownDelay.count();
			}
			else if(m_ShutdownTime < Now || m_RunningRequests.empty())
			{
				break;
			}
		}

		if(PollCode != CURLM_OK)
		{
			Lock.lock();
			log_error("http", "curl_multi_poll failed: %s", curl_multi_strerror(PollCode));
			m_State = CHttp::ERROR;
			break;
		}

		const CURLMcode PerformCode = curl_multi_perform(m_pMultiH, &Events);
		if(PerformCode != CURLM_OK)
		{
			Lock.lock();
			log_error("http", "curl_multi_perform failed: %s", curl_multi_strerror(PerformCode));
			m_State = CHttp::ERROR;
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
			if(g_Config.m_DbgCurl)
				log_debug("http", "task: %s %s", CHttpRequest::GetRequestType(pRequest->m_Type), pRequest->m_aUrl);

			if(pRequest->ShouldSkipRequest())
			{
				pRequest->OnCompletion(EHttpState::DONE);
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
		pRequest->OnCompletionInternal(nullptr, CURLE_ABORTED_BY_CALLBACK);
	}

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

	if(Cleanup)
	{
		curl_multi_cleanup(m_pMultiH);
		curl_global_cleanup();
	}
}

void CHttp::Run(std::shared_ptr<IHttpRequest> pRequest)
{
	std::shared_ptr<CHttpRequest> pRequestImpl = std::static_pointer_cast<CHttpRequest>(pRequest);
	std::unique_lock Lock(m_Lock);
	if(m_Shutdown || m_State == CHttp::ERROR)
	{
		str_copy(pRequestImpl->m_aErr, "Shutting down");
		pRequestImpl->OnCompletionInternal(nullptr, CURLE_ABORTED_BY_CALLBACK);
		return;
	}
	m_Cv.wait(Lock, [this]() { return m_State != CHttp::UNINITIALIZED; });
	m_PendingRequests.emplace_back(pRequestImpl);
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
