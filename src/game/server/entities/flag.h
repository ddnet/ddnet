/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_FLAG_H
#define GAME_SERVER_ENTITIES_FLAG_H

#include <game/server/entity.h>

class CFlag : public CEntity
{
public:
	static const int ms_PhysSize = 14;
	CCharacter *m_pCarrier;
	CCharacter *m_pLastCarrier;
	vec2 m_Vel;
	vec2 m_StandPos;
	bool m_IsGrounded;

	int m_Team;
	int m_AtStand;
	int m_DropTick;
	int m_GrabTick;

	CFlag(CGameWorld *pGameWorld, int Team);

	/* Getters */
	int GetTeam() const { return m_Team; }
	bool IsAtStand() const { return m_AtStand; }
	CCharacter *GetCarrier() const { return m_pCarrier; }
	int GetGrabTick() const { return m_GrabTick; }
	int GetDropTick() const { return m_DropTick; }

	/* Setters */
	void SetCarrier(CCharacter *pCarrier) { m_pCarrier = pCarrier; }

	/* CEntity functions */
	void Reset() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void TickDeferred() override;

	/* Functions */
	void Grab(class CCharacter *pChar);
	void Drop(vec2 Direction = vec2(0, 0));
};

#endif
