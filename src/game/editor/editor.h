/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_EDITOR_H
#define GAME_EDITOR_EDITOR_H

#include <base/bezier.h>
#include <base/system.h>

#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/mapitems.h>

#include <game/editor/enums.h>
#include <game/editor/file_browser.h>
#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer.h>
#include <game/editor/mapitems/layer_front.h>
#include <game/editor/mapitems/layer_game.h>
#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_speedup.h>
#include <game/editor/mapitems/layer_switch.h>
#include <game/editor/mapitems/layer_tele.h>
#include <game/editor/mapitems/layer_tiles.h>
#include <game/editor/mapitems/layer_tune.h>
#include <game/editor/mapitems/map.h>

#include <game/map/render_map.h>

#include <engine/console.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include "editor_history.h"
#include "editor_server_settings.h"
#include "editor_trackers.h"
#include "editor_ui.h"
#include "font_typer.h"
#include "layer_selector.h"
#include "map_view.h"
#include "quadart.h"
#include "smooth_value.h"
#include <game/editor/prompt.h>
#include <game/editor/quick_action.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

template<typename T>
using FDropdownRenderCallback = std::function<void(const T &, char (&aOutput)[128], std::vector<STextColorSplit> &)>;

// CEditor SPECIFIC
enum
{
	MODE_LAYERS = 0,
	MODE_IMAGES,
	MODE_SOUNDS,
	NUM_MODES,

	DIALOG_NONE = 0,
	DIALOG_FILE,
	DIALOG_MAPSETTINGS_ERROR,
	DIALOG_QUICK_PROMPT,

	// The font typer component sets m_Dialog
	// while it is active to make sure no other component
	// interprets the key presses
	DIALOG_PSEUDO_FONT_TYPER,
};

class CProperty
{
public:
	CProperty(const char *pName, int Value, int Type, int Min, int Max) :
		m_pName(pName), m_Value(Value), m_Type(Type), m_Min(Min), m_Max(Max) {}

	CProperty(std::nullptr_t) :
		m_pName(nullptr), m_Value(0), m_Type(0), m_Min(0), m_Max(0) {}

	const char *m_pName;
	int m_Value;
	int m_Type;
	int m_Min;
	int m_Max;
};

enum
{
	PROPTYPE_NULL = 0,
	PROPTYPE_BOOL,
	PROPTYPE_INT,
	PROPTYPE_ANGLE_SCROLL,
	PROPTYPE_COLOR,
	PROPTYPE_IMAGE,
	PROPTYPE_ENVELOPE,
	PROPTYPE_SHIFT,
	PROPTYPE_SOUND,
	PROPTYPE_AUTOMAPPER,
	PROPTYPE_AUTOMAPPER_REFERENCE,
};

class CEditor : public IEditor
{
	class IInput *m_pInput = nullptr;
	class IClient *m_pClient = nullptr;
	class IConfigManager *m_pConfigManager = nullptr;
	class CConfig *m_pConfig = nullptr;
	class IConsole *m_pConsole = nullptr;
	class IEngine *m_pEngine = nullptr;
	class IGraphics *m_pGraphics = nullptr;
	class ITextRender *m_pTextRender = nullptr;
	class ISound *m_pSound = nullptr;
	class IStorage *m_pStorage = nullptr;
	CRenderMap m_RenderMap;
	CUi m_UI;

	std::vector<std::reference_wrapper<CEditorComponent>> m_vComponents;
	CMapView m_MapView;
	CLayerSelector m_LayerSelector;
	CFileBrowser m_FileBrowser;
	CPrompt m_Prompt;
	CFontTyper m_FontTyper;

	bool m_EditorWasUsedBefore = false;

	IGraphics::CTextureHandle m_EntitiesTexture;

	IGraphics::CTextureHandle m_FrontTexture;
	IGraphics::CTextureHandle m_TeleTexture;
	IGraphics::CTextureHandle m_SpeedupTexture;
	IGraphics::CTextureHandle m_SwitchTexture;
	IGraphics::CTextureHandle m_TuneTexture;

	int GetTextureUsageFlag() const;

	enum EPreviewState
	{
		PREVIEW_UNLOADED,
		PREVIEW_LOADED,
		PREVIEW_ERROR,
	};

