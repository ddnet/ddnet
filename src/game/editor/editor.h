/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_EDITOR_H
#define GAME_EDITOR_EDITOR_H

#include <math.h>
#include <string>
#include <vector>

#include <base/math.h>
#include <base/system.h>

#include <base/tl/algorithm.h>
#include <base/tl/array.h>
#include <base/tl/sorted_array.h>
#include <base/tl/string.h>

#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/mapitems.h>

#include <engine/editor.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/sound.h>
#include <engine/storage.h>

#include "auto_map.h"

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
	array<CEnvPoint> m_lPoints;
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
		sort(m_lPoints.all());
		FindTopBottom(0xf);
	}

	void FindTopBottom(int ChannelMask)
	{
		m_Top = -1000000000.0f;
		m_Bottom = 1000000000.0f;
		for(int i = 0; i < m_lPoints.size(); i++)
		{
			for(int c = 0; c < m_Channels; c++)
			{
				if(ChannelMask & (1 << c))
				{
					float v = fx2f(m_lPoints[i].m_aValues[c]);
					if(v > m_Top)
						m_Top = v;
					if(v < m_Bottom)
						m_Bottom = v;
				}
			}
		}
	}

	int Eval(float Time, float *pResult)
	{
		CRenderTools::RenderEvalEnvelope(m_lPoints.base_ptr(), m_lPoints.size(), m_Channels, Time, pResult);
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
		m_lPoints.add(p);
		Resort();
	}

	float EndTime()
	{
		if(m_lPoints.size())
			return m_lPoints[m_lPoints.size() - 1].m_Time * (1.0f / 1000.0f);
		return 0;
	}
};

