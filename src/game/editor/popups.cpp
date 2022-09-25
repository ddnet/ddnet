/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/color.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <limits>

#include <game/client/ui_scrollregion.h>

#include "editor.h"

// popup menu handling
static struct
{
	CUIRect m_Rect;
	void *m_pId;
	int (*m_pfnFunc)(CEditor *pEditor, CUIRect Rect, void *pContext);
	int m_IsMenu;
	void *m_pContext;
} s_UiPopups[8];

static int g_UiNumPopups = 0;

void CEditor::UiInvokePopupMenu(void *pID, int Flags, float x, float y, float Width, float Height, int (*pfnFunc)(CEditor *pEditor, CUIRect Rect, void *pContext), void *pContext)
{
	if(g_UiNumPopups > 7)
		return;
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", "invoked");
	if(x + Width > UI()->Screen()->w)
		x -= Width;
	if(y + Height > UI()->Screen()->h)
		y -= Height;
	s_UiPopups[g_UiNumPopups].m_pId = pID;
	s_UiPopups[g_UiNumPopups].m_IsMenu = Flags;
	s_UiPopups[g_UiNumPopups].m_Rect.x = x;
	s_UiPopups[g_UiNumPopups].m_Rect.y = y;
	s_UiPopups[g_UiNumPopups].m_Rect.w = Width;
	s_UiPopups[g_UiNumPopups].m_Rect.h = Height;
	s_UiPopups[g_UiNumPopups].m_pfnFunc = pfnFunc;
	s_UiPopups[g_UiNumPopups].m_pContext = pContext;
	g_UiNumPopups++;
}

void CEditor::UiDoPopupMenu()
{
	for(int i = 0; i < g_UiNumPopups; i++)
	{
		bool Inside = UI()->MouseInside(&s_UiPopups[i].m_Rect);
		UI()->SetHotItem(&s_UiPopups[i].m_pId);

		if(Inside)
			m_MouseInsidePopup = true;

		if(UI()->CheckActiveItem(&s_UiPopups[i].m_pId))
		{
			if(!UI()->MouseButton(0))
			{
				if(!Inside)
				{
					g_UiNumPopups--;
					m_PopupEventWasActivated = false;
				}
				UI()->SetActiveItem(nullptr);
			}
		}
		else if(UI()->HotItem() == &s_UiPopups[i].m_pId)
		{
			if(UI()->MouseButton(0))
				UI()->SetActiveItem(&s_UiPopups[i].m_pId);
		}

		int Corners = IGraphics::CORNER_ALL;
		if(s_UiPopups[i].m_IsMenu)
			Corners = IGraphics::CORNER_R | IGraphics::CORNER_B;

		CUIRect r = s_UiPopups[i].m_Rect;
		r.Draw(ColorRGBA(0.5f, 0.5f, 0.5f, 0.75f), Corners, 3.0f);
		r.Margin(1.0f, &r);
		r.Draw(ColorRGBA(0, 0, 0, 0.75f), Corners, 3.0f);
		r.Margin(4.0f, &r);

		if(s_UiPopups[i].m_pfnFunc(this, r, s_UiPopups[i].m_pContext))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			g_UiNumPopups--;
			m_PopupEventWasActivated = false;
		}

		if(Input()->KeyPress(KEY_ESCAPE))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			g_UiNumPopups--;
			m_PopupEventWasActivated = false;
		}
	}
}

bool CEditor::UiPopupExists(void *pid)
{
	for(int i = 0; i < g_UiNumPopups; i++)
	{
		if(s_UiPopups[i].m_pId == pid)
			return true;
	}

	return false;
}

bool CEditor::UiPopupOpen()
{
	return g_UiNumPopups > 0;
}

