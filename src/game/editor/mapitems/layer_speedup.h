#ifndef GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_SPEEDUP_H

#include "layer_tiles.h"

struct SSpeedupTileStateChange
{
	bool m_Changed;
	struct SData
	{
		int m_Force;
		int m_Angle;
		int m_MaxSpeed;
		int m_Type;
		int m_Index;
	} m_Previous, m_Current;
};

class CLayerSpeedup : public CLayerTiles
{
public:
	CLayerSpeedup(CEditor *pEditor, int w, int h);
	CLayerSpeedup(const CLayerSpeedup &Other);
	~CLayerSpeedup();

	CSpeedupTile *m_pSpeedupTile;
	int m_SpeedupForce;
	int m_SpeedupMaxSpeed;
	int m_SpeedupAngle;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;

	EditorTileStateChangeHistory<SSpeedupTileStateChange> m_History;
	void ClearHistory() override
	{
		CLayerTiles::ClearHistory();
		m_History.clear();
	}

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	void RecordStateChange(int x, int y, SSpeedupTileStateChange::SData Previous, SSpeedupTileStateChange::SData Current);
};

#endif
