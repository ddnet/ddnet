
#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_TERMINALUI_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_TERMINALUI_H

#include <game/client/component.h>

#if defined(CONF_CURSES_CLIENT)
#include "terminalui_keys.h" /* undefines conflicting ncurses key codes */

#include <base/curses.h>

#include <unistd.h>
#endif /* CONF_CURSES_CLIENT */

extern CGameClient *g_pClient;

#define HEIGHT_NEEDED_FOR_SERVER_BROWSER_OFFSET_TOP 60

class CTerminalUI : public CComponent
{
#if defined(CONF_CURSES_CLIENT)
	enum
	{
		KEY_HISTORY_LEN = 20
	};

	class WindowInfo
	{
	public:
		WindowInfo()
		{
			m_X = 0;
			m_Y = 0;
			m_Width = 0;
			m_Height = 0;
		}
		int m_X;
		int m_Y;
		int m_Width;
		int m_Height;
	};
	WindowInfo m_WinServerBrowser;

	void RenderScoreboard(int Team, class CTermWindow *pWin);
	void OpenServerList();
	void RenderServerList();
	void RenderHelpPage();
	bool PickMenuItem();
	void CloseMenu();
	void OpenHelpPopup();
	int m_aLastPressedKey[KEY_HISTORY_LEN];
	bool KeyInHistory(int Key, int Ticks = KEY_HISTORY_LEN)
	{
		for(int i = 0; i < KEY_HISTORY_LEN && i < Ticks; i++)
			if(Key == m_aLastPressedKey[i])
				return true;
		return false;
	};
	void KeyHistoryToStr(char *pBuf, int Size)
	{
		str_copy(pBuf, "", Size);
		for(int i = 0; i < KEY_HISTORY_LEN; i++)
		{
			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), " %d/%c", m_aLastPressedKey[i], m_aLastPressedKey[i]);
			str_append(pBuf, aBuf, Size);
		}
	};
	void TrackKey(int Key)
	{
		for(int i = 0; i < KEY_HISTORY_LEN - 1; i++)
		{
			m_aLastPressedKey[i] = m_aLastPressedKey[i + 1];
		}
		m_aLastPressedKey[KEY_HISTORY_LEN - 1] = Key;
	};

	enum
	{
		INPUT_OFF = 0,
		INPUT_NORMAL,
		INPUT_LOCAL_CONSOLE,
		INPUT_REMOTE_CONSOLE,
		INPUT_CHAT,
		INPUT_CHAT_TEAM,
		INPUT_BROWSER_SEARCH,
		NUM_INPUTS,
		INPUT_SEARCH_LOCAL_CONSOLE,
		INPUT_SEARCH_REMOTE_CONSOLE,
		INPUT_SEARCH_CHAT,
		INPUT_SEARCH_CHAT_TEAM,
		INPUT_SEARCH_BROWSER_SEARCH,

		INPUT_HISTORY_MAX_LEN = 16,
	};
	const char *GetInputMode()
	{
		switch(m_InputMode)
		{
		case INPUT_OFF:
			return "OFF";
		case INPUT_NORMAL:
			return "NORMAL MODE";
		case INPUT_LOCAL_CONSOLE:
			return "LOCAL CONSOLE";
		case INPUT_REMOTE_CONSOLE:
			return "REMOTE CONSOLE";
		case INPUT_CHAT:
			return "CHAT";
		case INPUT_CHAT_TEAM:
			return "TEAM CHAT";
		case INPUT_BROWSER_SEARCH:
			return "INPUT_BROWSER_SEARCH";
		case INPUT_SEARCH_LOCAL_CONSOLE:
			return "SEARCH LOCAL CONSOLE";
		case INPUT_SEARCH_REMOTE_CONSOLE:
			return "SEARCH REMOTE CONSOLE";
		case INPUT_SEARCH_CHAT:
			return "SEARCH CHAT";
		case INPUT_SEARCH_CHAT_TEAM:
			return "SEARCH TEAM CHAT";
		case INPUT_SEARCH_BROWSER_SEARCH:
			return "SEARCH INPUT_BROWSER_SEARCH";
		default:
			return "INVALID";
		}
	};
	const char *GetInputModeSlug(int Mode)
	{
		switch(Mode)
		{
		case INPUT_OFF:
			return "off";
		case INPUT_NORMAL:
			return "normal_mode";
		case INPUT_LOCAL_CONSOLE:
			return "local_console";
		case INPUT_REMOTE_CONSOLE:
			return "remote_console";
		case INPUT_CHAT:
			return "chat";
		case INPUT_CHAT_TEAM:
			return "team_chat";
		case INPUT_BROWSER_SEARCH:
			return "input_browser_search";
		case INPUT_SEARCH_LOCAL_CONSOLE:
			return "search_local_console";
		case INPUT_SEARCH_REMOTE_CONSOLE:
			return "search_remote_console";
		case INPUT_SEARCH_CHAT:
			return "search_chat";
		case INPUT_SEARCH_CHAT_TEAM:
			return "search_team_chat";
		case INPUT_SEARCH_BROWSER_SEARCH:
			return "search_input_browser_search";
		default:
			return "invalid";
		}
	};

	/*
		m_LockKeyUntilRelease

		ncurses keycode of the locked key
		its input will be ignored until it is released

		the use case is that if for example a enter is used to submit
		a search string the enter is not also sent to a the new context
		which is then the search result.
	*/
	int m_LockKeyUntilRelease;

	/*
		m_aaInputHistory

		All submitted input strings for all inputs
		The lower the index the more recent is the entry
	*/
	char m_aaInputHistory[NUM_INPUTS][INPUT_HISTORY_MAX_LEN][1024];
	/*
		m_InputHistory

		All indecies of currently selected history entries
		pointing to the strings in m_aaInputHistory
	*/
	int m_InputHistory[NUM_INPUTS];

	void InputHistoryToStr(int Type, char *pBuf, int Size)
	{
		str_copy(pBuf, "", Size);
		for(int i = 0; i < INPUT_HISTORY_MAX_LEN; i++)
		{
			char aBuf[1024 + 16];
			str_format(aBuf, sizeof(aBuf), "%d: %s", i, m_aaInputHistory[Type][i]);
			str_append(pBuf, aBuf, Size);
		}
	};
	void AddInputHistory(int Type, const char *pInput)
	{
		// we get a free max hist size
		// if we dump our buffer into the file at the end
		// instead of appending on every command
		// SaveInputToHistoryFile(Type, pInput);
		for(int i = (INPUT_HISTORY_MAX_LEN - 1); i > 0; i--)
		{
			str_copy(m_aaInputHistory[Type][i], m_aaInputHistory[Type][i - 1], sizeof(m_aaInputHistory[Type][i]));
		}
		str_copy(m_aaInputHistory[Type][0], pInput, sizeof(m_aaInputHistory[Type][0]));
	}
	/*
		Function: ShiftHistoryFromHighToLow

		Expects a history that is not full as in less elements than INPUT_HISTORY_MAX_LEN
		And they should be at the upper bound of the array
		since the history search starts from the bottom and stops at the bottom
		this function fixes the history buffer by shifting the values down until
		they reach 0
	*/
	void ShiftHistoryFromHighToLow(int Type);
	/*
		Function: SaveInputToHistoryFile

		Appends a single input to a history file
		Currently no longer in use and replaced by
		SaveCurrentHistoryBufferToDisk()
		because we get a free max hist size
		if we dump our buffer into the file at the end
		instead of appending on every command
	*/
	bool SaveInputToHistoryFile(int Type, const char *pInput);
	bool SaveCurrentHistoryBufferToDisk(int Type);
	bool LoadInputHistoryFile(int Type);

	virtual void OnInit() override;
	virtual void OnConsoleInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnShutdown() override;
	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnMapLoad() override;

	static void ConTerm(IConsole::IResult *pResult, void *pUserData);

	void RenderConnecting();
	bool RenderDownload();
	bool RconAuthed() { return Client()->RconAuthed(); }
	int GetInput();
	void DrawAllBorders();
	void LogDraw();
	void InfoDraw();
	void InputDraw();
	int CursesTick();
	/*
		ClearInput

		Do not run `g_aInputStr[0] = '\0';`
		and call it a day

		Clearing the input cleanly is a bit more effort
		call this helper instead
	*/
	void ClearInput();
	void ResetCompletion();
	/*
		Function: ClearCompletionPreview

		Clears out the greyed out text that appears when tab completing
		console commands.
	*/
	void ClearCompletionPreview() { m_aCompletionPreview[0] = '\0'; }
	void SetServerBrowserPage(int NewPage);
	void RenderPopup();
	bool IsSearchInputMode() { return m_InputMode > NUM_INPUTS; }
	void OnInputModeChange(int Old, int New);
	/*
		UpdateCursor

		Update the terminal cursor position
		Used for the input textbox
	*/
	void UpdateCursor();
	/*
		_UpdateInputSearch

		When in reverse i search input mode.
		This function computes the m_aInputSearchMatch
		based on the current input search type (chat/console/etc)
		and the current search term m_aInputSearch

		Do not call me! Call my daddy RenderInputSearch()
	*/
	void _UpdateInputSearch();
	/*
		RenderInputSearch

		Implementation of the reverse i search mode
		Call this when the search term changed

		It will do a new search and redraw the input window
	*/
	void RenderInputSearch();
	/*
		m_aInputSearch

		When in reverse i search input mode.
		This variable holds the current search term.
	*/
	char m_aInputSearch[1024];
	/*
		m_aInputSearchMatch

		When in reverse i search input mode.
		This variable holds the current search match.
	*/
	char m_aInputSearchMatch[1024];
	/*
		m_Popup

		Curses UI popups inspired by CMenus::m_Popup
	*/
	int m_Popup;
	char m_aPopupTitle[128];
	enum
	{
		MAX_POPUP_BODY_WIDTH = 1024,
		MAX_POPUP_BODY_HEIGHT = 512,
	};
	char m_aaPopupBody[MAX_POPUP_BODY_HEIGHT][MAX_POPUP_BODY_WIDTH];
	size_t m_PopupBodyWidth;
	size_t m_PopupBodyHeight;
	bool DoPopup(int Popup, const char *pTitle, const char *pBody = nullptr, size_t BodyWidth = -1, size_t BodyHeight = -1);
	enum
	{
		POPUP_NONE = 0,
		// POPUP_CONNECTING, // TODO: use this
		/*
			POPUP_MESSAGE

			show a generic message that can be closed with an OK button
		*/
		POPUP_MESSAGE,
		/*
			POPUP_NOT_IMPORTANT

			show a generic message that can be closed with an OK button
			so very similar to POPUP_MESSAGE
			but it also closes it self when the user gets otherwise
			for example when the server browser is opend
		*/
		POPUP_NOT_IMPORTANT,
		POPUP_DISCONNECTED, // TODO: implement
		// POPUP_QUIT, // TODO: implement
		POPUP_WARNING, // TODO: implement
	};
	int AimX;
	int AimY;
	int m_BroadcastTick;
	bool m_ScoreboardActive;
	bool m_RenderServerList;
	bool m_RenderHelpPage;
	int m_SelectedServer;
	/*
		m_InputMode

		do not set directly use
		SetInputMode()
	*/
	int m_InputMode;
	void SetInputMode(int Mode);
	int InputMode() { return m_InputMode; }
	int64_t m_LastKeyPress;
	char m_LastKeyPressed;
	int m_NumServers;
	bool m_NewInput;
	int m_InputCursor;
	int m_CompletionIndex;
	int m_LastCompletionLength;
	char m_aCompletionBuffer[1024];
	char m_aCompletionPreview[1024];
	bool m_UpdateCompletionBuffer;
	/*
		m_CompletionChosen

		Somehow a dupe of m_CompletionIndex
		but m_CompletionIndex is used for my own completion implementation
		and m_CompletionChosen for a more teeworlds style implementation
	*/
	int m_CompletionChosen;
	int m_CompletionEnumerationCount;
	void CompleteNames(bool IsReverse = false);
	void CompleteCommands(bool IsReverse = false);

	char m_aCommandName[IConsole::TEMPCMD_NAME_LENGTH];
	char m_aCommandHelp[IConsole::TEMPCMD_HELP_LENGTH];
	char m_aCommandParams[IConsole::TEMPCMD_PARAMS_LENGTH];

	void RefreshConsoleCmdHelpText();

	static void PossibleCommandsCompleteCallback(int Index, const char *pStr, void *pUser);

	// render in game
	void RenderGame();
	void RenderItems();
	void RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted = false);
	void RenderTilemap(CTile *pTiles, int offX, int offY, int WinWidth, int WinHeight, int w, int h, float Scale, vec4 Color, int RenderFlags, int ColorEnv, int ColorEnvOffset);
	void RenderPlayers(int offX, int offY, int w, int h);
	bool IsPlayerInfoAvailable(int ClientId) const;
	int m_NextRender; // TODO: remove
	bool m_RenderGame;
	char m_aTileSolidTexture[16];
	char m_aTileUnhookTexture[16];

public:
	int OnKeyPress(int Key, WINDOW *pWin);
#endif
public:
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];
	bool m_SendData[NUM_DUMMIES];

	virtual int Sizeof() const override { return sizeof(*this); }
};

#endif
