/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_EDITOR_H
#define GAME_EDITOR_EDITOR_H

#include <base/bezier.h>
#include <base/system.h>

#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include "auto_map.h"
#include "map_view.h"
#include "smooth_value.h"

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono_literals;

typedef std::function<void(int *pIndex)> FIndexModifyFunction;

// CEditor SPECIFIC
enum
{
	MODE_LAYERS = 0,
	MODE_IMAGES,
	MODE_SOUNDS,
	NUM_MODES,

	DIALOG_NONE = 0,
	DIALOG_FILE,
};

class CEnvelope
{
	class CEnvelopePointAccess : public IEnvelopePointAccess
	{
		std::vector<CEnvPoint_runtime> *m_pvPoints;

	public:
		CEnvelopePointAccess(std::vector<CEnvPoint_runtime> *pvPoints)
		{
			m_pvPoints = pvPoints;
		}

		int NumPoints() const override
		{
			return m_pvPoints->size();
		}

		const CEnvPoint *GetPoint(int Index) const override
		{
			if(Index < 0 || (size_t)Index >= m_pvPoints->size())
				return nullptr;
			return &m_pvPoints->at(Index);
		}

		const CEnvPointBezier *GetBezier(int Index) const override
		{
			if(Index < 0 || (size_t)Index >= m_pvPoints->size())
				return nullptr;
			return &m_pvPoints->at(Index).m_Bezier;
		}
	};

	int m_Channels;

public:
	std::vector<CEnvPoint_runtime> m_vPoints;
	CEnvelopePointAccess m_PointsAccess;
	char m_aName[32];
	float m_Bottom, m_Top;
	bool m_Synchronized;

	CEnvelope(int Channels) :
		m_PointsAccess(&m_vPoints)
	{
		SetChannels(Channels);
		m_aName[0] = '\0';
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
		CEnvPoint_runtime *pPrevPoint = nullptr;
		for(auto &Point : m_vPoints)
		{
			for(int c = 0; c < m_Channels; c++)
			{
				if(ChannelMask & (1 << c))
				{
					{
						// value handle
						const float v = fx2f(Point.m_aValues[c]);
						m_Top = maximum(m_Top, v);
						m_Bottom = minimum(m_Bottom, v);
					}

					if(Point.m_Curvetype == CURVETYPE_BEZIER)
					{
						// out-tangent handle
						const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aOutTangentDeltaY[c]);
						m_Top = maximum(m_Top, v);
						m_Bottom = minimum(m_Bottom, v);
					}

					if(pPrevPoint != nullptr && pPrevPoint->m_Curvetype == CURVETYPE_BEZIER)
					{
						// in-tangent handle
						const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aInTangentDeltaY[c]);
						m_Top = maximum(m_Top, v);
						m_Bottom = minimum(m_Bottom, v);
					}
				}
			}
			pPrevPoint = &Point;
		}
	}

	int Eval(float Time, ColorRGBA &Color)
	{
		CRenderTools::RenderEvalEnvelope(&m_PointsAccess, m_Channels, std::chrono::nanoseconds((int64_t)((double)Time * (double)std::chrono::nanoseconds(1s).count())), Color);
		return m_Channels;
	}

	void AddPoint(int Time, int v0, int v1 = 0, int v2 = 0, int v3 = 0)
	{
		CEnvPoint_runtime p;
		p.m_Time = Time;
		p.m_aValues[0] = v0;
		p.m_aValues[1] = v1;
		p.m_aValues[2] = v2;
		p.m_aValues[3] = v3;
		p.m_Curvetype = CURVETYPE_LINEAR;
		for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
		{
			p.m_Bezier.m_aInTangentDeltaX[c] = 0;
			p.m_Bezier.m_aInTangentDeltaY[c] = 0;
			p.m_Bezier.m_aOutTangentDeltaX[c] = 0;
			p.m_Bezier.m_aOutTangentDeltaY[c] = 0;
		}
		m_vPoints.push_back(p);
		Resort();
	}

	float EndTime() const
	{
		if(m_vPoints.empty())
			return 0.0f;
		return m_vPoints.back().m_Time / 1000.0f;
	}

	int GetChannels() const
	{
		return m_Channels;
	}

	void SetChannels(int Channels)
	{
		m_Channels = clamp<int>(Channels, 1, CEnvPoint::MAX_CHANNELS);
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
		str_copy(m_aName, "(invalid)");
		m_Visible = true;
		m_Readonly = false;
		m_Flags = 0;
		m_pEditor = nullptr;
	}

	CLayer(const CLayer &Other)
	{
		str_copy(m_aName, Other.m_aName);
		m_Flags = Other.m_Flags;
		m_pEditor = Other.m_pEditor;
		m_Type = Other.m_Type;
		m_Visible = true;
		m_Readonly = false;
	}

	virtual ~CLayer()
	{
	}

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) {}
	virtual void BrushDraw(std::shared_ptr<CLayer> pBrush, float x, float y) {}
	virtual void BrushPlace(std::shared_ptr<CLayer> pBrush, float x, float y) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual bool IsEntitiesLayer() const { return false; }

	virtual void Render(bool Tileset = false) {}
	virtual CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUI::POPUP_KEEP_OPEN; }

	virtual void ModifyImageIndex(FIndexModifyFunction pfnFunc) {}
	virtual void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) {}
	virtual void ModifySoundIndex(FIndexModifyFunction pfnFunc) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;

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
};

