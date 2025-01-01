// This file can be included several times.

#ifndef REGISTER_QUICK_ACTION
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description)
#endif

#define ALWAYS_FALSE []() -> bool { return false; }
#define DEFAULT_BTN []() -> int { return -1; }

REGISTER_QUICK_ACTION(
	ToggleGrid,
	"Toggle grid",
	[&]() { MapView()->MapGrid()->Toggle(); },
	ALWAYS_FALSE,
	[&]() -> bool { return MapView()->MapGrid()->IsEnabled(); },
	DEFAULT_BTN,
	"[Ctrl+G] Toggle grid.")
REGISTER_QUICK_ACTION(
	GameTilesAir,
	"Game tiles: Air",
	[&]() { FillGameTiles(EGameTileOp::AIR); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesHookable,
	"Game tiles: Hookable",
	[&]() { FillGameTiles(EGameTileOp::HOOKABLE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeath,
	"Game tiles: Death",
	[&]() { FillGameTiles(EGameTileOp::DEATH); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesUnhookable,
	"Game tiles: Unhookable",
	[&]() { FillGameTiles(EGameTileOp::UNHOOKABLE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesHookthrough,
	"Game tiles: Hookthrough",
	[&]() { FillGameTiles(EGameTileOp::HOOKTHROUGH); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesFreeze,
	"Game tiles: Freeze",
	[&]() { FillGameTiles(EGameTileOp::FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesUnfreeze,
	"Game tiles: Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeepFreeze,
	"Game tiles: Deep Freeze",
	[&]() { FillGameTiles(EGameTileOp::DEEP_FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeepUnfreeze,
	"Game tiles: Deep Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::DEEP_UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesBlueCheckTele,
	"Game tiles: Blue Check Tele",
	[&]() { FillGameTiles(EGameTileOp::BLUE_CHECK_TELE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesRedCheckTele,
	"Game tiles: Red Check Tele",
	[&]() { FillGameTiles(EGameTileOp::RED_CHECK_TELE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesLiveFreeze,
	"Game tiles: Live Freeze",
	[&]() { FillGameTiles(EGameTileOp::LIVE_FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesLiveUnfreeze,
	"Game tiles: Live Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::LIVE_UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Construct game tiles from this layer.")
REGISTER_QUICK_ACTION(
	AddGroup,
	"Add group",
	[&]() { AddGroup(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Add a new group.")
REGISTER_QUICK_ACTION(
	ResetZoom,
	"Reset zoom",
	[&]() { MapView()->ResetZoom(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Numpad*] Zoom to normal and remove editor offset.")
REGISTER_QUICK_ACTION(
	ZoomOut,
	"Zoom out",
	[&]() { MapView()->Zoom()->ChangeValue(50.0f); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Numpad-] Zoom out.")
REGISTER_QUICK_ACTION(
	ZoomIn,
	"Zoom in",
	[&]() { MapView()->Zoom()->ChangeValue(-50.0f); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Numpad+] Zoom in.")
REGISTER_QUICK_ACTION(
	Refocus,
	"Refocus",
	[&]() { MapView()->Focus(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Home] Restore map focus.")
REGISTER_QUICK_ACTION(
	Proof,
	"Proof",
	[&]() { MapView()->ProofMode()->Toggle(); },
	ALWAYS_FALSE,
	[&]() -> bool { return MapView()->ProofMode()->IsEnabled(); },
	DEFAULT_BTN,
	"Toggle proof borders. These borders represent the area that a player can see with default zoom.")
REGISTER_QUICK_ACTION(
	AddTileLayer, "Add tile layer", [&]() { AddTileLayer(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "Create a new tile layer.")
REGISTER_QUICK_ACTION(
	AddSwitchLayer,
	"Add switch layer",
	[&]() { AddSwitchLayer(); },
	[&]() -> bool { return !GetSelectedGroup()->m_GameGroup || m_Map.m_pSwitchLayer; },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Create a new switch layer.")
REGISTER_QUICK_ACTION(
	AddTuneLayer,
	"Add tune layer",
	[&]() { AddTuneLayer(); },
	[&]() -> bool { return !GetSelectedGroup()->m_GameGroup || m_Map.m_pTuneLayer; },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Create a new tuning layer.")
REGISTER_QUICK_ACTION(
	AddSpeedupLayer,
	"Add speedup layer",
	[&]() { AddSpeedupLayer(); },
	[&]() -> bool { return !GetSelectedGroup()->m_GameGroup || m_Map.m_pSpeedupLayer; },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Create a new speedup layer.")
REGISTER_QUICK_ACTION(
	AddTeleLayer,
	"Add tele layer",
	[&]() { AddTeleLayer(); },
	[&]() -> bool { return !GetSelectedGroup()->m_GameGroup || m_Map.m_pTeleLayer; },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Create a new tele layer.")
REGISTER_QUICK_ACTION(
	AddFrontLayer,
	"Add front layer",
	[&]() { AddFrontLayer(); },
	[&]() -> bool { return !GetSelectedGroup()->m_GameGroup || m_Map.m_pFrontLayer; },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Create a new item layer.")
REGISTER_QUICK_ACTION(
	AddQuadsLayer, "Add quads layer", [&]() { AddQuadsLayer(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "Create a new quads layer.")
REGISTER_QUICK_ACTION(
	AddSoundLayer, "Add sound layer", [&]() { AddSoundLayer(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "Create a new sound layer.")
REGISTER_QUICK_ACTION(
	SaveAs,
	"Save as",
	[&]() { InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save as", "maps", true, CEditor::CallbackSaveMap, this); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Ctrl+Shift+S] Save the current map under a new name.")
REGISTER_QUICK_ACTION(
	LoadCurrentMap,
	"Load current map",
	[&]() {
		if(HasUnsavedData())
		{
			m_PopupEventType = POPEVENT_LOADCURRENT;
			m_PopupEventActivated = true;
		}
		else
		{
			LoadCurrentMap();
		}
	},
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Ctrl+Alt+L] Open the current ingame map for editing.")
REGISTER_QUICK_ACTION(
	Envelopes,
	"Envelopes",
	[&]() { m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES ? EXTRAEDITOR_NONE : EXTRAEDITOR_ENVELOPES; },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	[&]() -> int { return m_ShowPicker ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES; },
	"Toggle the envelope editor.")
REGISTER_QUICK_ACTION(
	ServerSettings,
	"Server settings",
	[&]() { m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS ? EXTRAEDITOR_NONE : EXTRAEDITOR_SERVER_SETTINGS; },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	[&]() -> int { return m_ShowPicker ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_SERVER_SETTINGS; },
	"Toggle the server settings editor.")
REGISTER_QUICK_ACTION(
	History,
	"History",
	[&]() { m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_HISTORY ? EXTRAEDITOR_NONE : EXTRAEDITOR_HISTORY; },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	[&]() -> int { return m_ShowPicker ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_HISTORY; },
	"Toggle the editor history view.")
REGISTER_QUICK_ACTION(
	AddImage,
	"Add image",
	[&]() { InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", false, AddImage, this); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Load a new image to use in the map.")
REGISTER_QUICK_ACTION(
	LayerPropAddImage,
	"Layer: add image",
	[&]() { LayerSelectImage(); },
	[&]() -> bool { return !IsNonGameTileLayerSelected(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Pick mapres image for currently selected layer.")
REGISTER_QUICK_ACTION(
	ShowInfoOff,
	"Show info: Off",
	[&]() {
		m_ShowTileInfo = SHOW_TILE_OFF;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	},
	ALWAYS_FALSE,
	[&]() -> bool { return m_ShowTileInfo == SHOW_TILE_OFF; },
	DEFAULT_BTN,
	"Do not show tile information.")
REGISTER_QUICK_ACTION(
	ShowInfoDec,
	"Show info: Dec",
	[&]() {
		m_ShowTileInfo = SHOW_TILE_DECIMAL;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	},
	ALWAYS_FALSE,
	[&]() -> bool { return m_ShowTileInfo == SHOW_TILE_DECIMAL; },
	DEFAULT_BTN,
	"[Ctrl+I] Show tile information.")
REGISTER_QUICK_ACTION(
	ShowInfoHex,
	"Show info: Hex",
	[&]() {
		m_ShowTileInfo = SHOW_TILE_HEXADECIMAL;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	},
	ALWAYS_FALSE,
	[&]() -> bool { return m_ShowTileInfo == SHOW_TILE_HEXADECIMAL; },
	DEFAULT_BTN,
	"[Ctrl+Shift+I] Show tile information in hexadecimal.")
REGISTER_QUICK_ACTION(
	DeleteLayer,
	"Delete layer",
	[&]() { DeleteSelectedLayer(); },
	[&]() -> bool {
		std::shared_ptr<CLayer> pCurrentLayer = GetSelectedLayer(0);
		if(!pCurrentLayer)
			return true;
		return m_Map.m_pGameLayer == pCurrentLayer;
	},
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Delete the layer.")
REGISTER_QUICK_ACTION(
	Pipette,
	"Pipette",
	[&]() { m_ColorPipetteActive = !m_ColorPipetteActive; },
	ALWAYS_FALSE,
	[&]() -> bool { return m_ColorPipetteActive; },
	DEFAULT_BTN,
	"[Ctrl+Shift+C] Color pipette. Pick a color from the screen by clicking on it.")
REGISTER_QUICK_ACTION(
	MapDetails,
	"Map details",
	[&]() { MapDetails(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Adjust the map details of the current map.")
REGISTER_QUICK_ACTION(
	AddQuad,
	"Add quad",
	[&]() { AddQuadOrSound(); },
	[&]() -> bool {
		std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
		if(!pLayer)
			return false;
		return pLayer->m_Type != LAYERTYPE_QUADS;
	},
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Ctrl+Q] Add a new quad.")
REGISTER_QUICK_ACTION(
	AddSoundSource,
	"Add sound source",
	[&]() { AddQuadOrSound(); },
	[&]() -> bool {
		std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
		if(!pLayer)
			return false;
		return pLayer->m_Type != LAYERTYPE_SOUNDS;
	},
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Ctrl+Q] Add a new sound source.")

#undef ALWAYS_FALSE
#undef DEFAULT_BTN
