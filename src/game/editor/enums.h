#ifndef GAME_EDITOR_ENUMS_H
#define GAME_EDITOR_ENUMS_H

constexpr const char *g_apGametileOpNames[] = {
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

#endif
