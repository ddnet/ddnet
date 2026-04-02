#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <unordered_map>
#include <vector>

#include <base/vmath.h>
#include <engine/graphics.h>

#include <game/client/component.h>

struct CVisualItem
{
	int m_Type;
	int m_Id;
	const void *m_pData;
	const void *m_pPrevData;
	int m_RenderOrder;
	int m_Flags;
};

struct CSpringState
{
	vec2 m_Pos;
	vec2 m_Vel;
	vec2 m_Pos2; // second point (e.g. To for lines)
	vec2 m_Vel2;
	float m_Angle;
	float m_AngleVel;
};

class CVisuals : public CComponent
{
	void RenderLine(const CVisualItem &Item);
	void RenderCircle(const CVisualItem &Item);
	void RenderQuad(const CVisualItem &Item);
	void RenderTile(const CVisualItem &Item);
	void RenderItem(const CVisualItem &Item);

	void ApplyGroupScreenMapping(int GroupIndex);

	vec2 SpringDamp(vec2 &Pos, vec2 &Vel, vec2 Target, float Dt, float Omega);
	float SpringDampAngle(float &Angle, float &Vel, float Target, float Dt, float Omega);
	CSpringState &FindOrCreateSpring(int SnapId, vec2 Pos, vec2 Pos2, float Angle);
	void CleanupSprings();

	IGraphics::CTextureHandle GetImageTexture(int ImageIndex);

	std::vector<CVisualItem> m_vVisualItems;
	std::unordered_map<int, CSpringState> m_Springs;
	std::unordered_map<int, IGraphics::CTextureHandle> m_TexCache;
	int m_GameGroupIndex;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override;
};

#endif
