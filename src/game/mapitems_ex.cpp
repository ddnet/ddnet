#include "mapitems_ex.h"

#include <base/math.h>
#include <engine/shared/uuid_manager.h>

int GetParallaxZoom(const CMapItemGroup *pGroup, const CMapItemGroupEx *pGroupEx)
{
	if(pGroupEx)
		return pGroupEx->m_ParallaxZoom;

	return GetParallaxZoomDefault(pGroup->m_ParallaxX, pGroup->m_ParallaxY);
}

int GetParallaxZoomDefault(int ParallaxX, int ParallaxY)
{
	return clamp(maximum(ParallaxX, ParallaxY), 0, 100);
}

void RegisterMapItemTypeUuids(CUuidManager *pManager)
{
#define UUID(id, name) pManager->RegisterName(id, name);
#include "mapitems_ex_types.h"
#undef UUID
}
