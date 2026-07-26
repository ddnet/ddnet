#include "http_emscripten.h"
#if defined(CONF_PLATFORM_EMSCRIPTEN)

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>
#include <base/thread.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

#include <limits>
#include <thread>

CHttpRequestEmscripten::CHttpRequestEmscripten(const char *pUrl) :
	IHttpRequest(pUrl)
{
}

CHttpRequestEmscripten::~CHttpRequestEmscripten()
{
	dbg_assert(m_pFetch == nullptr, "HTTP request fetch handle was not closed");
}

void CHttpRequestEmscripten::Header(const char *pNameColonValue)
{
	const char *pColon = str_find(pNameColonValue, ":");
	dbg_assert(pColon != nullptr, "Header name and value not separated with colon: '%s'", pNameColonValue);
	std::string Name = std::string(pNameColonValue, pColon - pNameColonValue);
	std::string Value = std::string(str_skip_whitespaces_const(pColon + 1));
	m_vRequestHeaders.emplace_back(std::move(Name), std::move(Value));
}

void CHttpRequestEmscripten::Abort()
{
	if(m_Abort)
	{
		return;
	}

	IHttpRequest::Abort();

	if(m_pFetch == nullptr)
	{
		return;
	}

	m_pHttp->AddPendingStateChange(m_pFetch, EHttpState::ABORTED);
}

EM_JS(void, FormatTimestampJsImpl, (char *pBuf, size_t Size, time_t Timestamp), {
	const timestampString = new Date(Number(Timestamp) * 1000).toUTCString();
	stringToUTF8(timestampString, pBuf, Size);
});

bool CHttpRequestEmscripten::ConfigureAndRun()
{
	if(!BeforeInit())
	{
		return false;
	}

	HeaderString("User-Agent", USER_AGENT_STRING);

	if(m_Type == REQUEST::POST_JSON)
	{
		Header("Content-Type: application/json");
	}
	else if(m_Type == REQUEST::POST)
	{
		Header("Content-Type:");
	}

	if(m_IfModifiedSince >= 0)
	{
		char aTimestamp[64];
		FormatTimestampJsImpl(aTimestamp, sizeof(aTimestamp), m_IfModifiedSince);
		HeaderString("If-Modified-Since", aTimestamp);
	}

	std::vector<const char *> vpPackedHeaders;
	vpPackedHeaders.reserve(2 * m_vRequestHeaders.size() + 1);
	for(const auto &[Name, Value] : m_vRequestHeaders)
	{
		vpPackedHeaders.push_back(Name.c_str());
		vpPackedHeaders.push_back(Value.c_str());
	}
	vpPackedHeaders.push_back(nullptr);

	emscripten_fetch_attr_t FetchAttributes;
	emscripten_fetch_attr_init(&FetchAttributes);
	str_copy(FetchAttributes.requestMethod, GetRequestType(m_Type));
	FetchAttributes.userData = this;
	FetchAttributes.onsuccess = FetchCallbackSuccess;
	FetchAttributes.onerror = FetchCallbackFailure;
	FetchAttributes.onprogress = FetchCallbackProgress;
	FetchAttributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	// Using low speed limit/time properties of timeout is not supported.
	FetchAttributes.timeoutMSecs = m_Timeout.m_ConnectTimeoutMs + m_Timeout.m_TimeoutMs;
	FetchAttributes.requestHeaders = vpPackedHeaders.data();
	FetchAttributes.requestData = reinterpret_cast<const char *>(m_pBody);
	FetchAttributes.requestDataSize = m_BodyLength;

	{
		std::unique_lock WaitLock(m_WaitMutex);
		m_State = EHttpState::RUNNING;
	}
	m_pFetch = emscripten_fetch(&FetchAttributes, m_aUrl);
	dbg_assert(m_pFetch != nullptr, "emscripten_fetch failure");

	return true;
}

void CHttpRequestEmscripten::OnSuccess()
{
	dbg_assert(!m_CallbackFinished, "OnSuccess was called multiple times");
	dbg_assert(m_pFetch != nullptr, "OnSuccess was called with an unset fetch handle");
	m_CallbackFinished = true;

	OnData(m_pFetch->data, m_pFetch->numBytes);

	m_pHttp->AddPendingStateChange(m_pFetch, EHttpState::DONE);
}

