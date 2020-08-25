#ifndef ENGINE_STEAM_H
#define ENGINE_STEAM_H

#include "kernel.h"

class ISteam : public IInterface
{
	MACRO_INTERFACE("steam", 0)
public:
	// Returns NULL if the name cannot be determined.
	virtual const char *GetPlayerName() = 0;
};

ISteam *CreateSteam();

#endif // ENGINE_STEAM_H
