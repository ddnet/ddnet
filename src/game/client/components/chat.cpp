/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/editor.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/csv.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

#include <game/client/animstate.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include "chat.h"

char CChat::ms_aDisplayText[MAX_LINE_LENGTH] = {'\0'};

CChat::CChat()
{
	for(auto &Line : m_aLines)
	{
		// reset the container indices, so the text containers can be deleted on reset
		Line.m_TextContainerIndex.Reset();
		Line.m_QuadContainerIndex = -1;
	}

	m_Mode = MODE_NONE;

	m_Input.SetClipboardLineCallback([this](const char *pStr) { SendChatQueued(pStr); });
	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = minimum(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vCommands.emplace_back(pName, pParams, pHelpText);
	m_CommandsNeedSorting = true;
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vCommands.erase(std::remove_if(m_vCommands.begin(), m_vCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vCommands.end());
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		// recalculate sizes
		Line.m_aYOffset[0] = -1.f;
		Line.m_aYOffset[1] = -1.f;
	}
}

void CChat::ClearLines()
{
	for(auto &Line : m_aLines)
	{
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		Line.m_Time = 0;
		Line.m_aText[0] = 0;
		Line.m_aName[0] = 0;
		Line.m_Friend = false;
		Line.m_TimesRepeated = 0;
		Line.m_HasRenderTee = false;
	}
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = 0x0;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_ServerSupportsCommandInfo = false;
	m_CommandsNeedSorting = false;
	mem_zero(m_aCurrentInputText, sizeof(m_aCurrentInputText));
	DisableMode();
	m_vCommands.clear();

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat *)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat *)pUserData)->EnableMode(1);
	else
		((CChat *)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat *)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::Echo(const char *pString)
{
	AddLine(CLIENT_MSG, 0, pString);
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

bool CChat::OnInput(const IInput::CEvent &Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		DisableMode();
		m_pClient->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_CommandsNeedSorting)
		{
			std::sort(m_vCommands.begin(), m_vCommands.end());
			m_CommandsNeedSorting = false;
		}

		SendChatQueued(m_Input.GetString());
		m_pHistoryEntry = nullptr;
		DisableMode();
		m_pClient->OnRelease();
		m_Input.Clear();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : m_pClient->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = m_pClient->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != 0)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &p1, const CRateablePlayer &p2) -> bool {
					return p1.Score < p2.Score;
				});
		}

		if(m_aCompletionBuffer[0] == '/' && !m_vCommands.empty())
		{
			CCommand *pCompletionCommand = 0;

			const size_t NumCommands = m_vCommands.size();

			if(ShiftPressed && m_CompletionUsed)
				m_CompletionChosen--;
			else if(!ShiftPressed)
				m_CompletionChosen++;
			m_CompletionChosen = (m_CompletionChosen + 2 * NumCommands) % (2 * NumCommands);

			m_CompletionUsed = true;

			const char *pCommandStart = m_aCompletionBuffer + 1;
			for(size_t i = 0; i < 2 * NumCommands; ++i)
			{
				int SearchType;
				int Index;

				if(ShiftPressed)
				{
					SearchType = ((m_CompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen - i + NumCommands) % NumCommands;
				}
				else
				{
					SearchType = ((m_CompletionChosen + i) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen + i) % NumCommands;
				}

				auto &Command = m_vCommands[Index];

				if(str_startswith_nocase(Command.m_aName, pCommandStart))
				{
					pCompletionCommand = &Command;
					m_CompletionChosen = Index + SearchType * NumCommands;
					break;
				}
			}

			// insert the command
			if(pCompletionCommand)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// add the command
				str_append(aBuf, "/");
				str_append(aBuf, pCompletionCommand->m_aName);

				// add separator
				const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
				str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName) + 1;
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
		else
		{
			// find next possible name
			const char *pCompletionString = 0;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &m_pClient->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if(m_Input.GetString()[0] == '/' && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
			m_pHistoryEntry = m_History.Last();

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode == MODE_NONE)
	{
		if(Team)
			m_Mode = MODE_TEAM;
		else
			m_Mode = MODE_ALL;

		Input()->Clear();
		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_Input.Activate(EInputPriority::CHAT);
	}
}

void CChat::DisableMode()
{
	if(m_Mode != MODE_NONE)
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(m_pClient->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

#define SAVES_FILE "ddnet-saves.txt"
const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

	// TODO: Find a simple way to get the names of team members. This doesn't
	// work since team is killed first, then save message gets sent:
	/*
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_paInfoByDDTeam[i];
		if(!pInfo)
			continue;
		pInfo->m_Team // All 0
	}
	*/

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		Client()->GetCurrentMap(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

void CChat::AddLine(int ClientId, int Team, const char *pLine)
{
	if(*pLine == 0 ||
		(ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (m_pClient->m_aClients[ClientId].m_aName[0] == '\0' || // unknown client
					  m_pClient->m_aClients[ClientId].m_ChatIgnore ||
					  (m_pClient->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !m_pClient->m_aClients[ClientId].m_Friend) ||
					  (m_pClient->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && m_pClient->IsOtherTeam(ClientId) && m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
					  (m_pClient->m_Snap.m_LocalClientId != ClientId && m_pClient->m_aClients[ClientId].m_Foe))))
		return;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = 0;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = 0;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;

		if(++Length >= MAX_LINE_LENGTH)
		{
			*(const_cast<char *>(pStr)) = 0;
			break;
		}
	}
	if(pEnd != 0)
		*(const_cast<char *>(pEnd)) = 0;

	if(*pLine == 0)
		return;

	bool Highlighted = false;

	auto &&FChatMsgCheckAndPrint = [this](CLine *pLine_) {
		if(pLine_->m_ClientId < 0) // server or client message
		{
			if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
				StoreSave(pLine_->m_aText);
		}

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s%s", pLine_->m_aName, pLine_->m_ClientId >= 0 ? ": " : "", pLine_->m_aText);

		ColorRGBA ChatLogColor{1, 1, 1, 1};
		if(pLine_->m_Highlighted)
		{
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		}
		else
		{
			if(pLine_->m_Friend && g_Config.m_ClMessageFriend)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
			else if(pLine_->m_Team)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			else if(pLine_->m_ClientId == SERVER_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			else if(pLine_->m_ClientId == CLIENT_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			else // regular message
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		}

		const char *pFrom;
		if(pLine_->m_Whisper)
			pFrom = "chat/whisper";
		else if(pLine_->m_Team)
			pFrom = "chat/team";
		else if(pLine_->m_ClientId == SERVER_MSG)
			pFrom = "chat/server";
		else if(pLine_->m_ClientId == CLIENT_MSG)
			pFrom = "chat/client";
		else
			pFrom = "chat/all";

		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor);
	};

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));

	CLine *pCurrentLine = &m_aLines[m_CurrentLine];

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	// If it's a client message, m_aText will have ": " prepended so we have to work around it.
	if(pCurrentLine->m_TeamNumber == Team && pCurrentLine->m_ClientId == ClientId && str_comp(pCurrentLine->m_aText, pLine) == 0 && pCurrentLine->m_CustomColor == CustomColor)
	{
		pCurrentLine->m_TimesRepeated++;
		TextRender()->DeleteTextContainer(pCurrentLine->m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(pCurrentLine->m_QuadContainerIndex);
		pCurrentLine->m_Time = time();
		pCurrentLine->m_aYOffset[0] = -1.f;
		pCurrentLine->m_aYOffset[1] = -1.f;

		FChatMsgCheckAndPrint(pCurrentLine);
		return;
	}

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;

	pCurrentLine = &m_aLines[m_CurrentLine];
	pCurrentLine->m_TimesRepeated = 0;
	pCurrentLine->m_Time = time();
	pCurrentLine->m_aYOffset[0] = -1.0f;
	pCurrentLine->m_aYOffset[1] = -1.0f;
	pCurrentLine->m_ClientId = ClientId;
	pCurrentLine->m_TeamNumber = Team;
	pCurrentLine->m_Team = Team == 1;
	pCurrentLine->m_Whisper = Team >= 2;
	pCurrentLine->m_NameColor = -2;
	pCurrentLine->m_Friend = false;
	pCurrentLine->m_HasRenderTee = false;
	pCurrentLine->m_CustomColor = CustomColor;

	TextRender()->DeleteTextContainer(pCurrentLine->m_TextContainerIndex);
	Graphics()->DeleteQuadContainer(pCurrentLine->m_QuadContainerIndex);

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != m_pClient->m_aLocalIds[0] && (!m_pClient->Client()->DummyConnected() || ClientId != m_pClient->m_aLocalIds[1]))
		{
			// main character
			Highlighted |= LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName);
			// dummy
			Highlighted |= m_pClient->Client()->DummyConnected() && LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName);
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= m_pClient->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_aName);
	}

	pCurrentLine->m_Highlighted = Highlighted;

	if(pCurrentLine->m_ClientId == SERVER_MSG)
	{
		str_copy(pCurrentLine->m_aName, "*** ");
		str_copy(pCurrentLine->m_aText, pLine);
	}
	else if(pCurrentLine->m_ClientId == CLIENT_MSG)
	{
		str_copy(pCurrentLine->m_aName, "— ");
		str_copy(pCurrentLine->m_aText, pLine);
	}
	else
	{
		auto &LineAuthor = m_pClient->m_aClients[pCurrentLine->m_ClientId];

		if(LineAuthor.m_Team == TEAM_SPECTATORS)
			pCurrentLine->m_NameColor = TEAM_SPECTATORS;

		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
		{
			if(LineAuthor.m_Team == TEAM_RED)
				pCurrentLine->m_NameColor = TEAM_RED;
			else if(LineAuthor.m_Team == TEAM_BLUE)
				pCurrentLine->m_NameColor = TEAM_BLUE;
		}

		if(Team == TEAM_WHISPER_SEND)
		{
			str_format(pCurrentLine->m_aName, sizeof(pCurrentLine->m_aName), "→ %s", LineAuthor.m_aName);
			pCurrentLine->m_NameColor = TEAM_BLUE;
			pCurrentLine->m_Highlighted = false;
			Highlighted = false;
		}
		else if(Team == TEAM_WHISPER_RECV)
		{
			str_format(pCurrentLine->m_aName, sizeof(pCurrentLine->m_aName), "← %s", LineAuthor.m_aName);
			pCurrentLine->m_NameColor = TEAM_RED;
			pCurrentLine->m_Highlighted = true;
			Highlighted = true;
		}
		else
			str_copy(pCurrentLine->m_aName, LineAuthor.m_aName);

		str_copy(pCurrentLine->m_aText, pLine);
		pCurrentLine->m_Friend = LineAuthor.m_Friend;

		if(pCurrentLine->m_aName[0] != '\0')
		{
			if(!g_Config.m_ClChatOld)
			{
				str_copy(pCurrentLine->m_aSkinName, LineAuthor.m_aSkinName);
				pCurrentLine->m_TeeRenderInfo = LineAuthor.m_RenderInfo;
				pCurrentLine->m_HasRenderTee = true;
			}
		}
	}

	FChatMsgCheckAndPrint(pCurrentLine);

	// play sound
	int64_t Now = time();
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				m_pClient->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				m_pClient->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = m_aLines[m_CurrentLine].m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
			}
