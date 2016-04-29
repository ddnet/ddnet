#include <game/mapitems.h>

bool IsValidGameTile(int Index)
{
	return (IsValidEntity(Index)
		||  Index == TILE_AIR
		|| (Index >= TILE_SOLID && Index <= TILE_NOLASER)
		||  Index == TILE_THROUGH
		||  Index == TILE_FREEZE
		|| (Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE)
		|| (Index >= TILE_WALLJUMP && Index <= TILE_SOLO_END)
		|| (Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA)
		|| (Index >= TILE_CP && Index <= TILE_THROUGH_DIR)
		|| (Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM)
		|| (Index >= TILE_NPC_END && Index <= TILE_NPH_END)
		|| (Index >= TILE_NPC_START && Index <= TILE_NPH_START)
		|| (Index >= TILE_ENTITIES_OFF_1 && Index <= TILE_ENTITIES_OFF_2)
	);
}

bool IsValidFrontTile(int Index)
{
	return (IsValidEntity(Index)
		||  Index == TILE_AIR
		||  Index == TILE_DEATH
		|| (Index >= TILE_NOLASER && Index <= TILE_THROUGH)
		||  Index == TILE_FREEZE
		|| (Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE)
		|| (Index >= TILE_WALLJUMP && Index <= TILE_SOLO_END)
		|| (Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA)
		|| (Index >= TILE_CP && Index <= TILE_THROUGH_DIR)
		|| (Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM)
		|| (Index >= TILE_NPC_END && Index <= TILE_NPH_END)
		|| (Index >= TILE_NPC_START && Index <= TILE_NPH_START)
	);
}

bool IsValidEntity(int Index)
{
	return Index > ENTITY_OFFSET;
}