class CLayerGroup
{
public:
	class CEditorMap *m_pMap;

	std::vector<std::shared_ptr<CLayer>> m_vpLayers;

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
		return m_vpLayers.empty();
	}

	void OnEdited()
	{
		if(!m_CustomParallaxZoom)
			m_ParallaxZoom = GetParallaxZoomDefault(m_ParallaxX, m_ParallaxY);
	}

	void Clear()
	{
		m_vpLayers.clear();
	}

	void AddLayer(const std::shared_ptr<CLayer> &pLayer);

	void ModifyImageIndex(FIndexModifyFunction Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyImageIndex(Func);
	}

	void ModifyEnvelopeIndex(FIndexModifyFunction Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyEnvelopeIndex(Func);
	}

	void ModifySoundIndex(FIndexModifyFunction Func)
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
		m_Texture.Invalidate();
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
	char m_aName[IO_MAX_PATH_LENGTH];
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
	char m_aName[IO_MAX_PATH_LENGTH];

	void *m_pData;
	unsigned m_DataSize;
};

class CEditorMap
{
	void MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup);
	void MakeGameLayer(const std::shared_ptr<CLayer> &pLayer);

public:
	CEditor *m_pEditor;

	CEditorMap()
	{
		Clean();
	}

	~CEditorMap()
	{
		Clean();
	}

	bool m_Modified; // unsaved changes in manual save
	bool m_ModifiedAuto; // unsaved changes in autosave
	float m_LastModifiedTime;
	float m_LastSaveTime;
	float m_LastAutosaveUpdateTime;
	void OnModify();

	std::vector<std::shared_ptr<CLayerGroup>> m_vpGroups;
	std::vector<std::shared_ptr<CEditorImage>> m_vpImages;
	std::vector<std::shared_ptr<CEnvelope>> m_vpEnvelopes;
	std::vector<std::shared_ptr<CEditorSound>> m_vpSounds;

	class CMapInfo
	{
	public:
		char m_aAuthor[32];
		char m_aVersion[16];
		char m_aCredits[128];
		char m_aLicense[32];

		void Reset()
		{
			m_aAuthor[0] = '\0';
			m_aVersion[0] = '\0';
			m_aCredits[0] = '\0';
			m_aLicense[0] = '\0';
		}

		void Copy(const CMapInfo &Source)
		{
			str_copy(m_aAuthor, Source.m_aAuthor);
			str_copy(m_aVersion, Source.m_aVersion);
			str_copy(m_aCredits, Source.m_aCredits);
			str_copy(m_aLicense, Source.m_aLicense);
		}
	};
	CMapInfo m_MapInfo;
	CMapInfo m_MapInfoTmp;

	struct CSetting
	{
		char m_aCommand[256];

		CSetting(const char *pCommand)
		{
			str_copy(m_aCommand, pCommand);
		}
	};
	std::vector<CSetting> m_vSettings;

	std::shared_ptr<class CLayerGame> m_pGameLayer;
	std::shared_ptr<CLayerGroup> m_pGameGroup;

	std::shared_ptr<CEnvelope> NewEnvelope(int Channels)
	{
		OnModify();
		std::shared_ptr<CEnvelope> pEnv = std::make_shared<CEnvelope>(Channels);
		m_vpEnvelopes.push_back(pEnv);
		return pEnv;
	}

	void DeleteEnvelope(int Index);
	void SwapEnvelopes(int Index0, int Index1);
	template<typename F>
	void VisitEnvelopeReferences(F &&Visitor);

	std::shared_ptr<CLayerGroup> NewGroup()
	{
		OnModify();
		std::shared_ptr<CLayerGroup> pGroup = std::make_shared<CLayerGroup>();
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
		OnModify();
		std::swap(m_vpGroups[Index0], m_vpGroups[Index1]);
		return Index1;
	}

	void DeleteGroup(int Index)
	{
		if(Index < 0 || Index >= (int)m_vpGroups.size())
			return;
		OnModify();
		m_vpGroups.erase(m_vpGroups.begin() + Index);
	}

	void ModifyImageIndex(FIndexModifyFunction pfnFunc)
	{
		OnModify();
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifyImageIndex(pfnFunc);
	}

	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc)
	{
		OnModify();
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifyEnvelopeIndex(pfnFunc);
	}

	void ModifySoundIndex(FIndexModifyFunction pfnFunc)
	{
		OnModify();
		for(auto &pGroup : m_vpGroups)
			pGroup->ModifySoundIndex(pfnFunc);
	}

	void Clean();
	void CreateDefault(IGraphics::CTextureHandle EntitiesTexture);

	// io
	bool Save(const char *pFilename);
	bool Load(const char *pFilename, int StorageType, const std::function<void(const char *pErrorMessage)> &ErrorHandler);
	void PerformSanityChecks(const std::function<void(const char *pErrorMessage)> &ErrorHandler);

	// DDRace

	std::shared_ptr<class CLayerTele> m_pTeleLayer;
	std::shared_ptr<class CLayerSpeedup> m_pSpeedupLayer;
	std::shared_ptr<class CLayerFront> m_pFrontLayer;
	std::shared_ptr<class CLayerSwitch> m_pSwitchLayer;
	std::shared_ptr<class CLayerTune> m_pTuneLayer;
	void MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer);
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
	DIRECTION_LEFT = 0,
	DIRECTION_RIGHT,
	DIRECTION_UP,
	DIRECTION_DOWN,
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
			ShiftBy = minimum(ShiftBy, m_Width);
			for(int y = 0; y < m_Height; ++y)
			{
				if(ShiftBy < m_Width)
					mem_move(&pTiles[y * m_Width], &pTiles[y * m_Width + ShiftBy], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width + (m_Width - ShiftBy)], ShiftBy * sizeof(T));
			}
			break;
		case DIRECTION_RIGHT:
			ShiftBy = minimum(ShiftBy, m_Width);
			for(int y = 0; y < m_Height; ++y)
			{
				if(ShiftBy < m_Width)
					mem_move(&pTiles[y * m_Width + ShiftBy], &pTiles[y * m_Width], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width], ShiftBy * sizeof(T));
			}
			break;
		case DIRECTION_UP:
			ShiftBy = minimum(ShiftBy, m_Height);
			for(int y = ShiftBy; y < m_Height; ++y)
			{
				mem_copy(&pTiles[(y - ShiftBy) * m_Width], &pTiles[y * m_Width], m_Width * sizeof(T));
			}
			for(int y = m_Height - ShiftBy; y < m_Height; ++y)
			{
				mem_zero(&pTiles[y * m_Width], m_Width * sizeof(T));
			}
			break;
		case DIRECTION_DOWN:
			ShiftBy = minimum(ShiftBy, m_Height);
			for(int y = m_Height - ShiftBy - 1; y >= 0; --y)
			{
				mem_copy(&pTiles[(y + ShiftBy) * m_Width], &pTiles[y * m_Width], m_Width * sizeof(T));
			}
			for(int y = 0; y < ShiftBy; ++y)
			{
				mem_zero(&pTiles[y * m_Width], m_Width * sizeof(T));
			}
			break;
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
	virtual void SetTile(int x, int y, CTile Tile);

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);

	void MakePalette();
	void Render(bool Tileset = false) override;

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, RECTi *pOut);
	void Snap(CUIRect *pRect);
	void Clamp(RECTi *pRect);

	virtual bool IsEntitiesLayer() const override;

	virtual bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer);
	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	std::shared_ptr<CLayer> Duplicate() const override;

	virtual void ShowInfo();
	CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

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
	static CUI::EPopupMenuFunctionResult RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers);

	void ModifyImageIndex(FIndexModifyFunction pfnFunc) override;
	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) override;

	void PrepareForSave();
	void ExtractTiles(int TilemapItemVersion, const CTile *pSavedTiles, size_t SavedTilesSize);

	void GetSize(float *pWidth, float *pHeight) override
	{
		*pWidth = m_Width * 32.0f;
		*pHeight = m_Height * 32.0f;
	}

	void FlagModified(int x, int y, int w, int h);

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
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void BrushPlace(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	void ModifyImageIndex(FIndexModifyFunction pfnFunc) override;
	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) override;

	void GetSize(float *pWidth, float *pHeight) override;
	std::shared_ptr<CLayer> Duplicate() const override;

	int m_Image;
	std::vector<CQuad> m_vQuads;
};