int CEditor::PopupGroup(CEditor *pEditor, CUIRect View, void *pContext)
{
	// remove group button
	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;

	// don't allow deletion of game group
	if(pEditor->m_Map.m_pGameGroup != pEditor->GetSelectedGroup())
	{
		if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete group", 0, &Button, 0, "Delete group"))
		{
			pEditor->m_Map.DeleteGroup(pEditor->m_SelectedGroup);
			pEditor->m_SelectedGroup = maximum(0, pEditor->m_SelectedGroup - 1);
			return 1;
		}
	}
	else
	{
		if(pEditor->DoButton_Editor(&s_DeleteButton, "Clean up game tiles", 0, &Button, 0, "Removes game tiles that aren't based on a layer"))
		{
			// gather all tile layers
			std::vector<CLayerTiles *> vpLayers;
			for(auto &pLayer : pEditor->m_Map.m_pGameGroup->m_vpLayers)
			{
				if(pLayer != pEditor->m_Map.m_pGameLayer && pLayer->m_Type == LAYERTYPE_TILES)
					vpLayers.push_back(static_cast<CLayerTiles *>(pLayer));
			}

			// search for unneeded game tiles
			CLayerTiles *pGameLayer = pEditor->m_Map.m_pGameLayer;
			for(int y = 0; y < pGameLayer->m_Height; ++y)
				for(int x = 0; x < pGameLayer->m_Width; ++x)
				{
					if(pGameLayer->m_pTiles[y * pGameLayer->m_Width + x].m_Index > static_cast<unsigned char>(TILE_NOHOOK))
						continue;

					bool Found = false;
					for(const auto &pLayer : vpLayers)
					{
						if(x < pLayer->m_Width && y < pLayer->m_Height && pLayer->m_pTiles[y * pLayer->m_Width + x].m_Index)
						{
							Found = true;
							break;
						}
					}

					if(!Found)
					{
						pGameLayer->m_pTiles[y * pGameLayer->m_Width + x].m_Index = TILE_AIR;
						pEditor->m_Map.m_Modified = true;
					}
				}

			return 1;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pTeleLayer)
	{
		// new tele layer
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewTeleLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewTeleLayerButton, "Add tele layer", 0, &Button, 0, "Creates a new tele layer"))
		{
			CLayer *pTeleLayer = new CLayerTele(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeTeleLayer(pTeleLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTeleLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_Brush.Clear();
			return 1;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pSpeedupLayer)
	{
		// new speedup layer
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewSpeedupLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewSpeedupLayerButton, "Add speedup layer", 0, &Button, 0, "Creates a new speedup layer"))
		{
			CLayer *pSpeedupLayer = new CLayerSpeedup(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeSpeedupLayer(pSpeedupLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSpeedupLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_Brush.Clear();
			return 1;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pTuneLayer)
	{
		// new tune layer
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewTuneLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewTuneLayerButton, "Add tune layer", 0, &Button, 0, "Creates a new tuning layer"))
		{
			CLayer *pTuneLayer = new CLayerTune(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeTuneLayer(pTuneLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTuneLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_Brush.Clear();
			return 1;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pFrontLayer)
	{
		// new front layer
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewFrontLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewFrontLayerButton, "Add front layer", 0, &Button, 0, "Creates a new item layer"))
		{
			CLayer *pFrontLayer = new CLayerFront(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeFrontLayer(pFrontLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pFrontLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_Brush.Clear();
			return 1;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pSwitchLayer)
	{
		// new Switch layer
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewSwitchLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewSwitchLayerButton, "Add switch layer", 0, &Button, 0, "Creates a new switch layer"))
		{
			CLayer *pSwitchLayer = new CLayerSwitch(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeSwitchLayer(pSwitchLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSwitchLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_Brush.Clear();
			return 1;
		}
	}

	// new quad layer
	View.HSplitBottom(5.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewQuadLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewQuadLayerButton, "Add quads layer", 0, &Button, 0, "Creates a new quad layer"))
	{
		CLayer *pQuadLayer = new CLayerQuads;
		pQuadLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pQuadLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return 1;
	}

	// new tile layer
	View.HSplitBottom(5.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewTileLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewTileLayerButton, "Add tile layer", 0, &Button, 0, "Creates a new tile layer"))
	{
		CLayer *pTileLayer = new CLayerTiles(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
		pTileLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTileLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return 1;
	}

	// new sound layer
	View.HSplitBottom(5.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewSoundLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewSoundLayerButton, "Add sound layer", 0, &Button, 0, "Creates a new sound layer"))
	{
		CLayer *pSoundLayer = new CLayerSounds;
		pSoundLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSoundLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return 1;
	}

	// group name
	if(!pEditor->GetSelectedGroup()->m_GameGroup)
	{
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		pEditor->UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_LEFT);
		Button.VSplitLeft(40.0f, nullptr, &Button);
		static float s_Name = 0;
		if(pEditor->DoEditBox(&s_Name, &Button, pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_aName, sizeof(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_aName), 10.0f, &s_Name))
			pEditor->m_Map.m_Modified = true;
	}

	enum
	{
		PROP_ORDER = 0,
		PROP_POS_X,
		PROP_POS_Y,
		PROP_PARA_X,
		PROP_PARA_Y,
		PROP_CUSTOM_ZOOM,
		PROP_PARA_ZOOM,
		PROP_USE_CLIPPING,
		PROP_CLIP_X,
		PROP_CLIP_Y,
		PROP_CLIP_W,
		PROP_CLIP_H,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Order", pEditor->m_SelectedGroup, PROPTYPE_INT_STEP, 0, (int)pEditor->m_Map.m_vpGroups.size() - 1},
		{"Pos X", -pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetX, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos Y", -pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetY, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Para X", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxX, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Para Y", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxY, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Custom Zoom", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_CustomParallaxZoom, PROPTYPE_BOOL, 0, 1},
		{"Para Zoom", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxZoom, PROPTYPE_INT_SCROLL, -1000000, 1000000},

		{"Use Clipping", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_UseClipping, PROPTYPE_BOOL, 0, 1},
		{"Clip X", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipX, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Clip Y", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipY, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Clip W", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipW, PROPTYPE_INT_SCROLL, 0, 1000000},
		{"Clip H", pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipH, PROPTYPE_INT_SCROLL, 0, 1000000},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;

	// cut the properties that isn't needed
	if(pEditor->GetSelectedGroup()->m_GameGroup)
		aProps[PROP_POS_X].m_pName = nullptr;

	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		pEditor->m_Map.m_Modified = true;

	if(Prop == PROP_ORDER)
		pEditor->m_SelectedGroup = pEditor->m_Map.SwapGroups(pEditor->m_SelectedGroup, NewVal);

	// these can not be changed on the game group
	if(!pEditor->GetSelectedGroup()->m_GameGroup)
	{
		if(Prop == PROP_PARA_X)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxX = NewVal;
		else if(Prop == PROP_PARA_Y)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxY = NewVal;
		else if(Prop == PROP_CUSTOM_ZOOM)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_CustomParallaxZoom = NewVal;
		else if(Prop == PROP_PARA_ZOOM)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_CustomParallaxZoom = 1;
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxZoom = NewVal;
		}
		else if(Prop == PROP_POS_X)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetX = -NewVal;
		else if(Prop == PROP_POS_Y)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetY = -NewVal;
		else if(Prop == PROP_USE_CLIPPING)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_UseClipping = NewVal;
		else if(Prop == PROP_CLIP_X)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipX = NewVal;
		else if(Prop == PROP_CLIP_Y)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipY = NewVal;
		else if(Prop == PROP_CLIP_W)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipW = NewVal;
		else if(Prop == PROP_CLIP_H)
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipH = NewVal;

		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->OnEdited();
	}

	return 0;
}

int CEditor::PopupLayer(CEditor *pEditor, CUIRect View, void *pContext)
{
	CLayerPopupContext *pPopup = (CLayerPopupContext *)pContext;

	// remove layer button
	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;

	if(pPopup->m_vpLayers.size() > 1)
	{
		return CLayerTiles::RenderCommonProperties(pPopup->m_CommonPropState, pEditor, &View, pPopup->m_vpLayers);
	}

	// duplicate layer button
	CUIRect DupButton;
	static int s_DuplicationButton = 0;
	View.HSplitBottom(4.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &DupButton);

	if(!pEditor->IsSpecialLayer(pEditor->GetSelectedLayer(0)))
	{
		if(pEditor->DoButton_Editor(&s_DuplicationButton, "Duplicate layer", 0, &DupButton, 0, "Duplicates the layer"))
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->DuplicateLayer(pEditor->m_vSelectedLayers[0]);
			return 1;
		}
	}

	// don't allow deletion of game layer
	if(pEditor->m_Map.m_pGameLayer != pEditor->GetSelectedLayer(0) &&
		pEditor->DoButton_Editor(&s_DeleteButton, "Delete layer", 0, &Button, 0, "Deletes the layer"))
	{
		if(pEditor->GetSelectedLayer(0) == pEditor->m_Map.m_pFrontLayer)
			pEditor->m_Map.m_pFrontLayer = nullptr;
		if(pEditor->GetSelectedLayer(0) == pEditor->m_Map.m_pTeleLayer)
			pEditor->m_Map.m_pTeleLayer = nullptr;
		if(pEditor->GetSelectedLayer(0) == pEditor->m_Map.m_pSpeedupLayer)
			pEditor->m_Map.m_pSpeedupLayer = nullptr;
		if(pEditor->GetSelectedLayer(0) == pEditor->m_Map.m_pSwitchLayer)
			pEditor->m_Map.m_pSwitchLayer = nullptr;
		if(pEditor->GetSelectedLayer(0) == pEditor->m_Map.m_pTuneLayer)
			pEditor->m_Map.m_pTuneLayer = nullptr;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->DeleteLayer(pEditor->m_vSelectedLayers[0]);
		return 1;
	}

	// layer name
	// if(pEditor->m_Map.m_pGameLayer != pEditor->GetSelectedLayer(0))
	if(pEditor->m_Map.m_pGameLayer != pEditor->GetSelectedLayer(0) && pEditor->m_Map.m_pTeleLayer != pEditor->GetSelectedLayer(0) && pEditor->m_Map.m_pSpeedupLayer != pEditor->GetSelectedLayer(0) && pEditor->m_Map.m_pFrontLayer != pEditor->GetSelectedLayer(0) && pEditor->m_Map.m_pSwitchLayer != pEditor->GetSelectedLayer(0) && pEditor->m_Map.m_pTuneLayer != pEditor->GetSelectedLayer(0))
	{
		View.HSplitBottom(5.0f, &View, &Button);
		View.HSplitBottom(12.0f, &View, &Button);
		pEditor->UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_LEFT);
		Button.VSplitLeft(40.0f, nullptr, &Button);
		static float s_Name = 0;
		if(pEditor->DoEditBox(&s_Name, &Button, pEditor->GetSelectedLayer(0)->m_aName, sizeof(pEditor->GetSelectedLayer(0)->m_aName), 10.0f, &s_Name))
			pEditor->m_Map.m_Modified = true;
	}

	View.HSplitBottom(10.0f, &View, nullptr);

	CLayerGroup *pCurrentGroup = pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup];
	CLayer *pCurrentLayer = pEditor->GetSelectedLayer(0);

	enum
	{
		PROP_GROUP = 0,
		PROP_ORDER,
		PROP_HQ,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Group", pEditor->m_SelectedGroup, PROPTYPE_INT_STEP, 0, (int)pEditor->m_Map.m_vpGroups.size() - 1},
		{"Order", pEditor->m_vSelectedLayers[0], PROPTYPE_INT_STEP, 0, (int)pCurrentGroup->m_vpLayers.size() - 1},
		{"Detail", pCurrentLayer && pCurrentLayer->m_Flags & LAYERFLAG_DETAIL, PROPTYPE_BOOL, 0, 1},
		{nullptr},
	};

	// if(pEditor->m_Map.m_pGameLayer == pEditor->GetSelectedLayer(0)) // don't use Group and Detail from the selection if this is the game layer
	if(pEditor->m_Map.m_pGameLayer == pEditor->GetSelectedLayer(0) || pEditor->m_Map.m_pTeleLayer == pEditor->GetSelectedLayer(0) || pEditor->m_Map.m_pSpeedupLayer == pEditor->GetSelectedLayer(0) || pEditor->m_Map.m_pFrontLayer == pEditor->GetSelectedLayer(0) || pEditor->m_Map.m_pSwitchLayer == pEditor->GetSelectedLayer(0) || pEditor->m_Map.m_pTuneLayer == pEditor->GetSelectedLayer(0)) // don't use Group and Detail from the selection if this is the game layer
	{
		aProps[0].m_Type = PROPTYPE_NULL;
		aProps[2].m_Type = PROPTYPE_NULL;
	}

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		pEditor->m_Map.m_Modified = true;

	if(Prop == PROP_ORDER)
		pEditor->SelectLayer(pCurrentGroup->SwapLayers(pEditor->m_vSelectedLayers[0], NewVal));
	else if(Prop == PROP_GROUP && pCurrentLayer && pCurrentLayer->m_Type != LAYERTYPE_GAME)
	{
		if(NewVal >= 0 && (size_t)NewVal < pEditor->m_Map.m_vpGroups.size())
		{
			auto Position = std::find(pCurrentGroup->m_vpLayers.begin(), pCurrentGroup->m_vpLayers.end(), pCurrentLayer);
			if(Position != pCurrentGroup->m_vpLayers.end())
				pCurrentGroup->m_vpLayers.erase(Position);
			pEditor->m_Map.m_vpGroups[NewVal]->m_vpLayers.push_back(pCurrentLayer);
			pEditor->m_SelectedGroup = NewVal;
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[NewVal]->m_vpLayers.size() - 1);
		}
	}
	else if(Prop == PROP_HQ && pCurrentLayer)
	{
		pCurrentLayer->m_Flags &= ~LAYERFLAG_DETAIL;
		if(NewVal)
			pCurrentLayer->m_Flags |= LAYERFLAG_DETAIL;
	}

	if(!pCurrentLayer)
		return true;
	return pCurrentLayer->RenderProperties(&View);
}

