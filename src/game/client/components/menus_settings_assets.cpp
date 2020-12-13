#include "binds.h"
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include "menus.h"

void CMenus::LoadEntities(SCustomEntities *pEntitiesItem, void *pUser)
{
	CMenus *pThis = (CMenus *)pUser;

	char aBuff[MAX_PATH_LENGTH];

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

				if(pEntitiesItem->m_RenderTexture == -1)
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

				if(pEntitiesItem->m_RenderTexture == -1)
					pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
			}
			else
			{
				str_format(aBuff, sizeof(aBuff), "assets/entities/%s.png", pEntitiesItem->m_aName);
				CImageInfo ImgInfo;
				if(pThis->Graphics()->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
				{
					pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
					pThis->Graphics()->FreePNG(&ImgInfo);

					if(pEntitiesItem->m_RenderTexture == -1)
						pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
				}
			}
		}
	}
}

int CMenus::EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pThis = (CMenus *)pUser;
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
		pThis->m_EntitiesList.add(EntitiesItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, aName, sizeof(EntitiesItem.m_aName));
			CMenus::LoadEntities(&EntitiesItem, pUser);
			pThis->m_EntitiesList.add(EntitiesItem);
		}
	}

	return 0;
}

template<typename TName>
static void LoadAsset(TName *pAssetItem, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
	char aBuff[MAX_PATH_LENGTH];

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
			CImageInfo ImgInfo;
			if(pGraphics->LoadPNG(&ImgInfo, aBuff, IStorage::TYPE_ALL))
			{
				pAssetItem->m_RenderTexture = pGraphics->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, ImgInfo.m_Format, 0);
				pGraphics->FreePNG(&ImgInfo);
			}
		}
	}
}

template<typename TName>
static int AssetScan(const char *pName, int IsDir, int DirType, sorted_array<TName> &AssetList, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
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
		AssetList.add(AssetItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			TName AssetItem;
			str_copy(AssetItem.m_aName, aName, sizeof(AssetItem.m_aName));
			LoadAsset(&AssetItem, pAssetName, pGraphics, pUser);
			AssetList.add(AssetItem);
		}
	}

	return 0;
}

int CMenus::GameScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pMenus = (CMenus *)pUser;
	IGraphics *pGraphics = pMenus->Graphics();
	return AssetScan(pName, IsDir, DirType, pMenus->m_GameList, "game", pGraphics, pUser);
}

int CMenus::EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pMenus = (CMenus *)pUser;
	IGraphics *pGraphics = pMenus->Graphics();
	return AssetScan(pName, IsDir, DirType, pMenus->m_EmoticonList, "emoticons", pGraphics, pUser);
}

int CMenus::ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pMenus = (CMenus *)pUser;
	IGraphics *pGraphics = pMenus->Graphics();
	return AssetScan(pName, IsDir, DirType, pMenus->m_ParticlesList, "particles", pGraphics, pUser);
}

static sorted_array<const CMenus::SCustomEntities *> s_SearchEntitiesList;
static sorted_array<const CMenus::SCustomGame *> s_SearchGamesList;
static sorted_array<const CMenus::SCustomEmoticon *> s_SearchEmoticonsList;
static sorted_array<const CMenus::SCustomParticle *> s_SearchParticlesList;

static bool s_InitCustomList[4] = {
	true,
};

static int s_CustomListSize[4] = {
	0,
};

static char s_aFilterString[4][50];

static int s_CurCustomTab = 0;

static const CMenus::SCustomItem *GetCustomItem(int CurTab, int Index)
{
	if(CurTab == 0)
		return s_SearchEntitiesList[Index];
	else if(CurTab == 1)
		return s_SearchGamesList[Index];
	else if(CurTab == 2)
		return s_SearchEmoticonsList[Index];
	else if(CurTab == 3)
		return s_SearchParticlesList[Index];

	return NULL;
}

