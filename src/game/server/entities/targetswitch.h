/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_TARGETSWITCH_H
#define GAME_SERVER_ENTITIES_TARGETSWITCH_H

#include <game/server/entity.h>

class CTargetSwitch : public CEntity
{
public:
	static const int ms_CollisionExtraSize = 6;

	CTargetSwitch(CGameWorld *pGameWorld, vec2 Pos, int Type, int Layer, int Number, int Flags, int Delay);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	int Type() const { return m_Type; }
	int Subtype() const { return m_Subtype; }
	void GetHit(int TeamHitFrom, bool Weakly = false);

private:
	int m_Type;
	int m_Subtype;
	int m_Flags;
	int m_Delay;

	void Move();
	vec2 m_Core;
};

#endif
