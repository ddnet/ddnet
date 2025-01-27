/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "chat.h"
#include "emoticon.h"
#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <game/client/gameclient.h>

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
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	m_TouchPressedOutside = false;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CEmoticon::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CEmoticon::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_Active)
	{
		if(m_TouchPressedOutside)
		{
			m_SelectedEmote = -1;
			m_SelectedEyeEmote = -1;
			m_TouchPressedOutside = false;
		}

		if(m_WasActive && m_SelectedEmote != -1)
			Emote(m_SelectedEmote);
		if(m_WasActive && m_SelectedEyeEmote != -1)
			EyeEmote(m_SelectedEyeEmote);
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

	const CUIRect Screen = *Ui()->Screen();

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
		const float TouchCenterDistance = length(TouchPos);
		if(TouchCenterDistance <= 170.0f)
		{
			m_SelectorMouse = TouchPos;
		}
		else if(TouchCenterDistance > 190.0f)
		{
			m_TouchPressedOutside = true;
		}
	}
	else if(WasTouchPressed)
	{
		m_Active = false;
		return;
	}

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	float SelectedAngle = angle(m_SelectorMouse) + 2 * pi / 24;
	if(SelectedAngle < 0)
		SelectedAngle += 2 * pi;

	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	if(length(m_SelectorMouse) > 110.0f)
		m_SelectedEmote = (int)(SelectedAngle / (2 * pi) * NUM_EMOTICONS);
	else if(length(m_SelectorMouse) > 40.0f)
		m_SelectedEyeEmote = (int)(SelectedAngle / (2 * pi) * NUM_EMOTES);

	const vec2 ScreenCenter = Screen.Center();

	Ui()->MapScreen();

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.3f);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 190.0f, 64);
	Graphics()->QuadsEnd();

	Graphics()->WrapClamp();
	for(int Emote = 0; Emote < NUM_EMOTICONS; Emote++)
	{
		float Angle = 2 * pi * Emote / NUM_EMOTICONS;
		if(Angle > pi)
			Angle -= 2 * pi;

		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsBegin();
		const vec2 Nudge = direction(Angle) * 150.0f;
		const float Size = m_SelectedEmote == Emote ? 80.0f : 50.0f;
		IGraphics::CQuadItem QuadItem(ScreenCenter.x + Nudge.x, ScreenCenter.y + Nudge.y, Size, Size);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	Graphics()->WrapNormal();

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && m_pClient->m_aLocalIds[g_Config.m_ClDummy] >= 0)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0, 1.0, 1.0, 0.3f);
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 100.0f, 64);
		Graphics()->QuadsEnd();

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_RenderInfo;

		for(int Emote = 0; Emote < NUM_EMOTES; Emote++)
		{
			float Angle = 2 * pi * Emote / NUM_EMOTES;
			if(Angle > pi)
				Angle -= 2 * pi;

			const vec2 Nudge = direction(Angle) * 70.0f;
			TeeInfo.m_Size = m_SelectedEyeEmote == Emote ? 64.0f : 48.0f;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1, 0), ScreenCenter + Nudge);
		}

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.3f);
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 30.0f, 64);
		Graphics()->QuadsEnd();
	}
	else
		m_SelectedEyeEmote = -1;

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse, 24.0f);
}

void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgDummy(NETMSGTYPE_CL_EMOTICON, false);
		MsgDummy.AddInt(Emoticon);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgDummy, MSGFLAG_VITAL);
	}
}

void CEmoticon::EyeEmote(int Emote)
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
	GameClient()->m_Chat.SendChat(0, aBuf);
}
