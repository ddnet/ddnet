/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/lock.h>
#include <base/logger.h>
#include <base/math.h>
#include <base/system.h>

#include <game/generated/client_data.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/ringbuffer.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/localization.h>
#include <game/version.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <iterator>

#include "console.h"

static constexpr float FONT_SIZE = 10.0f;
static constexpr float LINE_SPACING = 1.0f;

class CConsoleLogger : public ILogger
{
	CGameConsole *m_pConsole;
	CLock m_ConsoleMutex;

public:
	CConsoleLogger(CGameConsole *pConsole) :
		m_pConsole(pConsole)
	{
		dbg_assert(pConsole != nullptr, "console pointer must not be null");
	}

	void Log(const CLogMessage *pMessage) override REQUIRES(!m_ConsoleMutex);
	void OnConsoleDeletion() REQUIRES(!m_ConsoleMutex);
};

void CConsoleLogger::Log(const CLogMessage *pMessage)
{
	if(m_Filter.Filters(pMessage))
	{
		return;
	}
	ColorRGBA Color = gs_ConsoleDefaultColor;
	if(pMessage->m_HaveColor)
	{
		Color.r = pMessage->m_Color.r / 255.0;
		Color.g = pMessage->m_Color.g / 255.0;
		Color.b = pMessage->m_Color.b / 255.0;
	}
	const CLockScope LockScope(m_ConsoleMutex);
	if(m_pConsole)
	{
		m_pConsole->m_LocalConsole.PrintLine(pMessage->m_aLine, pMessage->m_LineLength, Color);
	}
}

void CConsoleLogger::OnConsoleDeletion()
{
	const CLockScope LockScope(m_ConsoleMutex);
	m_pConsole = nullptr;
}

enum class EArgumentCompletionType
{
	NONE,
	TUNE,
	SETTING,
	KEY,
};

class CArgumentCompletionEntry
{
public:
	EArgumentCompletionType m_Type;
	const char *m_pCommandName;
	int m_ArgumentIndex;
};

static const CArgumentCompletionEntry gs_aArgumentCompletionEntries[] = {
	{EArgumentCompletionType::TUNE, "tune", 0},
	{EArgumentCompletionType::TUNE, "tune_reset", 0},
	{EArgumentCompletionType::TUNE, "toggle_tune", 0},
	{EArgumentCompletionType::TUNE, "tune_zone", 1},
	{EArgumentCompletionType::SETTING, "reset", 0},
	{EArgumentCompletionType::SETTING, "toggle", 0},
	{EArgumentCompletionType::SETTING, "access_level", 0},
	{EArgumentCompletionType::SETTING, "+toggle", 0},
	{EArgumentCompletionType::KEY, "bind", 0},
	{EArgumentCompletionType::KEY, "binds", 0},
	{EArgumentCompletionType::KEY, "unbind", 0},
};

static std::pair<EArgumentCompletionType, int> ArgumentCompletion(const char *pStr)
{
	const char *pCommandStart = pStr;
	const char *pIt = pStr;
	pIt = str_skip_to_whitespace_const(pIt);
	int CommandLength = pIt - pCommandStart;
	const char *pCommandEnd = pIt;

	if(!CommandLength)
		return {EArgumentCompletionType::NONE, -1};

	pIt = str_skip_whitespaces_const(pIt);
	if(pIt == pCommandEnd)
		return {EArgumentCompletionType::NONE, -1};

	for(const auto &Entry : gs_aArgumentCompletionEntries)
	{
		int Length = maximum(str_length(Entry.m_pCommandName), CommandLength);
		if(str_comp_nocase_num(Entry.m_pCommandName, pCommandStart, Length) == 0)
		{
			int CurrentArg = 0;
			const char *pArgStart = nullptr, *pArgEnd = nullptr;
			while(CurrentArg < Entry.m_ArgumentIndex)
			{
				pArgStart = pIt;
				pIt = str_skip_to_whitespace_const(pIt); // Skip argument value
				pArgEnd = pIt;

				if(!pIt[0] || pArgStart == pIt) // Check that argument is not empty
					return {EArgumentCompletionType::NONE, -1};

				pIt = str_skip_whitespaces_const(pIt); // Go to next argument position
				CurrentArg++;
			}
			if(pIt == pArgEnd)
				return {EArgumentCompletionType::NONE, -1}; // Check that there is at least one space after
			return {Entry.m_Type, pIt - pStr};
		}
	}
	return {EArgumentCompletionType::NONE, -1};
}

static int PossibleTunings(const char *pStr, IConsole::FPossibleCallback pfnCallback = IConsole::EmptyPossibleCommandCallback, void *pUser = nullptr)
{
	int Index = 0;
	for(int i = 0; i < CTuningParams::Num(); i++)
	{
		if(str_find_nocase(CTuningParams::Name(i), pStr))
		{
			pfnCallback(Index, CTuningParams::Name(i), pUser);
			Index++;
		}
	}
	return Index;
}

static int PossibleKeys(const char *pStr, IInput *pInput, IConsole::FPossibleCallback pfnCallback = IConsole::EmptyPossibleCommandCallback, void *pUser = nullptr)
{
	int Index = 0;
	for(int Key = KEY_A; Key < KEY_JOY_AXIS_11_RIGHT; Key++)
	{
		// Ignore unnamed keys starting with '&'
		const char *pKeyName = pInput->KeyName(Key);
		if(pKeyName[0] != '&' && str_find_nocase(pKeyName, pStr))
		{
			pfnCallback(Index, pKeyName, pUser);
			Index++;
		}
	}
	return Index;
}

const ColorRGBA CGameConsole::ms_SearchHighlightColor = ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f);
const ColorRGBA CGameConsole::ms_SearchSelectedColor = ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f);

CGameConsole::CInstance::CInstance(int Type)
{
	m_pHistoryEntry = 0x0;

	m_Type = Type;

	if(Type == CGameConsole::CONSOLETYPE_LOCAL)
	{
		m_pName = "local_console";
		m_CompletionFlagmask = CFGFLAG_CLIENT;
	}
	else
	{
		m_pName = "remote_console";
		m_CompletionFlagmask = CFGFLAG_SERVER;
	}

	m_aCompletionBuffer[0] = 0;
	m_CompletionChosen = -1;
	m_aCompletionBufferArgument[0] = 0;
	m_CompletionChosenArgument = -1;
	m_CompletionArgumentPosition = 0;
	Reset();

	m_aUser[0] = '\0';
	m_UserGot = false;
	m_UsernameReq = false;

	m_IsCommand = false;

	m_Backlog.SetPopCallback([this](CBacklogEntry *pEntry) {
		if(pEntry->m_LineCount != -1)
		{
			m_NewLineCounter -= pEntry->m_LineCount;
		}
	});

	m_Input.SetClipboardLineCallback([this](const char *pStr) { ExecuteLine(pStr); });

	m_CurrentMatchIndex = -1;
	m_aCurrentSearchString[0] = '\0';
}

void CGameConsole::CInstance::Init(CGameConsole *pGameConsole)
{
	m_pGameConsole = pGameConsole;
}

void CGameConsole::CInstance::ClearBacklog()
{
	{
		// We must ensure that no log messages are printed while owning
		// m_BacklogPendingLock or this will result in a dead lock.
		const CLockScope LockScope(m_BacklogPendingLock);
		m_BacklogPending.Init();
	}

	m_Backlog.Init();
	m_BacklogCurLine = 0;
	ClearSearch();
}

void CGameConsole::CInstance::UpdateBacklogTextAttributes()
{
	// Pending backlog entries are not handled because they don't have text attributes yet.
	for(CBacklogEntry *pEntry = m_Backlog.First(); pEntry; pEntry = m_Backlog.Next(pEntry))
	{
		UpdateEntryTextAttributes(pEntry);
	}
}

