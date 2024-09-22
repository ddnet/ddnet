#ifndef GAME_EDITOR_MAPITEMS_LAYER_TUNE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TUNE_H

#include "layer_tiles.h"

struct STuneTileStateChange
{
	bool m_Changed;
	struct SData
	{
		int m_Number;
		int m_Type;
		int m_Index;
	} m_Previous, m_Current;
};

class CLayerTune : public CLayerTiles
{
public:
	CLayerTune(CEditor *pEditor, int w, int h);
	CLayerTune(const CLayerTune &Other);
	~CLayerTune();

	CTuneTile *m_pTuneTile;
	unsigned char m_TuningNumber;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;

	EditorTileStateChangeHistory<STuneTileStateChange> m_History;
	inline void ClearHistory() override
	{
		CLayerTiles::ClearHistory();
		m_History.clear();
	}

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	void RecordStateChange(int x, int y, STuneTileStateChange::SData Previous, STuneTileStateChange::SData Current);
};

#endif
