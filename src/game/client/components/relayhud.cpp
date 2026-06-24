/* (c) yirou. Relay mode HUD for live streaming */
#include "relayhud.h"

#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/skins.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <generated/protocol.h>

CRelayHud::CRelayHud()
{
	m_TeamCount = 0;
	m_SplitScreenMode = false;
}

void CRelayHud::OnInit()
{
	for(int i = 0; i < 2; i++)
		m_aTeams[i].Reset();
}

void CRelayHud::OnConsoleInit()
{
	Console()->Register("cl_relay_splitscreen", "", CFGFLAG_CLIENT, ConToggleSplitScreen, this, "Toggle relay split screen mode");
}

void CRelayHud::ConToggleSplitScreen(IConsole::IResult* pResult, void* pUserData)
{
	CRelayHud* pSelf = (CRelayHud*)pUserData;
	pSelf->m_SplitScreenMode = !pSelf->m_SplitScreenMode;
}

void CRelayHud::OnMessage(int MsgType, void* pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_RELAYSTATE)
	{
		CNetMsg_Sv_RelayState* pMsg = (CNetMsg_Sv_RelayState*)pRawMsg;
		
		// Find or create team slot
		int TeamIdx = -1;
		for(int i = 0; i < 2; i++)
		{
			if(m_aTeams[i].m_Team == pMsg->m_Team)
			{
				TeamIdx = i;
				break;
			}
		}
		if(TeamIdx == -1)
		{
			// Find empty slot
			for(int i = 0; i < 2; i++)
			{
				if(m_aTeams[i].m_Team == -1)
				{
					TeamIdx = i;
					m_TeamCount++;
					break;
				}
			}
		}
		
		if(TeamIdx != -1)
		{
			SRelayTeamState& State = m_aTeams[TeamIdx];
			State.m_Team = pMsg->m_Team;
			State.m_State = pMsg->m_State;
			State.m_CurrentRunnerOrder = pMsg->m_CurrentRunnerOrder;
			State.m_RunnerCount = pMsg->m_RunnerCount;
			State.m_DurationSec = pMsg->m_DurationSec;
			State.m_ElapsedTick = pMsg->m_ElapsedTick;
			State.m_LastUpdateTime = time_get();
			
			for(int i = 0; i < 16; i++)
			{
				State.m_aRunnerClientIds[i] = pMsg->m_aRunnerClientIds[i];
				State.m_aRunnerOrders[i] = pMsg->m_aRunnerOrders[i];
			}
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RELAYACTION)
	{
		CNetMsg_Sv_RelayAction* pMsg = (CNetMsg_Sv_RelayAction*)pRawMsg;
		
		// Find team
		for(int i = 0; i < 2; i++)
		{
			if(m_aTeams[i].m_Team == pMsg->m_Team)
			{
				TriggerActionFlash(i, pMsg->m_ClientId);
				break;
			}
		}
	}
}

void CRelayHud::TriggerActionFlash(int TeamIdx, int ClientId)
{
	if(TeamIdx < 0 || TeamIdx >= 2)
		return;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	
	m_aTeams[TeamIdx].m_ActionFlashEndTime[ClientId] = time_get() + time_freq() * 1.0f; // Flash for 1 second
}

vec2 CRelayHud::GetTeamCameraCenter(int TeamIdx) const
{
	if(TeamIdx < 0 || TeamIdx >= 2)
		return vec2(0, 0);
	
	const SRelayTeamState& State = m_aTeams[TeamIdx];
	if(State.m_CurrentRunnerOrder <= 0)
		return vec2(0, 0);
	
	// Find current runner
	int CurrentRunnerId = -1;
	for(int i = 0; i < 16; i++)
	{
		if(State.m_aRunnerOrders[i] == State.m_CurrentRunnerOrder)
		{
			CurrentRunnerId = State.m_aRunnerClientIds[i];
			break;
		}
	}
	
	if(CurrentRunnerId != -1 && m_pClient->m_Snap.m_aCharacters[CurrentRunnerId].m_Active)
	{
		return m_pClient->m_Snap.m_aCharacters[CurrentRunnerId].m_Position;
	}
	
	return vec2(0, 0);
}

float CRelayHud::CalculateTeamZoom(int TeamIdx) const
{
	if(TeamIdx < 0 || TeamIdx >= 2)
		return 1.0f;
	
	const SRelayTeamState& State = m_aTeams[TeamIdx];
	if(State.m_State != RELAY_STATE_RUNNING && State.m_State != RELAY_STATE_COUNTDOWN)
		return 1.0f;
	
	// Default zoom for single team view
	return 1.0f;
}

void CRelayHud::RenderRunnerTee(int ClientId, float X, float Y, float Size, bool FlashRed)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	
	const CGameClient::CClientData* pClientData = &m_pClient->m_aClients[ClientId];
	if(!pClientData->m_Active)
		return;
	
	CTeeRenderInfo TeeInfo = pClientData->m_RenderInfo;
	if(FlashRed)
	{
		// Tint red
		TeeInfo.m_ColorBody.r = 1.0f;
		TeeInfo.m_ColorBody.g = 0.0f;
		TeeInfo.m_ColorBody.b = 0.0f;
	}
	
	vec2 Pos(X, Y);
	vec2 Direction(1, 0);
	float WalkTime = 0;
	
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, Direction, Pos, Size);
}

