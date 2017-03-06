/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>

#include <game/client/components/scoreboard.h>
#include <game/client/components/sounds.h>
#include <game/localization.h>

#ifdef CONF_PLATFORM_MACOSX
#include <osx/notification.h>
#endif

#include "chat.h"


CChat::CChat()
{
	OnReset();
}

void CChat::OnReset()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		m_aLines[i].m_Time = 0;
		m_aLines[i].m_aText[0] = 0;
		m_aLines[i].m_aName[0] = 0;
	}

	m_ReverseTAB = false;
	m_Mode = MODE_NONE;
	m_Show = false;
	m_InputUpdate = false;
	m_ChatStringOffset = 0;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = 0x0;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;

	for(int i = 0; i < CHAT_NUM; ++i)
		m_aLastSoundPlayed[i] = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
	{
		m_Mode = MODE_NONE;
		Input()->SetIMEState(false);
		for(int i = 0; i < MAX_LINES; i++)
			m_aLines[i].m_Time = 0;
		m_CurrentLine = 0;
	}
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat*)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat*)pUserData)->EnableMode(1);
	else
		((CChat*)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat*)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->AddLine(-2, 0, pResult->GetString(0));
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT, ConEcho, this, "Echo the text in chat window");
}

bool CChat::OnInput(IInput::CEvent Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_V))
	{
		const char *Text = Input()->GetClipboardText();
		if(Text)
		{
			// if the text has more than one line, we send all lines except the last one
			// the last one is set as in the text field
			char Line[256];
			int i, Begin = 0;
			for(i = 0; i < str_length(Text); i++)
			{
				if(Text[i] == '\n')
				{
					int max = min(i - Begin + 1, (int)sizeof(Line));
					str_copy(Line, Text + Begin, max);
					Begin = i+1;
					SayChat(Line);
					while(Text[i] == '\n') i++;
				}
			}
			int max = min(i - Begin + 1, (int)sizeof(Line));
			str_copy(Line, Text + Begin, max);
			Begin = i+1;
			m_Input.Add(Line);
		}
	}

	if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_C))
	{
		Input()->SetClipboardText(m_Input.GetString());
	}

	if(Input()->KeyIsPressed(KEY_LCTRL)) // jump to spaces and special ASCII characters
	{
		int SearchDirection = 0;
		if(Input()->KeyPress(KEY_LEFT) || Input()->KeyPress(KEY_BACKSPACE))
			SearchDirection = -1;
		else if(Input()->KeyPress(KEY_RIGHT))
			SearchDirection = 1;

		if(SearchDirection != 0)
		{
			int FoundAt = SearchDirection > 0 ? m_Input.GetLength() - 1 : 0;
			for(int i = m_Input.GetCursorOffset() + SearchDirection; SearchDirection > 0 ? i < m_Input.GetLength() - 1 : i > 0; i += SearchDirection)
			{
				int Next = i + SearchDirection;
				if(	(m_Input.GetString()[Next] == ' ') ||
					(m_Input.GetString()[Next] >= 32 && m_Input.GetString()[Next] <= 47) ||
					(m_Input.GetString()[Next] >= 58 && m_Input.GetString()[Next] <= 64) ||
					(m_Input.GetString()[Next] >= 91 && m_Input.GetString()[Next] <= 96))
				{
					FoundAt = i;
					if (SearchDirection < 0)
						FoundAt++;
					break;
				}
			}
			if(Input()->KeyPress(KEY_BACKSPACE))
			{
				if(m_Input.GetCursorOffset() != 0)
				{
					char aText[512];
					str_copy(aText, m_Input.GetString(), FoundAt + 1);

					if(m_Input.GetCursorOffset() != str_length(m_Input.GetString()))
						str_append(aText, m_Input.GetString() + m_Input.GetCursorOffset(), str_length(m_Input.GetString()));

					m_Input.Set(aText);
				}
			}
			m_Input.SetCursorOffset(FoundAt);
		}
	}

	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		m_Mode = MODE_NONE;
		m_pClient->OnRelease();
		if(g_Config.m_ClChatReset)
			m_Input.Clear();

		// abort text editing when pressing escape
		Input()->SetIMEState(false);
	}
	else if(Event.m_Flags&IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_Input.GetString()[0])
		{
			bool AddEntry = false;

			if(m_LastChatSend+time_freq() < time_get())
			{
				Say(m_Mode == MODE_ALL ? 0 : 1, m_Input.GetString());
				AddEntry = true;
			}
			else if(m_PendingChatCounter < 3)
			{
				++m_PendingChatCounter;
				AddEntry = true;
			}

			if(AddEntry)
			{
				CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry)+m_Input.GetLength());
				pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
				mem_copy(pEntry->m_aText, m_Input.GetString(), m_Input.GetLength()+1);
			}
		}
		m_pHistoryEntry = 0x0;
		m_Mode = MODE_NONE;
		m_pClient->OnRelease();
		m_Input.Clear();

		// stop text editing after send chat.
		Input()->SetIMEState(false);
	}
	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		// fill the completion buffer
		if(m_CompletionChosen < 0)
		{
			const char *pCursor = m_Input.GetString()+m_Input.GetCursorOffset();
			for(int Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor-1) != ' '; --pCursor, ++Count);
			m_PlaceholderOffset = pCursor-m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_copy(m_aCompletionBuffer, m_Input.GetString()+m_PlaceholderOffset, min(static_cast<int>(sizeof(m_aCompletionBuffer)), m_PlaceholderLength+1));
		}

		// find next possible name
		const char *pCompletionString = 0;

			if(m_ReverseTAB)
				m_CompletionChosen = (m_CompletionChosen-1 + 2*MAX_CLIENTS)%(2*MAX_CLIENTS);
			else
				m_CompletionChosen = (m_CompletionChosen+1)%(2*MAX_CLIENTS);

		for(int i = 0; i < 2*MAX_CLIENTS; ++i)
		{
			int SearchType;
			int Index;

			if(m_ReverseTAB)
			{
				SearchType = ((m_CompletionChosen-i +2*MAX_CLIENTS)%(2*MAX_CLIENTS))/MAX_CLIENTS;
				Index = (m_CompletionChosen-i + MAX_CLIENTS )%MAX_CLIENTS;
			}
			else
			{
				SearchType = ((m_CompletionChosen+i)%(2*MAX_CLIENTS))/MAX_CLIENTS;
				Index = (m_CompletionChosen+i)%MAX_CLIENTS;
			}


			if(!m_pClient->m_Snap.m_paInfoByName[Index])
				continue;

			int Index2 = m_pClient->m_Snap.m_paInfoByName[Index]->m_ClientID;

			bool Found = false;
			if(SearchType == 1)
			{
				if(str_comp_nocase_num(m_pClient->m_aClients[Index2].m_aName, m_aCompletionBuffer, str_length(m_aCompletionBuffer)) &&
					str_find_nocase(m_pClient->m_aClients[Index2].m_aName, m_aCompletionBuffer))
					Found = true;
			}
			else if(!str_comp_nocase_num(m_pClient->m_aClients[Index2].m_aName, m_aCompletionBuffer, str_length(m_aCompletionBuffer)))
				Found = true;

			if(Found)
			{
				pCompletionString = m_pClient->m_aClients[Index2].m_aName;
				m_CompletionChosen = Index+SearchType*MAX_CLIENTS;
				break;
			}
		}

		// insert the name
		if(pCompletionString)
		{
			char aBuf[256];
			// add part before the name
			str_copy(aBuf, m_Input.GetString(), min(static_cast<int>(sizeof(aBuf)), m_PlaceholderOffset+1));

			// add the name
			str_append(aBuf, pCompletionString, sizeof(aBuf));

			// add seperator
			const char *pSeparator = "";
			if(*(m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength) != ' ')
				pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
			else if(m_PlaceholderOffset == 0)
				pSeparator = ":";
			if(*pSeparator)
				str_append(aBuf, pSeparator, sizeof(aBuf));

			// add part after the name
			str_append(aBuf, m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength, sizeof(aBuf));

			m_PlaceholderLength = str_length(pSeparator)+str_length(pCompletionString);
			m_OldChatStringLength = m_Input.GetLength();
			m_Input.Set(aBuf); // TODO: Use Add instead
			m_Input.SetCursorOffset(m_PlaceholderOffset+m_PlaceholderLength);
			m_InputUpdate = true;
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key != KEY_TAB)
			if(Event.m_Key != KEY_LSHIFT)
				m_CompletionChosen = -1;

		m_OldChatStringLength = m_Input.GetLength();
		m_Input.ProcessInput(Event);
		m_InputUpdate = true;
	}
	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_LSHIFT)
	{
		m_ReverseTAB = true;
	}
	else if(Event.m_Flags&IInput::FLAG_RELEASE && Event.m_Key == KEY_LSHIFT)
	{
		m_ReverseTAB = false;
	}
	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
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
	else if (Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if (m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
		else
			m_Input.Clear();
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

		Input()->SetIMEState(true);
		Input()->Clear();
		m_CompletionChosen = -1;
		UI()->AndroidShowTextInput("", Team ? Localize("Team chat") : Localize("Chat"));
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		AddLine(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_find_nocase(pLine, pName);

	if (pHL)
	{
		int Length = str_length(pName);

		if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;

	}

	return false;
}

void CChat::AddLine(int ClientID, int Team, const char *pLine)
{
	if(*pLine == 0 || (ClientID >= 0 && (m_pClient->m_aClients[ClientID].m_aName[0] == '\0' || // unknown client
		m_pClient->m_aClients[ClientID].m_ChatIgnore ||
		(m_pClient->m_Snap.m_LocalClientID != ClientID && g_Config.m_ClShowChatFriends && !m_pClient->m_aClients[ClientID].m_Friend) ||
		(m_pClient->m_Snap.m_LocalClientID != ClientID && m_pClient->m_aClients[ClientID].m_Foe))))
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
		if(str_utf8_isspace(Code))
		{
			pEnd = 0;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;

		if(++Length >= 256)
		{
			*(const_cast<char *>(pStr)) = 0;
			break;
		}
	}
	if(pEnd != 0)
		*(const_cast<char *>(pEnd)) = 0;

	bool Highlighted = false;
	char *p = const_cast<char*>(pLine);
	while(*p)
	{
		Highlighted = false;
		pLine = p;
		// find line seperator and strip multiline
		while(*p)
		{
			if(*p++ == '\n')
			{
				*(p-1) = 0;
				break;
			}
		}

		m_CurrentLine = (m_CurrentLine+1)%MAX_LINES;
		m_aLines[m_CurrentLine].m_Time = time_get();
		m_aLines[m_CurrentLine].m_YOffset[0] = -1.0f;
		m_aLines[m_CurrentLine].m_YOffset[1] = -1.0f;
		m_aLines[m_CurrentLine].m_ClientID = ClientID;
		m_aLines[m_CurrentLine].m_Team = Team;
		m_aLines[m_CurrentLine].m_NameColor = -2;

		// check for highlighted name
		if (Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(ClientID != m_pClient->m_LocalIDs[0])
			{
				// main character
				if (LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
					Highlighted = true;
				// dummy
				if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
					Highlighted = true;
			}
		}
		else
		{
			// on demo playback use local id from snap directly,
			// since m_LocalIDs isn't valid there
			if (LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName))
				Highlighted = true;
		}


		m_aLines[m_CurrentLine].m_Highlighted = Highlighted;

		if(ClientID < 0) // server or client message
		{
			str_copy(m_aLines[m_CurrentLine].m_aName, "*** ", sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), "%s", pLine);
		}
		else
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_SPECTATORS)
				m_aLines[m_CurrentLine].m_NameColor = TEAM_SPECTATORS;

			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
			{
				if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
			}

			if (Team == 2) // whisper send
			{
				str_format(m_aLines[m_CurrentLine].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName), "→ %s", m_pClient->m_aClients[ClientID].m_aName);
				m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
				m_aLines[m_CurrentLine].m_Highlighted = false;
				m_aLines[m_CurrentLine].m_Team = 0;
				Highlighted = false;
			}
			else if (Team == 3) // whisper recv
			{
				str_format(m_aLines[m_CurrentLine].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName), "← %s", m_pClient->m_aClients[ClientID].m_aName);
				m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				m_aLines[m_CurrentLine].m_Highlighted = true;
				m_aLines[m_CurrentLine].m_Team = 0;
				Highlighted = true;
			}
			else
			{
				if(g_Config.m_ClMessageFriend && m_pClient->m_aClients[ClientID].m_Friend)
				{
					str_format(m_aLines[m_CurrentLine].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName), "♥ %s", m_pClient->m_aClients[ClientID].m_aName);
					m_aLines[m_CurrentLine].m_NameColor = 2;
				}
				else
					str_copy(m_aLines[m_CurrentLine].m_aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName));
			}

			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), ": %s", pLine);
		}

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Team >= 2?"whisper":(m_aLines[m_CurrentLine].m_Team?"teamchat":"chat"), aBuf, Highlighted);
	}

	// play sound
	int64 Now = time_get();
	if(ClientID == -1) // Server message
	{
		if(Now-m_aLastSoundPlayed[CHAT_SERVER] >= time_freq()*3/10)
		{
			if(g_Config.m_SndServerMessage)
			{
				m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 0);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientID == -2) // Client message
	{
		// No sound yet
	}
	else if(Highlighted)
	{
		if(Now-m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq()*3/10)
		{
#ifdef CONF_PLATFORM_MACOSX
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s%s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
			CNotification::notify("DDNet-Chat", aBuf);
#else
			Graphics()->NotifyWindow();
#endif
			if(g_Config.m_SndHighlight)
			{
				m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}
		}
	}
	else if(Team != 2)
	{
		if(Now-m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq()*3/10)
		{
			if ((g_Config.m_SndTeamChat || !m_aLines[m_CurrentLine].m_Team)
				&& (g_Config.m_SndChat || m_aLines[m_CurrentLine].m_Team))
			{
				m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 0);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}
}

void CChat::OnRender()
{
	if (!g_Config.m_ClShowChat)
		return;

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend+time_freq() < time_get())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter-1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				Say(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	float Width = 300.0f*Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, 300.0f);
	float x = 5.0f;
	float y = 300.0f-20.0f;
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x, y, 8.0f, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width-190.0f;
		Cursor.m_MaxLines = 2;

		if(m_Mode == MODE_ALL)
			TextRender()->TextEx(&Cursor, Localize("All"), -1);
		else if(m_Mode == MODE_TEAM)
			TextRender()->TextEx(&Cursor, Localize("Team"), -1);
		else
			TextRender()->TextEx(&Cursor, Localize("Chat"), -1);

		TextRender()->TextEx(&Cursor, ": ", -1);

		// IME candidate editing
		bool Editing = false;
		int EditingCursor = Input()->GetEditingCursor();
		if (Input()->GetIMEState())
		{
			if(str_length(Input()->GetIMECandidate()))
			{
				m_Input.Editing(Input()->GetIMECandidate(), EditingCursor);
				Editing = true;
			}
		}

		// check if the visible text has to be moved
		if(m_InputUpdate)
		{
			if(m_ChatStringOffset > 0 && m_Input.GetLength(Editing) < m_OldChatStringLength)
				m_ChatStringOffset = max(0, m_ChatStringOffset-(m_OldChatStringLength-m_Input.GetLength(Editing)));

			if(m_ChatStringOffset > m_Input.GetCursorOffset(Editing))
				m_ChatStringOffset -= m_ChatStringOffset-m_Input.GetCursorOffset(Editing);
			else
			{
				CTextCursor Temp = Cursor;
				Temp.m_Flags = 0;
				TextRender()->TextEx(&Temp, m_Input.GetString(Editing)+m_ChatStringOffset, m_Input.GetCursorOffset(Editing)-m_ChatStringOffset);
				TextRender()->TextEx(&Temp, "|", -1);
				while(Temp.m_LineCount > 2)
				{
					++m_ChatStringOffset;
					Temp = Cursor;
					Temp.m_Flags = 0;
					TextRender()->TextEx(&Temp, m_Input.GetString(Editing)+m_ChatStringOffset, m_Input.GetCursorOffset(Editing)-m_ChatStringOffset);
					TextRender()->TextEx(&Temp, "|", -1);
				}
			}
			m_InputUpdate = false;
		}

		TextRender()->TextEx(&Cursor, m_Input.GetString(Editing)+m_ChatStringOffset, m_Input.GetCursorOffset(Editing)-m_ChatStringOffset);
		static float MarkerOffset = TextRender()->TextWidth(0, 8.0f, "|", -1)/3;
		CTextCursor Marker = Cursor;
		Marker.m_X -= MarkerOffset;
		TextRender()->TextEx(&Marker, "|", -1);
		TextRender()->TextEx(&Cursor, m_Input.GetString(Editing)+m_Input.GetCursorOffset(Editing), -1);
	}

	y -= 8.0f;