void CGameConsole::CInstance::PumpBacklogPending()
{
	{
		// We must ensure that no log messages are printed while owning
		// m_BacklogPendingLock or this will result in a dead lock.
		const CLockScope LockScopePending(m_BacklogPendingLock);
		for(CBacklogEntry *pPendingEntry = m_BacklogPending.First(); pPendingEntry; pPendingEntry = m_BacklogPending.Next(pPendingEntry))
		{
			const size_t EntrySize = sizeof(CBacklogEntry) + pPendingEntry->m_Length;
			CBacklogEntry *pEntry = m_Backlog.Allocate(EntrySize);
			mem_copy(pEntry, pPendingEntry, EntrySize);
		}

		m_BacklogPending.Init();
	}

	// Update text attributes and count number of added lines
	m_pGameConsole->Ui()->MapScreen();
	for(CBacklogEntry *pEntry = m_Backlog.First(); pEntry; pEntry = m_Backlog.Next(pEntry))
	{
		if(pEntry->m_LineCount == -1)
		{
			UpdateEntryTextAttributes(pEntry);
			m_NewLineCounter += pEntry->m_LineCount;
		}
	}
}

void CGameConsole::CInstance::ClearHistory()
{
	m_History.Init();
	m_pHistoryEntry = 0;
}

void CGameConsole::CInstance::Reset()
{
	m_CompletionRenderOffset = 0.0f;
	m_CompletionRenderOffsetChange = 0.0f;
	m_pCommandName = "";
	m_pCommandHelp = "";
	m_pCommandParams = "";
	m_CompletionArgumentPosition = 0;
}

