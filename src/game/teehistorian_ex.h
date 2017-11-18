#ifndef GAME_TEEHISTORIAN_EX_H
#define GAME_TEEHISTORIAN_EX_H
#include <engine/shared/protocol_ex.h>

enum
{
	__TEEHISTORIAN_UUID_HELPER=OFFSET_GAME_UUID-1,
	#define UUID(id, name) id,
	#include "teehistorian_ex_chunks.h"
	#undef UUID
	OFFSET_TEEHISTORIAN_UUID
};

void RegisterTeehistorianUuids(class CUuidManager *pManager);
#endif // GAME_TEEHISTORIAN_EX_H
