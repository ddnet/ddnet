#ifndef ENGINE_CLIENT_UPDATER_H
#define ENGINE_CLIENT_UPDATER_H

#include <base/detect.h>
#include <base/lock.h>

#include <engine/updater.h>

#include <forward_list>
#include <memory>
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
#else
#define PLAT_NAME CONF_PLATFORM_STRING "-unsupported"
#endif
#else
#if defined(AUTOUPDATE)
#error Compiling with autoupdater on an unsupported platform
#endif
#define PLAT_EXT ""
#define PLAT_NAME "unsupported-unsupported"
#endif

#define PLAT_CLIENT_DOWN CLIENT_EXEC "-" PLAT_NAME PLAT_EXT
#define PLAT_SERVER_DOWN SERVER_EXEC "-" PLAT_NAME PLAT_EXT

#define PLAT_CLIENT_EXEC CLIENT_EXEC PLAT_EXT
#define PLAT_SERVER_EXEC SERVER_EXEC PLAT_EXT

class CUpdaterFetchTask;

class CUpdater : public IUpdater
{
	friend class CUpdaterFetchTask;

	class IClient *m_pClient;
	class IStorage *m_pStorage;
	class IEngine *m_pEngine;
	class CHttp *m_pHttp;

	CLock m_Lock;

	EUpdaterState m_State GUARDED_BY(m_Lock);
	char m_aStatus[256] GUARDED_BY(m_Lock);
	int m_Percent GUARDED_BY(m_Lock);
	char m_aClientExecTmp[64];
	char m_aServerExecTmp[64];

	std::forward_list<std::pair<std::string, bool>> m_FileJobs;
	std::shared_ptr<CUpdaterFetchTask> m_pCurrentTask;
	decltype(m_FileJobs)::iterator m_CurrentJob;

	bool m_ClientUpdate;
	bool m_ServerUpdate;

	bool m_ClientFetched;
	bool m_ServerFetched;

	void AddFileJob(const char *pFile, bool Job);
	void FetchFile(const char *pFile, const char *pDestPath = nullptr) REQUIRES(!m_Lock);
	bool MoveFile(const char *pFile);

	void ParseUpdate() REQUIRES(!m_Lock);
	void PerformUpdate() REQUIRES(!m_Lock);
	void RunningUpdate() REQUIRES(!m_Lock);
	void CommitUpdate() REQUIRES(!m_Lock);

	bool ReplaceClient();
	bool ReplaceServer();

	void SetCurrentState(EUpdaterState NewState) REQUIRES(!m_Lock);

public:
	CUpdater();

	EUpdaterState GetCurrentState() override REQUIRES(!m_Lock);
	void GetCurrentFile(char *pBuf, int BufSize) override REQUIRES(!m_Lock);
	int GetCurrentPercent() override REQUIRES(!m_Lock);

	void InitiateUpdate() REQUIRES(!m_Lock) override;
	void Init(CHttp *pHttp);
	void Update() REQUIRES(!m_Lock) override;
};

#endif
