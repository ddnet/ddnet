#include "multiview.h"

#include "camera.h"

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/client/components/spectator.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <limits>

static float MapValue(float MaxValue, float MinValue, float MaxRange, float MinRange, float Value)
{
	return (MaxRange - MinRange) / (MaxValue - MinValue) * (Value - MinValue) + MinRange;
}

CMultiView::CMultiView()
{
	m_Team = 0;
	m_ShowHud = false;
	m_Solo = false;
	m_IsInit = false;
	m_Teleported = false;
	m_OldPos = vec2(0.0f, 0.0f);
	m_OldPersonalZoom = 0;
	m_SecondChance = 0.0f;
	m_OldCameraDistance = 0.0f;
	OnReset();
}

void CMultiView::OnReset()
{
	m_PersonalZoom = 0.0f;
	m_Activated = false;
	m_IsInit = false;
}

void CMultiView::Reset()
{
	GameClient()->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);
	m_PersonalZoom = 0.0f;
	m_Teleported = false;
	m_OldCameraDistance = 0.0f;
	OnReset();
}

void CMultiView::ViewPlayer(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	m_aViewedClients[ClientId].m_IsViewed = true;
}

void CMultiView::ToggleView(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	bool &Id = m_aViewedClients[ClientId].m_IsViewed;
	Id = !Id;
}

void CMultiView::ClearViews()
{
	for(auto &ViewedClient : m_aViewedClients)
	{
		ViewedClient.m_IsViewed = false;
	}
}

bool CMultiView::IsAnyViewed() const
{
	return std::any_of(std::begin(m_aViewedClients), std::end(m_aViewedClients), [](const CViewedClient &ViewedClient) { return ViewedClient.m_IsViewed; });
}

int CMultiView::FindFirstViewed() const
{
	const auto *It = std::find_if(std::begin(m_aViewedClients), std::end(m_aViewedClients), [](const CViewedClient &ViewedClient) { return ViewedClient.m_IsViewed && !ViewedClient.m_Vanish; });
	if(It == std::end(m_aViewedClients))
		return SPEC_FREEVIEW;
	return It - std::begin(m_aViewedClients);
}

void CMultiView::ClearView(int ClientId)
{
	if(ClientId >= MAX_CLIENTS || ClientId < 0)
		return;

	m_aViewedClients[ClientId].Reset();
}

void CMultiView::ClearAll()
{
	for(auto &ViewedClient : m_aViewedClients)
	{
		ViewedClient.Reset();
	}
}

void CMultiView::OnMessage(int MsgId, void *pRawMsg)
{
	if(MsgId != NETMSGTYPE_SV_KILLMSG)
		return;

	CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

	// if we are spectating a static id set (team 0) and somebody killed, and its not a guy in solo, we remove them from the list
	// never remove players from the list if it is a pvp server
	if(IsAnyViewed() && m_Team == 0 && m_aViewedClients[pMsg->m_Victim].m_IsViewed && !GameClient()->m_aClients[pMsg->m_Victim].m_Spec && !m_Solo && !GameClient()->m_GameInfo.m_Pvp)
	{
		m_aViewedClients[pMsg->m_Victim].m_IsViewed = false;

		// if everyone of a team killed, we have no ids to spectate anymore, so we disable multi view
		if(!IsAnyViewed())
		{
			Reset();
		}
		else
		{
			// the "main" tee killed, search a new one
			if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == pMsg->m_Victim)
			{
				int NewClientId = FindFirstViewed();
				if(NewClientId != SPEC_FREEVIEW)
				{
					CViewedClient &Client = m_aViewedClients[NewClientId];
					Client.Reset();
					Client.m_IsViewed = true;
					GameClient()->m_Spectator.Spectate(NewClientId);
				}
			}
		}
	}
}

