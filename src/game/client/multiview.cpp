#include "gameclient.h"

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/components/spectator.h>

#include <algorithm>

void CGameClient::ResetMultiView()
{
	m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);
	m_MultiViewPersonalZoom = 0.0f;
	m_MultiViewActivated = false;
	m_MultiView.m_Solo = false;
	m_MultiView.m_IsInit = false;
	m_MultiView.m_Teleported = false;
	m_MultiView.m_OldCameraDistance = 0.0f;
}

int CGameClient::FindFirstMultiViewId()
{
	int ClientId = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aMultiViewId[i] && !m_MultiView.m_aVanish[i])
			return i;
	}
	return ClientId;
}

void CGameClient::CleanMultiViewId(int ClientId)
{
	if(ClientId >= MAX_CLIENTS || ClientId < 0)
		return;

	m_aMultiViewId[ClientId] = false;
	m_MultiView.m_aLastFreeze[ClientId] = 0.0f;
	m_MultiView.m_aVanish[ClientId] = false;
}

bool CGameClient::InitMultiView(int Team)
{
	float Width, Height;
	CleanMultiViewIds();
	m_MultiView.m_IsInit = true;

	// get the current view coordinates
	Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), m_Camera.m_Zoom, &Width, &Height);
	vec2 AxisX = vec2(m_Camera.m_Center.x - (Width / 2.0f), m_Camera.m_Center.x + (Width / 2.0f));
	vec2 AxisY = vec2(m_Camera.m_Center.y - (Height / 2.0f), m_Camera.m_Center.y + (Height / 2.0f));

	if(Team > 0)
	{
		m_MultiViewTeam = Team;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
			m_aMultiViewId[ClientId] = m_Teams.Team(ClientId) == Team;
	}
	else
	{
		// we want to allow spectating players in teams directly if there is no other team on screen
		// to do that, -1 is used temporarily for "we don't know which team to spectate yet"
		m_MultiViewTeam = -1;

		int Count = 0;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			vec2 PlayerPos;

			// get the position of the player
			if(m_Snap.m_aCharacters[ClientId].m_Active)
				PlayerPos = vec2(m_Snap.m_aCharacters[ClientId].m_Cur.m_X, m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
			else if(m_aClients[ClientId].m_Spec)
				PlayerPos = m_aClients[ClientId].m_SpecChar;
			else
				continue;

			if(PlayerPos.x == 0 || PlayerPos.y == 0)
				continue;

			// skip players that aren't in view
			if(PlayerPos.x <= AxisX.x || PlayerPos.x >= AxisX.y || PlayerPos.y <= AxisY.x || PlayerPos.y >= AxisY.y)
				continue;

			if(m_MultiViewTeam == -1)
			{
				// use the current player's team for now, but it might switch to team 0 if any other team is found
				m_MultiViewTeam = m_Teams.Team(ClientId);
			}
			else if(m_MultiViewTeam != 0 && m_Teams.Team(ClientId) != m_MultiViewTeam)
			{
				// mismatched teams; remove all previously added players again and switch to team 0 instead
				std::fill_n(m_aMultiViewId, ClientId, false);
				m_MultiViewTeam = 0;
			}

			m_aMultiViewId[ClientId] = true;
			Count++;
		}

		// might still be -1 if not a single player was in view; fallback to team 0 in that case
		if(m_MultiViewTeam == -1)
			m_MultiViewTeam = 0;

		// we are spectating only one player
		m_MultiView.m_Solo = Count == 1;
	}

	if(IsMultiViewIdSet())
	{
		int SpectatorId = m_Snap.m_SpecInfo.m_SpectatorId;
		int NewSpectatorId = -1;

		vec2 CurPosition(m_Camera.m_Center);
		if(SpectatorId != SPEC_FREEVIEW)
		{
			const CNetObj_Character &CurCharacter = m_Snap.m_aCharacters[SpectatorId].m_Cur;
			CurPosition.x = CurCharacter.m_X;
			CurPosition.y = CurCharacter.m_Y;
		}

		int ClosestDistance = std::numeric_limits<int>::max();
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(!m_Snap.m_apPlayerInfos[ClientId] || m_Snap.m_apPlayerInfos[ClientId]->m_Team == TEAM_SPECTATORS || m_Teams.Team(ClientId) != m_MultiViewTeam)
				continue;

			vec2 PlayerPos;
			if(m_Snap.m_aCharacters[ClientId].m_Active)
				PlayerPos = vec2(m_aClients[ClientId].m_RenderPos.x, m_aClients[ClientId].m_RenderPos.y);
			else if(m_aClients[ClientId].m_Spec) // tee is in spec
				PlayerPos = m_aClients[ClientId].m_SpecChar;
			else
				continue;

			int Distance = distance(CurPosition, PlayerPos);
			if(NewSpectatorId == -1 || Distance < ClosestDistance)
			{
				NewSpectatorId = ClientId;
				ClosestDistance = Distance;
			}
		}

		if(NewSpectatorId > -1)
			m_Spectator.Spectate(NewSpectatorId);
	}

	return IsMultiViewIdSet();
}

