#ifndef ENGINE_HMASTERSERVER_H
#define ENGINE_HMASTERSERVER_H

#include <engine/hmasterserver.h>

#define HTTP_MASTER_VERSION "v1"

class CHMasterServer : public IHMasterServer
{
public:
    CMasterInfo m_aMasterServers[MAX_MASTERSERVERS];

    int m_Count;
    IStorage *m_pStorage;

    CHMasterServer() : m_Count(0), m_pStorage(nullptr)
    {
        mem_zero(m_aMasterServers, sizeof(m_aMasterServers));
    }

    void Init(class IStorage *pStorage);
    bool SetDefaults();
    bool Load();
    bool Save();

    const CMasterInfo *Get(int ID) { return &m_aMasterServers[ID]; };
    void SetState(int ID, int State) { m_aMasterServers[ID].m_State = State; };
};

#endif // ENGINE_HMASTERSERVER_H