template<typename TName>
void ClearAssetList(sorted_array<TName> &List, IGraphics *pGraphics)
{
	for(int i = 0; i < List.size(); ++i)
	{
		if(List[i].m_RenderTexture != -1)
			pGraphics->UnloadTexture(List[i].m_RenderTexture);
		List[i].m_RenderTexture = IGraphics::CTextureHandle();
	}
	List.clear();
}

void CMenus::ClearCustomItems(int CurTab)
{
	if(CurTab == 0)
	{
		for(int i = 0; i < m_EntitiesList.size(); ++i)
		{
			for(auto &Image : m_EntitiesList[i].m_aImages)
			{
				if(Image.m_Texture != -1)
					Graphics()->UnloadTexture(Image.m_Texture);
				Image.m_Texture = IGraphics::CTextureHandle();
			}
		}
		m_EntitiesList.clear();
	}
	else if(CurTab == 1)
	{
		ClearAssetList(m_GameList, Graphics());
	}
	else if(CurTab == 2)
	{
		ClearAssetList(m_EmoticonList, Graphics());
	}
	else if(CurTab == 3)
	{
		ClearAssetList(m_ParticlesList, Graphics());
	}
	s_InitCustomList[CurTab] = true;
}

template<typename TName, typename TCaller>
void InitAssetList(sorted_array<TName> &AssetList, const char *pAssetPath, const char *pAssetName, FS_LISTDIR_CALLBACK pfnCallback, IGraphics *pGraphics, IStorage *pStorage, TCaller Caller)
{
	if(AssetList.size() == 0)
	{
		TName AssetItem;
		str_copy(AssetItem.m_aName, "default", sizeof(AssetItem.m_aName));
		LoadAsset(&AssetItem, pAssetName, pGraphics, Caller);
		AssetList.add(AssetItem);

		// load assets
		pStorage->ListDirectory(IStorage::TYPE_ALL, pAssetPath, pfnCallback, Caller);
	}
	if(AssetList.size() != s_CustomListSize[s_CurCustomTab])
		s_InitCustomList[s_CurCustomTab] = true;
}

template<typename TName>
int InitSearchList(sorted_array<const TName *> &SearchList, sorted_array<TName> &AssetList)
{
	SearchList.clear();
	int ListSize = AssetList.size();
	for(int i = 0; i < ListSize; ++i)
	{
		const TName *s = &AssetList[i];

		// filter quick search
		if(s_aFilterString[s_CurCustomTab][0] != '\0' && !str_find_nocase(s->m_aName, s_aFilterString[s_CurCustomTab]))
			continue;

		SearchList.add_unsorted(s);
	}
	return AssetList.size();
}

