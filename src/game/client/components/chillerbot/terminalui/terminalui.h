
#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_TERMINALUI_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_TERMINALUI_H

#include <game/client/component.h>

#if defined(CONF_CURSES_CLIENT)
#include "terminalui_keys.h" /* undefines conflicting ncurses key codes */

#include <base/curses.h>

#include <unistd.h>
#endif /* CONF_CURSES_CLIENT */

extern CGameClient *g_pClient;

class CTerminalUI : public CComponent
{
#if defined(CONF_CURSES_CLIENT)
	enum
	{
		KEY_HISTORY_LEN = 20
	};

	void DrawBorders(WINDOW *screen, int x, int y, int w, int h);
	void DrawBorders(WINDOW *screen);
	void RenderScoreboard(int Team, WINDOW *pWin);
	void RenderServerList();
	void RenderHelpPage();
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

		NC_INFO_SIZE = 3,
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
		for(int i = INPUT_HISTORY_MAX_LEN; i > 0; i--)
		{
			str_copy(m_aaInputHistory[Type][i], m_aaInputHistory[Type][i - 1], sizeof(m_aaInputHistory[Type][i]));
		}
		str_copy(m_aaInputHistory[Type][0], pInput, sizeof(m_aaInputHistory[Type][0]));
	}

	virtual void OnInit() override;
	virtual void OnRender() override;
	virtual void OnShutdown() override;
	virtual void OnStateChange(int NewState, int OldState) override;
	void RenderConnecting();
	bool RenderDownload();
	bool RconAuthed() { return Client()->RconAuthed(); }
	int GetInput();
	void DrawAllBorders();
	void LogDraw();
	void InfoDraw();
	void InputDraw();
	int CursesTick();
	void ResetCompletion();
	void SetServerBrowserPage(int NewPage);
	void RenderPopup();
	bool IsSearchInputMode() { return m_InputMode > NUM_INPUTS; }
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
	bool DoPopup(int Popup, const char *pTitle);
	enum
	{
		POPUP_NONE = 0,
		// POPUP_CONNECTING, // TODO: use this
		POPUP_MESSAGE,
		POPUP_DISCONNECTED, // TODO: implement
		// POPUP_QUIT, // TODO: implement
		POPUP_WARNING, // TODO: implement
	};
	int AimX;
	int AimY;
	bool m_ScoreboardActive;
	bool m_RenderServerList;
	bool m_RenderHelpPage;
	int m_SelectedServer;
	int m_InputMode;
	int64_t m_LastKeyPress;
	char m_LastKeyPressed;
	int m_NumServers;
	bool m_NewInput;
	int m_InputCursor;
	int m_CompletionIndex;
	int m_LastCompletionLength;
	char m_aCompletionBuffer[1024];

	// render in game
	void RenderGame();
	void RenderTilemap(CTile *pTiles, int offX, int offY, int WinWidth, int WinHeight, int w, int h, float Scale, vec4 Color, int RenderFlags, int ColorEnv, int ColorEnvOffset);
	int m_NextRender; // TODO: remove

public:
	int OnKeyPress(int Key, WINDOW *pWin);
#endif
public:
	CNetObj_PlayerInput m_InputData[NUM_DUMMIES];
	bool m_SendData[NUM_DUMMIES];

	virtual int Sizeof() const override { return sizeof(*this); }
};

#endif
