#include <game/mapitems.h>

ColorRGBA CEnvPoint::ColorValue() const
{
	return ColorRGBA(fx2f(m_aValues[0]), fx2f(m_aValues[1]), fx2f(m_aValues[2]), fx2f(m_aValues[3]));
}

void CEnvPoint::SetColorValue(const ColorRGBA &Color)
{
	m_aValues[0] = f2fx(Color.r);
	m_aValues[1] = f2fx(Color.g);
	m_aValues[2] = f2fx(Color.b);
	m_aValues[3] = f2fx(Color.a);
}

bool IsValidGameTile(int Index)
{
	return (
		Index == TILE_AIR ||
		(Index >= TILE_SOLID && Index <= TILE_THROUGH) ||
		Index == TILE_FREEZE ||
		(Index >= TILE_UNFREEZE && Index <= TILE_DUNFREEZE) ||
		(Index >= TILE_LFREEZE && Index <= TILE_LUNFREEZE) ||
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
		(Index >= TILE_LFREEZE && Index <= TILE_LUNFREEZE) ||
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

bool IsTeleTileCheckpoint(int Index)
{
	return Index == TILE_TELECHECK || Index == TILE_TELECHECKOUT;
}

bool IsTeleTileNumberUsed(int Index, bool Checkpoint)
{
	if(Checkpoint)
		return IsTeleTileCheckpoint(Index);
	return !IsTeleTileCheckpoint(Index) && Index != TILE_TELECHECKIN &&
	       Index != TILE_TELECHECKINEVIL;
}

bool IsTeleTileNumberUsedAny(int Index)
{
	return Index != TILE_TELECHECKIN &&
	       Index != TILE_TELECHECKINEVIL;
}

bool IsValidSpeedupTile(int Index)
{
	return Index == TILE_SPEED_BOOST_OLD || Index == TILE_SPEED_BOOST;
}

bool IsValidSwitchTile(int Index)
{
	return (
		Index == TILE_JUMP ||
		Index == TILE_FREEZE ||
		Index == TILE_DFREEZE ||
		Index == TILE_DUNFREEZE ||
		Index == TILE_LFREEZE ||
		Index == TILE_LUNFREEZE ||
		Index == TILE_HIT_ENABLE ||
		Index == TILE_HIT_DISABLE ||
		(Index >= TILE_SWITCHTIMEDOPEN && Index <= TILE_SWITCHCLOSE) ||
		Index == TILE_ADD_TIME ||
		Index == TILE_SUBTRACT_TIME ||
		Index == TILE_ALLOW_TELE_GUN ||
		Index == TILE_ALLOW_BLUE_TELE_GUN ||
		(IsValidEntity(Index) && Index >= ENTITY_OFFSET + ENTITY_ARMOR_1));
}

bool IsSwitchTileFlagsUsed(int Index)
{
	return Index != TILE_FREEZE &&
	       Index != TILE_DFREEZE &&
	       Index != TILE_DUNFREEZE;
}

bool IsSwitchTileNumberUsed(int Index)
{
	return Index != TILE_JUMP &&
	       Index != TILE_HIT_ENABLE &&
	       Index != TILE_HIT_DISABLE &&
	       Index != TILE_ALLOW_TELE_GUN &&
	       Index != TILE_ALLOW_BLUE_TELE_GUN;
}

bool IsSwitchTileDelayUsed(int Index)
{
	return Index != TILE_DFREEZE &&
	       Index != TILE_DUNFREEZE;
}

bool IsValidTuneTile(int Index)
{
	return Index == TILE_TUNE;
}

bool IsValidEntity(int Index)
{
	Index -= ENTITY_OFFSET;
	return (
		(Index >= ENTITY_SPAWN && Index <= ENTITY_ARMOR_LASER) ||
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
		Index - ENTITY_OFFSET == ENTITY_CRAZY_SHOTGUN ||
		(Index - ENTITY_OFFSET >= ENTITY_ARMOR_1 && Index - ENTITY_OFFSET <= ENTITY_WEAPON_LASER) ||
		(Index - ENTITY_OFFSET >= ENTITY_ARMOR_SHOTGUN && Index - ENTITY_OFFSET <= ENTITY_ARMOR_LASER));
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

int PackColor(const CColor &Color)
{
	int PackedColor = 0;
	PackedColor |= (Color.r & 0xFF) << 24;
	PackedColor |= (Color.g & 0xFF) << 16;
	PackedColor |= (Color.b & 0xFF) << 8;
	PackedColor |= Color.a & 0xFF;
	return PackedColor;
}

CColor UnpackColor(int PackedColor)
{
	CColor Color;
	Color.r = (PackedColor >> 24) & 0xFF;
	Color.g = (PackedColor >> 16) & 0xFF;
	Color.b = (PackedColor >> 8) & 0xFF;
	Color.a = PackedColor & 0xFF;
	return Color;
}
