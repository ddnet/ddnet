#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_H

#if defined(CONF_PLATFORM_LINUX)
#include "terminalui_keys.h" /* undefines conflicting ncurses key codes */
#include <ncurses.h>
#endif

#include <game/client/component.h>

class CTerminalUI : public CComponent
{
#if defined(CONF_PLATFORM_LINUX)
	enum
	{
		KEY_HISTORY_LEN = 20
	};

	void DrawBorders(WINDOW *screen, int x, int y, int w, int h);
	void DrawBorders(WINDOW *screen);
	void RenderScoreboard(int Team, WINDOW *pWin);
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

	virtual void OnInit();
	virtual void OnRender();
	virtual void OnShutdown();
	void GetInput();
	int AimX;
	int AimY;
	bool m_ScoreboardActive;
	WINDOW *m_pLogWindow;
	WINDOW *m_pInfoWin;
	WINDOW *m_pInputWin;

public:
	int OnKeyPress(int Key, WINDOW *pWin);
#endif
};

#endif
