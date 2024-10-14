
#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_PAD_UTF8_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_TERMINALUI_PAD_UTF8_H

#if defined(CONF_CURSES_CLIENT)

#include <engine/shared/rust_version.h>

/*
    Function: str_pad_right_utf8

    Pad string with spaces supporting more than ascii
    Supporting also CJK full width characters

    using the rust crate unicode-width under the hood
*/
void str_pad_right_utf8(char *pStr, int size, int pad_len);

#endif

#endif
