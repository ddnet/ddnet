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

CUI::EPopupMenuFunctionResult CEditor::PopupMenuFile(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	static int s_NewMapButton = 0;
	static int s_SaveButton = 0;
	static int s_SaveAsButton = 0;
	static int s_SaveCopyButton = 0;
	static int s_OpenButton = 0;
	static int s_OpenCurrentMapButton = 0;
	static int s_AppendButton = 0;
	static int s_MapInfoButton = 0;
	static int s_ExitButton = 0;

	CUIRect Slot;
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_NewMapButton, "New", 0, &Slot, 0, "Creates a new map (ctrl+n)"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_NEW;
			pEditor->m_PopupEventActivated = true;
		}
		else
		{
			pEditor->Reset();
			pEditor->m_aFileName[0] = 0;
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_OpenButton, "Load", 0, &Slot, 0, "Opens a map for editing (ctrl+l)"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_LOAD;
			pEditor->m_PopupEventActivated = true;
		}
		else
			pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", false, pEditor->CallbackOpenMap, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_OpenCurrentMapButton, "Load Current Map", 0, &Slot, 0, "Opens the current in game map for editing (ctrl+alt+l)"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_LOADCURRENT;
			pEditor->m_PopupEventActivated = true;
		}
		else
		{
			pEditor->LoadCurrentMap();
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_AppendButton, "Append", 0, &Slot, 0, "Opens a map and adds everything from that map to the current one (ctrl+a)"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Append map", "Append", "maps", false, pEditor->CallbackAppendMap, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveButton, "Save", 0, &Slot, 0, "Saves the current map (ctrl+s)"))
	{
		if(pEditor->m_aFileName[0] && pEditor->m_ValidSaveFilename)
		{
			str_copy(pEditor->m_aFileSaveName, pEditor->m_aFileName);
			pEditor->m_PopupEventType = POPEVENT_SAVE;
			pEditor->m_PopupEventActivated = true;
		}
		else
			pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", false, pEditor->CallbackSaveMap, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveAsButton, "Save As", 0, &Slot, 0, "Saves the current map under a new name (ctrl+shift+s)"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", true, pEditor->CallbackSaveMap, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_SaveCopyButton, "Save Copy", 0, &Slot, 0, "Saves a copy of the current map under a new name (ctrl+shift+alt+s)"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", true, pEditor->CallbackSaveCopyMap, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_MapInfoButton, "Map details", 0, &Slot, 0, "Adjust the map details of the current map"))
	{
		const CUIRect *pScreen = pEditor->UI()->Screen();
		pEditor->m_Map.m_MapInfoTmp.Copy(pEditor->m_Map.m_MapInfo);
		static SPopupMenuId s_PopupMapInfoId;
		constexpr float PopupWidth = 400.0f;
		constexpr float PopupHeight = 170.0f;
		pEditor->UI()->DoPopupMenu(&s_PopupMapInfoId, pScreen->w / 2.0f - PopupWidth / 2.0f, pScreen->h / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight, pEditor, PopupMapInfo);
		pEditor->UI()->SetActiveItem(nullptr);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(10.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ExitButton, "Exit", 0, &Slot, 0, "Exits from the editor"))
	{
		if(pEditor->HasUnsavedData())
		{
			pEditor->m_PopupEventType = POPEVENT_EXIT;
			pEditor->m_PopupEventActivated = true;
		}
		else
			g_Config.m_ClEditor = 0;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupMenuTools(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect Slot;
	View.HSplitTop(12.0f, &Slot, &View);
	static int s_RemoveUnusedEnvelopesButton = 0;
	static CUI::SConfirmPopupContext s_ConfirmPopupContext;
	if(pEditor->DoButton_MenuItem(&s_RemoveUnusedEnvelopesButton, "Remove unused envelopes", 0, &Slot, 0, "Removes all unused envelopes from the map"))
	{
		s_ConfirmPopupContext.Reset();
		s_ConfirmPopupContext.YesNoButtons();
		str_copy(s_ConfirmPopupContext.m_aMessage, "Are you sure that you want to remove all unused envelopes from this map?");
		pEditor->UI()->ShowPopupConfirm(Slot.x + Slot.w, Slot.y, &s_ConfirmPopupContext);
	}
	if(s_ConfirmPopupContext.m_Result == CUI::SConfirmPopupContext::CONFIRMED)
		pEditor->RemoveUnusedEnvelopes();
	if(s_ConfirmPopupContext.m_Result != CUI::SConfirmPopupContext::UNSET)
	{
		s_ConfirmPopupContext.Reset();
		return CUI::POPUP_CLOSE_CURRENT;
	}

	static int s_BorderButton = 0;
	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_BorderButton, "Place Border", 0, &Slot, 0, "Place tiles in a 2-tile wide border at the edges of the layer"))
	{
		std::shared_ptr<CLayerTiles> pT = std::static_pointer_cast<CLayerTiles>(pEditor->GetSelectedLayerType(0, LAYERTYPE_TILES));
		if(pT && !pT->m_Tele && !pT->m_Speedup && !pT->m_Switch && !pT->m_Front && !pT->m_Tune)
		{
			pEditor->m_PopupEventType = POPEVENT_PLACE_BORDER_TILES;
			pEditor->m_PopupEventActivated = true;
		}
		else
		{
			static CUI::SMessagePopupContext s_MessagePopupContext;
			s_MessagePopupContext.DefaultColor(pEditor->m_pTextRender);
			str_copy(s_MessagePopupContext.m_aMessage, "No tile layer selected");
			pEditor->UI()->ShowPopupMessage(Slot.x, Slot.y + Slot.h, &s_MessagePopupContext);
		}
	}

	static int s_GotoButton = 0;
	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_GotoButton, "Goto XY", 0, &Slot, 0, "Go to a specified coordinate point on the map"))
	{
		static SPopupMenuId s_PopupGotoId;
		pEditor->UI()->DoPopupMenu(&s_PopupGotoId, Slot.x, Slot.y + Slot.h, 120, 52, pEditor, PopupGoto);
	}

	return CUI::POPUP_KEEP_OPEN;
}

static int EntitiesListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *)pUser;
	if(!IsDir && str_endswith(pName, ".png"))
	{
		std::string Name = pName;
		pEditor->m_vSelectEntitiesFiles.push_back(Name.substr(0, Name.length() - 4));
	}

	return 0;
}