void CRelayHud::RenderTeam(const SRelayTeamState& State, float X, float Y, float Width, bool RightAligned)
{
	if(State.m_Team == -1)
		return;
	
	// Background panel
	float Height = 120.0f;
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.7f);
	IGraphics::CQuadItem QuadItem(X, Y, Width, Height);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	
	// Title
	char aTitle[64];
	const char* pStateStr = "Unknown";
	switch(State.m_State)
	{
		case RELAY_STATE_IDLE: pStateStr = "Idle"; break;
		case RELAY_STATE_RESETTING: pStateStr = "Resetting"; break;
		case RELAY_STATE_COUNTDOWN: pStateStr = "Countdown"; break;
		case RELAY_STATE_RUNNING: pStateStr = "Running"; break;
		case RELAY_STATE_FINISHED: pStateStr = "Finished"; break;
	}
	str_format(aTitle, sizeof(aTitle), "Team %d - %s", State.m_Team, pStateStr);
	
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->Text(X + 10, Y + 5, 14, aTitle);
	
	// Runner list
	float TeeSize = 24.0f;
	float TeeSpacing = 30.0f;
	float StartX = X + 10;
	float StartY = Y + 30;
	
	for(int i = 0; i < 16 && i < State.m_RunnerCount; i++)
	{
		int RunnerId = State.m_aRunnerClientIds[i];
		int Order = State.m_aRunnerOrders[i];
		if(RunnerId == -1)
			continue;
		
		float PosX = StartX + (i % 8) * TeeSpacing;
		float PosY = StartY + (i / 8) * (TeeSize + 20);
		
		// Check if current runner
		bool IsCurrent = (Order == State.m_CurrentRunnerOrder);
		bool FlashRed = State.m_ActionFlashEndTime[RunnerId] > time_get();
		
		// Render tee
		RenderRunnerTee(RunnerId, PosX + TeeSize/2, PosY + TeeSize/2, TeeSize, FlashRed);
		
		// Highlight current runner
		if(IsCurrent)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1, 1, 0, 0.5f); // Yellow highlight
			IGraphics::CQuadItem HighlightQuad(PosX - 2, PosY - 2, TeeSize + 4, TeeSize + 4);
			Graphics()->QuadsDrawTL(&HighlightQuad, 1);
			Graphics()->QuadsEnd();
		}
		
		// Order number
		char aOrder[4];
		str_format(aOrder, sizeof(aOrder), "%d", Order);
		TextRender()->TextColor(1, 1, 1, 1);
		TextRender()->Text(PosX, PosY + TeeSize + 2, 8, aOrder);
	}
	
	// Timer info for running state
	if(State.m_State == RELAY_STATE_RUNNING)
	{
		int RemainingSec = State.m_DurationSec - (State.m_ElapsedTick / GameClient()->Client()->TickSpeed());
		if(RemainingSec < 0) RemainingSec = 0;
		
		char aTimer[32];
		str_format(aTimer, sizeof(aTimer), "%d:%02d", RemainingSec / 60, RemainingSec % 60);
		
		TextRender()->TextColor(1, RemainingSec <= 3 ? 0 : 1, RemainingSec <= 3 ? 0 : 1, 1);
		TextRender()->Text(X + Width - 60, Y + 5, 16, aTimer);
	}
}

void CRelayHud::RenderSplitScreen()
{
	if(m_TeamCount < 2)
		return;
	
	CCamera* pCamera = &GameClient()->m_Camera;
	
	// Save original camera
	vec2 OriginalCenter = pCamera->m_Center;
	float OriginalZoom = pCamera->m_Zoom;
	
	// This is a simplified version - full implementation would require
	// re-rendering the world twice with different viewports
	// For now, we just position the camera between the two teams
	
	vec2 CenterA = GetTeamCameraCenter(0);
	vec2 CenterB = GetTeamCameraCenter(1);
	
	if(CenterA != vec2(0, 0) && CenterB != vec2(0, 0))
	{
		// Position camera to show both teams
		vec2 Midpoint = (CenterA + CenterB) / 2.0f;
		float Distance = distance(CenterA, CenterB);
		
		// Adjust zoom to fit both
		float RequiredZoom = Distance / 800.0f; // Approximate
		RequiredZoom = clamp(RequiredZoom, 0.5f, 2.0f);
		
		pCamera->m_Center = Midpoint;
		pCamera->m_Zoom = RequiredZoom;
	}
	else if(CenterA != vec2(0, 0))
	{
		pCamera->m_Center = CenterA;
	}
	else if(CenterB != vec2(0, 0))
	{
		pCamera->m_Center = CenterB;
	}
}

void CRelayHud::OnRender()
{
	// Handle split screen mode camera
	if(m_SplitScreenMode && m_TeamCount >= 2)
	{
		RenderSplitScreen();
	}
	
	// Render HUD panels
	float ScreenWidth = Graphics()->ScreenWidth();
	float PanelWidth = 280.0f;
	float PanelHeight = 120.0f;
	float Margin = 10.0f;
	
	// Team A - Top Left
	if(m_aTeams[0].m_Team != -1)
	{
		RenderTeam(m_aTeams[0], Margin, Margin, PanelWidth, false);
	}
	
	// Team B - Top Right
	if(m_aTeams[1].m_Team != -1)
	{
		RenderTeam(m_aTeams[1], ScreenWidth - PanelWidth - Margin, Margin, PanelWidth, true);
	}
}
