#include "teehistorian_ex.h"

#include <engine/shared/uuid_manager.h>

void RegisterTeehistorianUuids(CUuidManager *pManager)
{
#define UUID(id, name) pManager->RegisterName(id, name);
#include "teehistorian_ex_chunks.h"
#undef UUID
}
