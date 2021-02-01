#include <game/mapitems.h>

bool IsValidGameTile(int Index)
{
	return (
		Index == TILE_AIR ||
		(Index >= TILE_SOLID && Index <= TILE_THROUGH) ||
		Index == TILE_FREEZE ||
		(Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE) ||
		(Index >= TILE_WALLJUMP && Index <= TILE_SOLO_DISABLE) ||
		(Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA) ||
		(Index >= TILE_CP && Index <= TILE_THROUGH_DIR) ||
		(Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM) ||
		(Index >= TILE_NPC_DISABLE && Index <= TILE_NPH_DISABLE) ||
		(Index >= TILE_TELE_GUN_ENABLE && Index <= TILE_TELE_GUN_DISABLE) ||
		(Index >= TILE_TELE_GRENADE_ENABLE && Index <= TILE_TELE_GRENADE_DISABLE) ||
		(Index >= TILE_TELE_LASER_ENABLE && Index <= TILE_TELE_LASER_DISABLE) ||
		(Index >= TILE_NPC_ENABLE && Index <= TILE_NPH_ENABLE) ||
		(Index >= TILE_ENTITIES_OFF_1 && Index <= TILE_ENTITIES_OFF_2) ||
		IsValidEntity(Index));
}

bool IsValidFrontTile(int Index)
{
	return (
		Index == TILE_AIR ||
		Index == TILE_DEATH ||
		(Index >= TILE_NOLASER && Index <= TILE_THROUGH) ||
		Index == TILE_FREEZE ||
		(Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE) ||
		(Index >= TILE_WALLJUMP && Index <= TILE_SOLO_DISABLE) ||
		(Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA) ||
		(Index >= TILE_CP && Index <= TILE_THROUGH_DIR) ||
		(Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM) ||
		(Index >= TILE_NPC_DISABLE && Index <= TILE_NPH_DISABLE) ||
		(Index >= TILE_TELE_GUN_ENABLE && Index <= TILE_ALLOW_BLUE_TELE_GUN) ||
		(Index >= TILE_TELE_GRENADE_ENABLE && Index <= TILE_TELE_GRENADE_DISABLE) ||
		(Index >= TILE_TELE_LASER_ENABLE && Index <= TILE_TELE_LASER_DISABLE) ||
		(Index >= TILE_NPC_ENABLE && Index <= TILE_NPH_ENABLE) ||
		(Index >= TILE_ENTITIES_OFF_1 && Index <= TILE_ENTITIES_OFF_2) ||
		IsValidEntity(Index));
}

bool IsValidTeleTile(int Index)
{
	return (
		Index == TILE_TELEINEVIL ||
		Index == TILE_TELEINWEAPON ||
		Index == TILE_TELEINHOOK ||
		Index == TILE_TELEIN ||
		Index == TILE_TELEOUT ||
		Index == TILE_TELECHECK ||
		Index == TILE_TELECHECKOUT ||
		Index == TILE_TELECHECKIN ||
		Index == TILE_TELECHECKINEVIL);
}

bool IsValidSpeedupTile(int Index)
{
	return Index == TILE_BOOST;
}

bool IsValidSwitchTile(int Index)
{
	return (
		Index == TILE_JUMP ||
		Index == TILE_FREEZE ||
		Index == TILE_DFREEZE ||
		Index == TILE_DUNFREEZE ||
		Index == TILE_HIT_ENABLE ||
		Index == TILE_HIT_DISABLE ||
		(Index >= TILE_SWITCHTIMEDOPEN && Index <= TILE_SWITCHCLOSE) ||
		Index == TILE_ADD_TIME ||
		Index == TILE_SUBTRACT_TIME ||
		Index == TILE_ALLOW_TELE_GUN ||
		Index == TILE_ALLOW_BLUE_TELE_GUN ||
		(IsValidEntity(Index) && Index >= ENTITY_OFFSET + ENTITY_ARMOR_1));
}

bool IsValidTuneTile(int Index)
{
	return Index == TILE_TUNE;
}

bool IsValidEntity(int Index)
{
	Index -= ENTITY_OFFSET;
	return (
		(Index >= ENTITY_SPAWN && Index <= ENTITY_LASER_O_FAST) ||
		(Index >= ENTITY_PLASMAE && Index <= ENTITY_CRAZY_SHOTGUN) ||
		(Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG_NW) ||
		Index == ENTITY_DOOR);
}

bool IsRotatableTile(int Index)
{
	return (
		Index == TILE_STOP ||
		Index == TILE_STOPS ||
		Index == TILE_CP ||
		Index == TILE_CP_F ||
		Index == TILE_THROUGH_DIR ||
		Index == TILE_ENTITIES_OFF_1 ||
		Index == TILE_ENTITIES_OFF_2 ||
		Index - ENTITY_OFFSET == ENTITY_CRAZY_SHOTGUN_EX ||
		Index - ENTITY_OFFSET == ENTITY_CRAZY_SHOTGUN);
}

bool IsCreditsTile(int TileIndex)
{
	return (
		(TILE_CREDITS_1 == TileIndex) ||
		(TILE_CREDITS_2 == TileIndex) ||
		(TILE_CREDITS_3 == TileIndex) ||
		(TILE_CREDITS_4 == TileIndex) ||
		(TILE_CREDITS_5 == TileIndex) ||
		(TILE_CREDITS_6 == TileIndex) ||
		(TILE_CREDITS_7 == TileIndex) ||
		(TILE_CREDITS_8 == TileIndex));
}
