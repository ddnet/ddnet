#include "http.h"

#include <base/system.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/version.h>

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"
#include "curl/easy.h"

// TODO: Non-global pls?
static CURLSH *gs_Share;
static LOCK gs_aLocks[CURL_LOCK_DATA_LAST + 1];

static int GetLockIndex(int Data)
{
	if(!(0 <= Data && Data < CURL_LOCK_DATA_LAST))
	{
		Data = CURL_LOCK_DATA_LAST;
	}
	return Data;
}

static void CurlLock(CURL *pHandle, curl_lock_data Data, curl_lock_access Access, void *pUser)
{
	(void)pHandle;
	(void)Access;
	(void)pUser;
	lock_wait(gs_aLocks[GetLockIndex(Data)]);
}

static void CurlUnlock(CURL *pHandle, curl_lock_data Data, void *pUser)
{
	(void)pHandle;
	(void)pUser;
	lock_unlock(gs_aLocks[GetLockIndex(Data)]);
}

bool HttpInit(IStorage *pStorage)
{
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		return true;
	}
	gs_Share = curl_share_init();
	if(!gs_Share)
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
	curl_share_setopt(gs_Share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(gs_Share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	curl_share_setopt(gs_Share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
	curl_share_setopt(gs_Share, CURLSHOPT_LOCKFUNC, CurlLock);
	curl_share_setopt(gs_Share, CURLSHOPT_UNLOCKFUNC, CurlUnlock);
	return false;
}

void EscapeUrl(char *pBuf, int Size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, Size);
	curl_free(pEsc);
}

CRequest::CRequest(const char *pUrl, CTimeout Timeout, bool LogProgress) :
	m_Timeout(Timeout),
	m_Size(0),
	m_Progress(0),
	m_LogProgress(LogProgress),
	m_State(HTTP_QUEUED),
	m_Abort(false)
{
	str_copy(m_aUrl, pUrl, sizeof(m_aUrl));
}

void CRequest::Run()
{
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

int CRequest::RunImpl(CURL *pHandle)
{
	if(!pHandle)
	{
		return HTTP_ERROR;
	}

	if(g_Config.m_DbgCurl)
	{
		curl_easy_setopt(pHandle, CURLOPT_VERBOSE, 1L);
	}
	char aErr[CURL_ERROR_SIZE];
	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, aErr);

	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, m_Timeout.ConnectTimeoutMs);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, m_Timeout.LowSpeedLimit);
	curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, m_Timeout.LowSpeedTime);

	curl_easy_setopt(pHandle, CURLOPT_SHARE, gs_Share);
	curl_easy_setopt(pHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pHandle, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");

	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);

	if(!AfterInit(pHandle))
	{
		return HTTP_ERROR;
	}

	if(g_Config.m_DbgCurl || m_LogProgress)
		dbg_msg("http", "http %s", m_aUrl);
	m_State = HTTP_RUNNING;
	int Ret = curl_easy_perform(pHandle);
	if(Ret != CURLE_OK)
	{
		if(g_Config.m_DbgCurl || m_LogProgress)
			dbg_msg("http", "task failed. libcurl error: %s", aErr);
		return (Ret == CURLE_ABORTED_BY_CALLBACK) ? HTTP_ABORTED : HTTP_ERROR;
	}
	else
	{
		if(g_Config.m_DbgCurl || m_LogProgress)
			dbg_msg("http", "task done %s", m_aUrl);
		return HTTP_DONE;
	}
}

size_t CRequest::WriteCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	return ((CRequest *)pUser)->OnData(pData, Size * Number);
}

int CRequest::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CGetFile *pTask = (CGetFile *)pUser;
	pTask->m_Current = DlCurr;
	pTask->m_Size = DlTotal;
	pTask->m_Progress = (100 * DlCurr) / (DlTotal ? DlTotal : 1);
	pTask->OnProgress();
	return pTask->m_Abort ? -1 : 0;
}

