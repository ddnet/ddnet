#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H

#include <game/client/component.h>

class CChillerBotUX : public CComponent
{
	bool m_IsNearFinish;

	virtual void OnTick();
};

#endif
