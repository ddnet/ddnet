#ifndef GAME_EDITOR_MAPITEMS_LAYER_REDIRECT_H
#define GAME_EDITOR_MAPITEMS_LAYER_REDIRECT_H

#include "layer_tiles.h"

struct SRedirectTileStateChange
{
	bool m_Changed;
	struct SData
	{
		unsigned char m_Type;
		unsigned short m_Port;
		int m_Index;
	} m_Previous, m_Current;
};

class CLayerRedirect : public CLayerTiles
{
public:
	CLayerRedirect(CEditor *pEditor, int w, int h);
	CLayerRedirect(const CLayerRedirect &Other);
	~CLayerRedirect() override;

	CRedirectTile *m_pRedirectTile;
	short m_RedirectPort;

	void Resize(int NewW, int NewH) override;
	void Shift(int Direction) override;
	bool IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer) override;
	void BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos) override;
	void BrushFlipX() override;
	void BrushFlipY() override;
	void BrushRotate(float Amount) override;
	void FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect) override;

	EditorTileStateChangeHistory<SRedirectTileStateChange> m_History;
	void ClearHistory() override
	{
		CLayerTiles::ClearHistory();
		m_History.clear();
	}

	std::shared_ptr<CLayer> Duplicate() const override;
	const char *TypeName() const override;

private:
	void RecordStateChange(int x, int y, SRedirectTileStateChange::SData Previous, SRedirectTileStateChange::SData Current);
};

#endif