CGet::CGet(const char *pUrl, CTimeout Timeout) :
	CRequest(pUrl, Timeout),
	m_BufferSize(0),
	m_BufferLength(0),
	m_pBuffer(NULL)
{
}

CGet::~CGet()
{
	m_BufferSize = 0;
	m_BufferLength = 0;
	free(m_pBuffer);
	m_pBuffer = NULL;
}

unsigned char *CGet::Result() const
{
	if(State() != HTTP_DONE)
	{
		return NULL;
	}
	return m_pBuffer;
}

unsigned char *CGet::TakeResult()
{
	unsigned char *pResult = Result();
	if(pResult)
	{
		m_BufferSize = 0;
		m_BufferLength = 0;
		m_pBuffer = NULL;
	}
	return pResult;
}

json_value *CGet::ResultJson() const
{
	unsigned char *pResult = Result();
	if(!pResult)
	{
		return NULL;
	}
	return json_parse((char *)pResult, m_BufferLength);
}

size_t CGet::OnData(char *pData, size_t DataSize)
{
	if(DataSize == 0)
	{
		return DataSize;
	}
	bool Reallocate = false;
	if(m_BufferSize == 0)
	{
		m_BufferSize = 1024;
		Reallocate = true;
	}
	while(m_BufferLength + DataSize > m_BufferSize)
	{
		m_BufferSize *= 2;
		Reallocate = true;
	}
	if(Reallocate)
	{
		m_pBuffer = (unsigned char *)realloc(m_pBuffer, m_BufferSize);
	}
	mem_copy(m_pBuffer + m_BufferLength, pData, DataSize);
	m_BufferLength += DataSize;
	return DataSize;
}

CGetFile::CGetFile(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType, CTimeout Timeout, bool LogProgress) :
	CRequest(pUrl, Timeout, LogProgress),
	m_pStorage(pStorage),
	m_File(0),
	m_StorageType(StorageType)
{
	str_copy(m_aDest, pDest, sizeof(m_aDest));

	if(m_StorageType == -2)
		m_pStorage->GetBinaryPath(m_aDest, m_aDestFull, sizeof(m_aDestFull));
	else
		m_pStorage->GetCompletePath(m_StorageType, m_aDest, m_aDestFull, sizeof(m_aDestFull));
}

bool CGetFile::BeforeInit()
{
	if(fs_makedir_rec_for(m_aDestFull) < 0)
	{
		dbg_msg("http", "i/o error, cannot create folder for: %s", m_aDestFull);
		return false;
	}

	m_File = io_open(m_aDestFull, IOFLAG_WRITE);
	if(!m_File)
	{
		dbg_msg("http", "i/o error, cannot open file: %s", m_aDest);
		return false;
	}
	return true;
}

size_t CGetFile::OnData(char *pData, size_t DataSize)
{
	return io_write(m_File, pData, DataSize);
}

int CGetFile::OnCompletion(int State)
{
	if(m_File && io_close(m_File) != 0)
	{
		dbg_msg("http", "i/o error, cannot close file: %s", m_aDest);
		State = HTTP_ERROR;
	}

	if(State == HTTP_ERROR || State == HTTP_ABORTED)
	{
		m_pStorage->RemoveFile(m_aDestFull, IStorage::TYPE_ABSOLUTE);
	}

	return State;
}

CPostJson::CPostJson(const char *pUrl, CTimeout Timeout, const char *pJson) :
	CRequest(pUrl, Timeout)
{
	str_copy(m_aJson, pJson, sizeof(m_aJson));
}

bool CPostJson::AfterInit(void *pCurl)
{
	CURL *pHandle = pCurl;

	curl_slist *pHeaders = NULL;
	pHeaders = curl_slist_append(pHeaders, "Content-Type: application/json");
	curl_easy_setopt(pHandle, CURLOPT_HTTPHEADER, pHeaders);
	curl_easy_setopt(pHandle, CURLOPT_POSTFIELDS, m_aJson);

	return true;
}
