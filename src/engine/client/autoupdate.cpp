#include "autoupdate.h"
#include <base/system.h>
#include <engine/fetcher.h>
#include <engine/storage.h>
#include <engine/client.h>
#include <engine/external/json-parser/json.h>
#include <game/version.h>

#include <stdlib.h> // system

using std::string;
using std::vector;

CAutoUpdate::CAutoUpdate()
{
	m_pClient = NULL;
	m_pStorage = NULL;
	m_pFetcher = NULL;
	m_State = CLEAN;
	m_Percent = 0;
}

void CAutoUpdate::Init()
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pFetcher = Kernel()->RequestInterface<IFetcher>();
}

void CAutoUpdate::ProgressCallback(CFetchTask *pTask, void *pUser)
{
	CAutoUpdate *pUpdate = (CAutoUpdate *)pUser;
	str_copy(pUpdate->m_Status, pTask->Dest(), sizeof(pUpdate->m_Status));
	pUpdate->m_Percent = pTask->Progress();
}

void CAutoUpdate::CompletionCallback(CFetchTask *pTask, void *pUser)
{
	CAutoUpdate *pUpdate = (CAutoUpdate *)pUser;
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
			pUpdate->m_State = NEED_RESTART;
		}
		else if(pTask->State() == CFetchTask::STATE_ERROR)
			pUpdate->m_State = FAIL;
	}
	delete pTask;
}

void CAutoUpdate::FetchFile(const char *pFile, const char *pDestPath)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "https://%s/%s", g_Config.m_ClDDNetUpdateServer, pFile);
	if(!pDestPath)
		pDestPath = pFile;
	CFetchTask *Task = new CFetchTask;
	m_pFetcher->QueueAdd(Task, aBuf, pDestPath, 2, this, &CAutoUpdate::CompletionCallback, &CAutoUpdate::ProgressCallback);
}

void CAutoUpdate::Update()
{
	switch(m_State)
	{
		case GOT_MANIFEST:
			PerformUpdate();
		default:
			return;
	}
}

void CAutoUpdate::AddNewFile(const char *pFile)
{
	//Check if already on the download list
	for(vector<string>::iterator it = m_AddedFiles.begin(); it < m_AddedFiles.end(); ++it)
	{
		if(!str_comp(it->c_str(), pFile))
			return;
	}
	m_AddedFiles.push_back(string(pFile));
}

void CAutoUpdate::AddRemovedFile(const char *pFile)
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

void CAutoUpdate::ReplaceClient()
{
	dbg_msg("autoupdate", "Replacing " PLAT_CLIENT_EXEC);

	//Replace running executable by renaming twice...
	m_pStorage->RemoveFile("DDNet.old", 2);
	m_pStorage->RenameFile(PLAT_CLIENT_EXEC, "DDNet.old", 2);
	m_pStorage->RenameFile("DDNet.tmp", PLAT_CLIENT_EXEC, 2);
	#if !defined(CONF_FAMILY_WINDOWS)
		if (system("chmod +x " PLAT_CLIENT_EXEC))
			dbg_msg("autoupdate", "Error setting client executable bit");
	#endif
}

void CAutoUpdate::ReplaceServer()
{
	dbg_msg("autoupdate", "Replacing " PLAT_SERVER_EXEC);

	//Replace running executable by renaming twice...
	m_pStorage->RemoveFile("DDNet-Server.old", 2);
	m_pStorage->RenameFile(PLAT_SERVER_EXEC, "DDNet-Server.old", 2);
	m_pStorage->RenameFile("DDNet-Server.tmp", PLAT_SERVER_EXEC, 2);
	#if !defined(CONF_FAMILY_WINDOWS)
		if (system("chmod +x " PLAT_SERVER_EXEC))
			dbg_msg("autoupdate", "Error setting server executable bit");
	#endif
}

void CAutoUpdate::ParseUpdate()
{
	IOHANDLE File = m_pStorage->OpenFile("update.json", IOFLAG_READ, IStorage::TYPE_ALL);
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

void CAutoUpdate::InitiateUpdate()
{
	m_State = GETTING_MANIFEST;
	FetchFile("update.json");
}

void CAutoUpdate::IgnoreUpdate()
{
	m_State = IGNORED;
}

void CAutoUpdate::PerformUpdate()
{
	m_State = PARSING_UPDATE;
	dbg_msg("autoupdate", "Parsing update.json");
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
		m_pStorage->RemoveFile(m_RemovedFiles.back().c_str(), IStorage::TYPE_SAVE);
		m_RemovedFiles.pop_back();
	}
	if(m_ServerUpdate)
		FetchFile(PLAT_SERVER_DOWN, "DDNet-Server.tmp");
	if(m_ClientUpdate)
		FetchFile(PLAT_CLIENT_DOWN, "DDNet.tmp");
}