int CEditor::PopupQuad(CEditor *pEditor, CUIRect View, void *pContext)
{
	std::vector<CQuad *> vpQuads = pEditor->GetSelectedQuads();
	CQuad *pCurrentQuad = vpQuads[pEditor->m_SelectedQuadIndex];

	CUIRect Button;

	// delete button
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;
	if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, "Deletes the current quad"))
	{
		CLayerQuads *pLayer = (CLayerQuads *)pEditor->GetSelectedLayerType(0, LAYERTYPE_QUADS);
		if(pLayer)
		{
			pEditor->m_Map.m_Modified = true;
			pEditor->DeleteSelectedQuads();
		}
		return 1;
	}

	// aspect ratio button
	View.HSplitBottom(10.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	CLayerQuads *pLayer = (CLayerQuads *)pEditor->GetSelectedLayerType(0, LAYERTYPE_QUADS);
	if(pLayer && pLayer->m_Image >= 0 && (size_t)pLayer->m_Image < pEditor->m_Map.m_vpImages.size())
	{
		static int s_AspectRatioButton = 0;
		if(pEditor->DoButton_Editor(&s_AspectRatioButton, "Aspect ratio", 0, &Button, 0, "Resizes the current Quad based on the aspect ratio of the image"))
		{
			for(auto &pQuad : vpQuads)
			{
				int Top = pQuad->m_aPoints[0].y;
				int Left = pQuad->m_aPoints[0].x;
				int Right = pQuad->m_aPoints[0].x;

				for(int k = 1; k < 4; k++)
				{
					if(pQuad->m_aPoints[k].y < Top)
						Top = pQuad->m_aPoints[k].y;
					if(pQuad->m_aPoints[k].x < Left)
						Left = pQuad->m_aPoints[k].x;
					if(pQuad->m_aPoints[k].x > Right)
						Right = pQuad->m_aPoints[k].x;
				}

				int Height = (Right - Left) * pEditor->m_Map.m_vpImages[pLayer->m_Image]->m_Height / pEditor->m_Map.m_vpImages[pLayer->m_Image]->m_Width;

				pQuad->m_aPoints[0].x = Left;
				pQuad->m_aPoints[0].y = Top;
				pQuad->m_aPoints[1].x = Right;
				pQuad->m_aPoints[1].y = Top;
				pQuad->m_aPoints[2].x = Left;
				pQuad->m_aPoints[2].y = Top + Height;
				pQuad->m_aPoints[3].x = Right;
				pQuad->m_aPoints[3].y = Top + Height;
				pEditor->m_Map.m_Modified = true;
			}
			return 1;
		}
	}

	// align button
	View.HSplitBottom(6.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_AlignButton = 0;
	if(pEditor->DoButton_Editor(&s_AlignButton, "Align", 0, &Button, 0, "Aligns coordinates of the quad points"))
	{
		for(auto &pQuad : vpQuads)
		{
			for(int k = 1; k < 4; k++)
			{
				pQuad->m_aPoints[k].x = 1000.0f * (pQuad->m_aPoints[k].x / 1000);
				pQuad->m_aPoints[k].y = 1000.0f * (pQuad->m_aPoints[k].y / 1000);
			}
			pEditor->m_Map.m_Modified = true;
		}
		return 1;
	}

	// square button
	View.HSplitBottom(6.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_Button = 0;
	if(pEditor->DoButton_Editor(&s_Button, "Square", 0, &Button, 0, "Squares the current quad"))
	{
		for(auto &pQuad : vpQuads)
		{
			int Top = pQuad->m_aPoints[0].y;
			int Left = pQuad->m_aPoints[0].x;
			int Bottom = pQuad->m_aPoints[0].y;
			int Right = pQuad->m_aPoints[0].x;

			for(int k = 1; k < 4; k++)
			{
				if(pQuad->m_aPoints[k].y < Top)
					Top = pQuad->m_aPoints[k].y;
				if(pQuad->m_aPoints[k].x < Left)
					Left = pQuad->m_aPoints[k].x;
				if(pQuad->m_aPoints[k].y > Bottom)
					Bottom = pQuad->m_aPoints[k].y;
				if(pQuad->m_aPoints[k].x > Right)
					Right = pQuad->m_aPoints[k].x;
			}

			pQuad->m_aPoints[0].x = Left;
			pQuad->m_aPoints[0].y = Top;
			pQuad->m_aPoints[1].x = Right;
			pQuad->m_aPoints[1].y = Top;
			pQuad->m_aPoints[2].x = Left;
			pQuad->m_aPoints[2].y = Bottom;
			pQuad->m_aPoints[3].x = Right;
			pQuad->m_aPoints[3].y = Bottom;
			pEditor->m_Map.m_Modified = true;
		}
		return 1;
	}

	// slice button
	View.HSplitBottom(6.0f, &View, &Button);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_SliceButton = 0;
	if(pEditor->DoButton_Editor(&s_SliceButton, "Slice", 0, &Button, 0, "Enables quad knife mode"))
	{
		pEditor->m_QuadKnifeCount = 0;
		pEditor->m_QuadKnifeActive = true;
		return 1;
	}

	enum
	{
		PROP_ORDER = 0,
		PROP_POS_X,
		PROP_POS_Y,
		PROP_POS_ENV,
		PROP_POS_ENV_OFFSET,
		PROP_COLOR_ENV,
		PROP_COLOR_ENV_OFFSET,
		NUM_PROPS,
	};

	int NumQuads = pLayer ? (int)pLayer->m_vQuads.size() : 0;
	CProperty aProps[] = {
		{"Order", pEditor->m_vSelectedQuads[pEditor->m_SelectedQuadIndex], PROPTYPE_INT_STEP, 0, NumQuads},
		{"Pos X", fx2i(pCurrentQuad->m_aPoints[4].x), PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos Y", fx2i(pCurrentQuad->m_aPoints[4].y), PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos. Env", pCurrentQuad->m_PosEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Pos. TO", pCurrentQuad->m_PosEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Color Env", pCurrentQuad->m_ColorEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Color TO", pCurrentQuad->m_ColorEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},

		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		pEditor->m_Map.m_Modified = true;

	float OffsetX = i2fx(NewVal) - pCurrentQuad->m_aPoints[4].x;
	float OffsetY = i2fx(NewVal) - pCurrentQuad->m_aPoints[4].y;

	if(Prop == PROP_ORDER && pLayer)
	{
		int QuadIndex = pLayer->SwapQuads(pEditor->m_vSelectedQuads[pEditor->m_SelectedQuadIndex], NewVal);
		pEditor->m_vSelectedQuads[pEditor->m_SelectedQuadIndex] = QuadIndex;
	}

	for(auto &pQuad : vpQuads)
	{
		if(Prop == PROP_POS_X)
		{
			for(auto &Point : pQuad->m_aPoints)
				Point.x += OffsetX;
		}
		if(Prop == PROP_POS_Y)
		{
			for(auto &Point : pQuad->m_aPoints)
				Point.y += OffsetY;
		}
		if(Prop == PROP_POS_ENV)
		{
			int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
			int Step = (Index - pQuad->m_PosEnv) % 2;
			if(Step != 0)
			{
				for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
					if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->m_Channels == 3)
					{
						pQuad->m_PosEnv = Index;
						break;
					}
			}
		}
		if(Prop == PROP_POS_ENV_OFFSET)
			pQuad->m_PosEnvOffset = NewVal;
		if(Prop == PROP_COLOR_ENV)
		{
			int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
			int Step = (Index - pQuad->m_ColorEnv) % 2;
			if(Step != 0)
			{
				for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
					if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->m_Channels == 4)
					{
						pQuad->m_ColorEnv = Index;
						break;
					}
			}
		}
		if(Prop == PROP_COLOR_ENV_OFFSET)
			pQuad->m_ColorEnvOffset = NewVal;
	}

	return 0;
}

int CEditor::PopupSource(CEditor *pEditor, CUIRect View, void *pContext)
{
	CSoundSource *pSource = pEditor->GetSelectedSource();

	CUIRect Button;

	// delete button
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;
	if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, "Deletes the current source"))
	{
		CLayerSounds *pLayer = (CLayerSounds *)pEditor->GetSelectedLayerType(0, LAYERTYPE_SOUNDS);
		if(pLayer)
		{
			pEditor->m_Map.m_Modified = true;
			pLayer->m_vSources.erase(pLayer->m_vSources.begin() + pEditor->m_SelectedSource);
			pEditor->m_SelectedSource--;
		}
		return 1;
	}

	// Sound shape button
	CUIRect ShapeButton;
	View.HSplitBottom(3.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &ShapeButton);
	static int s_ShapeTypeButton = 0;

	static const char *s_apShapeNames[] = {
		"Rectangle",
		"Circle"};

	pSource->m_Shape.m_Type = pSource->m_Shape.m_Type % CSoundShape::NUM_SHAPES; // prevent out of array errors

	if(pEditor->DoButton_Editor(&s_ShapeTypeButton, s_apShapeNames[pSource->m_Shape.m_Type], 0, &ShapeButton, 0, "Change shape"))
	{
		pSource->m_Shape.m_Type = (pSource->m_Shape.m_Type + 1) % CSoundShape::NUM_SHAPES;

		// set default values
		switch(pSource->m_Shape.m_Type)
		{
		case CSoundShape::SHAPE_CIRCLE:
		{
			pSource->m_Shape.m_Circle.m_Radius = 1000.0f;
			break;
		}
		case CSoundShape::SHAPE_RECTANGLE:
		{
			pSource->m_Shape.m_Rectangle.m_Width = f2fx(1000.0f);
			pSource->m_Shape.m_Rectangle.m_Height = f2fx(800.0f);
			break;
		}
		}
	}

	enum
	{
		PROP_POS_X = 0,
		PROP_POS_Y,
		PROP_LOOP,
		PROP_PAN,
		PROP_TIME_DELAY,
		PROP_FALLOFF,
		PROP_POS_ENV,
		PROP_POS_ENV_OFFSET,
		PROP_SOUND_ENV,
		PROP_SOUND_ENV_OFFSET,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Pos X", pSource->m_Position.x / 1000, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos Y", pSource->m_Position.y / 1000, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Loop", pSource->m_Loop, PROPTYPE_BOOL, 0, 1},
		{"Pan", pSource->m_Pan, PROPTYPE_BOOL, 0, 1},
		{"Delay", pSource->m_TimeDelay, PROPTYPE_INT_SCROLL, 0, 1000000},
		{"Falloff", pSource->m_Falloff, PROPTYPE_INT_SCROLL, 0, 255},
		{"Pos. Env", pSource->m_PosEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Pos. TO", pSource->m_PosEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Sound Env", pSource->m_SoundEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Sound. TO", pSource->m_SoundEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},

		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		pEditor->m_Map.m_Modified = true;

	if(Prop == PROP_POS_X)
		pSource->m_Position.x = NewVal * 1000;
	if(Prop == PROP_POS_Y)
		pSource->m_Position.y = NewVal * 1000;
	if(Prop == PROP_LOOP)
		pSource->m_Loop = NewVal;
	if(Prop == PROP_PAN)
		pSource->m_Pan = NewVal;
	if(Prop == PROP_TIME_DELAY)
		pSource->m_TimeDelay = NewVal;
	if(Prop == PROP_FALLOFF)
		pSource->m_Falloff = NewVal;
	if(Prop == PROP_POS_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
		int Step = (Index - pSource->m_PosEnv) % 2;
		if(Step != 0)
		{
			for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
				if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->m_Channels == 3)
				{
					pSource->m_PosEnv = Index;
					break;
				}
		}
	}
	if(Prop == PROP_POS_ENV_OFFSET)
		pSource->m_PosEnvOffset = NewVal;
	if(Prop == PROP_SOUND_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
		int Step = (Index - pSource->m_SoundEnv) % 2;
		if(Step != 0)
		{
			for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
				if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->m_Channels == 1)
				{
					pSource->m_SoundEnv = Index;
					break;
				}
		}
	}
	if(Prop == PROP_SOUND_ENV_OFFSET)
		pSource->m_SoundEnvOffset = NewVal;

	// source shape properties
	switch(pSource->m_Shape.m_Type)
	{
	case CSoundShape::SHAPE_CIRCLE:
	{
		enum
		{
			PROP_CIRCLE_RADIUS = 0,
			NUM_CIRCLE_PROPS,
		};

		CProperty aCircleProps[] = {
			{"Radius", pSource->m_Shape.m_Circle.m_Radius, PROPTYPE_INT_SCROLL, 0, 1000000},

			{nullptr},
		};

		static int s_aCircleIds[NUM_CIRCLE_PROPS] = {0};

		NewVal = 0;
		Prop = pEditor->DoProperties(&View, aCircleProps, s_aCircleIds, &NewVal);
		if(Prop != -1)
			pEditor->m_Map.m_Modified = true;

		if(Prop == PROP_CIRCLE_RADIUS)
			pSource->m_Shape.m_Circle.m_Radius = NewVal;

		break;
	}

	case CSoundShape::SHAPE_RECTANGLE:
	{
		enum
		{
			PROP_RECTANGLE_WIDTH = 0,
			PROP_RECTANGLE_HEIGHT,
			NUM_RECTANGLE_PROPS,
		};

		CProperty aRectangleProps[] = {
			{"Width", pSource->m_Shape.m_Rectangle.m_Width / 1024, PROPTYPE_INT_SCROLL, 0, 1000000},
			{"Height", pSource->m_Shape.m_Rectangle.m_Height / 1024, PROPTYPE_INT_SCROLL, 0, 1000000},

			{nullptr},
		};

		static int s_aRectangleIds[NUM_RECTANGLE_PROPS] = {0};

		NewVal = 0;
		Prop = pEditor->DoProperties(&View, aRectangleProps, s_aRectangleIds, &NewVal);
		if(Prop != -1)
			pEditor->m_Map.m_Modified = true;

		if(Prop == PROP_RECTANGLE_WIDTH)
			pSource->m_Shape.m_Rectangle.m_Width = NewVal * 1024;
		if(Prop == PROP_RECTANGLE_HEIGHT)
			pSource->m_Shape.m_Rectangle.m_Height = NewVal * 1024;

		break;
	}
	}

	return 0;
}

