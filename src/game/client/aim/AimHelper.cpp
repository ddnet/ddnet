//
// Created by danii on 04.09.2023.
//

#include "AimHelper.h"
#include "base/vmath.h"
#include "game/client/gameclient.h"

int AimHelper::getLocalClientId()
{
	return this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy];
}

std::vector<int> AimHelper::getPlayers(bool includeLocalPlayer)
{
	std::vector<int> playersIds;

	for (auto player : this->m_pClient->m_aClients) {
		if (!IsActive(player.m_Predicted.m_Id)) {
			continue;
		}

		if (!includeLocalPlayer && player.m_Predicted.m_Id == this->getLocalClientId()) {
			continue;
		}

		playersIds.push_back(player.m_Predicted.m_Id);
	}

	return playersIds;
}

std::vector<int> AimHelper::filterPlayersByTeam(std::vector<int> playersIds, int team)
{
	std::vector<int> filteredPlayers;

	for (auto playerId : playersIds)
	{
		CGameClient::CClientData player = this->m_pClient->m_aClients[playerId];

		if (player.m_Team == team) {
			filteredPlayers.push_back(playerId);
		}
	}

	return filteredPlayers;
}

std::vector<int> AimHelper::filterPlayersByDistance(std::vector<int> playersIds, int maxDistance)
{
	CGameClient::CClientData localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];
	std::vector<int> filteredPlayers;

	for (auto playerId : playersIds)
	{
		CGameClient::CClientData player = this->m_pClient->m_aClients[playerId];

		if (distance(localClientData.m_Predicted.m_Pos, player.m_Predicted.m_Pos) <= maxDistance) {
			filteredPlayers.push_back(playerId);
		}
	}

	return filteredPlayers;
}

std::vector<Line> AimHelper::predictLaserShoot(int shooterId, vec2 shootPoint, int &playerIdTakeLaser)
{
	CGameClient::CClientData shooter = this->m_pClient->m_aClients[shooterId];
	vec2 shootDirection = normalize(shootPoint - shooter.m_Predicted.m_Pos);
	std::queue<vec2> shootPoints;
	vec2 startShootPoint = shooter.m_Predicted.m_Pos + shooter.m_Predicted.m_Vel;
	int bouncesLeft = shooter.m_Predicted.m_Tuning.m_LaserBounceNum * 2;
	int laserLengthLeft = shooter.m_Predicted.m_Tuning.m_LaserReach;
	std::vector<Line> laserLines;

	shootPoints.push(shootDirection * laserLengthLeft + startShootPoint);

	while (!shootPoints.empty() && bouncesLeft >= 0 && laserLengthLeft > 0) {
		vec2 toPoint = shootPoints.front();
		shootPoints.pop();
		bouncesLeft--;

		vec2 collisionPoint;
		int null;

		vec2 playerTakenShootPosition;
		auto playerId = GameClient()->IntersectCharacter(startShootPoint, toPoint, playerTakenShootPosition, shooterId);
		int intersectTile = Collision()->IntersectLineTeleWeapon(startShootPoint, toPoint, 0, &collisionPoint, &null);

		if (playerId >= 0 && distance(startShootPoint, playerTakenShootPosition) <= distance(startShootPoint, collisionPoint)) {
			auto playerTakeShoot = GameClient()->m_aClients[playerId];

			laserLines.push_back(Line(startShootPoint, playerTakenShootPosition));
			playerIdTakeLaser = playerId;

			break;
		}

		if (intersectTile == 0) {
			laserLines.push_back(Line(startShootPoint, toPoint));
			break;
		} else {
			laserLengthLeft -= distance(startShootPoint, collisionPoint);
			laserLines.push_back(Line(startShootPoint, collisionPoint));
			Collision()->MovePoint(&startShootPoint, &shootDirection, 1.0f, 0);
			shootPoints.push(shootDirection * laserLengthLeft + collisionPoint);
			startShootPoint = collisionPoint;
		}
	}

	return laserLines;
}

bool AimHelper::IsActive(int ClientID)
{
	return (GameClient()->m_aClients[ClientID].m_Active && GameClient()->m_Snap.m_aCharacters[ClientID].m_Active);
}

void AimHelper::OnRender()
{
	if (Client()->State() != IClient::STATE_ONLINE) {
		return;
	}
}