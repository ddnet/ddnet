#ifndef GAME_CLIENT_PREDICTION_ENTITIES_TARGETSWITCH_H
#define GAME_CLIENT_PREDICTION_ENTITIES_TARGETSWITCH_H

#include <game/client/prediction/entity.h>

class CTargetSwitchData;

class CTargetSwitch : public CEntity
{
public:
	CTargetSwitch(CGameWorld *pGameWorld, int Id, const CNetObj_DDNetTargetSwitch *pData);

	bool Match(const CTargetSwitch *pTarget) const;
	void Read(const CNetObj_DDNetTargetSwitch *pData);

	void Reset();
	void Tick() override;

	void GetHit(int ClientId, bool Weakly = false, int ForcedTeam = -1);

private:
	int m_Type;
	int m_Flags;
	int m_Delay;

	void Move();
	vec2 m_Core;
};

#endif