class CLayer;
class CLayerGroup;
class CEditorMap;

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
		m_pEditor = 0;
		m_BrushRefCount = 0;
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

	virtual void GetSize(float *w, float *h)
	{
		*w = 0;
		*h = 0;
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

	array<CLayer *> m_lLayers;

	int m_OffsetX;
	int m_OffsetY;

	int m_ParallaxX;
	int m_ParallaxY;

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

	void GetSize(float *w, float *h);

	void DeleteLayer(int Index);
	int SwapLayers(int Index0, int Index1);

	bool IsEmpty() const
	{
		return m_lLayers.size() == 0; // stupid function, since its bad for Fillselection: TODO add a function for Fillselection that returns whether a specific tile is used in the given layer
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

	void Clear()
	{
		//m_lLayers.delete_all();
		m_lLayers.clear();
	}

	void AddLayer(CLayer *l);

	void ModifyImageIndex(INDEX_MODIFY_FUNC Func)
	{
		for(int i = 0; i < m_lLayers.size(); i++)
			m_lLayers[i]->ModifyImageIndex(Func);
	}

	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC Func)
	{
		for(int i = 0; i < m_lLayers.size(); i++)
			m_lLayers[i]->ModifyEnvelopeIndex(Func);
	}

	void ModifySoundIndex(INDEX_MODIFY_FUNC Func)
	{
		for(int i = 0; i < m_lLayers.size(); i++)
			m_lLayers[i]->ModifySoundIndex(Func);
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
		m_pData = 0;
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
		m_External = 0;
		m_SoundID = 0;

		m_pData = 0x0;
		m_DataSize = 0;
	}

	~CEditorSound();

	int m_SoundID;
	int m_External;
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
	int m_UndoModified;

	CEditorMap()
	{
		Clean();
	}

	~CEditorMap()
	{
		Clean();
	}

	array<CLayerGroup *> m_lGroups;
	array<CEditorImage *> m_lImages;
	array<CEnvelope *> m_lEnvelopes;
	array<CEditorSound *> m_lSounds;

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
	array<CSetting> m_lSettings;

	class CLayerGame *m_pGameLayer;
	CLayerGroup *m_pGameGroup;

	CEnvelope *NewEnvelope(int Channels)
	{
		m_Modified = true;
		m_UndoModified++;
		CEnvelope *e = new CEnvelope(Channels);
		m_lEnvelopes.add(e);
		return e;
	}

	void DeleteEnvelope(int Index);

	CLayerGroup *NewGroup()
	{
		m_Modified = true;
		m_UndoModified++;
		CLayerGroup *g = new CLayerGroup;
		g->m_pMap = this;
		m_lGroups.add(g);
		return g;
	}

	int SwapGroups(int Index0, int Index1)
	{
		if(Index0 < 0 || Index0 >= m_lGroups.size())
			return Index0;
		if(Index1 < 0 || Index1 >= m_lGroups.size())
			return Index0;
		if(Index0 == Index1)
			return Index0;
		m_Modified = true;
		m_UndoModified++;
		swap(m_lGroups[Index0], m_lGroups[Index1]);
		return Index1;
	}

	void DeleteGroup(int Index)
	{
		if(Index < 0 || Index >= m_lGroups.size())
			return;
		m_Modified = true;
		m_UndoModified++;
		delete m_lGroups[Index];
		m_lGroups.remove_index(Index);
	}

	void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		m_UndoModified++;
		for(int i = 0; i < m_lGroups.size(); i++)
			m_lGroups[i]->ModifyImageIndex(pfnFunc);
	}

	void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		m_UndoModified++;
		for(int i = 0; i < m_lGroups.size(); i++)
			m_lGroups[i]->ModifyEnvelopeIndex(pfnFunc);
	}

	void ModifySoundIndex(INDEX_MODIFY_FUNC pfnFunc)
	{
		m_Modified = true;
		m_UndoModified++;
		for(int i = 0; i < m_lGroups.size(); i++)
			m_lGroups[i]->ModifySoundIndex(pfnFunc);
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

typedef struct
{
	int x, y;
	int w, h;
} RECTi;

class CLayerTiles : public CLayer
{
public:
	CLayerTiles(int w, int h);
	~CLayerTiles();

	virtual CTile GetTile(int x, int y);
	virtual void SetTile(int x, int y, CTile tile);

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);

	void MakePalette();
	virtual void Render(bool Tileset = false);

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, RECTi *pOut);
	void Snap(CUIRect *pRect);
	void Clamp(RECTi *pRect);

	virtual bool IsEmpty(CLayerTiles *pLayer);
	virtual void BrushSelecting(CUIRect Rect);
	virtual int BrushGrab(CLayerGroup *pBrush, CUIRect Rect);
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect);
	virtual void BrushDraw(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);

	virtual void ShowInfo();
	virtual int RenderProperties(CUIRect *pToolbox);

	struct SCommonPropState
	{
		bool Modified = false;
		int Width = -1;
		int Height = -1;
		int Color = 0;
	};
	static int RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, array<CLayerTiles *> &pLayers);

	virtual void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc);
	virtual void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc);

	void PrepareForSave();

	void GetSize(float *w, float *h)
	{
		*w = m_Width * 32.0f;
		*h = m_Height * 32.0f;
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
	char m_aFileName[512];
};

class CLayerQuads : public CLayer
{
public:
	CLayerQuads();
	~CLayerQuads();

	virtual void Render(bool QuadPicker = false);
	CQuad *NewQuad(int x, int y, int Width, int Height);

	virtual void BrushSelecting(CUIRect Rect);
	virtual int BrushGrab(CLayerGroup *pBrush, CUIRect Rect);
	virtual void BrushPlace(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);

	virtual int RenderProperties(CUIRect *pToolbox);

	virtual void ModifyImageIndex(INDEX_MODIFY_FUNC pfnFunc);
	virtual void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc);

	void GetSize(float *w, float *h);

	int m_Image;
	array<CQuad> m_lQuads;
};

class CLayerGame : public CLayerTiles
{
public:
	CLayerGame(int w, int h);
	~CLayerGame();

	virtual CTile GetTile(int x, int y);
	virtual void SetTile(int x, int y, CTile tile);

	virtual int RenderProperties(CUIRect *pToolbox);
};

class CEditor : public IEditor
{
	class IInput *m_pInput;
	class IClient *m_pClient;
	class IConsole *m_pConsole;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class ISound *m_pSound;
	class IStorage *m_pStorage;
	CRenderTools m_RenderTools;
	CUI m_UI;

public:
	class IInput *Input() { return m_pInput; };
	class IClient *Client() { return m_pClient; };
	class IConsole *Console() { return m_pConsole; };
	class IGraphics *Graphics() { return m_pGraphics; };
	class ISound *Sound() { return m_pSound; }
	class ITextRender *TextRender() { return m_pTextRender; };
	class IStorage *Storage() { return m_pStorage; };
	CUI *UI() { return &m_UI; }
	CRenderTools *RenderTools() { return &m_RenderTools; }

