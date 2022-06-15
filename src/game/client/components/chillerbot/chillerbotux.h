#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H

#include <game/client/component.h>
#include <game/client/render.h>
#include <game/mapitems.h>

#include <engine/console.h>

#include <engine/shared/http.h>

#define MAX_COMPONENT_LEN 16
#define MAX_COMPONENTS_ENABLED 8

class CChillerBotUX : public CComponent
{
	class CChatHelper *m_pChatHelper;

	enum
	{
		STATE_DONE,
		STATE_WANTREFRESH,
		STATE_REFRESHING,
	};
	int m_HeartbeatState;
	std::shared_ptr<CHttpRequest> m_pAliveGet;
	int64_t m_NextHeartbeat;
	int64_t m_AfkTill;

	bool m_IsNearFinish;

	char m_aAfkMessage[32];
	char m_aLastAfkPing[2048];
	char m_aLastKiller[2][32];
	char m_aLastKillerTime[2][32];

	struct CUiComponent
	{
		char m_aName[MAX_COMPONENT_LEN];
		char m_aNoteShort[16];
		char m_aNoteLong[2048];
	};
	CUiComponent m_aEnabledComponents[MAX_COMPONENTS_ENABLED];

	struct CKillMsg
	{
		int m_Weapon;

		int m_VictimID;
		int m_VictimTeam;
		int m_VictimDDTeam;
		char m_aVictimName[64];
		CTeeRenderInfo m_VictimRenderInfo;

		int m_KillerID;
		int m_KillerTeam;
		char m_aKillerName[64];
		CTeeRenderInfo m_KillerRenderInfo;

		int m_ModeSpecial; // for CTF, if the guy is carrying a flag for example
		int m_Tick;
		int m_FlagCarrierBlue;
	};

	int m_AfkActivity;
	int m_CampHackX1;
	int m_CampHackY1;
	int m_CampHackX2;
	int m_CampHackY2;
	int m_CampClick;
	int m_ForceDir;
	int m_LastForceDir;
	int m_GotoSwitchOffset;
	int m_GotoSwitchLastX;
	int m_GotoSwitchLastY;
	int m_GotoTeleOffset;
	int m_GotoTeleLastX;
	int m_GotoTeleLastY;
	int64_t m_LastNotification;

	// broadcasts
	char m_aBroadcastText[1024];
	int m_BroadcastTick;
	bool m_IsLeftSidedBroadcast;

	void OnChatMessage(int ClientID, int Team, const char *pMsg);
	void GoAfk(int Minutes, const char *pMsg = 0);
	void ChangeTileNotifyTick();
	void FinishRenameTick();
	void CampHackTick();
	void CheckEmptyTick();
	void SelectCampArea(int Key);
	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom);
	void RenderSpeedHud();
	void RenderWeaponHud();
	void RenderEnabledComponents();
	void GotoSwitch(int Number, int Offset = -1);
	void GotoTele(int Number, int Offset = -1);
	void TraceSpikes();
	void DumpPlayers(const char *pSearch = 0);

	// helpers
	int CountOnlinePlayers();

	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	virtual bool OnInput(IInput::CEvent e) override;

	static void ConAfk(IConsole::IResult *pResult, void *pUserData);
	static void ConCampHack(IConsole::IResult *pResult, void *pUserData);
	static void ConCampHackAbs(IConsole::IResult *pResult, void *pUserData);
	static void ConUnCampHack(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoSwitch(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoTele(IConsole::IResult *pResult, void *pUserData);
	static void ConLoadMap(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpPlayers(IConsole::IResult *pResult, void *pUserData);
	static void ConForceQuit(IConsole::IResult *pResult, void *pUserData);
	static void ConSetGametile(IConsole::IResult *pResult, void *pUserData);

	static void ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChillerbotHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWeaponHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAutoReply(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainFinishRename(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainShowLastKiller(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainShowLastPing(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	int m_IgnoreChatAfk;

	void ReturnFromAfk(const char *pChatMessage = 0);
	int64_t GetAfkTime() { return m_AfkTill; }
	const char *GetAfkMessage() { return m_aAfkMessage; }
	int GetAfkActivity() { return m_AfkActivity; }
	bool IsAfk() { return GetAfkTime() && GetAfkActivity() < 25; }

	void EnableComponent(const char *pComponent, const char *pNoteShort = 0, const char *pNoteLong = 0);
	void DisableComponent(const char *pComponent);
	bool SetComponentNoteShort(const char *pComponent, const char *pNoteShort = 0);
	bool SetComponentNoteLong(const char *pComponent, const char *pNoteLong = 0);
	void UpdateComponents();
};

#endif