void CGameConsole::CInstance::ExecuteLine(const char *pLine)
{
	if(m_Type == CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
	{
		const char *pPrevEntry = m_History.Last();
		if(pPrevEntry == nullptr || str_comp(pPrevEntry, pLine) != 0)
		{
			const size_t Size = str_length(pLine) + 1;
			char *pEntry = m_History.Allocate(Size);
			str_copy(pEntry, pLine, Size);
		}
		// print out the user's commands before they get run
		char aBuf[IConsole::CMDLINE_LENGTH + 3];
		str_format(aBuf, sizeof(aBuf), "> %s", pLine);
		m_pGameConsole->PrintLine(m_Type, aBuf);
	}

	if(m_Type == CGameConsole::CONSOLETYPE_LOCAL)
	{
		m_pGameConsole->m_pConsole->ExecuteLine(pLine);
	}
	else
	{
		if(m_pGameConsole->Client()->RconAuthed())
		{
			m_pGameConsole->Client()->Rcon(pLine);
		}
		else
		{
			if(!m_UserGot && m_UsernameReq)
			{
				m_UserGot = true;
				str_copy(m_aUser, pLine);
			}
			else
			{
				m_pGameConsole->Client()->RconAuth(m_aUser, pLine, g_Config.m_ClDummy);
				m_UserGot = false;
			}
		}
	}
}

void CGameConsole::CInstance::PossibleCommandsCompleteCallback(int Index, const char *pStr, void *pUser)
{
	CGameConsole::CInstance *pInstance = (CGameConsole::CInstance *)pUser;
	if(pInstance->m_CompletionChosen == Index)
	{
		char aBefore[IConsole::CMDLINE_LENGTH];
		str_truncate(aBefore, sizeof(aBefore), pInstance->m_aCompletionBuffer, pInstance->m_CompletionCommandStart);
		char aBuf[IConsole::CMDLINE_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s%s%s", aBefore, pStr, pInstance->m_aCompletionBuffer + pInstance->m_CompletionCommandEnd);
		pInstance->m_Input.Set(aBuf);
		pInstance->m_Input.SetCursorOffset(str_length(pStr) + pInstance->m_CompletionCommandStart);
	}
}

void CGameConsole::CInstance::GetCommand(const char *pInput, char (&aCmd)[IConsole::CMDLINE_LENGTH])
{
	char aInput[IConsole::CMDLINE_LENGTH];
	str_copy(aInput, pInput);
	m_CompletionCommandStart = 0;
	m_CompletionCommandEnd = 0;

	char aaSeparators[][2] = {";", "\""};
	for(auto *pSeparator : aaSeparators)
	{
		int Start, End;
		str_delimiters_around_offset(aInput + m_CompletionCommandStart, pSeparator, m_Input.GetCursorOffset() - m_CompletionCommandStart, &Start, &End);
		m_CompletionCommandStart += Start;
		m_CompletionCommandEnd = m_CompletionCommandStart + (End - Start);
		aInput[m_CompletionCommandEnd] = '\0';
	}

	str_copy(aCmd, aInput + m_CompletionCommandStart, sizeof(aCmd));
}

static void StrCopyUntilSpace(char *pDest, size_t DestSize, const char *pSrc)
{
	const char *pSpace = str_find(pSrc, " ");
	str_copy(pDest, pSrc, minimum<size_t>(pSpace ? pSpace - pSrc + 1 : 1, DestSize));
}

void CGameConsole::CInstance::PossibleArgumentsCompleteCallback(int Index, const char *pStr, void *pUser)
{
	CGameConsole::CInstance *pInstance = (CGameConsole::CInstance *)pUser;
	if(pInstance->m_CompletionChosenArgument == Index)
	{
		// get command
		char aBuf[IConsole::CMDLINE_LENGTH];
		str_copy(aBuf, pInstance->GetString(), pInstance->m_CompletionArgumentPosition);
		str_append(aBuf, " ");

		// append argument
		str_append(aBuf, pStr);
		pInstance->m_Input.Set(aBuf);
	}
}

bool CGameConsole::CInstance::OnInput(const IInput::CEvent &Event)
{
	bool Handled = false;

	// Don't allow input while the console is opening/closing
	if(m_pGameConsole->m_ConsoleState == CONSOLE_OPENING || m_pGameConsole->m_ConsoleState == CONSOLE_CLOSING)
		return Handled;

	auto &&SelectNextSearchMatch = [&](int Direction) {
		if(!m_vSearchMatches.empty())
		{
			m_CurrentMatchIndex += Direction;
			if(m_CurrentMatchIndex >= (int)m_vSearchMatches.size())
				m_CurrentMatchIndex = 0;
			if(m_CurrentMatchIndex < 0)
				m_CurrentMatchIndex = (int)m_vSearchMatches.size() - 1;
			m_HasSelection = false;
			// Also scroll to the correct line
			ScrollToCenter(m_vSearchMatches[m_CurrentMatchIndex].m_StartLine, m_vSearchMatches[m_CurrentMatchIndex].m_EndLine);
		}
	};

	const int BacklogPrevLine = m_BacklogCurLine;
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
		{
			if(!m_Searching)
			{
				if(!m_Input.IsEmpty() || (m_UsernameReq && !m_pGameConsole->Client()->RconAuthed() && !m_UserGot))
				{
					ExecuteLine(m_Input.GetString());
					m_Input.Clear();
					m_pHistoryEntry = nullptr;
				}
			}
			else
			{
				SelectNextSearchMatch(m_pGameConsole->m_pClient->Input()->ShiftIsPressed() ? -1 : 1);
			}

			Handled = true;
		}
		else if(Event.m_Key == KEY_UP)
		{
			if(m_Searching)
			{
				SelectNextSearchMatch(-1);
			}
			else if(m_Type == CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
			{
				if(m_pHistoryEntry)
				{
					char *pTest = m_History.Prev(m_pHistoryEntry);

					if(pTest)
						m_pHistoryEntry = pTest;
				}
				else
					m_pHistoryEntry = m_History.Last();

				if(m_pHistoryEntry)
					m_Input.Set(m_pHistoryEntry);
			}
			Handled = true;
		}
		else if(Event.m_Key == KEY_DOWN)
		{
			if(m_Searching)
			{
				SelectNextSearchMatch(1);
			}
			else if(m_Type == CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
			{
				if(m_pHistoryEntry)
					m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

				if(m_pHistoryEntry)
					m_Input.Set(m_pHistoryEntry);
				else
					m_Input.Clear();
			}
			Handled = true;
		}
		else if(Event.m_Key == KEY_TAB)
		{
			const int Direction = m_pGameConsole->m_pClient->Input()->ShiftIsPressed() ? -1 : 1;

			if(!m_Searching)
			{
				char aSearch[IConsole::CMDLINE_LENGTH];
				GetCommand(m_aCompletionBuffer, aSearch);

				// command completion
				const bool UseTempCommands = m_Type == CGameConsole::CONSOLETYPE_REMOTE && m_pGameConsole->Client()->RconAuthed() && m_pGameConsole->Client()->UseTempRconCommands();
				int CompletionEnumerationCount = m_pGameConsole->m_pConsole->PossibleCommands(aSearch, m_CompletionFlagmask, UseTempCommands);
				if(m_Type == CGameConsole::CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
				{
					if(CompletionEnumerationCount)
					{
						if(m_CompletionChosen == -1 && Direction < 0)
							m_CompletionChosen = 0;
						m_CompletionChosen = (m_CompletionChosen + Direction + CompletionEnumerationCount) % CompletionEnumerationCount;
						m_CompletionArgumentPosition = 0;
						m_pGameConsole->m_pConsole->PossibleCommands(aSearch, m_CompletionFlagmask, UseTempCommands, PossibleCommandsCompleteCallback, this);
					}
					else if(m_CompletionChosen != -1)
					{
						m_CompletionChosen = -1;
						Reset();
					}
				}

				// Argument completion
				const auto [CompletionType, CompletionPos] = ArgumentCompletion(GetString());
				if(CompletionType == EArgumentCompletionType::TUNE)
					CompletionEnumerationCount = PossibleTunings(m_aCompletionBufferArgument);
				else if(CompletionType == EArgumentCompletionType::SETTING)
					CompletionEnumerationCount = m_pGameConsole->m_pConsole->PossibleCommands(m_aCompletionBufferArgument, m_CompletionFlagmask, UseTempCommands);
				else if(CompletionType == EArgumentCompletionType::KEY)
					CompletionEnumerationCount = PossibleKeys(m_aCompletionBufferArgument, m_pGameConsole->Input());

				if(CompletionEnumerationCount)
				{
					if(m_CompletionChosenArgument == -1 && Direction < 0)
						m_CompletionChosenArgument = 0;
					m_CompletionChosenArgument = (m_CompletionChosenArgument + Direction + CompletionEnumerationCount) % CompletionEnumerationCount;
					m_CompletionArgumentPosition = CompletionPos;
					if(CompletionType == EArgumentCompletionType::TUNE)
						PossibleTunings(m_aCompletionBufferArgument, PossibleArgumentsCompleteCallback, this);
					else if(CompletionType == EArgumentCompletionType::SETTING)
						m_pGameConsole->m_pConsole->PossibleCommands(m_aCompletionBufferArgument, m_CompletionFlagmask, UseTempCommands, PossibleArgumentsCompleteCallback, this);
					else if(CompletionType == EArgumentCompletionType::KEY)
						PossibleKeys(m_aCompletionBufferArgument, m_pGameConsole->Input(), PossibleArgumentsCompleteCallback, this);
				}
				else if(m_CompletionChosenArgument != -1)
				{
					m_CompletionChosenArgument = -1;
					Reset();
				}
			}
			else
			{
				// Use Tab / Shift-Tab to cycle through search matches
				SelectNextSearchMatch(Direction);
			}
			Handled = true;
		}
		else if(Event.m_Key == KEY_PAGEUP)
		{
			m_BacklogCurLine += GetLinesToScroll(-1, m_LinesRendered);
			Handled = true;
		}
		else if(Event.m_Key == KEY_PAGEDOWN)
		{
			m_BacklogCurLine -= GetLinesToScroll(1, m_LinesRendered);
			if(m_BacklogCurLine < 0)
			{
				m_BacklogCurLine = 0;
			}
			Handled = true;
		}
		else if(Event.m_Key == KEY_MOUSE_WHEEL_UP)
		{
			m_BacklogCurLine += GetLinesToScroll(-1, 1);
			Handled = true;
		}
		else if(Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			--m_BacklogCurLine;
			if(m_BacklogCurLine < 0)
			{
				m_BacklogCurLine = 0;
			}
			Handled = true;
		}
		// in order not to conflict with CLineInput's handling of Home/End only
		// react to it when the input is empty
		else if(Event.m_Key == KEY_HOME && m_Input.IsEmpty())
		{
			m_BacklogCurLine += GetLinesToScroll(-1, -1);
			m_BacklogLastActiveLine = m_BacklogCurLine;
			Handled = true;
		}
		else if(Event.m_Key == KEY_END && m_Input.IsEmpty())
		{
			m_BacklogCurLine = 0;
			Handled = true;
		}
		else if(Event.m_Key == KEY_ESCAPE && m_Searching)
		{
			SetSearching(false);
			Handled = true;
		}
		else if(Event.m_Key == KEY_F && m_pGameConsole->Input()->ModifierIsPressed())
		{
			SetSearching(true);
			Handled = true;
		}
	}

	if(m_BacklogCurLine != BacklogPrevLine)
	{
		m_HasSelection = false;
	}

	if(!Handled)
	{
		Handled = m_Input.ProcessInput(Event);
		if(Handled)
			UpdateSearch();
	}

	if(Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_TEXT))
	{
		if(Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			const char *pInputStr = m_Input.GetString();

			m_CompletionChosen = -1;
			str_copy(m_aCompletionBuffer, pInputStr);

			const auto [CompletionType, CompletionPos] = ArgumentCompletion(GetString());
			if(CompletionType != EArgumentCompletionType::NONE)
			{
				for(const auto &Entry : gs_aArgumentCompletionEntries)
				{
					if(Entry.m_Type != CompletionType)
						continue;
					const int Len = str_length(Entry.m_pCommandName);
					if(str_comp_nocase_num(pInputStr, Entry.m_pCommandName, Len) == 0 && str_isspace(pInputStr[Len]))
					{
						m_CompletionChosenArgument = -1;
						str_copy(m_aCompletionBufferArgument, &pInputStr[CompletionPos]);
					}
				}
			}

			Reset();
		}

		// find the current command
		{
			char aCmd[IConsole::CMDLINE_LENGTH];
			GetCommand(GetString(), aCmd);
			char aBuf[IConsole::CMDLINE_LENGTH];
			StrCopyUntilSpace(aBuf, sizeof(aBuf), aCmd);

			const IConsole::CCommandInfo *pCommand = m_pGameConsole->m_pConsole->GetCommandInfo(aBuf, m_CompletionFlagmask,
				m_Type != CGameConsole::CONSOLETYPE_LOCAL && m_pGameConsole->Client()->RconAuthed() && m_pGameConsole->Client()->UseTempRconCommands());
			if(pCommand)
			{
				m_IsCommand = true;
				m_pCommandName = pCommand->m_pName;
				m_pCommandHelp = pCommand->m_pHelp;
				m_pCommandParams = pCommand->m_pParams;
			}
			else
				m_IsCommand = false;
		}
	}

	return Handled;
}

void CGameConsole::CInstance::PrintLine(const char *pLine, int Len, ColorRGBA PrintColor)
{
	// We must ensure that no log messages are printed while owning
	// m_BacklogPendingLock or this will result in a dead lock.
	const CLockScope LockScope(m_BacklogPendingLock);
	CBacklogEntry *pEntry = m_BacklogPending.Allocate(sizeof(CBacklogEntry) + Len);
	pEntry->m_YOffset = -1.0f;
	pEntry->m_PrintColor = PrintColor;
	pEntry->m_Length = Len;
	pEntry->m_LineCount = -1;
	str_copy(pEntry->m_aText, pLine, Len + 1);
}

int CGameConsole::CInstance::GetLinesToScroll(int Direction, int LinesToScroll)
{
	auto *pEntry = m_Backlog.Last();
	int Line = 0;
	int LinesToSkip = (Direction == -1 ? m_BacklogCurLine + m_LinesRendered : m_BacklogCurLine - 1);
	while(Line < LinesToSkip && pEntry)
	{
		if(pEntry->m_LineCount == -1)
			UpdateEntryTextAttributes(pEntry);
		Line += pEntry->m_LineCount;
		pEntry = m_Backlog.Prev(pEntry);
	}

	int Amount = maximum(0, Line - LinesToSkip);
	while(pEntry && (LinesToScroll > 0 ? Amount < LinesToScroll : true))
	{
		if(pEntry->m_LineCount == -1)
			UpdateEntryTextAttributes(pEntry);
		Amount += pEntry->m_LineCount;
		pEntry = Direction == -1 ? m_Backlog.Prev(pEntry) : m_Backlog.Next(pEntry);
	}

	return LinesToScroll > 0 ? minimum(Amount, LinesToScroll) : Amount;
}

void CGameConsole::CInstance::ScrollToCenter(int StartLine, int EndLine)
{
	// This method is used to scroll lines from `StartLine` to `EndLine` to the center of the screen, if possible.

	// Find target line
	int Target = maximum(0, (int)ceil(StartLine - minimum(StartLine - EndLine, m_LinesRendered) / 2) - m_LinesRendered / 2);
	if(m_BacklogCurLine == Target)
		return;

	// Compute acutal amount of lines to scroll to make sure lines fit in viewport and we don't have empty space
	int Direction = m_BacklogCurLine - Target < 0 ? -1 : 1;
	int LinesToScroll = absolute(Target - m_BacklogCurLine);
	int ComputedLines = GetLinesToScroll(Direction, LinesToScroll);

	if(Direction == -1)
		m_BacklogCurLine += ComputedLines;
	else
		m_BacklogCurLine -= ComputedLines;
}

void CGameConsole::CInstance::UpdateEntryTextAttributes(CBacklogEntry *pEntry) const
{
	CTextCursor Cursor;
	m_pGameConsole->TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, FONT_SIZE, 0);
	Cursor.m_LineWidth = m_pGameConsole->Ui()->Screen()->w - 10;
	Cursor.m_MaxLines = 10;
	Cursor.m_LineSpacing = LINE_SPACING;
	m_pGameConsole->TextRender()->TextEx(&Cursor, pEntry->m_aText, -1);
	pEntry->m_YOffset = Cursor.Height();
	pEntry->m_LineCount = Cursor.m_LineCount;
}

void CGameConsole::CInstance::SetSearching(bool Searching)
{
	m_Searching = Searching;
	if(Searching)
	{
		m_Input.SetClipboardLineCallback(nullptr); // restore default behavior (replace newlines with spaces)
		m_Input.Set(m_aCurrentSearchString);
		m_Input.SelectAll();
		UpdateSearch();
	}
	else
	{
		m_Input.SetClipboardLineCallback([this](const char *pLine) { ExecuteLine(pLine); });
		m_Input.Clear();
	}
}

void CGameConsole::CInstance::ClearSearch()
{
	m_vSearchMatches.clear();
	m_CurrentMatchIndex = -1;
	m_Input.Clear();
	m_aCurrentSearchString[0] = '\0';
}

void CGameConsole::CInstance::UpdateSearch()
{
	if(!m_Searching)
		return;

	const char *pSearchText = m_Input.GetString();
	bool SearchChanged = str_utf8_comp_nocase(pSearchText, m_aCurrentSearchString) != 0;

	int SearchLength = m_Input.GetLength();
	str_copy(m_aCurrentSearchString, pSearchText);

	m_vSearchMatches.clear();
	if(pSearchText[0] == '\0')
	{
		m_CurrentMatchIndex = -1;
		return;
	}

	if(SearchChanged)
	{
		m_CurrentMatchIndex = -1;
		m_HasSelection = false;
	}

	ITextRender *pTextRender = m_pGameConsole->Ui()->TextRender();
	const int LineWidth = m_pGameConsole->Ui()->Screen()->w - 10.0f;

	CBacklogEntry *pEntry = m_Backlog.Last();
	int EntryLine = 0, LineToScrollStart = 0, LineToScrollEnd = 0;

	for(; pEntry; EntryLine += pEntry->m_LineCount, pEntry = m_Backlog.Prev(pEntry))
	{
		const char *pSearchPos = str_utf8_find_nocase(pEntry->m_aText, pSearchText);
		if(!pSearchPos)
			continue;

		int EntryLineCount = pEntry->m_LineCount;

		// Find all occurences of the search string and save their positions
		while(pSearchPos)
		{
			int Pos = pSearchPos - pEntry->m_aText;

			if(EntryLineCount == 1)
			{
				m_vSearchMatches.emplace_back(Pos, EntryLine, EntryLine, EntryLine);
				if(EntryLine > LineToScrollStart)
				{
					LineToScrollStart = EntryLine;
					LineToScrollEnd = EntryLine;
				}
			}
			else
			{
				// A match can span multiple lines in case of a multiline entry, so we need to know which line the match starts at
				// and which line it ends at in order to put it in viewport properly
				STextSizeProperties Props;
				int LineCount;
				Props.m_pLineCount = &LineCount;

				// Compute line of end match
				pTextRender->TextWidth(FONT_SIZE, pEntry->m_aText, Pos + SearchLength, LineWidth, 0, Props);
				int EndLine = (EntryLineCount - LineCount);
				int MatchEndLine = EntryLine + EndLine;

				// Compute line of start of match
				int MatchStartLine = MatchEndLine;
				if(LineCount > 1)
				{
					pTextRender->TextWidth(FONT_SIZE, pEntry->m_aText, Pos, LineWidth, 0, Props);
					int StartLine = (EntryLineCount - LineCount);
					MatchStartLine = EntryLine + StartLine;
				}

				if(MatchStartLine > LineToScrollStart)
				{
					LineToScrollStart = MatchStartLine;
					LineToScrollEnd = MatchEndLine;
				}

				m_vSearchMatches.emplace_back(Pos, MatchStartLine, MatchEndLine, EntryLine);
			}

			pSearchPos = str_utf8_find_nocase(pEntry->m_aText + Pos + SearchLength, pSearchText);
		}
	}

	if(!m_vSearchMatches.empty() && SearchChanged)
		m_CurrentMatchIndex = 0;
	else
		m_CurrentMatchIndex = std::clamp(m_CurrentMatchIndex, -1, (int)m_vSearchMatches.size() - 1);

	// Reverse order of lines by sorting so we have matches from top to bottom instead of bottom to top
	std::sort(m_vSearchMatches.begin(), m_vSearchMatches.end(), [](const SSearchMatch &MatchA, const SSearchMatch &MatchB) {
		if(MatchA.m_StartLine == MatchB.m_StartLine)
			return MatchA.m_Pos < MatchB.m_Pos; // Make sure to keep position order
		return MatchA.m_StartLine > MatchB.m_StartLine;
	});

	if(!m_vSearchMatches.empty() && SearchChanged)
	{
		ScrollToCenter(LineToScrollStart, LineToScrollEnd);
	}
}

void CGameConsole::CInstance::Dump()
{
	char aTimestamp[20];
	str_timestamp(aTimestamp, sizeof(aTimestamp));
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "dumps/%s_dump_%s.txt", m_pName, aTimestamp);
	IOHANDLE File = m_pGameConsole->Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(File)
	{
		PumpBacklogPending();
		for(CInstance::CBacklogEntry *pEntry = m_Backlog.First(); pEntry; pEntry = m_Backlog.Next(pEntry))
		{
			io_write(File, pEntry->m_aText, pEntry->m_Length);
			io_write_newline(File);
		}
		io_close(File);
		log_info("console", "%s contents were written to '%s'", m_pName, aFilename);
	}
	else
	{
		log_error("console", "Failed to open '%s'", aFilename);
	}
}

CGameConsole::CGameConsole() :
	m_LocalConsole(CONSOLETYPE_LOCAL), m_RemoteConsole(CONSOLETYPE_REMOTE)
{
	m_ConsoleType = CONSOLETYPE_LOCAL;
	m_ConsoleState = CONSOLE_CLOSED;
	m_StateChangeEnd = 0.0f;
	m_StateChangeDuration = 0.1f;

	m_pConsoleLogger = new CConsoleLogger(this);
}

CGameConsole::~CGameConsole()
{
	if(m_pConsoleLogger)
		m_pConsoleLogger->OnConsoleDeletion();
}

CGameConsole::CInstance *CGameConsole::ConsoleForType(int ConsoleType)
{
	if(ConsoleType == CONSOLETYPE_REMOTE)
		return &m_RemoteConsole;
	return &m_LocalConsole;
}

CGameConsole::CInstance *CGameConsole::CurrentConsole()
{
	return ConsoleForType(m_ConsoleType);
}

void CGameConsole::OnReset()
{
	m_RemoteConsole.Reset();
}

// only defined for 0<=t<=1
static float ConsoleScaleFunc(float t)
{
	return std::sin(std::acos(1.0f - t));
}

struct CCompletionOptionRenderInfo
{
	CGameConsole *m_pSelf;
	CTextCursor m_Cursor;
	const char *m_pCurrentCmd;
	int m_WantedCompletion;
	float m_Offset;
	float *m_pOffsetChange;
	float m_Width;
	float m_TotalWidth;
};

void CGameConsole::PossibleCommandsRenderCallback(int Index, const char *pStr, void *pUser)
{
	CCompletionOptionRenderInfo *pInfo = static_cast<CCompletionOptionRenderInfo *>(pUser);

	ColorRGBA TextColor;
	if(Index == pInfo->m_WantedCompletion)
	{
		TextColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		const float TextWidth = pInfo->m_pSelf->TextRender()->TextWidth(pInfo->m_Cursor.m_FontSize, pStr);
		const CUIRect Rect = {pInfo->m_Cursor.m_X - 2.0f, pInfo->m_Cursor.m_Y - 2.0f, TextWidth + 4.0f, pInfo->m_Cursor.m_FontSize + 4.0f};
		Rect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.85f), IGraphics::CORNER_ALL, 2.0f);

		// scroll when out of sight
		const bool MoveLeft = Rect.x - *pInfo->m_pOffsetChange < 0.0f;
		const bool MoveRight = Rect.x + Rect.w - *pInfo->m_pOffsetChange > pInfo->m_Width;
		if(MoveLeft && !MoveRight)
		{
			*pInfo->m_pOffsetChange -= -Rect.x + pInfo->m_Width / 4.0f;
		}
		else if(!MoveLeft && MoveRight)
		{
			*pInfo->m_pOffsetChange += Rect.x + Rect.w - pInfo->m_Width + pInfo->m_Width / 4.0f;
		}
	}
	else
	{
		TextColor = ColorRGBA(0.75f, 0.75f, 0.75f, 1.0f);
	}

	const char *pMatchStart = str_find_nocase(pStr, pInfo->m_pCurrentCmd);
	if(pMatchStart)
	{
		pInfo->m_pSelf->TextRender()->TextColor(TextColor);
		pInfo->m_pSelf->TextRender()->TextEx(&pInfo->m_Cursor, pStr, pMatchStart - pStr);
		pInfo->m_pSelf->TextRender()->TextColor(1.0f, 0.75f, 0.0f, 1.0f);
		pInfo->m_pSelf->TextRender()->TextEx(&pInfo->m_Cursor, pMatchStart, str_length(pInfo->m_pCurrentCmd));
		pInfo->m_pSelf->TextRender()->TextColor(TextColor);
		pInfo->m_pSelf->TextRender()->TextEx(&pInfo->m_Cursor, pMatchStart + str_length(pInfo->m_pCurrentCmd));
	}
	else
	{
		pInfo->m_pSelf->TextRender()->TextColor(TextColor);
		pInfo->m_pSelf->TextRender()->TextEx(&pInfo->m_Cursor, pStr);
	}

	pInfo->m_Cursor.m_X += 7.0f;
	pInfo->m_TotalWidth = pInfo->m_Cursor.m_X + pInfo->m_Offset;
}