CUI::EPopupMenuFunctionResult CEditor::PopupMenuSettings(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect Slot;
	View.HSplitTop(12.0f, &Slot, &View);
	static int s_EntitiesButtonID = 0;
	char aButtonText[64];
	str_format(aButtonText, sizeof(aButtonText), "Entities: %s", pEditor->m_SelectEntitiesImage.c_str());
	if(pEditor->DoButton_MenuItem(&s_EntitiesButtonID, aButtonText, 0, &Slot, 0, "Choose game layer entities image for different gametypes"))
	{
		pEditor->m_vSelectEntitiesFiles.clear();
		pEditor->Storage()->ListDirectory(IStorage::TYPE_ALL, "editor/entities", EntitiesListdirCallback, pEditor);
		std::sort(pEditor->m_vSelectEntitiesFiles.begin(), pEditor->m_vSelectEntitiesFiles.end());

		static SPopupMenuId s_PopupEntitiesId;
		pEditor->UI()->DoPopupMenu(&s_PopupEntitiesId, Slot.x, Slot.y + Slot.h, 250, pEditor->m_vSelectEntitiesFiles.size() * 14.0f + 10.0f, pEditor, PopupEntities);
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	{
		Slot.VMargin(5.0f, &Slot);

		CUIRect Label, Selector;
		Slot.VSplitMid(&Label, &Selector);
		CUIRect No, Yes;
		Selector.VSplitMid(&No, &Yes);

		pEditor->UI()->DoLabel(&Label, "Brush coloring", 10.0f, TEXTALIGN_ML);
		static int s_ButtonNo = 0;
		static int s_ButtonYes = 0;
		if(pEditor->DoButton_ButtonDec(&s_ButtonNo, "No", !pEditor->m_BrushColorEnabled, &No, 0, "Disable brush coloring"))
		{
			pEditor->m_BrushColorEnabled = false;
		}
		if(pEditor->DoButton_ButtonInc(&s_ButtonYes, "Yes", pEditor->m_BrushColorEnabled, &Yes, 0, "Enable brush coloring"))
		{
			pEditor->m_BrushColorEnabled = true;
		}
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	{
		Slot.VMargin(5.0f, &Slot);

		CUIRect Label, Selector;
		Slot.VSplitMid(&Label, &Selector);
		CUIRect No, Yes;
		Selector.VSplitMid(&No, &Yes);

		pEditor->UI()->DoLabel(&Label, "Allow unused", 10.0f, TEXTALIGN_ML);
		static int s_ButtonNo = 0;
		static int s_ButtonYes = 0;
		if(pEditor->DoButton_ButtonDec(&s_ButtonNo, "No", !pEditor->m_AllowPlaceUnusedTiles, &No, 0, "[ctrl+u] Disallow placing unused tiles"))
		{
			pEditor->m_AllowPlaceUnusedTiles = false;
		}
		if(pEditor->DoButton_ButtonInc(&s_ButtonYes, "Yes", pEditor->m_AllowPlaceUnusedTiles, &Yes, 0, "[ctrl+u] Allow placing unused tiles"))
		{
			pEditor->m_AllowPlaceUnusedTiles = true;
		}
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	{
		Slot.VMargin(5.0f, &Slot);

		CUIRect Label, Selector;
		Slot.VSplitMid(&Label, &Selector);
		CUIRect Off, Dec, Hex;
		Selector.VSplitLeft(Selector.w / 3.0f, &Off, &Selector);
		Selector.VSplitMid(&Dec, &Hex);

		pEditor->UI()->DoLabel(&Label, "Show Info", 10.0f, TEXTALIGN_ML);
		static int s_ButtonOff = 0;
		static int s_ButtonDec = 0;
		static int s_ButtonHex = 0;
		if(pEditor->DoButton_ButtonDec(&s_ButtonOff, "Off", pEditor->m_ShowTileInfo == SHOW_TILE_OFF, &Off, 0, "Do not show tile information"))
		{
			pEditor->m_ShowTileInfo = SHOW_TILE_OFF;
			pEditor->m_ShowEnvelopePreview = SHOWENV_NONE;
		}
		if(pEditor->DoButton_Ex(&s_ButtonDec, "Dec", pEditor->m_ShowTileInfo == SHOW_TILE_DECIMAL, &Dec, 0, "[ctrl+i] Show tile information", IGraphics::CORNER_NONE))
		{
			pEditor->m_ShowTileInfo = SHOW_TILE_DECIMAL;
			pEditor->m_ShowEnvelopePreview = SHOWENV_NONE;
		}
		if(pEditor->DoButton_ButtonInc(&s_ButtonHex, "Hex", pEditor->m_ShowTileInfo == SHOW_TILE_HEXADECIMAL, &Hex, 0, "[ctrl+shift+i] Show tile information in hexadecimal"))
		{
			pEditor->m_ShowTileInfo = SHOW_TILE_HEXADECIMAL;
			pEditor->m_ShowEnvelopePreview = SHOWENV_NONE;
		}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupGroup(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

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
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}
	else
	{
		if(pEditor->DoButton_Editor(&s_DeleteButton, "Clean up game tiles", 0, &Button, 0, "Removes game tiles that aren't based on a layer"))
		{
			// gather all tile layers
			std::vector<std::shared_ptr<CLayerTiles>> vpLayers;
			for(auto &pLayer : pEditor->m_Map.m_pGameGroup->m_vpLayers)
			{
				if(pLayer != pEditor->m_Map.m_pGameLayer && pLayer->m_Type == LAYERTYPE_TILES)
					vpLayers.push_back(std::static_pointer_cast<CLayerTiles>(pLayer));
			}

			// search for unneeded game tiles
			std::shared_ptr<CLayerTiles> pGameLayer = pEditor->m_Map.m_pGameLayer;
			for(int y = 0; y < pGameLayer->m_Height; ++y)
			{
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
						pEditor->m_Map.OnModify();
					}
				}
			}

			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pTeleLayer)
	{
		// new tele layer
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewTeleLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewTeleLayerButton, "Add tele layer", 0, &Button, 0, "Creates a new tele layer"))
		{
			std::shared_ptr<CLayer> pTeleLayer = std::make_shared<CLayerTele>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeTeleLayer(pTeleLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTeleLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_pBrush->Clear();
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pSpeedupLayer)
	{
		// new speedup layer
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewSpeedupLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewSpeedupLayerButton, "Add speedup layer", 0, &Button, 0, "Creates a new speedup layer"))
		{
			std::shared_ptr<CLayer> pSpeedupLayer = std::make_shared<CLayerSpeedup>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeSpeedupLayer(pSpeedupLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSpeedupLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_pBrush->Clear();
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pTuneLayer)
	{
		// new tune layer
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewTuneLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewTuneLayerButton, "Add tune layer", 0, &Button, 0, "Creates a new tuning layer"))
		{
			std::shared_ptr<CLayer> pTuneLayer = std::make_shared<CLayerTune>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeTuneLayer(pTuneLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTuneLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_pBrush->Clear();
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pFrontLayer)
	{
		// new front layer
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewFrontLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewFrontLayerButton, "Add front layer", 0, &Button, 0, "Creates a new item layer"))
		{
			std::shared_ptr<CLayer> pFrontLayer = std::make_shared<CLayerFront>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeFrontLayer(pFrontLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pFrontLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_pBrush->Clear();
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	if(pEditor->GetSelectedGroup()->m_GameGroup && !pEditor->m_Map.m_pSwitchLayer)
	{
		// new Switch layer
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		static int s_NewSwitchLayerButton = 0;
		if(pEditor->DoButton_Editor(&s_NewSwitchLayerButton, "Add switch layer", 0, &Button, 0, "Creates a new switch layer"))
		{
			std::shared_ptr<CLayer> pSwitchLayer = std::make_shared<CLayerSwitch>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
			pEditor->m_Map.MakeSwitchLayer(pSwitchLayer);
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSwitchLayer);
			pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
			pEditor->m_pBrush->Clear();
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	// new quad layer
	View.HSplitBottom(5.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewQuadLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewQuadLayerButton, "Add quads layer", 0, &Button, 0, "Creates a new quad layer"))
	{
		std::shared_ptr<CLayer> pQuadLayer = std::make_shared<CLayerQuads>();
		pQuadLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pQuadLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// new tile layer
	View.HSplitBottom(5.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewTileLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewTileLayerButton, "Add tile layer", 0, &Button, 0, "Creates a new tile layer"))
	{
		std::shared_ptr<CLayer> pTileLayer = std::make_shared<CLayerTiles>(pEditor->m_Map.m_pGameLayer->m_Width, pEditor->m_Map.m_pGameLayer->m_Height);
		pTileLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pTileLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// new sound layer
	View.HSplitBottom(5.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_NewSoundLayerButton = 0;
	if(pEditor->DoButton_Editor(&s_NewSoundLayerButton, "Add sound layer", 0, &Button, 0, "Creates a new sound layer"))
	{
		std::shared_ptr<CLayer> pSoundLayer = std::make_shared<CLayerSounds>();
		pSoundLayer->m_pEditor = pEditor;
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->AddLayer(pSoundLayer);
		pEditor->SelectLayer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_vpLayers.size() - 1);
		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_Collapse = false;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// group name
	if(!pEditor->GetSelectedGroup()->m_GameGroup)
	{
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Button);
		pEditor->UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_ML);
		Button.VSplitLeft(40.0f, nullptr, &Button);
		static CLineInput s_NameInput;
		s_NameInput.SetBuffer(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_aName, sizeof(pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_aName));
		if(pEditor->DoEditBox(&s_NameInput, &Button, 10.0f))
			pEditor->m_Map.OnModify();
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

	// cut the properties that aren't needed
	if(pEditor->GetSelectedGroup()->m_GameGroup)
		aProps[PROP_POS_X].m_pName = nullptr;

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
	{
		pEditor->m_Map.OnModify();
	}

	if(Prop == PROP_ORDER)
	{
		pEditor->m_SelectedGroup = pEditor->m_Map.SwapGroups(pEditor->m_SelectedGroup, NewVal);
	}

	// these can not be changed on the game group
	if(!pEditor->GetSelectedGroup()->m_GameGroup)
	{
		if(Prop == PROP_PARA_X)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxX = NewVal;
		}
		else if(Prop == PROP_PARA_Y)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxY = NewVal;
		}
		else if(Prop == PROP_CUSTOM_ZOOM)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_CustomParallaxZoom = NewVal;
		}
		else if(Prop == PROP_PARA_ZOOM)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_CustomParallaxZoom = 1;
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ParallaxZoom = NewVal;
		}
		else if(Prop == PROP_POS_X)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetX = -NewVal;
		}
		else if(Prop == PROP_POS_Y)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_OffsetY = -NewVal;
		}
		else if(Prop == PROP_USE_CLIPPING)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_UseClipping = NewVal;
		}
		else if(Prop == PROP_CLIP_X)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipX = NewVal;
		}
		else if(Prop == PROP_CLIP_Y)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipY = NewVal;
		}
		else if(Prop == PROP_CLIP_W)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipW = NewVal;
		}
		else if(Prop == PROP_CLIP_H)
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->m_ClipH = NewVal;
		}

		pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->OnEdited();
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupLayer(void *pContext, CUIRect View, bool Active)
{
	SLayerPopupContext *pPopup = (SLayerPopupContext *)pContext;
	CEditor *pEditor = pPopup->m_pEditor;

	std::shared_ptr<CLayerGroup> pCurrentGroup = pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup];
	std::shared_ptr<CLayer> pCurrentLayer = pEditor->GetSelectedLayer(0);

	if(pPopup->m_vpLayers.size() > 1)
	{
		return CLayerTiles::RenderCommonProperties(pPopup->m_CommonPropState, pEditor, &View, pPopup->m_vpLayers);
	}

	const bool EntitiesLayer = pCurrentLayer->IsEntitiesLayer();

	// delete button
	if(pEditor->m_Map.m_pGameLayer != pCurrentLayer) // entities layers except the game layer can be deleted
	{
		CUIRect DeleteButton;
		View.HSplitBottom(12.0f, &View, &DeleteButton);
		static int s_DeleteButton = 0;
		if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete layer", 0, &DeleteButton, 0, "Deletes the layer"))
		{
			if(pCurrentLayer == pEditor->m_Map.m_pFrontLayer)
				pEditor->m_Map.m_pFrontLayer = nullptr;
			if(pCurrentLayer == pEditor->m_Map.m_pTeleLayer)
				pEditor->m_Map.m_pTeleLayer = nullptr;
			if(pCurrentLayer == pEditor->m_Map.m_pSpeedupLayer)
				pEditor->m_Map.m_pSpeedupLayer = nullptr;
			if(pCurrentLayer == pEditor->m_Map.m_pSwitchLayer)
				pEditor->m_Map.m_pSwitchLayer = nullptr;
			if(pCurrentLayer == pEditor->m_Map.m_pTuneLayer)
				pEditor->m_Map.m_pTuneLayer = nullptr;
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->DeleteLayer(pEditor->m_vSelectedLayers[0]);
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	// duplicate button
	if(!EntitiesLayer) // entities layers cannot be duplicated
	{
		CUIRect DuplicateButton;
		View.HSplitBottom(4.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &DuplicateButton);
		static int s_DuplicationButton = 0;
		if(pEditor->DoButton_Editor(&s_DuplicationButton, "Duplicate layer", 0, &DuplicateButton, 0, "Duplicates the layer"))
		{
			pEditor->m_Map.m_vpGroups[pEditor->m_SelectedGroup]->DuplicateLayer(pEditor->m_vSelectedLayers[0]);
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	// layer name
	if(!EntitiesLayer) // name cannot be changed for entities layers
	{
		CUIRect Label, EditBox;
		View.HSplitBottom(5.0f, &View, nullptr);
		View.HSplitBottom(12.0f, &View, &Label);
		Label.VSplitLeft(40.0f, &Label, &EditBox);
		pEditor->UI()->DoLabel(&Label, "Name:", 10.0f, TEXTALIGN_ML);
		static CLineInput s_NameInput;
		s_NameInput.SetBuffer(pCurrentLayer->m_aName, sizeof(pCurrentLayer->m_aName));
		if(pEditor->DoEditBox(&s_NameInput, &EditBox, 10.0f))
			pEditor->m_Map.OnModify();
	}

	// spacing if any button was rendered
	if(!EntitiesLayer || pEditor->m_Map.m_pGameLayer != pCurrentLayer)
		View.HSplitBottom(10.0f, &View, nullptr);

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
		{"Detail", pCurrentLayer->m_Flags & LAYERFLAG_DETAIL, PROPTYPE_BOOL, 0, 1},
		{nullptr},
	};

	// don't use Group and Detail from the selection if this is an entities layer
	if(EntitiesLayer)
	{
		aProps[0].m_Type = PROPTYPE_NULL;
		aProps[2].m_Type = PROPTYPE_NULL;
	}

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
	{
		pEditor->m_Map.OnModify();
	}

	if(Prop == PROP_ORDER)
	{
		pEditor->SelectLayer(pCurrentGroup->SwapLayers(pEditor->m_vSelectedLayers[0], NewVal));
	}
	else if(Prop == PROP_GROUP)
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
	else if(Prop == PROP_HQ)
	{
		pCurrentLayer->m_Flags &= ~LAYERFLAG_DETAIL;
		if(NewVal)
			pCurrentLayer->m_Flags |= LAYERFLAG_DETAIL;
	}

	return pCurrentLayer->RenderProperties(&View);
}

CUI::EPopupMenuFunctionResult CEditor::PopupQuad(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	std::vector<CQuad *> vpQuads = pEditor->GetSelectedQuads();
	CQuad *pCurrentQuad = vpQuads[pEditor->m_SelectedQuadIndex];
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(pEditor->GetSelectedLayerType(0, LAYERTYPE_QUADS));

	CUIRect Button;

	// delete button
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;
	if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, "Deletes the current quad"))
	{
		if(pLayer)
		{
			pEditor->m_Map.OnModify();
			pEditor->DeleteSelectedQuads();
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// aspect ratio button
	View.HSplitBottom(10.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &Button);
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

				const int Height = (Right - Left) * pEditor->m_Map.m_vpImages[pLayer->m_Image]->m_Height / pEditor->m_Map.m_vpImages[pLayer->m_Image]->m_Width;

				pQuad->m_aPoints[0].x = Left;
				pQuad->m_aPoints[0].y = Top;
				pQuad->m_aPoints[1].x = Right;
				pQuad->m_aPoints[1].y = Top;
				pQuad->m_aPoints[2].x = Left;
				pQuad->m_aPoints[2].y = Top + Height;
				pQuad->m_aPoints[3].x = Right;
				pQuad->m_aPoints[3].y = Top + Height;
				pEditor->m_Map.OnModify();
			}
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	// align button
	View.HSplitBottom(6.0f, &View, nullptr);
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
			pEditor->m_Map.OnModify();
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// square button
	View.HSplitBottom(6.0f, &View, nullptr);
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
			pEditor->m_Map.OnModify();
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// slice button
	View.HSplitBottom(6.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_SliceButton = 0;
	if(pEditor->DoButton_Editor(&s_SliceButton, "Slice", 0, &Button, 0, "Enables quad knife mode"))
	{
		pEditor->m_QuadKnifeCount = 0;
		pEditor->m_QuadKnifeActive = true;
		return CUI::POPUP_CLOSE_CURRENT;
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

	const int NumQuads = pLayer ? (int)pLayer->m_vQuads.size() : 0;
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
	{
		pEditor->m_Map.OnModify();
	}

	const float OffsetX = i2fx(NewVal) - pCurrentQuad->m_aPoints[4].x;
	const float OffsetY = i2fx(NewVal) - pCurrentQuad->m_aPoints[4].y;

	if(Prop == PROP_ORDER && pLayer)
	{
		const int QuadIndex = pLayer->SwapQuads(pEditor->m_vSelectedQuads[pEditor->m_SelectedQuadIndex], NewVal);
		pEditor->m_vSelectedQuads[pEditor->m_SelectedQuadIndex] = QuadIndex;
	}

	for(auto &pQuad : vpQuads)
	{
		if(Prop == PROP_POS_X)
		{
			for(auto &Point : pQuad->m_aPoints)
				Point.x += OffsetX;
		}
		else if(Prop == PROP_POS_Y)
		{
			for(auto &Point : pQuad->m_aPoints)
				Point.y += OffsetY;
		}
		else if(Prop == PROP_POS_ENV)
		{
			int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
			int StepDirection = Index < pQuad->m_PosEnv ? -1 : 1;
			if(StepDirection != 0)
			{
				for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += StepDirection)
				{
					if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->GetChannels() == 3)
					{
						pQuad->m_PosEnv = Index;
						break;
					}
				}
			}
		}
		else if(Prop == PROP_POS_ENV_OFFSET)
		{
			pQuad->m_PosEnvOffset = NewVal;
		}
		else if(Prop == PROP_COLOR_ENV)
		{
			int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
			int StepDirection = Index < pQuad->m_ColorEnv ? -1 : 1;
			if(StepDirection != 0)
			{
				for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += StepDirection)
				{
					if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->GetChannels() == 4)
					{
						pQuad->m_ColorEnv = Index;
						break;
					}
				}
			}
		}
		else if(Prop == PROP_COLOR_ENV_OFFSET)
		{
			pQuad->m_ColorEnvOffset = NewVal;
		}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupSource(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	CSoundSource *pSource = pEditor->GetSelectedSource();

	CUIRect Button;

	// delete button
	View.HSplitBottom(12.0f, &View, &Button);
	static int s_DeleteButton = 0;
	if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete", 0, &Button, 0, "Deletes the current source"))
	{
		std::shared_ptr<CLayerSounds> pLayer = std::static_pointer_cast<CLayerSounds>(pEditor->GetSelectedLayerType(0, LAYERTYPE_SOUNDS));
		if(pLayer)
		{
			pEditor->m_Map.OnModify();
			pLayer->m_vSources.erase(pLayer->m_vSources.begin() + pEditor->m_SelectedSource);
			pEditor->m_SelectedSource--;
		}
		return CUI::POPUP_CLOSE_CURRENT;
	}

	// Sound shape button
	CUIRect ShapeButton;
	View.HSplitBottom(3.0f, &View, nullptr);
	View.HSplitBottom(12.0f, &View, &ShapeButton);

	static const char *s_apShapeNames[CSoundShape::NUM_SHAPES] = {
		"Rectangle",
		"Circle"};

	pSource->m_Shape.m_Type = pSource->m_Shape.m_Type % CSoundShape::NUM_SHAPES; // prevent out of array errors

	static int s_ShapeTypeButton = 0;
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
	{
		pEditor->m_Map.OnModify();
	}

	if(Prop == PROP_POS_X)
	{
		pSource->m_Position.x = NewVal * 1000;
	}
	else if(Prop == PROP_POS_Y)
	{
		pSource->m_Position.y = NewVal * 1000;
	}
	else if(Prop == PROP_LOOP)
	{
		pSource->m_Loop = NewVal;
	}
	else if(Prop == PROP_PAN)
	{
		pSource->m_Pan = NewVal;
	}
	else if(Prop == PROP_TIME_DELAY)
	{
		pSource->m_TimeDelay = NewVal;
	}
	else if(Prop == PROP_FALLOFF)
	{
		pSource->m_Falloff = NewVal;
	}
	else if(Prop == PROP_POS_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
		const int StepDirection = Index < pSource->m_PosEnv ? -1 : 1;
		for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += StepDirection)
		{
			if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->GetChannels() == 3)
			{
				pSource->m_PosEnv = Index;
				break;
			}
		}
	}
	else if(Prop == PROP_POS_ENV_OFFSET)
	{
		pSource->m_PosEnvOffset = NewVal;
	}
	else if(Prop == PROP_SOUND_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)pEditor->m_Map.m_vpEnvelopes.size() - 1);
		const int StepDirection = Index < pSource->m_SoundEnv ? -1 : 1;
		for(; Index >= -1 && Index < (int)pEditor->m_Map.m_vpEnvelopes.size(); Index += StepDirection)
		{
			if(Index == -1 || pEditor->m_Map.m_vpEnvelopes[Index]->GetChannels() == 1)
			{
				pSource->m_SoundEnv = Index;
				break;
			}
		}
	}
	else if(Prop == PROP_SOUND_ENV_OFFSET)
	{
		pSource->m_SoundEnvOffset = NewVal;
	}

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
		{
			pEditor->m_Map.OnModify();
		}

		if(Prop == PROP_CIRCLE_RADIUS)
		{
			pSource->m_Shape.m_Circle.m_Radius = NewVal;
		}

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
		{
			pEditor->m_Map.OnModify();
		}

		if(Prop == PROP_RECTANGLE_WIDTH)
		{
			pSource->m_Shape.m_Rectangle.m_Width = NewVal * 1024;
		}
		else if(Prop == PROP_RECTANGLE_HEIGHT)
		{
			pSource->m_Shape.m_Rectangle.m_Height = NewVal * 1024;
		}

		break;
	}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupPoint(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	std::vector<std::pair<CQuad *, int>> vpQuads = pEditor->GetSelectedQuadPoints();
	if(!in_range<int>(pEditor->m_SelectedQuadIndex, 0, vpQuads.size() - 1))
		return CUI::POPUP_CLOSE_CURRENT;
	CQuad *pCurrentQuad = vpQuads[pEditor->m_SelectedQuadIndex].first;

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

	const int X = fx2i(pCurrentQuad->m_aPoints[pEditor->m_SelectedQuadPoint].x);
	const int Y = fx2i(pCurrentQuad->m_aPoints[pEditor->m_SelectedQuadPoint].y);
	const int TextureU = fx2f(pCurrentQuad->m_aTexcoords[pEditor->m_SelectedQuadPoint].x) * 1024;
	const int TextureV = fx2f(pCurrentQuad->m_aTexcoords[pEditor->m_SelectedQuadPoint].y) * 1024;

	CProperty aProps[] = {
		{"Pos X", X, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Pos Y", Y, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Color", Color, PROPTYPE_COLOR, 0, 0},
		{"Tex U", TextureU, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Tex V", TextureV, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);
	if(Prop != -1)
	{
		pEditor->m_Map.OnModify();
	}

	for(auto [pQuad, SelectedPoints] : vpQuads)
	{
		if(Prop == PROP_POS_X)
		{
			for(int v = 0; v < 4; v++)
				if(SelectedPoints & (1 << v))
					pQuad->m_aPoints[v].x = i2fx(fx2i(pQuad->m_aPoints[v].x) + NewVal - X);
		}
		else if(Prop == PROP_POS_Y)
		{
			for(int v = 0; v < 4; v++)
				if(SelectedPoints & (1 << v))
					pQuad->m_aPoints[v].y = i2fx(fx2i(pQuad->m_aPoints[v].y) + NewVal - Y);
		}
		else if(Prop == PROP_COLOR)
		{
			for(int v = 0; v < 4; v++)
			{
				if(SelectedPoints & (1 << v))
				{
					pQuad->m_aColors[v].r = (NewVal >> 24) & 0xff;
					pQuad->m_aColors[v].g = (NewVal >> 16) & 0xff;
					pQuad->m_aColors[v].b = (NewVal >> 8) & 0xff;
					pQuad->m_aColors[v].a = NewVal & 0xff;
				}
			}
		}
		else if(Prop == PROP_TEX_U)
		{
			for(int v = 0; v < 4; v++)
				if(SelectedPoints & (1 << v))
					pQuad->m_aTexcoords[v].x = f2fx(fx2f(pQuad->m_aTexcoords[v].x) + (NewVal - TextureU) / 1024.0f);
		}
		else if(Prop == PROP_TEX_V)
		{
			for(int v = 0; v < 4; v++)
				if(SelectedPoints & (1 << v))
					pQuad->m_aTexcoords[v].y = f2fx(fx2f(pQuad->m_aTexcoords[v].y) + (NewVal - TextureV) / 1024.0f);
		}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupEnvPoint(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	const float RowHeight = 12.0f;
	CUIRect Row, Label, EditBox;

	pEditor->m_ShowEnvelopePreview = SHOWENV_SELECTED;

	static CLineInputNumber s_CurValueInput;
	static CLineInputNumber s_CurTimeInput;

	std::shared_ptr<CEnvelope> pEnvelope = pEditor->m_Map.m_vpEnvelopes[pEditor->m_SelectedEnvelope];
	if(pEditor->m_UpdateEnvPointInfo)
	{
		pEditor->m_UpdateEnvPointInfo = false;

		int CurrentTime;
		int CurrentValue;
		if(pEditor->IsTangentInSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentInPoint;

			CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel];
			CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaY[SelectedChannel];
		}
		else if(pEditor->IsTangentOutSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentOutPoint;

			CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel];
			CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaY[SelectedChannel];
		}
		else
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_vSelectedEnvelopePoints.front();

			CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time;
			CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
		}

		// update displayed text
		s_CurValueInput.SetFloat(fx2f(CurrentValue));
		s_CurTimeInput.SetFloat(CurrentTime / 1000.0f);
	}

	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->UI()->DoLabel(&Label, "Value:", RowHeight - 2.0f, TEXTALIGN_LEFT);
	pEditor->DoEditBox(&s_CurValueInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The value of the selected envelope point");

	View.HMargin(4.0f, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->UI()->DoLabel(&Label, "Time (in s):", RowHeight - 2.0f, TEXTALIGN_LEFT);
	pEditor->DoEditBox(&s_CurTimeInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The time of the selected envelope point");

	if(pEditor->Input()->KeyIsPressed(KEY_RETURN) || pEditor->Input()->KeyIsPressed(KEY_KP_ENTER))
	{
		float CurrentTime = s_CurTimeInput.GetFloat();
		float CurrentValue = s_CurValueInput.GetFloat();
		if(pEditor->IsTangentInSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentInPoint;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel] = minimum<int>(CurrentTime * 1000.0f - pEnvelope->m_vPoints[SelectedIndex].m_Time, 0);
			CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel]) / 1000.0f;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaY[SelectedChannel] = f2fx(CurrentValue) - pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
		}
		else if(pEditor->IsTangentOutSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentOutPoint;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel] = maximum<int>(CurrentTime * 1000.0f - pEnvelope->m_vPoints[SelectedIndex].m_Time, 0);
			CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel]) / 1000.0f;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaY[SelectedChannel] = f2fx(CurrentValue) - pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
		}
		else
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_vSelectedEnvelopePoints.front();

			pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = f2fx(CurrentValue);
			if(SelectedIndex != 0)
			{
				pEnvelope->m_vPoints[SelectedIndex].m_Time = CurrentTime * 1000.0f;

				if(pEnvelope->m_vPoints[SelectedIndex].m_Time < pEnvelope->m_vPoints[SelectedIndex - 1].m_Time)
					pEnvelope->m_vPoints[SelectedIndex].m_Time = pEnvelope->m_vPoints[SelectedIndex - 1].m_Time + 1;
				if(static_cast<size_t>(SelectedIndex) + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[SelectedIndex].m_Time > pEnvelope->m_vPoints[SelectedIndex + 1].m_Time)
					pEnvelope->m_vPoints[SelectedIndex].m_Time = pEnvelope->m_vPoints[SelectedIndex + 1].m_Time - 1;

				CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time / 1000.0f;
			}
			else
			{
				CurrentTime = 0.0f;
				pEnvelope->m_vPoints[SelectedIndex].m_Time = 0.0f;
			}
		}

		s_CurTimeInput.SetFloat(static_cast<int>(CurrentTime * 1000.0f) / 1000.0f);
		s_CurValueInput.SetFloat(fx2f(f2fx(CurrentValue)));

		pEditor->m_Map.OnModify();
	}

	View.HMargin(6.0f, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	static int s_DeleteButtonID = 0;
	const char *pButtonText = pEditor->IsTangentSelected() ? "Reset" : "Delete";
	const char *pTooltip = pEditor->IsTangentSelected() ? "Reset tangent point to default value." : "Delete current envelope point in all channels.";
	if(pEditor->DoButton_Editor(&s_DeleteButtonID, pButtonText, 0, &Row, 0, pTooltip))
	{
		if(pEditor->IsTangentInSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentInPoint;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel] = 0.0f;
			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaY[SelectedChannel] = 0.0f;
		}
		else if(pEditor->IsTangentOutSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_SelectedTangentOutPoint;

			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel] = 0.0f;
			pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaY[SelectedChannel] = 0.0f;
		}
		else
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->m_vSelectedEnvelopePoints.front();

			pEnvelope->m_vPoints.erase(pEnvelope->m_vPoints.begin() + SelectedIndex);
		}
		pEditor->m_Map.OnModify();

		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