int CEditor::PopupPoint(CEditor *pEditor, CUIRect View, void *pContext)
{
	std::vector<CQuad *> vpQuads = pEditor->GetSelectedQuads();
	CQuad *pCurrentQuad = vpQuads[pEditor->m_SelectedQuadIndex];

	enum
	{
		PROP_POS_X = 0,
		PROP_POS_Y,
		PROP_COLOR,
		PROP_TEX_U,
		PROP_TEX_V,
		NUM_PROPS,
	};

	int Color = 0;
	Color |= pCurrentQuad->m_aColors[pEditor->m_SelectedQuadPoint].r << 24;
	Color |= pCurrentQuad->m_aColors[pEditor->m_SelectedQuadPoint].g << 16;
	Color |= pCurrentQuad->m_aColors[pEditor->m_SelectedQuadPoint].b << 8;
	Color |= pCurrentQuad->m_aColors[pEditor->m_SelectedQuadPoint].a;

	int x = fx2i(pCurrentQuad->m_aPoints[pEditor->m_SelectedQuadPoint].x);
	int y = fx2i(pCurrentQuad->m_aPoints[pEditor->m_SelectedQuadPoint].y);
	int tu = fx2f(pCurrentQuad->m_aTexcoords[pEditor->m_SelectedQuadPoint].x) * 1024;
	int tv = fx2f(pCurrentQuad->m_aTexcoords[pEditor->m_SelectedQuadPoint].y) * 1024;

	CProperty aProps[] = {
		{"Pos X", x, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos Y", y, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Color", Color, PROPTYPE_COLOR, 0, 0},
		{"Tex U", tu, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Tex V", tv, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
		pEditor->m_Map.m_Modified = true;

	for(auto &pQuad : vpQuads)
	{
		if(Prop == PROP_POS_X)
		{
			for(int v = 0; v < 4; v++)
				if(pEditor->m_SelectedPoints & (1 << v))
					pQuad->m_aPoints[v].x = i2fx(NewVal);
		}
		if(Prop == PROP_POS_Y)
		{
			for(int v = 0; v < 4; v++)
				if(pEditor->m_SelectedPoints & (1 << v))
					pQuad->m_aPoints[v].y = i2fx(NewVal);
		}
		if(Prop == PROP_COLOR)
		{
			for(int v = 0; v < 4; v++)
			{
				if(pEditor->m_SelectedPoints & (1 << v))
				{
					pQuad->m_aColors[v].r = (NewVal >> 24) & 0xff;
					pQuad->m_aColors[v].g = (NewVal >> 16) & 0xff;
					pQuad->m_aColors[v].b = (NewVal >> 8) & 0xff;
					pQuad->m_aColors[v].a = NewVal & 0xff;
				}
			}
		}
		if(Prop == PROP_TEX_U)
		{
			for(int v = 0; v < 4; v++)
				if(pEditor->m_SelectedPoints & (1 << v))
					pQuad->m_aTexcoords[v].x = f2fx(NewVal / 1024.0f);
		}
		if(Prop == PROP_TEX_V)
		{
			for(int v = 0; v < 4; v++)
				if(pEditor->m_SelectedPoints & (1 << v))
					pQuad->m_aTexcoords[v].y = f2fx(NewVal / 1024.0f);
		}
	}

	return 0;
}

int CEditor::PopupNewFolder(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect Label, ButtonBar;

	// title
	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(30.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Create new folder", 20.0f, TEXTALIGN_CENTER);

	View.HSplitBottom(10.0f, &View, nullptr);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	if(pEditor->m_aFileDialogErrString[0] == 0)
	{
		// interaction box
		View.HSplitBottom(40.0f, &View, nullptr);
		View.VMargin(40.0f, &View);
		View.HSplitBottom(20.0f, &View, &Label);
		static float s_FolderBox = 0;
		pEditor->DoEditBox(&s_FolderBox, &Label, pEditor->m_aFileDialogNewFolderName, sizeof(pEditor->m_aFileDialogNewFolderName), 15.0f, &s_FolderBox);
		View.HSplitBottom(20.0f, &View, &Label);
		pEditor->UI()->DoLabel(&Label, "Name:", 10.0f, TEXTALIGN_LEFT);

		// button bar
		ButtonBar.VSplitLeft(30.0f, nullptr, &ButtonBar);
		ButtonBar.VSplitLeft(110.0f, &Label, &ButtonBar);
		static int s_CreateButton = 0;
		if(pEditor->DoButton_Editor(&s_CreateButton, "Create", 0, &Label, 0, nullptr) || pEditor->Input()->KeyPress(KEY_RETURN) || pEditor->Input()->KeyPress(KEY_KP_ENTER))
		{
			// create the folder
			if(pEditor->m_aFileDialogNewFolderName[0])
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s/%s", pEditor->m_pFileDialogPath, pEditor->m_aFileDialogNewFolderName);
				if(pEditor->Storage()->CreateFolder(aBuf, IStorage::TYPE_SAVE))
				{
					pEditor->FilelistPopulate(IStorage::TYPE_SAVE);
					return 1;
				}
				else
					str_copy(pEditor->m_aFileDialogErrString, "Unable to create the folder", sizeof(pEditor->m_aFileDialogErrString));
			}
		}
		ButtonBar.VSplitRight(30.0f, &ButtonBar, nullptr);
		ButtonBar.VSplitRight(110.0f, &ButtonBar, &Label);
		static int s_AbortButton = 0;
		if(pEditor->DoButton_Editor(&s_AbortButton, "Abort", 0, &Label, 0, nullptr))
			return 1;
	}
	else
	{
		// error text
		View.HSplitTop(30.0f, nullptr, &View);
		View.VMargin(40.0f, &View);
		View.HSplitTop(20.0f, &Label, &View);
		pEditor->UI()->DoLabel(&Label, "Error:", 10.0f, TEXTALIGN_LEFT);
		View.HSplitTop(20.0f, &Label, &View);
		SLabelProperties Props;
		Props.m_MaxWidth = View.w;
		pEditor->UI()->DoLabel(&Label, "Unable to create the folder", 10.0f, TEXTALIGN_LEFT, Props);

		// button
		ButtonBar.VMargin(ButtonBar.w / 2.0f - 55.0f, &ButtonBar);
		static int s_CreateButton = 0;
		if(pEditor->DoButton_Editor(&s_CreateButton, "Ok", 0, &ButtonBar, 0, nullptr))
			return 1;
	}

	return 0;
}

int CEditor::PopupMapInfo(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect Label, ButtonBar, Button;

	// title
	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(30.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Map details", 20.0f, TEXTALIGN_CENTER);

	View.HSplitBottom(10.0f, &View, nullptr);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	View.VMargin(40.0f, &View);

	// author box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Author:", 10.0f, TEXTALIGN_LEFT);
	Label.VSplitLeft(45.0f, nullptr, &Button);
	Button.HMargin(3, &Button);
	static float s_AuthorBox = 0;
	pEditor->DoEditBox(&s_AuthorBox, &Button, pEditor->m_Map.m_MapInfo.m_aAuthorTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aAuthorTmp), 10.0f, &s_AuthorBox);

	// version box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Version:", 10.0f, TEXTALIGN_LEFT);
	Label.VSplitLeft(45.0f, nullptr, &Button);
	Button.HMargin(3, &Button);
	static float s_VersionBox = 0;
	pEditor->DoEditBox(&s_VersionBox, &Button, pEditor->m_Map.m_MapInfo.m_aVersionTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aVersionTmp), 10.0f, &s_VersionBox);

	// credits box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Credits:", 10.0f, TEXTALIGN_LEFT);
	Label.VSplitLeft(45.0f, nullptr, &Button);
	Button.HMargin(3, &Button);
	static float s_CreditsBox = 0;
	pEditor->DoEditBox(&s_CreditsBox, &Button, pEditor->m_Map.m_MapInfo.m_aCreditsTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aCreditsTmp), 10.0f, &s_CreditsBox);

	// license box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "License:", 10.0f, TEXTALIGN_LEFT);
	Label.VSplitLeft(45.0f, nullptr, &Button);
	Button.HMargin(3, &Button);
	static float s_LicenseBox = 0;
	pEditor->DoEditBox(&s_LicenseBox, &Button, pEditor->m_Map.m_MapInfo.m_aLicenseTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aLicenseTmp), 10.0f, &s_LicenseBox);

	// button bar
	ButtonBar.VSplitLeft(30.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &Label, &ButtonBar);
	static int s_CreateButton = 0;
	if(pEditor->DoButton_Editor(&s_CreateButton, "Ok", 0, &Label, 0, nullptr))
	{
		str_copy(pEditor->m_Map.m_MapInfo.m_aAuthor, pEditor->m_Map.m_MapInfo.m_aAuthorTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aAuthor));
		str_copy(pEditor->m_Map.m_MapInfo.m_aVersion, pEditor->m_Map.m_MapInfo.m_aVersionTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aVersion));
		str_copy(pEditor->m_Map.m_MapInfo.m_aCredits, pEditor->m_Map.m_MapInfo.m_aCreditsTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aCredits));
		str_copy(pEditor->m_Map.m_MapInfo.m_aLicense, pEditor->m_Map.m_MapInfo.m_aLicenseTmp, sizeof(pEditor->m_Map.m_MapInfo.m_aLicense));
		return 1;
	}

	ButtonBar.VSplitRight(30.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Label);
	static int s_AbortButton = 0;
	if(pEditor->DoButton_Editor(&s_AbortButton, "Abort", 0, &Label, 0, nullptr))
		return 1;

	return 0;
}

