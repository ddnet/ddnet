/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BROADCAST_H
#define GAME_CLIENT_COMPONENTS_BROADCAST_H

#include <engine/textrender.h>
#include <generated/protocol.h>

#include <game/client/component.h>

class CBroadcast : public CComponent
{
	// broadcasts
	char m_aaBroadcastText[NUM_DUMMIES][1024];
	int m_aBroadcastTick[NUM_DUMMIES];
	float m_BroadcastRenderOffset;
	STextContainerIndex m_TextContainerIndex;

	void RenderServerBroadcast();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnWindowResize() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;

	void DeleteBroadcastContainer();

	void DoDummyBroadcast(const char *pText);
	void DoBroadcast(const char *pText, bool Dummy = false);
};

#endif
