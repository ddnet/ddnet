/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/textrender.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include "nameplates.h"
#include "controls.h"
#include "camera.h"

#include "players.h"

void CNamePlates::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX / 100.0f, pGroup->m_ParallaxY / 100.0f, pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), 1.0f, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CNamePlates::RenderNameplate(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPlayerInfo
	)
{
	float IntraTick = Client()->IntraGameTick();
	int ClientID = pPlayerInfo->m_ClientID;

	vec2 Position;
	if(!m_pClient->AntiPingPlayers())
		Position = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), IntraTick);
	else
		Position = m_pPlayers->m_CurPredictedPos[ClientID];

	bool OtherTeam;

	if(m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team == TEAM_SPECTATORS && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
		OtherTeam = false;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		OtherTeam = m_pClient->m_Teams.Team(pPlayerInfo->m_ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);
	else
		OtherTeam = m_pClient->m_Teams.Team(pPlayerInfo->m_ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientID);

	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;
	float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNameplatesClanSize / 100.0f;
	// render name plate
	if(!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn)
	{
		float a = 1;
		if(g_Config.m_ClNameplatesAlways == 0)
			a = clamp(1-powf(distance(m_pClient->m_pControls->m_TargetPos[g_Config.m_ClDummy], Position)/200.0f,16.0f), 0.0f, 1.0f);

		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName; 
		if(str_comp(pName, m_aNamePlates[ClientID].m_aName) != 0 || FontSize != m_aNamePlates[ClientID].m_NameTextFontSize)
		{
			mem_copy(m_aNamePlates[ClientID].m_aName, pName, sizeof(m_aNamePlates[ClientID].m_aName));
			m_aNamePlates[ClientID].m_NameTextFontSize = FontSize;

			m_aNamePlates[ClientID].m_NameTextWidth = TextRender()->TextWidth(0, FontSize, pName, -1);

			if(m_aNamePlates[ClientID].m_NameTextContainerIndex != -1)
				TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex);

			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = -1;

			// create nameplates at standard zoom
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
			m_aNamePlates[ClientID].m_NameTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, pName);
			Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
		}

		if(g_Config.m_ClNameplatesClan)
		{
			const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
			if(str_comp(pClan, m_aNamePlates[ClientID].m_aClanName) != 0 || FontSizeClan != m_aNamePlates[ClientID].m_ClanNameTextFontSize)
			{
				mem_copy(m_aNamePlates[ClientID].m_aClanName, pClan, sizeof(m_aNamePlates[ClientID].m_aClanName));
				m_aNamePlates[ClientID].m_ClanNameTextFontSize = FontSizeClan;

				m_aNamePlates[ClientID].m_ClanNameTextWidth = TextRender()->TextWidth(0, FontSizeClan, pClan, -1);

				if(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex != -1)
					TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex);

				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, 0, 0, FontSizeClan, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = -1;

				// create nameplates at standard zoom
				float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
				Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
				MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
				m_aNamePlates[ClientID].m_ClanNameTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, pClan);
				Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
			}
		}

		float tw = m_aNamePlates[ClientID].m_NameTextWidth;
		vec3 rgb = vec3(1.0f, 1.0f, 1.0f);
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Teams.Team(ClientID))
			rgb = HslToRgb(vec3(m_pClient->m_Teams.Team(ClientID) / 64.0f, 1.0f, 0.75f));

		STextRenderColor TColor;
		STextRenderColor TOutlineColor;

		if(OtherTeam)
		{
			TOutlineColor.Set(0.0f, 0.0f, 0.0f, 0.2f);
			TColor.Set(rgb.r, rgb.g, rgb.b, g_Config.m_ClShowOthersAlpha / 100.0f);
		}
		else
		{
			TOutlineColor.Set(0.0f, 0.0f, 0.0f, 0.5f*a);
			TColor.Set(rgb.r, rgb.g, rgb.b, a);
		}
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
				TColor.Set(1.0f, 0.5f, 0.5f, a);
			else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
				TColor.Set(0.7f, 0.7f, 1.0f, a);
		}

		if(m_aNamePlates[ClientID].m_NameTextContainerIndex != -1)
			TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex, &TColor, &TOutlineColor, Position.x - tw / 2.0f, Position.y - FontSize - 38.0f);

		if(g_Config.m_ClNameplatesClan)
		{
			if(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex != -1)
				TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex, &TColor, &TOutlineColor, Position.x - m_aNamePlates[ClientID].m_ClanNameTextWidth / 2.0f, Position.y - FontSize - FontSizeClan - 38.0f);
		}

		if(g_Config.m_Debug) // render client id when in debug as well
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf),"%d", pPlayerInfo->m_ClientID);
			float Offset = g_Config.m_ClNameplatesClan ? (FontSize * 2 + FontSizeClan) : (FontSize * 2);
			float tw_id = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, Position.x-tw_id/2.0f, Position.y-Offset-38.0f, 28.0f, aBuf, -1);
		}

		TextRender()->TextColor(1,1,1,1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
	}
}

void CNamePlates::OnRender()
{
	if(!g_Config.m_ClNameplates)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// only render active characters
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

		if(pInfo)
		{
			RenderNameplate(
				&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
				&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
				(const CNetObj_PlayerInfo *)pInfo);
		}
	}
}

void CNamePlates::SetPlayers(CPlayers* pPlayers)
{
	m_pPlayers = pPlayers;
}

void CNamePlates::ResetNamePlates()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aNamePlates[i].m_NameTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aNamePlates[i].m_NameTextContainerIndex);
		if(m_aNamePlates[i].m_ClanNameTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aNamePlates[i].m_ClanNameTextContainerIndex);

		m_aNamePlates[i].Reset();
	}

}

void CNamePlates::OnWindowResize()
{
	ResetNamePlates();
}

void CNamePlates::OnInit()
{
	ResetNamePlates();
}
