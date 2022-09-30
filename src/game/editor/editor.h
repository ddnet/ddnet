/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_EDITOR_H
#define GAME_EDITOR_EDITOR_H

#include <string>
#include <vector>

#include <base/system.h>

#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <engine/editor.h>
#include <engine/graphics.h>

#include "auto_map.h"

#include <chrono>

using namespace std::chrono_literals;

typedef void (*INDEX_MODIFY_FUNC)(int *pIndex);

//CRenderTools m_RenderTools;

// CEditor SPECIFIC
enum
{
	MODE_LAYERS = 0,
	MODE_IMAGES,
	MODE_SOUNDS,

	DIALOG_NONE = 0,
	DIALOG_FILE,
};

class CEnvelope
{
public:
	int m_Channels;
	std::vector<CEnvPoint> m_vPoints;
	char m_aName[32];
	float m_Bottom, m_Top;
	bool m_Synchronized;

	CEnvelope(int Chan)
	{
		m_Channels = Chan;
		m_aName[0] = 0;
		m_Bottom = 0;
		m_Top = 0;
		m_Synchronized = false;
	}

	void Resort()
	{
		std::sort(m_vPoints.begin(), m_vPoints.end());
		FindTopBottom(0xf);
	}

	void FindTopBottom(int ChannelMask)
	{
		m_Top = -1000000000.0f;
		m_Bottom = 1000000000.0f;
		for(auto &Point : m_vPoints)
		{
			for(int c = 0; c < m_Channels; c++)
			{
				if(ChannelMask & (1 << c))
				{
					float v = fx2f(Point.m_aValues[c]);
					if(v > m_Top)
						m_Top = v;
					if(v < m_Bottom)
						m_Bottom = v;
				}
			}
		}
	}

	int Eval(float Time, ColorRGBA &Color)
	{
		CRenderTools::RenderEvalEnvelope(&m_vPoints[0], m_vPoints.size(), m_Channels, std::chrono::nanoseconds((int64_t)((double)Time * (double)std::chrono::nanoseconds(1s).count())), Color);
		return m_Channels;
	}

	void AddPoint(int Time, int v0, int v1 = 0, int v2 = 0, int v3 = 0)
	{
		CEnvPoint p;
		p.m_Time = Time;
		p.m_aValues[0] = v0;
		p.m_aValues[1] = v1;
		p.m_aValues[2] = v2;
		p.m_aValues[3] = v3;
		p.m_Curvetype = CURVETYPE_LINEAR;
		m_vPoints.push_back(p);
		Resort();
	}

	float EndTime() const
	{
		if(!m_vPoints.empty())
			return m_vPoints[m_vPoints.size() - 1].m_Time * (1.0f / 1000.0f);
		return 0;
	}
};

class CLayerGroup;

class CLayer
{
public:
	class CEditor *m_pEditor;
	class IGraphics *Graphics();
	class ITextRender *TextRender();

	CLayer()
	{
		m_Type = LAYERTYPE_INVALID;
		str_copy(m_aName, "(invalid)", sizeof(m_aName));
		m_Visible = true;
		m_Readonly = false;
		m_Flags = 0;
		m_pEditor = nullptr;
		m_BrushRefCount = 0;
	}

	CLayer(const CLayer &Other)
	{
		str_copy(m_aName, Other.m_aName, sizeof(m_aName));
		m_Flags = Other.m_Flags;
		m_pEditor = Other.m_pEditor;
		m_Type = Other.m_Type;
		m_BrushRefCount = 0;
		m_Visible = true;
		m_Readonly = false;
	}

	virtual ~CLayer()
	{
	}

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) {}
	virtual void BrushDraw(CLayer *pBrush, float x, float y) {}
	virtual void BrushPlace(CLayer *pBrush, float x, float y) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual void Render(bool Tileset = false) {}
	virtual int RenderProperties(CUIRect *pToolbox) { return 0; }

	virtual void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc) {}
	virtual void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc) {}
	virtual void ModifySoundIndex(INDEX_MODIFY_FUNC pfnFunc) {}

	virtual CLayer *Duplicate() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight)
	{
		*pWidth = 0;
		*pHeight = 0;
	}

	char m_aName[12];
	int m_Type;
	int m_Flags;

	bool m_Readonly;
	bool m_Visible;
	int m_BrushRefCount;
};

class CLayerGroup
{
public:
	class CEditorMap *m_pMap;

	std::vector<CLayer *> m_vpLayers;

	int m_OffsetX;
	int m_OffsetY;

	int m_ParallaxX;
	int m_ParallaxY;
	int m_CustomParallaxZoom;
	int m_ParallaxZoom;

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	char m_aName[12];
	bool m_GameGroup;
	bool m_Visible;
	bool m_Collapse;

	CLayerGroup();
	~CLayerGroup();

	void Convert(CUIRect *pRect);
	void Render();
	void MapScreen();
	void Mapping(float *pPoints);

