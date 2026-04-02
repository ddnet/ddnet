#ifndef GAME_SERVER_ENTITIES_VISUAL_LINE_H
#define GAME_SERVER_ENTITIES_VISUAL_LINE_H

#include <game/server/entity.h>
#include <game/visual_primitives.h>

class CVisualLine : public CEntity
{
public:
	CVisualLine(CGameWorld *pGameWorld, vec2 From, vec2 To,
		int Color, int Width = 1, int Flags = 0,
		int RenderOrder = RENDERORDER_DEFAULT,
		int Lifetime = -1, CClientMask Mask = CClientMask().set());

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetFrom(vec2 From) { m_From = From; }
	void SetTo(vec2 To) { m_To = To; }
	void SetColor(int Color) { m_Color = Color; }
	void SetWidth(int Width) { m_Width = Width; }
	void SetFlags(int Flags) { m_VisualFlags = Flags; }

private:
	vec2 m_From;
	vec2 m_To;
	int m_Color;
	int m_Width;
	int m_VisualFlags;
	int m_RenderOrder;
	int m_Lifetime;
	CClientMask m_Mask;
};

#endif