void CHttpRequestEmscripten::OnFailure()
{
	if(m_CallbackFinished || m_pFetch == nullptr)
	{
		// Failure callback may be called several times after the fetch handle has already been closed.
		return;
	}
	m_CallbackFinished = true;

	m_pHttp->AddPendingStateChange(m_pFetch, !m_FailOnErrorStatus && m_pFetch->status >= 400 ? EHttpState::DONE : EHttpState::ERROR);
}

void CHttpRequestEmscripten::OnProgress()
{
	m_Current.store(m_pFetch->dataOffset, std::memory_order_relaxed);
	m_Size.store(m_pFetch->totalBytes, std::memory_order_relaxed);
	m_Progress.store(m_pFetch->totalBytes == 0 ? 0 : (100 * m_pFetch->dataOffset) / m_pFetch->totalBytes, std::memory_order_relaxed);
	if(m_pProgressCallback != nullptr)
	{
		m_pProgressCallback->OnProgress();
	}
}

EM_JS(int64_t, ParseDateJsImpl, (const char *pStr), {
	const parsedDate = Date.parse(UTF8ToString(pStr));
	if(isNaN(parsedDate)) // Invalid dates are represented as NaN for some reason
	{
		return -1;
	}
	return BigInt(Math.floor(parsedDate / 1000));
});

void CHttpRequestEmscripten::OnHeader(const char *pName, const char *pValue)
{
	if(str_comp_nocase(pName, "Date") == 0)
	{
		const int64_t Value = ParseDateJsImpl(pValue);
		if(Value != -1)
		{
			m_ResultDate = Value;
		}
	}
	else if(str_comp_nocase(pName, "Last-Modified") == 0)
	{
		const int64_t Value = ParseDateJsImpl(pValue);
		if(Value != -1)
		{
			m_ResultLastModified = Value;
		}
	}
}

void CHttpRequestEmscripten::OnCompletionInternal(EHttpState State, const char *pErrorDetail)
{
	if(m_pFetch != nullptr)
	{
		const size_t HeadersLength = emscripten_fetch_get_response_headers_length(m_pFetch);
		if(HeadersLength != 0)
		{
			char *pHeaders = static_cast<char *>(malloc(HeadersLength + 1));
			dbg_assert(emscripten_fetch_get_response_headers(m_pFetch, pHeaders, HeadersLength + 1) == HeadersLength + 1, "emscripten_fetch_get_response_headers failure");
			char **ppUnpackedHeaders = emscripten_fetch_unpack_response_headers(pHeaders);

			int HeaderIndex = 0;
			while(ppUnpackedHeaders[HeaderIndex] != nullptr)
			{
				const char *pName = ppUnpackedHeaders[HeaderIndex];
				++HeaderIndex;
				const char *pValue = ppUnpackedHeaders[HeaderIndex];
				++HeaderIndex;
				dbg_assert(pValue != nullptr, "emscripten_fetch_unpack_response_headers result unexpected: value is nullptr for header '%s'", pName);
				OnHeader(pName, pValue);
			}

			emscripten_fetch_free_unpacked_response_headers(ppUnpackedHeaders);
			free(pHeaders);
		}

		m_StatusCode = m_pFetch->status;
	}

	if(State == EHttpState::DONE)
	{
		if(g_Config.m_DbgHttp || m_LogProgress >= HTTPLOG::ALL)
		{
			log_info("http", "task done: %s", m_aUrl);
		}
	}
	else
	{
		if(g_Config.m_DbgHttp || m_LogProgress >= HTTPLOG::FAILURE)
		{
			const char *pError =
				pErrorDetail != nullptr                                  ? pErrorDetail :
				State == EHttpState::ABORTED                             ? "Request aborted" :
				(m_pFetch != nullptr && m_pFetch->statusText[0] != '\0') ? m_pFetch->statusText :
											   "Unknown (check the web browser console)";
			log_error("http", "%s failed. fetch error: %s", m_aUrl, pError);
		}
	}

	if(m_pFetch != nullptr)
	{
		// Unset member variable before closing handle so completion callbacks are not called again.
		emscripten_fetch_t *pFetch = m_pFetch;
		m_pFetch = nullptr;
		dbg_assert(emscripten_fetch_close(pFetch) == EMSCRIPTEN_RESULT_SUCCESS, "emscripten_fetch_close failure");
	}

	IHttpRequest::OnCompletionInternal(State);
}

