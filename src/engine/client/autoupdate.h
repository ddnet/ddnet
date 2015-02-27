#ifndef ENGINE_CLIENT_AUTOUPDATE_H
#define ENGINE_CLIENT_AUTOUPDATE_H

#include <engine/autoupdate.h>
#include <engine/fetcher.h>
#include <vector>
#include <string>

class CAutoUpdate : public IAutoUpdate
{
    class IClient *m_pClient;
    class IStorage *m_pStorage;
    class IFetcher *m_pFetcher;

    int m_State;
    char m_Status[256];
    int m_Percent;
    char m_aLastFile[256];

    bool m_ClientUpdate;
    bool m_ServerUpdate;

    std::vector<std::string> m_AddedFiles;
    std::vector<std::string> m_RemovedFiles;

    void AddNewFile(const char *pFile);
    void AddRemovedFile(const char *pFile);
    void FetchFile(const char *pFile, const char *pDestPath = 0);

    void ParseUpdate();
    void PerformUpdate();
    void ReplaceExecutable();

public: 
    CAutoUpdate();
    static void ProgressCallback(CFetchTask *pTask, void *pUser);
    static void CompletionCallback(CFetchTask *pTask, void *pUser);

    int GetCurrentState() { return m_State; };
    char *GetCurrentFile() { return m_Status; };
    int GetCurrentPercent() { return m_Percent; };

    virtual void InitiateUpdate();
    void IgnoreUpdate();   
    void Init();
    virtual void Update();
    void Restart();
};

#endif