int CEditor::PopupEvent(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect Label, ButtonBar;

	// title
	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(30.0f, &Label, &View);
	if(pEditor->m_PopupEventType == POPEVENT_EXIT)
		pEditor->UI()->DoLabel(&Label, "Exit the editor", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_LOAD)
		pEditor->UI()->DoLabel(&Label, "Load map", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_NEW)
		pEditor->UI()->DoLabel(&Label, "New map", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_SAVE)
		pEditor->UI()->DoLabel(&Label, "Save map", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_LARGELAYER)
		pEditor->UI()->DoLabel(&Label, "Large layer", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_PREVENTUNUSEDTILES)
		pEditor->UI()->DoLabel(&Label, "Unused tiles disabled", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGEDIV16)
		pEditor->UI()->DoLabel(&Label, "Image width/height", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGE_MAX)
		pEditor->UI()->DoLabel(&Label, "Max images", 20.0f, TEXTALIGN_CENTER);
	else if(pEditor->m_PopupEventType == POPEVENT_PLACE_BORDER_TILES)
		pEditor->UI()->DoLabel(&Label, "Place border tiles", 20.0f, TEXTALIGN_CENTER);

	View.HSplitBottom(10.0f, &View, nullptr);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	// notification text
	View.HSplitTop(30.0f, nullptr, &View);
	View.VMargin(40.0f, &View);
	View.HSplitTop(20.0f, &Label, &View);
	SLabelProperties Props;
	Props.m_MaxWidth = Label.w - 10.0f;
	if(pEditor->m_PopupEventType == POPEVENT_EXIT)
		pEditor->UI()->DoLabel(&Label, "The map contains unsaved data, you might want to save it before you exit the editor.\nContinue anyway?", 10.0f, TEXTALIGN_LEFT, Props);
	else if((pEditor->m_PopupEventType == POPEVENT_LOAD) || (pEditor->m_PopupEventType == POPEVENT_LOADCURRENT))
		pEditor->UI()->DoLabel(&Label, "The map contains unsaved data, you might want to save it before you load a new map.\nContinue anyway?", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_NEW)
		pEditor->UI()->DoLabel(&Label, "The map contains unsaved data, you might want to save it before you create a new map.\nContinue anyway?", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_SAVE)
		pEditor->UI()->DoLabel(&Label, "The file already exists.\nDo you want to overwrite the map?", 10.0f, TEXTALIGN_LEFT);
	else if(pEditor->m_PopupEventType == POPEVENT_LARGELAYER)
		pEditor->UI()->DoLabel(&Label, "You are trying to set the height or width of a layer to more than 1000 tiles. This is actually possible, but only rarely necessary. It may cause the editor to work slower, larger file size as well as higher memory usage for client and server.", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_PREVENTUNUSEDTILES)
		pEditor->UI()->DoLabel(&Label, "Unused tiles can't be placed by default because they could get a use later and then destroy your map.\nActivate the 'Unused' switch to be able to place every tile.", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGEDIV16)
		pEditor->UI()->DoLabel(&Label, "The width or height of this image is not divisible by 16. This is required for images used in tile layers.", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGE_MAX)
		pEditor->UI()->DoLabel(&Label, "The client only allows a maximum of 64 images.", 10.0f, TEXTALIGN_LEFT, Props);
	else if(pEditor->m_PopupEventType == POPEVENT_PLACE_BORDER_TILES)
		pEditor->UI()->DoLabel(&Label, "This is going to overwrite any existing tiles around the edges of the layer.\nContinue?", 10.0f, TEXTALIGN_LEFT, Props);

	// button bar
	ButtonBar.VSplitLeft(30.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &Label, &ButtonBar);
	static int s_OkButton = 0;
	if(pEditor->DoButton_Editor(&s_OkButton, "Ok", 0, &Label, 0, nullptr) || pEditor->Input()->KeyPress(KEY_RETURN) || pEditor->Input()->KeyPress(KEY_KP_ENTER))
	{
		if(pEditor->m_PopupEventType == POPEVENT_EXIT)
			g_Config.m_ClEditor = 0;
		else if(pEditor->m_PopupEventType == POPEVENT_LOAD)
			pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", CEditor::CallbackOpenMap, pEditor);
		else if(pEditor->m_PopupEventType == POPEVENT_LOADCURRENT)
			pEditor->LoadCurrentMap();
		else if(pEditor->m_PopupEventType == POPEVENT_NEW)
		{
			pEditor->Reset();
			pEditor->m_aFileName[0] = 0;
		}
		else if(pEditor->m_PopupEventType == POPEVENT_SAVE)
			CEditor::CallbackSaveMap(pEditor->m_aFileSaveName, IStorage::TYPE_SAVE, pEditor);
		else if(pEditor->m_PopupEventType == POPEVENT_PLACE_BORDER_TILES)
			pEditor->PlaceBorderTiles();
		pEditor->m_PopupEventWasActivated = false;
		return 1;
	}
	ButtonBar.VSplitRight(30.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Label);
	if(pEditor->m_PopupEventType != POPEVENT_LARGELAYER && pEditor->m_PopupEventType != POPEVENT_PREVENTUNUSEDTILES && pEditor->m_PopupEventType != POPEVENT_IMAGEDIV16 && pEditor->m_PopupEventType != POPEVENT_IMAGE_MAX)
	{
		static int s_AbortButton = 0;
		if(pEditor->DoButton_Editor(&s_AbortButton, "Abort", 0, &Label, 0, nullptr) || pEditor->Input()->KeyPress(KEY_ESCAPE))
		{
			pEditor->m_PopupEventWasActivated = false;
			return 1;
		}
	}

	return 0;
}

