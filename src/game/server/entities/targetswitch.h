#ifndef GAME_SERVER_ENTITIES_TARGETSWITCH_H
#define GAME_SERVER_ENTITIES_TARGETSWITCH_H

#include <game/server/entity.h>

class CTargetSwitch : public CEntity
{
public:
	CTargetSwitch(CGameWorld *pGameWorld, vec2 Pos, int Type, int Layer, int Number, int Flags, int Delay);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	void GetHit(int TeamHitFrom, bool Weakly = false);

private:
	int m_Type;
	int m_Flags;
	int m_Delay;

	void Move();
	vec2 m_Core;
};

#endif
