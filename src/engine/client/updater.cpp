#include "updater.h"
#include <base/system.h>
#include <engine/client.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/json.h>
#include <engine/storage.h>
#include <game/version.h>

#include <stdlib.h> // system

class CUpdaterFetchTask : public CGetFile
{
	char m_aBuf[256];
	char m_aBuf2[256];
	int m_PreviousDownloaded;
	CUpdater *m_pUpdater;

	virtual void OnProgress();

protected:
	virtual int OnCompletion(int State);

public:
	CUpdaterFetchTask(CUpdater *pUpdater, const char *pFile, const char *pDestPath);
};

static const char *GetUpdaterUrl(char *pBuf, int BufSize, const char *pFile)
{
	str_format(pBuf, BufSize, "https://update6.ddnet.tw/%s", pFile);
	return pBuf;
}

static const char *GetUpdaterDestPath(char *pBuf, int BufSize, const char *pFile, const char *pDestPath)
{
	if(!pDestPath)
	{
		pDestPath = pFile;
	}
	str_format(pBuf, BufSize, "update/%s", pDestPath);
	return pBuf;
}

CUpdaterFetchTask::CUpdaterFetchTask(CUpdater *pUpdater, const char *pFile, const char *pDestPath) :
	CGetFile(pUpdater->m_pStorage, GetUpdaterUrl(m_aBuf, sizeof(m_aBuf), pFile), GetUpdaterDestPath(m_aBuf2, sizeof(m_aBuf), pFile, pDestPath), -2, CTimeout{0, 0, 0}),
	m_PreviousDownloaded(0),
	m_pUpdater(pUpdater)
{
}

int CUpdaterFetchTask::OnCompletion(int State)
{
	State = CGetFile::OnCompletion(State);

	if(State == HTTP_DONE)
		m_pUpdater->m_CompletedFetchJobs++;
	else if(State == HTTP_ERROR)
		m_pUpdater->m_State = IUpdater::FAIL;

	return State;
}

void CUpdaterFetchTask::OnProgress()
{
	if(m_pUpdater->m_State == IUpdater::FAIL)
		Abort();

	m_pUpdater->m_TotalDownloaded += Current() - m_PreviousDownloaded;
	m_PreviousDownloaded = Current();
}

CUpdater::CUpdater()
{
	m_pClient = NULL;
	m_pStorage = NULL;
	m_pEngine = NULL;
	m_State = CLEAN;
	m_DownloadStart = 0;
	m_TotalDownloaded = 0;
	m_PreventRestart = false;

	str_format(m_aClientExecTmp, sizeof(m_aClientExecTmp), CLIENT_EXEC ".%d.tmp", pid());
	str_format(m_aServerExecTmp, sizeof(m_aServerExecTmp), SERVER_EXEC ".%d.tmp", pid());
}

void CUpdater::Init()
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_IsWinXP = os_is_winxp_or_lower();
}

float CUpdater::Progress() const
{
	return m_CompletedFetchJobs / (float)m_FetchJobs.size();
}

char *CUpdater::Speed(char *pBuf, int BufSize) const
{
	float KB = m_TotalDownloaded / 1024;
	float s = (float)(time_get() - m_DownloadStart) / time_freq();
	str_format(pBuf, BufSize, "%d KB/s", s > 0 ? round_to_int(KB / s) : 0);

	return pBuf;
}

std::shared_ptr<CUpdaterFetchTask> CUpdater::FetchFile(const char *pFile, const char *pDestPath)
{
	m_FetchJobs.emplace_back(std::make_shared<CUpdaterFetchTask>(this, pFile, pDestPath));
	m_pEngine->AddJob(m_FetchJobs.back());

	return m_FetchJobs.back();
}

bool CUpdater::MoveFile(const char *pFile)
{
	char aBuf[256];
	size_t len = str_length(pFile);
	bool Success = true;

#if !defined(CONF_FAMILY_WINDOWS)
	if(!str_comp_nocase(pFile + len - 4, ".dll"))
		return Success;
#endif

#if !defined(CONF_PLATFORM_LINUX)
	if(!str_comp_nocase(pFile + len - 4, ".so"))
		return Success;
#endif

	if(!str_comp_nocase(pFile + len - 4, ".dll") || !str_comp_nocase(pFile + len - 4, ".ttf") || !str_comp_nocase(pFile + len - 3, ".so"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.old", pFile);
		m_pStorage->RenameBinaryFile(pFile, aBuf);
		str_format(aBuf, sizeof(aBuf), "update/%s", pFile);
		Success &= m_pStorage->RenameBinaryFile(aBuf, pFile);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "update/%s", pFile);
		Success &= m_pStorage->RenameBinaryFile(aBuf, pFile);
	}

	return Success;
}