void CGameConsole::Prompt(char (&aPrompt)[32])
{
	CInstance *pConsole = CurrentConsole();
	if(pConsole->m_Searching)
	{
		str_format(aPrompt, sizeof(aPrompt), "%s: ", Localize("Searching"));
	}
	else if(m_ConsoleType == CONSOLETYPE_REMOTE)
	{
		if(Client()->State() == IClient::STATE_LOADING || Client()->State() == IClient::STATE_ONLINE)
		{
			if(Client()->RconAuthed())
				str_copy(aPrompt, "rcon> ");
			else if(pConsole->m_UsernameReq && !pConsole->m_UserGot)
				str_format(aPrompt, sizeof(aPrompt), "%s> ", Localize("Enter Username"));
			else
				str_format(aPrompt, sizeof(aPrompt), "%s> ", Localize("Enter Password"));
		}
		else
			str_format(aPrompt, sizeof(aPrompt), "%s> ", Localize("NOT CONNECTED"));
	}
	else
	{
		str_copy(aPrompt, "> ");
	}
}

void CGameConsole::OnRender()
{
	CUIRect Screen = *Ui()->Screen();
	CInstance *pConsole = CurrentConsole();

	const float MaxConsoleHeight = Screen.h * 3 / 5.0f;
	float Progress = (Client()->GlobalTime() - (m_StateChangeEnd - m_StateChangeDuration)) / m_StateChangeDuration;

	if(Progress >= 1.0f)
	{
		if(m_ConsoleState == CONSOLE_CLOSING)
		{
			m_ConsoleState = CONSOLE_CLOSED;
			pConsole->m_BacklogLastActiveLine = -1;
		}
		else if(m_ConsoleState == CONSOLE_OPENING)
		{
			m_ConsoleState = CONSOLE_OPEN;
			pConsole->m_Input.Activate(EInputPriority::CONSOLE);
		}

		Progress = 1.0f;
	}

	if(m_ConsoleState == CONSOLE_OPEN && g_Config.m_ClEditor)
		Toggle(CONSOLETYPE_LOCAL);

	if(m_ConsoleState == CONSOLE_CLOSED)
		return;

	if(m_ConsoleState == CONSOLE_OPEN)
		Input()->MouseModeAbsolute();

	float ConsoleHeightScale;
	if(m_ConsoleState == CONSOLE_OPENING)
		ConsoleHeightScale = ConsoleScaleFunc(Progress);
	else if(m_ConsoleState == CONSOLE_CLOSING)
		ConsoleHeightScale = ConsoleScaleFunc(1.0f - Progress);
	else // CONSOLE_OPEN
		ConsoleHeightScale = ConsoleScaleFunc(1.0f);

	const float ConsoleHeight = ConsoleHeightScale * MaxConsoleHeight;

	const ColorRGBA ShadowColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f);
	const ColorRGBA TransparentColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
	const ColorRGBA aBackgroundColors[NUM_CONSOLETYPES] = {ColorRGBA(0.2f, 0.2f, 0.2f, 0.9f), ColorRGBA(0.4f, 0.2f, 0.2f, 0.9f)};
	const ColorRGBA aBorderColors[NUM_CONSOLETYPES] = {ColorRGBA(0.1f, 0.1f, 0.1f, 0.9f), ColorRGBA(0.2f, 0.1f, 0.1f, 0.9f)};

	Ui()->MapScreen();

	// background
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BACKGROUND_NOISE].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(aBackgroundColors[m_ConsoleType]);
	Graphics()->QuadsSetSubset(0, 0, Screen.w / 80.0f, ConsoleHeight / 80.0f);
	IGraphics::CQuadItem QuadItemBackground(0.0f, 0.0f, Screen.w, ConsoleHeight);
	Graphics()->QuadsDrawTL(&QuadItemBackground, 1);
	Graphics()->QuadsEnd();

	// bottom border
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(aBorderColors[m_ConsoleType]);
	IGraphics::CQuadItem QuadItemBorder(0.0f, ConsoleHeight, Screen.w, 1.0f);
	Graphics()->QuadsDrawTL(&QuadItemBorder, 1);
	Graphics()->QuadsEnd();

	// bottom shadow
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor4(ShadowColor, ShadowColor, TransparentColor, TransparentColor);
	IGraphics::CQuadItem QuadItemShadow(0.0f, ConsoleHeight + 1.0f, Screen.w, 10.0f);
	Graphics()->QuadsDrawTL(&QuadItemShadow, 1);
	Graphics()->QuadsEnd();

	{
		// Get height of 1 line
		const float LineHeight = TextRender()->TextBoundingBox(FONT_SIZE, " ", -1, -1.0f, LINE_SPACING).m_H;

		const float RowHeight = FONT_SIZE * 2.0f;

		float x = 3;
		float y = ConsoleHeight - RowHeight - 18.0f;

		const float InitialX = x;
		const float InitialY = y;

		// render prompt
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x, y + FONT_SIZE / 2.0f, FONT_SIZE, TEXTFLAG_RENDER);

		char aPrompt[32];
		Prompt(aPrompt);
		TextRender()->TextEx(&Cursor, aPrompt);

		// check if mouse is pressed
		const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
		const vec2 ScreenSize = vec2(Screen.w, Screen.h);
		Ui()->UpdateTouchState(m_TouchState);
		const auto &&GetMousePosition = [&]() -> vec2 {
			if(m_TouchState.m_PrimaryPressed)
			{
				return m_TouchState.m_PrimaryPosition * ScreenSize;
			}
			else
			{
				return Input()->NativeMousePos() / WindowSize * ScreenSize;
			}
		};
		if(!pConsole->m_MouseIsPress && (m_TouchState.m_PrimaryPressed || Input()->NativeMousePressed(1)))
		{
			pConsole->m_MouseIsPress = true;
			pConsole->m_MousePress = GetMousePosition();
		}
		if(pConsole->m_MouseIsPress && !m_TouchState.m_PrimaryPressed && !Input()->NativeMousePressed(1))
		{
			pConsole->m_MouseIsPress = false;
		}
		if(pConsole->m_MouseIsPress)
		{
			pConsole->m_MouseRelease = GetMousePosition();
		}
		const float ScaledRowHeight = RowHeight / ScreenSize.y;
		if(absolute(m_TouchState.m_ScrollAmount.y) >= ScaledRowHeight)
		{
			if(m_TouchState.m_ScrollAmount.y > 0.0f)
			{
				pConsole->m_BacklogCurLine += pConsole->GetLinesToScroll(-1, 1);
				m_TouchState.m_ScrollAmount.y -= ScaledRowHeight;
			}
			else
			{
				--pConsole->m_BacklogCurLine;
				if(pConsole->m_BacklogCurLine < 0)
					pConsole->m_BacklogCurLine = 0;
				m_TouchState.m_ScrollAmount.y += ScaledRowHeight;
			}
			pConsole->m_HasSelection = false;
		}

		x = Cursor.m_X;

		if(m_ConsoleState == CONSOLE_OPEN)
		{
			if(pConsole->m_MousePress.y >= pConsole->m_BoundingBox.m_Y && pConsole->m_MousePress.y < pConsole->m_BoundingBox.m_Y + pConsole->m_BoundingBox.m_H)
			{
				CLineInput::SMouseSelection *pMouseSelection = pConsole->m_Input.GetMouseSelection();
				pMouseSelection->m_Selecting = pConsole->m_MouseIsPress;
				pMouseSelection->m_PressMouse = pConsole->m_MousePress;
				pMouseSelection->m_ReleaseMouse = pConsole->m_MouseRelease;
			}
			else if(pConsole->m_MouseIsPress)
			{
				pConsole->m_Input.SelectNothing();
			}
		}

		// render console input (wrap line)
		pConsole->m_Input.SetHidden(m_ConsoleType == CONSOLETYPE_REMOTE && Client()->State() == IClient::STATE_ONLINE && !Client()->RconAuthed() && (pConsole->m_UserGot || !pConsole->m_UsernameReq));
		if(m_ConsoleState == CONSOLE_OPEN)
		{
			pConsole->m_Input.Activate(EInputPriority::CONSOLE); // Ensure that the input is active
		}
		const CUIRect InputCursorRect = {x, y + FONT_SIZE * 1.5f, 0.0f, 0.0f};
		const bool WasChanged = pConsole->m_Input.WasChanged();
		const bool WasCursorChanged = pConsole->m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		pConsole->m_BoundingBox = pConsole->m_Input.Render(&InputCursorRect, FONT_SIZE, TEXTALIGN_BL, Changed, Screen.w - 10.0f - x, LINE_SPACING);
		if(pConsole->m_LastInputHeight == 0.0f && pConsole->m_BoundingBox.m_H != 0.0f)
			pConsole->m_LastInputHeight = pConsole->m_BoundingBox.m_H;
		if(pConsole->m_Input.HasSelection())
			pConsole->m_HasSelection = false; // Clear console selection if we have a line input selection

		y -= pConsole->m_BoundingBox.m_H - FONT_SIZE;
		TextRender()->SetCursor(&Cursor, x, y, FONT_SIZE, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Screen.w - 10.0f - x;
		Cursor.m_LineSpacing = LINE_SPACING;

		if(pConsole->m_LastInputHeight != pConsole->m_BoundingBox.m_H)
		{
			pConsole->m_HasSelection = false;
			pConsole->m_MouseIsPress = false;
			pConsole->m_LastInputHeight = pConsole->m_BoundingBox.m_H;
		}

		// render possible commands
		if(!pConsole->m_Searching && (m_ConsoleType == CONSOLETYPE_LOCAL || Client()->RconAuthed()) && !pConsole->m_Input.IsEmpty())
		{
			CCompletionOptionRenderInfo Info;
			Info.m_pSelf = this;
			Info.m_WantedCompletion = pConsole->m_CompletionChosen;
			Info.m_Offset = pConsole->m_CompletionRenderOffset;
			Info.m_pOffsetChange = &pConsole->m_CompletionRenderOffsetChange;
			Info.m_Width = Screen.w;
			Info.m_TotalWidth = 0.0f;
			char aCmd[IConsole::CMDLINE_LENGTH];
			pConsole->GetCommand(pConsole->m_aCompletionBuffer, aCmd);
			Info.m_pCurrentCmd = aCmd;

			TextRender()->SetCursor(&Info.m_Cursor, InitialX - Info.m_Offset, InitialY + RowHeight + 2.0f, FONT_SIZE, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Info.m_Cursor.m_LineWidth = std::numeric_limits<float>::max();
			const int NumCommands = m_pConsole->PossibleCommands(Info.m_pCurrentCmd, pConsole->m_CompletionFlagmask, m_ConsoleType != CGameConsole::CONSOLETYPE_LOCAL && Client()->RconAuthed() && Client()->UseTempRconCommands(), PossibleCommandsRenderCallback, &Info);
			pConsole->m_CompletionRenderOffset = Info.m_Offset;

			if(NumCommands <= 0 && pConsole->m_IsCommand)
			{
				const auto [CompletionType, _] = ArgumentCompletion(Info.m_pCurrentCmd);
				int NumArguments = 0;
				if(CompletionType != EArgumentCompletionType::NONE)
				{
					Info.m_WantedCompletion = pConsole->m_CompletionChosenArgument;
					Info.m_TotalWidth = 0.0f;
					Info.m_pCurrentCmd = pConsole->m_aCompletionBufferArgument;
					if(CompletionType == EArgumentCompletionType::TUNE)
						NumArguments = PossibleTunings(Info.m_pCurrentCmd, PossibleCommandsRenderCallback, &Info);
					else if(CompletionType == EArgumentCompletionType::SETTING)
						NumArguments = m_pConsole->PossibleCommands(Info.m_pCurrentCmd, pConsole->m_CompletionFlagmask, m_ConsoleType != CGameConsole::CONSOLETYPE_LOCAL && Client()->RconAuthed() && Client()->UseTempRconCommands(), PossibleCommandsRenderCallback, &Info);
					else if(CompletionType == EArgumentCompletionType::KEY)
						NumArguments = PossibleKeys(Info.m_pCurrentCmd, Input(), PossibleCommandsRenderCallback, &Info);
					pConsole->m_CompletionRenderOffset = Info.m_Offset;
				}

				if(NumArguments <= 0 && pConsole->m_IsCommand)
				{
					char aBuf[1024];
					str_format(aBuf, sizeof(aBuf), "Help: %s ", pConsole->m_pCommandHelp);
					TextRender()->TextEx(&Info.m_Cursor, aBuf, -1);
					TextRender()->TextColor(0.75f, 0.75f, 0.75f, 1);
					str_format(aBuf, sizeof(aBuf), "Usage: %s %s", pConsole->m_pCommandName, pConsole->m_pCommandParams);
					TextRender()->TextEx(&Info.m_Cursor, aBuf, -1);
				}
			}

			Ui()->DoSmoothScrollLogic(&pConsole->m_CompletionRenderOffset, &pConsole->m_CompletionRenderOffsetChange, Info.m_Width, Info.m_TotalWidth);
		}
		else if(pConsole->m_Searching && !pConsole->m_Input.IsEmpty())
		{ // Render current match and match count
			CTextCursor MatchInfoCursor;
			TextRender()->SetCursor(&MatchInfoCursor, InitialX, InitialY + RowHeight + 2.0f, FONT_SIZE, TEXTFLAG_RENDER);
			TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
			if(!pConsole->m_vSearchMatches.empty())
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), Localize("Match %d of %d"), pConsole->m_CurrentMatchIndex + 1, (int)pConsole->m_vSearchMatches.size());
				TextRender()->TextEx(&MatchInfoCursor, aBuf, -1);
			}
			else
			{
				TextRender()->TextEx(&MatchInfoCursor, Localize("No results"), -1);
			}
		}

		pConsole->PumpBacklogPending();
		if(pConsole->m_NewLineCounter != 0)
		{
			pConsole->UpdateSearch();

			// keep scroll position when new entries are printed.
			if(pConsole->m_BacklogCurLine != 0 || pConsole->m_HasSelection)
			{
				pConsole->m_BacklogCurLine += pConsole->m_NewLineCounter;
				pConsole->m_BacklogLastActiveLine += pConsole->m_NewLineCounter;
			}
			if(pConsole->m_NewLineCounter < 0)
				pConsole->m_NewLineCounter = 0;
		}

		// render console log (current entry, status, wrap lines)
		CInstance::CBacklogEntry *pEntry = pConsole->m_Backlog.Last();
		float OffsetY = 0.0f;

		std::string SelectionString;

		if(pConsole->m_BacklogLastActiveLine < 0)
			pConsole->m_BacklogLastActiveLine = pConsole->m_BacklogCurLine;

		int LineNum = -1;
		pConsole->m_LinesRendered = 0;

		int SkippedLines = 0;
		bool First = true;

		const float XScale = Graphics()->ScreenWidth() / Screen.w;
		const float YScale = Graphics()->ScreenHeight() / Screen.h;
		const float CalcOffsetY = LineHeight * std::floor((y - RowHeight) / LineHeight);
		const float ClipStartY = (y - CalcOffsetY) * YScale;
		Graphics()->ClipEnable(0, ClipStartY, Screen.w * XScale, (y + 2.0f) * YScale - ClipStartY);

		while(pEntry)
		{
			if(pEntry->m_LineCount == -1)
				pConsole->UpdateEntryTextAttributes(pEntry);

			LineNum += pEntry->m_LineCount;
			if(LineNum < pConsole->m_BacklogLastActiveLine)
			{
				SkippedLines += pEntry->m_LineCount;
				pEntry = pConsole->m_Backlog.Prev(pEntry);
				continue;
			}
			TextRender()->TextColor(pEntry->m_PrintColor);

			if(First)
			{
				OffsetY -= (pConsole->m_BacklogLastActiveLine - SkippedLines) * LineHeight;
			}

			const float LocalOffsetY = OffsetY + pEntry->m_YOffset / (float)pEntry->m_LineCount;
			OffsetY += pEntry->m_YOffset;

			// Only apply offset if we do not keep scroll position (m_BacklogCurLine == 0)
			if((pConsole->m_HasSelection || pConsole->m_MouseIsPress) && pConsole->m_NewLineCounter > 0 && pConsole->m_BacklogCurLine == 0)
			{
				pConsole->m_MousePress.y -= pEntry->m_YOffset;
				if(!pConsole->m_MouseIsPress)
					pConsole->m_MouseRelease.y -= pEntry->m_YOffset;
			}

			// stop rendering when lines reach the top
			const bool Outside = y - OffsetY <= RowHeight;
			const bool CanRenderOneLine = y - LocalOffsetY > RowHeight;
			if(Outside && !CanRenderOneLine)
				break;

			const int LinesNotRendered = pEntry->m_LineCount - minimum((int)std::floor((y - LocalOffsetY) / RowHeight), pEntry->m_LineCount);
			pConsole->m_LinesRendered -= LinesNotRendered;

			TextRender()->SetCursor(&Cursor, 0.0f, y - OffsetY, FONT_SIZE, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = Screen.w - 10.0f;
			Cursor.m_MaxLines = pEntry->m_LineCount;
			Cursor.m_LineSpacing = LINE_SPACING;
			Cursor.m_CalculateSelectionMode = (m_ConsoleState == CONSOLE_OPEN && pConsole->m_MousePress.y < pConsole->m_BoundingBox.m_Y && (pConsole->m_MouseIsPress || (pConsole->m_CurSelStart != pConsole->m_CurSelEnd) || pConsole->m_HasSelection)) ? TEXT_CURSOR_SELECTION_MODE_CALCULATE : TEXT_CURSOR_SELECTION_MODE_NONE;
			Cursor.m_PressMouse = pConsole->m_MousePress;
			Cursor.m_ReleaseMouse = pConsole->m_MouseRelease;

			if(pConsole->m_Searching && pConsole->m_CurrentMatchIndex != -1)
			{
				std::vector<CInstance::SSearchMatch> vMatches;
				std::copy_if(pConsole->m_vSearchMatches.begin(), pConsole->m_vSearchMatches.end(), std::back_inserter(vMatches), [&](const CInstance::SSearchMatch &Match) { return Match.m_EntryLine == LineNum + 1 - pEntry->m_LineCount; });

				auto CurrentSelectedOccurrence = pConsole->m_vSearchMatches[pConsole->m_CurrentMatchIndex];

				std::vector<STextColorSplit> vColorSplits;
				for(const auto &Match : vMatches)
				{
					bool IsSelected = CurrentSelectedOccurrence.m_EntryLine == Match.m_EntryLine && CurrentSelectedOccurrence.m_Pos == Match.m_Pos;
					Cursor.m_vColorSplits.emplace_back(
						Match.m_Pos,
						pConsole->m_Input.GetLength(),
						IsSelected ? ms_SearchSelectedColor : ms_SearchHighlightColor);
				}
			}

			TextRender()->TextEx(&Cursor, pEntry->m_aText, -1);
			Cursor.m_vColorSplits = {};

			if(Cursor.m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
			{
				pConsole->m_CurSelStart = minimum(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);
				pConsole->m_CurSelEnd = maximum(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);
			}
			pConsole->m_LinesRendered += First ? pEntry->m_LineCount - (pConsole->m_BacklogLastActiveLine - SkippedLines) : pEntry->m_LineCount;

			if(pConsole->m_CurSelStart != pConsole->m_CurSelEnd)
			{
				if(m_WantsSelectionCopy)
				{
					const bool HasNewLine = !SelectionString.empty();
					const size_t OffUTF8Start = str_utf8_offset_chars_to_bytes(pEntry->m_aText, pConsole->m_CurSelStart);
					const size_t OffUTF8End = str_utf8_offset_chars_to_bytes(pEntry->m_aText, pConsole->m_CurSelEnd);
					SelectionString.insert(0, (std::string(&pEntry->m_aText[OffUTF8Start], OffUTF8End - OffUTF8Start) + (HasNewLine ? "\n" : "")));
				}
				pConsole->m_HasSelection = true;
			}

			if(pConsole->m_NewLineCounter > 0) // Decrease by the entry line count since we can have multiline entries
				pConsole->m_NewLineCounter -= pEntry->m_LineCount;

			pEntry = pConsole->m_Backlog.Prev(pEntry);

			// reset color
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			First = false;

			if(!pEntry)
				break;
		}

		// Make sure to reset m_NewLineCounter when we are done drawing
		// This is because otherwise, if many entries are printed at once while console is
		// hidden, m_NewLineCounter will always be > 0 since the console won't be able to render
		// them all, thus wont be able to decrease m_NewLineCounter to 0.
		// This leads to an infinite increase of m_BacklogCurLine and m_BacklogLastActiveLine
		// when we want to keep scroll position.
		pConsole->m_NewLineCounter = 0;

		Graphics()->ClipDisable();

		pConsole->m_BacklogLastActiveLine = pConsole->m_BacklogCurLine;

		if(m_WantsSelectionCopy && !SelectionString.empty())
		{
			pConsole->m_HasSelection = false;
			pConsole->m_CurSelStart = -1;
			pConsole->m_CurSelEnd = -1;
			Input()->SetClipboardText(SelectionString.c_str());
			m_WantsSelectionCopy = false;
		}

		TextRender()->TextColor(TextRender()->DefaultTextColor());

		// render current lines and status (locked, following)
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), Localize("Lines %d - %d (%s)"), pConsole->m_BacklogCurLine + 1, pConsole->m_BacklogCurLine + pConsole->m_LinesRendered, pConsole->m_BacklogCurLine != 0 ? Localize("Locked") : Localize("Following"));
		TextRender()->Text(10.0f, FONT_SIZE / 2.f, FONT_SIZE, aBuf);

		if(m_ConsoleType == CONSOLETYPE_REMOTE && Client()->ReceivingRconCommands())
		{
			float Percentage = Client()->GotRconCommandsPercentage();
			SProgressSpinnerProperties ProgressProps;
			ProgressProps.m_Progress = Percentage;
			Ui()->RenderProgressSpinner(vec2(Screen.w / 4.0f + FONT_SIZE / 2.f, FONT_SIZE), FONT_SIZE / 2.f, ProgressProps);

			char aLoading[128];
			str_copy(aLoading, Localize("Loading commands…"));
			if(Percentage > 0)
			{
				char aPercentage[8];
				str_format(aPercentage, sizeof(aPercentage), " %d%%", (int)(Percentage * 100));
				str_append(aLoading, aPercentage);
			}
			TextRender()->Text(Screen.w / 4.0f + FONT_SIZE + 2.0f, FONT_SIZE / 2.f, FONT_SIZE, aLoading);
		}

		// render version
		str_copy(aBuf, "v" GAME_VERSION " on " CONF_PLATFORM_STRING " " CONF_ARCH_STRING);
		TextRender()->Text(Screen.w - TextRender()->TextWidth(FONT_SIZE, aBuf) - 10.0f, FONT_SIZE / 2.f, FONT_SIZE, aBuf);
	}
}

