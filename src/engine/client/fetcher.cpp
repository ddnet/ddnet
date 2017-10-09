#include <base/system.h>
#include <engine/engine.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/version.h>

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"
#include "curl/easy.h"

#include "fetcher.h"

class CFetchTask : IFetchTask
{
	friend class CFetcher;
	class CFetcher *m_pFetcher;

	CJob m_Job;
	CURL *m_pHandle;
	void *m_pUser;

	char m_aUrl[256];
	char m_aDest[128];
	int m_StorageType;
	bool m_UseDDNetCA;
	bool m_CanTimeout;

	PROGFUNC m_pfnProgressCallback;
	COMPFUNC m_pfnCompCallback;

	double m_Size;
	double m_Current;
	int m_Progress;
	int m_State;

	bool m_Abort;
	bool m_Destroy;
public:

	double Current() const { return m_Current; };
	double Size() const { return m_Size; };
	int Progress() const { return m_Progress; };
	int State() const { return m_State; };
	const char *Dest() const { return m_aDest; };

	void Abort() { m_Abort = true; };
	void Destroy();
};

void CFetchTask::Destroy()
{
	if(m_State >= IFetchTask::STATE_DONE || m_State == IFetchTask::STATE_ERROR)
		mem_free(this);
	else {
		m_Abort = true;
		m_Destroy = true;
	}
}

bool CFetcher::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	if(curl_global_init(CURL_GLOBAL_DEFAULT))
		return false;
	return true;
}

void CFetcher::Escape(char *pBuf, size_t size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, size);
	curl_free(pEsc);
}

IFetchTask *CFetcher::FetchFile(const char *pUrl, const char *pDest, int StorageType, bool UseDDNetCA, bool CanTimeout, void *pUser, COMPFUNC pfnCompCb, PROGFUNC pfnProgCb)
{
	CFetchTask *pTask = (CFetchTask *)mem_alloc(sizeof *pTask, 1);
	pTask->m_pFetcher = this;

	str_copy(pTask->m_aUrl, pUrl, sizeof(pTask->m_aUrl));
	str_copy(pTask->m_aDest, pDest, sizeof(pTask->m_aDest));
	pTask->m_StorageType = StorageType;
	pTask->m_pUser = pUser;
	pTask->m_pfnCompCallback = pfnCompCb;
	pTask->m_pfnProgressCallback = pfnProgCb;

	pTask->m_Abort = false;
	pTask->m_Destroy = false;
	pTask->m_Size = pTask->m_Progress = 0;

	pTask->m_State = IFetchTask::STATE_QUEUED;
	m_pEngine->AddJob(&pTask->m_Job, FetcherThread, pTask);

	return pTask;
}

int CFetcher::FetcherThread(void *pUser)
{
	CFetchTask *pTask = (CFetchTask *)pUser;

	pTask->m_pHandle = curl_easy_init();
	if(!pTask->m_pHandle)
		return 1;

	char aPath[512];
	if(pTask->m_StorageType == -2)
		pTask->m_pFetcher->m_pStorage->GetBinaryPath(pTask->m_aDest, aPath, sizeof(aPath));
	else
		pTask->m_pFetcher->m_pStorage->GetCompletePath(pTask->m_StorageType, pTask->m_aDest, aPath, sizeof(aPath));

	if(fs_makedir_rec_for(aPath) < 0)
		dbg_msg("fetcher", "i/o error, cannot create folder for: %s", aPath);

	IOHANDLE File = io_open(aPath, IOFLAG_WRITE);

	if(!File)
	{
		dbg_msg("fetcher", "i/o error, cannot open file: %s", pTask->m_aDest);
		pTask->m_State = IFetchTask::STATE_ERROR;
		return 1;
	}

	char aErr[CURL_ERROR_SIZE];
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_ERRORBUFFER, aErr);

	//curl_easy_setopt(pTask->m_pHandle, CURLOPT_VERBOSE, 1L);
	if(pTask->m_CanTimeout)
	{
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_CONNECTTIMEOUT_MS, (long)g_Config.m_ClHTTPConnectTimeoutMs);
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_LOW_SPEED_LIMIT, (long)g_Config.m_ClHTTPLowSpeedLimit);
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_LOW_SPEED_TIME, (long)g_Config.m_ClHTTPLowSpeedTime);
	}
	else
	{
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_CONNECTTIMEOUT_MS, 0);
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_LOW_SPEED_LIMIT, 0);
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_LOW_SPEED_TIME, 0);
	}
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_FAILONERROR, 1L);
	if(pTask->m_UseDDNetCA)
	{
		char aCAFile[512];
		pTask->m_pFetcher->m_pStorage->GetBinaryPath("data/ca-ddnet.pem", aCAFile, sizeof aCAFile);
		curl_easy_setopt(pTask->m_pHandle, CURLOPT_CAINFO, aCAFile);
	}
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_URL, pTask->m_aUrl);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_WRITEFUNCTION, &CFetcher::WriteToFile);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_PROGRESSDATA, pTask);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_PROGRESSFUNCTION, &CFetcher::ProgressCallback);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pTask->m_pHandle, CURLOPT_USERAGENT, "DDNet " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");

	dbg_msg("fetcher", "downloading %s", pTask->m_aDest);
	pTask->m_State = IFetchTask::STATE_RUNNING;
	int ret = curl_easy_perform(pTask->m_pHandle);
	io_close(File);
	if(ret != CURLE_OK)
	{
		dbg_msg("fetcher", "task failed. libcurl error: %s", aErr);
		pTask->m_State = (ret == CURLE_ABORTED_BY_CALLBACK) ? IFetchTask::STATE_ABORTED : IFetchTask::STATE_ERROR;
	}
	else
	{
		dbg_msg("fetcher", "task done %s", pTask->m_aDest);
		pTask->m_State = IFetchTask::STATE_DONE;
	}

	curl_easy_cleanup(pTask->m_pHandle);

	if(pTask->m_pfnCompCallback)
		pTask->m_pfnCompCallback(pTask, pTask->m_pUser);

	if(pTask->m_Destroy)
		mem_free(pTask);

	return(ret != CURLE_OK) ? 1 : 0;
}

size_t CFetcher::WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile)
{
	return io_write((IOHANDLE)pFile, pData, size*nmemb);
}

int CFetcher::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CFetchTask *pTask = (CFetchTask *)pUser;
	pTask->m_Current = DlCurr;
	pTask->m_Size = DlTotal;
	pTask->m_Progress = (100 * DlCurr) / (DlTotal ? DlTotal : 1);
	if(pTask->m_pfnProgressCallback)
		pTask->m_pfnProgressCallback(pTask, pTask->m_pUser);
	return pTask->m_Abort ? -1 : 0;
}
