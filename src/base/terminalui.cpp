#if defined(CONF_CURSES_CLIENT)

#include "ncurses.h"
#include <base/system.h>

#include "terminalui.h"

static char gs_aaChillerLogger[CHILLER_LOGGER_HEIGHT][CHILLER_LOGGER_WIDTH];

WINDOW *g_pLogWindow = NULL;
WINDOW *g_pInfoWin = NULL;
WINDOW *g_pInputWin = NULL;

int g_ParentX;
int g_ParentY;
int g_NewX;
int g_NewY;
char g_aInfoStr[1024];
char g_aInfoStr2[1024];
char g_aInputStr[1024];
bool gs_NeedLogDraw;

void curses_init()
{
	for(int i = 0; i < CHILLER_LOGGER_HEIGHT; i++)
		gs_aaChillerLogger[i][0] = '\0';
}

void curses_refresh_windows()
{
	if(gs_NeedLogDraw)
	{
		wrefresh(g_pLogWindow);
		wclear(g_pLogWindow);
		draw_borders(g_pLogWindow);
	}
	wrefresh(g_pInfoWin);
	wrefresh(g_pInputWin);
	gs_NeedLogDraw = false;
}

void log_draw()
{
	if(!gs_NeedLogDraw)
		return;
	int x, y;
	getmaxyx(g_pLogWindow, y, x);
	int Max = CHILLER_LOGGER_HEIGHT > y ? y : CHILLER_LOGGER_HEIGHT;
	int Top = CHILLER_LOGGER_HEIGHT - 2;
	int Bottom = CHILLER_LOGGER_HEIGHT - Max;
	int line = 0;
	for(int i = Top; i > Bottom; i--)
	{
		if(gs_aaChillerLogger[i][0] == '\0')
			continue; // TODO: break
		char aBuf[1024 * 4 + 16];
		str_format(aBuf, sizeof(aBuf), "%2d|%2d|%s", line++, i, gs_aaChillerLogger[i]);
		aBuf[x - 2] = '\0'; // prevent line wrapping and cut on screen border
		mvwprintw(g_pLogWindow, CHILLER_LOGGER_HEIGHT - i - 1, 1, aBuf);
	}
}

void draw_borders(WINDOW *screen)
{
	int x, y, i;

	getmaxyx(screen, y, x);

	// 4 corners
	mvwprintw(screen, 0, 0, "+");
	mvwprintw(screen, y - 1, 0, "+");
	mvwprintw(screen, 0, x - 1, "+");
	mvwprintw(screen, y - 1, x - 1, "+");

	// sides
	for(i = 1; i < (y - 1); i++)
	{
		mvwprintw(screen, i, 0, "|");
		mvwprintw(screen, i, x - 1, "|");
	}

	// top and bottom
	for(i = 1; i < (x - 1); i++)
	{
		mvwprintw(screen, 0, i, "-");
		mvwprintw(screen, y - 1, i, "-");
	}
	// if(m_aSrvBroadcast[0] != '\0' && screen == g_pLogWindow)
	// {
	// 	char aBuf[1024*4];
	// 	str_format(aBuf, sizeof(aBuf), "-[ %s ]", m_aSrvBroadcast);
	// 	aBuf[x-2] = '\0';
	// 	mvwprintw(screen, 0, 1, aBuf);
	// }
}

void curses_log_push(const char *pStr)
{
	// if ncurses is not intialized yet (terminalui.OnInit()) just print to stdout
	if(!g_pLogWindow)
	{
		puts(pStr);
		return;
	}
	// first empty slot
	int x, y;
	getmaxyx(g_pLogWindow, y, x);
	int Max = CHILLER_LOGGER_HEIGHT > y ? y : CHILLER_LOGGER_HEIGHT;
	int Top = CHILLER_LOGGER_HEIGHT - 2;
	int Bottom = CHILLER_LOGGER_HEIGHT - Max;
	str_format(g_aInfoStr, sizeof(g_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d top=%d bottom=%d                                            ",
		Max, CHILLER_LOGGER_HEIGHT, y, Top, Bottom
	);
	gs_NeedLogDraw = true;
	for(int i = Top; i > Bottom; i--)
	{
		if(gs_aaChillerLogger[i][0] == '\0')
		{
			str_copy(gs_aaChillerLogger[i], pStr, sizeof(gs_aaChillerLogger[i]));
			// str_format(g_aInfoStr, sizeof(g_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d i=%d", Max, CHILLER_LOGGER_HEIGHT, y, i);
			return;
		}
	}
	str_format(g_aInfoStr, sizeof(g_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d FULLL!!! top=%d bottom=%d                          ",
		Max, CHILLER_LOGGER_HEIGHT, y, Top, Bottom
	);
	// no free slot found -> shift all
	for(int i = Top; i > 0; i--)
	{
		str_copy(gs_aaChillerLogger[i], gs_aaChillerLogger[i - 1], sizeof(gs_aaChillerLogger[i]));
	}
	// insert newest on the bottom
	str_copy(gs_aaChillerLogger[Bottom + 1], pStr, sizeof(gs_aaChillerLogger[Bottom + 1]));
}

// ChillerDragon: no fucking idea why on macOS vdbg needs it but dbg doesn't
//				  yes this is a format vuln but only caused if used wrong same as in dbg_msg
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void curses_logf(const char *sys, const char *fmt, ...)
{
	va_list args;
	char str[1024 * 4];
	char *msg;
	int len;

	char timestr[80];
	str_timestamp_format(timestr, sizeof(timestr), FORMAT_SPACE);

	str_format(str, sizeof(str), "[%s][%s]: ", timestr, sys);

	len = strlen(str);
	msg = (char *)str + len;

	va_start(args, fmt);
#if defined(CONF_FAMILY_WINDOWS) && !defined(__GNUC__)
	_vsprintf_p(msg, sizeof(str) - len, fmt, args);
#else
	vsnprintf(msg, sizeof(str) - len, fmt, args);
#endif
	va_end(args);

	// printf("%s\n", str);
	curses_log_push(str);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
