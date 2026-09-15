#ifndef ENGINE_CLIENT_KEYBOARD_H
#define ENGINE_CLIENT_KEYBOARD_H

const char *KeyName(int Key);
const char *KeyNameHumanReadable(int Key);

void GenerateKeyNameHumanReadable(char *pHumanReadableKeyName, int Key, int KeyNameSize);

template<int N>
void GenerateKeyNameHumanReadable(char (&aHumanReadableKeyName)[N], int Key)
{
	GenerateKeyNameHumanReadable(aHumanReadableKeyName, Key, N);
}

#endif
