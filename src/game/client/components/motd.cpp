/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "motd.h"

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/important_alert.h>
#include <game/client/gameclient.h>

CMotd::CMotd()
{
	m_aServerMotd[0] = '\0';
	m_ServerMotdTime = 0;
	m_ServerMotdUpdateTime = 0;
}

void CMotd::Clear()
{
	m_ServerMotdTime = 0;
	Graphics()->DeleteQuadContainer(m_RectQuadContainer);
	TextRender()->DeleteTextContainer(m_TextContainerIndex);
}

bool CMotd::IsActive() const
{
	return time() < m_ServerMotdTime;
}

void CMotd::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE || OldState == IClient::STATE_OFFLINE)
		Clear();
}

void CMotd::OnWindowResize()
{
	Graphics()->DeleteQuadContainer(m_RectQuadContainer);
	TextRender()->DeleteTextContainer(m_TextContainerIndex);
}

void CMotd::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!IsActive())
		return;

	if(GameClient()->m_ImportantAlert.IsActive())
	{
		Clear();
		return;
	}

	const int MaxLines = 24;
	const float FontSize = 32.0f; // also the size of the margin and rect rounding
	const float ScreenHeight = 40.0f * FontSize; // multiple of the font size to get perfect alignment
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);

	const float RectHeight = (MaxLines + 2) * FontSize;
	const float RectWidth = 630.0f + 2.0f * FontSize;
	const float RectX = ScreenWidth / 2.0f - RectWidth / 2.0f;
	const float RectY = 160.0f;

	if(m_RectQuadContainer == -1)
	{
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.5f);
		m_RectQuadContainer = Graphics()->CreateRectQuadContainer(RectX, RectY, RectWidth, RectHeight, FontSize, IGraphics::CORNER_ALL);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if(m_RectQuadContainer != -1)
	{
		Graphics()->TextureClear();
		Graphics()->RenderQuadContainer(m_RectQuadContainer, -1);
	}

	if(!m_TextContainerIndex.Valid())
	{
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(RectX + FontSize, RectY + FontSize));
		// TODO: Set TEXTFLAG_ELLIPSIS_AT_END when https://github.com/ddnet/ddnet/issues/11419 is fixed
		// Cursor.m_Flags |= TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.m_FontSize = FontSize;
		Cursor.m_LineWidth = RectWidth - 2.0f * FontSize;
		Cursor.m_MaxLines = MaxLines;
		TextRender()->CreateTextContainer(m_TextContainerIndex, &Cursor, ServerMotd());
	}

	if(m_TextContainerIndex.Valid())
		TextRender()->RenderTextContainer(m_TextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor());
}

void CMotd::OnMessage(int MsgType, void *pRawMsg)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(MsgType == NETMSGTYPE_SV_MOTD)
	{
		const CNetMsg_Sv_Motd *pMsg = static_cast<CNetMsg_Sv_Motd *>(pRawMsg);

		// copy it manually to process all \n
		const char *pMsgStr = pMsg->m_pMessage;
		const size_t MotdLen = str_length(pMsgStr) + 1;
		const char *pLast = m_aServerMotd; // for console printing
		for(size_t i = 0, k = 0; i < MotdLen && k < sizeof(m_aServerMotd); i++, k++)
		{
			// handle incoming "\\n"
			if(pMsgStr[i] == '\\' && pMsgStr[i + 1] == 'n')
			{
				m_aServerMotd[k] = '\n';
				i++; // skip the 'n'
			}
			else
				m_aServerMotd[k] = pMsgStr[i];

			// print the line to the console when receiving the newline character
			if(g_Config.m_ClPrintMotd && m_aServerMotd[k] == '\n')
			{
				m_aServerMotd[k] = '\0';
				GameClient()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "motd", pLast, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
				m_aServerMotd[k] = '\n';
				pLast = m_aServerMotd + k + 1;
			}
		}
		m_aServerMotd[sizeof(m_aServerMotd) - 1] = '\0';
		if(g_Config.m_ClPrintMotd && *pLast != '\0')
			GameClient()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "motd", pLast, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));

		m_ServerMotdUpdateTime = time();
		if(m_aServerMotd[0] && g_Config.m_ClMotdTime)
			m_ServerMotdTime = m_ServerMotdUpdateTime + time_freq() * g_Config.m_ClMotdTime;
		else
			m_ServerMotdTime = 0;
		TextRender()->DeleteTextContainer(m_TextContainerIndex);
	}
}

bool CMotd::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		Clear();
		return true;
	}
	return false;
}
