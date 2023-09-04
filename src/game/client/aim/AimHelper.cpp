//
// Created by danii on 04.09.2023.
//

#include "AimHelper.h"
#include "base/vmath.h"
#include "game/client/gameclient.h"

int* AimHelper::getPlayerIdsSortedByCursor()
{
	int ids[64];

	for (auto clientData : this->m_pClient->m_aClients) {
//		clientData->
	}

	return ids;
}

int AimHelper::getPlayerIdForLaserShootFng()
{
	CGameClient::CClientData localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];

	for (auto clientData : this->m_pClient->m_aClients) {
		if (clientData.m_Predicted.m_Id == this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]) {
			continue;
		}

		if (distance(clientData.m_Predicted.m_Pos, localClientData.m_Predicted.m_Pos) > localClientData.m_Predicted.m_Tuning.m_LaserReach) {
			continue;
		}

		if (clientData.m_Team == localClientData.m_Team) {
			continue;
		}

		this->m_pClient->m_Chat.AddLine(localClientData.m_Predicted.m_Id, 0, clientData.m_aName);
	}
	return 0;
}