void CGameConsole::OnMessage(int MsgType, void *pRawMsg)
{
}

bool CGameConsole::OnInput(const IInput::CEvent &Event)
{
	// accept input when opening, but not at first frame to discard the input that caused the console to open
	if(m_ConsoleState != CONSOLE_OPEN && (m_ConsoleState != CONSOLE_OPENING || m_StateChangeEnd == Client()->GlobalTime() + m_StateChangeDuration))
		return false;
	if((Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F12) || (Event.m_Key >= KEY_F13 && Event.m_Key <= KEY_F24))
		return false;

	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS) && !CurrentConsole()->m_Searching)
		Toggle(m_ConsoleType);
	else if(!CurrentConsole()->OnInput(Event))
	{
		if(m_pClient->Input()->ModifierIsPressed() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_C)
			m_WantsSelectionCopy = true;
	}

	return true;
}

void CGameConsole::Toggle(int Type)
{
	if(m_ConsoleType != Type && (m_ConsoleState == CONSOLE_OPEN || m_ConsoleState == CONSOLE_OPENING))
	{
		// don't toggle console, just switch what console to use
	}
	else
	{
		if(m_ConsoleState == CONSOLE_CLOSED || m_ConsoleState == CONSOLE_OPEN)
		{
			m_StateChangeEnd = Client()->GlobalTime() + m_StateChangeDuration;
		}
		else
		{
			float Progress = m_StateChangeEnd - Client()->GlobalTime();
			float ReversedProgress = m_StateChangeDuration - Progress;

			m_StateChangeEnd = Client()->GlobalTime() + ReversedProgress;
		}

		if(m_ConsoleState == CONSOLE_CLOSED || m_ConsoleState == CONSOLE_CLOSING)
		{
			Ui()->SetEnabled(false);
			m_ConsoleState = CONSOLE_OPENING;
		}
		else
		{
			ConsoleForType(Type)->m_Input.Deactivate();
			Input()->MouseModeRelative();
			Ui()->SetEnabled(true);
			m_pClient->OnRelease();
			m_ConsoleState = CONSOLE_CLOSING;
		}
	}
	m_ConsoleType = Type;
}

