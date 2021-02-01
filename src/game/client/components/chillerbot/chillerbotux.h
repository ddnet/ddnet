#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H

#include <game/client/component.h>

class CChillerBotUX : public CComponent
{
	bool m_IsNearFinish;
	char m_aGreetName[32];
	char m_aLastPingName[32];
	int64 m_LastGreet;
	int64 m_AfkTill;
	char m_aAfkMessage[32];
	int m_AfkActivity;
	char m_aLastAfkPing[2048];
	int m_CampHackX1;
	int m_CampHackY1;
	int m_CampHackX2;
	int m_CampHackY2;
	int m_CampClick;
	int m_ForceDir;
	int m_LastForceDir;

	void OnChatMessage(int ClientID, int Team, const char *pMsg);
	bool LineShouldHighlight(const char *pLine, const char *pName);
	bool IsGreeting(const char *pMsg);
	void Get128Name(const char *pMsg, char *pName);
	void DoGreet();
	void SayFormat(const char *pMsg);
	void GoAfk(int Minutes, const char *pMsg = 0);
	void FinishRenameTick();
	void CampHackTick();
	void SelectCampArea(int Key);
	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom);

	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnConsoleInit();
	virtual void OnInit();
	virtual bool OnMouseMove(float x, float y);
	virtual bool OnInput(IInput::CEvent e);

	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConSayFormat(IConsole::IResult *pResult, void *pUserData);
	static void ConAfk(IConsole::IResult *pResult, void *pUserData);

	static void ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	void ReturnFromAfk(const char *pChatMessage = 0);
};

#endif
