#include <base/system.h>

#include <algorithm>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/maplayers.h>

#include "menu_background.h"

CMenuBackground::CMenuBackground() :
	CBackground(CMapLayers::TYPE_FULL_DESIGN, false)
{
	m_RotationCenter = vec2(0.0f, 0.0f);
	m_AnimationStartPos = vec2(0.0f, 0.0f);
	m_Camera.m_Center = vec2(0.0f, 0.0f);
	m_Camera.m_PrevCenter = vec2(0.0f, 0.0f);
	m_MenuCenter = vec2(0.0f, 0.0f);

	m_ChangedPosition = false;

	ResetPositions();

	m_CurrentPosition = -1;
	m_MoveTime = 0.0f;

	m_IsInit = false;
}

CBackgroundEngineMap *CMenuBackground::CreateBGMap()
{
	return new CMenuMap;
}

void CMenuBackground::OnInit()
{
	m_pBackgroundMap = CreateBGMap();
	m_pMap = m_pBackgroundMap;

	m_IsInit = true;

	m_pImages->m_pClient = GameClient();
	Kernel()->RegisterInterface<CMenuMap>((CMenuMap *)m_pBackgroundMap);
	if(g_Config.m_ClMenuMap[0] != '\0')
		LoadMenuBackground();

	m_Camera.m_pClient = GameClient();
	m_Camera.m_ZoomSet = false;
	m_Camera.m_ZoomSmoothingTarget = 0;
}

void CMenuBackground::ResetPositions()
{
	m_Positions[POS_START] = vec2(500.0f, 500.0f);
	m_Positions[POS_BROWSER_INTERNET] = vec2(1000.0f, 1000.0f);
	m_Positions[POS_BROWSER_LAN] = vec2(1100.0f, 1000.0f);
	m_Positions[POS_DEMOS] = vec2(900.0f, 100.0f);
	m_Positions[POS_NEWS] = vec2(500.0f, 750.0f);
	m_Positions[POS_BROWSER_FAVORITES] = vec2(1250.0f, 500.0f);
	m_Positions[POS_SETTINGS_LANGUAGE] = vec2(500.0f, 1200.0f);
	m_Positions[POS_SETTINGS_GENERAL] = vec2(500.0f, 1000.0f);
	m_Positions[POS_SETTINGS_PLAYER] = vec2(600.0f, 1000.0f);
	m_Positions[POS_SETTINGS_TEE] = vec2(700.0f, 1000.0f);
	m_Positions[POS_SETTINGS_HUD] = vec2(200.0f, 1000.0f);
	m_Positions[POS_SETTINGS_CONTROLS] = vec2(800.0f, 1000.0f);
	m_Positions[POS_SETTINGS_GRAPHICS] = vec2(900.0f, 1000.0f);
	m_Positions[POS_SETTINGS_SOUND] = vec2(1000.0f, 1000.0f);
	m_Positions[POS_SETTINGS_DDNET] = vec2(1200.0f, 200.0f);
	m_Positions[POS_SETTINGS_ASSETS] = vec2(500.0f, 500.0f);
	for(int i = 0; i < POS_BROWSER_CUSTOM_NUM; ++i)
		m_Positions[POS_BROWSER_CUSTOM0 + i] = vec2(500.0f + (75.0f * (float)i), 650.0f - (75.0f * (float)i));
	for(int i = 0; i < POS_SETTINGS_RESERVED_NUM; ++i)
		m_Positions[POS_SETTINGS_RESERVED0 + i] = vec2(0, 0);
	for(int i = 0; i < POS_RESERVED_NUM; ++i)
		m_Positions[POS_RESERVED0 + i] = vec2(0, 0);
}

int CMenuBackground::ThemeScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenuBackground *pSelf = (CMenuBackground *)pUser;
	const char *pSuffix = str_endswith(pName, ".map");
	if(IsDir || !pSuffix)
		return 0;
	char aFullName[128];
	char aThemeName[128];
	str_truncate(aFullName, sizeof(aFullName), pName, pSuffix - pName);

	bool IsDay = false;
	bool IsNight = false;
	if((pSuffix = str_endswith(aFullName, "_day")))
	{
		str_truncate(aThemeName, sizeof(aThemeName), pName, pSuffix - aFullName);
		IsDay = true;
	}
	else if((pSuffix = str_endswith(aFullName, "_night")))
	{
		str_truncate(aThemeName, sizeof(aThemeName), pName, pSuffix - aFullName);
		IsNight = true;
	}
	else
		str_copy(aThemeName, aFullName, sizeof(aThemeName));

	if(str_comp(aThemeName, "none") == 0 || str_comp(aThemeName, "auto") == 0 || str_comp(aThemeName, "rand") == 0) // "none", "auto" and "rand" reserved, disallowed for maps
		return 0;

	// try to edit an existing theme
	for(auto &Theme : pSelf->m_lThemes)
	{
		if(str_comp(Theme.m_Name, aThemeName) == 0)
		{
			if(IsDay)
				Theme.m_HasDay = true;
			if(IsNight)
				Theme.m_HasNight = true;
			return 0;
		}
	}

	// make new theme
	CTheme Theme(aThemeName, IsDay, IsNight);
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "added theme %s from themes/%s", aThemeName, pName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
	pSelf->m_lThemes.push_back(Theme);
	return 0;
}