void CMenus::RenderSettingsCustom(CUIRect MainView)
{
	CUIRect Label, CustomList, QuickSearch, QuickSearchClearButton, DirectoryButton, Page1Tab, Page2Tab, Page3Tab, Page4Tab, ReloadButton;

	MainView.HSplitTop(20, &Label, &MainView);
	float TabsW = Label.w;
	Label.VSplitLeft(TabsW / 4, &Page1Tab, &Page2Tab);
	Page2Tab.VSplitLeft(TabsW / 4, &Page2Tab, &Page3Tab);
	Page3Tab.VSplitLeft(TabsW / 4, &Page3Tab, &Page4Tab);

	if(DoButton_MenuTab((void *)&Page1Tab, Localize("Entities"), s_CurCustomTab == 0, &Page1Tab, 5, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = 0;
	if(DoButton_MenuTab((void *)&Page2Tab, Localize("Game"), s_CurCustomTab == 1, &Page2Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = 1;
	if(DoButton_MenuTab((void *)&Page3Tab, Localize("Emoticons"), s_CurCustomTab == 2, &Page3Tab, 0, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = 2;
	if(DoButton_MenuTab((void *)&Page4Tab, Localize("Particles"), s_CurCustomTab == 3, &Page4Tab, 10, NULL, NULL, NULL, NULL, 4))
		s_CurCustomTab = 3;

	if(s_CurCustomTab == 0)
	{
		if(m_EntitiesList.size() == 0)
		{
			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, "default", sizeof(EntitiesItem.m_aName));
			LoadEntities(&EntitiesItem, this);
			m_EntitiesList.add(EntitiesItem);

			// load entities
			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/entities", EntitiesScan, this);
		}
		if(m_EntitiesList.size() != s_CustomListSize[s_CurCustomTab])
			s_InitCustomList[s_CurCustomTab] = true;
	}
	else if(s_CurCustomTab == 1)
	{
		InitAssetList(m_GameList, "assets/game", "game", GameScan, Graphics(), Storage(), this);
	}
	else if(s_CurCustomTab == 2)
	{
		InitAssetList(m_EmoticonList, "assets/emoticons", "emoticons", EmoticonsScan, Graphics(), Storage(), this);
	}
	else if(s_CurCustomTab == 3)
	{
		InitAssetList(m_ParticlesList, "assets/particles", "particles", ParticlesScan, Graphics(), Storage(), this);
	}

	MainView.HSplitTop(10.0f, 0, &MainView);

	// skin selector
	MainView.HSplitTop(MainView.h - 10.0f - ms_ButtonHeight, &CustomList, &MainView);
	static float s_ScrollValue = 0.0f;
	if(s_InitCustomList[s_CurCustomTab])
	{
		int ListSize = 0;
		if(s_CurCustomTab == 0)
		{
			s_SearchEntitiesList.clear();
			ListSize = m_EntitiesList.size();
			for(int i = 0; i < ListSize; ++i)
			{
				const SCustomEntities *s = &m_EntitiesList[i];

				// filter quick search
				if(s_aFilterString[s_CurCustomTab][0] != '\0' && !str_find_nocase(s->m_aName, s_aFilterString[s_CurCustomTab]))
					continue;

				s_SearchEntitiesList.add_unsorted(s);
			}
		}
		else if(s_CurCustomTab == 1)
		{
			ListSize = InitSearchList(s_SearchGamesList, m_GameList);
		}
		else if(s_CurCustomTab == 2)
		{
			ListSize = InitSearchList(s_SearchEmoticonsList, m_EmoticonList);
		}
		else if(s_CurCustomTab == 3)
		{
			ListSize = InitSearchList(s_SearchParticlesList, m_ParticlesList);
		}
		s_InitCustomList[s_CurCustomTab] = false;
		s_CustomListSize[s_CurCustomTab] = ListSize;
	}

	int OldSelected = -1;
	float Margin = 10;
	float TextureWidth = 150;
	float TextureHeight = 150;

	int SearchListSize = 0;

	if(s_CurCustomTab == 0)
	{
		SearchListSize = s_SearchEntitiesList.size();
	}
	else if(s_CurCustomTab == 1)
	{
		SearchListSize = s_SearchGamesList.size();
		TextureHeight = 75;
	}
	else if(s_CurCustomTab == 2)
	{
		SearchListSize = s_SearchEmoticonsList.size();
	}
	else if(s_CurCustomTab == 3)
	{
		SearchListSize = s_SearchParticlesList.size();
	}

	UiDoListboxStart(&s_InitCustomList[s_CurCustomTab], &CustomList, TextureHeight + 15.0f + 10.0f + Margin, "", "", SearchListSize, CustomList.w / (Margin + TextureWidth), OldSelected, s_ScrollValue, true);
	for(int i = 0; i < SearchListSize; ++i)
	{
		const SCustomItem *s = GetCustomItem(s_CurCustomTab, i);
		if(s == NULL)
			continue;

		if(s_CurCustomTab == 0)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetsEntites) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == 1)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetGame) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == 2)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetEmoticons) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == 3)
		{
			if(str_comp(s->m_aName, g_Config.m_ClAssetParticles) == 0)
				OldSelected = i;
		}

		CListboxItem Item = UiDoListboxNextItem(s, OldSelected == i);
		CUIRect ItemRect = Item.m_Rect;
		ItemRect.Margin(Margin / 2, &ItemRect);
		if(Item.m_Visible)
		{
			CUIRect TextureRect;
			ItemRect.HSplitTop(15, &ItemRect, &TextureRect);
			TextureRect.HSplitTop(10, NULL, &TextureRect);
			UI()->DoLabelScaled(&ItemRect, s->m_aName, ItemRect.h - 2, 0);
			if(s->m_RenderTexture != -1)
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
			if(s_CurCustomTab == 0)
			{
				str_copy(g_Config.m_ClAssetsEntites, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetsEntites));
				m_pClient->m_pMapimages->ChangeEntitiesPath(GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
			}
			else if(s_CurCustomTab == 1)
			{
				str_copy(g_Config.m_ClAssetGame, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetGame));
				GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			}
			else if(s_CurCustomTab == 2)
			{
				str_copy(g_Config.m_ClAssetEmoticons, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetEmoticons));
				GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
			}
			else if(s_CurCustomTab == 3)
			{
				str_copy(g_Config.m_ClAssetParticles, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName, sizeof(g_Config.m_ClAssetParticles));
				GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
			}
		}
	}

	// render quick search
	{
		MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
		QuickSearch.VSplitLeft(240.0f, &QuickSearch, &DirectoryButton);
		QuickSearch.HSplitTop(5.0f, 0, &QuickSearch);
		const char *pSearchLabel = "\xEE\xA2\xB6";
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		UI()->DoLabelScaled(&QuickSearch, pSearchLabel, 14.0f, -1, -1, 0);
		float wSearch = TextRender()->TextWidth(0, 14.0f, pSearchLabel, -1, -1.0f);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);
		QuickSearch.VSplitLeft(wSearch, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);
		QuickSearch.VSplitLeft(QuickSearch.w - 15.0f, &QuickSearch, &QuickSearchClearButton);
		static int s_ClearButton = 0;
		static float Offset = 0.0f;
		if(Input()->KeyPress(KEY_F) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
			UI()->SetActiveItem(&s_aFilterString[s_CurCustomTab]);
		if(DoClearableEditBox(&s_aFilterString[s_CurCustomTab], &s_ClearButton, &QuickSearch, s_aFilterString[s_CurCustomTab], sizeof(s_aFilterString[0]), 14.0f, &Offset, false, CUI::CORNER_ALL, Localize("Search")))
			s_InitCustomList[s_CurCustomTab] = true;
	}

	DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &ReloadButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, 0);
	if(DoButton_Menu(&DirectoryButton, Localize("Assets directory"), 0, &DirectoryButton))
	{
		char aBuf[MAX_PATH_LENGTH];
		char aBufFull[MAX_PATH_LENGTH + 7];
		if(s_CurCustomTab == 0)
			str_copy(aBufFull, "assets/entities", sizeof(aBufFull));
		else if(s_CurCustomTab == 1)
			str_copy(aBufFull, "assets/game", sizeof(aBufFull));
		else if(s_CurCustomTab == 2)
			str_copy(aBufFull, "assets/emoticons", sizeof(aBufFull));
		else if(s_CurCustomTab == 3)
			str_copy(aBufFull, "assets/particles", sizeof(aBufFull));
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aBufFull, aBuf, sizeof(aBuf));
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(aBufFull, IStorage::TYPE_SAVE);
		str_format(aBufFull, sizeof(aBufFull), "file://%s", aBuf);
		if(!open_link(aBufFull))
		{
			dbg_msg("menus", "couldn't open link");
		}
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(DoButton_Menu(&ReloadButton, "\xEE\x97\x95", 0, &ReloadButton, NULL, 15, 5, 0, vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4(1, 1, 1, 0.5f), 0))
	{
		ClearCustomItems(s_CurCustomTab);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetCurFont(NULL);
}
