#ifndef GAME_EDITOR_MAPITEMS_LAYER_TILES_H
#define GAME_EDITOR_MAPITEMS_LAYER_TILES_H

#include "layer.h"

#include <game/editor/editor_trackers.h>
#include <game/editor/enums.h>

#include <map>

struct STileStateChange
{
	bool m_Changed;
	CTile m_Previous;
	CTile m_Current;
};

template<typename T>
using EditorTileStateChangeHistory = std::map<int, std::map<int, T>>;

/**
 * Represents a direction to shift a tile layer with the CLayerTiles::Shift function.
 * The underlying type is `int` as this is also used with the CEditor::DoPropertiesWithState function.
 */
enum class EShiftDirection : int
{
	LEFT,
	RIGHT,
	UP,
	DOWN,
};

struct RECTi
{
	int x, y;
	int w, h;
};

class CLayerTiles : public CLayer, public CMapItemLayerTilemap
{
protected:
	template<typename T>
	void ShiftImpl(T *pTiles, EShiftDirection Direction, int ShiftBy)
	{
		switch(Direction)
		{
		case EShiftDirection::LEFT:
			ShiftBy = minimum(ShiftBy, m_Width);
			for(int y = 0; y < m_Height; ++y)
			{
				if(ShiftBy < m_Width)
					mem_move(&pTiles[y * m_Width], &pTiles[y * m_Width + ShiftBy], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width + (m_Width - ShiftBy)], ShiftBy * sizeof(T));
			}
			break;
		case EShiftDirection::RIGHT:
			ShiftBy = minimum(ShiftBy, m_Width);
			for(int y = 0; y < m_Height; ++y)
			{
				if(ShiftBy < m_Width)
					mem_move(&pTiles[y * m_Width + ShiftBy], &pTiles[y * m_Width], (m_Width - ShiftBy) * sizeof(T));
				mem_zero(&pTiles[y * m_Width], ShiftBy * sizeof(T));
			}
			break;
		case EShiftDirection::UP:
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
		case EShiftDirection::DOWN:
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
		default:
			dbg_assert(false, "Direction invalid: %d", (int)Direction);
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
	~CLayerTiles() override;

	[[nodiscard]] virtual CTile GetTile(int x, int y) const;
	virtual void SetTile(int x, int y, CTile Tile);
	void SetTileIgnoreHistory(int x, int y, CTile Tile) const;

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(EShiftDirection Direction);

	void MakePalette() const;
	void Render(bool Tileset = false) override;

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, RECTi *pOut) const;
	void Snap(CUIRect *pRect) const;
	void Clamp(RECTi *pRect) const;

	bool IsEntitiesLayer() const override;

	[[nodiscard]] virtual bool IsEmpty() const;
	void BrushSelecting(CUIRect Rect) override;
	int BrushGrab(CLayerGroup *pBrush, CUIRect Rect) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	void FillGameTiles(EGameTileOp Fill);
	bool CanFillGameTiles() const;
	void BrushDraw(CLayer *pBrush, vec2 WorldPos) override;
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

	void ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction) override;
	void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction) override;

	void PrepareForSave();
	void ExtractTiles(int TilemapItemVersion, const CTile *pSavedTiles, size_t SavedTilesSize) const;

	void GetSize(float *pWidth, float *pHeight) override
	{
		*pWidth = m_Width * 32.0f;
		*pHeight = m_Height * 32.0f;
	}

	void FlagModified(int x, int y, int w, int h);

	bool m_HasGame;
	CTile *m_pTiles;

	// DDRace

	int m_FillGameTile = -1;
	bool m_LiveGameTiles = false;
	int m_AutoMapperConfig;
	int m_AutoMapperReference;
	int m_Seed;
	bool m_AutoAutoMap;
	bool m_HasTele;
	bool m_HasSpeedup;
	bool m_HasFront;
	bool m_HasSwitch;
	bool m_HasTune;
	char m_aFileName[IO_MAX_PATH_LENGTH];
	bool m_KnownTextModeLayer = false;

	EditorTileStateChangeHistory<STileStateChange> m_TilesHistory;
	virtual void ClearHistory() { m_TilesHistory.clear(); }

	static bool HasAutomapEffect(ETilesProp Prop);

protected:
	void RecordStateChange(int x, int y, CTile Previous, CTile Tile);

	void ShowPreventUnusedTilesWarning();

	friend class CAutoMapper;
};

#endif