void CHttpRequestEmscripten::FetchCallbackSuccess(emscripten_fetch_t *pFetch)
{
	static_cast<CHttpRequestEmscripten *>(pFetch->userData)->OnSuccess();
}

void CHttpRequestEmscripten::FetchCallbackFailure(emscripten_fetch_t *pFetch)
{
	static_cast<CHttpRequestEmscripten *>(pFetch->userData)->OnFailure();
}

void CHttpRequestEmscripten::FetchCallbackProgress(emscripten_fetch_t *pFetch)
{
	static_cast<CHttpRequestEmscripten *>(pFetch->userData)->OnProgress();
}

std::unique_ptr<IHttpRequest> CreateHttpRequest(const char *pUrl)
{
	return std::make_unique<CHttpRequestEmscripten>(pUrl);
}

bool CHttpEmscripten::Init(std::chrono::milliseconds ShutdownDelay)
{
	m_ShutdownDelay = ShutdownDelay;

	m_pThread = thread_init(CHttpEmscripten::ThreadMain, this, "http");

	std::unique_lock Lock(m_Lock);
	m_ConditionVariableInit.wait(Lock, [this]() { return m_Initialized.load(std::memory_order_seq_cst); });

	return true;
}

void CHttpEmscripten::Shutdown()
{
	{
		std::unique_lock Lock(m_Lock);
		if(m_Shutdown || !m_Initialized)
		{
			return;
		}
		m_Shutdown = true;
	}
	m_ConditionVariableLoop.notify_all();
}

CHttpEmscripten::~CHttpEmscripten()
{
	if(!m_pThread)
	{
		return;
	}

	Shutdown();
	thread_wait(m_pThread);
}

void CHttpEmscripten::Run(std::shared_ptr<IHttpRequest> pRequest)
{
	{
		std::unique_lock Lock(m_Lock);
		dbg_assert(m_Initialized, "HTTP not initialized");
		std::shared_ptr<CHttpRequestEmscripten> pRequestImpl = std::static_pointer_cast<CHttpRequestEmscripten>(pRequest);
		pRequestImpl->m_pHttp = this;
		if(m_Shutdown)
		{
			pRequestImpl->OnCompletionInternal(EHttpState::ABORTED, "Shutting down");
			return;
		}
		m_PendingRequests.emplace_back(pRequestImpl);
	}
	m_ConditionVariableLoop.notify_all();
}

EM_JS(void, EscapeUrlJsImpl, (char *pBuf, size_t Size, const char *pStr), {
	const escapedString = encodeURIComponent(UTF8ToString(pStr));
	stringToUTF8(escapedString, pBuf, Size);
});

void EscapeUrl(char *pBuf, size_t Size, const char *pStr)
{
	EscapeUrlJsImpl(pBuf, Size, pStr);
}

bool CHttpEmscripten::HasIpresolveBug() const
{
	return false;
}

void CHttpEmscripten::ThreadMain(void *pUser)
{
	static_cast<CHttpEmscripten *>(pUser)->RunLoop();
}

