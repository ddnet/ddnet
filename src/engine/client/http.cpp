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

static char CA_FILE_PATH[512];
// TODO: Non-global pls?
static CURLSH *gs_Share;
static LOCK gs_aLocks[CURL_LOCK_DATA_LAST+1];

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
	pStorage->GetBinaryPath("data/ca-ddnet.pem", CA_FILE_PATH, sizeof(CA_FILE_PATH));
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
	{
		return true;
	}
	gs_Share = curl_share_init();
	if(!gs_Share)
	{
		return true;
	}
	for(unsigned int i = 0; i < sizeof(gs_aLocks) / sizeof(gs_aLocks[0]); i++)
	{
		gs_aLocks[i] = lock_create();
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

CRequest::CRequest(const char *pUrl, bool CanTimeout) :
	m_CanTimeout(CanTimeout),
	m_Size(0),
	m_Progress(0),
	m_State(HTTP_QUEUED),
	m_Abort(false)
{
	str_copy(m_aUrl, pUrl, sizeof(m_aUrl));
}

void CRequest::Run()
{
	if(!BeforeInit())
	{
		m_State = HTTP_ERROR;
		OnCompletion();
		return;
	}

	CURL *pHandle = curl_easy_init();
	m_State = RunImpl(pHandle);
	curl_easy_cleanup(pHandle);

	OnCompletion();
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

	if(m_CanTimeout)
	{
		curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, (long)g_Config.m_ClHTTPConnectTimeoutMs);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, (long)g_Config.m_ClHTTPLowSpeedLimit);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, (long)g_Config.m_ClHTTPLowSpeedTime);
	}
	else
	{
		curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, 0L);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, 0L);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, 0L);
	}
	curl_easy_setopt(pHandle, CURLOPT_SHARE, gs_Share);
	curl_easy_setopt(pHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pHandle, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, "DDNet " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");

	// We only trust our own custom-selected CAs for our own servers.
	// Other servers can use any CA trusted by the system.
	if(false
		|| str_comp_nocase_num("maps.ddnet.tw/", m_aUrl, 14) == 0
		|| str_comp_nocase_num("http://maps.ddnet.tw/", m_aUrl, 21) == 0
		|| str_comp_nocase_num("https://maps.ddnet.tw/", m_aUrl, 22) == 0
		|| str_comp_nocase_num("http://info.ddnet.tw/", m_aUrl, 21) == 0
		|| str_comp_nocase_num("https://info.ddnet.tw/", m_aUrl, 22) == 0
		|| str_comp_nocase_num("https://update4.ddnet.tw/", m_aUrl, 25) == 0)
	{
		curl_easy_setopt(pHandle, CURLOPT_CAINFO, CA_FILE_PATH);
	}
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);

	if(!AfterInit(pHandle))
	{
		return HTTP_ERROR;
	}

	dbg_msg("http", "http %s", m_aUrl);
	m_State = HTTP_RUNNING;
	int Ret = curl_easy_perform(pHandle);
	if(!BeforeCompletion())
	{
		return HTTP_ERROR;
	}
	if(Ret != CURLE_OK)
	{
		dbg_msg("http", "task failed. libcurl error: %s", aErr);
		return (Ret == CURLE_ABORTED_BY_CALLBACK) ? HTTP_ABORTED : HTTP_ERROR;
	}
	else
	{
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

CGet::CGet(const char *pUrl, bool CanTimeout) :
	CRequest(pUrl, CanTimeout),
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

CGetFile::CGetFile(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType, bool CanTimeout) :
	CRequest(pUrl, CanTimeout),
	m_pStorage(pStorage),
	m_StorageType(StorageType)
{
	str_copy(m_aDest, pDest, sizeof(m_aDest));
}

bool CGetFile::BeforeInit()
{
	char aPath[512];
	if(m_StorageType == -2)
		m_pStorage->GetBinaryPath(m_aDest, aPath, sizeof(aPath));
	else
		m_pStorage->GetCompletePath(m_StorageType, m_aDest, aPath, sizeof(aPath));

	if(fs_makedir_rec_for(aPath) < 0)
	{
		dbg_msg("http", "i/o error, cannot create folder for: %s", aPath);
		return false;
	}

	m_File = io_open(aPath, IOFLAG_WRITE);
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

bool CGetFile::BeforeCompletion()
{
	return io_close(m_File) == 0;
}

CPostJson::CPostJson(const char *pUrl, bool CanTimeout, const char *pJson)
	: CRequest(pUrl, CanTimeout)
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