static int g_SelectImageSelected = -100;
static int g_SelectImageCurrent = -100;

int CEditor::PopupSelectImage(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect ButtonBar, ImageView;
	View.VSplitLeft(150.0f, &ButtonBar, &View);
	View.Margin(10.0f, &ImageView);

	int ShowImage = g_SelectImageCurrent;

	const float ButtonHeight = 12.0f;
	const float ButtonMargin = 2.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = (ButtonHeight + ButtonMargin) * 5;
	s_ScrollRegion.Begin(&ButtonBar, &ScrollOffset, &ScrollParams);
	ButtonBar.y += ScrollOffset.y;

	for(int i = -1; i < (int)pEditor->m_Map.m_vpImages.size(); i++)
	{
		CUIRect Button;
		ButtonBar.HSplitTop(ButtonMargin, nullptr, &ButtonBar);
		ButtonBar.HSplitTop(ButtonHeight, &Button, &ButtonBar);
		if(s_ScrollRegion.AddRect(Button))
		{
			if(pEditor->UI()->MouseInside(&Button))
				ShowImage = i;

			static int s_NoneButton = 0;
			if(pEditor->DoButton_MenuItem(i == -1 ? (void *)&s_NoneButton : &pEditor->m_Map.m_vpImages[i], i == -1 ? "None" : pEditor->m_Map.m_vpImages[i]->m_aName, i == g_SelectImageCurrent, &Button))
				g_SelectImageSelected = i;
		}
	}

	s_ScrollRegion.End();

	if(ShowImage >= 0 && (size_t)ShowImage < pEditor->m_Map.m_vpImages.size())
	{
		if(ImageView.h < ImageView.w)
			ImageView.w = ImageView.h;
		else
			ImageView.h = ImageView.w;
		float Max = (float)(maximum(pEditor->m_Map.m_vpImages[ShowImage]->m_Width, pEditor->m_Map.m_vpImages[ShowImage]->m_Height));
		ImageView.w *= pEditor->m_Map.m_vpImages[ShowImage]->m_Width / Max;
		ImageView.h *= pEditor->m_Map.m_vpImages[ShowImage]->m_Height / Max;
		pEditor->Graphics()->TextureSet(pEditor->m_Map.m_vpImages[ShowImage]->m_Texture);
		pEditor->Graphics()->BlendNormal();
		pEditor->Graphics()->WrapClamp();
		pEditor->Graphics()->QuadsBegin();
		IGraphics::CQuadItem QuadItem(ImageView.x, ImageView.y, ImageView.w, ImageView.h);
		pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);
		pEditor->Graphics()->QuadsEnd();
		pEditor->Graphics()->WrapNormal();
	}

	return 0;
}

void CEditor::PopupSelectImageInvoke(int Current, float x, float y)
{
	static int s_SelectImagePopupId = 0;
	g_SelectImageSelected = -100;
	g_SelectImageCurrent = Current;
	UiInvokePopupMenu(&s_SelectImagePopupId, 0, x, y, 450, 300, PopupSelectImage);
}

int CEditor::PopupSelectImageResult()
{
	if(g_SelectImageSelected == -100)
		return -100;

	g_SelectImageCurrent = g_SelectImageSelected;
	g_SelectImageSelected = -100;
	return g_SelectImageCurrent;
}

static int g_SelectSoundSelected = -100;
static int g_SelectSoundCurrent = -100;

int CEditor::PopupSelectSound(CEditor *pEditor, CUIRect View, void *pContext)
{
	const float ButtonHeight = 12.0f;
	const float ButtonMargin = 2.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = (ButtonHeight + ButtonMargin) * 5;
	s_ScrollRegion.Begin(&View, &ScrollOffset, &ScrollParams);
	View.y += ScrollOffset.y;

	for(int i = -1; i < (int)pEditor->m_Map.m_vpSounds.size(); i++)
	{
		CUIRect Button;
		View.HSplitTop(ButtonMargin, nullptr, &View);
		View.HSplitTop(ButtonHeight, &Button, &View);
		if(s_ScrollRegion.AddRect(Button))
		{
			static int s_NoneButton = 0;
			if(pEditor->DoButton_MenuItem(i == -1 ? (void *)&s_NoneButton : &pEditor->m_Map.m_vpSounds[i], i == -1 ? "None" : pEditor->m_Map.m_vpSounds[i]->m_aName, i == g_SelectSoundCurrent, &Button))
				g_SelectSoundSelected = i;
		}
	}

	s_ScrollRegion.End();

	return 0;
}

void CEditor::PopupSelectSoundInvoke(int Current, float x, float y)
{
	static int s_SelectSoundPopupId = 0;
	g_SelectSoundSelected = -100;
	g_SelectSoundCurrent = Current;
	UiInvokePopupMenu(&s_SelectSoundPopupId, 0, x, y, 150, 300, PopupSelectSound);
}

int CEditor::PopupSelectSoundResult()
{
	if(g_SelectSoundSelected == -100)
		return -100;

	g_SelectSoundCurrent = g_SelectSoundSelected;
	g_SelectSoundSelected = -100;
	return g_SelectSoundCurrent;
}

static int s_GametileOpSelected = -1;

int CEditor::PopupSelectGametileOp(CEditor *pEditor, CUIRect View, void *pContext)
{
	static const char *s_apButtonNames[] = {
		"Air",
		"Hookable",
		"Death",
		"Unhookable",
		"Hookthrough",
		"Freeze",
		"Unfreeze",
		"Deep Freeze",
		"Deep Unfreeze",
		"Blue Check-Tele",
		"Red Check-Tele",
		"Live Freeze",
		"Live Unfreeze",
	};
	static unsigned s_NumButtons = std::size(s_apButtonNames);
	CUIRect Button;

	for(unsigned i = 0; i < s_NumButtons; ++i)
	{
		View.HSplitTop(2.0f, nullptr, &View);
		View.HSplitTop(12.0f, &Button, &View);
		if(pEditor->DoButton_Editor(&s_apButtonNames[i], s_apButtonNames[i], 0, &Button, 0, nullptr))
			s_GametileOpSelected = i;
	}

	return 0;
}

