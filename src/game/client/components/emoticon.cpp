/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/gamecore.h> // get_angle
#include <game/client/animstate.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include "chat.h"
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
	((CEmoticon *)pUserData)->Emoticon(pResult->GetInteger(0));
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	//m_DrawEmotes = false;
	m_SelectedEmoticon = -1;
	m_SelectedEmote = -1;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
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
		if(m_WasActive && m_SelectedEmoticon != -1)
			Emoticon(m_SelectedEmoticon);
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

	m_SelectedEmoticon = -1;
	m_SelectedEmote = -1;
	if (length(m_SelectorMouse) > 110.0f)
		m_SelectedEmoticon = (int)(SelectedAngle / (2*pi) * NUM_EMOTICONS);
	else if(length(m_SelectorMouse) > 40.0f)
		m_SelectedEmote = (int)((SelectedAngle +2*pi/24) / (2*pi) * NUM_EMOTES);
		

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

		bool Selected = m_SelectedEmoticon == i;

		float Size = Selected ? 80.0f : 50.0f;

		float NudgeX = 150.0f * cosf(Angle);
		float NudgeY = 150.0f * sinf(Angle);
		RenderTools()->SelectSprite(SPRITE_OOP + i);
		IGraphics::CQuadItem QuadItem(Screen.w/2 + NudgeX, Screen.h/2 + NudgeY, Size, Size);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	Graphics()->QuadsEnd();
	
	CServerInfo pServerInfo;
	Client()->GetServerInfo(&pServerInfo);
	if((IsDDRace(&pServerInfo) || IsDDNet(&pServerInfo) || IsPlus(&pServerInfo)) && g_Config.m_ClEyeWheel) 
	{
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0,1.0,1.0,0.3f);
		DrawCircle(Screen.w/2, Screen.h/2, 100.0f, 64);
		Graphics()->QuadsEnd();
		
		CTeeRenderInfo *pTeeInfo;
		if(g_Config.m_ClDummy)
			pTeeInfo = &m_pClient->m_aClients[m_pClient->Client()->m_LocalIDs[1]].m_RenderInfo;
		else
			pTeeInfo = &m_pClient->m_aClients[m_pClient->Client()->m_LocalIDs[0]].m_RenderInfo;
		
		Graphics()->TextureSet(pTeeInfo->m_Texture);
	
		for (int i = 0; i < NUM_EMOTES; i++)
		{
			float Angle = 2*pi*i/NUM_EMOTES;
			if (Angle > pi)
				Angle -= 2*pi;
	
			bool Selected = m_SelectedEmote == i;
	
			pTeeInfo->m_Size = Selected ? 64.0f : 48.0f;
			
			float NudgeX = 70.0f * cosf(Angle);
			float NudgeY = 70.0f * sinf(Angle);
			RenderTools()->RenderTee(CAnimState::GetIdle(), pTeeInfo, i, vec2(-1,0), vec2(Screen.w/2 + NudgeX, Screen.h/2 + NudgeY));
		}
	
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0,0,0,0.3f);
		DrawCircle(Screen.w/2, Screen.h/2, 30.0f, 64);
		Graphics()->QuadsEnd();
	}

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x+Screen.w/2,m_SelectorMouse.y+Screen.h/2,24,24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CEmoticon::Emoticon(int Emoticon)
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

void CEmoticon::Emote(int Emote)
{
	char aBuf[32];
	switch(Emote)
	{
	case EMOTE_NORMAL:
		str_format(aBuf, sizeof(aBuf), "/emote normal %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_PAIN:
		str_format(aBuf, sizeof(aBuf), "/emote pain %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_HAPPY:
		str_format(aBuf, sizeof(aBuf), "/emote happy %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_SURPRISE:
		str_format(aBuf, sizeof(aBuf), "/emote surprise %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_ANGRY:
		str_format(aBuf, sizeof(aBuf), "/emote angry %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_BLINK:
		str_format(aBuf, sizeof(aBuf), "/emote blink %d", g_Config.m_ClEyeDuration);
		break;
	}
	GameClient()->m_pChat->Say(0, aBuf);
}
