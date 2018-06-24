#include <base/system.h>
#include <engine/engine.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/version.h>

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"
#include "curl/easy.h"

#include "fetcher.h"

double CGetFile::Current() const { return m_Current; }
double CGetFile::Size() const { return m_Size; }
int CGetFile::Progress() const { return m_Progress; }
int CGetFile::State() const { return m_State; }
const char *CGetFile::Dest() const { return m_aDest; }

void CGetFile::Abort() { m_Abort = true; };

CGetFile::CGetFile(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType, bool UseDDNetCA, bool CanTimeout) :
	m_pStorage(pStorage),
	m_StorageType(StorageType),
	m_UseDDNetCA(UseDDNetCA),
	m_CanTimeout(CanTimeout),
	m_Size(0),
	m_Progress(0),
	m_State(HTTP_QUEUED),
	m_Abort(false)
{
	str_copy(m_aUrl, pUrl, sizeof(m_aUrl));
	str_copy(m_aDest, pDest, sizeof(m_aDest));
}


bool FetcherInit()
{
	return !curl_global_init(CURL_GLOBAL_DEFAULT);
}

void EscapeUrl(char *pBuf, int Size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, Size);
	curl_free(pEsc);
}

static void SetCurlOptions(CURL *pHandle, char *pErrorBuffer, const char *pUrl, bool CanTimeout)
{
	//curl_easy_setopt(pHandle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, pErrorBuffer);

	if(CanTimeout)
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
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pHandle, CURLOPT_URL, pUrl);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, "DDNet " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");
}

void CGetFile::Run()
{
	CURL *pHandle = curl_easy_init();
	if(!pHandle)
	{
		m_State = HTTP_ERROR;
		return;
	}

	char aPath[512];
	if(m_StorageType == -2)
		m_pStorage->GetBinaryPath(m_aDest, aPath, sizeof(aPath));
	else
		m_pStorage->GetCompletePath(m_StorageType, m_aDest, aPath, sizeof(aPath));

	if(fs_makedir_rec_for(aPath) < 0)
		dbg_msg("fetcher", "i/o error, cannot create folder for: %s", aPath);

	IOHANDLE File = io_open(aPath, IOFLAG_WRITE);

	if(!File)
	{
		dbg_msg("fetcher", "i/o error, cannot open file: %s", m_aDest);
		m_State = HTTP_ERROR;
		return;
	}

	char aErr[CURL_ERROR_SIZE];
	SetCurlOptions(pHandle, aErr, m_aUrl, m_CanTimeout);
	if(m_UseDDNetCA)
	{
		char aCAFile[512];
		m_pStorage->GetBinaryPath("data/ca-ddnet.pem", aCAFile, sizeof aCAFile);
		curl_easy_setopt(pHandle, CURLOPT_CAINFO, aCAFile);
	}
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);

	dbg_msg("fetcher", "downloading %s", m_aDest);
	m_State = HTTP_RUNNING;
	int Ret = curl_easy_perform(pHandle);
	io_close(File);
	if(Ret != CURLE_OK)
	{
		dbg_msg("fetcher", "task failed. libcurl error: %s", aErr);
		m_State = (Ret == CURLE_ABORTED_BY_CALLBACK) ? HTTP_ABORTED : HTTP_ERROR;
	}
	else
	{
		dbg_msg("fetcher", "task done %s", m_aDest);
		m_State = HTTP_DONE;
	}

	curl_easy_cleanup(pHandle);

	OnCompletion();
}

size_t CGetFile::WriteCallback(char *pData, size_t Size, size_t Number, void *pFile)
{
	return io_write((IOHANDLE)pFile, pData, Size * Number);
}

int CGetFile::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CGetFile *pTask = (CGetFile *)pUser;
	pTask->m_Current = DlCurr;
	pTask->m_Size = DlTotal;
	pTask->m_Progress = (100 * DlCurr) / (DlTotal ? DlTotal : 1);
	pTask->OnProgress();
	return pTask->m_Abort ? -1 : 0;
}

int CPostJson::State() const { return m_State; }

CPostJson::CPostJson(const char *pUrl, const char *pJson)
	: m_State(HTTP_QUEUED)
{
	str_copy(m_aUrl, pUrl, sizeof(m_aUrl));
	str_copy(m_aJson, pJson, sizeof(m_aJson));
}

void CPostJson::Run()
{
	CURL *pHandle = curl_easy_init();
	if(!pHandle)
	{
		m_State = HTTP_ERROR;
		return;
	}

	char aErr[CURL_ERROR_SIZE];
	SetCurlOptions(pHandle, aErr, m_aUrl, true);

	curl_slist *pHeaders = NULL;
	pHeaders = curl_slist_append(pHeaders, "Content-Type: application/json");
	curl_easy_setopt(pHandle, CURLOPT_HTTPHEADER, pHeaders);
	curl_easy_setopt(pHandle, CURLOPT_POSTFIELDS, m_aJson);

	dbg_msg("fetcher", "posting to %s", m_aUrl);
	m_State = HTTP_RUNNING;
	int Result = curl_easy_perform(pHandle);

	curl_slist_free_all(pHeaders);
	if(Result != CURLE_OK)
	{
		dbg_msg("fetcher", "task failed. libcurl error: %s", aErr);
		m_State = HTTP_ERROR;
	}
	else
	{
		dbg_msg("fetcher", "posting to %s done", m_aUrl);
		m_State = HTTP_DONE;
	}

	curl_easy_cleanup(pHandle);

	OnCompletion();
}

static char EscapeJsonChar(char c)
{
	switch(c)
	{
	case '\"': return '\"';
	case '\\': return '\\';
	case '\b': return 'b';
	case '\n': return 'n';
	case '\r': return 'r';
	case '\t': return 't';
	// Don't escape '\f', who uses that. :)
	default: return 0;
	}
}

char *EscapeJson(char *pBuffer, int BufferSize, const char *pString)
{
	dbg_assert(BufferSize > 0, "can't null-terminate the string");
	// Subtract the space for null termination early.
	BufferSize--;

	char *pResult = pBuffer;
	while(BufferSize && *pString)
	{
		char c = *pString;
		pString++;
		char Escaped = EscapeJsonChar(c);
		if(Escaped)
		{
			if(BufferSize < 2)
			{
				break;
			}
			*pBuffer++ = '\\';
			*pBuffer++ = Escaped;
			BufferSize -= 2;
		}
		// Assuming ASCII/UTF-8, "if control character".
		else if((unsigned char)c < 0x20)
		{
			// \uXXXX
			if(BufferSize < 6)
			{
				break;
			}
			str_format(pBuffer, BufferSize, "\\u%04x", c);
			pBuffer += 6;
			BufferSize -= 6;
		}
		else
		{
			*pBuffer++ = c;
			BufferSize--;
		}
	}
	*pBuffer = 0;
	return pResult;
}
