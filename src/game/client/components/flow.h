/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_FLOW_H
#define GAME_CLIENT_COMPONENTS_FLOW_H
#include <base/vmath.h>
#include <game/client/component.h>

class CFlow : public CComponent
{
	struct CCell
	{
		vec2 m_Vel;
	};

	CCell *m_pCells = nullptr;
	int m_Height = 0;
	int m_Width = 0;
	int m_Spacing = 0;

	void DbgRender();
	void Init();

public:
	CFlow();
	virtual int Sizeof() const override { return sizeof(*this); }

	vec2 Get(vec2 Pos);
	void Add(vec2 Pos, vec2 Vel, float Size);
	void Update();
};

#endif