	std::shared_ptr<CLayerGroup> m_apSavedBrushes[10];
	static inline constexpr ColorRGBA ms_DefaultPropColor = ColorRGBA(1, 1, 1, 0.5f);

public:
	class IInput *Input() const { return m_pInput; }
	class IClient *Client() const { return m_pClient; }
	class IConfigManager *ConfigManager() const { return m_pConfigManager; }
	class CConfig *Config() const { return m_pConfig; }
	class IConsole *Console() const { return m_pConsole; }
	class IEngine *Engine() const { return m_pEngine; }
	class IGraphics *Graphics() const { return m_pGraphics; }
	class ISound *Sound() const { return m_pSound; }
	class ITextRender *TextRender() const { return m_pTextRender; }
	class IStorage *Storage() const { return m_pStorage; }
	CUi *Ui() { return &m_UI; }
	CRenderMap *RenderMap() { return &m_RenderMap; }

	CMapView *MapView() { return &m_MapView; }
	const CMapView *MapView() const { return &m_MapView; }
	CLayerSelector *LayerSelector() { return &m_LayerSelector; }

	void SelectNextLayer();
	void SelectPreviousLayer();

	void FillGameTiles(EGameTileOp FillTile) const;
	bool CanFillGameTiles() const;
	void AddQuadOrSound();
	void AddGroup();
	void AddSoundLayer();
	void AddTileLayer();
	void AddQuadsLayer();
	void AddSwitchLayer();
	void AddFrontLayer();
	void AddTuneLayer();
	void AddSpeedupLayer();
	void AddTeleLayer();
	void DeleteSelectedLayer();
	void LayerSelectImage();
	bool IsNonGameTileLayerSelected() const;
	void MapDetails();
	void TestMapLocally();
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description) CQuickAction m_QuickAction##name;
#include <game/editor/quick_actions.h>
#undef REGISTER_QUICK_ACTION

	CEditor() :
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description) m_QuickAction##name(text, description, callback, disabled, active, button_color),
#include <game/editor/quick_actions.h>
#undef REGISTER_QUICK_ACTION
		m_ZoomEnvelopeX(1.0f, 0.1f, 600.0f),
		m_ZoomEnvelopeY(640.0f, 0.1f, 32000.0f),
		m_Map(this),
		m_MapSettingsCommandContext(m_MapSettingsBackend.NewContext(&m_SettingsCommandInput))
	{
		m_EntitiesTexture.Invalidate();
		m_FrontTexture.Invalidate();
		m_TeleTexture.Invalidate();
		m_SpeedupTexture.Invalidate();
		m_SwitchTexture.Invalidate();
		m_TuneTexture.Invalidate();

		m_Mode = MODE_LAYERS;
		m_Dialog = 0;

		m_BrushColorEnabled = true;

		m_aFileName[0] = '\0';
		m_aFileNamePending[0] = '\0';
		m_ValidSaveFilename = false;

		m_PopupEventActivated = false;
		m_PopupEventWasActivated = false;

		m_ToolbarPreviewSound = -1;

		m_SelectEntitiesImage = "DDNet";

		m_ResetZoomEnvelope = true;
		m_OffsetEnvelopeX = 0.1f;
		m_OffsetEnvelopeY = 0.5f;

		m_ShowMousePointer = true;

		m_GuiActive = true;
		m_PreviewZoom = false;

		m_ShowTileInfo = SHOW_TILE_OFF;
		m_ShowDetail = true;
		m_Animate = false;
		m_AnimateStart = 0;
		m_AnimateTime = 0;
		m_AnimateSpeed = 1;
		m_AnimateUpdatePopup = false;

		m_ShowEnvelopePreview = SHOWENV_NONE;
		m_SelectedQuadEnvelope = -1;

		m_vSelectedEnvelopePoints = {};
		m_UpdateEnvPointInfo = false;
		m_SelectedTangentInPoint = std::pair(-1, -1);
		m_SelectedTangentOutPoint = std::pair(-1, -1);
		m_CurrentQuadIndex = -1;

		m_QuadKnifeActive = false;
		m_QuadKnifeCount = 0;
		mem_zero(m_aQuadKnifePoints, sizeof(m_aQuadKnifePoints));

		for(size_t i = 0; i < std::size(m_aSavedColors); ++i)
		{
			m_aSavedColors[i] = color_cast<ColorRGBA>(ColorHSLA(i / (float)std::size(m_aSavedColors), 1.0f, 0.5f));
		}

		m_CheckerTexture.Invalidate();
		for(auto &CursorTexture : m_aCursorTextures)
			CursorTexture.Invalidate();

		m_CursorType = CURSOR_NORMAL;

		// DDRace

		m_TeleNumber = 1;
		m_TeleCheckpointNumber = 1;
		m_ViewTeleNumber = 0;

		m_TuningNum = 1;
		m_ViewTuning = 0;

		m_SwitchNum = 1;
		m_SwitchDelay = 0;
		m_SpeedupForce = 50;
		m_SpeedupMaxSpeed = 0;
		m_SpeedupAngle = 0;
		m_LargeLayerWasWarned = false;
		m_PreventUnusedTilesWasWarned = false;
		m_AllowPlaceUnusedTiles = EUnusedEntities::NOT_ALLOWED;
		m_BrushDrawDestructive = true;
	}

	class CHoverTile
	{
	public:
		CHoverTile(int Group, int Layer, int x, int y, const CTile Tile) :
			m_Group(Group),
			m_Layer(Layer),
			m_X(x),
			m_Y(y),
			m_Tile(Tile)
		{
		}

		int m_Group;
		int m_Layer;
		int m_X;
		int m_Y;
		const CTile m_Tile;
	};
	std::vector<CHoverTile> m_vHoverTiles;
	const std::vector<CHoverTile> &HoverTiles() const { return m_vHoverTiles; }

	void Init() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnActivate() override;
	void OnWindowResize() override;
	void OnClose() override;
	void OnDialogClose();
	bool HasUnsavedData() const override { return m_Map.m_Modified; }
	void UpdateMentions() override { m_Mentions++; }
	void ResetMentions() override { m_Mentions = 0; }
	void OnIngameMoved() override { m_IngameMoved = true; }
	void ResetIngameMoved() override { m_IngameMoved = false; }

	void HandleCursorMovement();
	void OnMouseMove(vec2 MousePos);
	void MouseAxisLock(vec2 &CursorRel);
	vec2 m_MouseAxisInitialPos = vec2(0.0f, 0.0f);
	enum class EAxisLock
	{
		Start,
		None,
		Horizontal,
		Vertical
	} m_MouseAxisLockState = EAxisLock::Start;
	void HandleAutosave();
	bool PerformAutosave();
	void HandleWriterFinishJobs();

	// TODO: The name of the ShowFileDialogError function is not accurate anymore, this is used for generic error messages.
	//       Popups in UI should be shared_ptrs to make this even more generic.
	struct SStringKeyComparator
	{
		bool operator()(const char *pLhs, const char *pRhs) const
		{
			return str_comp(pLhs, pRhs) < 0;
		}
	};
	std::map<const char *, CUi::SMessagePopupContext *, SStringKeyComparator> m_PopupMessageContexts;
	[[gnu::format(printf, 2, 3)]] void ShowFileDialogError(const char *pFormat, ...);

	void Reset(bool CreateDefault = true);
	bool Save(const char *pFilename) override;
	bool Load(const char *pFilename, int StorageType) override;
	bool HandleMapDrop(const char *pFilename, int StorageType) override;
	bool Append(const char *pFilename, int StorageType, bool IgnoreHistory = false);
	void LoadCurrentMap();
	void Render();

	void RenderPressedKeys(CUIRect View);
	void RenderSavingIndicator(CUIRect View);
	void FreeDynamicPopupMenus();
	void UpdateColorPipette();
	void RenderMousePointer();
	void RenderGameEntities(const std::shared_ptr<CLayerTiles> &pTiles);
	void RenderSwitchEntities(const std::shared_ptr<CLayerTiles> &pTiles);

	std::vector<CQuad *> GetSelectedQuads();
	std::shared_ptr<CLayer> GetSelectedLayerType(int Index, int Type) const;
	std::shared_ptr<CLayer> GetSelectedLayer(int Index) const;
	std::shared_ptr<CLayerGroup> GetSelectedGroup() const;
	CSoundSource *GetSelectedSource() const;
	void SelectLayer(int LayerIndex, int GroupIndex = -1);
	void AddSelectedLayer(int LayerIndex);
	void SelectQuad(int Index);
	void ToggleSelectQuad(int Index);
	void DeselectQuads();
	void DeselectQuadPoints();
	void SelectQuadPoint(int QuadIndex, int Index);
	void ToggleSelectQuadPoint(int QuadIndex, int Index);
	void DeleteSelectedQuads();
	bool IsQuadSelected(int Index) const;
	bool IsQuadCornerSelected(int Index) const;
	bool IsQuadPointSelected(int QuadIndex, int Index) const;
	int FindSelectedQuadIndex(int Index) const;

	int FindEnvPointIndex(int Index, int Channel) const;
	void SelectEnvPoint(int Index);
	void SelectEnvPoint(int Index, int Channel);
	void ToggleEnvPoint(int Index, int Channel);
	bool IsEnvPointSelected(int Index, int Channel) const;
	bool IsEnvPointSelected(int Index) const;
	void DeselectEnvPoints();
	void SelectTangentOutPoint(int Index, int Channel);
	bool IsTangentOutPointSelected(int Index, int Channel) const;
	void SelectTangentInPoint(int Index, int Channel);
	bool IsTangentInPointSelected(int Index, int Channel) const;
	bool IsTangentInSelected() const;
	bool IsTangentOutSelected() const;
	bool IsTangentSelected() const;
	std::pair<int, int> EnvGetSelectedTimeAndValue() const;

	template<typename E>
	SEditResult<E> DoPropertiesWithState(CUIRect *pToolbox, CProperty *pProps, int *pIds, int *pNewVal, const std::vector<ColorRGBA> &vColors = {});
	int DoProperties(CUIRect *pToolbox, CProperty *pProps, int *pIds, int *pNewVal, const std::vector<ColorRGBA> &vColors = {});

	CUi::SColorPickerPopupContext m_ColorPickerPopupContext;
	const void *m_pColorPickerPopupActiveId = nullptr;
	void DoColorPickerButton(const void *pId, const CUIRect *pRect, ColorRGBA Color, const std::function<void(ColorRGBA Color)> &SetColor);

	int m_Mode;
	int m_Dialog;
	char m_aTooltip[256] = "";

	bool m_BrushColorEnabled;

	char m_aFileName[IO_MAX_PATH_LENGTH];
	char m_aFileNamePending[IO_MAX_PATH_LENGTH];
	bool m_ValidSaveFilename;

	enum
	{
		POPEVENT_EXIT = 0,
		POPEVENT_LOAD,
		POPEVENT_LOADCURRENT,
		POPEVENT_LOADDROP,
		POPEVENT_NEW,
		POPEVENT_LARGELAYER,
		POPEVENT_PREVENTUNUSEDTILES,
		POPEVENT_IMAGEDIV16,
		POPEVENT_IMAGE_MAX,
		POPEVENT_SOUND_MAX,
		POPEVENT_PLACE_BORDER_TILES,
		POPEVENT_TILEART_BIG_IMAGE,
		POPEVENT_TILEART_MANY_COLORS,
		POPEVENT_TILEART_TOO_MANY_COLORS,
		POPEVENT_QUADART_BIG_IMAGE,
		POPEVENT_REMOVE_USED_IMAGE,
		POPEVENT_REMOVE_USED_SOUND,
		POPEVENT_RESTART_SERVER,
		POPEVENT_RESTARTING_SERVER,
	};

	int m_PopupEventType;
	int m_PopupEventActivated;
	int m_PopupEventWasActivated;
	bool m_LargeLayerWasWarned;
	bool m_PreventUnusedTilesWasWarned;

	enum class EUnusedEntities
	{
		ALLOWED_IMPLICIT = -1,
		NOT_ALLOWED = 0,
		ALLOWED_EXPLICIT = 1,
	};
	EUnusedEntities m_AllowPlaceUnusedTiles;
	bool IsAllowPlaceUnusedTiles() const;

	bool m_BrushDrawDestructive;

	int m_Mentions = 0;
	bool m_IngameMoved = false;

	int m_ToolbarPreviewSound;

	std::vector<std::string> m_vSelectEntitiesFiles;
	std::string m_SelectEntitiesImage;

	// Zooming
	CSmoothValue m_ZoomEnvelopeX;
	CSmoothValue m_ZoomEnvelopeY;

	bool m_ResetZoomEnvelope;

	float m_OffsetEnvelopeX;
	float m_OffsetEnvelopeY;

	bool m_ShowMousePointer;
	bool m_GuiActive;

	bool m_PreviewZoom;
	float m_MouseWorldScale = 1.0f; // Mouse (i.e. UI) scale relative to the World (selected Group)
	vec2 m_MouseWorldPos = vec2(0.0f, 0.0f);
	vec2 m_MouseWorldNoParaPos = vec2(0.0f, 0.0f);
	vec2 m_MouseDeltaWorld = vec2(0.0f, 0.0f);
	const void *m_pContainerPanned;
	const void *m_pContainerPannedLast;
	char m_MapEditorId; // UI element ID for the main map editor

	enum EShowTile
	{
		SHOW_TILE_OFF,
		SHOW_TILE_DECIMAL,
		SHOW_TILE_HEXADECIMAL
	};
	EShowTile m_ShowTileInfo;
	bool m_ShowDetail;

	bool m_Animate;
	int64_t m_AnimateStart;
	float m_AnimateTime;
	float m_AnimateSpeed;
	bool m_AnimateUpdatePopup;

	enum EExtraEditor
	{
		EXTRAEDITOR_NONE = -1,
		EXTRAEDITOR_ENVELOPES,
		EXTRAEDITOR_SERVER_SETTINGS,
		EXTRAEDITOR_HISTORY,
		NUM_EXTRAEDITORS,
	};
	EExtraEditor m_ActiveExtraEditor = EXTRAEDITOR_NONE;
	float m_aExtraEditorSplits[NUM_EXTRAEDITORS] = {250.0f, 250.0f, 250.0f};
	float m_ToolBoxWidth = 100.0f;

	enum EShowEnvelope
	{
		SHOWENV_NONE = 0,
		SHOWENV_SELECTED,
		SHOWENV_ALL
	};
	EShowEnvelope m_ShowEnvelopePreview;
	bool m_ShowPicker;

	std::vector<int> m_vSelectedLayers;
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoint;
	int m_SelectedQuadIndex;
	int m_SelectedGroup;
	int m_SelectedQuadPoints;
	int m_SelectedEnvelope;
	std::vector<std::pair<int, int>> m_vSelectedEnvelopePoints;
	int m_SelectedQuadEnvelope;
	int m_CurrentQuadIndex;
	int m_SelectedImage;
	int m_SelectedSound;
	int m_SelectedSource;
	std::pair<int, int> m_SelectedTangentInPoint;
	std::pair<int, int> m_SelectedTangentOutPoint;
	bool m_UpdateEnvPointInfo;

	bool m_QuadKnifeActive;
	int m_QuadKnifeCount;
	vec2 m_aQuadKnifePoints[4];

	// Color palette and pipette
	ColorRGBA m_aSavedColors[8];
	ColorRGBA m_PipetteColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	bool m_ColorPipetteActive = false;

	IGraphics::CTextureHandle m_CheckerTexture;

	enum ECursorType
	{
		CURSOR_NORMAL,
		CURSOR_RESIZE_V,
		CURSOR_RESIZE_H,
		NUM_CURSORS
	};
	IGraphics::CTextureHandle m_aCursorTextures[ECursorType::NUM_CURSORS];
	ECursorType m_CursorType;

	IGraphics::CTextureHandle GetEntitiesTexture();

	std::shared_ptr<CLayerGroup> m_pBrush;
	std::shared_ptr<CLayerTiles> m_pTilesetPicker;
	std::shared_ptr<CLayerQuads> m_pQuadsetPicker;

	const void *m_pUiGotContext = nullptr;

	CEditorMap m_Map;
	std::deque<std::shared_ptr<CDataFileWriterFinishJob>> m_WriterFinishJobs;

	int m_ShiftBy;

	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser);

	CLineInputBuffered<256> m_SettingsCommandInput;
	CMapSettingsBackend m_MapSettingsBackend;
	CMapSettingsBackend::CContext m_MapSettingsCommandContext;

	CImageInfo m_TileartImageInfo;
	void AddTileart(bool IgnoreHistory = false);
	char m_aTileartFilename[IO_MAX_PATH_LENGTH];
	void TileartCheckColors();

	CImageInfo m_QuadArtImageInfo;
	CQuadArtParameters m_QuadArtParameters;
	void AddQuadArt(bool IgnoreHistory = false);

	void PlaceBorderTiles();

	void UpdateTooltip(const void *pId, const CUIRect *pRect, const char *pToolTip);
	int DoButton_Editor_Common(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Editor(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Env(const void *pId, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA Color, int Corners);

	int DoButton_Ex(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = EditorFontSizes::MENU, int Align = TEXTALIGN_MC);
	int DoButton_FontIcon(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = 10.0f);
	int DoButton_MenuItem(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags = BUTTONFLAG_LEFT, const char *pToolTip = nullptr);

	int DoButton_DraggableEx(const void *pId, const char *pText, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted, int Flags, const char *pToolTip = nullptr, int Corners = IGraphics::CORNER_ALL, float FontSize = 10.0f);

	bool DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const char *pToolTip = nullptr, const std::vector<STextColorSplit> &vColorSplits = {});
	bool DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const char *pToolTip = nullptr, const std::vector<STextColorSplit> &vColorSplits = {});

	void DoMapSettingsEditBox(CMapSettingsBackend::CContext *pContext, const CUIRect *pRect, float FontSize, float DropdownMaxHeight, int Corners = IGraphics::CORNER_ALL, const char *pToolTip = nullptr);

	template<typename T>
	int DoEditBoxDropdown(SEditBoxDropdownContext *pDropdown, CLineInput *pLineInput, const CUIRect *pEditBoxRect, int x, float MaxHeight, bool AutoWidth, const std::vector<T> &vData, const FDropdownRenderCallback<T> &pfnMatchCallback);
	template<typename T>
	int RenderEditBoxDropdown(SEditBoxDropdownContext *pDropdown, CUIRect View, CLineInput *pLineInput, int x, float MaxHeight, bool AutoWidth, const std::vector<T> &vData, const FDropdownRenderCallback<T> &pfnMatchCallback);

	void RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness) const;

	SEditResult<int> UiDoValueSelector(void *pId, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree = false, bool IsHex = false, int Corners = IGraphics::CORNER_ALL, const ColorRGBA *pColor = nullptr, bool ShowValue = true);

	static CUi::EPopupMenuFunctionResult PopupMenuFile(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupMenuTools(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupMenuSettings(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupGroup(void *pContext, CUIRect View, bool Active);
	struct SLayerPopupContext : public SPopupMenuId
	{
		CEditor *m_pEditor;
		std::vector<std::shared_ptr<CLayerTiles>> m_vpLayers;
		std::vector<int> m_vLayerIndices;
		CLayerTiles::SCommonPropState m_CommonPropState;
	};
	static CUi::EPopupMenuFunctionResult PopupLayer(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupQuad(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSource(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupPoint(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupEnvPoint(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupEnvPointMulti(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupEnvPointCurveType(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupImage(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSound(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupMapInfo(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupEvent(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelectImage(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelectSound(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelectGametileOp(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelectConfigAutoMap(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelectAutoMapReference(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupTele(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSpeedup(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSwitch(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupTune(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupGoto(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupEntities(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupProofMode(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupAnimateSettings(void *pContext, CUIRect View, bool Active);
	int m_PopupEnvelopeSelectedPoint = -1;
	static CUi::EPopupMenuFunctionResult PopupEnvelopeCurvetype(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupQuadArt(void *pContext, CUIRect View, bool Active);

	static bool CallbackOpenMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackAppendMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackSaveMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackAddTileart(const char *pFilepath, int StorageType, void *pUser);
	static bool CallbackAddQuadArt(const char *pFilepath, int StorageType, void *pUser);
	static bool CallbackSaveImage(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackSaveSound(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackCustomEntities(const char *pFileName, int StorageType, void *pUser);

	void PopupSelectImageInvoke(int Current, float x, float y);
	int PopupSelectImageResult();

	void PopupSelectGametileOpInvoke(float x, float y);
	int PopupSelectGameTileOpResult();

	void PopupSelectConfigAutoMapInvoke(int Current, float x, float y);
	int PopupSelectConfigAutoMapResult();

	void PopupSelectSoundInvoke(int Current, float x, float y);
	int PopupSelectSoundResult();

	void PopupSelectAutoMapReferenceInvoke(int Current, float x, float y);
	int PopupSelectAutoMapReferenceResult();

	void DoQuadEnvelopes(const std::vector<CQuad> &vQuads, IGraphics::CTextureHandle Texture = IGraphics::CTextureHandle());
	void DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int pIndex);
	void DoQuadPoint(int LayerIndex, const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int v);
	void SetHotQuadPoint(const std::shared_ptr<CLayerQuads> &pLayer);

	float TriangleArea(vec2 A, vec2 B, vec2 C);
	bool IsInTriangle(vec2 Point, vec2 A, vec2 B, vec2 C);
	void DoQuadKnife(int QuadIndex);

	void DoSoundSource(int LayerIndex, CSoundSource *pSource, int Index);

	enum class EAxis
	{
		AXIS_NONE = 0,
		AXIS_X,
		AXIS_Y
	};
	struct SAxisAlignedBoundingBox
	{
		enum
		{
			POINT_TL = 0,
			POINT_TR,
			POINT_BL,
			POINT_BR,
			POINT_CENTER,
			NUM_POINTS
		};
		CPoint m_aPoints[NUM_POINTS];
	};
	void DoMapEditor(CUIRect View);
	void DoToolbarLayers(CUIRect Toolbar);
	void DoToolbarImages(CUIRect Toolbar);
	void DoToolbarSounds(CUIRect Toolbar);
	void DoQuad(int LayerIndex, const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int Index);
	void PreparePointDrag(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex);
	void DoPointDrag(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex, int OffsetX, int OffsetY);
	EAxis GetDragAxis(int OffsetX, int OffsetY) const;
	void DrawAxis(EAxis Axis, CPoint &OriginalPoint, CPoint &Point) const;
	void DrawAABB(const SAxisAlignedBoundingBox &AABB, int OffsetX = 0, int OffsetY = 0) const;
	ColorRGBA GetButtonColor(const void *pId, int Checked);

	// Alignment methods
	// These methods take `OffsetX` and `OffsetY` because the calculations are made with the original positions
	// of the quad(s), before we started dragging. This allows us to edit `OffsetX` and `OffsetY` based on the previously
	// calculated alignments.
	struct SAlignmentInfo
	{
		CPoint m_AlignedPoint; // The "aligned" point, which we want to align/snap to
		union
		{
			// The current changing value when aligned to this point. When aligning to a point on the X axis, then the X value is changing because
			// we aligned the Y values (X axis aligned => Y values are the same, Y axis aligned => X values are the same).
			int m_X;
			int m_Y;
		};
		EAxis m_Axis; // The axis we are aligning on
		int m_PointIndex; // The point index we are aligning
		int m_Diff; // Store the difference
	};
	void ComputePointAlignments(const std::shared_ptr<CLayerQuads> &pLayer, CQuad *pQuad, int QuadIndex, int PointIndex, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments, bool Append = false) const;
	void ComputePointsAlignments(const std::shared_ptr<CLayerQuads> &pLayer, bool Pivot, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments) const;
	void ComputeAABBAlignments(const std::shared_ptr<CLayerQuads> &pLayer, const SAxisAlignedBoundingBox &AABB, int OffsetX, int OffsetY, std::vector<SAlignmentInfo> &vAlignments) const;
	void DrawPointAlignments(const std::vector<SAlignmentInfo> &vAlignments, int OffsetX, int OffsetY) const;
	void QuadSelectionAABB(const std::shared_ptr<CLayerQuads> &pLayer, SAxisAlignedBoundingBox &OutAABB);
	void ApplyAlignments(const std::vector<SAlignmentInfo> &vAlignments, int &OffsetX, int &OffsetY);
	void ApplyAxisAlignment(int &OffsetX, int &OffsetY) const;

	bool ReplaceImage(const char *pFilename, int StorageType, bool CheckDuplicate);
	static bool ReplaceImageCallback(const char *pFilename, int StorageType, void *pUser);
	bool ReplaceSound(const char *pFileName, int StorageType, bool CheckDuplicate);
	static bool ReplaceSoundCallback(const char *pFileName, int StorageType, void *pUser);
	static bool AddImage(const char *pFilename, int StorageType, void *pUser);
	static bool AddSound(const char *pFileName, int StorageType, void *pUser);
	static bool IsAssetUsed(CFileBrowser::EFileType FileType, int Index, void *pUser);

	bool IsEnvelopeUsed(int EnvelopeIndex) const;
	void RemoveUnusedEnvelopes();

	static bool IsVanillaImage(const char *pImage);

	void RenderLayers(CUIRect LayersBox);
	void RenderImagesList(CUIRect Toolbox);
	void RenderSelectedImage(CUIRect View);
	void RenderSounds(CUIRect Toolbox);
	void RenderModebar(CUIRect View);
	void RenderStatusbar(CUIRect View, CUIRect *pTooltipRect);
	void RenderTooltip(CUIRect TooltipRect);

	void RenderEnvelopeEditor(CUIRect View);
	void RenderEnvelopeEditorColorBar(CUIRect ColorBar, const std::shared_ptr<CEnvelope> &pEnvelope);

	void RenderMapSettingsErrorDialog();
	void RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast);
	static void MapSettingsDropdownRenderCallback(const SPossibleValueMatch &Match, char (&aOutput)[128], std::vector<STextColorSplit> &vColorSplits);

	void RenderEditorHistory(CUIRect View);

	enum class EDragSide // Which side is the drag bar on
	{
		SIDE_BOTTOM,
		SIDE_LEFT,
		SIDE_TOP,
		SIDE_RIGHT
	};
	void DoEditorDragBar(CUIRect View, CUIRect *pDragBar, EDragSide Side, float *pValue, float MinValue = 100.0f, float MaxValue = 400.0f);

	void SetHotEnvelopePoint(const CUIRect &View, const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels);

	void RenderMenubar(CUIRect Menubar);

	void SelectGameLayer();
	std::vector<int> SortImages();

	void DoAudioPreview(CUIRect View, const void *pPlayPauseButtonId, const void *pStopButtonId, const void *pSeekBarId, int SampleId);

	// Zooming
	void ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View);
	void UpdateZoomEnvelopeX(const CUIRect &View);

	void ZoomAdaptOffsetY(float ZoomFactor, const CUIRect &View);
	void UpdateZoomEnvelopeY(const CUIRect &View);

	void ResetZoomEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels);
	void RemoveTimeOffsetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope);
	float ScreenToEnvelopeX(const CUIRect &View, float x) const;
	float EnvelopeToScreenX(const CUIRect &View, float x) const;
	float ScreenToEnvelopeY(const CUIRect &View, float y) const;
	float EnvelopeToScreenY(const CUIRect &View, float y) const;
	float ScreenToEnvelopeDX(const CUIRect &View, float DeltaX);
	float ScreenToEnvelopeDY(const CUIRect &View, float DeltaY);

	// DDRace

	IGraphics::CTextureHandle GetFrontTexture();
	IGraphics::CTextureHandle GetTeleTexture();
	IGraphics::CTextureHandle GetSpeedupTexture();
	IGraphics::CTextureHandle GetSwitchTexture();
	IGraphics::CTextureHandle GetTuneTexture();

	unsigned char m_TeleNumber;
	unsigned char m_TeleCheckpointNumber;
	unsigned char m_ViewTeleNumber;

	unsigned char m_TuningNum;
	unsigned char m_ViewTuning;

	unsigned char m_SpeedupForce;
	unsigned char m_SpeedupMaxSpeed;
	short m_SpeedupAngle;

	unsigned char m_SwitchNum;
	unsigned char m_SwitchDelay;
	unsigned char m_ViewSwitch;

	void AdjustBrushSpecialTiles(bool UseNextFree, int Adjust = 0);

	// Undo/Redo
	CEditorHistory m_EditorHistory;
	CEditorHistory m_ServerSettingsHistory;
	CEditorHistory m_EnvelopeEditorHistory;
	CQuadEditTracker m_QuadTracker;
	CEnvelopeEditorOperationTracker m_EnvOpTracker;

private:
	CEditorHistory &ActiveHistory();

	std::map<int, CPoint[5]> m_QuadDragOriginalPoints;
};

// make sure to inline this function
inline class IGraphics *CLayer::Graphics() { return m_pEditor->Graphics(); }
inline class ITextRender *CLayer::TextRender() { return m_pEditor->TextRender(); }

#endif
