#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <unordered_map>
#include <vector>

struct CVisualItem
{
	int m_Type;
	int m_Id;
	const void *m_pData;
	const void *m_pPrevData;
	int m_RenderOrder;
	int m_Flags;
	int m_GroupIndex;
	int m_BatchKey; // <0 = lines mode, 0 = quads no texture, >0 = quads textured (imageIndex + 1)
};

struct CSpringState
{
	vec2 m_Pos;
	vec2 m_Vel;
	vec2 m_Pos2; // second point (e.g. To for lines)
	vec2 m_Vel2;
	float m_Angle;
	float m_AngleVel;
	unsigned m_Generation;
};

class CVisuals : public CComponent
{
	void RenderLine(const CVisualItem &Item);
	void RenderCircle(const CVisualItem &Item);
	void RenderQuad(const CVisualItem &Item);
	void RenderTile(const CVisualItem &Item);
	void RenderItem(const CVisualItem &Item);
	void RenderBatchedItems(const CVisualItem *pItems, int NumItems);

	vec2 SpringDamp(vec2 &Pos, vec2 &Vel, vec2 Target, float Dt, float Omega, float Decay);
	float SpringDampAngle(float &Angle, float &Vel, float Target, float Dt, float Omega, float Decay);
	CSpringState &FindOrCreateSpring(int SnapId, vec2 Pos, vec2 Pos2, float Angle);
	void CleanupSprings();

	IGraphics::CTextureHandle GetImageTexture(int ImageIndex);

	std::vector<CVisualItem> m_vVisualItems; // all items, sorted by (GroupIndex, RenderOrder, BatchKey)
	std::unordered_map<int, CSpringState> m_Springs;
	std::unordered_map<int, IGraphics::CTextureHandle> m_TexCache;
	int m_GameGroupIndex = -1;
	unsigned m_SpringGeneration = 0;

	// Per-frame cached values
	float m_IntraGameTick = 0;
	float m_RenderFrameTime = 0;
	vec2 m_CameraCenter = vec2(0, 0);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override;

	// Called by CMapRenderer callback after each group's layers are rendered
	void RenderForGroup(int GroupId);
};

#endif