void CGameConsole::ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Toggle(CONSOLETYPE_LOCAL);
}

void CGameConsole::ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Toggle(CONSOLETYPE_REMOTE);
}

void CGameConsole::ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_LocalConsole.ClearBacklog();
}

void CGameConsole::ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_RemoteConsole.ClearBacklog();
}

void CGameConsole::ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_LocalConsole.Dump();
}

void CGameConsole::ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_RemoteConsole.Dump();
}

void CGameConsole::ConConsolePageUp(IConsole::IResult *pResult, void *pUserData)
{
	CInstance *pConsole = ((CGameConsole *)pUserData)->CurrentConsole();
	pConsole->m_BacklogCurLine += pConsole->GetLinesToScroll(-1, pConsole->m_LinesRendered);
	pConsole->m_HasSelection = false;
}

void CGameConsole::ConConsolePageDown(IConsole::IResult *pResult, void *pUserData)
{
	CInstance *pConsole = ((CGameConsole *)pUserData)->CurrentConsole();
	pConsole->m_BacklogCurLine -= pConsole->GetLinesToScroll(1, pConsole->m_LinesRendered);
	pConsole->m_HasSelection = false;
	if(pConsole->m_BacklogCurLine < 0)
		pConsole->m_BacklogCurLine = 0;
}

void CGameConsole::ConchainConsoleOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CGameConsole *pSelf = (CGameConsole *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		pSelf->m_pConsoleLogger->SetFilter(CLogFilter{IConsole::ToLogLevelFilter(g_Config.m_ConsoleOutputLevel)});
	}
}

