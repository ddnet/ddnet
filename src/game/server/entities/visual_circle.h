#ifndef GAME_SERVER_ENTITIES_VISUAL_CIRCLE_H
#define GAME_SERVER_ENTITIES_VISUAL_CIRCLE_H

#include <game/server/entity.h>
#include <game/visual_primitives.h>

class CVisualCircle : public CEntity
{
public:
	CVisualCircle(CGameWorld *pGameWorld, vec2 Pos, int Radius,
		int Color, int Width = 1, int Flags = 0,
		int RenderOrder = RENDERORDER_DEFAULT,
		int Lifetime = -1, CClientMask Mask = CClientMask().set());

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetRadius(int Radius) { m_Radius = Radius; }
	void SetColor(int Color) { m_Color = Color; }
	void SetFlags(int Flags) { m_VisualFlags = Flags; }

private:
	int m_Radius;
	int m_Color;
	int m_Width;
	int m_VisualFlags;
	int m_RenderOrder;
	int m_Lifetime;
	CClientMask m_Mask;
};

#endif