	void GetSize(float *pWidth, float *pHeight) const;

	void DeleteLayer(int Index);
	void DuplicateLayer(int Index);
	int SwapLayers(int Index0, int Index1);

	bool IsEmpty() const
	{
		return m_vpLayers.size() == 0; // stupid function, since its bad for Fillselection: TODO add a function for Fillselection that returns whether a specific tile is used in the given layer
	}

	/*bool IsUsedInThisLayer(int Layer, int Index) // <--------- this is what i meant but cause i don't know which Indexes belongs to which layers i can't finish yet
	{
		switch Layer
		{
			case LAYERTYPE_GAME: // security
				return true;
			case LAYERTYPE_FRONT:
				return true;
			case LAYERTYPE_TELE:
			{
				if (Index ==) // you could add an 2D array into mapitems.h which defines which Indexes belong to which layer(s)
			}
			case LAYERTYPE_SPEEDUP:
			{
				if (Index == TILE_BOOST)
					return true;
				else
					return false;
			}
			case LAYERTYPE_SWITCH:
			{

			}
			case LAYERTYPE_TUNE:
			{
				if (Index == TILE_TUNE)
					return true;
				else
					return false;
			}
			default:
				return false;
		}
	}*/

	void OnEdited()
	{
		if(!m_CustomParallaxZoom)
			m_ParallaxZoom = GetParallaxZoomDefault(m_ParallaxX, m_ParallaxY);
	}

	void Clear()
	{
		m_vpLayers.clear();
	}

	void AddLayer(CLayer *pLayer);

	void ModifyImageIndex(INDEX_MODIFY_FUNC Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyImageIndex(Func);
	}

	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyEnvelopeIndex(Func);
	}

	void ModifySoundIndex(INDEX_MODIFY_FUNC Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifySoundIndex(Func);
	}
};

class CEditorImage : public CImageInfo
{
public:
	CEditor *m_pEditor;

	CEditorImage(CEditor *pEditor) :
		m_AutoMapper(pEditor)
	{
		m_pEditor = pEditor;
		m_aName[0] = 0;
		m_External = 0;
		m_Width = 0;
		m_Height = 0;
		m_pData = nullptr;
		m_Format = 0;
	}

	~CEditorImage();

	void AnalyseTileFlags();

	IGraphics::CTextureHandle m_Texture;
	int m_External;
	char m_aName[128];
	unsigned char m_aTileFlags[256];
	class CAutoMapper m_AutoMapper;
};

class CEditorSound
{
public:
	CEditor *m_pEditor;

	CEditorSound(CEditor *pEditor)
	{
		m_pEditor = pEditor;
		m_aName[0] = 0;
		m_SoundID = 0;

		m_pData = nullptr;
		m_DataSize = 0;
	}

	~CEditorSound();

	int m_SoundID;
	char m_aName[128];

	void *m_pData;
	unsigned m_DataSize;
};

class CEditorMap
{
	void MakeGameGroup(CLayerGroup *pGroup);
	void MakeGameLayer(CLayer *pLayer);

public:
	CEditor *m_pEditor;
	bool m_Modified;

	CEditorMap()
	{
		Clean();
	}

	~CEditorMap()
	{
		Clean();
	}

	std::vector<CLayerGroup *> m_vpGroups;
	std::vector<CEditorImage *> m_vpImages;
	std::vector<CEnvelope *> m_vpEnvelopes;
	std::vector<CEditorSound *> m_vpSounds;

	class CMapInfo
	{
	public:
		char m_aAuthorTmp[32];
		char m_aVersionTmp[16];
		char m_aCreditsTmp[128];
		char m_aLicenseTmp[32];

		char m_aAuthor[32];
		char m_aVersion[16];
		char m_aCredits[128];
		char m_aLicense[32];

		void Reset()
		{
			m_aAuthorTmp[0] = 0;
			m_aVersionTmp[0] = 0;
			m_aCreditsTmp[0] = 0;
			m_aLicenseTmp[0] = 0;

			m_aAuthor[0] = 0;
			m_aVersion[0] = 0;
			m_aCredits[0] = 0;
			m_aLicense[0] = 0;
		}
	};
	CMapInfo m_MapInfo;

	struct CSetting
	{
		char m_aCommand[256];
	};
	std::vector<CSetting> m_vSettings;

	class CLayerGame *m_pGameLayer;
	CLayerGroup *m_pGameGroup;

	CEnvelope *NewEnvelope(int Channels)
	{
		m_Modified = true;
		CEnvelope *pEnv = new CEnvelope(Channels);
		m_vpEnvelopes.push_back(pEnv);
		return pEnv;
	}

	void DeleteEnvelope(int Index);

	CLayerGroup *NewGroup()
	{
		m_Modified = true;
		CLayerGroup *pGroup = new CLayerGroup;
		pGroup->m_pMap = this;
		m_vpGroups.push_back(pGroup);
		return pGroup;
	}

