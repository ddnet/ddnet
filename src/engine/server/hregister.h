#ifndef ENGINE_SERVER_HREGISTER_H
#define ENGINE_SERVER_HREGISTER_H

#include <memory>

#include <engine/shared/http.h>
#include <engine/shared/serverinfo.h>

#include <engine/engine.h>
#include <engine/hmasterserver.h>

#define V4 0
#define V6 1
#define SECRET_LENGTH 33

class CHRegister
{
    struct CMaster
    {
        int64 m_LastTry;
        int m_Beat;

        int m_Failures[2];

        char m_aSecret[SECRET_LENGTH + 1];
        std::shared_ptr<CPostJson> m_pRegisterTask;
    };
    CMaster m_aMasters[IHMasterServer::MAX_MASTERSERVERS];

    CServerInfo m_Info;
    bool m_ForceUpdate;

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