#endif
			if(PlaySound)
			{
				m_pClient->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}
}

void CChat::OnRefreshSkins()
{
	for(auto &Line : m_aLines)
	{
		if(Line.m_HasRenderTee)
		{
			Line.m_TeeRenderInfo.Apply(m_pClient->m_Skins.Find(Line.m_aSkinName));
		}
		else
		{
			Line.m_TeeRenderInfo.Reset();
		}
	}
}

void CChat::OnPrepareLines(float y)
{
	float x = 5.0f;
	float FontSize = this->FontSize();

	const bool IsScoreBoardOpen = m_pClient->m_Scoreboard.Active() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat;
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	int64_t Now = time();
	float LineWidth = (IsScoreBoardOpen ? maximum(85.f, (FontSize * 85.0f / 6.f)) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	CTextCursor Cursor;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];

		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		if(Line.m_TextContainerIndex.Valid() && !ForceRecreate)
			continue;

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);

		char aClientId[16] = "";
		if(g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load ***' to continue";
			}
		}

		if(g_Config.m_ClChatOld)
		{
			Line.m_HasRenderTee = false;
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			TextRender()->SetCursor(&Cursor, TextBegin, 0.0f, FontSize, 0);
			Cursor.m_LineWidth = LineWidth;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				Cursor.m_X += RealMsgPaddingTee;

				if(Line.m_Friend && g_Config.m_ClMessageFriend)
				{
					TextRender()->TextEx(&Cursor, "♥ ");
				}
			}

			TextRender()->TextEx(&Cursor, aClientId);
			TextRender()->TextEx(&Cursor, Line.m_aName);
			if(Line.m_TimesRepeated > 0)
				TextRender()->TextEx(&Cursor, aCount);

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&Cursor, ": ");
			}

			CTextCursor AppendCursor = Cursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = Cursor.m_X;
				AppendCursor.m_LineWidth -= Cursor.m_LongestLineWidth;
			}

			TextRender()->TextEx(&AppendCursor, pText);

			Line.m_aYOffset[OffsetType] = AppendCursor.Height() + RealMsgPaddingY;
		}

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		// the position the text was created
		Line.m_TextYOffset = y + RealMsgPaddingY / 2.f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		// reset the cursor
		TextRender()->SetCursor(&Cursor, TextBegin, Line.m_TextYOffset, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;

		// Message is from valid player
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			Cursor.m_X += RealMsgPaddingTee;

			if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)).WithAlpha(1.f));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &Cursor, "♥ ");
			}
		}

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Team)
			NameColor = CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else if(Line.m_NameColor == TEAM_RED)
			NameColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.f);
		else if(Line.m_NameColor == TEAM_BLUE)
			NameColor = ColorRGBA(0.7f, 0.7f, 1.0f, 1.f);
		else if(Line.m_NameColor == TEAM_SPECTATORS)
			NameColor = ColorRGBA(0.75f, 0.5f, 0.75f, 1.f);
		else if(Line.m_ClientId >= 0 && g_Config.m_ClChatTeamColors && m_pClient->m_Teams.Team(Line.m_ClientId))
			NameColor = m_pClient->GetDDTeamColor(m_pClient->m_Teams.Team(Line.m_ClientId), 0.75f);
		else
			NameColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.f);

		TextRender()->TextColor(NameColor);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &Cursor, aClientId);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &Cursor, Line.m_aName);

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &Cursor, aCount);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &Cursor, ": ");
		}

		ColorRGBA Color;
		if(Line.m_CustomColor)
			Color = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Highlighted)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Line.m_Team)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else // regular message
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = Cursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = Cursor.m_X;
			AppendCursor.m_LineWidth -= Cursor.m_LongestLineWidth;
		}

		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			float FullWidth = RealMsgPaddingX * 1.5f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				FullWidth += Cursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
			}
			else
			{
				FullWidth += maximum(Cursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
			}
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, y, FullWidth, Line.m_aYOffset[OffsetType], MessageRounding(), IGraphics::CORNER_ALL);
		}

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CChat::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				SendChat(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	float x = 5.0f;
	float y = 300.0f - 20.0f * FontSize() / 6.f;
	float ScaledFontSize = FontSize() * (8 / 6.f);
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x, y, ScaledFontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width - 190.0f;

		if(m_Mode == MODE_ALL)
			TextRender()->TextEx(&Cursor, Localize("All"));
		else if(m_Mode == MODE_TEAM)
			TextRender()->TextEx(&Cursor, Localize("Team"));
		else
			TextRender()->TextEx(&Cursor, Localize("Chat"));

		TextRender()->TextEx(&Cursor, ": ");

		const float MessageMaxWidth = Cursor.m_LineWidth - (Cursor.m_X - Cursor.m_StartX);
		const CUIRect ClippingRect = {Cursor.m_X, Cursor.m_Y, MessageMaxWidth, 2.25f * Cursor.m_FontSize};
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		Graphics()->ClipEnable((int)(ClippingRect.x * XScale), (int)(ClippingRect.y * YScale), (int)(ClippingRect.w * XScale), (int)(ClippingRect.h * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();

		m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
		const CUIRect InputCursorRect = {Cursor.m_X, Cursor.m_Y - ScrollOffset, 0.0f, 0.0f};
		const bool WasChanged = m_Input.WasChanged();
		const bool WasCursorChanged = m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		const STextBoundingBox BoundingBox = m_Input.Render(&InputCursorRect, Cursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);

		Graphics()->ClipDisable();

		// Scroll up or down to keep the caret inside the clipping rect
		const float CaretPositionY = m_Input.GetCaretPosition().y - ScrollOffsetChange;
		if(CaretPositionY < ClippingRect.y)
			ScrollOffsetChange -= ClippingRect.y - CaretPositionY;
		else if(CaretPositionY + Cursor.m_FontSize > ClippingRect.y + ClippingRect.h)
			ScrollOffsetChange += CaretPositionY + Cursor.m_FontSize - (ClippingRect.y + ClippingRect.h);

		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, ClippingRect.h, BoundingBox.m_H);

		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollOffsetChange);

		// Autocompletion hint
		if(m_Input.GetString()[0] == '/' && m_Input.GetString()[1] != '\0' && !m_vCommands.empty())
		{
			for(const auto &Command : m_vCommands)
			{
				if(str_startswith_nocase(Command.m_aName, m_Input.GetString() + 1))
				{
					Cursor.m_X = Cursor.m_X + TextRender()->TextWidth(Cursor.m_FontSize, m_Input.GetString(), -1, Cursor.m_LineWidth);
					Cursor.m_Y = m_Input.GetCaretPosition().y;
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
					TextRender()->TextEx(&Cursor, Command.m_aName + str_length(m_Input.GetString() + 1));
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					break;
				}
			}
		}
	}

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
		return;

	y -= ScaledFontSize;

	OnPrepareLines(y);

	bool IsScoreBoardOpen = m_pClient->m_Scoreboard.Active() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)

	int64_t Now = time();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		float Blend = Now > Line.m_Time + 14 * time_freq() && !m_PrevShowChat ? 1.0f - (Now - Line.m_Time - 14 * time_freq()) / (2.0f * time_freq()) : 1.0f;

		// Draw backgrounds for messages in one batch
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				Graphics()->SetColor(0, 0, 0, 0.12f * Blend);
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, 0, ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset));
			}
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			if(!g_Config.m_ClChatOld && Line.m_HasRenderTee)
			{
				const int TeeSize = MessageTeeSize();
				Line.m_TeeRenderInfo.m_Size = TeeSize;

				float RowHeight = FontSize() + RealMsgPaddingY;
				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &Line.m_TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + (RealMsgPaddingX + TeeSize) / 2.0f, y + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &Line.m_TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, Blend);
			}

			const ColorRGBA TextColor = TextRender()->DefaultTextColor().WithMultipliedAlpha(Blend);
			const ColorRGBA TextOutlineColor = TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend);
			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, 0, (y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset);
		}
	}
}

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

void CChat::SendChat(int Team, const char *pLine)
{
	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	m_LastChatSend = time();

	if(m_pClient->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CChat::SendChatQueued(const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	bool AddEntry = false;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		const int Length = str_length(pLine);
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
		pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
		str_copy(pEntry->m_aText, pLine, Length + 1);
	}
}