	int SwapGroups(int Index0, int Index1)
	{
		if(Index0 < 0 || Index0 >= (int)m_vpGroups.size())
			return Index0;
		if(Index1 < 0 || Index1 >= (int)m_vpGroups.size())
			return Index0;
		if(Index0 == Index1)
			return Index0;
		m_Modified = true;
		std::swap(m_vpGroups[Index0], m_vpGroups[Index1]);
		return Index1;
	}

	void DeleteGroup(int Index)
	{
		if(Index < 0 || Index >= (int)m_vpGroups.size())
			return;
		m_Modified = true;
		delete m_vpGroups[Index];
		m_vpGroups.erase(m_vpGroups.begin() + Index);
	}

	void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifyImageIndex(pfnFunc);
	}

	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifyEnvelopeIndex(pfnFunc);
	}

	void ModifySoundIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifySoundIndex(pfnFunc);
	}

	void Clean();
	void CreateDefault(IGraphics::CTextureHandle EntitiesTexture);

	// io
	int Save(class IStorage *pStorage, const char *pFilename);
	int Load(class IStorage *pStorage, const char *pFilename, int StorageType);

	// DDRace

	class CLayerTele *m_pTeleLayer;
	class CLayerSpeedup *m_pSpeedupLayer;
	class CLayerFront *m_pFrontLayer;
	class CLayerSwitch *m_pSwitchLayer;
	class CLayerTune *m_pTuneLayer;
	void MakeTeleLayer(CLayer *pLayer);
	void MakeSpeedupLayer(CLayer *pLayer);
	void MakeFrontLayer(CLayer *pLayer);
	void MakeSwitchLayer(CLayer *pLayer);
	void MakeTuneLayer(CLayer *pLayer);
};

struct CProperty
{
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
	PROPTYPE_INT_STEP,
	PROPTYPE_INT_SCROLL,
	PROPTYPE_ANGLE_SCROLL,
	PROPTYPE_COLOR,
	PROPTYPE_IMAGE,
	PROPTYPE_ENVELOPE,
	PROPTYPE_SHIFT,
	PROPTYPE_SOUND,
	PROPTYPE_AUTOMAPPER,
};

enum
{
	DIRECTION_LEFT = 1,
	DIRECTION_RIGHT = 2,
	DIRECTION_UP = 4,
	DIRECTION_DOWN = 8,
};

struct RECTi
{
	int x, y;
	int w, h;
};

class CLayerTiles : public CLayer
{
protected:
	template<typename T>
	void ShiftImpl(T *pTiles, int Direction, int ShiftBy)
	{
		switch(Direction)
		{
		case DIRECTION_LEFT:
			for(int y = 0; y < m_Height; ++y)
			{
				mem_move(&pTiles[y * m_Width], &pTiles[y * m_Width + ShiftBy], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width + (m_Width - ShiftBy)], ShiftBy * sizeof(T));
			}
			break;
		case DIRECTION_RIGHT:
			for(int y = 0; y < m_Height; ++y)
			{
				mem_move(&pTiles[y * m_Width + ShiftBy], &pTiles[y * m_Width], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width], ShiftBy * sizeof(T));
			}
			break;
		case DIRECTION_UP:
			for(int y = 0; y < m_Height - ShiftBy; ++y)
			{
				mem_copy(&pTiles[y * m_Width], &pTiles[(y + ShiftBy) * m_Width], m_Width * sizeof(T));
				mem_zero(&pTiles[(y + ShiftBy) * m_Width], m_Width * sizeof(T));
			}
			break;
		case DIRECTION_DOWN:
			for(int y = m_Height - 1; y >= ShiftBy; --y)
			{
				mem_copy(&pTiles[y * m_Width], &pTiles[(y - ShiftBy) * m_Width], m_Width * sizeof(T));
				mem_zero(&pTiles[(y - ShiftBy) * m_Width], m_Width * sizeof(T));
			}
		}
	}
	template<typename T>
	void BrushFlipXImpl(T *pTiles)
	{
		for(int y = 0; y < m_Height; y++)
			for(int x = 0; x < m_Width / 2; x++)
				std::swap(pTiles[y * m_Width + x], pTiles[(y + 1) * m_Width - 1 - x]);
	}
	template<typename T>
	void BrushFlipYImpl(T *pTiles)
	{
		for(int y = 0; y < m_Height / 2; y++)
			for(int x = 0; x < m_Width; x++)
				std::swap(pTiles[y * m_Width + x], pTiles[(m_Height - 1 - y) * m_Width + x]);
	}

