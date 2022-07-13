/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.				*/
#ifndef GAME_MAPITEMS_H
#define GAME_MAPITEMS_H

#include <base/vmath.h>

// layer types
enum
{
	// TODO(Shereef Marzouk): fix this for vanilla, make use of LAYERTYPE_GAME instead of using m_game variable in the editor.
	LAYERTYPE_INVALID = 0,
	LAYERTYPE_GAME,
	LAYERTYPE_TILES,
	LAYERTYPE_QUADS,
	LAYERTYPE_FRONT,
	LAYERTYPE_TELE,
	LAYERTYPE_SPEEDUP,
	LAYERTYPE_SWITCH,
	LAYERTYPE_TUNE,
	LAYERTYPE_SOUNDS_DEPRECATED, // deprecated! do not use this, this is just for compatibility reasons
	LAYERTYPE_SOUNDS,

	MAPITEMTYPE_VERSION = 0,
	MAPITEMTYPE_INFO,
	MAPITEMTYPE_IMAGE,
	MAPITEMTYPE_ENVELOPE,
	MAPITEMTYPE_GROUP,
	MAPITEMTYPE_LAYER,
	MAPITEMTYPE_ENVPOINTS,
	MAPITEMTYPE_SOUND,
	// High map item type numbers suggest that they use the alternate
	// format with UUIDs. See src/engine/shared/datafile.cpp for some of
	// the implementation.

	CURVETYPE_STEP = 0,
	CURVETYPE_LINEAR,
	CURVETYPE_SLOW,
	CURVETYPE_FAST,
	CURVETYPE_SMOOTH,
	NUM_CURVETYPES,

