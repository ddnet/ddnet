#include <game/client/gameclient.h>

#include "freezebars.h"


void CFreezeBars::RenderFreezeBar(const int ClientID)
{
	const float FreezeBarWidth = 64.0f;
	const float FreezeBarHalfWidth = 32.0f;
	const float FreezeBarHight = 16.0f;

	if(m_pClient->m_aClients[ClientID].m_Predicted.m_FreezeEnd <= 0.0f)
	{
		return;
	}
	const int Max = m_pClient->m_aClients[ClientID].m_Predicted.m_FreezeEnd - m_pClient->m_aClients[ClientID].m_Predicted.m_FreezeTick;
	float FreezeProgress = clamp(Max - (m_pClient->m_GameWorld.GameTick() - m_pClient->m_aClients[ClientID].m_Predicted.m_FreezeTick), 0, Max) / (float)Max;
	if(FreezeProgress <= 0.0f)
	{
		return;
	}

	vec2 Position;
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(m_pClient->m_aClients[ClientID].m_RenderPrev.m_X, m_pClient->m_aClients[ClientID].m_RenderPrev.m_Y), vec2(m_pClient->m_aClients[ClientID].m_RenderCur.m_X, m_pClient->m_aClients[ClientID].m_RenderCur.m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));
	Position.x -= FreezeBarHalfWidth;
	Position.y += 32;

	float Alpha = m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;

	m_pClient->m_Hud.RenderProgressBar(Position.x, Position.y, FreezeBarWidth, FreezeBarHight, FreezeProgress, Alpha);
}

void CFreezeBars::OnRender()
{
	// get screen edges to avoid rendering offscreen
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	// expand the edges to prevent popping in/out onscreen
	//
	// it is assumed that the tee with the freeze bar fit into a 240x240 box centered on the tee
	// this may need to be changed or calculated differently in the future
	float BorderBuffer = 120;
	ScreenX0 -= BorderBuffer;
	ScreenX1 += BorderBuffer;
	ScreenY0 -= BorderBuffer;
	ScreenY1 += BorderBuffer;

	int LocalClientID = m_pClient->m_Snap.m_LocalClientID;

	// render everyone else's freeze bar, then our own
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(ClientID == LocalClientID || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !m_pClient->m_Players.IsPlayerInfoAvailable(ClientID))
		{
			continue;
		}

		//don't render if the tee is offscreen
		vec2 *pRenderPos = &m_pClient->m_aClients[ClientID].m_RenderPos;
		if(pRenderPos->x < ScreenX0 || pRenderPos->x > ScreenX1 || pRenderPos->y < ScreenY0 || pRenderPos->y > ScreenY1)
		{
			continue;
		}

		RenderFreezeBar(ClientID);

		
	}
	if(LocalClientID != -1 && m_pClient->m_Snap.m_aCharacters[LocalClientID].m_Active && m_pClient->m_Players.IsPlayerInfoAvailable(LocalClientID))
	{
		RenderFreezeBar(LocalClientID);
	}
}