public:
	CLayerTiles(int w, int h);
	CLayerTiles(const CLayerTiles &Other);
	~CLayerTiles();

	virtual CTile GetTile(int x, int y);
	virtual void SetTile(int x, int y, CTile tile);

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);

	void MakePalette();
	void Render(bool Tileset = false) override;

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, RECTi *pOut);
	void Snap(CUIRect *pRect);
	void Clamp(RECTi *pRect);

	virtual bool IsEmpty(CLayerTiles *pLayer);
	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	void BrushDraw(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	CLayer *Duplicate() const override;

	virtual void ShowInfo();
	int RenderProperties(CUIRect *pToolbox) override;

	struct SCommonPropState
	{
		enum
		{
			MODIFIED_SIZE = 1 << 0,
			MODIFIED_COLOR = 1 << 1,
		};
		int m_Modified = 0;
		int m_Width = -1;
		int m_Height = -1;
		int m_Color = 0;
	};
	static int RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<CLayerTiles *> &vpLayers);

	void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc) override;
	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc) override;

	void PrepareForSave();

	void GetSize(float *pWidth, float *pHeight) override
	{
		*pWidth = m_Width * 32.0f;
		*pHeight = m_Height * 32.0f;
	}

	void FlagModified(int x, int y, int w, int h);

	IGraphics::CTextureHandle m_Texture;
	int m_Game;
	int m_Image;
	int m_Width;
	int m_Height;
	CColor m_Color;
	int m_ColorEnv;
	int m_ColorEnvOffset;
	CTile *m_pTiles;

	// DDRace

	int m_AutoMapperConfig;
	int m_Seed;
	bool m_AutoAutoMap;
	int m_Tele;
	int m_Speedup;
	int m_Front;
	int m_Switch;
	int m_Tune;
	char m_aFileName[IO_MAX_PATH_LENGTH];
};

class CLayerQuads : public CLayer
{
public:
	CLayerQuads();
	CLayerQuads(const CLayerQuads &Other);
	~CLayerQuads();

	void Render(bool QuadPicker = false) override;
	CQuad *NewQuad(int x, int y, int Width, int Height);
	int SwapQuads(int Index0, int Index1);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void BrushPlace(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	int RenderProperties(CUIRect *pToolbox) override;

	void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc) override;
	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc) override;

	void GetSize(float *pWidth, float *pHeight) override;
	CLayer *Duplicate() const override;

	int m_Image;
	std::vector<CQuad> m_vQuads;
};

class CLayerGame : public CLayerTiles
{
public:
	CLayerGame(int w, int h);
	~CLayerGame();

	CTile GetTile(int x, int y) override;
	void SetTile(int x, int y, CTile tile) override;

	int RenderProperties(CUIRect *pToolbox) override;
};

class CEditor : public IEditor
{
	class IInput *m_pInput;
	class IClient *m_pClient;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class ISound *m_pSound;
	class IStorage *m_pStorage;
	CRenderTools m_RenderTools;
	CUI m_UI;

	bool m_EditorWasUsedBefore = false;

	IGraphics::CTextureHandle m_EntitiesTexture;

	IGraphics::CTextureHandle m_FrontTexture;
	IGraphics::CTextureHandle m_TeleTexture;
	IGraphics::CTextureHandle m_SpeedupTexture;
	IGraphics::CTextureHandle m_SwitchTexture;
	IGraphics::CTextureHandle m_TuneTexture;

	int GetTextureUsageFlag();

public:
	class IInput *Input() { return m_pInput; }
	class IClient *Client() { return m_pClient; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IGraphics *Graphics() { return m_pGraphics; }
	class ISound *Sound() { return m_pSound; }
	class ITextRender *TextRender() { return m_pTextRender; }
	class IStorage *Storage() { return m_pStorage; }
	CUI *UI() { return &m_UI; }
	CRenderTools *RenderTools() { return &m_RenderTools; }