class CLayerGame : public CLayerTiles
{
public:
	CLayerGame(int w, int h);
	~CLayerGame();

	CTile GetTile(int x, int y) override;
	void SetTile(int x, int y, CTile Tile) override;

	CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;
};

class CDataFileWriterFinishJob : public IJob
{
	char m_aRealFileName[IO_MAX_PATH_LENGTH];
	char m_aTempFileName[IO_MAX_PATH_LENGTH];
	CDataFileWriter m_Writer;

	void Run() override
	{
		m_Writer.Finish();
	}

public:
	CDataFileWriterFinishJob(const char *pRealFileName, const char *pTempFileName, CDataFileWriter &&Writer) :
		m_Writer(std::move(Writer))
	{
		str_copy(m_aRealFileName, pRealFileName);
		str_copy(m_aTempFileName, pTempFileName);
	}

	const char *GetRealFileName() const { return m_aRealFileName; }
	const char *GetTempFileName() const { return m_aTempFileName; }
};

class CEditor : public IEditor
{
	class IInput *m_pInput = nullptr;
	class IClient *m_pClient = nullptr;
	class CConfig *m_pConfig = nullptr;
	class IConsole *m_pConsole = nullptr;
	class IEngine *m_pEngine = nullptr;
	class IGraphics *m_pGraphics = nullptr;
	class ITextRender *m_pTextRender = nullptr;
	class ISound *m_pSound = nullptr;
	class IStorage *m_pStorage = nullptr;
	CRenderTools m_RenderTools;
	CUI m_UI;

