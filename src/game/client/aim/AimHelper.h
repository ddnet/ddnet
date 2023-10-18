//
// Created by danii on 04.09.2023.
//

#ifndef DDNET_IAIMHELPER_H
#define DDNET_IAIMHELPER_H

#include "base/vmath.h"
#include "game/client/component.h"

struct Line {
	vec2 startPoint;
	vec2 endPoint;

	Line(vec2 startPoint, vec2 endPoint)
	{
		this->startPoint = startPoint;
		this->endPoint = endPoint;
	}
};

class AimHelper : public CComponent
{
public:
	std::vector<int> getPlayers(bool includeLocalPlayer = false);
	std::vector<int> filterPlayersByTeam(std::vector<int> playersIds, int team);
	std::vector<int> filterPlayersByDistance(std::vector<int> playersIds, int maxDistance);

	std::vector<Line> predictLaserShoot(int shooterId, vec2 shootPoint, int &playerIdTakeLaser);
	//	bool canShootFromLaserToPlayer(CGameClient::CClientData player);

	virtual int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
protected:
	int getLocalClientId();
	bool IsActive(int ClientID);
};

#endif // DDNET_IAIMHELPER_H