	CEditor() :
		m_TilesetPicker(16, 16)
	{
		m_pInput = nullptr;
		m_pClient = nullptr;
		m_pGraphics = nullptr;
		m_pTextRender = nullptr;
		m_pSound = nullptr;

		m_Mode = MODE_LAYERS;
		m_Dialog = 0;
		m_EditBoxActive = 0;
		m_pTooltip = nullptr;

		m_GridActive = false;
		m_GridFactor = 1;

		m_BrushColorEnabled = true;

		m_aFileName[0] = 0;
		m_aFileSaveName[0] = 0;
		m_ValidSaveFilename = false;

		m_PopupEventActivated = false;
		m_PopupEventWasActivated = false;
		m_MouseInsidePopup = false;

		m_FileDialogStorageType = 0;
		m_pFileDialogTitle = nullptr;
		m_pFileDialogButtonText = nullptr;
		m_pFileDialogUser = nullptr;
		m_aFileDialogFileName[0] = 0;
		m_aFileDialogCurrentFolder[0] = 0;
		m_aFileDialogCurrentLink[0] = 0;
		m_pFileDialogPath = m_aFileDialogCurrentFolder;
		m_FileDialogActivate = false;
		m_FileDialogOpening = false;
		m_FileDialogScrollValue = 0.0f;
		m_FilesSelectedIndex = -1;
		m_FilesStartAt = 0;
		m_FilesCur = 0;
		m_FilesStopAt = 999;

		m_SelectEntitiesImage = "DDNet";

		m_WorldOffsetX = 0;
		m_WorldOffsetY = 0;
		m_EditorOffsetX = 0.0f;
		m_EditorOffsetY = 0.0f;

		m_WorldZoom = 1.0f;
		m_ZoomLevel = 200;
		m_LockMouse = false;
		m_ShowMousePointer = true;
		m_MouseDeltaX = 0;
		m_MouseDeltaY = 0;
		m_MouseDeltaWx = 0;
		m_MouseDeltaWy = 0;

		m_GuiActive = true;
		m_ProofBorders = false;
		m_PreviewZoom = false;

		m_ShowTileInfo = false;
		m_ShowDetail = true;
		m_Animate = false;
		m_AnimateStart = 0;
		m_AnimateTime = 0;
		m_AnimateSpeed = 1;

		m_ShowEnvelopeEditor = 0;
		m_ShowServerSettingsEditor = false;

		m_ShowEnvelopePreview = SHOWENV_NONE;
		m_SelectedQuadEnvelope = -1;
		m_SelectedEnvelopePoint = -1;

		m_QuadKnifeActive = false;
		m_QuadKnifeCount = 0;

		m_CommandBox = 0.0f;
		m_aSettingsCommand[0] = 0;

		ms_pUiGotContext = nullptr;

		// DDRace

		m_TeleNumber = 1;
		m_SwitchNum = 1;
		m_TuningNum = 1;
		m_SwitchDelay = 0;
		m_SpeedupForce = 50;
		m_SpeedupMaxSpeed = 0;
		m_SpeedupAngle = 0;
		m_LargeLayerWasWarned = false;
		m_PreventUnusedTilesWasWarned = false;
		m_AllowPlaceUnusedTiles = 0;
		m_BrushDrawDestructive = true;
		m_GotoX = 0;
		m_GotoY = 0;

		m_Mentions = 0;
	}

	void Init() override;
	void OnUpdate() override;
	void OnRender() override;
	bool HasUnsavedData() const override { return m_Map.m_Modified; }
	void UpdateMentions() override { m_Mentions++; }
	void ResetMentions() override { m_Mentions = 0; }

	CLayerGroup *m_apSavedBrushes[10];

	void FilelistPopulate(int StorageType);
	void InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
		const char *pBasepath, const char *pDefaultName,
		void (*pfnFunc)(const char *pFilename, int StorageType, void *pUser), void *pUser);

	void Reset(bool CreateDefault = true);
	int Save(const char *pFilename) override;
	int Load(const char *pFilename, int StorageType) override;
	int Append(const char *pFilename, int StorageType);
	void LoadCurrentMap();
	void Render();

	std::vector<CQuad *> GetSelectedQuads();
	CLayer *GetSelectedLayerType(int Index, int Type) const;
	CLayer *GetSelectedLayer(int Index) const;
	CLayerGroup *GetSelectedGroup() const;
	CSoundSource *GetSelectedSource();
	void SelectLayer(int LayerIndex, int GroupIndex = -1);
	void AddSelectedLayer(int LayerIndex);
	void SelectQuad(int Index);
	void DeleteSelectedQuads();
	bool IsQuadSelected(int Index) const;
	int FindSelectedQuadIndex(int Index) const;
	bool IsSpecialLayer(const CLayer *pLayer) const;

	float ScaleFontSize(char *pText, int TextSize, float FontSize, int Width);
	int DoProperties(CUIRect *pToolbox, CProperty *pProps, int *pIDs, int *pNewVal, ColorRGBA Color = ColorRGBA(1, 1, 1, 0.5f));

	int m_Mode;
	int m_Dialog;
	int m_EditBoxActive;
	const char *m_pTooltip;

	bool m_GridActive;
	int m_GridFactor;

	bool m_BrushColorEnabled;

	char m_aFileName[IO_MAX_PATH_LENGTH];
	char m_aFileSaveName[IO_MAX_PATH_LENGTH];
	bool m_ValidSaveFilename;

	enum
	{
		POPEVENT_EXIT = 0,
		POPEVENT_LOAD,
		POPEVENT_LOADCURRENT,
		POPEVENT_NEW,
		POPEVENT_SAVE,
		POPEVENT_LARGELAYER,
		POPEVENT_PREVENTUNUSEDTILES,
		POPEVENT_IMAGEDIV16,
		POPEVENT_IMAGE_MAX,
		POPEVENT_PLACE_BORDER_TILES
	};

	int m_PopupEventType;
	int m_PopupEventActivated;
	int m_PopupEventWasActivated;
	bool m_MouseInsidePopup;
	bool m_LargeLayerWasWarned;
	bool m_PreventUnusedTilesWasWarned;
	int m_AllowPlaceUnusedTiles;
	bool m_BrushDrawDestructive;

	int m_Mentions;

