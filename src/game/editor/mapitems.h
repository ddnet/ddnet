#ifndef GAME_EDITOR_MAPITEMS_H
#define GAME_EDITOR_MAPITEMS_H

enum class EQuadProp
{
	NONE = -1,
	ORDER,
	POS_X,
	POS_Y,
	POS_ENV,
	POS_ENV_OFFSET,
	COLOR,
	COLOR_ENV,
	COLOR_ENV_OFFSET,
	NUM_PROPS,
};

enum class EQuadPointProp
{
	NONE = -1,
	POS_X,
	POS_Y,
	COLOR,
	TEX_U,
	TEX_V,
	NUM_PROPS,
};

enum class ESoundProp
{
	NONE = -1,
	POS_X,
	POS_Y,
	LOOP,
	PAN,
	TIME_DELAY,
	FALLOFF,
	POS_ENV,
	POS_ENV_OFFSET,
	SOUND_ENV,
	SOUND_ENV_OFFSET,
	NUM_PROPS,
};

enum class ERectangleShapeProp
{
	NONE = -1,
	RECTANGLE_WIDTH,
	RECTANGLE_HEIGHT,
	NUM_PROPS,
};

enum class ECircleShapeProp
{
	NONE = -1,
	CIRCLE_RADIUS,
	NUM_PROPS,
};

enum class ELayerProp
{
	NONE = -1,
	GROUP,
	ORDER,
	HQ,
	NUM_PROPS,
};

enum class ETilesProp
{
	NONE = -1,
	WIDTH,
	HEIGHT,
	SHIFT,
	SHIFT_BY,
	IMAGE,
	COLOR,
	COLOR_ENV,
	COLOR_ENV_OFFSET,
	AUTOMAPPER,
	AUTOMAPPER_REFERENCE,
	LIVE_GAMETILES,
	SEED,
	NUM_PROPS,
};

enum class ETilesCommonProp
{
	NONE = -1,
	WIDTH,
	HEIGHT,
	SHIFT,
	SHIFT_BY,
	COLOR,
	NUM_PROPS,
};

enum class EGroupProp
{
	NONE = -1,
	ORDER,
	POS_X,
	POS_Y,
	PARA_X,
	PARA_Y,
	USE_CLIPPING,
	CLIP_X,
	CLIP_Y,
	CLIP_W,
	CLIP_H,
	NUM_PROPS,
};

enum class ELayerQuadsProp
{
	NONE = -1,
	IMAGE,
	NUM_PROPS,
};

enum class ELayerSoundsProp
{
	NONE = -1,
	SOUND,
	NUM_PROPS,
};

#endif
