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

    for(int i = 0; i < m_pHMasterServer->MasterCount(); i++)
    {
        CMaster *pMaster = &m_aMasters[i];
        const CMasterInfo *pMasterInfo = m_pHMasterServer->Get(i);

        if(pMasterInfo->m_State == CMasterInfo::STATE_ERROR)
            continue;

        if(!pMaster->m_aSecret[0])
        {
            secure_random_password(pMaster->m_aSecret, sizeof(pMaster->m_aSecret), SECRET_LENGTH - 1);
        }

        if(pMaster->m_pRegisterTask)
        {
            switch(pMaster->m_pRegisterTask->State())
            {
            case HTTP_ERROR:
                pMaster->m_Failures[pMaster->m_pRegisterTask->isV6()]++;
                if(pMaster->m_Failures[V4] >= 3 && pMaster->m_Failures[V6] >= 3)
                    m_pHMasterServer->SetState(i, CMasterInfo::STATE_ERROR);
                break;
            case HTTP_RUNNING:
                continue;
            }
            pMaster->m_pRegisterTask = nullptr;
        }

        // Update only if we need to update
        if(pMaster->m_LastTry + (5 * Freq) > Now && !m_ForceUpdate)
            continue;

        dbg_msg("hregister", "registering on master %d with %s (b%d %d %d)", i, !(pMaster->m_Beat % 2) ? "ipv6" : "ipv4", pMaster->m_Beat, pMaster->m_Failures[V4], pMaster->m_Failures[V6]);
        json_value *pBeat = json_object_new(0);

        json_object_push(pBeat, "port", json_integer_new(m_Info.m_NetAddr.port));
        json_object_push(pBeat, "secret", json_string_new(pMaster->m_aSecret));
        json_object_push(pBeat, "beat", json_integer_new(pMaster->m_Beat));
        json_object_push(pBeat, "info", m_Info.ToJson());

        char *pBuf = (char *)malloc(json_measure(pBeat));
        json_serialize(pBuf, pBeat);

        // FIX: Probably need to free way more
        json_builder_free(pBeat);

        dbg_msg("JSON", "%s", pBuf);

        char aUrl[256];
        pMasterInfo->GetEndpoint(aUrl, sizeof(aUrl), "servers");

        pMaster->m_pRegisterTask = std::make_shared<CPostJson>(aUrl, true, pBuf, !(pMaster->m_Beat++ % 2) && pMaster->m_Failures[V6] < 3);
        pMaster->m_LastTry = Now;
        m_pEngine->AddJob(pMaster->m_pRegisterTask);

        if(m_ForceUpdate)
        {
            pMaster->m_pRegisterTask = std::make_shared<CPostJson>(aUrl, true, pBuf, !(pMaster->m_Beat++ % 2) && pMaster->m_Failures[V6] < 3);
            pMaster->m_LastTry = Now;
            m_pEngine->AddJob(pMaster->m_pRegisterTask);
            m_ForceUpdate = false;
        }

        free(pBuf);
    }
}

void CHRegister::UpdateServerInfo(const CServerInfo *pInfo)
{
    // TODO: Might want to do more here like calculating the json
    m_Info = *pInfo;
    m_ForceUpdate = true;
}