void CMultiView::UpdateActivation()
{
	// check if multi view got activated
	if(m_IsInit || !m_Activated)
		return;

	int TeamId = 0;
	if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId >= 0)
		TeamId = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId);

	if(TeamId > MAX_CLIENTS || TeamId < 0)
		TeamId = 0;

	if(!Init(TeamId))
	{
		Reset();
	}
}

void CMultiView::CheckReset()
{
	if(!m_Activated && m_IsInit)
		Reset();
}

bool CMultiView::Init(int Team)
{
	float Width, Height;
	ClearAll();
	m_IsInit = true;

	// get the current view coordinates
	Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom, &Width, &Height);
	vec2 AxisX = vec2(GameClient()->m_Camera.m_Center.x - (Width / 2.0f), GameClient()->m_Camera.m_Center.x + (Width / 2.0f));
	vec2 AxisY = vec2(GameClient()->m_Camera.m_Center.y - (Height / 2.0f), GameClient()->m_Camera.m_Center.y + (Height / 2.0f));

	if(Team > 0)
	{
		m_Team = Team;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(GameClient()->m_Teams.Team(ClientId) == Team)
			{
				ViewPlayer(ClientId);
			}
		}
	}
	else
	{
		// we want to allow spectating players in teams directly if there is no other team on screen
		// to do that, -1 is used temporarily for "we don't know which team to spectate yet"
		m_Team = -1;

		int Count = 0;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			vec2 PlayerPos;

			// get the position of the player
			if(GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
				PlayerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
			else if(GameClient()->m_aClients[ClientId].m_Spec)
				PlayerPos = GameClient()->m_aClients[ClientId].m_SpecChar;
			else
				continue;

			if(PlayerPos.x == 0 || PlayerPos.y == 0)
				continue;

			// skip players that aren't in view
			if(PlayerPos.x <= AxisX.x || PlayerPos.x >= AxisX.y || PlayerPos.y <= AxisY.x || PlayerPos.y >= AxisY.y)
				continue;

			if(m_Team == -1)
			{
				// use the current player's team for now, but it might switch to team 0 if any other team is found
				m_Team = GameClient()->m_Teams.Team(ClientId);
			}
			else if(m_Team != 0 && GameClient()->m_Teams.Team(ClientId) != m_Team)
			{
				// mismatched teams; remove all previously added players again and switch to team 0 instead
				for(int PreviousClientId = 0; PreviousClientId < ClientId; ++PreviousClientId)
				{
					m_aViewedClients[PreviousClientId].m_IsViewed = false;
				}
				m_Team = 0;
			}

			ViewPlayer(ClientId);
			Count++;
		}

		// might still be -1 if not a single player was in view; fallback to team 0 in that case
		if(m_Team == -1)
			m_Team = 0;

		// we are spectating only one player
		m_Solo = Count == 1;
	}

	if(IsAnyViewed())
	{
		int SpectatorId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
		int NewSpectatorId = SPEC_FREEVIEW;

		vec2 CurPosition(GameClient()->m_Camera.m_Center);
		if(SpectatorId != SPEC_FREEVIEW)
		{
			const CNetObj_Character &CurCharacter = GameClient()->m_Snap.m_aCharacters[SpectatorId].m_Cur;
			CurPosition.x = CurCharacter.m_X;
			CurPosition.y = CurCharacter.m_Y;
		}

		int ClosestDistance = std::numeric_limits<int>::max();
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId] || GameClient()->m_Snap.m_apPlayerInfos[ClientId]->m_Team == TEAM_SPECTATORS || GameClient()->m_Teams.Team(ClientId) != m_Team)
				continue;

			vec2 PlayerPos;
			if(GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
				PlayerPos = vec2(GameClient()->m_aClients[ClientId].m_RenderPos.x, GameClient()->m_aClients[ClientId].m_RenderPos.y);
			else if(GameClient()->m_aClients[ClientId].m_Spec) // tee is in spec
				PlayerPos = GameClient()->m_aClients[ClientId].m_SpecChar;
			else
				continue;

			int Distance = distance(CurPosition, PlayerPos);
			if(NewSpectatorId == SPEC_FREEVIEW || Distance < ClosestDistance)
			{
				NewSpectatorId = ClientId;
				ClosestDistance = Distance;
			}
		}

		if(NewSpectatorId != SPEC_FREEVIEW)
			GameClient()->m_Spectator.Spectate(NewSpectatorId);
		return true;
	}
	return false;
}