	enum
	{
		FILETYPE_MAP,
		FILETYPE_IMG,
		FILETYPE_SOUND,
	};

	int m_FileDialogStorageType;
	const char *m_pFileDialogTitle;
	const char *m_pFileDialogButtonText;
	void (*m_pfnFileDialogFunc)(const char *pFileName, int StorageType, void *pUser);
	void *m_pFileDialogUser;
	char m_aFileDialogFileName[IO_MAX_PATH_LENGTH];
	char m_aFileDialogCurrentFolder[IO_MAX_PATH_LENGTH];
	char m_aFileDialogCurrentLink[IO_MAX_PATH_LENGTH];
	char m_aFileDialogSearchText[64];
	char m_aFileDialogPrevSearchText[64];
	char *m_pFileDialogPath;
	bool m_FileDialogActivate;
	int m_FileDialogFileType;
	float m_FileDialogScrollValue;
	int m_FilesSelectedIndex;
	char m_aFileDialogNewFolderName[64];
	char m_aFileDialogErrString[64];
	IGraphics::CTextureHandle m_FilePreviewImage;
	bool m_PreviewImageIsLoaded;
	CImageInfo m_FilePreviewImageInfo;
	bool m_FileDialogOpening;

	struct CFilelistItem
	{
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aName[128];
		bool m_IsDir;
		bool m_IsLink;
		int m_StorageType;
		bool m_IsVisible;

		bool operator<(const CFilelistItem &Other) const { return !str_comp(m_aFilename, "..") ? true : !str_comp(Other.m_aFilename, "..") ? false : m_IsDir && !Other.m_IsDir ? true : !m_IsDir && Other.m_IsDir ? false : str_comp_filenames(m_aFilename, Other.m_aFilename) < 0; }
	};
	std::vector<CFilelistItem> m_vFileList;
	int m_FilesStartAt;
	int m_FilesCur;
	int m_FilesStopAt;

	std::vector<std::string> m_vSelectEntitiesFiles;
	std::string m_SelectEntitiesImage;

	float m_WorldOffsetX;
	float m_WorldOffsetY;
	float m_EditorOffsetX;
	float m_EditorOffsetY;
	float m_WorldZoom;
	int m_ZoomLevel;
	bool m_LockMouse;
	bool m_ShowMousePointer;
	bool m_GuiActive;
	bool m_ProofBorders;
	bool m_PreviewZoom;
	float m_MouseWScale = 1.0f; // Mouse (i.e. UI) scale relative to the World (selected Group)
	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	float m_MouseWorldX = 0.0f;
	float m_MouseWorldY = 0.0f;
	float m_MouseDeltaX;
	float m_MouseDeltaY;
	float m_MouseDeltaWx;
	float m_MouseDeltaWy;

	bool m_ShowTileInfo;
	bool m_ShowDetail;
	bool m_Animate;
	int64_t m_AnimateStart;
	float m_AnimateTime;
	float m_AnimateSpeed;

	int m_ShowEnvelopeEditor;

	enum EShowEnvelope
	{
		SHOWENV_NONE = 0,
		SHOWENV_SELECTED,
		SHOWENV_ALL
	};
	EShowEnvelope m_ShowEnvelopePreview;
	bool m_ShowServerSettingsEditor;
	bool m_ShowPicker;

	std::vector<int> m_vSelectedLayers;
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoint;
	int m_SelectedQuadIndex;
	int m_SelectedGroup;
	int m_SelectedPoints;
	int m_SelectedEnvelope;
	int m_SelectedEnvelopePoint;
	int m_SelectedQuadEnvelope;
	int m_SelectedImage;
	int m_SelectedSound;
	int m_SelectedSource;

	bool m_QuadKnifeActive;
	int m_QuadKnifeCount;
	vec2 m_aQuadKnifePoints[4];

	IGraphics::CTextureHandle m_CheckerTexture;
	IGraphics::CTextureHandle m_BackgroundTexture;
	IGraphics::CTextureHandle m_CursorTexture;

	IGraphics::CTextureHandle GetEntitiesTexture();

	CLayerGroup m_Brush;
	CLayerTiles m_TilesetPicker;
	CLayerQuads m_QuadsetPicker;

	static const void *ms_pUiGotContext;

