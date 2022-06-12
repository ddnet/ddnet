#include "mapitems_ex.h"

#include <engine/shared/uuid_manager.h>

void RegisterMapItemTypeUuids(CUuidManager *pManager)
{
#define UUID(id, name) pManager->RegisterName(id, name);
#include "mapitems_ex_types.h"
#undef UUID
}
