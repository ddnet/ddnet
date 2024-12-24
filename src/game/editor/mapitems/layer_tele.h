#ifndef GAME_EDITOR_MAPITEMS_LAYER_TELE_H
#define GAME_EDITOR_MAPITEMS_LAYER_TELE_H

#include "layer_tiles.h"

struct STeleTileStateChange
{
	bool m_Changed;
	struct SData
	{
		int m_Number;
		int m_Type;
		int m_Index;
	} m_Previous, m_Current;
};

class CLayerTele : public CLayerTiles
{
public:
	CLayerTele(CEditor *pEditor, int w, int h);
	CLayerTele(const CLayerTele &Other);
	~CLayerTele();

	CTeleTile *m_pTeleTile;
	unsigned char m_TeleNum;
	unsigned char m_TeleCheckpointNum;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;
	virtual bool ContainsElementWithId(int Id, bool Checkpoint);
	virtual void GetPos(int Number, int Offset, int &TeleX, int &TeleY);

	int m_GotoTeleOffset;
	ivec2 m_GotoTeleLastPos;

	EditorTileStateChangeHistory<STeleTileStateChange> m_History;
	inline void ClearHistory() override
	{
		CLayerTiles::ClearHistory();
		m_History.clear();
	}

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	void RecordStateChange(int x, int y, STeleTileStateChange::SData Previous, STeleTileStateChange::SData Current);

	friend class CLayerTiles;
};

#endif