void CGameClient::HandleMultiView()
{
	bool IsTeamZero = IsMultiViewIdSet();
	bool Init = false;
	vec2 MinPos, MaxPos;
	float SumVel = 0.0f;
	int AmountPlayers = 0;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		// look at players who are vanished
		if(m_MultiView.m_aVanish[ClientId])
		{
			// not in freeze anymore and the delay is over
			if(m_MultiView.m_aLastFreeze[ClientId] + 6.0f <= Client()->LocalTime() && m_aClients[ClientId].m_FreezeEnd == 0)
			{
				m_MultiView.m_aVanish[ClientId] = false;
				m_MultiView.m_aLastFreeze[ClientId] = 0.0f;
			}
		}

		// we look at team 0 and the player is not in the spec list
		if(IsTeamZero && !m_aMultiViewId[ClientId])
			continue;

		// player is vanished
		if(m_MultiView.m_aVanish[ClientId])
			continue;

		// the player is not in the team we are spectating
		if(m_Teams.Team(ClientId) != m_MultiViewTeam)
			continue;

		vec2 PlayerPos;
		if(m_Snap.m_aCharacters[ClientId].m_Active)
			PlayerPos = m_aClients[ClientId].m_RenderPos;
		else if(m_aClients[ClientId].m_Spec) // tee is in spec
			PlayerPos = m_aClients[ClientId].m_SpecChar;
		else
			continue;

		// player is far away and frozen
		if(distance(m_MultiView.m_OldPos, PlayerPos) > 1100 && m_aClients[ClientId].m_FreezeEnd != 0)
		{
			// check if the player is frozen for more than 3 seconds, if so vanish them
			if(m_MultiView.m_aLastFreeze[ClientId] == 0.0f)
			{
				m_MultiView.m_aLastFreeze[ClientId] = Client()->LocalTime();
			}
			else if(m_MultiView.m_aLastFreeze[ClientId] + 3.0f <= Client()->LocalTime())
			{
				m_MultiView.m_aVanish[ClientId] = true;
				// player we want to be vanished is our "main" tee, so lets switch the tee
				if(ClientId == m_Snap.m_SpecInfo.m_SpectatorId)
					m_Spectator.Spectate(FindFirstMultiViewId());
			}
		}
		else if(m_MultiView.m_aLastFreeze[ClientId] != 0)
		{
			m_MultiView.m_aLastFreeze[ClientId] = 0;
		}

		// set the minimum and maximum position
		if(!Init)
		{
			MinPos = PlayerPos;
			MaxPos = PlayerPos;
			Init = true;
		}
		else
		{
			MinPos.x = std::min(MinPos.x, PlayerPos.x);
			MaxPos.x = std::max(MaxPos.x, PlayerPos.x);
			MinPos.y = std::min(MinPos.y, PlayerPos.y);
			MaxPos.y = std::max(MaxPos.y, PlayerPos.y);
		}

		// sum up the velocity of all players we are spectating
		const CNetObj_Character &CurrentCharacter = m_Snap.m_aCharacters[ClientId].m_Cur;
		SumVel += length(vec2(CurrentCharacter.m_VelX / 256.0f, CurrentCharacter.m_VelY / 256.0f)) * 50.0f / 32.0f;
		AmountPlayers++;
	}

	// if we have found no players, we disable multi view
	if(AmountPlayers == 0)
	{
		if(m_MultiView.m_SecondChance == 0.0f)
		{
			m_MultiView.m_SecondChance = Client()->LocalTime() + 0.3f;
		}
		else if(m_MultiView.m_SecondChance < Client()->LocalTime())
		{
			ResetMultiView();
		}
		return;
	}
	else if(m_MultiView.m_SecondChance != 0.0f)
	{
		m_MultiView.m_SecondChance = 0.0f;
	}

	// if we only have one tee that's in the list, we activate solo-mode
	m_MultiView.m_Solo = std::count(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), true) == 1;

	vec2 TargetPos = vec2((MinPos.x + MaxPos.x) / 2.0f, (MinPos.y + MaxPos.y) / 2.0f);
	// dont hide the position hud if its only one player
	m_MultiViewShowHud = AmountPlayers == 1;
	// get the average velocity
	float AvgVel = std::clamp(SumVel / AmountPlayers, 0.0f, 1000.0f);

	if(m_MultiView.m_OldPersonalZoom == m_MultiViewPersonalZoom)
		m_Camera.SetZoom(CalculateMultiViewZoom(MinPos, MaxPos, AvgVel), g_Config.m_ClMultiViewZoomSmoothness, false);
	else
		m_Camera.SetZoom(CalculateMultiViewZoom(MinPos, MaxPos, AvgVel), 50, false);

	m_Snap.m_SpecInfo.m_Position = m_MultiView.m_OldPos + ((TargetPos - m_MultiView.m_OldPos) * CalculateMultiViewMultiplier(TargetPos));
	m_MultiView.m_OldPos = m_Snap.m_SpecInfo.m_Position;
	m_Snap.m_SpecInfo.m_UsePosition = true;
}

