#include "keyboard.h"

#include "keynames.h"

#include <base/dbg.h>

#include <engine/keys.h>

const char *KeyName(int Key)
{
	dbg_assert(Key >= KEY_FIRST && Key < KEY_LAST, "Key invalid: %d", Key);
	return g_aaKeyStrings[Key];
}