void CUpdater::Update()
{
	switch(m_State.load())
	{
	case GETTING_MANIFEST:
		switch(m_ManifestJob->Status())
		{
		case HTTP_DONE:
			m_State = GOT_MANIFEST;
			break;
		case HTTP_ERROR:
		case HTTP_ABORTED:
			m_State = FAIL;
		}
		break;
	case GOT_MANIFEST:
		PerformUpdate(ParseUpdate());
		break;
	case DOWNLOADING:
		if(m_CompletedFetchJobs == m_FetchJobs.size())
			m_State = MOVE_FILES;
		break;
	case MOVE_FILES:
		m_FetchJobs.clear();
		CommitUpdate();
		break;
	}
}

bool CUpdater::ReplaceClient()
{
	dbg_msg("updater", "replacing " PLAT_CLIENT_EXEC);
	bool Success = true;
	char aPath[512];

	// Replace running executable by renaming twice...
	if(!m_IsWinXP)
	{
		m_pStorage->RemoveBinaryFile(CLIENT_EXEC ".old");
		Success &= m_pStorage->RenameBinaryFile(PLAT_CLIENT_EXEC, CLIENT_EXEC ".old");
		str_format(aPath, sizeof(aPath), "update/%s", m_aClientExecTmp);
		Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_CLIENT_EXEC);
	}
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aPath, sizeof aPath);
	char aBuf[512];
	str_format(aBuf, sizeof aBuf, "chmod +x %s", aPath);
	if(system(aBuf))
	{
		dbg_msg("updater", "ERROR: failed to set client executable bit");
		Success = false;
	}
#endif
	return Success;
}

bool CUpdater::ReplaceServer()
{
	dbg_msg("updater", "replacing " PLAT_SERVER_EXEC);
	bool Success = true;
	char aPath[512];

	//Replace running executable by renaming twice...
	m_pStorage->RemoveBinaryFile(SERVER_EXEC ".old");
	Success &= m_pStorage->RenameBinaryFile(PLAT_SERVER_EXEC, SERVER_EXEC ".old");
	str_format(aPath, sizeof(aPath), "update/%s", m_aServerExecTmp);
	Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_SERVER_EXEC);
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_SERVER_EXEC, aPath, sizeof aPath);
	char aBuf[512];
	str_format(aBuf, sizeof aBuf, "chmod +x %s", aPath);
	if(system(aBuf))
	{
		dbg_msg("updater", "ERROR: failed to set server executable bit");
		Success = false;
	}
#endif
	return Success;
}

std::map<std::string, bool> CUpdater::ParseUpdate()
{
	dbg_msg("updater", "parsing update.json");

	char aPath[512];
	IOHANDLE File = m_pStorage->OpenFile(m_pStorage->GetBinaryPath("update/update.json", aPath, sizeof aPath), IOFLAG_READ, IStorage::TYPE_ABSOLUTE);
	if(!File)
		return {};

	long int Length = io_length(File);
	char *pBuf = (char *)malloc(Length);
	mem_zero(pBuf, Length);
	io_read(File, pBuf, Length);
	io_close(File);

	json_value *pVersions = json_parse(pBuf, Length);
	free(pBuf);

	std::map<std::string, bool> Jobs;
	if(pVersions && pVersions->type == json_array)
	{
		for(int i = 0; i < json_array_length(pVersions); i++)
		{
			const json_value *pTemp;
			const json_value *pCurrent = json_array_get(pVersions, i);
			if(str_comp(json_string_get(json_object_get(pCurrent, "version")), GAME_RELEASE_VERSION))
			{
				if(json_boolean_get(json_object_get(pCurrent, "client")))
					m_ClientUpdate = true;
				if(json_boolean_get(json_object_get(pCurrent, "server")))
					m_ServerUpdate = true;
				if((pTemp = json_object_get(pCurrent, "download"))->type == json_array)
				{
					for(int j = 0; j < json_array_length(pTemp); j++)
						Jobs[json_string_get(json_array_get(pTemp, j))] = true;
				}
				if((pTemp = json_object_get(pCurrent, "remove"))->type == json_array)
				{
					for(int j = 0; j < json_array_length(pTemp); j++)
						Jobs[json_string_get(json_array_get(pTemp, j))] = false;
				}
			}
			else
				break;
		}
	}

	return Jobs;
}

