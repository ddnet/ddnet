/* (c) yirou. Relay mode HUD for live streaming */
#ifndef GAME_CLIENT_COMPONENTS_RELAYHUD_H
#define GAME_CLIENT_COMPONENTS_RELAYHUD_H

#include <game/client/component.h>
#include <engine/shared/protocol.h>

// Relay state enum matching server
enum ETeamRelayState
{
	RELAY_STATE_IDLE = 0,
	RELAY_STATE_RESETTING,
	RELAY_STATE_COUNTDOWN,
	RELAY_STATE_RUNNING,
	RELAY_STATE_FINISHED,
};

struct SRelayTeamState
{
	int m_Team;
	int m_State;
	int m_CurrentRunnerOrder;
	int m_RunnerCount;
	int m_DurationSec;
	int m_ElapsedTick;
	int m_aRunnerClientIds[16];
	int m_aRunnerOrders[16];
	
	// Client-side visual data
	float m_ActionFlashEndTime[MAX_CLIENTS]; // Flash red when player uses /b or /r
	float m_LastUpdateTime;
	
	void Reset()
	{
		m_Team = -1;
		m_State = RELAY_STATE_IDLE;
		m_CurrentRunnerOrder = 0;
		m_RunnerCount = 0;
		m_DurationSec = 5;
		m_ElapsedTick = 0;
		memset(m_aRunnerClientIds, -1, sizeof(m_aRunnerClientIds));
		memset(m_aRunnerOrders, 0, sizeof(m_aRunnerOrders));
		memset(m_ActionFlashEndTime, 0, sizeof(m_ActionFlashEndTime));
		m_LastUpdateTime = 0;
	}
	
	SRelayTeamState() { Reset(); }
};

class CRelayHud : public CComponent
{
	SRelayTeamState m_aTeams[2]; // Track up to 2 relay teams
	int m_TeamCount;
	bool m_SplitScreenMode;
	
	void RenderTeam(const SRelayTeamState& State, float X, float Y, float Width, bool RightAligned);
	void RenderRunnerTee(int ClientId, float X, float Y, float Size, bool FlashRed);
	void RenderSplitScreen();
	vec2 GetTeamCameraCenter(int TeamIdx) const;
	float CalculateTeamZoom(int TeamIdx) const;
	
public:
	CRelayHud();
	int Sizeof() const override { return sizeof(*this); }
	
	void OnInit() override;
	void OnRender() override;
	void OnMessage(int MsgType, void* pRawMsg) override;
	void OnConsoleInit() override;
	
	// Actions
	void TriggerActionFlash(int TeamIdx, int ClientId);
	
	// Config
	static void ConToggleSplitScreen(IConsole::IResult* pResult, void* pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_RELAYHUD_H
