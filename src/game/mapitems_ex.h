#ifndef GAME_MAPITEMS_EX_H
#define GAME_MAPITEMS_EX_H
#include <game/generated/protocol.h>

enum
{
	__MAPITEMTYPE_UUID_HELPER = OFFSET_MAPITEMTYPE_UUID - 1,
#define UUID(id, name) id,
#include "mapitems_ex_types.h"
#undef UUID
	END_MAPITEMTYPES_UUID,
};

struct CMapItemTest
{
	enum
	{
		CURRENT_VERSION = 1
	};

	int m_Version = 0;
	int m_aFields[2] = {0};
	int m_Field3 = 0;
	int m_Field4 = 0;
};

struct CMapItemAutoMapperConfig
{
	enum
	{
		CURRENT_VERSION = 1
	};
	enum
	{
		FLAG_AUTOMATIC = 1
	};

	int m_Version = 0;
	int m_GroupId = 0;
	int m_LayerId = 0;
	int m_AutomapperConfig = 0;
	int m_AutomapperSeed = 0;
	int m_Flags = 0;
};

void RegisterMapItemTypeUuids(class CUuidManager *pManager);
#endif // GAME_MAPITEMS_EX_H