void CUpdater::InitiateUpdate()
{
	m_State = GETTING_MANIFEST;
	m_ManifestJob = FetchFile("update.json");
}

void CUpdater::PerformUpdate(const std::map<std::string, bool> &Jobs, bool PreventRestart)
{
	m_PreventRestart = PreventRestart;
	m_DownloadStart = time_get();
	m_State = DOWNLOADING;
	m_FileJobs = Jobs;

	for(const auto &FileJob : Jobs)
	{
		dbg_msg("debug", "considering %s %s", FileJob.first.c_str(), FileJob.second ? "true" : "false");
		if(FileJob.second)
		{
			const char *pFile = FileJob.first.c_str();
			size_t len = str_length(pFile);
			if(!str_comp_nocase(pFile + len - 4, ".dll"))
			{
#if defined(CONF_FAMILY_WINDOWS)
				char aBuf[512];
				str_copy(aBuf, pFile, sizeof(aBuf)); // SDL
				str_copy(aBuf + len - 4, "-" PLAT_NAME, sizeof(aBuf) - len + 4); // -win32
				str_append(aBuf, pFile + len - 4, sizeof(aBuf)); // .dll
				FetchFile(aBuf, pFile);
#endif
				// Ignore DLL downloads on other platforms
			}
			else if(!str_comp_nocase(pFile + len - 3, ".so"))
			{
#if defined(CONF_PLATFORM_LINUX)
				char aBuf[512];
				str_copy(aBuf, pFile, sizeof(aBuf)); // libsteam_api
				str_copy(aBuf + len - 3, "-" PLAT_NAME, sizeof(aBuf) - len + 3); // -linux-x86_64
				str_append(aBuf, pFile + len - 3, sizeof(aBuf)); // .so
				FetchFile(aBuf, pFile);
#endif
				// Ignore DLL downloads on other platforms, on Linux we statically link anyway
			}
			else
			{
				FetchFile(pFile);
			}
		}
		else
			m_pStorage->RemoveBinaryFile(FileJob.first.c_str());
	}

	if(m_ServerUpdate)
		FetchFile(PLAT_SERVER_DOWN, m_aServerExecTmp);

	if(m_ClientUpdate)
		FetchFile(PLAT_CLIENT_DOWN, m_aClientExecTmp);
}

void CUpdater::CommitUpdate()
{
	bool Success = true;

	for(auto &FileJob : m_FileJobs)
		if(FileJob.second)
			Success &= MoveFile(FileJob.first.c_str());

	if(m_ClientUpdate)
		Success &= ReplaceClient();
	if(m_ServerUpdate)
		Success &= ReplaceServer();
	if(!Success)
		m_State = FAIL;
	else if(m_pClient->State() == IClient::STATE_ONLINE || m_pClient->EditorHasUnsavedData() || m_PreventRestart)
		m_State = NEED_RESTART;
	else
	{
		if(!m_IsWinXP)
			m_pClient->Restart();
		else
			WinXpRestart();
	}
}

void CUpdater::WinXpRestart()
{
	char aBuf[512];
	IOHANDLE bhFile = io_open(m_pStorage->GetBinaryPath("du.bat", aBuf, sizeof aBuf), IOFLAG_WRITE);
	if(!bhFile)
		return;
	char bBuf[512];
	str_format(bBuf, sizeof(bBuf), ":_R\r\ndel \"" PLAT_CLIENT_EXEC "\"\r\nif exist \"" PLAT_CLIENT_EXEC "\" goto _R\r\n:_T\r\nmove /y \"update\\%s\" \"" PLAT_CLIENT_EXEC "\"\r\nif not exist \"" PLAT_CLIENT_EXEC "\" goto _T\r\nstart " PLAT_CLIENT_EXEC "\r\ndel \"du.bat\"\r\n", m_aClientExecTmp);
	io_write(bhFile, bBuf, str_length(bBuf));
	io_close(bhFile);
	shell_execute(aBuf);
	m_pClient->Quit();
}