int CMenuBackground::ThemeIconScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenuBackground *pSelf = (CMenuBackground *)pUser;
	const char *pSuffix = str_endswith(pName, ".png");
	if(IsDir || !pSuffix)
		return 0;

	char aThemeName[128];
	str_truncate(aThemeName, sizeof(aThemeName), pName, pSuffix - pName);

	// save icon for an existing theme
	for(CTheme &Theme : pSelf->m_lThemes) // bit slow but whatever
	{
		if(str_comp(Theme.m_Name, aThemeName) == 0 || (!Theme.m_Name[0] && str_comp(aThemeName, "none") == 0))
		{
			char aBuf[MAX_PATH_LENGTH];
			str_format(aBuf, sizeof(aBuf), "themes/%s", pName);
			CImageInfo Info;
			if(!pSelf->Graphics()->LoadPNG(&Info, aBuf, DirType))
			{
				str_format(aBuf, sizeof(aBuf), "failed to load theme icon from %s", pName);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
				return 0;
			}
			str_format(aBuf, sizeof(aBuf), "loaded theme icon %s", pName);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);

			Theme.m_IconTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
			pSelf->Graphics()->FreePNG(&Info);
			return 0;
		}
	}
	return 0; // no existing theme
}

void CMenuBackground::LoadMenuBackground(bool HasDayHint, bool HasNightHint)
{
	if(!m_IsInit)
		return;

	if(m_Loaded && m_pMap == m_pBackgroundMap)
		m_pMap->Unload();

	m_Loaded = false;
	m_pMap = m_pBackgroundMap;
	m_pLayers = m_pBackgroundLayers;
	m_pImages = m_pBackgroundImages;

	ResetPositions();

	bool NeedImageLoading = false;

	str_copy(m_aMapName, g_Config.m_ClMenuMap, sizeof(m_aMapName));

	if(g_Config.m_ClMenuMap[0] != '\0')
	{
		const char *pMenuMap = g_Config.m_ClMenuMap;
		if(str_comp(pMenuMap, "auto") == 0)
		{
			switch(time_season())
			{
			case SEASON_SPRING:
				pMenuMap = "heavens";
				break;
			case SEASON_SUMMER:
				pMenuMap = "jungle";
				break;
			case SEASON_AUTUMN:
				pMenuMap = "autumn";
				break;
			case SEASON_WINTER:
				pMenuMap = "winter";
				break;
			case SEASON_NEWYEAR:
				pMenuMap = "newyear";
				break;
			}
		}
		else if(str_comp(pMenuMap, "rand") == 0)
		{
			//make sure to load themes
			std::vector<CTheme> &ThemesRef = GetThemes();
			int RandomThemeIndex = rand() % (ThemesRef.size() - PREDEFINED_THEMES_COUNT);
			if(RandomThemeIndex + PREDEFINED_THEMES_COUNT < (int)ThemesRef.size())
				pMenuMap = ThemesRef[RandomThemeIndex + PREDEFINED_THEMES_COUNT].m_Name.cstr();
		}

		char aBuf[128];

		const int HourOfTheDay = time_houroftheday();
		const bool IsDaytime = HourOfTheDay >= 6 && HourOfTheDay < 18;

		if(!m_Loaded && ((HasDayHint && IsDaytime) || (HasNightHint && !IsDaytime)))
		{
			str_format(aBuf, sizeof(aBuf), "themes/%s_%s.map", pMenuMap, IsDaytime ? "day" : "night");
			if(m_pMap->Load(aBuf))
			{
				m_Loaded = true;
			}
		}

		if(!m_Loaded)
		{
			str_format(aBuf, sizeof(aBuf), "themes/%s.map", pMenuMap);
			if(m_pMap->Load(aBuf))
			{
				m_Loaded = true;
			}
		}

		if(!m_Loaded && ((HasDayHint && !IsDaytime) || (HasNightHint && IsDaytime)))
		{
			str_format(aBuf, sizeof(aBuf), "themes/%s_%s.map", pMenuMap, IsDaytime ? "night" : "day");
			if(m_pMap->Load(aBuf))
			{
				m_Loaded = true;
			}
		}

		if(m_Loaded)
		{
			m_pLayers->InitBackground(m_pMap);
			RenderTools()->RenderTilemapGenerateSkip(m_pLayers);
			NeedImageLoading = true;

			CMapLayers::OnMapLoad();
			if(NeedImageLoading)
				m_pImages->LoadBackground(m_pLayers, m_pMap);

			// look for custom positions
			CMapItemLayerTilemap *pTLayer = m_pLayers->GameLayer();
			if(pTLayer)
			{
				int DataIndex = pTLayer->m_Data;
				unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
				void *pTiles = m_pLayers->Map()->GetData(DataIndex);
				unsigned int TileSize = sizeof(CTile);

				if(Size >= pTLayer->m_Width * pTLayer->m_Height * TileSize)
				{
					int x = 0;
					int y = 0;
					for(y = 0; y < pTLayer->m_Height; ++y)
					{
						for(x = 0; x < pTLayer->m_Width; ++x)
						{
							unsigned char Index = ((CTile *)pTiles)[y * pTLayer->m_Width + x].m_Index;
							if(Index >= TILE_CHECKPOINT_FIRST && Index <= TILE_CHECKPOINT_LAST)
							{
								int ArrayIndex = clamp<int>((Index - TILE_CHECKPOINT_FIRST), 0, NUM_POS);
								m_Positions[ArrayIndex] = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
							}

							x += ((CTile *)pTiles)[y * pTLayer->m_Width + x].m_Skip;
						}
					}
				}
			}
		}

		m_LastLoad = time_get();
	}
}