	// game layer tiles
	// TODO define which Layer uses which tiles (needed for mapeditor)
	ENTITY_NULL = 0,
	ENTITY_SPAWN,
	ENTITY_SPAWN_RED,
	ENTITY_SPAWN_BLUE,
	ENTITY_FLAGSTAND_RED,
	ENTITY_FLAGSTAND_BLUE,
	ENTITY_ARMOR_1,
	ENTITY_HEALTH_1,
	ENTITY_WEAPON_SHOTGUN,
	ENTITY_WEAPON_GRENADE,
	ENTITY_POWERUP_NINJA,
	ENTITY_WEAPON_LASER,
	//DDRace - Main Lasers
	ENTITY_LASER_FAST_CCW,
	ENTITY_LASER_NORMAL_CCW,
	ENTITY_LASER_SLOW_CCW,
	ENTITY_LASER_STOP,
	ENTITY_LASER_SLOW_CW,
	ENTITY_LASER_NORMAL_CW,
	ENTITY_LASER_FAST_CW,
	//DDRace - Laser Modifiers
	ENTITY_LASER_SHORT,
	ENTITY_LASER_MEDIUM,
	ENTITY_LASER_LONG,
	ENTITY_LASER_C_SLOW,
	ENTITY_LASER_C_NORMAL,
	ENTITY_LASER_C_FAST,
	ENTITY_LASER_O_SLOW,
	ENTITY_LASER_O_NORMAL,
	ENTITY_LASER_O_FAST,
	//DDRace - Plasma
	ENTITY_PLASMAE = 29,
	ENTITY_PLASMAF,
	ENTITY_PLASMA,
	ENTITY_PLASMAU,
	//DDRace - Shotgun
	ENTITY_CRAZY_SHOTGUN_EX,
	ENTITY_CRAZY_SHOTGUN,
	//DDNet - Removing specific weapon
	ENTITY_ARMOR_SHOTGUN,
	ENTITY_ARMOR_GRENADE,
	ENTITY_ARMOR_NINJA,
	ENTITY_ARMOR_LASER,
	//DDRace - Draggers
	ENTITY_DRAGGER_WEAK = 42,
	ENTITY_DRAGGER_NORMAL,
	ENTITY_DRAGGER_STRONG,
	//Draggers Behind Walls
	ENTITY_DRAGGER_WEAK_NW,
	ENTITY_DRAGGER_NORMAL_NW,
	ENTITY_DRAGGER_STRONG_NW,
	//Doors
	ENTITY_DOOR = 49,
	//End Of Lower Tiles
	NUM_ENTITIES,
	//Start From Top Left
	//Tile Controllers
	TILE_AIR = 0,
	TILE_SOLID,
	TILE_DEATH,
	TILE_NOHOOK,
	TILE_NOLASER,
	TILE_THROUGH_CUT,
	TILE_THROUGH,
	TILE_JUMP,
	TILE_FREEZE = 9,
	TILE_TELEINEVIL,
	TILE_UNFREEZE,
	TILE_DFREEZE,
	TILE_DUNFREEZE,
	TILE_TELEINWEAPON,
	TILE_TELEINHOOK,
	TILE_WALLJUMP = 16,
	TILE_EHOOK_ENABLE,
	TILE_EHOOK_DISABLE,
	TILE_HIT_ENABLE,
	TILE_HIT_DISABLE,
	TILE_SOLO_ENABLE,
	TILE_SOLO_DISABLE,
	//Switches
	TILE_SWITCHTIMEDOPEN = 22,
	TILE_SWITCHTIMEDCLOSE,
	TILE_SWITCHOPEN,
	TILE_SWITCHCLOSE,
	TILE_TELEIN,
	TILE_TELEOUT,
	TILE_BOOST,
	TILE_TELECHECK,
	TILE_TELECHECKOUT,
	TILE_TELECHECKIN,
	TILE_REFILL_JUMPS = 32,
	TILE_START,
	TILE_FINISH,
	TILE_TIME_CHECKPOINT_FIRST = 35,
	TILE_TIME_CHECKPOINT_LAST = 59,
	TILE_STOP = 60,
	TILE_STOPS,
	TILE_STOPA,
	TILE_TELECHECKINEVIL,
	TILE_CP,
	TILE_CP_F,
	TILE_THROUGH_ALL,
	TILE_THROUGH_DIR,
	TILE_TUNE,
	TILE_OLDLASER = 71,
	TILE_NPC,
	TILE_EHOOK,
	TILE_NOHIT,
	TILE_NPH,
	TILE_UNLOCK_TEAM,
	TILE_ADD_TIME = 79,
	TILE_NPC_DISABLE = 88,
	TILE_UNLIMITED_JUMPS_DISABLE,
	TILE_JETPACK_DISABLE,
	TILE_NPH_DISABLE,
	TILE_SUBTRACT_TIME = 95,
	TILE_TELE_GUN_ENABLE = 96,
	TILE_TELE_GUN_DISABLE = 97,
	TILE_ALLOW_TELE_GUN = 98,
	TILE_ALLOW_BLUE_TELE_GUN = 99,
	TILE_NPC_ENABLE = 104,
	TILE_UNLIMITED_JUMPS_ENABLE,
	TILE_JETPACK_ENABLE,
	TILE_NPH_ENABLE,
	TILE_TELE_GRENADE_ENABLE = 112,
	TILE_TELE_GRENADE_DISABLE = 113,
	TILE_TELE_LASER_ENABLE = 128,
	TILE_TELE_LASER_DISABLE = 129,
	TILE_CREDITS_1 = 140,
	TILE_CREDITS_2 = 141,
	TILE_CREDITS_3 = 142,
	TILE_CREDITS_4 = 143,
	TILE_LFREEZE = 144,
	TILE_LUNFREEZE = 145,
	TILE_CREDITS_5 = 156,
	TILE_CREDITS_6 = 157,
	TILE_CREDITS_7 = 158,
	TILE_CREDITS_8 = 159,
	TILE_ENTITIES_OFF_1 = 190,
	TILE_ENTITIES_OFF_2,
	//End of higher tiles
	//Layers
	LAYER_GAME = 0,
	LAYER_FRONT,
	LAYER_TELE,
	LAYER_SPEEDUP,
	LAYER_SWITCH,
	LAYER_TUNE,
	NUM_LAYERS,
	//Flags
	TILEFLAG_VFLIP = 1,
	TILEFLAG_HFLIP = 2,
	TILEFLAG_OPAQUE = 4,
	TILEFLAG_ROTATE = 8,
	//Rotation
	ROTATION_0 = 0,
	ROTATION_90 = TILEFLAG_ROTATE,
	ROTATION_180 = (TILEFLAG_VFLIP | TILEFLAG_HFLIP),
	ROTATION_270 = (TILEFLAG_VFLIP | TILEFLAG_HFLIP | TILEFLAG_ROTATE),

	LAYERFLAG_DETAIL = 1,
	TILESLAYERFLAG_GAME = 1,
	TILESLAYERFLAG_TELE = 2,
	TILESLAYERFLAG_SPEEDUP = 4,
	TILESLAYERFLAG_FRONT = 8,
	TILESLAYERFLAG_SWITCH = 16,
	TILESLAYERFLAG_TUNE = 32,

	ENTITY_OFFSET = 255 - 16 * 4,
};

typedef ivec2 CPoint; // 22.10 fixed point
typedef ivec4 CColor;

struct CQuad
{
	CPoint m_aPoints[5];
	CColor m_aColors[4];
	CPoint m_aTexcoords[4];

	int m_PosEnv = 0;
	int m_PosEnvOffset = 0;

	int m_ColorEnv = 0;
	int m_ColorEnvOffset = 0;
};

class CTile
{
public:
	unsigned char m_Index = 0;
	unsigned char m_Flags = 0;
	unsigned char m_Skip = 0;
	unsigned char m_Reserved = 0;
};

struct CMapItemInfo
{
	int m_Version = 0;
	int m_Author = 0;
	int m_MapVersion = 0;
	int m_Credits = 0;
	int m_License = 0;
};

struct CMapItemInfoSettings : CMapItemInfo
{
	int m_Settings = 0;
};

