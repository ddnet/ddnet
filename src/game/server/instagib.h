// do we need some gctf system.h?

#ifndef GAME_SERVER_INSTAGIB_H
#define GAME_SERVER_INSTAGIB_H

#include "stdio.h"
#include <cstring>
#include <cctype>

#include <base/system.h>

const char *str_find_digit(const char *haystack);
bool str_contains_ip(const char *pStr);

#endif
