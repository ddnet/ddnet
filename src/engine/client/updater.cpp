#include "updater.h"
#include <base/system.h>
#include <engine/fetcher.h>
#include <engine/storage.h>
#include <engine/client.h>
#include <engine/external/json-parser/json.h>
#include <game/version.h>

#include <stdlib.h> // system

using std::string;
using std::vector;

CUpdater::CUpdater()
{
	m_pClient = NULL;
	m_pStorage = NULL;
	m_pFetcher = NULL;
	m_State = CLEAN;
	m_Percent = 0;
}

void CUpdater::Init()
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pFetcher = Kernel()->RequestInterface<IFetcher>();
	m_IsWinXP = os_compare_version(5, 1) <= 0;
}

void CUpdater::ProgressCallback(CFetchTask *pTask, void *pUser)
{
	CUpdater *pUpdate = (CUpdater *)pUser;
	str_copy(pUpdate->m_Status, pTask->Dest(), sizeof(pUpdate->m_Status));
	pUpdate->m_Percent = pTask->Progress();
}

void CUpdater::CompletionCallback(CFetchTask *pTask, void *pUser)
{
	CUpdater *pUpdate = (CUpdater *)pUser;
	if(!str_comp(pTask->Dest(), "update.json"))
	{
		if(pTask->State() == CFetchTask::STATE_DONE)
			pUpdate->m_State = GOT_MANIFEST;
		else if(pTask->State() == CFetchTask::STATE_ERROR)
			pUpdate->m_State = FAIL;
	}
	else if(!str_comp(pTask->Dest(), pUpdate->m_aLastFile))
	{
		if(pTask->State() == CFetchTask::STATE_DONE)
		{
			if(pUpdate->m_ClientUpdate)
				pUpdate->ReplaceClient();
			if(pUpdate->m_ServerUpdate)
				pUpdate->ReplaceServer();
			if(pUpdate->m_pClient->State() == IClient::STATE_ONLINE || pUpdate->m_pClient->EditorHasUnsavedData())
				pUpdate->m_State = NEED_RESTART;
			else{
				if(!pUpdate->m_IsWinXP)
					pUpdate->m_pClient->Restart();
				else
					pUpdate->WinXpRestart();
			}
		}
		else if(pTask->State() == CFetchTask::STATE_ERROR)
			pUpdate->m_State = FAIL;
	}
	delete pTask;
}

void CUpdater::FetchFile(const char *pFile, const char *pDestPath)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "https://%s/%s", g_Config.m_ClDDNetUpdateServer, pFile);
	if(!pDestPath)
		pDestPath = pFile;
	CFetchTask *Task = new CFetchTask;
	m_pFetcher->QueueAdd(Task, aBuf, pDestPath, -2, this, &CUpdater::CompletionCallback, &CUpdater::ProgressCallback);
}

void CUpdater::Update()
{
	switch(m_State)
	{
		case GOT_MANIFEST:
			PerformUpdate();
		default:
			return;
	}
}

void CUpdater::AddNewFile(const char *pFile)
{
	//Check if already on the download list
	for(vector<string>::iterator it = m_AddedFiles.begin(); it < m_AddedFiles.end(); ++it)
	{
		if(!str_comp(it->c_str(), pFile))
			return;
	}
	m_AddedFiles.push_back(string(pFile));
}

void CUpdater::AddRemovedFile(const char *pFile)
{
	//First remove from to be downloaded list
	for(vector<string>::iterator it = m_AddedFiles.begin(); it < m_AddedFiles.end(); ++it)
	{
		if(!str_comp(it->c_str(), pFile)){
			m_AddedFiles.erase(it);
			break;
		}
	}
	m_RemovedFiles.push_back(string(pFile));
}

void CUpdater::ReplaceClient()
{
	dbg_msg("updater", "Replacing " PLAT_CLIENT_EXEC);

	//Replace running executable by renaming twice...
	if(m_IsWinXP)
	{
		m_pStorage->RemoveBinaryFile("DDNet.old");
		m_pStorage->RenameBinaryFile(PLAT_CLIENT_EXEC, "DDNet.old");
		m_pStorage->RenameBinaryFile("DDNet.tmp", PLAT_CLIENT_EXEC);
	}
	#if !defined(CONF_FAMILY_WINDOWS)
		char aPath[512];
		m_pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aPath, sizeof aPath);
		char aBuf[512];
		str_format(aBuf, sizeof aBuf, "chmod +x %s", aPath);
		if (system(aBuf))
			dbg_msg("updater", "Error setting client executable bit");
	#endif
}