struct CMapItemImage
{
	int m_Version = 0;
	int m_Width = 0;
	int m_Height = 0;
	int m_External = 0;
	int m_ImageName = 0;
	int m_ImageData = 0;
};

struct CMapItemGroup_v1
{
	int m_Version = 0;
	int m_OffsetX = 0;
	int m_OffsetY = 0;
	int m_ParallaxX = 0;
	int m_ParallaxY = 0;

	int m_StartLayer = 0;
	int m_NumLayers = 0;
};

struct CMapItemGroup : public CMapItemGroup_v1
{
	enum
	{
		CURRENT_VERSION = 3
	};

	int m_UseClipping = 0;
	int m_ClipX = 0;
	int m_ClipY = 0;
	int m_ClipW = 0;
	int m_ClipH = 0;

	int m_aName[3] = {0};
};

struct CMapItemLayer
{
	int m_Version = 0;
	int m_Type = 0;
	int m_Flags = 0;
};

struct CMapItemLayerTilemap
{
	CMapItemLayer m_Layer;
	int m_Version = 0;

	int m_Width = 0;
	int m_Height = 0;
	int m_Flags = 0;

	CColor m_Color;
	int m_ColorEnv = 0;
	int m_ColorEnvOffset = 0;

	int m_Image = 0;
	int m_Data = 0;

	int m_aName[3] = {0};

	// DDRace

	int m_Tele = 0;
	int m_Speedup = 0;
	int m_Front = 0;
	int m_Switch = 0;
	int m_Tune = 0;
};

struct CMapItemLayerQuads
{
	CMapItemLayer m_Layer;
	int m_Version = 0;

	int m_NumQuads = 0;
	int m_Data = 0;
	int m_Image = 0;

	int m_aName[3] = {0};
};

struct CMapItemVersion
{
	int m_Version = 0;
};

struct CEnvPoint
{
	int m_Time = 0; // in ms
	int m_Curvetype = 0;
	int m_aValues[4] = {0}; // 1-4 depending on envelope (22.10 fixed point)

	bool operator<(const CEnvPoint &Other) const { return m_Time < Other.m_Time; }
};

struct CMapItemEnvelope_v1
{
	int m_Version = 0;
	int m_Channels = 0;
	int m_StartPoint = 0;
	int m_NumPoints = 0;
	int m_aName[8] = {0};
};

struct CMapItemEnvelope : public CMapItemEnvelope_v1
{
	enum
	{
		CURRENT_VERSION = 2
	};
	int m_Synchronized = 0;
};

struct CSoundShape
{
	enum
	{
		SHAPE_RECTANGLE = 0,
		SHAPE_CIRCLE,
		NUM_SHAPES,
	};

	struct CRectangle
	{
		int m_Width = 0, m_Height = 0; // fxp 22.10
	};

	struct CCircle
	{
		int m_Radius = 0;
	};

	int m_Type = 0;

	union
	{
		CRectangle m_Rectangle;
		CCircle m_Circle;
	};

	CSoundShape() {} // required due to union
};

struct CSoundSource
{
	CPoint m_Position = {0, 0};
	int m_Loop = 0;
	int m_Pan = 0; // 0 - no panning, 1 - panning
	int m_TimeDelay = 0; // in s
	int m_Falloff = 0; // [0,255] // 0 - No falloff, 255 - full

	int m_PosEnv = 0;
	int m_PosEnvOffset = 0;
	int m_SoundEnv = 0;
	int m_SoundEnvOffset = 0;

	CSoundShape m_Shape;
};

struct CMapItemLayerSounds
{
	enum
	{
		CURRENT_VERSION = 2
	};

	CMapItemLayer m_Layer;
	int m_Version = 0;

	int m_NumSources = 0;
	int m_Data = 0;
	int m_Sound = 0;

	int m_aName[3] = {0};
};

struct CMapItemSound
{
	int m_Version = 0;

	int m_External = 0;

	int m_SoundName = 0;
	int m_SoundData = 0;
	int m_SoundDataSize = 0;
};

// DDRace

class CTeleTile
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
};

class CSpeedupTile
{
public:
	unsigned char m_Force = 0;
	unsigned char m_MaxSpeed = 0;
	unsigned char m_Type = 0;
	short m_Angle = 0;
};

class CSwitchTile
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
	unsigned char m_Flags = 0;
	unsigned char m_Delay = 0;
};

class CDoorTile
{
public:
	unsigned char m_Index = 0;
	unsigned char m_Flags = 0;
	int m_Number = 0;
};

class CTuneTile
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
};

bool IsValidGameTile(int Index);
bool IsValidFrontTile(int Index);
bool IsValidTeleTile(int Index);
bool IsValidSpeedupTile(int Index);
bool IsValidSwitchTile(int Index);
bool IsValidTuneTile(int Index);
bool IsValidEntity(int Index);
bool IsRotatableTile(int Index);
bool IsCreditsTile(int TileIndex);

#endif
