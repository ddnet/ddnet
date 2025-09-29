/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "important_alert.h"

#include <base/log.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

inline constexpr LOG_COLOR IMPORTANT_ALERT_COLOR{255, 0, 0};

void CImportantAlert::OnReset()
{
	m_ShowUntilTick = -1;
	DeleteTextContainers();
}

void CImportantAlert::OnWindowResize()
{
	DeleteTextContainers();
}

void CImportantAlert::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderImportantAlert();
}

void CImportantAlert::DeleteTextContainers()
{
	TextRender()->DeleteTextContainer(m_TitleTextContainerIndex);
	TextRender()->DeleteTextContainer(m_MessageTextContainerIndex);
}

void CImportantAlert::RenderImportantAlert()
{
	const float Seconds = SecondsRemaining();
	if(Seconds <= 0.0f)
	{
		DeleteTextContainers();
		return;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const float TitleFontSize = 20.0f;
	const float MessageFontSize = 16.0f;
	const float Alpha = Seconds >= 1.0f ? 1.0f : Seconds;

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
}

void CImportantAlert::DoImportantAlert(int Type, const char *pMessage)
{
	const char *pLogGroup;
	switch(Type)
	{
	case ALERTMESSAGETYPE_SERVER:
		str_copy(m_aTitleText, Localize("Server alert"));
		pLogGroup = "server_alert";
		break;
	case ALERTMESSAGETYPE_MODERATOR:
		str_copy(m_aTitleText, Localize("Moderator alert"));
		pLogGroup = "moderator_alert";
		break;
	default:
		dbg_assert(false, "DoImportantAlert Type invalid: %d", Type);
		dbg_break();
	}
	str_copy(m_aMessageText, pMessage);
	m_ShowUntilTick = Client()->GameTick(0) + 20 * Client()->GameTickSpeed();
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

float CImportantAlert::SecondsRemaining() const
{
	if(m_ShowUntilTick < 0)
	{
		return 0.0f;
	}
	return (m_ShowUntilTick - Client()->GameTick(0)) / (float)Client()->GameTickSpeed();
}

void CImportantAlert::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_IMPORTANTALERT)
	{
		const CNetMsg_Sv_ImportantAlert *pMsg = static_cast<const CNetMsg_Sv_ImportantAlert *>(pRawMsg);
		DoImportantAlert(pMsg->m_Type, pMsg->m_pMessage);
	}
}

bool CImportantAlert::IsActive() const
{
	return SecondsRemaining() > 0.0f;
}
