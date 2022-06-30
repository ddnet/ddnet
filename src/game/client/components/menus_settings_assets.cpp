#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include "menus.h"

#include <chrono>

using namespace std::chrono_literals;

typedef std::function<void()> TMenuAssetScanLoadedFunc;

struct SMenuAssetScanUser
{
	void *m_pUser;
	TMenuAssetScanLoadedFunc m_LoadedFunc;
};

// IDs of the tabs in the Assets menu
enum
{
	ASSETS_TAB_ENTITIES = 0,
	ASSETS_TAB_GAME = 1,
	ASSETS_TAB_EMOTICONS = 2,
	ASSETS_TAB_PARTICLES = 3,
	ASSETS_TAB_HUD = 4,
	ASSETS_TAB_EXTRAS = 5,
	NUMBER_OF_ASSETS_TABS = 6,
};

void CMenus::LoadEntities(SCustomEntities *pEntitiesItem, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;

	char aBuff[IO_MAX_PATH_LENGTH];

	if(str_comp(pEntitiesItem->m_aName, "default") == 0)
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aBuff, sizeof(aBuff), "editor/entities_clear/%s.png", gs_aModEntitiesNames[i]);
			CImageInfo ImgInfo;
			if(pThis->Graphics()->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
			{
				pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
				pThis->Graphics()->FreePNG(&ImgInfo);

				if(!pEntitiesItem->m_RenderTexture.IsValid())
					pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
			}
		}
	}
	else
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aBuff, sizeof(aBuff), "assets/entities/%s/%s.png", pEntitiesItem->m_aName, gs_aModEntitiesNames[i]);
			CImageInfo ImgInfo;
			if(pThis->Graphics()->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
			{
				pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
				pThis->Graphics()->FreePNG(&ImgInfo);

				if(!pEntitiesItem->m_RenderTexture.IsValid())
					pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
			}
			else
			{
				str_format(aBuff, sizeof(aBuff), "assets/entities/%s.png", pEntitiesItem->m_aName);
				if(pThis->Graphics()->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
				{
					pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
					pThis->Graphics()->FreePNG(&ImgInfo);

					if(!pEntitiesItem->m_RenderTexture.IsValid())
						pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
				}
			}
		}
	}
}

int CMenus::EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		SCustomEntities EntitiesItem;
		str_copy(EntitiesItem.m_aName, pName, sizeof(EntitiesItem.m_aName));
		CMenus::LoadEntities(&EntitiesItem, pUser);
		pThis->m_vEntitiesList.push_back(EntitiesItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, aName, sizeof(EntitiesItem.m_aName));
			CMenus::LoadEntities(&EntitiesItem, pUser);
			pThis->m_vEntitiesList.push_back(EntitiesItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

template<typename TName>
static void LoadAsset(TName *pAssetItem, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
	char aBuff[IO_MAX_PATH_LENGTH];

	if(str_comp(pAssetItem->m_aName, "default") == 0)
	{
		str_format(aBuff, sizeof(aBuff), "%s.png", pAssetName);
		CImageInfo ImgInfo;
		if(pGraphics->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
		{
			pAssetItem->m_RenderTexture = pGraphics->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
			pGraphics->FreePNG(&ImgInfo);
		}
	}
	else
	{
		str_format(aBuff, sizeof(aBuff), "assets/%s/%s.png", pAssetName, pAssetItem->m_aName);
		CImageInfo ImgInfo;
		if(pGraphics->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
		{
			pAssetItem->m_RenderTexture = pGraphics->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
			pGraphics->FreePNG(&ImgInfo);
		}
		else
		{
			str_format(aBuff, sizeof(aBuff), "assets/%s/%s/%s.png", pAssetName, pAssetItem->m_aName, pAssetName);
			if(pGraphics->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
			{
				pAssetItem->m_RenderTexture = pGraphics->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
				pGraphics->FreePNG(&ImgInfo);
			}
		}
	}
}

template<typename TName>
static int AssetScan(const char *pName, int IsDir, int DirType, std::vector<TName> &vAssetList, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		TName AssetItem;
		str_copy(AssetItem.m_aName, pName, sizeof(AssetItem.m_aName));
		LoadAsset(&AssetItem, pAssetName, pGraphics, pUser);
		vAssetList.push_back(AssetItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			TName AssetItem;
			str_copy(AssetItem.m_aName, aName, sizeof(AssetItem.m_aName));
			LoadAsset(&AssetItem, pAssetName, pGraphics, pUser);
			vAssetList.push_back(AssetItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

int CMenus::GameScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vGameList, "game", pGraphics, pUser);
}

int CMenus::EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vEmoticonList, "emoticons", pGraphics, pUser);
}

int CMenus::ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vParticlesList, "particles", pGraphics, pUser);
}

int CMenus::HudScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vHudList, "hud", pGraphics, pUser);
}

