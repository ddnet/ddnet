/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#define GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <vector>

class CDamageInd : public CComponent
{
	struct CItem
	{
		vec2 m_Pos;
		vec2 m_Dir;
		float m_RemainingLife;
		float m_StartAngle;
		ColorRGBA m_Color;
	};

	enum
	{
		MAX_ITEMS = 1024,
	};

	std::vector<CItem> m_vItems;
	float m_ExpandSize;
	int m_DamageIndQuadContainerIndex;

public:
	int Sizeof() const override { return sizeof(*this); }

	void Create(vec2 Pos, vec2 Dir, float Alpha);
	void OnReset() override;
	void OnRender() override;
	void OnInit() override;
};
#endif
