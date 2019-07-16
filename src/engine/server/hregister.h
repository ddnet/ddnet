#ifndef ENGINE_SERVER_HREGISTER_H
#define ENGINE_SERVER_HREGISTER_H

#include <memory>

#include <engine/shared/http.h>
#include <engine/shared/serverinfo.h>

#include <engine/engine.h>
#include <engine/hmasterserver.h>

class CHRegister
{
    struct CMaster
    {
        int64 m_LastUpdate;
        std::shared_ptr<CPostJson> m_pRegisterTask;
    };
    CMaster m_aMasters[IHMasterServer::MAX_MASTERSERVERS];

    CServerInfo m_Info;

    class IEngine *m_pEngine;
    IHMasterServer *m_pHMasterServer;

public:
    CHRegister() :  m_Info{}, m_pEngine(nullptr), m_pHMasterServer(nullptr)
    {
        mem_zero(m_aMasters, sizeof(m_aMasters));
    }

    void Init(class IEngine *pEngine, IHMasterServer *pHMasterServer);
    void Update();
    void UpdateServerInfo(const CServerInfo *pInfo);

};

#endif // ENGINE_SERVER_HREGISTER_H