int CMenus::ExtrasScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vExtrasList, "extras", pGraphics, pUser);
}

static std::vector<const CMenus::SCustomEntities *> s_vpSearchEntitiesList;
static std::vector<const CMenus::SCustomGame *> s_vpSearchGamesList;
static std::vector<const CMenus::SCustomEmoticon *> s_vpSearchEmoticonsList;
static std::vector<const CMenus::SCustomParticle *> s_vpSearchParticlesList;
static std::vector<const CMenus::SCustomHud *> s_vpSearchHudList;
static std::vector<const CMenus::SCustomExtras *> s_vpSearchExtrasList;

static bool s_InitCustomList[NUMBER_OF_ASSETS_TABS] = {
	true,
};

static size_t s_CustomListSize[NUMBER_OF_ASSETS_TABS] = {
	0,
};

static char s_aFilterString[NUMBER_OF_ASSETS_TABS][50];

static int s_CurCustomTab = ASSETS_TAB_ENTITIES;

static const CMenus::SCustomItem *GetCustomItem(int CurTab, size_t Index)
{
	if(CurTab == ASSETS_TAB_ENTITIES)
		return s_vpSearchEntitiesList[Index];
	else if(CurTab == ASSETS_TAB_GAME)
		return s_vpSearchGamesList[Index];
	else if(CurTab == ASSETS_TAB_EMOTICONS)
		return s_vpSearchEmoticonsList[Index];
	else if(CurTab == ASSETS_TAB_PARTICLES)
		return s_vpSearchParticlesList[Index];
	else if(CurTab == ASSETS_TAB_HUD)
		return s_vpSearchHudList[Index];
	else if(CurTab == ASSETS_TAB_EXTRAS)
		return s_vpSearchExtrasList[Index];

	return NULL;
}

template<typename TName>
void ClearAssetList(std::vector<TName> &vList, IGraphics *pGraphics)
{
	for(size_t i = 0; i < vList.size(); ++i)
	{
		if(vList[i].m_RenderTexture.IsValid())
			pGraphics->UnloadTexture(&(vList[i].m_RenderTexture));
		vList[i].m_RenderTexture = IGraphics::CTextureHandle();
	}
	vList.clear();
}

void CMenus::ClearCustomItems(int CurTab)
{
	if(CurTab == ASSETS_TAB_ENTITIES)
	{
		for(auto &Entity : m_vEntitiesList)
		{
			for(auto &Image : Entity.m_aImages)
			{
				if(Image.m_Texture.IsValid())
					Graphics()->UnloadTexture(&Image.m_Texture);
				Image.m_Texture = IGraphics::CTextureHandle();
			}
		}
		m_vEntitiesList.clear();

		// reload current entities
		m_pClient->m_MapImages.ChangeEntitiesPath(g_Config.m_ClAssetsEntites);
	}
	else if(CurTab == ASSETS_TAB_GAME)
	{
		ClearAssetList(m_vGameList, Graphics());

		// reload current game skin
		GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
	}
	else if(CurTab == ASSETS_TAB_EMOTICONS)
	{
		ClearAssetList(m_vEmoticonList, Graphics());

		// reload current emoticons skin
		GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
	}
	else if(CurTab == ASSETS_TAB_PARTICLES)
	{
		ClearAssetList(m_vParticlesList, Graphics());

		// reload current particles skin
		GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
	}
	else if(CurTab == ASSETS_TAB_HUD)
	{
		ClearAssetList(m_vHudList, Graphics());

		// reload current hud skin
		GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
	}
	else if(CurTab == ASSETS_TAB_EXTRAS)
	{
		ClearAssetList(m_vExtrasList, Graphics());

		// reload current DDNet particles skin
		GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
	}
	s_InitCustomList[CurTab] = true;
}

