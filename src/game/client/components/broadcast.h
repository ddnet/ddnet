/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BROADCAST_H
#define GAME_CLIENT_COMPONENTS_BROADCAST_H

#include <engine/textrender.h>

#include <game/client/component.h>

class CBroadcast : public CComponent
{
	// broadcasts
	char m_aClientBroadcastText[1024];
	char m_aBroadcastText[1024];
	int m_BroadcastTick;
	float m_BroadcastRenderOffset;
	float m_ClientBroadcastRenderOffset;
	float m_ClientBroadcastTime;
	STextContainerIndex m_TextContainerIndex;
	STextContainerIndex m_ClientTextContainerIndex;

	void RenderServerBroadcast();
	void RenderClientBroadcast();
	void OnBroadcastMessage(const CNetMsg_Sv_Broadcast *pMsg);

	static void ConClientBroadcast(IConsole::IResult *pResult, void *pUserData);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnWindowResize() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnConsoleInit() override;

	void DoClientBroadcast(const char *pText);
};

#endif
