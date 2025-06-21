#ifndef GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H
#define GAME_EDITOR_MAPITEMS_LAYER_SWITCH_H

#include "layer_tiles.h"

struct SSwitchTileStateChange
{
	bool m_Changed;
	struct SData
	{
		int m_Number;
		int m_Type;
		int m_Flags;
		int m_Delay;
		int m_Index;
	} m_Previous, m_Current;
};

class CLayerSwitch : public CLayerTiles
{
public:
	CLayerSwitch(CEditorMap *pMap, int w, int h);
	CLayerSwitch(const CLayerSwitch &Other);
	~CLayerSwitch() override;

	CSwitchTile *m_pSwitchTile;
	unsigned char m_SwitchNumber;
	unsigned char m_SwitchDelay;

	void Resize(int NewW, int NewH) override;
	void Shift(EShiftDirection Direction) override;
	[[nodiscard]] bool IsEmpty() const override;
	void BrushDraw(CLayer *pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect) override;
	int FindNextFreeNumber() const;
	bool ContainsElementWithId(int Id) const;
	void GetPos(int Number, int Offset, ivec2 &SwitchPos);

	int m_GotoSwitchOffset;
	ivec2 m_GotoSwitchLastPos;

	EditorTileStateChangeHistory<SSwitchTileStateChange> m_History;
	void ClearHistory() override
	{
		CLayerTiles::ClearHistory();
		m_History.clear();
	}

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	void RecordStateChange(int x, int y, SSwitchTileStateChange::SData Previous, SSwitchTileStateChange::SData Current);
};

#endif
