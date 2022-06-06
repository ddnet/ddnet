#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/controls.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/nameplates.h>
#include <game/client/gameclient.h>

// nobo copy of countryflags.cpp
#include "playerpics.h"

int CPlayerPics::LoadImageByName(const char *pImgName, int IsDir, int DirType, void *pUser)
{
	CPlayerPics *pSelf = (CPlayerPics *)pUser;

	if(IsDir || !str_endswith(pImgName, ".png"))
		return 0;

	dbg_msg("chiller", "load image '%s'", pImgName);

	// chop .png off
	char aName[128];
	str_copy(aName, pImgName, sizeof(aName));
	aName[str_length(aName) - 4] = 0;
	// return 0;

	char aBuf[128];
	CImageInfo Info;
	str_format(aBuf, sizeof(aBuf), "playerpics/%s.png", aName);
	if(!pSelf->Graphics()->LoadPNG(&Info, aBuf, IStorage::TYPE_ALL))
	{
		char aMsg[128];
		str_format(aMsg, sizeof(aMsg), "failed to load '%s'", aBuf);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
		return 0;
	}
	else
	{
		char aMsg[128];
		str_format(aMsg, sizeof(aMsg), "SUCCESS loading '%s'", aBuf);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
	}

	// add entry
	CPlayerPic CountryFlag;
	str_copy(CountryFlag.m_aPlayerName, aName, sizeof(CountryFlag.m_aPlayerName));
	CountryFlag.m_Texture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	free(Info.m_pData);
	if(g_Config.m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "loaded player pic '%s'", pImgName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aBuf);
	}
	pSelf->m_vPlayerPics.push_back(CountryFlag);
	return 0;
}

void CPlayerPics::LoadPlayerpicsIndexfile()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "playerpics", LoadImageByName, this);
	std::sort(m_vPlayerPics.begin(), m_vPlayerPics.end());
}

int CPlayerPics::Num() const
{
	return m_vPlayerPics.size();
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByName(const char *pName) const
{
	int i = 0;
	for(const auto &PlayerPic : m_vPlayerPics)
	{
		if(str_comp(PlayerPic.m_aPlayerName, pName) == 0)
			return GetByIndex(i);
		i++;
	}
	return NULL;
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByIndex(int Index) const
{
	return &m_vPlayerPics[Index % m_vPlayerPics.size()];
}

void CPlayerPics::Render(const char *pName, const vec4 *pColor, float x, float y, float w, float h)
{
	const CPlayerPic *pFlag = GetByName(pName);
	if(!pFlag)
		return;

	if(pFlag->m_Texture.IsValid())
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		RenderTools()->MapScreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup());
		Graphics()->TextureSet(pFlag->m_Texture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(pColor->r, pColor->g, pColor->b, pColor->a);
		IGraphics::CQuadItem QuadItem(x, y, w, h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
}

void CPlayerPics::RenderNameplate(
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

void CPlayerPics::RenderNameplatePos(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha)
{
	int ClientID = pPlayerInfo->m_ClientID;
	float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNameplatesClanSize / 100.0f;
	float YOffset = Position.y - (g_Config.m_ClRenderPicHeight + (g_Config.m_ClNameplatesClan ? 90.0f : 60.0f));

	// render playerpic
	if(!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn)
	{
		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName;
		if(g_Config.m_ClRenderPic)
		{
			// render player pics
			vec4 Color(1.0f, 1.0f, 1.0f, g_Config.m_ClRenderPicAlpha / 100.0f);
			Render(pName, &Color, Position.x - (g_Config.m_ClRenderPicWidth / 2), YOffset, g_Config.m_ClRenderPicWidth, g_Config.m_ClRenderPicHeight);
		}

		TextRender()->TextColor(1, 1, 1, 1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);

		TextRender()->SetRenderFlags(0);

		char aWarReason[128];
		m_pClient->m_WarList.GetWarReason(pName, aWarReason, sizeof(aWarReason));
		if(g_Config.m_ClNameplatesWarReason && aWarReason[0])
		{
			if(str_comp(aWarReason, m_aNamePlates[ClientID].m_aWarReason) != 0 || FontSizeClan != m_aNamePlates[ClientID].m_WarReasonTextFontSize)
			{
				mem_copy(m_aNamePlates[ClientID].m_aWarReason, aWarReason, sizeof(m_aNamePlates[ClientID].m_aWarReason));
				m_aNamePlates[ClientID].m_WarReasonTextFontSize = FontSizeClan;

				if(m_aNamePlates[ClientID].m_WarReasonTextContainerIndex != -1)
					TextRender()->DeleteTextContainer(m_aNamePlates[ClientID].m_WarReasonTextContainerIndex);

				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, 0, 0, FontSizeClan, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = -1;

				// create nameplates at standard zoom
				float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
				Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
				RenderTools()->MapScreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup());

				m_aNamePlates[ClientID].m_WarReasonTextWidth = TextRender()->TextWidth(0, FontSizeClan, aWarReason, -1, -1.0f);

				m_aNamePlates[ClientID].m_WarReasonTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, aWarReason);
				Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
			}
			YOffset += FontSizeClan;
			STextRenderColor TColor;
			STextRenderColor TOutlineColor;
			TOutlineColor.Set(0.0f, 0.0f, 0.0f, 0.6f);
			TColor.Set(1.0f, 0.0f, 0.0f, 0.7f);
			if(m_aNamePlates[ClientID].m_WarReasonTextContainerIndex != -1)
				TextRender()->RenderTextContainer(m_aNamePlates[ClientID].m_WarReasonTextContainerIndex, &TColor, &TOutlineColor, Position.x - m_aNamePlates[ClientID].m_WarReasonTextWidth / 2.0f, YOffset);
		}
	}
}

void CPlayerPics::OnRender()
{
	if(!g_Config.m_ClNameplates)
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
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
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

void CPlayerPics::ResetNamePlates()
{
	for(auto &NamePlate : m_aNamePlates)
	{
		if(NamePlate.m_NameTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(NamePlate.m_NameTextContainerIndex);

		NamePlate.Reset();
	}
}

void CPlayerPics::OnWindowResize()
{
	ResetNamePlates();
}

void CPlayerPics::OnInit()
{
	ResetNamePlates();
	// load country flags
	m_vPlayerPics.clear();
	LoadPlayerpicsIndexfile();
	if(m_vPlayerPics.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "playerpics", "failed to load directory 'playerpics/'.");
		CPlayerPic DummyEntry;
		mem_zero(DummyEntry.m_aPlayerName, sizeof(DummyEntry.m_aPlayerName));
		m_vPlayerPics.push_back(DummyEntry);
	}
}
