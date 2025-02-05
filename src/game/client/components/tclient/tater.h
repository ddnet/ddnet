#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TATER_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TATER_H
#include <game/client/component.h>
#include <engine/shared/http.h>

class CTater : public CComponent
{
	static void ConRandomTee(IConsole::IResult *pResult, void *pUserData);
	static void ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void RandomBodyColor();
	static void RandomFeetColor();
	static void RandomSkin(void *pUserData);
	static void RandomFlag(void *pUserData);

	class IEngineGraphics *m_pGraphics = nullptr;

	char m_PreviousOwnMessage[2048] = {};

	bool SendNonDuplicateMessage(int Team, const char *pLine);


public:
	CTater();
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnConsoleInit() override;
	void OnRender() override;

	std::shared_ptr<CHttpRequest> m_pTClientInfoTask = nullptr;
	void FetchTClientInfo();
	void LoadTClientInfoJson();
	void FinishTClientInfo();
	void ResetTClientInfoTask();
	bool NeedUpdate();

	char m_aVersionStr[10] = "0";
};

#endif
