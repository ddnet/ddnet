/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include "camera.h"
#include "controls.h"
#include "nameplates.h"
#include <game/client/animstate.h>
#include <game/client/gameclient.h>

#include "players.h"

void CNamePlates::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX, pGroup->m_ParallaxY, pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), 1.0f, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CNamePlates::RenderNameplate(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPlayerInfo)
{
	int ClientID = pPlayerInfo->m_ClientID;

	vec2 Position;
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));

	RenderNameplatePos(Position, pPlayerInfo, 1.0f);
}

void CNamePlates::RenderNameplatePos(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha)
{
	int ClientID = pPlayerInfo->m_ClientID;

	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);

	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;
	float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNameplatesClanSize / 100.0f;

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE);
	float YOffset = Position.y - 38;
	ColorRGBA rgb = ColorRGBA(1.0f, 1.0f, 1.0f);

	// render name plate
	if(!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn)
	{
		float a = 1;
		if(g_Config.m_ClNameplatesAlways == 0)
			a = clamp(1 - powf(distance(m_pClient->m_pControls->m_TargetPos[g_Config.m_ClDummy], Position) / 200.0f, 16.0f), 0.0f, 1.0f);

		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName;
		if(str_comp(pName, m_aNamePlates[ClientID].m_aName) != 0 || FontSize != m_aNamePlates[ClientID].m_NameTextFontSize)
		{
			mem_copy(m_aNamePlates[ClientID].m_aName, pName, sizeof(m_aNamePlates[ClientID].m_aName));
			m_aNamePlates[ClientID].m_NameTextFontSize = FontSize;

			if(m_aNamePlates[ClientID].m_NameTextContainerIndex != -1)
				TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex);

			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = -1;

			// create nameplates at standard zoom
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());

			m_aNamePlates[ClientID].m_NameTextWidth = TextRender()->TextWidth(0, FontSize, pName, -1, -1.0f);

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

				if(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex != -1)
					TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex);

				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, 0, 0, FontSizeClan, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = -1;

				// create nameplates at standard zoom
				float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
				Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
				MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());

				m_aNamePlates[ClientID].m_ClanNameTextWidth = TextRender()->TextWidth(0, FontSizeClan, pClan, -1, -1.0f);

				m_aNamePlates[ClientID].m_ClanNameTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, pClan);
				Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
			}
		}

		float tw = m_aNamePlates[ClientID].m_NameTextWidth;
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Teams.Team(ClientID))
			rgb = color_cast<ColorRGBA>(ColorHSLA(m_pClient->m_Teams.Team(ClientID) / 64.0f, 1.0f, 0.75f));

		STextRenderColor TColor;
		STextRenderColor TOutlineColor;

		if(OtherTeam)
		{
			TOutlineColor.Set(0.0f, 0.0f, 0.0f, 0.2f * g_Config.m_ClShowOthersAlpha / 100.0f);
			TColor.Set(rgb.r, rgb.g, rgb.b, g_Config.m_ClShowOthersAlpha / 100.0f);
		}
		else
		{
			TOutlineColor.Set(0.0f, 0.0f, 0.0f, 0.5f * a);
			TColor.Set(rgb.r, rgb.g, rgb.b, a);
		}
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
				TColor.Set(1.0f, 0.5f, 0.5f, a);
			else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
				TColor.Set(0.7f, 0.7f, 1.0f, a);
		}

		TOutlineColor.m_A *= Alpha;
		TColor.m_A *= Alpha;

		if(m_aNamePlates[ClientID].m_NameTextContainerIndex != -1)
		{
			YOffset -= FontSize;
			TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex, &TColor, &TOutlineColor, Position.x - tw / 2.0f, YOffset);
		}

		if(g_Config.m_ClNameplatesClan)
		{
			YOffset -= FontSizeClan;
			if(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex != -1)
				TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex, &TColor, &TOutlineColor, Position.x - m_aNamePlates[ClientID].m_ClanNameTextWidth / 2.0f, YOffset);
		}

		if(g_Config.m_ClNameplatesFriendMark && m_pClient->m_aClients[ClientID].m_Friend)
		{
			YOffset -= FontSize;
			char aFriendMark[] = "♥";
			TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
			float XOffSet = TextRender()->TextWidth(0, FontSize, aFriendMark, -1, -1.0f) / 2.0f;
			TextRender()->Text(0, Position.x - XOffSet, YOffset, FontSize, aFriendMark, -1.0f);
		}

		if(g_Config.m_Debug || g_Config.m_ClNameplatesIDs) // render client id when in debug as well
		{
			YOffset -= FontSize;
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%d", pPlayerInfo->m_ClientID);
			float XOffset = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f) / 2.0f;
			TextRender()->TextColor(rgb);
			TextRender()->Text(0, Position.x - XOffset, YOffset, FontSize, aBuf, -1.0f);
		}

		if(g_Config.m_ClNameplatesHA) // render health and armor in nameplate
		{
			int Health = m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Health;
			int Armor = m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Armor;

			if(Health > 0 || Armor > 0)
			{
				float HFontSize = 5.0f + 20.0f * g_Config.m_ClNameplatesHASize / 100.0f;
				float AFontSize = 6.0f + 24.0f * g_Config.m_ClNameplatesHASize / 100.0f;
				char aHealth[40] = "\0";
				char aArmor[40] = "\0";
				for(int i = 0; i < Health; i++)
					str_append(aHealth, "♥", sizeof(aHealth));
				for(int i = Health; i < 10; i++)
					str_append(aHealth, "♡", sizeof(aHealth));
				str_append(aHealth, "\0", sizeof(aHealth));
				for(int i = 0; i < Armor; i++)
					str_append(aArmor, "⚫", sizeof(aArmor));
				for(int i = Armor; i < 10; i++)
					str_append(aArmor, "⚪", sizeof(aArmor));
				str_append(aArmor, "\0", sizeof(aArmor));

				YOffset -= HFontSize + AFontSize;
				float PosHealth = TextRender()->TextWidth(0, HFontSize, aHealth, -1, -1.0f);
				TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
				TextRender()->Text(0, Position.x - PosHealth / 2.0f, YOffset, HFontSize, aHealth, -1);

				YOffset -= -AFontSize + 3.0f;
				float PosArmor = TextRender()->TextWidth(0, AFontSize, aArmor, -1, -1.0f);
				TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 0.0f));
				TextRender()->Text(0, Position.x - PosArmor / 2.0f, YOffset, AFontSize, aArmor, -1);
			}
		}
	}

	if(g_Config.m_Debug)
	{
		CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterByID(pPlayerInfo->m_ClientID);
		if(pCharacter)
		{
			YOffset -= FontSize;
			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "⇢ %d", pCharacter->GetStrongWeakID());
			float XOffset = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f) / 2.0f;
			TextRender()->TextColor(rgb);
			TextRender()->Text(0, Position.x - XOffset, YOffset, FontSize, aBuf, -1.0f);
		}
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

	TextRender()->SetRenderFlags(0);
}

void CNamePlates::OnRender()
{
	if(!g_Config.m_ClNameplates)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
		if(!pInfo)
		{
			continue;
		}

		if(m_pClient->m_aClients[i].m_SpecCharPresent)
		{
			bool OtherTeam = m_pClient->IsOtherTeam(i);
			float Alpha = 0.4f * (OtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f);
			RenderNameplatePos(m_pClient->m_aClients[i].m_SpecChar, pInfo, Alpha);
		}

		// only render active characters
		if(m_pClient->m_Snap.m_aCharacters[i].m_Active)
		{
			RenderNameplate(
				&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
				&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
				pInfo);
		}
	}
}

void CNamePlates::SetPlayers(CPlayers *pPlayers)
{
	m_pPlayers = pPlayers;
}

void CNamePlates::ResetNamePlates()
{
	for(auto &NamePlate : m_aNamePlates)
	{
		if(NamePlate.m_NameTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(NamePlate.m_NameTextContainerIndex);
		if(NamePlate.m_ClanNameTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(NamePlate.m_ClanNameTextContainerIndex);

		NamePlate.Reset();
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
