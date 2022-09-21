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
#endif

#define WIN32_LEAN_AND_MEAN
#include <curl/curl.h>

// TODO: Non-global pls?
static CURLSH *gs_pShare;
static LOCK gs_aLocks[CURL_LOCK_DATA_LAST + 1];
static bool gs_Initialized = false;

static int GetLockIndex(int Data)
{
	if(!(0 <= Data && Data < CURL_LOCK_DATA_LAST))
	{
		Data = CURL_LOCK_DATA_LAST;
	}
	return Data;
}

static void CurlLock(CURL *pHandle, curl_lock_data Data, curl_lock_access Access, void *pUser) ACQUIRE(gs_aLocks[GetLockIndex(Data)])
{
	(void)pHandle;
	(void)Access;
	(void)pUser;
	lock_wait(gs_aLocks[GetLockIndex(Data)]);
}

static void CurlUnlock(CURL *pHandle, curl_lock_data Data, void *pUser) RELEASE(gs_aLocks[GetLockIndex(Data)])
{
	(void)pHandle;
	(void)pUser;
	lock_unlock(gs_aLocks[GetLockIndex(Data)]);
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

bool HttpInit(IStorage *pStorage)
{
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		return true;
	}
	gs_pShare = curl_share_init();
	if(!gs_pShare)
	{
		return true;
	}
	// print curl version
	{
		curl_version_info_data *pVersion = curl_version_info(CURLVERSION_NOW);
		dbg_msg("http", "libcurl version %s (compiled = " LIBCURL_VERSION ")", pVersion->version);
	}

	for(auto &Lock : gs_aLocks)
	{
		Lock = lock_create();
	}
	curl_share_setopt(gs_pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(gs_pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	curl_share_setopt(gs_pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
	curl_share_setopt(gs_pShare, CURLSHOPT_LOCKFUNC, CurlLock);
	curl_share_setopt(gs_pShare, CURLSHOPT_UNLOCKFUNC, CurlUnlock);

#if !defined(CONF_FAMILY_WINDOWS)
	// As a multithreaded application we have to tell curl to not install signal
	// handlers and instead ignore SIGPIPE from OpenSSL ourselves.
	signal(SIGPIPE, SIG_IGN);
#endif

	gs_Initialized = true;

	return false;
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

void CHttpRequest::Run()
{
	dbg_assert(gs_Initialized, "must initialize HTTP before running HTTP requests");
	int FinalState;
	if(!BeforeInit())
	{
		FinalState = HTTP_ERROR;
	}
	else
	{
		CURL *pHandle = curl_easy_init();
		FinalState = RunImpl(pHandle);
		curl_easy_cleanup(pHandle);
	}

	m_State = OnCompletion(FinalState);
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

int CHttpRequest::RunImpl(CURL *pUser)
{
	CURL *pHandle = (CURL *)pUser;
	if(!pHandle)
	{
		return HTTP_ERROR;
	}

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
	char aErr[CURL_ERROR_SIZE];
	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, aErr);

	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.ConnectTimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_TIMEOUT_MS, m_Timeout.TimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.LowSpeedLimit);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, m_Timeout.LowSpeedTime);
	if(m_MaxResponseSize >= 0)
	{
		curl_easy_setopt(pHandle, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)m_MaxResponseSize);
	}

	curl_easy_setopt(pHandle, CURLOPT_SHARE, gs_pShare);
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

	if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
		dbg_msg("http", "fetching %s", m_aUrl);
	m_State = HTTP_RUNNING;
	int Ret = curl_easy_perform(pHandle);
	if(Ret != CURLE_OK)
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::FAILURE)
			dbg_msg("http", "%s failed. libcurl error (%d): %s", m_aUrl, (int)Ret, aErr);
		return (Ret == CURLE_ABORTED_BY_CALLBACK) ? HTTP_ABORTED : HTTP_ERROR;
	}
	else
	{
		if(g_Config.m_DbgCurl || m_LogProgress >= HTTPLOG::ALL)
			dbg_msg("http", "task done %s", m_aUrl);
		return HTTP_DONE;
	}
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

int CHttpRequest::OnCompletion(int State)
{
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
	return State;
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