	CMapView m_MapView;

	bool m_EditorWasUsedBefore = false;

	IGraphics::CTextureHandle m_EntitiesTexture;

	IGraphics::CTextureHandle m_FrontTexture;
	IGraphics::CTextureHandle m_TeleTexture;
	IGraphics::CTextureHandle m_SpeedupTexture;
	IGraphics::CTextureHandle m_SwitchTexture;
	IGraphics::CTextureHandle m_TuneTexture;

	int GetTextureUsageFlag();

	enum EPreviewState
	{
		PREVIEW_UNLOADED,
		PREVIEW_LOADED,
		PREVIEW_ERROR,
	};

	std::shared_ptr<CLayerGroup> m_apSavedBrushes[10];

public:
	class IInput *Input() { return m_pInput; }
	class IClient *Client() { return m_pClient; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IEngine *Engine() { return m_pEngine; }
	class IGraphics *Graphics() { return m_pGraphics; }
	class ISound *Sound() { return m_pSound; }
	class ITextRender *TextRender() { return m_pTextRender; }
	class IStorage *Storage() { return m_pStorage; }
	CUI *UI() { return &m_UI; }
	CRenderTools *RenderTools() { return &m_RenderTools; }

	CMapView *MapView() { return &m_MapView; }
	const CMapView *MapView() const { return &m_MapView; }