void CUpdater::ReplaceServer()
{
	dbg_msg("updater", "Replacing " PLAT_SERVER_EXEC);

	//Replace running executable by renaming twice...
	m_pStorage->RemoveBinaryFile("DDNet-Server.old");
	m_pStorage->RenameBinaryFile(PLAT_SERVER_EXEC, "DDNet-Server.old");
	m_pStorage->RenameBinaryFile("DDNet-Server.tmp", PLAT_SERVER_EXEC);
	#if !defined(CONF_FAMILY_WINDOWS)
		char aPath[512];
		m_pStorage->GetBinaryPath(PLAT_SERVER_EXEC, aPath, sizeof aPath);
		char aBuf[512];
		str_format(aBuf, sizeof aBuf, "chmod +x %s", aPath);
		if (system(aBuf))
			dbg_msg("updater", "Error setting server executable bit");
	#endif
}

void CUpdater::ParseUpdate()
{
	char aPath[512];
	IOHANDLE File = m_pStorage->OpenFile(m_pStorage->GetBinaryPath("update.json", aPath, sizeof aPath), IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		char aBuf[4096*4];
		mem_zero(aBuf, sizeof (aBuf));
		io_read(File, aBuf, sizeof(aBuf));
		io_close(File);

		json_value *pVersions = json_parse(aBuf);

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
							AddNewFile(json_string_get(json_array_get(pTemp, j)));
					}
					if((pTemp = json_object_get(pCurrent, "remove"))->type == json_array)
					{
						for(int j = 0; j < json_array_length(pTemp); j++)
							AddRemovedFile(json_string_get(json_array_get(pTemp, j)));
					}
				}
				else
					break;
			}
		}
	}
}

void CUpdater::InitiateUpdate()
{
	m_State = GETTING_MANIFEST;
	FetchFile("update.json");
}

void CUpdater::PerformUpdate()
{
	m_State = PARSING_UPDATE;
	dbg_msg("updater", "Parsing update.json");
	ParseUpdate();
	m_State = DOWNLOADING;

	const char *aLastFile;
	if(m_ClientUpdate)
		aLastFile = "DDNet.tmp";
	else if(!m_AddedFiles.empty())
		aLastFile= m_AddedFiles.front().c_str();
	else
		aLastFile = "";

	str_copy(m_aLastFile, aLastFile, sizeof(m_aLastFile));

	while(!m_AddedFiles.empty())
	{
		FetchFile(m_AddedFiles.back().c_str());
		m_AddedFiles.pop_back();
	}
	while(!m_RemovedFiles.empty())
	{
		m_pStorage->RemoveBinaryFile(m_RemovedFiles.back().c_str());
		m_RemovedFiles.pop_back();
	}
	if(m_ServerUpdate)
		FetchFile(PLAT_SERVER_DOWN, "DDNet-Server.tmp");
	if(m_ClientUpdate)
		FetchFile(PLAT_CLIENT_DOWN, "DDNet.tmp");
}

void CUpdater::WinXpRestart()
{		
		char aBuf[512];
		IOHANDLE bhFile = io_open(m_pStorage->GetBinaryPath("du.bat", aBuf, sizeof aBuf), IOFLAG_WRITE);
		if(!bhFile)
			return;
		char bBuf[512];
		str_format(bBuf, sizeof(bBuf), ":_R\r\ndel \"DDNet.exe\"\r\nif exist \"DDNet.exe\" goto _R\r\nrename \"DDNet.tmp\" \"DDNet.exe\"\r\n:_T\r\nif not exist \"DDNet.exe\" goto _T\r\nstart DDNet.exe\r\ndel \"du.bat\"\r\n");
		io_write(bhFile, bBuf, str_length(bBuf));
		io_close(bhFile);
		shell_execute(aBuf);
		m_pClient->Quit();
}