	CEditor() :
		m_TilesetPicker(16, 16)
	{
		m_pInput = 0;
		m_pClient = 0;
		m_pGraphics = 0;
		m_pTextRender = 0;
		m_pSound = 0;

		m_Mode = MODE_LAYERS;
		m_Dialog = 0;
		m_EditBoxActive = 0;
		m_pTooltip = 0;

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
		m_pFileDialogTitle = 0;
		m_pFileDialogButtonText = 0;
		m_pFileDialogUser = 0;
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

		m_ShowTileInfo = false;
		m_ShowDetail = true;
		m_Animate = false;
		m_AnimateStart = 0;
		m_AnimateTime = 0;
		m_AnimateSpeed = 1;

		m_ShowEnvelopeEditor = 0;
		m_ShowUndo = 0;
		m_UndoScrollValue = 0.0f;
		m_ShowServerSettingsEditor = false;

		m_ShowEnvelopePreview = 0;
		m_SelectedQuadEnvelope = -1;
		m_SelectedEnvelopePoint = -1;

		m_CommandBox = 0.0f;
		m_aSettingsCommand[0] = 0;

		ms_pUiGotContext = 0;

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

		m_Mentions = 0;
	}

	virtual void Init();
	virtual void UpdateAndRender();
	virtual bool HasUnsavedData() { return m_Map.m_Modified; }
	virtual void UpdateMentions() { m_Mentions++; }
	virtual void ResetMentions() { m_Mentions = 0; }

	int64 m_LastUndoUpdateTime;
	bool m_UndoRunning;
	void CreateUndoStep();
	static void CreateUndoStepThread(void *pUser);
	int UndoStep();
	struct CUndo
	{
		int m_FileNum;
		int m_ButtonId;
		char m_aName[128];
		IGraphics::CTextureHandle m_PreviewImage;
		bool m_PreviewImageIsLoaded;
	};
	array<CUndo> m_lUndoSteps;
	bool m_Undo;
	int m_ShowUndo;
	float m_UndoScrollValue;

	CLayerGroup *m_apSavedBrushes[10];