void CEditor::PopupSelectGametileOpInvoke(float x, float y)
{
	static int s_SelectGametileOpPopupId = 0;
	s_GametileOpSelected = -1;
	UiInvokePopupMenu(&s_SelectGametileOpPopupId, 0, x, y, 120.0f, 189.0f, PopupSelectGametileOp);
}

int CEditor::PopupSelectGameTileOpResult()
{
	if(s_GametileOpSelected < 0)
		return -1;

	int Result = s_GametileOpSelected;
	s_GametileOpSelected = -1;
	return Result;
}

static int s_AutoMapConfigSelected = -100;
static int s_AutoMapConfigCurrent = -100;

int CEditor::PopupSelectConfigAutoMap(CEditor *pEditor, CUIRect View, void *pContext)
{
	CLayerTiles *pLayer = static_cast<CLayerTiles *>(pEditor->GetSelectedLayer(0));
	CAutoMapper *pAutoMapper = &pEditor->m_Map.m_vpImages[pLayer->m_Image]->m_AutoMapper;

	const float ButtonHeight = 12.0f;
	const float ButtonMargin = 2.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = (ButtonHeight + ButtonMargin) * 5;
	s_ScrollRegion.Begin(&View, &ScrollOffset, &ScrollParams);
	View.y += ScrollOffset.y;

	for(int i = -1; i < pAutoMapper->ConfigNamesNum(); i++)
	{
		CUIRect Button;
		View.HSplitTop(ButtonMargin, nullptr, &View);
		View.HSplitTop(ButtonHeight, &Button, &View);
		if(s_ScrollRegion.AddRect(Button))
		{
			static int s_NoneButton = 0;
			if(pEditor->DoButton_MenuItem(i == -1 ? (void *)&s_NoneButton : pAutoMapper->GetConfigName(i), i == -1 ? "None" : pAutoMapper->GetConfigName(i), i == s_AutoMapConfigCurrent, &Button))
				s_AutoMapConfigSelected = i;
		}
	}

	s_ScrollRegion.End();

	return 0;
}

void CEditor::PopupSelectConfigAutoMapInvoke(int Current, float x, float y)
{
	static int s_AutoMapConfigSelectID = 0;
	s_AutoMapConfigSelected = -100;
	s_AutoMapConfigCurrent = Current;
	CLayerTiles *pLayer = static_cast<CLayerTiles *>(GetSelectedLayer(0));
	const int ItemCount = minimum(m_Map.m_vpImages[pLayer->m_Image]->m_AutoMapper.ConfigNamesNum(), 10);
	// Width for buttons is 120, 15 is the scrollbar width, 2 is the margin between both.
	UiInvokePopupMenu(&s_AutoMapConfigSelectID, 0, x, y, 120.0f + 15.0f + 2.0f, 26.0f + 14.0f * ItemCount, PopupSelectConfigAutoMap);
}

int CEditor::PopupSelectConfigAutoMapResult()
{
	if(s_AutoMapConfigSelected == -100)
		return -100;

	s_AutoMapConfigCurrent = s_AutoMapConfigSelected;
	s_AutoMapConfigSelected = -100;
	return s_AutoMapConfigCurrent;
}

// DDRace

int CEditor::PopupTele(CEditor *pEditor, CUIRect View, void *pContext)
{
	static int s_PreviousNumber = -1;

	CUIRect NumberPicker;
	CUIRect FindEmptySlot;

	View.VSplitRight(15.f, &NumberPicker, &FindEmptySlot);
	NumberPicker.VSplitRight(2.f, &NumberPicker, nullptr);

	// find empty number button
	{
		static int s_EmptySlotPid = 0;
		if(pEditor->DoButton_Editor(&s_EmptySlotPid, "F", 0, &FindEmptySlot, 0, "[ctrl+f] Find empty slot") || pEditor->Input()->KeyPress(KEY_F))
		{
			int number = -1;
			for(int i = 1; i <= 255; i++)
			{
				if(!pEditor->m_Map.m_pTeleLayer->ContainsElementWithId(i))
				{
					number = i;
					break;
				}
			}

			if(number != -1)
			{
				pEditor->m_TeleNumber = number;
			}
		}
	}

	// number picker
	{
		static ColorRGBA s_Color = ColorRGBA(0.5f, 1, 0.5f, 0.5f);

		enum
		{
			PROP_TELE = 0,
			NUM_PROPS,
		};
		CProperty aProps[] = {
			{"Number", pEditor->m_TeleNumber, PROPTYPE_INT_STEP, 1, 255},
			{nullptr},
		};

		static int s_aIds[NUM_PROPS] = {0};

		static int NewVal = 0;
		int Prop = pEditor->DoProperties(&NumberPicker, aProps, s_aIds, &NewVal, s_Color);
		if(Prop == PROP_TELE)
		{
			pEditor->m_TeleNumber = (NewVal + 256) % 256;
		}

		if(s_PreviousNumber == 1 || s_PreviousNumber != pEditor->m_TeleNumber)
		{
			s_Color = pEditor->m_Map.m_pTeleLayer->ContainsElementWithId(pEditor->m_TeleNumber) ? ColorRGBA(1, 0.5f, 0.5f, 0.5f) : ColorRGBA(0.5f, 1, 0.5f, 0.5f);
		}
	}

	s_PreviousNumber = pEditor->m_TeleNumber;

	return 0;
}

int CEditor::PopupSpeedup(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);

	enum
	{
		PROP_FORCE = 0,
		PROP_MAXSPEED,
		PROP_ANGLE,
		NUM_PROPS
	};

	CProperty aProps[] = {
		{"Force", pEditor->m_SpeedupForce, PROPTYPE_INT_STEP, 0, 255},
		{"Max Speed", pEditor->m_SpeedupMaxSpeed, PROPTYPE_INT_STEP, 0, 255},
		{"Angle", pEditor->m_SpeedupAngle, PROPTYPE_ANGLE_SCROLL, 0, 359},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);

	if(Prop == PROP_FORCE)
		pEditor->m_SpeedupForce = clamp(NewVal, 0, 255);
	if(Prop == PROP_MAXSPEED)
		pEditor->m_SpeedupMaxSpeed = clamp(NewVal, 0, 255);
	if(Prop == PROP_ANGLE)
		pEditor->m_SpeedupAngle = clamp(NewVal, 0, 359);

	return 0;
}

int CEditor::PopupSwitch(CEditor *pEditor, CUIRect View, void *pContext)
{
	static int s_PreviousNumber = -1;

	CUIRect NumberPicker;
	CUIRect FindEmptySlot;

	CUIRect DelayPicker;

	View.HSplitMid(&NumberPicker, &DelayPicker);
	NumberPicker.VSplitRight(15.f, &NumberPicker, &FindEmptySlot);
	NumberPicker.VSplitRight(2.f, &NumberPicker, nullptr);

	// find empty number button
	{
		static int s_EmptySlotPid = 0;
		if(pEditor->DoButton_Editor(&s_EmptySlotPid, "F", 0, &FindEmptySlot, 0, "[ctrl+f] Find empty slot") || pEditor->Input()->KeyPress(KEY_F))
		{
			int number = -1;
			for(int i = 1; i <= 255; i++)
			{
				if(!pEditor->m_Map.m_pSwitchLayer->ContainsElementWithId(i))
				{
					number = i;
					break;
				}
			}

			if(number != -1)
			{
				pEditor->m_SwitchNum = number;
			}
		}
	}

	// number picker
	{
		static ColorRGBA s_Color = ColorRGBA(1, 1, 1, 0.5f);

		enum
		{
			PROP_SwitchNumber = 0,
			PROP_SwitchDelay,
			NUM_PROPS,
		};

		CProperty aProps[] = {
			{"Number", pEditor->m_SwitchNum, PROPTYPE_INT_STEP, 1, 255},
			{"Delay", pEditor->m_SwitchDelay, PROPTYPE_INT_STEP, 0, 255},
			{nullptr},
		};

		static int s_aIds[NUM_PROPS] = {0};
		int NewVal = 0;
		int Prop = pEditor->DoProperties(&NumberPicker, aProps, s_aIds, &NewVal, s_Color);

		if(Prop == PROP_SwitchNumber)
		{
			pEditor->m_SwitchNum = (NewVal + 256) % 256;
		}
		else if(Prop == PROP_SwitchDelay)
		{
			pEditor->m_SwitchDelay = (NewVal + 256) % 256;
		}

		if(s_PreviousNumber == 1 || s_PreviousNumber != pEditor->m_SwitchNum)
		{
			s_Color = pEditor->m_Map.m_pSwitchLayer->ContainsElementWithId(pEditor->m_SwitchNum) ? ColorRGBA(1, 0.5f, 0.5f, 0.5f) : ColorRGBA(0.5f, 1, 0.5f, 0.5f);
		}
	}

	s_PreviousNumber = pEditor->m_SwitchNum;
	return 0;
}