void CHttpEmscripten::RunLoop()
{
	{
		std::unique_lock Lock(m_Lock);
		m_Initialized = true;
	}
	m_ConditionVariableInit.notify_all();

	while(true)
	{
		{
			std::unique_lock Lock(m_Lock);
			if(m_Shutdown)
			{
				if(m_RunningRequests.empty() && m_PendingRequests.empty())
					break;

				const auto Now = std::chrono::steady_clock::now();
				if(!m_ShutdownTime.has_value())
				{
					m_ShutdownTime = Now + m_ShutdownDelay;
				}
				else if(m_ShutdownTime < Now)
				{
					if(m_StartedShutdown)
					{
						break;
					}
					else
					{
						for(auto &[_, pRequest] : m_RunningRequests)
						{
							auto [ExistingElement, Inserted] = m_PendingFetchChanges.emplace(pRequest->m_pFetch, EHttpState::ABORTED);
							if(!Inserted)
							{
								ExistingElement->second = EHttpState::ABORTED;
							}
						}
						m_StartedShutdown = true;
						m_ShutdownTime = Now + m_ShutdownDelay;
					}
				}
			}
		}

		decltype(m_PendingRequests) PendingRequests = {};
		decltype(m_PendingFetchChanges) PendingFetchChanges = {};
		{
			std::unique_lock Lock(m_Lock);
			std::swap(m_PendingRequests, PendingRequests);
			std::swap(m_PendingFetchChanges, PendingFetchChanges);
		}

		while(!PendingRequests.empty())
		{
			auto &pRequest = PendingRequests.front();
			if(g_Config.m_DbgHttp)
			{
				log_debug("http", "task: %s %s", CHttpRequestEmscripten::GetRequestType(pRequest->m_Type), pRequest->m_aUrl);
			}

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
				PendingRequests.pop_front();
				continue;
			}

			if(m_StartedShutdown || pRequest->IsAbortRequested())
			{
				pRequest->OnCompletionInternal(EHttpState::ABORTED, m_StartedShutdown ? "Shutting down" : "Request aborted");
				PendingRequests.pop_front();
				continue;
			}

			if(!pRequest->ConfigureAndRun())
			{
				pRequest->OnCompletionInternal(EHttpState::ABORTED, "Failed to initialize request");
				PendingRequests.pop_front();
				continue;
			}

			{
				emscripten_fetch_t *pFetch = pRequest->m_pFetch;
				auto [_, Inserted] = m_RunningRequests.emplace(pFetch, std::move(pRequest));
				dbg_assert(Inserted, "Request with same fetch handle already running");
			}
			PendingRequests.pop_front();
		}

		for(const auto &[pFetch, NewState] : PendingFetchChanges)
		{
			auto pRequest = m_RunningRequests.find(pFetch);
			if(pRequest == m_RunningRequests.end())
			{
				// Requests can be aborted even if they are not in m_RunningRequests anymore.
				// We only hold the lock to swap the pending fetch changes above, so another
				// pending state change to abort a request can be added while the HTTP thread
				// is removing the running request in the branch below.
				dbg_assert(NewState == EHttpState::ABORTED, "Request for pending fetch state change not found");
			}
			else
			{
				pRequest->second->OnCompletionInternal(NewState, nullptr);
				m_RunningRequests.erase(pRequest);
			}
		}
		PendingFetchChanges.clear();

		// Return control to the browser so the created fetch handles are serviced.
		// This will cause the success, failure and progress callbacks to be called.
		emscripten_sleep(0);

		// Wait a bit for state changes, but also wake up periodically because we
		// need to call emscripten_sleep to service the handles.
		std::unique_lock Lock(m_Lock);
		const auto &&WaitPredicate = [this]() { return m_Shutdown || !m_PendingRequests.empty() || !m_PendingFetchChanges.empty(); };
		const auto WaitTime = std::chrono::milliseconds(100);
		const auto Now = std::chrono::steady_clock::now();
		if(m_ShutdownTime.has_value() && m_ShutdownTime.value() - Now < WaitTime)
		{
			m_ConditionVariableLoop.wait_until(Lock, m_ShutdownTime.value(), WaitPredicate);
		}
		else
		{
			m_ConditionVariableLoop.wait_for(Lock, WaitTime, WaitPredicate);
		}
	}

	std::unique_lock Lock(m_Lock);
	for(auto &pRequest : m_PendingRequests)
	{
		pRequest->OnCompletionInternal(EHttpState::ABORTED, "Shutting down");
	}
	m_PendingRequests.clear();

	for(auto &[_, pRequest] : m_RunningRequests)
	{
		pRequest->OnCompletionInternal(EHttpState::ABORTED, "Shutting down");
	}
	m_RunningRequests.clear();
}

void CHttpEmscripten::AddPendingStateChange(emscripten_fetch_t *pFetch, EHttpState State)
{
	{
		std::unique_lock Lock(m_Lock);
		auto [ExistingElement, Inserted] = m_PendingFetchChanges.emplace(pFetch, State);
		if(!Inserted && ExistingElement->second != EHttpState::ABORTED)
		{
			ExistingElement->second = State;
		}
	}
	m_ConditionVariableLoop.notify_all();
}

IEngineHttp *CreateEngineHttp()
{
	return new CHttpEmscripten;
}

#endif // CONF_PLATFORM_EMSCRIPTEN