	CEditor() :
		m_ZoomEnvelopeX(1.0f, 0.1f, 600.0f),
		m_ZoomEnvelopeY(640.0f, 0.1f, 32000.0f)
	{
		m_EntitiesTexture.Invalidate();
		m_FrontTexture.Invalidate();
		m_TeleTexture.Invalidate();
		m_SpeedupTexture.Invalidate();
		m_SwitchTexture.Invalidate();
		m_TuneTexture.Invalidate();

		m_Mode = MODE_LAYERS;
		m_Dialog = 0;
		m_pTooltip = nullptr;

		m_GridActive = false;
		m_GridFactor = 1;

		m_BrushColorEnabled = true;

		m_aFileName[0] = '\0';
		m_aFileSaveName[0] = '\0';
		m_ValidSaveFilename = false;

		m_PopupEventActivated = false;
		m_PopupEventWasActivated = false;

		m_FileDialogStorageType = 0;
		m_FileDialogLastPopulatedStorageType = 0;
		m_pFileDialogTitle = nullptr;
		m_pFileDialogButtonText = nullptr;
		m_pFileDialogUser = nullptr;
		m_aFileDialogCurrentFolder[0] = '\0';
		m_aFileDialogCurrentLink[0] = '\0';
		m_aFilesSelectedName[0] = '\0';
		m_pFileDialogPath = m_aFileDialogCurrentFolder;
		m_FileDialogOpening = false;
		m_FilesSelectedIndex = -1;

		m_FilePreviewImage.Invalidate();
		m_FilePreviewSound = -1;
		m_FilePreviewState = PREVIEW_UNLOADED;

		m_SelectEntitiesImage = "DDNet";

		m_ResetZoomEnvelope = true;
		m_OffsetEnvelopeX = 0.1f;
		m_OffsetEnvelopeY = 0.5f;

		m_ShowMousePointer = true;
		m_MouseDeltaX = 0;
		m_MouseDeltaY = 0;
		m_MouseDeltaWx = 0;
		m_MouseDeltaWy = 0;

		m_GuiActive = true;
		m_ProofBorders = PROOF_BORDER_OFF;
		m_CurrentMenuProofIndex = 0;
		m_PreviewZoom = false;

		m_ShowTileInfo = SHOW_TILE_OFF;
		m_ShowDetail = true;
		m_Animate = false;
		m_AnimateStart = 0;
		m_AnimateTime = 0;
		m_AnimateSpeed = 1;

		m_ShowEnvelopePreview = SHOWENV_NONE;
		m_SelectedQuadEnvelope = -1;

		m_vSelectedEnvelopePoints = {};
		m_UpdateEnvPointInfo = false;
		m_SelectedTangentInPoint = std::pair(-1, -1);
		m_SelectedTangentOutPoint = std::pair(-1, -1);
		m_CurrentQuadIndex = -1;

		m_QuadKnifeActive = false;
		m_QuadKnifeCount = 0;

		m_CheckerTexture.Invalidate();
		m_BackgroundTexture.Invalidate();
		m_CursorTexture.Invalidate();

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
	}

	void Init() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnActivate() override;
	void OnWindowResize() override;
	bool HasUnsavedData() const override { return m_Map.m_Modified; }
	void UpdateMentions() override { m_Mentions++; }
	void ResetMentions() override { m_Mentions = 0; }
	void OnIngameMoved() override { m_IngameMoved = true; }
	void ResetIngameMoved() override { m_IngameMoved = false; }

	void HandleCursorMovement();
	void DispatchInputEvents();
	void HandleAutosave();
	bool PerformAutosave();
	void HandleWriterFinishJobs();

	void RefreshFilteredFileList();
	void FilelistPopulate(int StorageType, bool KeepSelection = false);
	void InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
		const char *pBasepath, bool FilenameAsDefault,
		bool (*pfnFunc)(const char *pFilename, int StorageType, void *pUser), void *pUser);
	struct SStringKeyComparator
	{
		bool operator()(char const *pLhs, char const *pRhs) const
		{
			return str_comp(pLhs, pRhs) < 0;
		}
	};
	std::map<const char *, CUI::SMessagePopupContext *, SStringKeyComparator> m_PopupMessageContexts;
	void ShowFileDialogError(const char *pFormat, ...)
		GNUC_ATTRIBUTE((format(printf, 2, 3)));

	void Reset(bool CreateDefault = true);
	bool Save(const char *pFilename) override;
	bool Load(const char *pFilename, int StorageType) override;
	bool Append(const char *pFilename, int StorageType);
	void LoadCurrentMap();
	void Render();

	void RenderPressedKeys(CUIRect View);
	void RenderSavingIndicator(CUIRect View);
	void FreeDynamicPopupMenus();
	void RenderMousePointer();

