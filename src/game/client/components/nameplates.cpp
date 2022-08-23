/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include "camera.h"
#include "controls.h"
#include "nameplates.h"

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

void CNamePlates::RenderNameplatePos(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha)
{
	int ClientID = pPlayerInfo->m_ClientID;

	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);

	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;
	float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNameplatesClanSize / 100.0f;

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE);
	float YOffset = Position.y - 38;
	ColorRGBA rgb = ColorRGBA(1.0f, 1.0f, 1.0f);

	// render players' key presses
	int ShowDirection = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ShowDirection = g_Config.m_ClVideoShowDirection;
#endif
	if((pPlayerInfo->m_Local && ShowDirection == 2) || (!pPlayerInfo->m_Local && ShowDirection >= 1))
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);

		const float ShowDirectionImgSize = 22.0f;
		YOffset -= ShowDirectionImgSize;
		vec2 ShowDirectionPos = vec2(Position.x - 11.0f, YOffset);

		if(m_pClient->m_Snap.m_aCharacters[pPlayerInfo->m_ClientID].m_Cur.m_Direction == -1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->QuadsSetRotation(pi);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, ShowDirectionPos.x - 30.f, ShowDirectionPos.y);
		}
		else if(m_pClient->m_Snap.m_aCharacters[pPlayerInfo->m_ClientID].m_Cur.m_Direction == 1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, ShowDirectionPos.x + 30.f, ShowDirectionPos.y);
		}
		if(m_pClient->m_Snap.m_aCharacters[pPlayerInfo->m_ClientID].m_Cur.m_Jumped & 1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->QuadsSetRotation(pi * 3 / 2);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, ShowDirectionPos.x, ShowDirectionPos.y);
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	// render name plate
	if((!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn) && g_Config.m_ClNameplates)
	{
		float a = 1;
		if(g_Config.m_ClNameplatesAlways == 0)
			a = clamp(1 - powf(distance(m_pClient->m_Controls.m_aTargetPos[g_Config.m_ClDummy], Position) / 200.0f, 16.0f), 0.0f, 1.0f);

		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName;
		if(str_comp(pName, m_aNamePlates[ClientID].m_aName) != 0 || FontSize != m_aNamePlates[ClientID].m_NameTextFontSize)
		{
			mem_copy(m_aNamePlates[ClientID].m_aName, pName, sizeof(m_aNamePlates[ClientID].m_aName));
			m_aNamePlates[ClientID].m_NameTextFontSize = FontSize;

			TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex);

			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = -1;

			// create nameplates at standard zoom
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			RenderTools()->MapScreenToInterface(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y);

			m_aNamePlates[ClientID].m_NameTextWidth = TextRender()->TextWidth(0, FontSize, pName, -1, -1.0f);

			TextRender()->CreateTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex, &Cursor, pName);
			Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
		}

		if(g_Config.m_ClNameplatesClan)
		{
			const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
			if(str_comp(pClan, m_aNamePlates[ClientID].m_aClanName) != 0 || FontSizeClan != m_aNamePlates[ClientID].m_ClanNameTextFontSize)
			{
				mem_copy(m_aNamePlates[ClientID].m_aClanName, pClan, sizeof(m_aNamePlates[ClientID].m_aClanName));
				m_aNamePlates[ClientID].m_ClanNameTextFontSize = FontSizeClan;

				TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex);

				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, 0, 0, FontSizeClan, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = -1;

				// create nameplates at standard zoom
				float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
				Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
				RenderTools()->MapScreenToInterface(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y);

				m_aNamePlates[ClientID].m_ClanNameTextWidth = TextRender()->TextWidth(0, FontSizeClan, pClan, -1, -1.0f);

				TextRender()->CreateTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex, &Cursor, pClan);
				Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
			}
		}

		float tw = m_aNamePlates[ClientID].m_NameTextWidth;
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Teams.Team(ClientID))
			rgb = color_cast<ColorRGBA>(ColorHSLA(m_pClient->m_Teams.Team(ClientID) / 64.0f, 1.0f, 0.75f));

		ColorRGBA TColor;
		ColorRGBA TOutlineColor;

		if(OtherTeam && !ForceAlpha)
		{
			TOutlineColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.2f * g_Config.m_ClShowOthersAlpha / 100.0f);
			TColor = ColorRGBA(rgb.r, rgb.g, rgb.b, g_Config.m_ClShowOthersAlpha / 100.0f);
		}
		else
		{
			TOutlineColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f * a);
			TColor = ColorRGBA(rgb.r, rgb.g, rgb.b, a);
		}
		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
				TColor = ColorRGBA(1.0f, 0.5f, 0.5f, a);
			else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
				TColor = ColorRGBA(0.7f, 0.7f, 1.0f, a);
		}

		TOutlineColor.a *= Alpha;
		TColor.a *= Alpha;

		if(m_aNamePlates[ClientID].m_NameTextContainerIndex != -1)
		{
			YOffset -= FontSize;
			TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_NameTextContainerIndex, TColor, TOutlineColor, Position.x - tw / 2.0f, YOffset);
		}

		if(g_Config.m_ClNameplatesClan)
		{
			YOffset -= FontSizeClan;
			if(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex != -1)
				TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_ClanNameTextContainerIndex, TColor, TOutlineColor, Position.x - m_aNamePlates[ClientID].m_ClanNameTextWidth / 2.0f, YOffset);
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
	}

	if((g_Config.m_Debug || g_Config.m_ClNameplatesStrong) && g_Config.m_ClNameplates)
	{
		if(m_pClient->m_Snap.m_LocalClientID != -1 && m_pClient->m_Snap.m_aCharacters[pPlayerInfo->m_ClientID].m_HasExtendedData && m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_HasExtendedData)
		{
			CCharacter *pLocalChar = m_pClient->m_GameWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);
			CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterByID(pPlayerInfo->m_ClientID);
			if(pCharacter && pLocalChar)
			{
				if(pPlayerInfo->m_Local)
					TextRender()->TextColor(rgb);
				else
				{
					float ScaleX, ScaleY;
					const float StrongWeakImgSize = 40.0f;
					Graphics()->TextureClear();
					Graphics()->TextureSet(g_pData->m_aImages[IMAGE_STRONGWEAK].m_Id);
					Graphics()->QuadsBegin();
					ColorRGBA StrongWeakStatusColor;
					int StrongWeakSpriteID;
					if(pLocalChar->GetStrongWeakID() > pCharacter->GetStrongWeakID())
					{
						StrongWeakStatusColor = color_cast<ColorRGBA>(ColorHSLA(6401973));
						StrongWeakSpriteID = SPRITE_HOOK_STRONG;
					}
					else
					{
						StrongWeakStatusColor = color_cast<ColorRGBA>(ColorHSLA(41131));
						StrongWeakSpriteID = SPRITE_HOOK_WEAK;
					}
					Graphics()->SetColor(StrongWeakStatusColor);
					RenderTools()->SelectSprite(StrongWeakSpriteID);
					RenderTools()->GetSpriteScale(StrongWeakSpriteID, ScaleX, ScaleY);
					TextRender()->TextColor(StrongWeakStatusColor);

					YOffset -= StrongWeakImgSize * ScaleY;
					RenderTools()->DrawSprite(Position.x, YOffset + (StrongWeakImgSize / 2.0f) * ScaleY, StrongWeakImgSize);
					Graphics()->QuadsEnd();
				}
				if(g_Config.m_Debug || g_Config.m_ClNameplatesStrong == 2)
				{
					YOffset -= FontSize;
					char aBuf[12];
					str_format(aBuf, sizeof(aBuf), "%d", pCharacter->GetStrongWeakID());
					float XOffset = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f) / 2.0f;
					TextRender()->Text(0, Position.x - XOffset, YOffset, FontSize, aBuf, -1.0f);
				}
			}
		}
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

	TextRender()->SetRenderFlags(0);
}

