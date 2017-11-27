#include <base/system.h>
#include <engine/engine.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/version.h>

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"
#include "curl/easy.h"

#include "fetcher.h"

double CFetchTask::Current() const { return m_Current; }
double CFetchTask::Size() const { return m_Size; }
int CFetchTask::Progress() const { return m_Progress; }
int CFetchTask::State() const { return m_State; }
const char *CFetchTask::Dest() const { return m_aDest; }

void CFetchTask::Abort() { m_Abort = true; };

CFetchTask::CFetchTask(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType, bool UseDDNetCA, bool CanTimeout) :
	m_pStorage(pStorage),
	m_StorageType(StorageType),
	m_UseDDNetCA(UseDDNetCA),
	m_CanTimeout(CanTimeout),
	m_Size(0),
	m_Progress(0),
	m_State(CFetchTask::STATE_QUEUED),
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

void CFetchTask::Run()
{
	CURL *pHandle = curl_easy_init();
	if(!pHandle)
	{
		m_State = STATE_ERROR;
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
		m_State = CFetchTask::STATE_ERROR;
		return;
	}

	char aErr[CURL_ERROR_SIZE];
	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, aErr);

	//curl_easy_setopt(pHandle, CURLOPT_VERBOSE, 1L);
	if(m_CanTimeout)
	{
		curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, (long)g_Config.m_ClHTTPConnectTimeoutMs);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, (long)g_Config.m_ClHTTPLowSpeedLimit);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, (long)g_Config.m_ClHTTPLowSpeedTime);
	}
	else
	{
		curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, 0);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_LIMIT, 0);
		curl_easy_setopt(pHandle, CURLOPT_LOW_SPEED_TIME, 0);
	}
	curl_easy_setopt(pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pHandle, CURLOPT_FAILONERROR, 1L);
	if(m_UseDDNetCA)
	{
		char aCAFile[512];
		m_pStorage->GetBinaryPath("data/ca-ddnet.pem", aCAFile, sizeof aCAFile);
		curl_easy_setopt(pHandle, CURLOPT_CAINFO, aCAFile);
	}
	curl_easy_setopt(pHandle, CURLOPT_URL, m_aUrl);
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pHandle, CURLOPT_USERAGENT, "DDNet " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");

	dbg_msg("fetcher", "downloading %s", m_aDest);
	m_State = CFetchTask::STATE_RUNNING;
	int Ret = curl_easy_perform(pHandle);
	io_close(File);
	if(Ret != CURLE_OK)
	{
		dbg_msg("fetcher", "task failed. libcurl error: %s", aErr);
		m_State = (Ret == CURLE_ABORTED_BY_CALLBACK) ? CFetchTask::STATE_ABORTED : CFetchTask::STATE_ERROR;
	}
	else
	{
		dbg_msg("fetcher", "task done %s", m_aDest);
		m_State = CFetchTask::STATE_DONE;
	}

	curl_easy_cleanup(pHandle);

	OnCompletion();
}

size_t CFetchTask::WriteCallback(char *pData, size_t Size, size_t Number, void *pFile)
{
	return io_write((IOHANDLE)pFile, pData, Size * Number);
}

int CFetchTask::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CFetchTask *pTask = (CFetchTask *)pUser;
	pTask->m_Current = DlCurr;
	pTask->m_Size = DlTotal;
	pTask->m_Progress = (100 * DlCurr) / (DlTotal ? DlTotal : 1);
	pTask->OnProgress();
	return pTask->m_Abort ? -1 : 0;
}
