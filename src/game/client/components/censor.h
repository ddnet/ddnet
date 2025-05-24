#ifndef GAME_CLIENT_COMPONENTS_CENSOR_H
#define GAME_CLIENT_COMPONENTS_CENSOR_H
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <base/lock.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/jobs.h>

#include <game/client/component.h>

class CCensor : public CComponent
{
private:
	std::vector<std::string> m_vCensoredWords;

	class CBlacklistDownloadJob : public IJob
	{
	public:
		CBlacklistDownloadJob(CCensor *pCensor, const char *pUrl, const char *pSaveFilePath);

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

	std::shared_ptr<CBlacklistDownloadJob> m_pBlackListDownloadJob = nullptr;

public:
	CCensor();
	int Sizeof() const override { return sizeof(*this); }

	void Reset();
	void OnInit() override;
	void OnRender() override;

	std::optional<std::vector<std::string>> LoadCensorList(const char *pFilePath) const;
	void CensorMessage(char *pMessage) const;
};

#endif // GAME_CLIENT_COMPONENTS_CENSOR_H
