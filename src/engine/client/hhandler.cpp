#include <engine/engine.h>
#include <engine/hmasterserver.h>
#include <engine/storage.h>

#include <engine/external/json/json.h>

#include "hhandler.h"

void CHHandler::Init(IEngine *pEngine, IHMasterServer *pHMasterServer, IStorage *pStorage)
{
    m_pEngine = pEngine;
    m_pHMasterServer = pHMasterServer;
    m_pStorage = pStorage;
}

void CHHandler::Update()
{
    if(!m_pServerlistTask || m_pServerlistTask->State() == HTTP_RUNNING || m_pServerlistTask->State() == HTTP_QUEUED)
        return;

    if(m_pServerlistTask->State() == HTTP_DONE)
    {
        IOHANDLE File = m_pStorage->OpenFile(SERVERLIST_TMP, IOFLAG_READ, IStorage::TYPE_SAVE);
        if(File)
        {
            if(ReadServerList(File, m_pfnCallback, m_pCbUser))
                m_pStorage->RenameFile(SERVERLIST_TMP, SERVERLIST, IStorage::TYPE_SAVE);
            else // TODO: Mark the masterserver broken here
                dbg_msg("hhandler", "masterserver %d sent a broken serverlist.json", m_MasterId);

            io_close(File);
        }
        else
        {
            dbg_msg("hhandler", "received serverlist not readable");
        }
    }
    else if(m_pServerlistTask->State() == HTTP_ERROR)
    {
        dbg_msg("hhandler", "masterserver %d failed to respond", m_MasterId);
        m_pHMasterServer->SetState(m_MasterId, CMasterInfo::STATE_ERROR);
        m_MasterId = (m_MasterId + 1) % IHMasterServer::MAX_MASTERSERVERS;
    }

    m_pServerlistTask = nullptr;
    m_pfnCallback = nullptr;
    m_pCbUser = nullptr;
}

int CHHandler::GetServerList(FServerListCb pfnCb, void *pUser)
{
    if(m_pServerlistTask)
        return STATE_BUSY;

    const CMasterInfo *pMaster = m_pHMasterServer->Get(m_MasterId);
    char aBuf[256];
    pMaster->GetEndpoint(aBuf, sizeof(aBuf), "servers");

    m_pfnCallback = pfnCb;
    m_pCbUser = pUser;
    m_pServerlistTask = std::make_shared<CGetFile>(m_pStorage, aBuf, SERVERLIST_TMP, IStorage::TYPE_SAVE);
    m_pEngine->AddJob(m_pServerlistTask);

    return 0;
}

bool CHHandler::ReadServerList(IOHANDLE File, FServerListCb pfnCallback, void *pCbUser)
{
    if(!File)
        return false;

    int Length = io_length(File);
    char *pBuf = (char *)malloc(Length);

    if(Length <= 0 || io_read(File, pBuf, Length) <= 0)
        return false; // Degenerate file

    json_value Data = *json_parse(pBuf, Length);
    if(Data.type != json_array)
        return false;

    for(int i = 0; i < json_array_length(&Data); i++)
    {
        json_value Server = Data[i];

        NETADDR Addr;
        if(Server["ip"].type != json_string || net_addr_from_str(&Addr, Server["ip"]))
            return false;

        if(Server["port"].type != json_integer || (int)Server["port"] < 0 || (int)Server["port"] > 65536)
            return false;
        Addr.port = (int)Server["port"];

        CServerInfo Info = {};
        if(Server["info"].type != json_object || !Info.FromJson(Addr, Server["info"]))
            return false;

        pfnCallback(&Addr, &Info, pCbUser);
    }

    return true;
}
