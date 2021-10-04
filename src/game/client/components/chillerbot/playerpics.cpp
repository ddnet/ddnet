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
	pSelf->m_aPlayerPics.add_unsorted(CountryFlag);
	return 0;
}

void CPlayerPics::LoadPlayerpicsIndexfile()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, "playerpics", LoadImageByName, this);
	m_aPlayerPics.sort_range();
}

void CPlayerPics::OnInit()
{
	// load country flags
	m_aPlayerPics.clear();
	LoadPlayerpicsIndexfile();
	if(!m_aPlayerPics.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "playerpics", "failed to load directory 'playerpics/'.");
		CPlayerPic DummyEntry;
		mem_zero(DummyEntry.m_aPlayerName, sizeof(DummyEntry.m_aPlayerName));
		m_aPlayerPics.add(DummyEntry);
	}
}

int CPlayerPics::Num() const
{
	return m_aPlayerPics.size();
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByName(const char *pName) const
{
	for(int i = 0; i < m_aPlayerPics.size(); i++)
	{
		if(str_comp(m_aPlayerPics[i].m_aPlayerName, pName) == 0)
		{
			return GetByIndex(i);
		}
	}
	return NULL;
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByIndex(int Index) const
{
	return &m_aPlayerPics[maximum(0, Index % m_aPlayerPics.size())];
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
		MapscreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup());
		Graphics()->TextureSet(pFlag->m_Texture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(pColor->r, pColor->g, pColor->b, pColor->a);
		IGraphics::CQuadItem QuadItem(x, y, w, h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
}

void CPlayerPics::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX, pGroup->m_ParallaxY, pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), 1.0f, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
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

void CPlayerPics::RenderNameplatePos(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha)
{
	// render playerpic
	if(!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn)
	{
		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName;
		if(g_Config.m_ClRenderPic)
		{
			// render player pics
			vec4 Color(1.0f, 1.0f, 1.0f, g_Config.m_ClRenderPicAlpha / 100.0f);
			Render(pName, &Color, Position.x - (g_Config.m_ClRenderPicWidth / 2), Position.y - (g_Config.m_ClRenderPicHeight + (g_Config.m_ClNameplatesClan ? 90.0f : 60.0f)), g_Config.m_ClRenderPicWidth, g_Config.m_ClRenderPicHeight);
		}

		TextRender()->TextColor(1, 1, 1, 1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);

		TextRender()->SetRenderFlags(0);
	}
}

void CPlayerPics::OnRender()
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
