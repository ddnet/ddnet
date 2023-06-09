#include <game/client/gameclient.h>
#include <game/generated/client_data.h>

#include "countdowns.h"

void CCountdowns::RenderBars()
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

		RenderPlayerBars(ClientID);
	}
	if(LocalClientID != -1 && m_pClient->m_Snap.m_aCharacters[LocalClientID].m_Active && IsPlayerInfoAvailable(LocalClientID))
	{
		RenderPlayerBars(LocalClientID);
	}
}

void CCountdowns::RenderPlayerBars(const int ClientID)
{
	CCharacterCore *pCharacter = &m_pClient->m_aClients[ClientID].m_Predicted;

	// Freeze Bar
	bool ShowFreezeBar = true;
	if(pCharacter->m_FreezeEnd <= 0.0f || pCharacter->m_FreezeStart == 0 || pCharacter->m_FreezeEnd <= pCharacter->m_FreezeStart || !m_pClient->m_Snap.m_aCharacters[ClientID].m_HasExtendedDisplayInfo || (pCharacter->m_IsInFreeze && g_Config.m_ClFreezeBarsAlphaInsideFreeze == 0))
	{
		ShowFreezeBar = false;
	}

	if(ShowFreezeBar)
	{
		const int Max = pCharacter->m_FreezeEnd - pCharacter->m_FreezeStart;
		float FreezeProgress = clamp(Max - (Client()->GameTick(g_Config.m_ClDummy) - pCharacter->m_FreezeStart), 0, Max) / (float)Max;
		if(FreezeProgress <= 0.0f)
		{
			ShowFreezeBar = false;
		}
		else
		{
			vec2 Position = m_pClient->m_aClients[ClientID].m_RenderPos;
			// Center the bar under the tee body
			Position.x -= 32;
			Position.y += 26;

			float Alpha = m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
			if(pCharacter->m_IsInFreeze)
			{
				Alpha *= g_Config.m_ClFreezeBarsAlphaInsideFreeze / 100.0f;
			}

			RenderFreezeBarAtPos(&Position, FreezeProgress, Alpha);
		}
	}

	// Ninja Bar
	if(pCharacter->m_Ninja.m_ActivationTick > 0.0f && m_pClient->m_Snap.m_aCharacters[ClientID].m_HasExtendedDisplayInfo)
	{
		const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
		float NinjaProgress = clamp(pCharacter->m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000 - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
		if(NinjaProgress > 0.0f)
		{
			vec2 Position = m_pClient->m_aClients[ClientID].m_RenderPos;
			// Center the bar under the tee body
			Position.x -= 32;
			Position.y += 26;
			if(ShowFreezeBar)
			{
				Position.y += 16;
			}
			float Alpha = m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
			RenderNinjaBarAtPos(&Position, NinjaProgress, Alpha);
		}
	}
}

void CCountdowns::RenderFreezeBarAtPos(const vec2 *Pos, float Progress, const float Alpha)
{
	RenderBarAtPos(Pos, Progress, Alpha, m_pClient->m_OpdSkin.m_SpriteOpdFreezeBarFull, m_pClient->m_OpdSkin.m_SpriteOpdFreezeBarEmpty);
}

void CCountdowns::RenderNinjaBarAtPos(const vec2 *Pos, float Progress, const float Alpha)
{
	RenderBarAtPos(Pos, Progress, Alpha, m_pClient->m_OpdSkin.m_SpriteOpdNinjaBarFull, m_pClient->m_OpdSkin.m_SpriteOpdNinjaBarEmpty);
}

void CCountdowns::RenderBarAtPos(const vec2 *Pos, float Progress, const float Alpha, IGraphics::CTextureHandle TextureFull, IGraphics::CTextureHandle TextureEmpty)
{
	Progress = clamp(Progress, 0.0f, 1.0f);

	static const float BarWidth = 64.0f;
	static const float BarHeight = 16.0f;

	static const float EndRestPct = 0.125f;
	static const float ProgPct = 0.75f;

	static const float EndRestWidth = BarWidth * EndRestPct;
	static const float ProgressBarWidth = BarWidth - (EndRestWidth * 2.0f);

	// Full
	Graphics()->WrapClamp();
	Graphics()->TextureSet(TextureFull);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: top_l, top_m, btm_m, btm_l
	Graphics()->QuadsSetSubsetFree(0, 0, EndRestPct + ProgPct * Progress, 0, EndRestPct + ProgPct * Progress, 1, 0, 1);
	IGraphics::CQuadItem QuadFullBeginning(Pos->x, Pos->y, EndRestWidth + ProgressBarWidth * Progress, BarHeight);
	Graphics()->QuadsDrawTL(&QuadFullBeginning, 1);
	Graphics()->QuadsEnd();

	// Empty
	Graphics()->WrapClamp();
	Graphics()->TextureSet(TextureEmpty);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: top_m, top_r, btm_r, btm_m
	Graphics()->QuadsSetSubsetFree(EndRestPct + ProgPct * Progress, 0, 1, 0, 1, 1, EndRestPct + ProgPct * Progress, 1);
	IGraphics::CQuadItem QuadEmptyBeginning(Pos->x + EndRestWidth + ProgressBarWidth * Progress, Pos->y, EndRestWidth + ProgressBarWidth * (1.0f - Progress), BarHeight);
	Graphics()->QuadsDrawTL(&QuadEmptyBeginning, 1);
	Graphics()->QuadsEnd();

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->WrapNormal();
}

inline bool CCountdowns::IsPlayerInfoAvailable(int ClientID) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientID);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientID);
	return pPrevInfo && pInfo;
}

