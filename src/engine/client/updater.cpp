#include <base/system.h>

#include "updater.h"
#include <engine/client.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/version.h>

#include <cstdlib> // system

using std::string;

class CUpdaterFetchTask : public CHttpRequest
{
	char m_aBuf[256];
	char m_aBuf2[256];
	CUpdater *m_pUpdater;

	void OnProgress() override;

protected:
	void OnCompletion(EHttpState State) override;

public:
	CUpdaterFetchTask(CUpdater *pUpdater, const char *pFile, const char *pDestPath);
};

static const char *GetUpdaterUrl(char *pBuf, int BufSize, const char *pFile)
{
	str_format(pBuf, BufSize, "https://update.ddnet.org/%s", pFile);
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
	CHttpRequest(GetUpdaterUrl(m_aBuf, sizeof(m_aBuf), pFile)),
	m_pUpdater(pUpdater)
{
	WriteToFile(pUpdater->m_pStorage, GetUpdaterDestPath(m_aBuf2, sizeof(m_aBuf2), pFile, pDestPath), -2);
}

void CUpdaterFetchTask::OnProgress()
{
	CLockScope ls(m_pUpdater->m_Lock);
	m_pUpdater->m_Percent = Progress();
}

void CUpdaterFetchTask::OnCompletion(EHttpState State)
{
	const char *pFileName = 0;
	for(const char *pPath = Dest(); *pPath; pPath++)
		if(*pPath == '/')
			pFileName = pPath + 1;
	pFileName = pFileName ? pFileName : Dest();
	if(!str_comp(pFileName, "update.json"))
	{
		if(State == EHttpState::DONE)
			m_pUpdater->SetCurrentState(IUpdater::GOT_MANIFEST);
		else if(State == EHttpState::ERROR)
			m_pUpdater->SetCurrentState(IUpdater::FAIL);
	}
}

CUpdater::CUpdater()
{
	m_pClient = nullptr;
	m_pStorage = nullptr;
	m_pEngine = nullptr;
	m_pHttp = nullptr;
	m_State = CLEAN;
	m_Percent = 0;
	m_pCurrentTask = nullptr;

	m_ClientUpdate = m_ServerUpdate = m_ClientFetched = m_ServerFetched = false;

	IStorage::FormatTmpPath(m_aClientExecTmp, sizeof(m_aClientExecTmp), CLIENT_EXEC);
	IStorage::FormatTmpPath(m_aServerExecTmp, sizeof(m_aServerExecTmp), SERVER_EXEC);
}

void CUpdater::Init(CHttp *pHttp)
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pHttp = pHttp;
}

void CUpdater::SetCurrentState(EUpdaterState NewState)
{
	CLockScope ls(m_Lock);
	m_State = NewState;
}

IUpdater::EUpdaterState CUpdater::GetCurrentState()
{
	CLockScope ls(m_Lock);
	return m_State;
}

void CUpdater::GetCurrentFile(char *pBuf, int BufSize)
{
	CLockScope ls(m_Lock);
	str_copy(pBuf, m_aStatus, BufSize);
}

int CUpdater::GetCurrentPercent()
{
	CLockScope ls(m_Lock);
	return m_Percent;
}

void CUpdater::FetchFile(const char *pFile, const char *pDestPath)
{
	CLockScope ls(m_Lock);
	m_pCurrentTask = std::make_shared<CUpdaterFetchTask>(this, pFile, pDestPath);
	str_copy(m_aStatus, m_pCurrentTask->Dest());
	m_pHttp->Run(m_pCurrentTask);
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
	if(!str_comp_nocase(pFile + len - 3, ".so"))
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
	switch(GetCurrentState())
	{
	case IUpdater::GOT_MANIFEST:
		PerformUpdate();
		break;
	case IUpdater::DOWNLOADING:
		RunningUpdate();
		break;
	case IUpdater::MOVE_FILES:
		CommitUpdate();
		break;
	default:
		return;
	}
}

void CUpdater::AddFileJob(const char *pFile, bool Job)
{
	m_FileJobs.emplace_front(pFile, Job);
}

