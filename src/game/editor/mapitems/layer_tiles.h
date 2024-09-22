#ifndef GAME_EDITOR_MAPITEMS_LAYER_TILES_H
#define GAME_EDITOR_MAPITEMS_LAYER_TILES_H

#include <game/editor/editor_trackers.h>
#include <game/editor/enums.h>
#include <map>

#include "layer.h"

struct STileStateChange
{
	bool m_Changed;
	CTile m_Previous;
	CTile m_Current;
};

template<typename T>
using EditorTileStateChangeHistory = std::map<int, std::map<int, T>>;

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
	CLayerTiles(CEditor *pEditor, int w, int h);
	CLayerTiles(const CLayerTiles &Other);
	~CLayerTiles();

	virtual CTile GetTile(int x, int y);
	virtual void SetTile(int x, int y, CTile Tile);
	void SetTileIgnoreHistory(int x, int y, CTile Tile) const;

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(int Direction);

	void MakePalette() const;
	void Render(bool Tileset = false) override;

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, RECTi *pOut) const;
	void Snap(CUIRect *pRect) const;
	void Clamp(RECTi *pRect) const;

	virtual bool IsEntitiesLayer() const override;

	virtual bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer);
	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
	void FillGameTiles(EGameTileOp Fill);
	bool CanFillGameTiles() const;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

	virtual void ShowInfo();
	CUi::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;

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
	static CUi::EPopupMenuFunctionResult RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers, std::vector<int> &vLayerIndices);

	void ModifyImageIndex(FIndexModifyFunction pfnFunc) override;
	void ModifyEnvelopeIndex(FIndexModifyFunction pfnFunc) override;

	void PrepareForSave();
	void ExtractTiles(int TilemapItemVersion, const CTile *pSavedTiles, size_t SavedTilesSize) const;

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

	EditorTileStateChangeHistory<STileStateChange> m_TilesHistory;
	inline virtual void ClearHistory() { m_TilesHistory.clear(); }

	static bool HasAutomapEffect(ETilesProp Prop);

protected:
	void RecordStateChange(int x, int y, CTile Previous, CTile Tile);

	void ShowPreventUnusedTilesWarning();

	friend class CAutoMapper;
};

#endif
