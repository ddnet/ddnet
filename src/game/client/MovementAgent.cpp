//
// Created by danii on 09.09.2023.
//

#include "MovementAgent.h"
#include "base/vmath.h"
#include "game/client/gameclient.h"
#include <engine/shared/http.h>

void MovementAgent::moveTo(vec2 position)
{
	CGameClient::CClientData localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];
	path = GameClient()->map.aStar(floor(localClientData.m_Predicted.m_Pos / 32), floor(position / 32));

	this->moveToPosition = position;
	this->isMoving = true;
}

void MovementAgent::OnUpdate()
{
	if (!isMoving) {
		return;
	}

	CNetObj_PlayerInput* input = &this->m_pClient->pythonController.inputs[g_Config.m_ClDummy];
	CGameClient::CClientData localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];

	ivec2 startMapPosition = floor(localClientData.m_Predicted.m_Pos / 32);
	ivec2 endMapPosition = floor(this->moveToPosition / 32);

	if (GameClient()->map.canWalk(startMapPosition, endMapPosition)) {
		this->moveToOnGround(this->moveToPosition);
	} else if (GameClient()->map.canFallFromGround(startMapPosition, endMapPosition)) {
		this->moveToOnGround(this->moveToPosition);
	} else if (GameClient()->map.canFall(startMapPosition, endMapPosition)) {
		this->moveToOnGround(this->moveToPosition);
	} else if (GameClient()->map.canJumpOnWall(startMapPosition, endMapPosition)) {
		this->moveToOnGround(this->moveToPosition);
		input->m_Jump = 1;
	} else {
		input->m_Direction = 0;
		input->m_Jump = 0;
	}
}

void MovementAgent::moveToOnGround(vec2 position)
{
	CGameClient::CClientData localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];
	CNetObj_PlayerInput* input = &this->m_pClient->pythonController.inputs[g_Config.m_ClDummy];

	if (localClientData.m_Predicted.m_Pos.x + localClientData.m_Predicted.m_Vel.x - 4 > position.x) {
		input->m_Direction = -1;
	} else if (localClientData.m_Predicted.m_Pos.x + localClientData.m_Predicted.m_Vel.x + 4 < position.x) {
		input->m_Direction = 1;
	} else {
		input->m_Direction = 0;
	}
}

void MovementAgent::OnRender()
{
	if (Client()->State() != IClient::STATE_ONLINE) {
		return;
	}

	UI()->MapScreen();
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	Node* lastNode = nullptr;

	for (auto node : path) {
		if (node->x < 0 || node->y < 0) {
			continue;
		}

		if (lastNode == nullptr) {
			lastNode = node;
			continue;
		}
		vec2 lastPos = GameClient()->map.convertMapToUI(vec2(lastNode->x + 0.5, lastNode->y + 0.5));
		vec2 currentPos = GameClient()->map.convertMapToUI(vec2(node->x + 0.5, node->y + 0.5));

		IGraphics::CLineItem lineItem(lastPos.x, lastPos.y, currentPos.x, currentPos.y);
		Graphics()->LinesDraw(&lineItem, 1);
		lastNode = node;
	}

	Graphics()->LinesEnd();
}
