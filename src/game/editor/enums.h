#ifndef GAME_EDITOR_ENUMS_H
#define GAME_EDITOR_ENUMS_H

constexpr const char *GAME_TILE_OP_NAMES[] = {
	"Air",
	"Hookable",
	"Death",
	"Unhookable",
	"Hookthrough",
	"Freeze",
	"Unfreeze",
	"Deep Freeze",
	"Deep Unfreeze",
	"Blue Check-Tele",
	"Red Check-Tele",
	"Live Freeze",
	"Live Unfreeze",
};
enum class EGameTileOp
{
	AIR,
	HOOKABLE,
	DEATH,
	UNHOOKABLE,
	HOOKTHROUGH,
	FREEZE,
	UNFREEZE,
	DEEP_FREEZE,
	DEEP_UNFREEZE,
	BLUE_CHECK_TELE,
	RED_CHECK_TELE,
	LIVE_FREEZE,
	LIVE_UNFREEZE,
};

constexpr const char *AUTOMAP_REFERENCE_NAMES[] = {
	"Game Layer",
	"Hookable",
	"Death",
	"Unhookable",
	"Freeze",
	"Unfreeze",
	"Deep Freeze",
	"Deep Unfreeze",
	"Live Freeze",
	"Live Unfreeze",
};

#endif
