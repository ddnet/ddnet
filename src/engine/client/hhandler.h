#ifndef ENGINE_CLIENT_HHANDLER_H
#define ENGINE_CLIENT_HHANDLER_H

#include <memory>

#include <engine/shared/http.h>
#include <engine/shared/serverinfo.h>
#include <engine/hmasterserver.h>

#define SERVERLIST "serverlist.json"
#define SERVERLIST_TMP SERVERLIST ".tmp"

typedef void (*FServerListCb)(const NETADDR *pAddr, const CServerInfo *pInfo, void *pUser);

class CHHandler
{
    enum {
        STATE_IDLE = 0,
        STATE_BUSY
    };

    int m_MasterId;
    std::shared_ptr<CGetFile> m_pServerlistTask;
    FServerListCb m_pfnCallback;
    void *m_pCbUser;

    class IEngine *m_pEngine;
    IHMasterServer *m_pHMasterServer;
    class IStorage *m_pStorage;

public:
    CHHandler() : m_MasterId(0), m_pServerlistTask(nullptr), m_pfnCallback(nullptr), m_pCbUser(nullptr) {};

    void Init(class IEngine *pEngine, IHMasterServer *pHMasterServer, class IStorage *pStorage);
    void Update();
    int GetServerList(FServerListCb pfnCb, void *pUser);

    static bool ReadServerList(IOHANDLE File, FServerListCb pfnCallback, void *pCbUser);
};

#endif // ENGINE_CLIENT_HHANDLER_H
