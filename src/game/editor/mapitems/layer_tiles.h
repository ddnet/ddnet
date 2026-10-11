#ifndef GAME_EDITOR_MAPITEMS_LAYER_TILES_H
#define GAME_EDITOR_MAPITEMS_LAYER_TILES_H

#include "layer.h"

#include <base/dbg.h>
#include <base/mem.h>

#include <game/editor/enums.h>
#include <game/editor/history/tile_projection.h>
#include <game/editor/mapitems.h>
#include <game/map/render_map.h>

#include <map>

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

class CIntRect
{
public:
	int x, y;
	int w, h;
};

class CLayerTiles : public CLayer, public CLayerTilesValues
{
protected:
	template<typename T>
	void ShiftImpl(CEditorTilePlane<T> &Tiles, EShiftDirection Direction, int ShiftBy)
	{
		switch(Direction)
		{
		case EShiftDirection::LEFT: Tiles.Shift(-ShiftBy, 0); break;
		case EShiftDirection::RIGHT: Tiles.Shift(ShiftBy, 0); break;
		case EShiftDirection::UP: Tiles.Shift(0, -ShiftBy); break;
		case EShiftDirection::DOWN: Tiles.Shift(0, ShiftBy); break;
		default: dbg_assert_failed("Direction invalid: %d", (int)Direction);
		}
	}

public:
	CLayerTiles(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId = 0);
	CLayerTiles(const CLayerTiles &Other);
	~CLayerTiles() override;

	[[nodiscard]] virtual CTile GetTile(int x, int y) const;
	virtual void SetTile(int x, int y, CTile Tile);

	virtual void Resize(int NewW, int NewH);
	virtual void Shift(EShiftDirection Direction);

	void MakePalette();
	void Render(const CEditorMap *pRenderMap) override;

	int ConvertX(float x) const;
	int ConvertY(float y) const;
	void Convert(CUIRect Rect, CIntRect *pOut) const;
	void Snap(CUIRect *pRect) const;
	void Clamp(CIntRect *pRect) const;

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
	static CUi::EPopupMenuFunctionResult RenderCommonProperties(SCommonPropState &State, CEditorMap *pEditorMap, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers, std::vector<int> &vLayerIndices);

	bool IsEnvelopeUsed(int EnvelopeIndex) const override;
	bool IsImageUsed(int ImageIndex) const override;

	void VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction) override;
	void VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction) override;

	std::vector<CTile> TilesForSave() const;
	CRenderTileSource<CTile> TilesForRender(const CEditorMap *pRenderMap);

	void GetSize(float *pWidth, float *pHeight) override
	{
		*pWidth = Width() * 32.0f;
		*pHeight = Height() * 32.0f;
	}

	void FlagModified(int x, int y, int w, int h);

	// DDRace

	char m_aFilename[IO_MAX_PATH_LENGTH]{};
	bool m_KnownTextModeLayer = false;
	bool m_RenderOverlays = true;

	static bool HasAutomapEffect(ETilesProp Prop);

protected:
	void ShowPreventUnusedTilesWarning();

	friend class CAutomapper;
};

#endif