void CNamePlates::OnRender()
{
	int ShowDirection = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ShowDirection = g_Config.m_ClVideoShowDirection;
#endif
	if(!g_Config.m_ClNameplates && ShowDirection == 0)
		return;

	// get screen edges to avoid rendering offscreen
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	// expand the edges to prevent popping in/out onscreen
	//
	// it is assumed that the nameplate and all its components fit into a 800x800 box placed directly above the tee
	// this may need to be changed or calculated differently in the future
	ScreenX0 -= 400;
	ScreenX1 += 400;
	//ScreenY0 -= 0;
	ScreenY1 += 800;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apPlayerInfos[i];
		if(!pInfo)
		{
			continue;
		}

		vec2 *pRenderPos;
		if(m_pClient->m_aClients[i].m_SpecCharPresent)
		{
			// Each player can also have a spec char whose nameplate is displayed independently
			pRenderPos = &m_pClient->m_aClients[i].m_SpecChar;
			// don't render offscreen
			if(!(pRenderPos->x < ScreenX0) && !(pRenderPos->x > ScreenX1) && !(pRenderPos->y < ScreenY0) && !(pRenderPos->y > ScreenY1))
			{
				RenderNameplatePos(m_pClient->m_aClients[i].m_SpecChar, pInfo, 0.4f, true);
			}
		}
		if(m_pClient->m_Snap.m_aCharacters[i].m_Active)
		{
			// Only render nameplates for active characters
			pRenderPos = &m_pClient->m_aClients[i].m_RenderPos;
			// don't render offscreen
			if(!(pRenderPos->x < ScreenX0) && !(pRenderPos->x > ScreenX1) && !(pRenderPos->y < ScreenY0) && !(pRenderPos->y > ScreenY1))
			{
				RenderNameplate(
					&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
					&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
					pInfo);
			}
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
		TextRender()->DeleteTextContainer(NamePlate.m_NameTextContainerIndex);
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

	// Quad for the direction arrows above the player
	m_DirectionQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	RenderTools()->QuadContainerAddSprite(m_DirectionQuadContainerIndex, 0.f, 0.f, 22.f);
	Graphics()->QuadContainerUpload(m_DirectionQuadContainerIndex);
}
