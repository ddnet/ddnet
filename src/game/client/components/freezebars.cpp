#include <game/client/gameclient.h>

#include "freezebars.h"

void CFreezeBars::RenderFreezeBar(const int ClientID)
{
	const float FreezeBarWidth = 64.0f;
	const float FreezeBarHalfWidth = 32.0f;
	const float FreezeBarHight = 32.0f;

	// pCharacter contains the predicted character for local players or the last snap for players who are spectated
	CCharacterCore *pCharacter = &m_pClient->m_aClients[ClientID].m_Predicted;

	const float PhysSize = 28.0f;
	bool Grounded = false;
	if(Collision()->CheckPoint(pCharacter->m_Pos.x + PhysSize / 2, pCharacter->m_Pos.y + PhysSize / 2 + 5))
		Grounded = true;
	if(Collision()->CheckPoint(pCharacter->m_Pos.x - PhysSize / 2, pCharacter->m_Pos.y + PhysSize / 2 + 5))
		Grounded = true;

	if(pCharacter->m_FreezeEnd <= 0.0f || !m_pClient->m_Snap.m_aCharacters[ClientID].m_HasExtendedDisplayInfo || (pCharacter->m_IsInFreeze && Grounded))
	{
		return;
	}
	const int Max = pCharacter->m_FreezeEnd - pCharacter->m_FreezeTick;
	float FreezeProgress = clamp(Max - (Client()->GameTick(g_Config.m_ClDummy) - pCharacter->m_FreezeTick), 0, Max) / (float)Max;
	if(FreezeProgress <= 0.0f)
	{
		return;
	}

	vec2 Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	Position.x -= FreezeBarHalfWidth;
	Position.y += 32;

	float Alpha = m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;

	m_pClient->m_Hud.RenderProgressBar(Position.x, Position.y, FreezeBarWidth, FreezeBarHight, FreezeProgress, Alpha);
}

inline bool CFreezeBars::IsPlayerInfoAvailable(int ClientID) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientID);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientID);
	return pPrevInfo && pInfo;
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
		if(ClientID == LocalClientID || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !IsPlayerInfoAvailable(ClientID))
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
	if(LocalClientID != -1 && m_pClient->m_Snap.m_aCharacters[LocalClientID].m_Active && IsPlayerInfoAvailable(LocalClientID))
	{
		RenderFreezeBar(LocalClientID);
	}
}