bool CGameClient::IsMultiViewIdSet()
{
	return std::any_of(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), [](bool IsSet) { return IsSet; });
}

void CGameClient::CleanMultiViewIds()
{
	std::fill(std::begin(m_aMultiViewId), std::end(m_aMultiViewId), false);
	std::fill(std::begin(m_MultiView.m_aLastFreeze), std::end(m_MultiView.m_aLastFreeze), 0.0f);
	std::fill(std::begin(m_MultiView.m_aVanish), std::end(m_MultiView.m_aVanish), false);
}

float CGameClient::CalculateMultiViewMultiplier(vec2 TargetPos)
{
	float MaxCameraDist = 200.0f;
	float MinCameraDist = 20.0f;
	float MaxVel = g_Config.m_ClMultiViewSensitivity / 150.0f;
	float MinVel = 0.007f;
	float CurrentCameraDistance = distance(m_MultiView.m_OldPos, TargetPos);
	float UpperLimit = 1.0f;

	if(m_MultiView.m_Teleported && CurrentCameraDistance <= 100.0f)
		m_MultiView.m_Teleported = false;

	// somebody got teleported very likely
	if((m_MultiView.m_Teleported || CurrentCameraDistance - m_MultiView.m_OldCameraDistance > 100.0f) && m_MultiView.m_OldCameraDistance != 0.0f)
	{
		UpperLimit = 0.1f; // dont try to compensate it by flickering
		m_MultiView.m_Teleported = true;
	}
	m_MultiView.m_OldCameraDistance = CurrentCameraDistance;

	return std::clamp(MapValue(MaxCameraDist, MinCameraDist, MaxVel, MinVel, CurrentCameraDistance), MinVel, UpperLimit);
}

float CGameClient::CalculateMultiViewZoom(vec2 MinPos, vec2 MaxPos, float Vel)
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
	Zoom -= Zoom * 0.1f * m_MultiViewPersonalZoom;
	m_MultiView.m_OldPersonalZoom = m_MultiViewPersonalZoom;

	return Zoom;
}

float CGameClient::MapValue(float MaxValue, float MinValue, float MaxRange, float MinRange, float Value)
{
	return (MaxRange - MinRange) / (MaxValue - MinValue) * (Value - MinValue) + MinRange;
}