void CCountdowns::GenerateCountdownStars()
{
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(!m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !IsPlayerInfoAvailable(ClientID))
		{
			continue;
		}
		GenerateFreezeCountdownStars(ClientID);
		GenerateNinjaCountdownStars(ClientID);
	}
}

void CCountdowns::GenerateFreezeCountdownStars(int ClientID)
{
	// We use GameTick here to emulate the old freeze star behaviour as good as possible (we could use PredGameTick)
	const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
	if(m_aaLastGenerateTick[FREEZE_COUNTDOWN][ClientID] >= GameTick)
	{
		return;
	}

	CCharacterCore *pCharacter = &m_pClient->m_aClients[ClientID].m_Predicted;
	if(pCharacter->m_FreezeEnd <= 0 || pCharacter->m_FreezeStart == 0 || pCharacter->m_FreezeEnd <= GameTick)
	{
		return;
	}

	const int FreezeTime = pCharacter->m_FreezeEnd - GameTick;
	// Server sends not every tick a snap
	if(FreezeTime % Client()->GameTickSpeed() == Client()->GameTickSpeed() - 1)
	{
		m_aaLastGenerateTick[FREEZE_COUNTDOWN][ClientID] = GameTick;
		m_pClient->m_DamageInd.CreateDamageInd(m_pClient->m_aClients[ClientID].m_RenderPos, 0, m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f, (FreezeTime + 1) / Client()->GameTickSpeed());
	}
	else if(FreezeTime % Client()->GameTickSpeed() == Client()->GameTickSpeed() - 2)
	{
		m_aaLastGenerateTick[FREEZE_COUNTDOWN][ClientID] = GameTick;
		m_pClient->m_DamageInd.CreateDamageInd(m_pClient->m_aClients[ClientID].m_RenderPos, 0, m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f, (FreezeTime + 2) / Client()->GameTickSpeed());
	}
}

void CCountdowns::GenerateNinjaCountdownStars(int ClientID)
{
	const int GameTick = Client()->PredGameTick(g_Config.m_ClDummy);
	if(m_aaLastGenerateTick[NINJA_COUNTDOWN][ClientID] >= GameTick)
	{
		return;
	}

	CCharacterCore *pCharacter = &m_pClient->m_aClients[ClientID].m_Predicted;
	if(pCharacter->m_Ninja.m_ActivationTick <= 0 || (GameTick - pCharacter->m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000))
	{
		return;
	}

	int NinjaTime = pCharacter->m_Ninja.m_ActivationTick + (g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000) - GameTick;
	if(NinjaTime % Client()->GameTickSpeed() == 0 && NinjaTime / Client()->GameTickSpeed() <= 5)
	{
		m_aaLastGenerateTick[NINJA_COUNTDOWN][ClientID] = GameTick;
		m_pClient->m_DamageInd.CreateDamageInd(m_pClient->m_aClients[ClientID].m_RenderPos, 0, m_pClient->IsOtherTeam(ClientID) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f, NinjaTime / Client()->GameTickSpeed());
	}
}

void CCountdowns::OnInit()
{
	for(auto &aLastGenerateTick : m_aaLastGenerateTick)
	{
		for(int &LastGenerateTick : aLastGenerateTick)
		{
			LastGenerateTick = 0;
		}
	}
}

void CCountdowns::OnRender()
{
	if(g_Config.m_ClShowStateChangeCountdown == 1)
	{
		RenderBars();
	}
	else if(g_Config.m_ClShowStateChangeCountdown == 2)
	{
		GenerateCountdownStars();
	}
}