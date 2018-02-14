#include <game/mapitems.h>

bool IsValidGameTile(int Index)
{
	return (
		    Index == TILE_AIR
		|| (Index >= TILE_SOLID && Index <= TILE_NOLASER)
		||  Index == TILE_THROUGH
		||  Index == TILE_FREEZE
		|| (Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE)
		|| (Index >= TILE_WALLJUMP && Index <= TILE_SOLO_END)
		|| (Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA)
		|| (Index >= TILE_CP && Index <= TILE_THROUGH_DIR)
		|| (Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM)
		|| (Index >= TILE_NPC_END && Index <= TILE_NPH_END)
		|| (Index >= TILE_TELE_GUN_ENABLE && Index <= TILE_NO_TELE_GUN)
		|| (Index >= TILE_TELE_GRENADE_ENABLE && Index <= TILE_TELE_GRENADE_DISABLE)
		|| (Index >= TILE_TELE_LASER_ENABLE && Index <= TILE_TELE_LASER_DISABLE)
		|| (Index >= TILE_NPC_START && Index <= TILE_NPH_START)
		|| (Index >= TILE_ENTITIES_OFF_1 && Index <= TILE_ENTITIES_OFF_2)
		||  IsValidEntity(Index)
	);
}

bool IsValidFrontTile(int Index)
{
	return (
		    Index == TILE_AIR
		||  Index == TILE_DEATH
		|| (Index >= TILE_NOLASER && Index <= TILE_THROUGH)
		||  Index == TILE_FREEZE
		|| (Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE)
		|| (Index >= TILE_WALLJUMP && Index <= TILE_SOLO_END)
		|| (Index >= TILE_REFILL_JUMPS && Index <= TILE_STOPA)
		|| (Index >= TILE_CP && Index <= TILE_THROUGH_DIR)
		|| (Index >= TILE_OLDLASER && Index <= TILE_UNLOCK_TEAM)
		|| (Index >= TILE_NPC_END && Index <= TILE_NPH_END)
		|| (Index >= TILE_TELE_GUN_ENABLE && Index <= TILE_NO_TELE_GUN)
		|| (Index >= TILE_TELE_GRENADE_ENABLE && Index <= TILE_TELE_GRENADE_DISABLE)
		|| (Index >= TILE_TELE_LASER_ENABLE && Index <= TILE_TELE_LASER_DISABLE)
		|| (Index >= TILE_NPC_START && Index <= TILE_NPH_START)
		||  IsValidEntity(Index)
	);
}

bool IsValidTeleTile(int Index)
{
	return (
		    Index == TILE_TELEINEVIL
		||  Index == TILE_TELEINWEAPON
		||  Index == TILE_TELEINHOOK
		||  Index == TILE_TELEIN
		||  Index == TILE_TELEOUT
		||  Index == TILE_TELECHECK
		||  Index == TILE_TELECHECKOUT
		||  Index == TILE_TELECHECKIN
		||  Index == TILE_TELECHECKINEVIL
	);
}

bool IsValidSpeedupTile(int Index)
{
	return Index == TILE_BOOST;
}

bool IsValidSwitchTile(int Index)
{
	return (
		    Index == TILE_JUMP
		||  Index == TILE_FREEZE
		||  Index == TILE_DFREEZE
		||  Index == TILE_DUNFREEZE
		||  Index == TILE_HIT_START
		||  Index == TILE_HIT_END
		|| (Index >= TILE_SWITCHTIMEDOPEN && Index <= TILE_SWITCHCLOSE)
		||  Index == TILE_PENALTY
		||  Index == TILE_BONUS
		||  IsValidEntity(Index)
	);
}

bool IsValidEntity(int Index)
{
	Index -= ENTITY_OFFSET;
	return (
		   (Index >= ENTITY_SPAWN && Index <= ENTITY_LASER_O_FAST)
		|| (Index >= ENTITY_PLASMAE && Index <= ENTITY_CRAZY_SHOTGUN)
		|| (Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG_NW)
		||  Index == ENTITY_DOOR
	);
}