#if defined(__ANDROID__)
	x += 120.0f;
#endif

	int64 Now = time_get();
	float LineWidth = m_pClient->m_pScoreboard->Active() ? 90.0f : 200.0f;
	float HeightLimit = m_pClient->m_pScoreboard->Active() ? 230.0f : m_Show ? 50.0f : 200.0f;
	float Begin = x;
#if defined(__ANDROID__)
	float FontSize = 10.0f;
#else
	float FontSize = 6.0f;
#endif
	CTextCursor Cursor;
	int OffsetType = m_pClient->m_pScoreboard->Active() ? 1 : 0;
	for(int i = 0; i < MAX_LINES; i++)
	{
		int r = ((m_CurrentLine-i)+MAX_LINES)%MAX_LINES;
		if(Now > m_aLines[r].m_Time+16*time_freq() && !m_Show)
			break;

		char aName[64] = "";
		if(g_Config.m_ClShowIDs && m_aLines[r].m_ClientID != -1 && m_aLines[r].m_aName[0] != '\0')
		{
			if (m_aLines[r].m_ClientID >= 10)
				str_format(aName, sizeof(aName),"%d: ", m_aLines[r].m_ClientID);
			else
				str_format(aName, sizeof(aName)," %d: ", m_aLines[r].m_ClientID);
			str_append(aName, m_aLines[r].m_aName,sizeof(aName));
		}
		else
		{
			str_copy(aName, m_aLines[r].m_aName, sizeof(aName));
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(m_aLines[r].m_YOffset[OffsetType] < 0.0f)
		{
			TextRender()->SetCursor(&Cursor, Begin, 0.0f, FontSize, 0);
			Cursor.m_LineWidth = LineWidth;
			TextRender()->TextEx(&Cursor, aName, -1);
			TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
			m_aLines[r].m_YOffset[OffsetType] = Cursor.m_Y + Cursor.m_FontSize;
		}
		y -= m_aLines[r].m_YOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		float Blend = Now > m_aLines[r].m_Time+14*time_freq() && !m_Show ? 1.0f-(Now-m_aLines[r].m_Time-14*time_freq())/(2.0f*time_freq()) : 1.0f;

		// reset the cursor
		TextRender()->SetCursor(&Cursor, Begin, y, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;

		// render name
		if (m_aLines[r].m_ClientID == -1) // system
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageSystemHue / 255.0f, g_Config.m_ClMessageSystemSat / 255.0f, g_Config.m_ClMessageSystemLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if (m_aLines[r].m_ClientID == -2) // client
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageClientHue / 255.0f, g_Config.m_ClMessageClientSat / 255.0f, g_Config.m_ClMessageClientLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if (m_aLines[r].m_Team)
		{
			vec3 rgb = CalculateNameColor(vec3(g_Config.m_ClMessageTeamHue / 255.0f, g_Config.m_ClMessageTeamSat / 255.0f, g_Config.m_ClMessageTeamLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend); // team message
		}
		else if(m_aLines[r].m_NameColor == TEAM_RED)
			TextRender()->TextColor(1.0f, 0.5f, 0.5f, Blend); // red
		else if(m_aLines[r].m_NameColor == TEAM_BLUE)
			TextRender()->TextColor(0.7f, 0.7f, 1.0f, Blend); // blue
		else if(m_aLines[r].m_NameColor == TEAM_SPECTATORS)
			TextRender()->TextColor(0.75f, 0.5f, 0.75f, Blend); // spectator
		else if(m_aLines[r].m_NameColor == 2){
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageFriendHue / 255.0f, g_Config.m_ClMessageFriendSat / 255.0f, g_Config.m_ClMessageFriendLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if(m_aLines[r].m_ClientID >= 0 && g_Config.m_ClChatTeamColors && m_pClient->m_Teams.Team(m_aLines[r].m_ClientID))
		{
			vec3 rgb = HslToRgb(vec3(m_pClient->m_Teams.Team(m_aLines[r].m_ClientID) / 64.0f, 1.0f, 0.75f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else
			TextRender()->TextColor(0.8f, 0.8f, 0.8f, Blend);

		TextRender()->TextEx(&Cursor, aName, -1);

		// render line
		if (m_aLines[r].m_ClientID == -1) // system
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageSystemHue / 255.0f, g_Config.m_ClMessageSystemSat / 255.0f, g_Config.m_ClMessageSystemLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if (m_aLines[r].m_ClientID == -2) // client
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageClientHue / 255.0f, g_Config.m_ClMessageClientSat / 255.0f, g_Config.m_ClMessageClientLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if (m_aLines[r].m_Highlighted) // highlighted
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageHighlightHue / 255.0f, g_Config.m_ClMessageHighlightSat / 255.0f, g_Config.m_ClMessageHighlightLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else if (m_aLines[r].m_Team) // team message
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageTeamHue / 255.0f, g_Config.m_ClMessageTeamSat / 255.0f, g_Config.m_ClMessageTeamLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}
		else // regular message
		{
			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageHue / 255.0f, g_Config.m_ClMessageSat / 255.0f, g_Config.m_ClMessageLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, Blend);
		}


		TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
	}

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

#if defined(__ANDROID__)
	static int deferEvent = 0;
	if( UI()->AndroidTextInputShown() )
	{
		if(m_Mode == MODE_NONE)
		{
			deferEvent++;
			if( deferEvent > 2 )
				EnableMode(0);
		}
		else
			deferEvent = 0;
	}
	else
	{
		if(m_Mode != MODE_NONE)
		{
			deferEvent++;
			if( deferEvent > 2 )
			{
				IInput::CEvent Event;
				Event.m_Flags = IInput::FLAG_PRESS;
				Event.m_Key = KEY_RETURN;
				OnInput(Event);
			}
		}
		else
			deferEvent = 0;
	}
#endif
}

void CChat::Say(int Team, const char *pLine)
{
	m_LastChatSend = time_get();

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CChat::SayChat(const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	bool AddEntry = false;

	if(m_LastChatSend+time_freq() < time_get())
	{
		Say(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry)+str_length(pLine)-1);
		pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
		mem_copy(pEntry->m_aText, pLine, str_length(pLine));
	}
}