void CMultiView::UpdateView()
{
	const bool AnyViewed = IsAnyViewed();
	vec2 MinPos, MaxPos;
	float SumVel = 0.0f;
	int AmountPlayers = 0;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CViewedClient &ViewedClient = m_aViewedClients[ClientId];
		// look at players who are vanished
		if(ViewedClient.m_Vanish)
		{
			// not in freeze anymore and the delay is over
			if(ViewedClient.m_LastFreeze + 6.0f <= Client()->LocalTime() && GameClient()->m_aClients[ClientId].m_FreezeEnd == 0)
			{
				ViewedClient.m_Vanish = false;
				ViewedClient.m_LastFreeze = 0.0f;
			}
		}

		// we look at team 0 and the player is not in the spec list
		if(AnyViewed && !ViewedClient.m_IsViewed)
			continue;

		// player is vanished
		if(ViewedClient.m_Vanish)
			continue;

		// the player is not in the team we are spectating
		if(GameClient()->m_Teams.Team(ClientId) != m_Team)
			continue;

		vec2 PlayerPos;
		if(GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			PlayerPos = GameClient()->m_aClients[ClientId].m_RenderPos;
		else if(GameClient()->m_aClients[ClientId].m_Spec) // tee is in spec
			PlayerPos = GameClient()->m_aClients[ClientId].m_SpecChar;
		else
			continue;

		// player is far away and frozen
		if(distance(m_OldPos, PlayerPos) > 1100 && GameClient()->m_aClients[ClientId].m_FreezeEnd != 0)
		{
			// check if the player is frozen for more than 3 seconds, if so vanish them
			if(ViewedClient.m_LastFreeze == 0.0f)
			{
				ViewedClient.m_LastFreeze = Client()->LocalTime();
			}
			else if(ViewedClient.m_LastFreeze + 3.0f <= Client()->LocalTime())
			{
				ViewedClient.m_Vanish = true;
				// player we want to be vanished is our "main" tee, so lets switch the tee
				if(ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId)
					GameClient()->m_Spectator.Spectate(FindFirstViewed());
			}
		}
		else if(ViewedClient.m_LastFreeze != 0)
		{
			ViewedClient.m_LastFreeze = 0;
		}

		// set the minimum and maximum position
		if(AmountPlayers == 0)
		{
			MinPos = PlayerPos;
			MaxPos = PlayerPos;
		}
		else
		{
			MinPos.x = std::min(MinPos.x, PlayerPos.x);
			MaxPos.x = std::max(MaxPos.x, PlayerPos.x);
			MinPos.y = std::min(MinPos.y, PlayerPos.y);
			MaxPos.y = std::max(MaxPos.y, PlayerPos.y);
		}

		// sum up the velocity of all players we are spectating
		const CNetObj_Character &CurrentCharacter = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
		SumVel += length(vec2(CurrentCharacter.m_VelX / 256.0f, CurrentCharacter.m_VelY / 256.0f)) * 50.0f / 32.0f;
		AmountPlayers++;
	}

	// if we have found no players, we disable multi view
	if(AmountPlayers == 0)
	{
		if(m_SecondChance == 0.0f)
		{
			m_SecondChance = Client()->LocalTime() + 0.3f;
		}
		else if(m_SecondChance < Client()->LocalTime())
		{
			Reset();
		}
		return;
	}
	else if(m_SecondChance != 0.0f)
	{
		m_SecondChance = 0.0f;
	}

	// if we only have one tee that's in the list, we activate solo-mode
	m_Solo = false;
	for(auto &ViewedClient : m_aViewedClients)
	{
		if(ViewedClient.m_IsViewed)
		{
			if(!m_Solo)
			{
				m_Solo = true;
			}
			else
			{
				// we found a second view, so we can stop searching
				m_Solo = false;
				break;
			}
		}
	}

	vec2 TargetPos = vec2((MinPos.x + MaxPos.x) / 2.0f, (MinPos.y + MaxPos.y) / 2.0f);
	// dont hide the position hud if its only one player
	m_ShowHud = AmountPlayers == 1;
	// get the average velocity
	float AvgVel = std::clamp(SumVel / AmountPlayers, 0.0f, 1000.0f);

	if(m_OldPersonalZoom == m_PersonalZoom)
		GameClient()->m_Camera.SetZoom(CalculateZoom(MinPos, MaxPos, AvgVel), g_Config.m_ClMultiViewZoomSmoothness, false);
	else
		GameClient()->m_Camera.SetZoom(CalculateZoom(MinPos, MaxPos, AvgVel), 50, false);

	GameClient()->m_Snap.m_SpecInfo.m_Position = m_OldPos + ((TargetPos - m_OldPos) * CalculateMultiplier(TargetPos));
	m_OldPos = GameClient()->m_Snap.m_SpecInfo.m_Position;
	GameClient()->m_Snap.m_SpecInfo.m_UsePosition = true;
}

