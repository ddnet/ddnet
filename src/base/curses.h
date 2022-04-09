// This file can be included several times.

#define NCURSES_WIDECHAR 1
#if __has_include(<ncursesw/ncurses.h>)
#include <ncursesw/ncurses.h>
#elif __has_include(<cursesw.h>)
#include <cursesw.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#else
#include <curses.h>
#endif /* HAS_CURSES */
