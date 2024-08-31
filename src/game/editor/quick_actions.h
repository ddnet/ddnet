// This file can be included several times.

#ifndef REGISTER_QUICK_ACTION
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description)
#endif

#define ALWAYS_FALSE []() -> bool { return false; }
#define DEFAULT_BTN []() -> int { return -1; }

REGISTER_QUICK_ACTION(
	AddGroup, "Add group", [&]() { AddGroup(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "Adds a new group")
REGISTER_QUICK_ACTION(
	Refocus, "Refocus", [&]() { MapView()->Focus(); }, ALWAYS_FALSE, ALWAYS_FALSE, DEFAULT_BTN, "[HOME] Restore map focus")
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
	"Saves the current map under a new name (ctrl+shift+s)")
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
	"Load a new image to use in the map")
REGISTER_QUICK_ACTION(
	LayerPropAddImage,
	"Layer: Add Image",
	[&]() { LayerSelectImage(); },
	[&]() -> bool { return !IsNonGameTileLayerSelected(); },
	ALWAYS_FALSE,
	DEFAULT_BTN,
	"Pick mapres image for currently selected layer")

#undef ALWAYS_FALSE
#undef DEFAULT_BTN