	void ResetMenuBackgroundPositions();

	std::vector<CQuad *> GetSelectedQuads();
	std::vector<std::pair<CQuad *, int>> GetSelectedQuadPoints();
	std::shared_ptr<CLayer> GetSelectedLayerType(int Index, int Type) const;
	std::shared_ptr<CLayer> GetSelectedLayer(int Index) const;
	std::shared_ptr<CLayerGroup> GetSelectedGroup() const;
	CSoundSource *GetSelectedSource();
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
	bool IsQuadPointSelected(int QuadIndex, int Index) const;
	int FindSelectedQuadIndex(int Index) const;
	int FindSelectedQuadPointIndex(int QuadIndex) const;

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

	int DoProperties(CUIRect *pToolbox, CProperty *pProps, int *pIDs, int *pNewVal, ColorRGBA Color = ColorRGBA(1, 1, 1, 0.5f));

	int m_Mode;
	int m_Dialog;
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
		POPEVENT_SAVE_COPY,
		POPEVENT_LARGELAYER,
		POPEVENT_PREVENTUNUSEDTILES,
		POPEVENT_IMAGEDIV16,
		POPEVENT_IMAGE_MAX,
		POPEVENT_PLACE_BORDER_TILES
	};

	int m_PopupEventType;
	int m_PopupEventActivated;
	int m_PopupEventWasActivated;
	bool m_LargeLayerWasWarned;
	bool m_PreventUnusedTilesWasWarned;
	int m_AllowPlaceUnusedTiles;
	bool m_BrushDrawDestructive;

	int m_Mentions = 0;
	bool m_IngameMoved = false;

	enum
	{
		FILETYPE_MAP,
		FILETYPE_IMG,
		FILETYPE_SOUND,
		NUM_FILETYPES
	};

	int m_FileDialogStorageType;
	int m_FileDialogLastPopulatedStorageType;
	const char *m_pFileDialogTitle;
	const char *m_pFileDialogButtonText;
	bool (*m_pfnFileDialogFunc)(const char *pFileName, int StorageType, void *pUser);
	void *m_pFileDialogUser;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_FileDialogFileNameInput;
	char m_aFileDialogCurrentFolder[IO_MAX_PATH_LENGTH];
	char m_aFileDialogCurrentLink[IO_MAX_PATH_LENGTH];
	char m_aFilesSelectedName[IO_MAX_PATH_LENGTH];
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_FileDialogFilterInput;
	char *m_pFileDialogPath;
	int m_FileDialogFileType;
	bool m_FileDialogMultipleStorages = false;
	bool m_FileDialogShowingRoot = false;
	int m_FilesSelectedIndex;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_FileDialogNewFolderNameInput;

	IGraphics::CTextureHandle m_FilePreviewImage;
	int m_FilePreviewSound;
	EPreviewState m_FilePreviewState;
	CImageInfo m_FilePreviewImageInfo;
	bool m_FileDialogOpening;

	struct CFilelistItem
	{
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aName[IO_MAX_PATH_LENGTH];
		bool m_IsDir;
		bool m_IsLink;
		int m_StorageType;
		time_t m_TimeModified;
	};
	std::vector<CFilelistItem> m_vCompleteFileList;
	std::vector<const CFilelistItem *> m_vpFilteredFileList;

	static bool CompareFilenameAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
	{
		if(str_comp(pLhs->m_aFilename, "..") == 0)
			return true;
		if(str_comp(pRhs->m_aFilename, "..") == 0)
			return false;
		if(pLhs->m_IsLink != pRhs->m_IsLink)
			return pLhs->m_IsLink;
		if(pLhs->m_IsDir != pRhs->m_IsDir)
			return pLhs->m_IsDir;
		return str_comp_filenames(pLhs->m_aName, pRhs->m_aName) < 0;
	}

	static bool CompareFilenameDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
	{
		if(str_comp(pLhs->m_aFilename, "..") == 0)
			return true;
		if(str_comp(pRhs->m_aFilename, "..") == 0)
			return false;
		if(pLhs->m_IsLink != pRhs->m_IsLink)
			return pLhs->m_IsLink;
		if(pLhs->m_IsDir != pRhs->m_IsDir)
			return pLhs->m_IsDir;
		return str_comp_filenames(pLhs->m_aName, pRhs->m_aName) > 0;
	}

	static bool CompareTimeModifiedAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
	{
		if(str_comp(pLhs->m_aFilename, "..") == 0)
			return true;
		if(str_comp(pRhs->m_aFilename, "..") == 0)
			return false;
		if(pLhs->m_IsLink || pRhs->m_IsLink)
			return pLhs->m_IsLink;
		if(pLhs->m_IsDir != pRhs->m_IsDir)
			return pLhs->m_IsDir;
		return pLhs->m_TimeModified < pRhs->m_TimeModified;
	}

	static bool CompareTimeModifiedDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
	{
		if(str_comp(pLhs->m_aFilename, "..") == 0)
			return true;
		if(str_comp(pRhs->m_aFilename, "..") == 0)
			return false;
		if(pLhs->m_IsLink || pRhs->m_IsLink)
			return pLhs->m_IsLink;
		if(pLhs->m_IsDir != pRhs->m_IsDir)
			return pLhs->m_IsDir;
		return pLhs->m_TimeModified > pRhs->m_TimeModified;
	}

	void SortFilteredFileList();
	int m_SortByFilename = 1;
	int m_SortByTimeModified = 0;

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

	enum EProofBorder
	{
		PROOF_BORDER_OFF,
		PROOF_BORDER_INGAME,
		PROOF_BORDER_MENU
	};

	EProofBorder m_ProofBorders;
	int m_CurrentMenuProofIndex;
	std::vector<vec2> m_vMenuBackgroundPositions;
	std::vector<const char *> m_vpMenuBackgroundPositionNames;
	std::vector<std::vector<int>> m_vMenuBackgroundCollisions;
	char m_aMenuBackgroundTooltip[256];
	bool m_PreviewZoom;
	float m_MouseWScale = 1.0f; // Mouse (i.e. UI) scale relative to the World (selected Group)
	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	float m_MouseWorldX = 0.0f;
	float m_MouseWorldY = 0.0f;
	float m_MouseWorldNoParaX = 0.0f;
	float m_MouseWorldNoParaY = 0.0f;
	float m_MouseDeltaX;
	float m_MouseDeltaY;
	float m_MouseDeltaWx;
	float m_MouseDeltaWy;

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

	enum EExtraEditor
	{
		EXTRAEDITOR_NONE = -1,
		EXTRAEDITOR_ENVELOPES,
		EXTRAEDITOR_SERVER_SETTINGS,
		NUM_EXTRAEDITORS,
	};
	EExtraEditor m_ActiveExtraEditor = EXTRAEDITOR_NONE;
	float m_aExtraEditorSplits[NUM_EXTRAEDITORS] = {250.0f, 250.0f};

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
	std::vector<std::pair<int, int>> m_vSelectedQuadPoints;
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

	std::vector<CQuad> m_vCopyBuffer;

	bool m_QuadKnifeActive;
	int m_QuadKnifeCount;
	vec2 m_aQuadKnifePoints[4];

	IGraphics::CTextureHandle m_CheckerTexture;
	IGraphics::CTextureHandle m_BackgroundTexture;
	IGraphics::CTextureHandle m_CursorTexture;

	IGraphics::CTextureHandle GetEntitiesTexture();

	std::shared_ptr<CLayerGroup> m_pBrush;
	std::shared_ptr<CLayerTiles> m_pTilesetPicker;
	std::shared_ptr<CLayerQuads> m_pQuadsetPicker;

	static const void *ms_pUiGotContext;

	CEditorMap m_Map;
	std::deque<std::shared_ptr<CDataFileWriterFinishJob>> m_WriterFinishJobs;

	int m_ShiftBy;

	static void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Channels, void *pUser);

	CLineInputBuffered<256> m_SettingsCommandInput;

	void PlaceBorderTiles();

	void UpdateTooltip(const void *pID, const CUIRect *pRect, const char *pToolTip);
	int DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Env(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA Color, int Corners);

	int DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = 10.0f);
	int DoButton_FontIcon(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize = 10.0f);
	int DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_File(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags = 0, const char *pToolTip = nullptr);

	int DoButton_DraggableEx(const void *pID, const char *pText, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted, int Flags, const char *pToolTip = nullptr, int Corners = IGraphics::CORNER_ALL, float FontSize = 10.0f);

	bool DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const char *pToolTip = nullptr);
	bool DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const char *pToolTip = nullptr);

	void RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness);

	void RenderGrid(const std::shared_ptr<CLayerGroup> &pGroup);
	void SnapToGrid(float &x, float &y);

	int UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree = false, bool IsHex = false, int corners = IGraphics::CORNER_ALL, ColorRGBA *pColor = nullptr, bool ShowValue = true);

	static CUI::EPopupMenuFunctionResult PopupMenuFile(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupMenuTools(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupMenuSettings(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupGroup(void *pContext, CUIRect View, bool Active);
	struct SLayerPopupContext : public SPopupMenuId
	{
		CEditor *m_pEditor;
		std::vector<std::shared_ptr<CLayerTiles>> m_vpLayers;
		CLayerTiles::SCommonPropState m_CommonPropState;
	};
	static CUI::EPopupMenuFunctionResult PopupLayer(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupQuad(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSource(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupPoint(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupEnvPoint(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupImage(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSound(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupNewFolder(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupMapInfo(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupEvent(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSelectImage(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSelectSound(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSelectGametileOp(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSelectConfigAutoMap(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupTele(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSpeedup(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSwitch(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupTune(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupGoto(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupEntities(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupProofMode(void *pContext, CUIRect View, bool Active);

	static bool CallbackOpenMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackAppendMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackSaveMap(const char *pFileName, int StorageType, void *pUser);
	static bool CallbackSaveCopyMap(const char *pFileName, int StorageType, void *pUser);

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
	void SetHotQuadPoint(const std::shared_ptr<CLayerQuads> &pLayer);

	float TriangleArea(vec2 A, vec2 B, vec2 C);
	bool IsInTriangle(vec2 Point, vec2 A, vec2 B, vec2 C);
	void DoQuadKnife(int QuadIndex);

	void DoSoundSource(CSoundSource *pSource, int Index);

	void DoMapEditor(CUIRect View);
	void DoToolbarLayers(CUIRect Toolbar);
	void DoToolbarSounds(CUIRect Toolbar);
	void DoQuad(CQuad *pQuad, int Index);
	ColorRGBA GetButtonColor(const void *pID, int Checked);

	bool ReplaceImage(const char *pFilename, int StorageType, bool CheckDuplicate);
	static bool ReplaceImageCallback(const char *pFilename, int StorageType, void *pUser);
	bool ReplaceSound(const char *pFileName, int StorageType, bool CheckDuplicate);
	static bool ReplaceSoundCallback(const char *pFileName, int StorageType, void *pUser);
	static bool AddImage(const char *pFilename, int StorageType, void *pUser);
	static bool AddSound(const char *pFileName, int StorageType, void *pUser);

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
	void RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast);

	void RenderExtraEditorDragBar(CUIRect View, CUIRect DragBar);

	void SetHotEnvelopePoint(const CUIRect &View, const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels);

	void RenderMenubar(CUIRect Menubar);
	void RenderFileDialog();

	void SelectGameLayer();
	void SortImages();
	bool SelectLayerByTile();

	// Tile Numbers For Explanations - TODO: Add/Improve tiles and explanations
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

	// Explanations
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
	float ScreenToEnvelopeDX(const CUIRect &View, float dx);
	float ScreenToEnvelopeDY(const CUIRect &View, float dy);

	// DDRace

	IGraphics::CTextureHandle GetFrontTexture();
	IGraphics::CTextureHandle GetTeleTexture();
	IGraphics::CTextureHandle GetSpeedupTexture();
	IGraphics::CTextureHandle GetSwitchTexture();
	IGraphics::CTextureHandle GetTuneTexture();

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

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
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
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
};

class CLayerFront : public CLayerTiles
{
public:
	CLayerFront(int w, int h);

	void Resize(int NewW, int NewH) override;
	void SetTile(int x, int y, CTile Tile) override;
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
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
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
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
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
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void BrushPlace(std::shared_ptr<CLayer> pBrush, float wx, float wy) override;

	CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) override;
	void ModifySoundIndex(FIndexModifyFunction pfnFunc) override;

	std::shared_ptr<CLayer> Duplicate() const override;

	int m_Sound;
	std::vector<CSoundSource> m_vSources;
};

#endif
