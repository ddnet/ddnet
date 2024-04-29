/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BROADCAST_H
#define GAME_CLIENT_COMPONENTS_BROADCAST_H

#include <base/color.h>
#include <engine/textrender.h>
#include <game/generated/protocol.h>

#include <game/client/component.h>

class CBroadcast : public CComponent
{
	enum
	{
		MAX_BROADCAST_MSG_SIZE = 1024,
		MAX_BROADCAST_COLOR_SEGMENTS = MAX_BROADCAST_MSG_SIZE / 4
	};

	class CBroadcastSegment
	{
	public:
		// start index in m_aBroadcastText
		int m_TextIndex = -1;
		ColorRGBA m_Color;
	};

	// broadcasts
	char m_aBroadcastText[MAX_BROADCAST_MSG_SIZE];
	int m_BroadcastTick;
	float m_BroadcastRenderOffset;
	STextContainerIndex m_aTextContainerIndices[MAX_BROADCAST_COLOR_SEGMENTS];
	CBroadcastSegment m_aSegments[MAX_BROADCAST_COLOR_SEGMENTS];

	void RenderServerBroadcast();
	void OnBroadcastMessage(const CNetMsg_Sv_Broadcast *pMsg);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnWindowResize() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
};

#endif
