/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#define GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#include <base/color.h>
#include <base/vmath.h>
#include <game/client/component.h>

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
		MAX_ITEMS = 64,
	};

	CItem m_aItems[MAX_ITEMS];
	int m_NumItems;

	int m_DmgIndQuadContainerIndex;

public:
	CDamageInd();
	virtual int Sizeof() const override { return sizeof(*this); }

	void Create(vec2 Pos, vec2 Dir, float Alpha);
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnInit() override;
};
#endif
