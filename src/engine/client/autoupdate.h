#ifndef ENGINE_CLIENT_AUTOUPDATE_H
#define ENGINE_CLIENT_AUTOUPDATE_H

#include <engine/autoupdate.h>
#include <engine/fetcher.h>
#include <vector>
#include <string>

#define CLIENT_EXEC "DDNet"
#define SERVER_EXEC "DDNet-Server"

#if defined(CONF_FAMILY_WINDOWS)
	#define PLAT_EXT ".exe"
	#define PLAT_NAME CONF_PLATFORM_STRING
#elif defined(CONF_FAMILY_UNIX)
	#define PLAT_EXT ""
	#if defined(CONF_ARCH_IA32)
		#define PLAT_NAME CONF_PLATFORM_STRING "-x86"
	#elif defined(CONF_ARCH_AMD64)
		#define PLAT_NAME CONF_PLATFORM_STRING "-x86_64"
	#endif
#endif

#define PLAT_CLIENT_DOWN CLIENT_EXEC "-" PLAT_NAME PLAT_EXT
#define PLAT_SERVER_DOWN SERVER_EXEC "-" PLAT_NAME PLAT_EXT

#define PLAT_CLIENT_EXEC CLIENT_EXEC PLAT_EXT
#define PLAT_SERVER_EXEC SERVER_EXEC PLAT_EXT

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

	void ReplaceClient();
	void ReplaceServer();

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
