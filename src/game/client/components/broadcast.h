/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BROADCAST_H
#define GAME_CLIENT_COMPONENTS_BROADCAST_H
#include <game/client/component.h>

class CBroadcast : public CComponent
{
	// broadcasts
	char m_aBroadcastText[1024];
	int m_BroadcastTick;
	float m_BroadcastRenderOffset;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
};

#endif
