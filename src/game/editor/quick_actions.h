// This file can be included several times.

#ifndef REGISTER_QUICK_ACTION
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description)
#endif

#define ALWAYS_FALSE []() -> bool { return false; }
#define DEFAULT_BTN []() -> int { return -1; }

REGISTER_QUICK_ACTION(
	ToggleGrid,
	"Toggle Grid",
	[&]() { MapView()->MapGrid()->Toggle(); },
	ALWAYS_FALSE,
	[&]() -> bool { return MapView()->MapGrid()->IsEnabled(); },
	DEFAULT_BTN,
	"[Ctrl+G] Toggle Grid.")
REGISTER_QUICK_ACTION(
	GameTilesAir,
	"Game tiles: Air",
	[&]() { FillGameTiles(EGameTileOp::AIR); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesHookable,
	"Game tiles: Hookable",
	[&]() { FillGameTiles(EGameTileOp::HOOKABLE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeath,
	"Game tiles: Death",
	[&]() { FillGameTiles(EGameTileOp::DEATH); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesUnhookable,
	"Game tiles: Unhookable",
	[&]() { FillGameTiles(EGameTileOp::UNHOOKABLE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesHookthrough,
	"Game tiles: Hookthrough",
	[&]() { FillGameTiles(EGameTileOp::HOOKTHROUGH); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesFreeze,
	"Game tiles: Freeze",
	[&]() { FillGameTiles(EGameTileOp::FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesUnfreeze,
	"Game tiles: Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeepFreeze,
	"Game tiles: Deep Freeze",
	[&]() { FillGameTiles(EGameTileOp::DEEP_FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesDeepUnfreeze,
	"Game tiles: Deep Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::DEEP_UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesBlueCheckTele,
	"Game tiles: Blue Check Tele",
	[&]() { FillGameTiles(EGameTileOp::BLUE_CHECK_TELE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesRedCheckTele,
	"Game tiles: Red Check Tele",
	[&]() { FillGameTiles(EGameTileOp::RED_CHECK_TELE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesLiveFreeze,
	"Game tiles: Live Freeze",
	[&]() { FillGameTiles(EGameTileOp::LIVE_FREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	GameTilesLiveUnfreeze,
	"Game tiles: Live Unfreeze",
	[&]() { FillGameTiles(EGameTileOp::LIVE_UNFREEZE); },
	[&]() -> bool { return !CanFillGameTiles(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Constructs game tiles from this layer.")
REGISTER_QUICK_ACTION(
	AddGroup,
	"Add group",
	[&]() { AddGroup(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Adds a new group.")
REGISTER_QUICK_ACTION(
	Refocus,
	"Refocus",
	[&]() { MapView()->Focus(); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[HOME] Restore map focus.")
REGISTER_QUICK_ACTION(
	Proof,
	"Proof",
	[&]() { MapView()->ProofMode()->Toggle(); },
	ALWAYS_FALSE,
	[&]() -> bool { return MapView()->ProofMode()->IsEnabled(); },
	DEFAULT_BTN,
	"Toggles proof borders. These borders represent the area that a player can see with default zoom.")
REGISTER_QUICK_ACTION(
	AddTileLayer, "Add tile layer", [&]() { AddTileLayer(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "Creates a new tile layer.")
REGISTER_QUICK_ACTION(
	SaveAs,
	"Save As",
	[&]() { InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save As", "maps", true, CEditor::CallbackSaveMap, this); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"[Ctrl+Shift+S] Saves the current map under a new name.")
REGISTER_QUICK_ACTION(
	LoadCurrentMap,
	"Load Current Map",
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
	"[Ctrl+Alt+L] Opens the current in game map for editing.")
REGISTER_QUICK_ACTION(
	Envelopes,
	"Envelopes",
	[&]() { m_ActiveExtraEditor = m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES ? EXTRAEDITOR_NONE : EXTRAEDITOR_ENVELOPES; },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	[&]() -> int { return m_ShowPicker ? -1 : m_ActiveExtraEditor == EXTRAEDITOR_ENVELOPES; },
	"Toggles the envelope editor.")
REGISTER_QUICK_ACTION(
	AddImage,
	"Add Image",
	[&]() { InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", false, AddImage, this); },
	ALWAYS_FALSE,
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Load a new image to use in the map.")
REGISTER_QUICK_ACTION(
	LayerPropAddImage,
	"Layer: Add Image",
	[&]() { LayerSelectImage(); },
	[&]() -> bool { return !IsNonGameTileLayerSelected(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Pick mapres image for currently selected layer.")
REGISTER_QUICK_ACTION(
	ShowInfoOff,
	"Show Info: Off",
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
	"Show Info: Dec",
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
	"Show Info: Hex",
	[&]() {
		m_ShowTileInfo = SHOW_TILE_HEXADECIMAL;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	},
	ALWAYS_FALSE,
	[&]() -> bool { return m_ShowTileInfo == SHOW_TILE_HEXADECIMAL; },
	DEFAULT_BTN,
	"[Ctrl+Shift+I] Show tile information in hexadecimal.")

#undef ALWAYS_FALSE
#undef DEFAULT_BTN