bool CUpdater::ReplaceClient()
{
	dbg_msg("updater", "replacing " PLAT_CLIENT_EXEC);
	bool Success = true;
	char aPath[IO_MAX_PATH_LENGTH];

	// Replace running executable by renaming twice...
	m_pStorage->RemoveBinaryFile(CLIENT_EXEC ".old");
	Success &= m_pStorage->RenameBinaryFile(PLAT_CLIENT_EXEC, CLIENT_EXEC ".old");
	str_format(aPath, sizeof(aPath), "update/%s", m_aClientExecTmp);
	Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_CLIENT_EXEC);
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aPath, sizeof(aPath));
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "chmod +x %s", aPath);
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
	char aPath[IO_MAX_PATH_LENGTH];

	//Replace running executable by renaming twice...
	m_pStorage->RemoveBinaryFile(SERVER_EXEC ".old");
	Success &= m_pStorage->RenameBinaryFile(PLAT_SERVER_EXEC, SERVER_EXEC ".old");
	str_format(aPath, sizeof(aPath), "update/%s", m_aServerExecTmp);
	Success &= m_pStorage->RenameBinaryFile(aPath, PLAT_SERVER_EXEC);
#if !defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPath(PLAT_SERVER_EXEC, aPath, sizeof(aPath));
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "chmod +x %s", aPath);
	if(system(aBuf))
	{
		dbg_msg("updater", "ERROR: failed to set server executable bit");
		Success = false;
	}
#endif
	return Success;
}

void CUpdater::ParseUpdate()
{
	char aPath[IO_MAX_PATH_LENGTH];
	void *pBuf;
	unsigned Length;
	if(!m_pStorage->ReadFile(m_pStorage->GetBinaryPath("update/update.json", aPath, sizeof(aPath)), IStorage::TYPE_ABSOLUTE, &pBuf, &Length))
		return;

	json_value *pVersions = json_parse((json_char *)pBuf, Length);
	free(pBuf);

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
						AddFileJob(json_string_get(json_array_get(pTemp, j)), true);
				}
				if((pTemp = json_object_get(pCurrent, "remove"))->type == json_array)
				{
					for(int j = 0; j < json_array_length(pTemp); j++)
						AddFileJob(json_string_get(json_array_get(pTemp, j)), false);
				}
			}
			else
				break;
		}
	}
	json_value_free(pVersions);
}

void CUpdater::InitiateUpdate()
{
	SetCurrentState(IUpdater::GETTING_MANIFEST);
	FetchFile("update.json");
}

void CUpdater::PerformUpdate()
{
	SetCurrentState(IUpdater::PARSING_UPDATE);
	dbg_msg("updater", "parsing update.json");
	ParseUpdate();
	m_CurrentJob = m_FileJobs.begin();
	SetCurrentState(IUpdater::DOWNLOADING);
}

void CUpdater::RunningUpdate()
{
	if(m_pCurrentTask)
	{
		if(!m_pCurrentTask->Done())
		{
			return;
		}
		else if(m_pCurrentTask->State() == EHttpState::ERROR || m_pCurrentTask->State() == EHttpState::ABORTED)
		{
			SetCurrentState(IUpdater::FAIL);
		}
	}

	if(m_CurrentJob != m_FileJobs.end())
	{
		auto &Job = *m_CurrentJob;
		if(Job.second)
		{
			const char *pFile = Job.first.c_str();
			size_t len = str_length(pFile);
			if(!str_comp_nocase(pFile + len - 4, ".dll"))
			{
#if defined(CONF_FAMILY_WINDOWS)
				char aBuf[512];
				str_copy(aBuf, pFile, sizeof(aBuf)); // SDL
				str_copy(aBuf + len - 4, "-" PLAT_NAME, sizeof(aBuf) - len + 4); // -win32
				str_append(aBuf, pFile + len - 4); // .dll
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
				str_append(aBuf, pFile + len - 3); // .so
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
		{
			m_pStorage->RemoveBinaryFile(Job.first.c_str());
		}

		m_CurrentJob++;
	}
	else
	{
		if(m_ServerUpdate && !m_ServerFetched)
		{
			FetchFile(PLAT_SERVER_DOWN, m_aServerExecTmp);
			m_ServerFetched = true;
			return;
		}

		if(m_ClientUpdate && !m_ClientFetched)
		{
			FetchFile(PLAT_CLIENT_DOWN, m_aClientExecTmp);
			m_ClientFetched = true;
			return;
		}

		SetCurrentState(IUpdater::MOVE_FILES);
	}
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
		SetCurrentState(IUpdater::FAIL);
	else if(m_pClient->State() == IClient::STATE_ONLINE || m_pClient->EditorHasUnsavedData())
		SetCurrentState(IUpdater::NEED_RESTART);
	else
	{
		m_pClient->Restart();
	}
}