void CMenuBackground::OnMapLoad()
{
	return;
}

void CMenuBackground::OnRender()
{
	return;
}

bool CMenuBackground::Render()
{
	if(!m_Loaded)
		return false;

	if(Client()->State() >= IClient::STATE_ONLINE)
		return false;

	m_Camera.m_Zoom = 0.7f;
	static vec2 Dir = vec2(1.0f, 0.0f);

	float DistToCenter = distance(m_Camera.m_Center, m_RotationCenter);
	if(!m_ChangedPosition && absolute(DistToCenter - (float)g_Config.m_ClRotationRadius) <= 0.5f)
	{
		// do little rotation
		float RotPerTick = 360.0f / (float)g_Config.m_ClRotationSpeed * clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
		Dir = rotate(Dir, RotPerTick);
		m_Camera.m_Center = m_RotationCenter + Dir * (float)g_Config.m_ClRotationRadius;
	}
	else
	{
		// positions for the animation
		vec2 DirToCenter;
		if(DistToCenter > 0.5f)
			DirToCenter = normalize(m_AnimationStartPos - m_RotationCenter);
		else
			DirToCenter = vec2(1, 0);
		vec2 TargetPos = m_RotationCenter + DirToCenter * (float)g_Config.m_ClRotationRadius;
		float Distance = distance(m_AnimationStartPos, TargetPos);
		if(Distance > 0.001f)
			Dir = normalize(m_AnimationStartPos - TargetPos);
		else
			Dir = vec2(1, 0);

		// move time
		m_MoveTime += clamp(Client()->RenderFrameTime(), 0.0f, 0.1f) * g_Config.m_ClCameraSpeed / 10.0f;
		float XVal = 1 - m_MoveTime;
		XVal = pow(XVal, 7.0f);

		m_Camera.m_Center = TargetPos + Dir * (XVal * Distance);
		if(m_CurrentPosition < 0)
		{
			m_AnimationStartPos = m_Camera.m_Center;
			m_MoveTime = 0.0f;
		}

		m_ChangedPosition = false;
	}

	CMapLayers::OnRender();

	m_CurrentPosition = -1;

	return true;
}

CCamera *CMenuBackground::GetCurCamera()
{
	return &m_Camera;
}

void CMenuBackground::ChangePosition(int PositionNumber)
{
	if(PositionNumber != m_CurrentPosition)
	{
		if(PositionNumber >= POS_START && PositionNumber < NUM_POS)
		{
			m_CurrentPosition = PositionNumber;
		}
		else
		{
			m_CurrentPosition = POS_START;
		}

		m_ChangedPosition = true;
	}
	m_AnimationStartPos = m_Camera.m_Center;
	m_RotationCenter = m_Positions[m_CurrentPosition];
	m_MoveTime = 0.0f;
}

std::vector<CTheme> &CMenuBackground::GetThemes()
{
	if(m_lThemes.size() == 0) // not loaded yet
	{
		// when adding more here, make sure to change the value of PREDEFINED_THEMES_COUNT too
		m_lThemes.push_back(CTheme("", true, true)); // no theme
		m_lThemes.push_back(CTheme("auto", true, true)); // auto theme
		m_lThemes.push_back(CTheme("rand", true, true)); // random theme

		Storage()->ListDirectory(IStorage::TYPE_ALL, "themes", ThemeScan, (CMenuBackground *)this);
		Storage()->ListDirectory(IStorage::TYPE_ALL, "themes", ThemeIconScan, (CMenuBackground *)this);

		std::sort(m_lThemes.begin() + PREDEFINED_THEMES_COUNT, m_lThemes.end());
	}
	return m_lThemes;
}
