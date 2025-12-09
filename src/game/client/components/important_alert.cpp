/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "important_alert.h"

#include <base/log.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

inline constexpr LOG_COLOR IMPORTANT_ALERT_COLOR{255, 0, 0};
inline constexpr float MINIMUM_ACTIVE_SECONDS = 5.0f;

void CImportantAlert::OnReset()
{
	m_Active = false;
	DeleteTextContainers();
}

void CImportantAlert::OnWindowResize()
{
	DeleteTextContainers();
}

void CImportantAlert::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		return;
	}
	if(!m_Active)
	{
		return;
	}
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && !g_Config.m_ClVideoShowImportantAlerts)
	{
		return;
	}
#endif
	RenderImportantAlert();
}

void CImportantAlert::DeleteTextContainers()
{
	TextRender()->DeleteTextContainer(m_TitleTextContainerIndex);
	TextRender()->DeleteTextContainer(m_MessageTextContainerIndex);
	TextRender()->DeleteTextContainer(m_CloseHintTextContainerIndex);
}

void CImportantAlert::RenderImportantAlert()
{
	if(m_FadeOutSince >= 0.0f && Client()->GlobalTime() - m_FadeOutSince >= 1.0f)
	{
		OnReset();
		return;
	}

	const float Seconds = SecondsActive();

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const float TitleFontSize = 20.0f;
	const float MessageFontSize = 16.0f;
	const float CloseHintFontSize = 6.0f;
	const float Alpha = m_FadeOutSince >= 0.0f ? 1.0f - (Client()->GlobalTime() - m_FadeOutSince) : 1.0f;

	if(!m_TitleTextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		Cursor.m_FontSize = TitleFontSize;
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_TitleTextContainerIndex, &Cursor, m_aTitleText);
	}
	if(m_TitleTextContainerIndex.Valid())
	{
		TextRender()->RenderTextContainer(m_TitleTextContainerIndex,
			ColorRGBA(1.0f, 0.0f, 0.0f, Alpha),
			TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha),
			Width / 2.0f - TextRender()->GetBoundingBoxTextContainer(m_TitleTextContainerIndex).m_W / 2.0f,
			40.0f);
	}

	if(!m_MessageTextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		Cursor.m_FontSize = MessageFontSize;
		Cursor.m_LineWidth = Width;
		TextRender()->CreateTextContainer(m_MessageTextContainerIndex, &Cursor, m_aMessageText);
	}
	if(m_MessageTextContainerIndex.Valid())
	{
		TextRender()->RenderTextContainer(m_MessageTextContainerIndex,
			TextRender()->DefaultTextColor().WithMultipliedAlpha(Alpha),
			TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha),
			Width / 2.0f - TextRender()->GetBoundingBoxTextContainer(m_MessageTextContainerIndex).m_W / 2.0f, 40.0f + TitleFontSize + 10.0f);
	}

	if(Seconds > MINIMUM_ACTIVE_SECONDS)
	{
		if(!m_CloseHintTextContainerIndex.Valid())
		{
			CTextCursor Cursor;
			Cursor.m_FontSize = CloseHintFontSize;
			Cursor.m_LineWidth = Width;
			TextRender()->CreateTextContainer(m_CloseHintTextContainerIndex, &Cursor, Localize("Press Esc or Tab to dismiss…", "Important alert message"));
		}
		if(m_CloseHintTextContainerIndex.Valid())
		{
			const float CloseHintAlpha = Seconds < (MINIMUM_ACTIVE_SECONDS + 1.0f) ? (Seconds - MINIMUM_ACTIVE_SECONDS) : 1.0f;
			TextRender()->RenderTextContainer(m_CloseHintTextContainerIndex,
				TextRender()->DefaultTextColor().WithMultipliedAlpha(Alpha * CloseHintAlpha),
				TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha * CloseHintAlpha),
				Width / 2.0f - TextRender()->GetBoundingBoxTextContainer(m_CloseHintTextContainerIndex).m_W / 2.0f, 40.0f - CloseHintFontSize - 2.0f);
		}
	}
}

void CImportantAlert::DoImportantAlert(const char *pTitle, const char *pLogGroup, const char *pMessage)
{
	str_copy(m_aTitleText, pTitle);
	str_copy(m_aMessageText, pMessage);
	m_Active = true;
	m_ActiveSince = Client()->GlobalTime();
	m_FadeOutSince = -1.0f;
	DeleteTextContainers();

	char aLine[sizeof(m_aMessageText)];
	while((pMessage = str_next_token(pMessage, "\n", aLine, sizeof(aLine))))
	{
		if(aLine[0] != '\0')
		{
			log_info_color(IMPORTANT_ALERT_COLOR, pLogGroup, "%s", aLine);
		}
	}

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		Client()->Notify(m_aTitleText, m_aMessageText);
	}
}

float CImportantAlert::SecondsActive() const
{
	return Client()->GlobalTime() - m_ActiveSince;
}

void CImportantAlert::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_SERVERALERT)
	{
		const CNetMsg_Sv_ServerAlert *pMsg = static_cast<const CNetMsg_Sv_ServerAlert *>(pRawMsg);
		DoImportantAlert(Localize("Server alert"), "server_alert", pMsg->m_pMessage);
	}
	else if(MsgType == NETMSGTYPE_SV_MODERATORALERT)
	{
		const CNetMsg_Sv_ModeratorAlert *pMsg = static_cast<const CNetMsg_Sv_ModeratorAlert *>(pRawMsg);
		DoImportantAlert(Localize("Moderator alert"), "moderator_alert", pMsg->m_pMessage);
	}
}

bool CImportantAlert::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() &&
		!GameClient()->m_Menus.IsActive() &&
		SecondsActive() >= MINIMUM_ACTIVE_SECONDS &&
		m_FadeOutSince < 0.0f &&
		(Event.m_Flags & IInput::FLAG_PRESS) != 0 &&
		(Event.m_Flags & IInput::FLAG_REPEAT) == 0 &&
		(Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_TAB))
	{
		m_FadeOutSince = Client()->GlobalTime();
		return true;
	}
	return false;
}

bool CImportantAlert::IsActive() const
{
	return m_Active;
}
