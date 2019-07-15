#ifndef ENGINE_SHARED_HMASTERSERVER
#define ENGINE_SHARED_HMASTERSERVER

#include <engine/kernel.h>

class CMasterInfo {
public:
    enum {
        STATE_UNKNOWN = 0,
        STATE_LIVE,
        STATE_ERROR
    };
    int m_State;

    char m_aUrl[128];

    int GetEndpoint(char *aBuf, int BufSize, const char *pEndpoint) const;
};

class IHMasterServer : public IInterface
{
    MACRO_INTERFACE("hmasterserver", 0)
public:
    enum {
        MAX_MASTERSERVERS = 4,
    };

    virtual void Init(class IStorage *pStorage) = 0;
    virtual bool Load() = 0;
    virtual bool Save() = 0;

    virtual const CMasterInfo *Get(int ID) = 0;
    virtual void SetState(int ID, int State) = 0;
};

extern IHMasterServer *CreateHMasterServer();

#endif // ENGINE_SHARED_HMASTERSERVER