template<typename TName, typename TCaller>
void InitAssetList(std::vector<TName> &vAssetList, const char *pAssetPath, const char *pAssetName, FS_LISTDIR_CALLBACK pfnCallback, IGraphics *pGraphics, IStorage *pStorage, TCaller Caller)
{
	if(vAssetList.empty())
	{
		TName AssetItem;
		str_copy(AssetItem.m_aName, "default", sizeof(AssetItem.m_aName));
		LoadAsset(&AssetItem, pAssetName, pGraphics, Caller);
		vAssetList.push_back(AssetItem);

		// load assets
		pStorage->ListDirectory(IStorage::TYPE_ALL, pAssetPath, pfnCallback, Caller);
		std::sort(vAssetList.begin(), vAssetList.end());
	}
	if(vAssetList.size() != s_CustomListSize[s_CurCustomTab])
		s_InitCustomList[s_CurCustomTab] = true;
}

template<typename TName>
int InitSearchList(std::vector<const TName *> &vpSearchList, std::vector<TName> &vAssetList)
{
	vpSearchList.clear();
	int ListSize = vAssetList.size();
	for(int i = 0; i < ListSize; ++i)
	{
		const TName *s = &vAssetList[i];

		// filter quick search
		if(s_aFilterString[s_CurCustomTab][0] != '\0' && !str_utf8_find_nocase(s->m_aName, s_aFilterString[s_CurCustomTab]))
			continue;

		vpSearchList.push_back(s);
	}
	return vAssetList.size();
}

