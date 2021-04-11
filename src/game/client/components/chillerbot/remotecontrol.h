#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_REMOTECONTROL_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_REMOTECONTROL_H

#include <game/client/component.h>

class CRemoteControl : public CComponent
{
	void OnChatMessage(int ClientID, int Team, const char *pMsg);

	virtual void OnMessage(int MsgType, void *pRawMsg);
};

#endif
