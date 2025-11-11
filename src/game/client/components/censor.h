#ifndef GAME_CLIENT_COMPONENTS_CENSOR_H
#define GAME_CLIENT_COMPONENTS_CENSOR_H
/*
#include <base/lock.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/jobs.h>

#include <game/client/component.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class CCensor : public CComponent
{
private:
	std::vector<std::string> m_vCensoredWords;

	class CCensorListDownloadJob : public IJob
	{
	public:
		CCensorListDownloadJob(CCensor *pCensor, const char *pUrl, const char *pSaveFilePath);

		bool Abort() override REQUIRES(!m_Lock);

		std::optional<std::vector<std::string>> m_vLoadedWords;

	protected:
		void Run() override REQUIRES(!m_Lock);

	private:
		CCensor *m_pCensor;
		char m_aUrl[IO_MAX_PATH_LENGTH];
		char m_aSaveFilePath[IO_MAX_PATH_LENGTH];
		CLock m_Lock;
		std::shared_ptr<CHttpRequest> m_pGetRequest;
	};

	std::shared_ptr<CCensorListDownloadJob> m_pCensorListDownloadJob = nullptr;

	static void ConchainRefreshCensorList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	CCensor();
	int Sizeof() const override { return sizeof(*this); }

	void Reset();
	void OnInit() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnShutdown() override;

	std::optional<std::vector<std::string>> LoadCensorListFromFile(const char *pFilePath) const;
	std::optional<std::vector<std::string>> LoadCensorList(const void *pListText, size_t ListTextLen) const;
	void CensorMessage(char *pMessage) const;
};
*/

#include <game/client/component.h>

class CCensor : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
};

#endif // GAME_CLIENT_COMPONENTS_CENSOR_H