void CMenus::RenderSettingsCustom(CUIRect MainView)
{
	CUIRect TabBar, CustomList, QuickSearch, QuickSearchClearButton, DirectoryButton, Page1Tab, Page2Tab, Page3Tab, Page4Tab, Page5Tab, Page6Tab, ReloadButton;

	MainView.HSplitTop(20, &TabBar, &MainView);
	float TabsW = TabBar.w;
	TabBar.VSplitLeft(TabsW / NUMBER_OF_ASSETS_TABS, &Page1Tab, &Page2Tab);
	Page2Tab.VSplitLeft(TabsW / NUMBER_OF_ASSETS_TABS, &Page2Tab, &Page3Tab);
	Page3Tab.VSplitLeft(TabsW / NUMBER_OF_ASSETS_TABS, &Page3Tab, &Page4Tab);
	Page4Tab.VSplitLeft(TabsW / NUMBER_OF_ASSETS_TABS, &Page4Tab, &Page5Tab);
	Page5Tab.VSplitLeft(TabsW / NUMBER_OF_ASSETS_TABS, &Page5Tab, &Page6Tab);

	static int s_aPageTabs[NUMBER_OF_ASSETS_TABS] = {};

	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_ENTITIES], Localize("Entities"), s_CurCustomTab == ASSETS_TAB_ENTITIES, &Page1Tab, 5, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_ENTITIES;
	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_GAME], Localize("Game"), s_CurCustomTab == ASSETS_TAB_GAME, &Page2Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_GAME;
	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_EMOTICONS], Localize("Emoticons"), s_CurCustomTab == ASSETS_TAB_EMOTICONS, &Page3Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_EMOTICONS;
	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_PARTICLES], Localize("Particles"), s_CurCustomTab == ASSETS_TAB_PARTICLES, &Page4Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_PARTICLES;
	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_HUD], Localize("HUD"), s_CurCustomTab == ASSETS_TAB_HUD, &Page5Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_HUD;
	if(DoButton_MenuTab((void *)&s_aPageTabs[ASSETS_TAB_EXTRAS], Localize("Extras"), s_CurCustomTab == ASSETS_TAB_EXTRAS, &Page6Tab, 10, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = ASSETS_TAB_EXTRAS;

	auto LoadStartTime = time_get_nanoseconds();
	SMenuAssetScanUser User;
	User.m_pUser = this;
	User.m_LoadedFunc = [&]() {
		if(time_get_nanoseconds() - LoadStartTime > 500ms)
			RenderLoading(Localize("Loading assets"), "", 0, false);
	};
	if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
	{
		if(m_vEntitiesList.empty())
		{
			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, "default", sizeof(EntitiesItem.m_aName));
			LoadEntities(&EntitiesItem, &User);
			m_vEntitiesList.push_back(EntitiesItem);

			// load entities
			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/entities", EntitiesScan, &User);
			std::sort(m_vEntitiesList.begin(), m_vEntitiesList.end());
		}
		if(m_vEntitiesList.size() != s_CustomListSize[s_CurCustomTab])
			s_InitCustomList[s_CurCustomTab] = true;
	}
	else if(s_CurCustomTab == ASSETS_TAB_GAME)
	{
		InitAssetList(m_vGameList, "assets/game", "game", GameScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
	{
		InitAssetList(m_vEmoticonList, "assets/emoticons", "emoticons", EmoticonsScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
	{
		InitAssetList(m_vParticlesList, "assets/particles", "particles", ParticlesScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_HUD)
	{
		InitAssetList(m_vHudList, "assets/hud", "hud", HudScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
	{
		InitAssetList(m_vExtrasList, "assets/extras", "extras", ExtrasScan, Graphics(), Storage(), &User);
	}

	MainView.HSplitTop(10.0f, 0, &MainView);

	// skin selector
	MainView.HSplitTop(MainView.h - 10.0f - ms_ButtonHeight, &CustomList, &MainView);
	static float s_ScrollValue = 0.0f;
	if(s_InitCustomList[s_CurCustomTab])
	{
		int ListSize = 0;
		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
		{
			s_vpSearchEntitiesList.clear();
			ListSize = m_vEntitiesList.size();
			for(int i = 0; i < ListSize; ++i)
			{
				const SCustomEntities *s = &m_vEntitiesList[i];

				// filter quick search
				if(s_aFilterString[s_CurCustomTab][0] != '\0' && !str_utf8_find_nocase(s->m_aName, s_aFilterString[s_CurCustomTab]))
					continue;

				s_vpSearchEntitiesList.push_back(s);
			}
		}
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
		{
			ListSize = InitSearchList(s_vpSearchGamesList, m_vGameList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
		{
			ListSize = InitSearchList(s_vpSearchEmoticonsList, m_vEmoticonList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
		{
			ListSize = InitSearchList(s_vpSearchParticlesList, m_vParticlesList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
		{
			ListSize = InitSearchList(s_vpSearchHudList, m_vHudList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
		{
			ListSize = InitSearchList(s_vpSearchExtrasList, m_vExtrasList);
		}
		s_InitCustomList[s_CurCustomTab] = false;
		s_CustomListSize[s_CurCustomTab] = ListSize;
	}

	int OldSelected = -1;
	float Margin = 10;
	float TextureWidth = 150;
	float TextureHeight = 150;

	size_t SearchListSize = 0;

	if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
	{
		SearchListSize = s_vpSearchEntitiesList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_GAME)
	{
		SearchListSize = s_vpSearchGamesList.size();
		TextureHeight = 75;
	}
	else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
	{
		SearchListSize = s_vpSearchEmoticonsList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
	{
		SearchListSize = s_vpSearchParticlesList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_HUD)
	{
		SearchListSize = s_vpSearchHudList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
	{
		SearchListSize = s_vpSearchExtrasList.size();
	}

	UiDoListboxStart(&s_InitCustomList[s_CurCustomTab], &CustomList, TextureHeight + 15.0f + 10.0f + Margin, "", "", SearchListSize, CustomList.w / (Margin + TextureWidth), OldSelected, s_ScrollValue, true);
	for(size_t i = 0; i < SearchListSize; ++i)
	{
		const SCustomItem *s = GetCustomItem(s_CurCustomTab, i);
		if(s == NULL)
			continue;

		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetsEntites) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetGame) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetEmoticons) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetParticles) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetHud) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetExtras) == 0)
				OldSelected = i;
		}

		CListboxItem Item = UiDoListboxNextItem(s, OldSelected >= 0 && (size_t)OldSelected == i);
		CUIRect ItemRect = Item.m_Rect;
		ItemRect.Margin(Margin / 2, &ItemRect);
		if(Item.m_Visible)
		{
			CUIRect TextureRect;
			ItemRect.HSplitTop(15, &ItemRect, &TextureRect);
			TextureRect.HSplitTop(10, NULL, &TextureRect);
			UI()->DoLabel(&ItemRect, s->m_aName, ItemRect.h - 2, TEXTALIGN_CENTER);
			if(s->m_RenderTexture.IsValid())
			{
				Graphics()->WrapClamp();
				Graphics()->TextureSet(s->m_RenderTexture);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1, 1, 1, 1);
				IGraphics::CQuadItem QuadItem(TextureRect.x + (TextureRect.w - TextureWidth) / 2, TextureRect.y + (TextureRect.h - TextureHeight) / 2, TextureWidth, TextureHeight);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
				Graphics()->WrapNormal();
			}
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		if(GetCustomItem(s_CurCustomTab, NewSelected)->m_aName[0] != '\0')
		{
			if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
			{
				str_copy(g_Config.m_ClAssetsEntites, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetsEntites));
				m_pClient->m_MapImages.ChangeEntitiesPath(GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
			}
			else if(s_CurCustomTab == ASSETS_TAB_GAME)
			{
				str_copy(g_Config.m_ClAssetGame, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetGame));
				GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			}
			else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
			{
				str_copy(g_Config.m_ClAssetEmoticons, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetEmoticons));
				GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
			}
			else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
			{
				str_copy(g_Config.m_ClAssetParticles, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetParticles));
				GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
			}
			else if(s_CurCustomTab == ASSETS_TAB_HUD)
			{
				str_copy(g_Config.m_ClAssetHud, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetHud));
				GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
			}
			else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
			{
				str_copy(g_Config.m_ClAssetExtras, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetExtras));
				GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
			}
		}
	}

	// render quick search
	{
		MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
		QuickSearch.VSplitLeft(240.0f, &QuickSearch, &DirectoryButton);
		QuickSearch.HSplitTop(5.0f, 0, &QuickSearch);
		const char *pSearchLabel = "\xEF\x80\x82";
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		SLabelProperties Props;
		Props.m_AlignVertically = 0;
		UI()->DoLabel(&QuickSearch, pSearchLabel, 14.0f, TEXTALIGN_LEFT, Props);
		float wSearch = TextRender()->TextWidth(0, 14.0f, pSearchLabel, -1, -1.0f);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);
		QuickSearch.VSplitLeft(wSearch, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);
		QuickSearch.VSplitLeft(QuickSearch.w - 15.0f, &QuickSearch, &QuickSearchClearButton);
		static int s_ClearButton = 0;
		static float s_Offset = 0.0f;
		SUIExEditBoxProperties EditProps;
		if(Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			UI()->SetActiveItem(&s_aFilterString[s_CurCustomTab]);
			EditProps.m_SelectText = true;
		}
		EditProps.m_pEmptyText = Localize("Search");
		if(UIEx()->DoClearableEditBox(&s_aFilterString[s_CurCustomTab], &s_ClearButton, &QuickSearch, s_aFilterString[s_CurCustomTab], sizeof(s_aFilterString[0]), 14.0f, &s_Offset, false, CUI::CORNER_ALL, EditProps))
			s_InitCustomList[s_CurCustomTab] = true;
	}

	DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &ReloadButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, 0);
	static int s_AssetsDirID = 0;
	if(DoButton_Menu(&s_AssetsDirID, Localize("Assets directory"), 0, &DirectoryButton))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		char aBufFull[IO_MAX_PATH_LENGTH + 7];
		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
			str_copy(aBufFull, "assets/entities", sizeof(aBufFull));
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
			str_copy(aBufFull, "assets/game", sizeof(aBufFull));
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
			str_copy(aBufFull, "assets/emoticons", sizeof(aBufFull));
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
			str_copy(aBufFull, "assets/particles", sizeof(aBufFull));
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
			str_copy(aBufFull, "assets/hud", sizeof(aBufFull));
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
			str_copy(aBufFull, "assets/extras", sizeof(aBufFull));
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aBufFull, aBuf, sizeof(aBuf));
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(aBufFull, IStorage::TYPE_SAVE);
		if(!open_file(aBuf))
		{
			dbg_msg("menus", "couldn't open file");
		}
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static int s_AssetsReloadBtnID = 0;
	if(DoButton_Menu(&s_AssetsReloadBtnID, "\xEF\x80\x9E", 0, &ReloadButton, NULL, 15, 5, 0, vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4(1, 1, 1, 0.5f), 0))
	{
		ClearCustomItems(s_CurCustomTab);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetCurFont(NULL);
}

void CMenus::ConchainAssetsEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetsEntites) != 0)
		{
			pThis->m_pClient->m_MapImages.ChangeEntitiesPath(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetGame(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetGame) != 0)
		{
			pThis->GameClient()->LoadGameSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetParticles(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetParticles) != 0)
		{
			pThis->GameClient()->LoadParticlesSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetEmoticons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetEmoticons) != 0)
		{
			pThis->GameClient()->LoadEmoticonsSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetHud) != 0)
		{
			pThis->GameClient()->LoadHudSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetExtras(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetExtras) != 0)
		{
			pThis->GameClient()->LoadExtrasSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}