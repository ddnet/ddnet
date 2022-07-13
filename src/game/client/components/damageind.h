/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#define GAME_CLIENT_COMPONENTS_DAMAGEIND_H
#include <base/vmath.h>
#include <game/client/component.h>

class CDamageInd : public CComponent
{
	int64_t m_Lastupdate = 0;
	struct CItem
	{
		vec2 m_Pos;
		vec2 m_Dir;
		float m_StartTime = 0;
		float m_StartAngle = 0;
	};

	enum
	{
		MAX_ITEMS = 64,
	};

	CItem m_aItems[MAX_ITEMS];
	int m_NumItems = 0;

	CItem *CreateI();
	void DestroyI(CItem *pItem);

	int m_DmgIndQuadContainerIndex = 0;

public:
	CDamageInd();
	virtual int Sizeof() const override { return sizeof(*this); }

	void Create(vec2 Pos, vec2 Dir);
	void Reset();
	virtual void OnRender() override;
	virtual void OnInit() override;
};
#endif
