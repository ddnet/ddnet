/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/gamecore.h> // get_angle
#include <game/client/ui.h>
#include <game/client/render.h>
#include "emoticon.h"

CEmoticon::CEmoticon()
{
	OnReset();
}

void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData)
{
	CEmoticon *pSelf = (CEmoticon *)pUserData;
	if(!pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CEmoticon::ConEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->Emote(pResult->GetInteger(0));
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "EmoticonId", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedEmote = -1;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

void CEmoticon::OnMessage(int MsgType, void *pRawMsg)
{
}

bool CEmoticon::OnMouseMove(float x, float y)
{
	if(!m_Active)
		return false;

#if defined(__ANDROID__) // No relative mouse on Android
	m_SelectorMouse = vec2(x,y);
#else
	UI()->ConvertMouseMove(&x, &y);
	m_SelectorMouse += vec2(x,y);
#endif
	return true;
}

void CEmoticon::DrawCircle(float x, float y, float r, int Segments)
{
	RenderTools()->DrawCircle(x, y, r, Segments);
}


void CEmoticon::OnRender()
{
	if(!m_Active)
	{
		if(m_WasActive && m_SelectedEmote != -1)
			Emote(m_SelectedEmote);
		m_WasActive = false;
		return;
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;

	if (length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	float SelectedAngle = GetAngle(m_SelectorMouse) + 2*pi/24;
	if (SelectedAngle < 0)
		SelectedAngle += 2*pi;

	if (length(m_SelectorMouse) > 110.0f)
		m_SelectedEmote = (int)(SelectedAngle / (2*pi) * NUM_EMOTICONS);
	else
		m_SelectedEmote = -1;

	CUIRect Screen = *UI()->Screen();

	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	Graphics()->BlendNormal();

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.3f);
	DrawCircle(Screen.w/2, Screen.h/2, 190.0f, 64);
	Graphics()->QuadsEnd();

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
	Graphics()->QuadsBegin();

	for (int i = 0; i < NUM_EMOTICONS; i++)
	{
		float Angle = 2*pi*i/NUM_EMOTICONS;
		if (Angle > pi)
			Angle -= 2*pi;

		bool Selected = m_SelectedEmote == i;

		float Size = Selected ? 80.0f : 50.0f;

		float NudgeX = 150.0f * cosf(Angle);
		float NudgeY = 150.0f * sinf(Angle);
		RenderTools()->SelectSprite(SPRITE_OOP + i);
		IGraphics::CQuadItem QuadItem(Screen.w/2 + NudgeX, Screen.h/2 + NudgeY, Size, Size);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	Graphics()->QuadsEnd();

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x+Screen.w/2,m_SelectorMouse.y+Screen.h/2,24,24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker Msg(NETMSGTYPE_CL_EMOTICON);
		Msg.AddInt(Emoticon);
		Client()->SendMsgExY(&Msg, MSGFLAG_VITAL, false, !g_Config.m_ClDummy);
	}
}