	void FilelistPopulate(int StorageType);
	void InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
		const char *pBasepath, const char *pDefaultName,
		void (*pfnFunc)(const char *pFilename, int StorageType, void *pUser), void *pUser);

	void Reset(bool CreateDefault = true);
	virtual int Save(const char *pFilename);
	virtual int Load(const char *pFilename, int StorageType);
	int Append(const char *pFilename, int StorageType);
	void LoadCurrentMap();
	void Render();

	array<CQuad *> GetSelectedQuads();
	CLayer *GetSelectedLayerType(int Index, int Type);
	CLayer *GetSelectedLayer(int Index);
	CLayerGroup *GetSelectedGroup();
	CSoundSource *GetSelectedSource();
	void SelectLayer(int LayerIndex, int GroupIndex = -1);
	void SelectQuad(int Index);
	void DeleteSelectedQuads();
	bool IsQuadSelected(int Index);
	int FindSelectedQuadIndex(int Index);

	float ScaleFontSize(char *aBuf, float FontSize, int Width);
	int DoProperties(CUIRect *pToolbox, CProperty *pProps, int *pIDs, int *pNewVal, ColorRGBA Color = ColorRGBA(1, 1, 1, 0.5f));

	int m_Mode;
	int m_Dialog;
	int m_EditBoxActive;
	const char *m_pTooltip;

	bool m_GridActive;
	int m_GridFactor;

	bool m_BrushColorEnabled;

	char m_aFileName[512];
	char m_aFileSaveName[512];
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
	char m_aFileDialogFileName[MAX_PATH_LENGTH];
	char m_aFileDialogCurrentFolder[MAX_PATH_LENGTH];
	char m_aFileDialogCurrentLink[MAX_PATH_LENGTH];
	char m_aFileDialogSearchText[64];
	char m_aFileDialogPrevSearchText[64];
	char *m_pFileDialogPath;
	bool m_FileDialogActivate;
	int m_FileDialogFileType;
	float m_FileDialogScrollValue;
	int m_FilesSelectedIndex;
	char m_FileDialogNewFolderName[64];
	char m_FileDialogErrString[64];
	IGraphics::CTextureHandle m_FilePreviewImage;
	bool m_PreviewImageIsLoaded;
	CImageInfo m_FilePreviewImageInfo;
	bool m_FileDialogOpening;

	struct CFilelistItem
	{
		char m_aFilename[128];
		char m_aName[128];
		bool m_IsDir;
		bool m_IsLink;
		int m_StorageType;
		bool m_IsVisible;

		bool operator<(const CFilelistItem &Other) { return !str_comp(m_aFilename, "..") ? true : !str_comp(Other.m_aFilename, "..") ? false : m_IsDir && !Other.m_IsDir ? true : !m_IsDir && Other.m_IsDir ? false : str_comp_nocase(m_aFilename, Other.m_aFilename) < 0; }
	};
	sorted_array<CFilelistItem> m_FileList;
	int m_FilesStartAt;
	int m_FilesCur;
	int m_FilesStopAt;

	std::vector<std::string> m_SelectEntitiesFiles;
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
	float m_MouseDeltaX;
	float m_MouseDeltaY;
	float m_MouseDeltaWx;
	float m_MouseDeltaWy;

	bool m_ShowTileInfo;
	bool m_ShowDetail;
	bool m_Animate;
	int64 m_AnimateStart;
	float m_AnimateTime;
	float m_AnimateSpeed;

	int m_ShowEnvelopeEditor;
	int m_ShowEnvelopePreview; //Values: 0-Off|1-Selected Envelope|2-All
	bool m_ShowServerSettingsEditor;
	bool m_ShowPicker;

	array<int> m_lSelectedLayers;
	array<int> m_lSelectedQuads;
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

	IGraphics::CTextureHandle m_CheckerTexture;
	IGraphics::CTextureHandle m_BackgroundTexture;
	IGraphics::CTextureHandle m_CursorTexture;
	IGraphics::CTextureHandle m_EntitiesTexture;

	CLayerGroup m_Brush;
	CLayerTiles m_TilesetPicker;
	CLayerQuads m_QuadsetPicker;

	static const void *ms_pUiGotContext;

	CEditorMap m_Map;
	int m_ShiftBy;

	static void EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser);

	float m_CommandBox;
	char m_aSettingsCommand[256];

	void PlaceBorderTiles();
	int DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA Color);

	int DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = 10.0f);
	int DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags = 0, const char *pToolTip = 0);

	int DoButton_ColorPicker(const void *pID, const CUIRect *pRect, ColorRGBA *pColor, const char *pToolTip = 0);

	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden = false, int Corners = CUI::CORNER_ALL);

	void RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness);

	void RenderGrid(CLayerGroup *pGroup);

	void UiInvokePopupMenu(void *pID, int Flags, float X, float Y, float W, float H, int (*pfnFunc)(CEditor *pEditor, CUIRect Rect, void *pContext), void *pExtra = 0);
	void UiDoPopupMenu();
	bool UiPopupExists(void *pID);
	bool UiPopupOpen();

	int UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree = false, bool IsHex = false, int corners = CUI::CORNER_ALL, ColorRGBA *Color = 0);

	static int PopupGroup(CEditor *pEditor, CUIRect View, void *pContext);

	struct CLayerPopupContext
	{
		array<CLayerTiles *> m_aLayers;
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

	float ButtonColorMul(const void *pID);

	void DoQuadEnvelopes(const array<CQuad> &m_lQuads, IGraphics::CTextureHandle Texture = IGraphics::CTextureHandle());
	void DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int pIndex);
	void DoQuadPoint(CQuad *pQuad, int QuadIndex, int v);

	void DoSoundSource(CSoundSource *pSource, int Index);

	void DoMapEditor(CUIRect View);
	void DoToolbar(CUIRect Toolbar);
	void DoQuad(CQuad *pQuad, int Index);
	float UiDoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	ColorRGBA GetButtonColor(const void *pID, int Checked);

	static void ReplaceImage(const char *pFilename, int StorageType, void *pUser);
	static void ReplaceSound(const char *pFileName, int StorageType, void *pUser);
	static void AddImage(const char *pFilename, int StorageType, void *pUser);
	static void AddSound(const char *pFileName, int StorageType, void *pUser);

	bool IsEnvelopeUsed(int EnvelopeIndex);

	void RenderImages(CUIRect Toolbox, CUIRect View);
	void RenderLayers(CUIRect Toolbox, CUIRect View);
	void RenderSounds(CUIRect Toolbox, CUIRect View);
	void RenderModebar(CUIRect View);
	void RenderStatusbar(CUIRect View);
	void RenderEnvelopeEditor(CUIRect View);
	void RenderUndoList(CUIRect View);
	void RenderServerSettingsEditor(CUIRect View);

	void RenderMenubar(CUIRect Menubar);
	void RenderFileDialog();

	void AddFileDialogEntry(int Index, CUIRect *pView);
	void SelectGameLayer();
	void SortImages();
	const char *Explain(int Tile, int Layer);

	int GetLineDistance();
	void ZoomMouseTarget(float ZoomFactor);

	static ColorHSVA ms_PickerColor;
	static int ms_SVPicker;
	static int ms_HuePicker;

	// DDRace

	IGraphics::CTextureHandle m_FrontTexture;
	IGraphics::CTextureHandle m_TeleTexture;
	IGraphics::CTextureHandle m_SpeedupTexture;
	IGraphics::CTextureHandle m_SwitchTexture;
	IGraphics::CTextureHandle m_TuneTexture;
	static int PopupTele(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSpeedup(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupSwitch(CEditor *pEditor, CUIRect View, void *pContext);
	static int PopupTune(CEditor *pEditor, CUIRect View, void *pContext);
	unsigned char m_TeleNumber;

	unsigned char m_TuningNum;

	unsigned char m_SpeedupForce;
	unsigned char m_SpeedupMaxSpeed;
	short m_SpeedupAngle;

	unsigned char m_SwitchNum;
	unsigned char m_SwitchDelay;
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

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);
	virtual bool IsEmpty(CLayerTiles *pLayer);
	virtual void BrushDraw(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect);
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

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);
	virtual bool IsEmpty(CLayerTiles *pLayer);
	virtual void BrushDraw(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect);
};

