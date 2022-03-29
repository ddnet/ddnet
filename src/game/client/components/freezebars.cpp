#include <game/client/gameclient.h>

#include "freezebars.h"

void CFreezeBars::RenderFreezeBar(const int ClientID)
{
	const float FreezeBarWidth = 64.0f;
	const float FreezeBarHalfWidth = 32.0f;
	const float FreezeBarHight = 16.0f;

	// pCharacter contains the predicted character for local players or the last snap for players who are spectated
	CCharacterCore *pCharacter = &m_pClient->m_aClients[ClientID].m_Predicted;
	if(pCharacter->m_FreezeEnd <= 0.0f || !m_pClient->m_Snap.m_aCharacters[ClientID].m_HasExtendedDisplayInfo || pCharacter->m_IsInFreeze)
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

	RenderFreezeBarPos(Position.x, Position.y, FreezeBarWidth, FreezeBarHight, FreezeProgress, Alpha);
}

void CFreezeBars::RenderFreezeBarPos(float x, const float y, const float width, const float height, float Progress, const float Alpha)
{
	Progress = clamp(Progress, 0.0f, 1.0f);
	// half of the ends are also used for the progress display
	const float EndWidth = height; // to keep the correct scale - the height of the sprite is as long as the width
	const float BarHeight = height;
	const float WholeBarWidth = width + EndWidth; // add the two empty halves to the total width
	const float MiddleBarWidth = WholeBarWidth - (EndWidth * 2.0f);
	const float EndProgressWidth = EndWidth / 2.0f;
	const float ProgressBarWidth = WholeBarWidth - (EndProgressWidth * 2.0f);
	const float EndProgressProportion = EndProgressWidth / ProgressBarWidth;
	const float MiddleProgressProportion = MiddleBarWidth / ProgressBarWidth;
	x -= EndProgressWidth; // half of the first sprite is empty

	// we cut 1% of all sides of all sprites so we don't get edge bleeding

	// beginning piece
	float BeginningPieceProgress = 1;
	if(Progress <= EndProgressProportion)
	{
		BeginningPieceProgress = Progress / EndProgressProportion;
	}
	const float BeginningPiecePercentVisible = 0.5f + 0.5f * BeginningPieceProgress;
	// full
	Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarFullLeft);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	Graphics()->QuadsSetSubset(0.01f, 0.01f, 0.01f + 0.98f * BeginningPiecePercentVisible, 0.99f);
	IGraphics::CQuadItem QuadFullBeginning(x, y, EndWidth * BeginningPiecePercentVisible, BarHeight);
	Graphics()->QuadsDrawTL(&QuadFullBeginning, 1);
	Graphics()->QuadsEnd();
	if(BeginningPiecePercentVisible < 1.0f)
	{
		// empty
		Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarEmptyRight);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		Graphics()->QuadsSetSubset(0.01f, 0.99f, 0.01f + 0.98f * (1.0f - BeginningPiecePercentVisible), 0.01f);
		Graphics()->QuadsSetRotation(pi);
		IGraphics::CQuadItem QuadEmptyBeginning(x + (EndWidth * BeginningPiecePercentVisible), y, EndWidth * (1.0f - BeginningPiecePercentVisible), BarHeight);
		Graphics()->QuadsDrawTL(&QuadEmptyBeginning, 1);
		Graphics()->QuadsEnd();
		Graphics()->QuadsSetRotation(0);
	}

	// middle piece
	x += EndWidth;

	float MiddlePieceProgress = 1;
	if(Progress <= EndProgressProportion + MiddleProgressProportion)
	{
		if(Progress <= EndProgressProportion)
		{
			MiddlePieceProgress = 0;
		}
		else
		{
			MiddlePieceProgress = (Progress - EndProgressProportion) / MiddleProgressProportion;
		}
	}

	const float FullMiddleBarWidth = MiddleBarWidth * MiddlePieceProgress;
	const float EmptyMiddleBarWidth = MiddleBarWidth - FullMiddleBarWidth;

	// full ninja bar
	Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarFull);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// select the middle portion of the sprite so we don't get edge bleeding
	if(MiddlePieceProgress * MiddleBarWidth <= EndWidth * 0.98f)
	{
		// prevent pixel puree, select only a small slice
		Graphics()->QuadsSetSubset(0.01f, 0.01f, 0.01f + 0.98f * MiddlePieceProgress, 0.99f);
	}
	else
	{
		Graphics()->QuadsSetSubset(0.01f, 0.01f, 0.99f, 0.99f);
	}
	IGraphics::CQuadItem QuadFull(x, y, FullMiddleBarWidth, BarHeight);
	Graphics()->QuadsDrawTL(&QuadFull, 1);
	Graphics()->QuadsEnd();

	// empty ninja bar
	Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarEmpty);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// select the middle portion of the sprite so we don't get edge bleeding
	if((1.0f - MiddlePieceProgress) * MiddleBarWidth <= EndWidth * 0.98f)
	{
		// prevent pixel puree, select only a small slice
		Graphics()->QuadsSetSubset(0.01f, 0.01f, 0.01f + 0.98f * (1.0f - MiddlePieceProgress), 0.99f);
	}
	else
	{
		Graphics()->QuadsSetSubset(0.01f, 0.01f, 0.99f, 0.99f);
	}

	IGraphics::CQuadItem QuadEmpty(x + FullMiddleBarWidth, y, EmptyMiddleBarWidth, BarHeight);
	Graphics()->QuadsDrawTL(&QuadEmpty, 1);
	Graphics()->QuadsEnd();

	// end piece
	x += MiddleBarWidth;
	float EndingPieceProgress = 1;
	if(Progress <= 1)
	{
		if(Progress <= (EndProgressProportion + MiddleProgressProportion))
		{
			EndingPieceProgress = 0;
		}
		else
		{
			EndingPieceProgress = (Progress - EndProgressProportion - MiddleProgressProportion) / EndProgressProportion;
		}
	}
	// full
	if(EndingPieceProgress > 0.0f)
	{
		Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarFullLeft);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		Graphics()->QuadsSetSubset(0.5f + 0.49f * (1.0f - EndingPieceProgress), 0.99f, 0.99f, 0.01f);
		Graphics()->QuadsSetRotation(pi);
		IGraphics::CQuadItem QuadFullEnding(x, y, (EndWidth / 2) * EndingPieceProgress, BarHeight);
		Graphics()->QuadsDrawTL(&QuadFullEnding, 1);
		Graphics()->QuadsEnd();
		Graphics()->QuadsSetRotation(0);
	}
	// empty
	Graphics()->TextureSet(m_pClient->m_HudSkin.m_SpriteHudFreezeBarEmptyRight);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	Graphics()->QuadsSetSubset(0.5f - 0.49f * (1.0f - EndingPieceProgress), 0.01f, 0.99f, 0.99f);
	IGraphics::CQuadItem QuadEmptyEnding(x + ((EndWidth / 2) * EndingPieceProgress), y, (EndWidth / 2) * (1.0f - EndingPieceProgress) + (EndWidth / 2), BarHeight);
	Graphics()->QuadsDrawTL(&QuadEmptyEnding, 1);
	Graphics()->QuadsEnd();
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
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