void CGameConsole::RequireUsername(bool UsernameReq)
{
	if((m_RemoteConsole.m_UsernameReq = UsernameReq))
	{
		m_RemoteConsole.m_aUser[0] = '\0';
		m_RemoteConsole.m_UserGot = false;
	}
}

void CGameConsole::PrintLine(int Type, const char *pLine)
{
	if(Type == CONSOLETYPE_LOCAL)
		m_LocalConsole.PrintLine(pLine, str_length(pLine), TextRender()->DefaultTextColor());
	else if(Type == CONSOLETYPE_REMOTE)
		m_RemoteConsole.PrintLine(pLine, str_length(pLine), TextRender()->DefaultTextColor());
}

void CGameConsole::OnConsoleInit()
{
	// init console instances
	m_LocalConsole.Init(this);
	m_RemoteConsole.Init(this);

	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("toggle_local_console", "", CFGFLAG_CLIENT, ConToggleLocalConsole, this, "Toggle local console");
	Console()->Register("toggle_remote_console", "", CFGFLAG_CLIENT, ConToggleRemoteConsole, this, "Toggle remote console");
	Console()->Register("clear_local_console", "", CFGFLAG_CLIENT, ConClearLocalConsole, this, "Clear local console");
	Console()->Register("clear_remote_console", "", CFGFLAG_CLIENT, ConClearRemoteConsole, this, "Clear remote console");
	Console()->Register("dump_local_console", "", CFGFLAG_CLIENT, ConDumpLocalConsole, this, "Write local console contents to a text file");
	Console()->Register("dump_remote_console", "", CFGFLAG_CLIENT, ConDumpRemoteConsole, this, "Write remote console contents to a text file");

	Console()->Register("console_page_up", "", CFGFLAG_CLIENT, ConConsolePageUp, this, "Previous page in console");
	Console()->Register("console_page_down", "", CFGFLAG_CLIENT, ConConsolePageDown, this, "Next page in console");
	Console()->Chain("console_output_level", ConchainConsoleOutputLevel, this);
}

void CGameConsole::OnInit()
{
	Engine()->SetAdditionalLogger(std::unique_ptr<ILogger>(m_pConsoleLogger));
	// add resize event
	Graphics()->AddWindowResizeListener([this]() {
		m_LocalConsole.UpdateBacklogTextAttributes();
		m_LocalConsole.m_HasSelection = false;
		m_RemoteConsole.UpdateBacklogTextAttributes();
		m_RemoteConsole.m_HasSelection = false;
	});
}

void CGameConsole::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_ONLINE && NewState == IClient::STATE_OFFLINE)
	{
		m_RemoteConsole.m_UserGot = false;
		m_RemoteConsole.m_aUser[0] = '\0';
		m_RemoteConsole.m_Input.Clear();
		m_RemoteConsole.m_UsernameReq = false;
	}
}
