#ifndef GAME_SERVER_ENTITIES_VISUAL_QUAD_H
#define GAME_SERVER_ENTITIES_VISUAL_QUAD_H

#include <game/server/entity.h>
#include <game/visual_primitives.h>

class CVisualQuad : public CEntity
{
public:
	CVisualQuad(CGameWorld *pGameWorld, vec2 Pos, int W, int H,
		int Angle, int Color, int Width = 0, int ImageIndex = -1,
		int Flags = VISUALFLAG_FILLED,
		int RenderOrder = RENDERORDER_DEFAULT,
		int Lifetime = -1, CClientMask Mask = CClientMask().set());

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetColor(int Color) { m_Color = Color; }
	void SetAngle(int Angle) { m_Angle = Angle; }

private:
	int m_W;
	int m_H;
	int m_Angle;
	int m_Color;
	int m_VisualWidth;
	int m_ImageIndex;
	int m_VisualFlags;
	int m_RenderOrder;
	int m_Lifetime;
	CClientMask m_Mask;
};

#endif
