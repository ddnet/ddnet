#include <engine/engine.h>
#include <engine/hmasterserver.h>
#include <engine/external/json/json-builder.h>

#include "hregister.h"

void CHRegister::Init(IEngine *pEngine, IHMasterServer *pHMasterServer)
{
    m_pEngine = pEngine;
    m_pHMasterServer = pHMasterServer;
}

void CHRegister::Update()
{
    int64 Now = time_get();
    int64 Freq = time_freq();

    for(int i = 0; i < IHMasterServer::MAX_MASTERSERVERS; i++)
    {
        CMaster *pMaster = &m_aMasters[i];
        const CMasterInfo *pMasterInfo = m_pHMasterServer->Get(i);

        // Finish up the task
        if(pMaster->m_pRegisterTask)
        {
            switch(pMaster->m_pRegisterTask->State())
            {
            case HTTP_DONE:
                pMaster->m_LastUpdate = Now;
            default:
                pMaster->m_pRegisterTask = nullptr;
            }
            continue;
        }

        // Update only if we need to update
        if(pMaster->m_LastUpdate + 15 * Freq < Now)
            continue;

        json_value Info = m_Info.ToJson();
        char *pBuf = (char *)malloc(json_measure(&Info));
        json_serialize(pBuf, &Info);

        char aUrl[256];
        pMasterInfo->GetEndpoint(aUrl, sizeof(aUrl), "servers");

        pMaster->m_pRegisterTask = std::make_shared<CPostJson>(aUrl, true, pBuf);
        free(pBuf);
        m_pEngine->AddJob(pMaster->m_pRegisterTask);
    }
}

void CHRegister::UpdateServerInfo(const CServerInfo *pInfo)
{
    // TODO: Might want to do more here like calculating the json
    m_Info = *pInfo;
}
