#ifndef BASE_TERMINALUI_H
#define BASE_TERMINALUI_H

#include "system.h"

#if defined(CONF_CURSES_CLIENT)

#include "ncurses.h"

#define CHILLER_LOGGER_WIDTH 1024 * 4
#define CHILLER_LOGGER_HEIGHT 128

extern WINDOW *g_pLogWindow;
extern WINDOW *g_pInfoWin;
extern WINDOW *g_pInputWin;

extern int g_ParentX;
extern int g_ParentY;
extern int g_NewX;
extern int g_NewY;
extern char g_aInfoStr[1024];
extern char g_aInfoStr2[1024];
extern char g_aInputStr[1024];
extern bool gs_NeedLogDraw;
extern int gs_LogsPushed;
extern IOHANDLE gs_Logfile;

void curses_init();
void log_draw();
void curses_refresh_windows();
void draw_borders(WINDOW *screen);
void curses_log_push(const char *pStr);
// void curses_logf(const char *sys, const char *fmt, ...);

#endif

#endif