int CEditor::PopupTune(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);

	enum
	{
		PROP_TUNE = 0,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Zone", pEditor->m_TuningNum, PROPTYPE_INT_STEP, 1, 255},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);

	if(Prop == PROP_TUNE)
		pEditor->m_TuningNum = (NewVal - 1 + 255) % 255 + 1;

	return 0;
}

int CEditor::PopupGoto(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect CoordXPicker;
	CUIRect CoordYPicker;

	View.HSplitMid(&CoordXPicker, &CoordYPicker);
	CoordXPicker.VSplitRight(2.f, &CoordXPicker, nullptr);

	static ColorRGBA s_Color = ColorRGBA(1, 1, 1, 0.5f);

	enum
	{
		PROP_CoordX = 0,
		PROP_CoordY,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"X", pEditor->m_GotoX, PROPTYPE_INT_STEP, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()},
		{"Y", pEditor->m_GotoY, PROPTYPE_INT_STEP, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&CoordXPicker, aProps, s_aIds, &NewVal, s_Color);

	if(Prop == PROP_CoordX)
	{
		pEditor->m_GotoX = NewVal;
	}
	else if(Prop == PROP_CoordY)
	{
		pEditor->m_GotoY = NewVal;
	}

	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);

	static int s_Button;
	if(pEditor->DoButton_Editor(&s_Button, "Go", 0, &Button, 0, ""))
	{
		pEditor->Goto(pEditor->m_GotoX + 0.5f, pEditor->m_GotoY + 0.5f);
	}

	return 0;
}

int CEditor::PopupColorPicker(CEditor *pEditor, CUIRect View, void *pContext)
{
	CUIRect SVPicker, HuePicker;
	View.VSplitRight(20.0f, &SVPicker, &HuePicker);
	HuePicker.VSplitLeft(4.0f, nullptr, &HuePicker);

	pEditor->Graphics()->TextureClear();
	pEditor->Graphics()->QuadsBegin();

	// base: white - hue
	ColorHSVA hsv = CEditor::ms_PickerColor;
	IGraphics::CColorVertex aColors[4];

	ColorRGBA c = color_cast<ColorRGBA>(ColorHSVA(hsv.x, 0.0f, 1.0f));
	aColors[0] = IGraphics::CColorVertex(0, c.r, c.g, c.b, 1.0f);
	c = color_cast<ColorRGBA>(ColorHSVA(hsv.x, 1.0f, 1.0f));
	aColors[1] = IGraphics::CColorVertex(1, c.r, c.g, c.b, 1.0f);
	c = color_cast<ColorRGBA>(ColorHSVA(hsv.x, 1.0f, 1.0f));
	aColors[2] = IGraphics::CColorVertex(2, c.r, c.g, c.b, 1.0f);
	c = color_cast<ColorRGBA>(ColorHSVA(hsv.x, 0.0f, 1.0f));
	aColors[3] = IGraphics::CColorVertex(3, c.r, c.g, c.b, 1.0f);

	pEditor->Graphics()->SetColorVertex(aColors, 4);

	IGraphics::CQuadItem QuadItem(SVPicker.x, SVPicker.y, SVPicker.w, SVPicker.h);
	pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);

	// base: transparent - black
	aColors[0] = IGraphics::CColorVertex(0, 0.0f, 0.0f, 0.0f, 0.0f);
	aColors[1] = IGraphics::CColorVertex(1, 0.0f, 0.0f, 0.0f, 0.0f);
	aColors[2] = IGraphics::CColorVertex(2, 0.0f, 0.0f, 0.0f, 1.0f);
	aColors[3] = IGraphics::CColorVertex(3, 0.0f, 0.0f, 0.0f, 1.0f);

	pEditor->Graphics()->SetColorVertex(aColors, 4);

	pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);

	pEditor->Graphics()->QuadsEnd();

	// marker
	vec2 Marker = vec2(hsv.y, (1.0f - hsv.z)) * vec2(SVPicker.w, SVPicker.h);
	pEditor->Graphics()->QuadsBegin();
	pEditor->Graphics()->SetColor(0.5f, 0.5f, 0.5f, 1.0f);
	IGraphics::CQuadItem aMarker[2];
	aMarker[0] = IGraphics::CQuadItem(SVPicker.x + Marker.x, SVPicker.y + Marker.y - 5.0f * pEditor->UI()->PixelSize(), pEditor->UI()->PixelSize(), 11.0f * pEditor->UI()->PixelSize());
	aMarker[1] = IGraphics::CQuadItem(SVPicker.x + Marker.x - 5.0f * pEditor->UI()->PixelSize(), SVPicker.y + Marker.y, 11.0f * pEditor->UI()->PixelSize(), pEditor->UI()->PixelSize());
	pEditor->Graphics()->QuadsDrawTL(aMarker, 2);
	pEditor->Graphics()->QuadsEnd();

	// logic
	float X, Y;
	if(pEditor->UI()->DoPickerLogic(&CEditor::ms_SVPicker, &SVPicker, &X, &Y))
	{
		hsv.y = X / SVPicker.w;
		hsv.z = 1.0f - Y / SVPicker.h;
	}

	// hue slider
	static const float s_aColorIndices[7][3] = {
		{1.0f, 0.0f, 0.0f}, // red
		{1.0f, 0.0f, 1.0f}, // magenta
		{0.0f, 0.0f, 1.0f}, // blue
		{0.0f, 1.0f, 1.0f}, // cyan
		{0.0f, 1.0f, 0.0f}, // green
		{1.0f, 1.0f, 0.0f}, // yellow
		{1.0f, 0.0f, 0.0f} // red
	};

	pEditor->Graphics()->QuadsBegin();
	ColorRGBA ColorTop, ColorBottom;
	float Offset = HuePicker.h / 6.0f;
	for(int j = 0; j < 6; j++)
	{
		ColorTop = ColorRGBA(s_aColorIndices[j][0], s_aColorIndices[j][1], s_aColorIndices[j][2], 1.0f);
		ColorBottom = ColorRGBA(s_aColorIndices[j + 1][0], s_aColorIndices[j + 1][1], s_aColorIndices[j + 1][2], 1.0f);

		aColors[0] = IGraphics::CColorVertex(0, ColorTop.r, ColorTop.g, ColorTop.b, ColorTop.a);
		aColors[1] = IGraphics::CColorVertex(1, ColorTop.r, ColorTop.g, ColorTop.b, ColorTop.a);
		aColors[2] = IGraphics::CColorVertex(2, ColorBottom.r, ColorBottom.g, ColorBottom.b, ColorBottom.a);
		aColors[3] = IGraphics::CColorVertex(3, ColorBottom.r, ColorBottom.g, ColorBottom.b, ColorBottom.a);
		pEditor->Graphics()->SetColorVertex(aColors, 4);
		QuadItem = IGraphics::CQuadItem(HuePicker.x, HuePicker.y + Offset * j, HuePicker.w, Offset);
		pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);
	}

	// marker
	pEditor->Graphics()->SetColor(0.5f, 0.5f, 0.5f, 1.0f);
	IGraphics::CQuadItem QuadItemMarker(HuePicker.x, HuePicker.y + (1.0f - hsv.x) * HuePicker.h, HuePicker.w, pEditor->UI()->PixelSize());
	pEditor->Graphics()->QuadsDrawTL(&QuadItemMarker, 1);

	pEditor->Graphics()->QuadsEnd();

	if(pEditor->UI()->DoPickerLogic(&CEditor::ms_HuePicker, &HuePicker, &X, &Y))
	{
		hsv.x = 1.0f - Y / HuePicker.h;
	}

	CEditor::ms_PickerColor = hsv;

	return 0;
}

int CEditor::PopupEntities(CEditor *pEditor, CUIRect View, void *pContext)
{
	for(int i = 0; i < (int)pEditor->m_vSelectEntitiesFiles.size(); i++)
	{
		CUIRect Button;
		View.HSplitTop(14.0f, &Button, &View);

		const char *pName = pEditor->m_vSelectEntitiesFiles[i].c_str();

		if(pEditor->DoButton_MenuItem(pName, pName, pEditor->m_vSelectEntitiesFiles[i] == pEditor->m_SelectEntitiesImage, &Button))
		{
			if(pEditor->m_vSelectEntitiesFiles[i] != pEditor->m_SelectEntitiesImage)
			{
				pEditor->m_SelectEntitiesImage = pEditor->m_vSelectEntitiesFiles[i];
				pEditor->m_AllowPlaceUnusedTiles = pEditor->m_SelectEntitiesImage == "DDNet" ? 0 : -1;
				pEditor->m_PreventUnusedTilesWasWarned = false;

				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "editor/entities/%s.png", pName);

				if(pEditor->m_EntitiesTexture.IsValid())
					pEditor->Graphics()->UnloadTexture(&pEditor->m_EntitiesTexture);
				int TextureLoadFlag = pEditor->GetTextureUsageFlag();
				pEditor->m_EntitiesTexture = pEditor->Graphics()->LoadTexture(aBuf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
				g_UiNumPopups--;
			}
		}
	}

	return 0;
}