	CEditorMap m_Map;
	int m_ShiftBy;

	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Channels, void *pUser);

	float m_CommandBox;
	char m_aSettingsCommand[256];

	void PlaceBorderTiles();
	int DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int AlignVert = 1);
	int DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA Color);

	int DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = 10.0f, int AlignVert = 1);
	int DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags = 0, const char *pToolTip = nullptr);

	int DoButton_ColorPicker(const void *pID, const CUIRect *pRect, ColorRGBA *pColor, const char *pToolTip = nullptr);

	bool DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden = false, int Corners = IGraphics::CORNER_ALL);
	bool DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners);

	void RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness);

	void RenderGrid(CLayerGroup *pGroup);

	void UiInvokePopupMenu(void *pID, int Flags, float X, float Y, float W, float H, int (*pfnFunc)(CEditor *pEditor, CUIRect Rect, void *pContext), void *pContext = nullptr);
	void UiDoPopupMenu();
	bool UiPopupExists(void *pID);
	bool UiPopupOpen();

	int UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree = false, bool IsHex = false, int corners = IGraphics::CORNER_ALL, ColorRGBA *pColor = nullptr);

	static int PopupGroup(CEditor *pEditor, CUIRect View, void *pContext);

	struct CLayerPopupContext
	{
		std::vector<CLayerTiles *> m_vpLayers;
		CLayerTiles::SCommonPropState m_CommonPropState;
	};
	static int PopupLayer(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupQuad(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupPoint(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupNewFolder(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupMapInfo(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupEvent(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSelectImage(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSelectSound(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSelectGametileOp(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupImage(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupMenuFile(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSelectConfigAutoMap(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSound(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSource(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupColorPicker(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupEntities(CEditor *pEditor, CUIRect View, void *pContext);

	static void CallbackOpenMap(const char *pFileName, int StorageType, void *pUser);
	static void CallbackAppendMap(const char *pFileName, int StorageType, void *pUser);
	static void CallbackSaveMap(const char *pFileName, int StorageType, void *pUser);
	static void CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser);

	void PopupSelectImageInvoke(int Current, float x, float y);
	int PopupSelectImageResult();

	void PopupSelectGametileOpInvoke(float x, float y);
	int PopupSelectGameTileOpResult();

	void PopupSelectConfigAutoMapInvoke(int Current, float x, float y);
	int PopupSelectConfigAutoMapResult();

	void PopupSelectSoundInvoke(int Current, float x, float y);
	int PopupSelectSoundResult();

	void DoQuadEnvelopes(const std::vector<CQuad> &vQuads, IGraphics::CTextureHandle Texture = IGraphics::CTextureHandle());
	void DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int pIndex);
	void DoQuadPoint(CQuad *pQuad, int QuadIndex, int v);

	float TriangleArea(vec2 A, vec2 B, vec2 C);
	bool IsInTriangle(vec2 Point, vec2 A, vec2 B, vec2 C);
	void DoQuadKnife(int QuadIndex);

	void DoSoundSource(CSoundSource *pSource, int Index);

	void DoMapEditor(CUIRect View);
	void DoToolbar(CUIRect Toolbar);
	void DoQuad(CQuad *pQuad, int Index);
	ColorRGBA GetButtonColor(const void *pID, int Checked);

	static void ReplaceImage(const char *pFilename, int StorageType, void *pUser);
	static void ReplaceSound(const char *pFileName, int StorageType, void *pUser);
	static void AddImage(const char *pFilename, int StorageType, void *pUser);
	static void AddSound(const char *pFileName, int StorageType, void *pUser);

	bool IsEnvelopeUsed(int EnvelopeIndex) const;

	void RenderLayers(CUIRect LayersBox);
	void RenderImagesList(CUIRect Toolbox);
	void RenderSelectedImage(CUIRect View);
	void RenderSounds(CUIRect Toolbox);
	void RenderModebar(CUIRect View);
	void RenderStatusbar(CUIRect View);
	void RenderEnvelopeEditor(CUIRect View);
	void RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast);

	void RenderMenubar(CUIRect Menubar);
	void RenderFileDialog();

	void AddFileDialogEntry(int Index, CUIRect *pView);
	void SelectGameLayer();
	void SortImages();
	bool SelectLayerByTile();

	//Tile Numbers For Explanations - TODO: Add/Improve tiles and explanations
	enum
	{
		TILE_PUB_AIR,
		TILE_PUB_HOOKABLE,
		TILE_PUB_DEATH,
		TILE_PUB_UNHOOKABLE,

		TILE_PUB_CREDITS1 = 140,
		TILE_PUB_CREDITS2,
		TILE_PUB_CREDITS3,
		TILE_PUB_CREDITS4,
		TILE_PUB_CREDITS5 = 156,
		TILE_PUB_CREDITS6,
		TILE_PUB_CREDITS7,
		TILE_PUB_CREDITS8,

		TILE_PUB_ENTITIES_OFF1 = 190,
		TILE_PUB_ENTITIES_OFF2,
	};

	enum
	{
		TILE_FNG_SPIKE_GOLD = 7,
		TILE_FNG_SPIKE_NORMAL,
		TILE_FNG_SPIKE_RED,
		TILE_FNG_SPIKE_BLUE,
		TILE_FNG_SCORE_RED,
		TILE_FNG_SCORE_BLUE,

		TILE_FNG_SPIKE_GREEN = 14,
		TILE_FNG_SPIKE_PURPLE,

		TILE_FNG_SPAWN = 192,
		TILE_FNG_SPAWN_RED,
		TILE_FNG_SPAWN_BLUE,
		TILE_FNG_FLAG_RED,
		TILE_FNG_FLAG_BLUE,
		TILE_FNG_SHIELD,
		TILE_FNG_HEART,
		TILE_FNG_SHOTGUN,
		TILE_FNG_GRENADE,
		TILE_FNG_NINJA,
		TILE_FNG_LASER,

		TILE_FNG_SPIKE_OLD1 = 208,
		TILE_FNG_SPIKE_OLD2,
		TILE_FNG_SPIKE_OLD3
	};

	enum
	{
		TILE_VANILLA_SPAWN = 192,
		TILE_VANILLA_SPAWN_RED,
		TILE_VANILLA_SPAWN_BLUE,
		TILE_VANILLA_FLAG_RED,
		TILE_VANILLA_FLAG_BLUE,
		TILE_VANILLA_SHIELD,
		TILE_VANILLA_HEART,
		TILE_VANILLA_SHOTGUN,
		TILE_VANILLA_GRENADE,
		TILE_VANILLA_NINJA,
		TILE_VANILLA_LASER,
	};

	//Explanations
	enum
	{
		EXPLANATION_DDNET,
		EXPLANATION_FNG,
		EXPLANATION_RACE,
		EXPLANATION_VANILLA,
		EXPLANATION_BLOCKWORLDS
	};
	static const char *Explain(int ExplanationID, int Tile, int Layer);

	int GetLineDistance() const;
	void ZoomMouseTarget(float ZoomFactor);

	static ColorHSVA ms_PickerColor;
	static int ms_SVPicker;
	static int ms_HuePicker;

	// DDRace

	IGraphics::CTextureHandle GetFrontTexture();
	IGraphics::CTextureHandle GetTeleTexture();
	IGraphics::CTextureHandle GetSpeedupTexture();
	IGraphics::CTextureHandle GetSwitchTexture();
	IGraphics::CTextureHandle GetTuneTexture();

	static int PopupTele(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSpeedup(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSwitch(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupTune(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupGoto(CEditor *pEditor, CUIRect View, void *pContext);
	void Goto(float X, float Y);
	unsigned char m_TeleNumber;

	unsigned char m_TuningNum;

	unsigned char m_SpeedupForce;
	unsigned char m_SpeedupMaxSpeed;
	short m_SpeedupAngle;

	unsigned char m_SwitchNum;
	unsigned char m_SwitchDelay;

	int m_GotoX;
	int m_GotoY;
};

// make sure to inline this function
inline class IGraphics *CLayer::Graphics() { return m_pEditor->Graphics(); }
inline class ITextRender *CLayer::TextRender() { return m_pEditor->TextRender(); }

// DDRace

class CLayerTele : public CLayerTiles
{
public:
	CLayerTele(int w, int h);
	~CLayerTele();

	CTeleTile *m_pTeleTile;
	unsigned char m_TeleNum;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(CLayerTiles *pLayer) override;
	void BrushDraw(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	virtual bool ContainsElementWithId(int Id);
};

class CLayerSpeedup : public CLayerTiles
{
public:
	CLayerSpeedup(int w, int h);
	~CLayerSpeedup();

	CSpeedupTile *m_pSpeedupTile;
	int m_SpeedupForce;
	int m_SpeedupMaxSpeed;
	int m_SpeedupAngle;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(CLayerTiles *pLayer) override;
	void BrushDraw(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
};

class CLayerFront : public CLayerTiles
{
public:
	CLayerFront(int w, int h);

	void Resize(int NewW, int NewH) override;
	void SetTile(int x, int y, CTile tile) override;
};

class CLayerSwitch : public CLayerTiles
{
public:
	CLayerSwitch(int w, int h);
	~CLayerSwitch();

	CSwitchTile *m_pSwitchTile;
	unsigned char m_SwitchNumber;
	unsigned char m_SwitchDelay;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(CLayerTiles *pLayer) override;
	void BrushDraw(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	virtual bool ContainsElementWithId(int Id);
};

class CLayerTune : public CLayerTiles
{
public:
	CLayerTune(int w, int h);
	~CLayerTune();

	CTuneTile *m_pTuneTile;
	unsigned char m_TuningNumber;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(CLayerTiles *pLayer) override;
	void BrushDraw(CLayer *pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
};

class CLayerSounds : public CLayer
{
public:
	CLayerSounds();
	CLayerSounds(const CLayerSounds &Other);
	~CLayerSounds();

	void Render(bool Tileset = false) override;
	CSoundSource *NewSource(int x, int y);

	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void BrushPlace(CLayer *pBrush, float wx, float wy) override;

	int RenderProperties(CUIRect *pToolbox) override;

	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc) override;
	void ModifySoundIndex(INDEX_MODIFY_FUNC pfnFunc) override;

	CLayer *Duplicate() const override;

	int m_Sound;
	std::vector<CSoundSource> m_vSources;
};

#endif
