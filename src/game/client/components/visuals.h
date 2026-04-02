#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <vector>

#include <game/client/component.h>

struct CVisualItem
{
	int m_Type;
	const void *m_pData;
	int m_RenderOrder;
};

class CVisuals : public CComponent
{
	void RenderLine(const void *pData);
	void RenderCircle(const void *pData);
	void RenderQuad(const void *pData);
	void RenderTile(const void *pData);
	void RenderItem(const CVisualItem &Item);

	void SetColorFromPacked(int Color);
	void ApplyGroupScreenMapping(int GroupIndex);

	std::vector<CVisualItem> m_vVisualItems;
	int m_GameGroupIndex;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override;
};

#endif