class CLayerFront : public CLayerTiles
{
public:
	CLayerFront(int w, int h);

	virtual void Resize(int NewW, int NewH);
	virtual void SetTile(int x, int y, CTile tile);
};

class CLayerSwitch : public CLayerTiles
{
public:
	CLayerSwitch(int w, int h);
	~CLayerSwitch();

	CSwitchTile *m_pSwitchTile;
	unsigned char m_SwitchNumber;
	unsigned char m_SwitchDelay;

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);
	virtual bool IsEmpty(CLayerTiles *pLayer);
	virtual void BrushDraw(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect);
	virtual bool ContainsElementWithId(int Id);
};

class CLayerTune : public CLayerTiles
{
public:
	CLayerTune(int w, int h);
	~CLayerTune();

	CTuneTile *m_pTuneTile;
	unsigned char m_TuningNumber;

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);
	virtual bool IsEmpty(CLayerTiles *pLayer);
	virtual void BrushDraw(CLayer *pBrush, float wx, float wy);
	virtual void BrushFlipX();
	virtual void BrushFlipY();
	virtual void BrushRotate(float Amount);
	virtual void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect);
};

class CLayerSounds : public CLayer
{
public:
	CLayerSounds();
	~CLayerSounds();

	virtual void Render(bool Tileset = false);
	CSoundSource *NewSource(int x, int y);

	virtual void BrushSelecting(CUIRect Rect);
	virtual int BrushGrab(CLayerGroup *pBrush, CUIRect Rect);
	virtual void BrushPlace(CLayer *pBrush, float wx, float wy);

	virtual int RenderProperties(CUIRect *pToolbox);

	virtual void ModifyEnvelopeIndex(INDEX_MODIFY_FUNC pfnFunc);
	virtual void ModifySoundIndex(INDEX_MODIFY_FUNC pfnFunc);

	int m_Sound;
	array<CSoundSource> m_lSources;
};

#endif