static const auto &&gs_ModifyIndexDeleted = [](int DeletedIndex) {
	return [DeletedIndex](int *pIndex) {
		if(*pIndex == DeletedIndex)
			*pIndex = -1;
		else if(*pIndex > DeletedIndex)
			*pIndex = *pIndex - 1;
	};
};

CUI::EPopupMenuFunctionResult CEditor::PopupImage(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	static int s_ReaddButton = 0;
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(12.0f, &Slot, &View);
	std::shared_ptr<CEditorImage> pImg = pEditor->m_Map.m_vpImages[pEditor->m_SelectedImage];

	static int s_ExternalButton = 0;
	if(pImg->m_External)
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Embed", 0, &Slot, 0, "Embeds the image into the map file."))
		{
			pImg->m_External = 0;
			return CUI::POPUP_CLOSE_CURRENT;
		}
		View.HSplitTop(5.0f, nullptr, &View);
		View.HSplitTop(12.0f, &Slot, &View);
	}
	else if(pEditor->IsVanillaImage(pImg->m_aName))
	{
		if(pEditor->DoButton_MenuItem(&s_ExternalButton, "Make external", 0, &Slot, 0, "Removes the image from the map file."))
		{
			pImg->m_External = 1;
			return CUI::POPUP_CLOSE_CURRENT;
		}
		View.HSplitTop(5.0f, nullptr, &View);
		View.HSplitTop(12.0f, &Slot, &View);
	}

	static CUI::SSelectionPopupContext s_SelectionPopupContext;
	static CScrollRegion s_SelectionPopupScrollRegion;
	s_SelectionPopupContext.m_pScrollRegion = &s_SelectionPopupScrollRegion;
	if(pEditor->DoButton_MenuItem(&s_ReaddButton, "Readd", 0, &Slot, 0, "Reloads the image from the mapres folder"))
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "%s.png", pImg->m_aName);
		s_SelectionPopupContext.Reset();
		std::set<std::string> EntriesSet;
		pEditor->Storage()->FindFiles(aFilename, "mapres", IStorage::TYPE_ALL, &EntriesSet);
		for(const auto &Entry : EntriesSet)
			s_SelectionPopupContext.m_vEntries.push_back(Entry);
		if(s_SelectionPopupContext.m_vEntries.empty())
		{
			pEditor->ShowFileDialogError("Error: could not find image '%s' in the mapres folder.", aFilename);
		}
		else if(s_SelectionPopupContext.m_vEntries.size() == 1)
		{
			s_SelectionPopupContext.m_pSelection = &s_SelectionPopupContext.m_vEntries.front();
		}
		else
		{
			str_copy(s_SelectionPopupContext.m_aMessage, "Select the wanted image:");
			pEditor->UI()->ShowPopupSelection(pEditor->UI()->MouseX(), pEditor->UI()->MouseY(), &s_SelectionPopupContext);
		}
	}
	if(s_SelectionPopupContext.m_pSelection != nullptr)
	{
		const bool WasExternal = pImg->m_External;
		const bool Result = pEditor->ReplaceImage(s_SelectionPopupContext.m_pSelection->c_str(), IStorage::TYPE_ALL, false);
		pImg->m_External = WasExternal;
		s_SelectionPopupContext.Reset();
		return Result ? CUI::POPUP_CLOSE_CURRENT : CUI::POPUP_KEEP_OPEN;
	}

	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the image with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Replace Image", "Replace", "mapres", false, ReplaceImageCallback, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the image from the map"))
	{
		pEditor->m_Map.m_vpImages.erase(pEditor->m_Map.m_vpImages.begin() + pEditor->m_SelectedImage);
		pEditor->m_Map.ModifyImageIndex(gs_ModifyIndexDeleted(pEditor->m_SelectedImage));
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupSound(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	static int s_ReaddButton = 0;
	static int s_ReplaceButton = 0;
	static int s_RemoveButton = 0;

	CUIRect Slot;
	View.HSplitTop(12.0f, &Slot, &View);
	std::shared_ptr<CEditorSound> pSound = pEditor->m_Map.m_vpSounds[pEditor->m_SelectedSound];

	static CUI::SSelectionPopupContext s_SelectionPopupContext;
	static CScrollRegion s_SelectionPopupScrollRegion;
	s_SelectionPopupContext.m_pScrollRegion = &s_SelectionPopupScrollRegion;
	if(pEditor->DoButton_MenuItem(&s_ReaddButton, "Readd", 0, &Slot, 0, "Reloads the sound from the mapres folder"))
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "%s.opus", pSound->m_aName);
		s_SelectionPopupContext.Reset();
		std::set<std::string> EntriesSet;
		pEditor->Storage()->FindFiles(aFilename, "mapres", IStorage::TYPE_ALL, &EntriesSet);
		for(const auto &Entry : EntriesSet)
			s_SelectionPopupContext.m_vEntries.push_back(Entry);
		if(s_SelectionPopupContext.m_vEntries.empty())
		{
			pEditor->ShowFileDialogError("Error: could not find sound '%s' in the mapres folder.", aFilename);
		}
		else if(s_SelectionPopupContext.m_vEntries.size() == 1)
		{
			s_SelectionPopupContext.m_pSelection = &s_SelectionPopupContext.m_vEntries.front();
		}
		else
		{
			str_copy(s_SelectionPopupContext.m_aMessage, "Select the wanted sound:");
			pEditor->UI()->ShowPopupSelection(pEditor->UI()->MouseX(), pEditor->UI()->MouseY(), &s_SelectionPopupContext);
		}
	}
	if(s_SelectionPopupContext.m_pSelection != nullptr)
	{
		const bool Result = pEditor->ReplaceSound(s_SelectionPopupContext.m_pSelection->c_str(), IStorage::TYPE_ALL, false);
		s_SelectionPopupContext.Reset();
		return Result ? CUI::POPUP_CLOSE_CURRENT : CUI::POPUP_KEEP_OPEN;
	}

	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_ReplaceButton, "Replace", 0, &Slot, 0, "Replaces the sound with a new one"))
	{
		pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_SOUND, "Replace sound", "Replace", "mapres", false, ReplaceSoundCallback, pEditor);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Slot, &View);
	if(pEditor->DoButton_MenuItem(&s_RemoveButton, "Remove", 0, &Slot, 0, "Removes the sound from the map"))
	{
		pEditor->m_Map.m_vpSounds.erase(pEditor->m_Map.m_vpSounds.begin() + pEditor->m_SelectedSound);
		pEditor->m_Map.ModifySoundIndex(gs_ModifyIndexDeleted(pEditor->m_SelectedSound));
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupNewFolder(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect Label, ButtonBar, Button;

	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	// title
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Create new folder", 20.0f, TEXTALIGN_MC);
	View.HSplitTop(10.0f, nullptr, &View);

	// folder name
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Name:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(50.0f, nullptr, &Button);
	Button.HMargin(2.0f, &Button);
	pEditor->DoEditBox(&pEditor->m_FileDialogNewFolderNameInput, &Button, 12.0f);

	// button bar
	ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
	static int s_CancelButton = 0;
	if(pEditor->DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, nullptr))
		return CUI::POPUP_CLOSE_CURRENT;

	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Button);
	static int s_CreateButton = 0;
	if(pEditor->DoButton_Editor(&s_CreateButton, "Create", 0, &Button, 0, nullptr) || (Active && pEditor->UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		// create the folder
		if(!pEditor->m_FileDialogNewFolderNameInput.IsEmpty())
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			str_format(aBuf, sizeof(aBuf), "%s/%s", pEditor->m_pFileDialogPath, pEditor->m_FileDialogNewFolderNameInput.GetString());
			if(pEditor->Storage()->CreateFolder(aBuf, IStorage::TYPE_SAVE))
			{
				pEditor->FilelistPopulate(IStorage::TYPE_SAVE);
				return CUI::POPUP_CLOSE_CURRENT;
			}
			else
			{
				pEditor->ShowFileDialogError("Failed to create the folder '%s'.", aBuf);
			}
		}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupMapInfo(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect Label, ButtonBar, Button;

	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	// title
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Map details", 20.0f, TEXTALIGN_MC);
	View.HSplitTop(10.0f, nullptr, &View);

	// author box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Author:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(60.0f, nullptr, &Button);
	Button.HMargin(3.0f, &Button);
	static CLineInput s_AuthorInput;
	s_AuthorInput.SetBuffer(pEditor->m_Map.m_MapInfoTmp.m_aAuthor, sizeof(pEditor->m_Map.m_MapInfoTmp.m_aAuthor));
	pEditor->DoEditBox(&s_AuthorInput, &Button, 10.0f);

	// version box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Version:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(60.0f, nullptr, &Button);
	Button.HMargin(3.0f, &Button);
	static CLineInput s_VersionInput;
	s_VersionInput.SetBuffer(pEditor->m_Map.m_MapInfoTmp.m_aVersion, sizeof(pEditor->m_Map.m_MapInfoTmp.m_aVersion));
	pEditor->DoEditBox(&s_VersionInput, &Button, 10.0f);

	// credits box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "Credits:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(60.0f, nullptr, &Button);
	Button.HMargin(3.0f, &Button);
	static CLineInput s_CreditsInput;
	s_CreditsInput.SetBuffer(pEditor->m_Map.m_MapInfoTmp.m_aCredits, sizeof(pEditor->m_Map.m_MapInfoTmp.m_aCredits));
	pEditor->DoEditBox(&s_CreditsInput, &Button, 10.0f);

	// license box
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, "License:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(60.0f, nullptr, &Button);
	Button.HMargin(3.0f, &Button);
	static CLineInput s_LicenseInput;
	s_LicenseInput.SetBuffer(pEditor->m_Map.m_MapInfoTmp.m_aLicense, sizeof(pEditor->m_Map.m_MapInfoTmp.m_aLicense));
	pEditor->DoEditBox(&s_LicenseInput, &Button, 10.0f);

	// button bar
	ButtonBar.VSplitLeft(110.0f, &Label, &ButtonBar);
	static int s_CancelButton = 0;
	if(pEditor->DoButton_Editor(&s_CancelButton, "Cancel", 0, &Label, 0, nullptr))
		return CUI::POPUP_CLOSE_CURRENT;

	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Label);
	static int s_ConfirmButton = 0;
	if(pEditor->DoButton_Editor(&s_ConfirmButton, "Confirm", 0, &Label, 0, nullptr) || (Active && pEditor->UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		pEditor->m_Map.m_MapInfo.Copy(pEditor->m_Map.m_MapInfoTmp);
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupEvent(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	const char *pTitle;
	const char *pMessage;
	if(pEditor->m_PopupEventType == POPEVENT_EXIT)
	{
		pTitle = "Exit the editor";
		pMessage = "The map contains unsaved data, you might want to save it before you exit the editor.\n\nContinue anyway?";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_LOAD || pEditor->m_PopupEventType == POPEVENT_LOADCURRENT)
	{
		pTitle = "Load map";
		pMessage = "The map contains unsaved data, you might want to save it before you load a new map.\n\nContinue anyway?";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_NEW)
	{
		pTitle = "New map";
		pMessage = "The map contains unsaved data, you might want to save it before you create a new map.\n\nContinue anyway?";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_SAVE || pEditor->m_PopupEventType == POPEVENT_SAVE_COPY)
	{
		pTitle = "Save map";
		pMessage = "The file already exists.\n\nDo you want to overwrite the map?";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_LARGELAYER)
	{
		pTitle = "Large layer";
		pMessage = "You are trying to set the height or width of a layer to more than 1000 tiles. This is actually possible, but only rarely necessary. It may cause the editor to work slower and will result in a larger file size as well as higher memory usage for client and server.";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_PREVENTUNUSEDTILES)
	{
		pTitle = "Unused tiles disabled";
		pMessage = "Unused tiles can't be placed by default because they could get a use later and then destroy your map.\n\nActivate the 'Allow Unused' setting to be able to place every tile.";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGEDIV16)
	{
		pTitle = "Image width/height";
		pMessage = "The width or height of this image is not divisible by 16. This is required for images used in tile layers.";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_IMAGE_MAX)
	{
		pTitle = "Max images";
		pMessage = "The client only allows a maximum of 64 images.";
	}
	else if(pEditor->m_PopupEventType == POPEVENT_PLACE_BORDER_TILES)
	{
		pTitle = "Place border tiles";
		pMessage = "This is going to overwrite any existing tiles around the edges of the layer.\n\nContinue?";
	}
	else
	{
		dbg_assert(false, "m_PopupEventType invalid");
		return CUI::POPUP_CLOSE_CURRENT;
	}

	CUIRect Label, ButtonBar, Button;

	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	// title
	View.HSplitTop(20.0f, &Label, &View);
	pEditor->UI()->DoLabel(&Label, pTitle, 20.0f, TEXTALIGN_MC);

	// message
	SLabelProperties Props;
	Props.m_MaxWidth = View.w;
	pEditor->UI()->DoLabel(&View, pMessage, 10.0f, TEXTALIGN_ML, Props);

	// button bar
	ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
	if(pEditor->m_PopupEventType != POPEVENT_LARGELAYER && pEditor->m_PopupEventType != POPEVENT_PREVENTUNUSEDTILES && pEditor->m_PopupEventType != POPEVENT_IMAGEDIV16 && pEditor->m_PopupEventType != POPEVENT_IMAGE_MAX)
	{
		static int s_CancelButton = 0;
		if(pEditor->DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, nullptr))
		{
			pEditor->m_PopupEventWasActivated = false;
			return CUI::POPUP_CLOSE_CURRENT;
		}
	}

	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Button);
	static int s_ConfirmButton = 0;
	if(pEditor->DoButton_Editor(&s_ConfirmButton, "Confirm", 0, &Button, 0, nullptr) || (Active && pEditor->UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		if(pEditor->m_PopupEventType == POPEVENT_EXIT)
		{
			g_Config.m_ClEditor = 0;
		}
		else if(pEditor->m_PopupEventType == POPEVENT_LOAD)
		{
			pEditor->InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", false, CEditor::CallbackOpenMap, pEditor);
		}
		else if(pEditor->m_PopupEventType == POPEVENT_LOADCURRENT)
		{
			pEditor->LoadCurrentMap();
		}
		else if(pEditor->m_PopupEventType == POPEVENT_NEW)
		{
			pEditor->Reset();
			pEditor->m_aFileName[0] = 0;
		}
		else if(pEditor->m_PopupEventType == POPEVENT_SAVE)
		{
			CallbackSaveMap(pEditor->m_aFileSaveName, IStorage::TYPE_SAVE, pEditor);
			return CUI::POPUP_CLOSE_CURRENT;
		}
		else if(pEditor->m_PopupEventType == POPEVENT_SAVE_COPY)
		{
			CallbackSaveCopyMap(pEditor->m_aFileSaveName, IStorage::TYPE_SAVE, pEditor);
			return CUI::POPUP_CLOSE_CURRENT;
		}
		else if(pEditor->m_PopupEventType == POPEVENT_PLACE_BORDER_TILES)
		{
			pEditor->PlaceBorderTiles();
		}
		pEditor->m_PopupEventWasActivated = false;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

static int g_SelectImageSelected = -100;
static int g_SelectImageCurrent = -100;

CUI::EPopupMenuFunctionResult CEditor::PopupSelectImage(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

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

	return CUI::POPUP_KEEP_OPEN;
}

void CEditor::PopupSelectImageInvoke(int Current, float x, float y)
{
	static SPopupMenuId s_PopupSelectImageId;
	g_SelectImageSelected = -100;
	g_SelectImageCurrent = Current;
	UI()->DoPopupMenu(&s_PopupSelectImageId, x, y, 450, 300, this, PopupSelectImage);
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

CUI::EPopupMenuFunctionResult CEditor::PopupSelectSound(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

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

	return CUI::POPUP_KEEP_OPEN;
}

void CEditor::PopupSelectSoundInvoke(int Current, float x, float y)
{
	static SPopupMenuId s_PopupSelectSoundId;
	g_SelectSoundSelected = -100;
	g_SelectSoundCurrent = Current;
	UI()->DoPopupMenu(&s_PopupSelectSoundId, x, y, 150, 300, this, PopupSelectSound);
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

static const char *s_apGametileOpButtonNames[] = {
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

CUI::EPopupMenuFunctionResult CEditor::PopupSelectGametileOp(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	const int PreviousSelected = s_GametileOpSelected;

	CUIRect Button;
	for(size_t i = 0; i < std::size(s_apGametileOpButtonNames); ++i)
	{
		View.HSplitTop(2.0f, nullptr, &View);
		View.HSplitTop(12.0f, &Button, &View);
		if(pEditor->DoButton_Editor(&s_apGametileOpButtonNames[i], s_apGametileOpButtonNames[i], 0, &Button, 0, nullptr))
			s_GametileOpSelected = i;
	}

	return s_GametileOpSelected == PreviousSelected ? CUI::POPUP_KEEP_OPEN : CUI::POPUP_CLOSE_CURRENT;
}

void CEditor::PopupSelectGametileOpInvoke(float x, float y)
{
	static SPopupMenuId s_PopupSelectGametileOpId;
	s_GametileOpSelected = -1;
	UI()->DoPopupMenu(&s_PopupSelectGametileOpId, x, y, 120.0f, std::size(s_apGametileOpButtonNames) * 14.0f + 10.0f, this, PopupSelectGametileOp);
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

CUI::EPopupMenuFunctionResult CEditor::PopupSelectConfigAutoMap(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(pEditor->GetSelectedLayer(0));
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

	return CUI::POPUP_KEEP_OPEN;
}

void CEditor::PopupSelectConfigAutoMapInvoke(int Current, float x, float y)
{
	static SPopupMenuId s_PopupSelectConfigAutoMapId;
	s_AutoMapConfigSelected = -100;
	s_AutoMapConfigCurrent = Current;
	std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayer(0));
	const int ItemCount = minimum(m_Map.m_vpImages[pLayer->m_Image]->m_AutoMapper.ConfigNamesNum(), 10);
	// Width for buttons is 120, 15 is the scrollbar width, 2 is the margin between both.
	UI()->DoPopupMenu(&s_PopupSelectConfigAutoMapId, x, y, 120.0f + 15.0f + 2.0f, 26.0f + 14.0f * ItemCount, this, PopupSelectConfigAutoMap);
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

CUI::EPopupMenuFunctionResult CEditor::PopupTele(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	static int s_PreviousNumber = -1;

	CUIRect NumberPicker;
	CUIRect FindEmptySlot;

	View.VSplitRight(15.f, &NumberPicker, &FindEmptySlot);
	NumberPicker.VSplitRight(2.f, &NumberPicker, nullptr);
	FindEmptySlot.HSplitTop(13.0f, &FindEmptySlot, nullptr);
	FindEmptySlot.HMargin(1.0f, &FindEmptySlot);

	// find empty number button
	{
		static int s_EmptySlotPid = 0;
		if(pEditor->DoButton_Editor(&s_EmptySlotPid, "F", 0, &FindEmptySlot, 0, "[ctrl+f] Find empty slot") || (Active && pEditor->Input()->ModifierIsPressed() && pEditor->Input()->KeyPress(KEY_F)))
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
			pEditor->m_TeleNumber = (NewVal - 1 + 255) % 255 + 1;
		}

		if(s_PreviousNumber == 1 || s_PreviousNumber != pEditor->m_TeleNumber)
		{
			s_Color = pEditor->m_Map.m_pTeleLayer->ContainsElementWithId(pEditor->m_TeleNumber) ? ColorRGBA(1, 0.5f, 0.5f, 0.5f) : ColorRGBA(0.5f, 1, 0.5f, 0.5f);
		}
	}

	s_PreviousNumber = pEditor->m_TeleNumber;

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupSpeedup(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	enum
	{
		PROP_FORCE = 0,
		PROP_MAXSPEED,
		PROP_ANGLE,
		NUM_PROPS
	};

	CProperty aProps[] = {
		{"Force", pEditor->m_SpeedupForce, PROPTYPE_INT_STEP, 1, 255},
		{"Max Speed", pEditor->m_SpeedupMaxSpeed, PROPTYPE_INT_STEP, 0, 255},
		{"Angle", pEditor->m_SpeedupAngle, PROPTYPE_ANGLE_SCROLL, 0, 359},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal);

	if(Prop == PROP_FORCE)
	{
		pEditor->m_SpeedupForce = clamp(NewVal, 1, 255);
	}
	else if(Prop == PROP_MAXSPEED)
	{
		pEditor->m_SpeedupMaxSpeed = clamp(NewVal, 0, 255);
	}
	else if(Prop == PROP_ANGLE)
	{
		pEditor->m_SpeedupAngle = clamp(NewVal, 0, 359);
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupSwitch(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect NumberPicker, FindEmptySlot;

	View.VSplitRight(15.0f, &NumberPicker, &FindEmptySlot);
	NumberPicker.VSplitRight(2.0f, &NumberPicker, nullptr);
	FindEmptySlot.HSplitTop(13.0f, &FindEmptySlot, nullptr);
	FindEmptySlot.HMargin(1.0f, &FindEmptySlot);

	// find empty number button
	{
		static int s_EmptySlotPid = 0;
		if(pEditor->DoButton_Editor(&s_EmptySlotPid, "F", 0, &FindEmptySlot, 0, "[ctrl+f] Find empty slot") || (Active && pEditor->Input()->ModifierIsPressed() && pEditor->Input()->KeyPress(KEY_F)))
		{
			int Number = -1;
			for(int i = 1; i <= 255; i++)
			{
				if(!pEditor->m_Map.m_pSwitchLayer->ContainsElementWithId(i))
				{
					Number = i;
					break;
				}
			}

			if(Number != -1)
			{
				pEditor->m_SwitchNum = Number;
			}
		}
	}

	// number picker
	static int s_PreviousNumber = -1;
	{
		static ColorRGBA s_Color = ColorRGBA(1, 1, 1, 0.5f);

		enum
		{
			PROP_SWITCH_NUMBER = 0,
			PROP_SWITCH_DELAY,
			NUM_PROPS,
		};

		CProperty aProps[] = {
			{"Number", pEditor->m_SwitchNum, PROPTYPE_INT_STEP, 0, 255},
			{"Delay", pEditor->m_SwitchDelay, PROPTYPE_INT_STEP, 0, 255},
			{nullptr},
		};

		static int s_aIds[NUM_PROPS] = {0};
		int NewVal = 0;
		int Prop = pEditor->DoProperties(&NumberPicker, aProps, s_aIds, &NewVal, s_Color);

		if(Prop == PROP_SWITCH_NUMBER)
		{
			pEditor->m_SwitchNum = (NewVal + 256) % 256;
		}
		else if(Prop == PROP_SWITCH_DELAY)
		{
			pEditor->m_SwitchDelay = (NewVal + 256) % 256;
		}

		if(s_PreviousNumber == 1 || s_PreviousNumber != pEditor->m_SwitchNum)
		{
			s_Color = pEditor->m_Map.m_pSwitchLayer->ContainsElementWithId(pEditor->m_SwitchNum) ? ColorRGBA(1, 0.5f, 0.5f, 0.5f) : ColorRGBA(0.5f, 1, 0.5f, 0.5f);
		}
	}

	s_PreviousNumber = pEditor->m_SwitchNum;
	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupTune(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

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
	{
		pEditor->m_TuningNum = (NewVal - 1 + 255) % 255 + 1;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupGoto(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	enum
	{
		PROP_COORD_X = 0,
		PROP_COORD_Y,
		NUM_PROPS,
	};

	static ivec2 s_GotoPos(0, 0);

	CProperty aProps[] = {
		{"X", s_GotoPos.x, PROPTYPE_INT_STEP, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()},
		{"Y", s_GotoPos.y, PROPTYPE_INT_STEP, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(&View, aProps, s_aIds, &NewVal, ColorRGBA(1, 1, 1, 0.5f));

	if(Prop == PROP_COORD_X)
	{
		s_GotoPos.x = NewVal;
	}
	else if(Prop == PROP_COORD_Y)
	{
		s_GotoPos.y = NewVal;
	}

	CUIRect Button;
	View.HSplitBottom(12.0f, &View, &Button);

	static int s_Button;
	if(pEditor->DoButton_Editor(&s_Button, "Go", 0, &Button, 0, nullptr))
	{
		pEditor->MapView()->m_WorldOffset = vec2(32.0f * s_GotoPos.x + 0.5f, 32.0f * s_GotoPos.y + 0.5f);
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupEntities(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	for(size_t i = 0; i < pEditor->m_vSelectEntitiesFiles.size(); i++)
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

				if(pEditor->m_EntitiesTexture.IsValid())
					pEditor->Graphics()->UnloadTexture(&pEditor->m_EntitiesTexture);

				char aBuf[IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "editor/entities/%s.png", pName);
				pEditor->m_EntitiesTexture = pEditor->Graphics()->LoadTexture(aBuf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, pEditor->GetTextureUsageFlag());
				return CUI::POPUP_CLOSE_CURRENT;
			}
		}
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CEditor::PopupProofMode(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	CUIRect Button;
	View.HSplitTop(12.0f, &Button, &View);
	static int s_ButtonIngame;
	if(pEditor->DoButton_MenuItem(&s_ButtonIngame, "Ingame", pEditor->m_ProofBorders == PROOF_BORDER_INGAME, &Button, 0, "These borders represent what a player maximum can see."))
	{
		pEditor->m_ProofBorders = PROOF_BORDER_INGAME;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(12.0f, &Button, &View);
	static int s_ButtonMenu;
	if(pEditor->DoButton_MenuItem(&s_ButtonMenu, "Menu", pEditor->m_ProofBorders == PROOF_BORDER_MENU, &Button, 0, "These borders represent what will be shown in the menu."))
	{
		pEditor->m_ProofBorders = PROOF_BORDER_MENU;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}