float CMultiView::CalculateMultiplier(vec2 TargetPos)
{
	float MaxCameraDist = 200.0f;
	float MinCameraDist = 20.0f;
	float MaxVel = g_Config.m_ClMultiViewSensitivity / 150.0f;
	float MinVel = 0.007f;
	float CurrentCameraDistance = distance(m_OldPos, TargetPos);
	float UpperLimit = 1.0f;

	if(m_Teleported && CurrentCameraDistance <= 100.0f)
		m_Teleported = false;

	// somebody got teleported very likely
	if((m_Teleported || CurrentCameraDistance - m_OldCameraDistance > 100.0f) && m_OldCameraDistance != 0.0f)
	{
		UpperLimit = 0.1f; // dont try to compensate it by flickering
		m_Teleported = true;
	}
	m_OldCameraDistance = CurrentCameraDistance;

	return std::clamp(MapValue(MaxCameraDist, MinCameraDist, MaxVel, MinVel, CurrentCameraDistance), MinVel, UpperLimit);
}

float CMultiView::CalculateZoom(vec2 MinPos, vec2 MaxPos, float Vel)
{
	float Ratio = Graphics()->ScreenAspect();
	float ZoomX = 0.0f, ZoomY;

	// only calc two axis if the aspect ratio is not 1:1
	if(Ratio != 1.0f)
		ZoomX = (0.001309f - 0.000328f * Ratio) * (MaxPos.x - MinPos.x) + (0.741413f - 0.032959f * Ratio);

	// calculate the according zoom with linear function
	ZoomY = 0.001309f * (MaxPos.y - MinPos.y) + 0.741413f;
	// choose the highest zoom
	float Zoom = std::max(ZoomX, ZoomY);
	// zoom out to maximum 10 percent of the current zoom for 70 velocity
	float Diff = std::clamp(MapValue(70.0f, 15.0f, Zoom * 0.10f, 0.0f, Vel), 0.0f, Zoom * 0.10f);
	// zoom should stay between 1.1 and 20.0
	Zoom = std::clamp(Zoom + Diff, 1.1f, 20.0f);
	// dont go below default zoom
	Zoom = std::max(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), Zoom);
	// add the user preference
	Zoom -= Zoom * 0.1f * m_PersonalZoom;
	m_OldPersonalZoom = m_PersonalZoom;

	return Zoom;
}

void CMultiView::CViewedClient::Reset()
{
	m_IsViewed = false;
	m_LastFreeze = 0.0f;
	m_Vanish = false;
}
