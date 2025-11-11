/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_MAPITEMS_H
#define GAME_MAPITEMS_H

#include <base/color.h>
#include <base/vmath.h>

// layer types
enum
{
	LAYERTYPE_INVALID = 0,
	LAYERTYPE_GAME, // unused
	LAYERTYPE_TILES,
	LAYERTYPE_QUADS,
	LAYERTYPE_FRONT,
	LAYERTYPE_TELE,
	LAYERTYPE_SPEEDUP,
	LAYERTYPE_SWITCH,
	LAYERTYPE_TUNE,
	LAYERTYPE_SOUNDS_DEPRECATED, // deprecated! do not use this, this is just for compatibility reasons
	LAYERTYPE_SOUNDS,
};

enum
{
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
};

enum
{
	CURVETYPE_STEP = 0,
	CURVETYPE_LINEAR,
	CURVETYPE_SLOW,
	CURVETYPE_FAST,
	CURVETYPE_SMOOTH,
	CURVETYPE_BEZIER,
	NUM_CURVETYPES,
};

enum
{
	// game layer tiles
	// TODO define which Layer uses which tiles (needed for mapeditor)
	ENTITY_NULL = 0,
	ENTITY_SPAWN = 1,
	ENTITY_SPAWN_RED = 2,
	ENTITY_SPAWN_BLUE = 3,
	ENTITY_FLAGSTAND_RED = 4,
	ENTITY_FLAGSTAND_BLUE = 5,
	ENTITY_ARMOR_1 = 6,
	ENTITY_HEALTH_1 = 7,
	ENTITY_WEAPON_SHOTGUN = 8,
	ENTITY_WEAPON_GRENADE = 9,
	ENTITY_POWERUP_NINJA = 10,
	ENTITY_WEAPON_LASER = 11,

	// DDRace - Main Lasers
	ENTITY_LASER_FAST_CCW = 12,
	ENTITY_LASER_NORMAL_CCW = 13,
	ENTITY_LASER_SLOW_CCW = 14,
	ENTITY_LASER_STOP = 15,
	ENTITY_LASER_SLOW_CW = 16,
	ENTITY_LASER_NORMAL_CW = 17,
	ENTITY_LASER_FAST_CW = 18,

	// DDRace - Laser Modifiers
	ENTITY_LASER_SHORT = 19,
	ENTITY_LASER_MEDIUM = 20,
	ENTITY_LASER_LONG = 21,
	ENTITY_LASER_C_SLOW = 22,
	ENTITY_LASER_C_NORMAL = 23,
	ENTITY_LASER_C_FAST = 24,
	ENTITY_LASER_O_SLOW = 25,
	ENTITY_LASER_O_NORMAL = 26,
	ENTITY_LASER_O_FAST = 27,

	// DDRace - Plasma
	ENTITY_PLASMAE = 29,
	ENTITY_PLASMAF = 30,
	ENTITY_PLASMA = 31,
	ENTITY_PLASMAU = 32,

	// DDRace - Shotgun
	ENTITY_CRAZY_SHOTGUN_EX = 33,
	ENTITY_CRAZY_SHOTGUN = 34,

	// DDNet - Removing specific weapon
	ENTITY_ARMOR_SHOTGUN = 35,
	ENTITY_ARMOR_GRENADE = 36,
	ENTITY_ARMOR_NINJA = 37,
	ENTITY_ARMOR_LASER = 38,

	// DDRace - Draggers
	ENTITY_DRAGGER_WEAK = 42,
	ENTITY_DRAGGER_NORMAL = 43,
	ENTITY_DRAGGER_STRONG = 44,
	// Draggers Behind Walls
	ENTITY_DRAGGER_WEAK_NW = 45,
	ENTITY_DRAGGER_NORMAL_NW = 46,
	ENTITY_DRAGGER_STRONG_NW = 47,

	// Doors
	ENTITY_DOOR = 49,
	// End Of Lower Tiles

	ENTITY_OFFSET = 255 - 16 * 4,
};

enum
{
	// Start From Top Left
	// Tile Controllers
	TILE_AIR = 0,
	TILE_SOLID = 1,
	TILE_DEATH = 2,
	TILE_NOHOOK = 3,
	TILE_NOLASER = 4,
	TILE_THROUGH_CUT = 5,
	TILE_THROUGH = 6,
	TILE_JUMP = 7,
	TILE_FREEZE = 9,
	TILE_TELEINEVIL = 10,
	TILE_UNFREEZE = 11,
	TILE_DFREEZE = 12,
	TILE_DUNFREEZE = 13,
	TILE_TELEINWEAPON = 14,
	TILE_TELEINHOOK = 15,
	TILE_WALLJUMP = 16,
	TILE_EHOOK_ENABLE = 17,
	TILE_EHOOK_DISABLE = 18,
	TILE_HIT_ENABLE = 19,
	TILE_HIT_DISABLE = 20,
	TILE_SOLO_ENABLE = 21,
	TILE_SOLO_DISABLE = 22,
	// Switches
	TILE_SWITCHTIMEDOPEN = 22,
	TILE_SWITCHTIMEDCLOSE = 23,
	TILE_SWITCHOPEN = 24,
	TILE_SWITCHCLOSE = 25,
	TILE_TELEIN = 26,
	TILE_TELEOUT = 27,
	TILE_SPEED_BOOST_OLD = 28,
	TILE_SPEED_BOOST = 29,
	TILE_TELECHECK = 29,
	TILE_TELECHECKOUT = 30,
	TILE_TELECHECKIN = 31,
	TILE_REFILL_JUMPS = 32,
	TILE_START = 33,
	TILE_FINISH = 34,
	TILE_TIME_CHECKPOINT_FIRST = 35,
	TILE_TIME_CHECKPOINT_LAST = 59,
	TILE_STOP = 60,
	TILE_STOPS = 61,
	TILE_STOPA = 62,
	TILE_TELECHECKINEVIL = 63,
	TILE_CP = 64,
	TILE_CP_F = 65,
	TILE_THROUGH_ALL = 66,
	TILE_THROUGH_DIR = 67,
	TILE_TUNE = 68,
	TILE_OLDLASER = 71,
	TILE_NPC = 72,
	TILE_EHOOK = 73,
	TILE_NOHIT = 74,
	TILE_NPH = 75,
	TILE_UNLOCK_TEAM = 76,
	TILE_ADD_TIME = 79,
	TILE_NPC_DISABLE = 88,
	TILE_UNLIMITED_JUMPS_DISABLE = 89,
	TILE_JETPACK_DISABLE = 90,
	TILE_NPH_DISABLE = 91,
	TILE_SUBTRACT_TIME = 95,
	TILE_TELE_GUN_ENABLE = 96,
	TILE_TELE_GUN_DISABLE = 97,
	TILE_ALLOW_TELE_GUN = 98,
	TILE_ALLOW_BLUE_TELE_GUN = 99,
	TILE_NPC_ENABLE = 104,
	TILE_UNLIMITED_JUMPS_ENABLE = 105,
	TILE_JETPACK_ENABLE = 106,
	TILE_NPH_ENABLE = 107,
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
	TILE_ENTITIES_OFF_2 = 191,
	// End of higher tiles
};

enum
{
	// Layers
	LAYER_GAME = 0,
	LAYER_FRONT,
	LAYER_TELE,
	LAYER_SPEEDUP,
	LAYER_SWITCH,
	LAYER_TUNE,
	NUM_LAYERS,
};

enum
{
	// Flags
	TILEFLAG_XFLIP = 1 << 0,
	TILEFLAG_YFLIP = 1 << 1,
	TILEFLAG_OPAQUE = 1 << 2,
	TILEFLAG_ROTATE = 1 << 3,
};

enum
{
	// Rotation
	ROTATION_0 = 0,
	ROTATION_90 = TILEFLAG_ROTATE,
	ROTATION_180 = (TILEFLAG_XFLIP | TILEFLAG_YFLIP),
	ROTATION_270 = (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE),
};

enum
{
	LAYERFLAG_DETAIL = 1,
	TILESLAYERFLAG_GAME = 1 << 0,
	TILESLAYERFLAG_TELE = 1 << 1,
	TILESLAYERFLAG_SPEEDUP = 1 << 2,
	TILESLAYERFLAG_FRONT = 1 << 3,
	TILESLAYERFLAG_SWITCH = 1 << 4,
	TILESLAYERFLAG_TUNE = 1 << 5,
};

static constexpr size_t MAX_MAPIMAGES = 64;
static constexpr size_t MAX_MAPSOUNDS = 64;

typedef ivec2 CPoint; // 22.10 fixed point
typedef ivec4 CColor;

class CFixedTime
{
	int m_FixedPoint;

public:
	constexpr CFixedTime() :
		m_FixedPoint(0) {}
	constexpr explicit CFixedTime(int FixedPoint) :
		m_FixedPoint(FixedPoint) {}

	constexpr int GetInternal() const
	{
		return m_FixedPoint;
	}

	constexpr float AsSeconds() const
	{
		return m_FixedPoint / 1000.0f;
	}

	constexpr static CFixedTime FromSeconds(float Seconds)
	{
		return CFixedTime((int)std::round(Seconds * 1000.0f));
	}

	constexpr bool operator<(const CFixedTime &Other) const
	{
		return m_FixedPoint < Other.m_FixedPoint;
	}

	constexpr bool operator<=(const CFixedTime &Other) const
	{
		return m_FixedPoint <= Other.m_FixedPoint;
	}

	constexpr bool operator>(const CFixedTime &Other) const
	{
		return m_FixedPoint > Other.m_FixedPoint;
	}

	constexpr bool operator>=(const CFixedTime &Other) const
	{
		return m_FixedPoint >= Other.m_FixedPoint;
	}

	constexpr bool operator==(const CFixedTime &Other) const
	{
		return m_FixedPoint == Other.m_FixedPoint;
	}

	constexpr bool operator!=(const CFixedTime &Other) const
	{
		return m_FixedPoint != Other.m_FixedPoint;
	}

	constexpr CFixedTime operator+(const CFixedTime &Other) const
	{
		return CFixedTime(m_FixedPoint + Other.m_FixedPoint);
	}

	constexpr CFixedTime operator-(const CFixedTime &Other) const
	{
		return CFixedTime(m_FixedPoint - Other.m_FixedPoint);
	}

	constexpr CFixedTime &operator+=(const CFixedTime &Other)
	{
		m_FixedPoint += Other.m_FixedPoint;
		return *this;
	}

	constexpr CFixedTime &operator-=(const CFixedTime &Other)
	{
		m_FixedPoint -= Other.m_FixedPoint;
		return *this;
	}
};

class CQuad
{
public:
	CPoint m_aPoints[5];
	CColor m_aColors[4];
	CPoint m_aTexcoords[4];

	int m_PosEnv;
	int m_PosEnvOffset;

	int m_ColorEnv;
	int m_ColorEnvOffset;
};

class CTile
{
public:
	unsigned char m_Index;
	unsigned char m_Flags;
	unsigned char m_Skip;
	unsigned char m_Reserved;
};

class CMapItemInfo
{
public:
	int m_Version;
	int m_Author;
	int m_MapVersion;
	int m_Credits;
	int m_License;
};

class CMapItemInfoSettings : public CMapItemInfo
{
public:
	int m_Settings;
};

class CMapItemImage_v1
{
public:
	int m_Version;
	int m_Width;
	int m_Height;
	int m_External;
	int m_ImageName;
	int m_ImageData;
};

class CMapItemImage_v2 : public CMapItemImage_v1
{
public:
	int m_MustBe1;
};

typedef CMapItemImage_v1 CMapItemImage;

class CMapItemGroup_v1
{
public:
	int m_Version;
	int m_OffsetX;
	int m_OffsetY;
	int m_ParallaxX;
	int m_ParallaxY;

	int m_StartLayer;
	int m_NumLayers;
};

class CMapItemGroup : public CMapItemGroup_v1
{
public:
	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	int m_aName[3];
};

class CMapItemLayer
{
public:
	int m_Version;
	int m_Type;
	int m_Flags;
};

class CMapItemLayerTilemap
{
public:
	/**
	 * @link CMapItemLayerTilemap @endlink with this version are only written to maps in upstream Teeworlds.
	 * The tile data of tilemaps using this version must be unpacked by repeating tiles according to the
	 * @link CTile::m_Skip @endlink values of the packed tile data.
	 *
	 * @see CMap::ExtractTiles
	 */
	static constexpr int VERSION_TEEWORLDS_TILESKIP = 4;

	CMapItemLayer m_Layer;
	int m_Version;

	int m_Width;
	int m_Height;
	int m_Flags;

	CColor m_Color;
	int m_ColorEnv;
	int m_ColorEnvOffset;

	int m_Image;
	int m_Data;

	int m_aName[3];

	// DDRace

	int m_Tele;
	int m_Speedup;
	int m_Front;
	int m_Switch;
	int m_Tune;
};

class CMapItemLayerQuads
{
public:
	CMapItemLayer m_Layer;
	int m_Version;

	int m_NumQuads;
	int m_Data;
	int m_Image;

	int m_aName[3];
};

class CMapItemVersion
{
public:
	int m_Version;
};

// Represents basic information about envelope points.
// In upstream Teeworlds, this is only used if all CMapItemEnvelope are version 1 or 2.
class CEnvPoint
{
public:
	enum
	{
		MAX_CHANNELS = 4,
	};

	CFixedTime m_Time;
	int m_Curvetype; // CURVETYPE_* constants, any unknown value behaves like CURVETYPE_LINEAR
	int m_aValues[MAX_CHANNELS]; // 1-4 depending on envelope (22.10 fixed point)

	bool operator<(const CEnvPoint &Other) const { return m_Time < Other.m_Time; }

	ColorRGBA ColorValue() const;
	void SetColorValue(const ColorRGBA &Color);
};

// Represents additional envelope point information for CURVETYPE_BEZIER.
// In DDNet, these are stored separately in an UUID-based map item.
// In upstream Teeworlds, CEnvPointBezier_upstream is used instead.
class CEnvPointBezier
{
public:
	CFixedTime m_aInTangentDeltaX[CEnvPoint::MAX_CHANNELS];
	int m_aInTangentDeltaY[CEnvPoint::MAX_CHANNELS]; // 22.10 fxp
	CFixedTime m_aOutTangentDeltaX[CEnvPoint::MAX_CHANNELS];
	int m_aOutTangentDeltaY[CEnvPoint::MAX_CHANNELS]; // 22.10 fxp
};

// Written to maps on upstream Teeworlds for envelope points including bezier information instead of the basic
// CEnvPoint items, if at least one CMapItemEnvelope with version 3 or higher exists in the map.
class CEnvPointBezier_upstream : public CEnvPoint
{
public:
	CEnvPointBezier m_Bezier;
};

// Used to represent all envelope point information at runtime in editor.
// (Can eventually be different than CEnvPointBezier_upstream)
class CEnvPoint_runtime : public CEnvPoint
{
public:
	CEnvPointBezier m_Bezier;
};

class CMapItemEnvelope_v1
{
public:
	/**
	 * @link CMapItemEnvelope @endlink with this version are only written to maps in upstream Teeworlds.
	 * If at least one of these exists in a map, then the envelope points are represented
	 * by @link CEnvPointBezier_upstream @endlink instead of @link CEnvPoint @endlink.
	 */
	static constexpr int VERSION_TEEWORLDS_BEZIER = 3;

	int m_Version;
	int m_Channels;
	int m_StartPoint;
	int m_NumPoints;
	int m_aName[8];
};

class CMapItemEnvelope_v2 : public CMapItemEnvelope_v1
{
public:
	int m_Synchronized;
};

typedef CMapItemEnvelope_v2 CMapItemEnvelope;

class CSoundShape
{
public:
	enum
	{
		SHAPE_RECTANGLE = 0,
		SHAPE_CIRCLE,
		NUM_SHAPES,
	};

	class CRectangle
	{
	public:
		int m_Width, m_Height; // fxp 22.10
	};

	class CCircle
	{
	public:
		int m_Radius;
	};

	int m_Type;

	union
	{
		CRectangle m_Rectangle;
		CCircle m_Circle;
	};
};

class CSoundSource
{
public:
	CPoint m_Position;
	int m_Loop;
	int m_Pan; // 0 - no panning, 1 - panning
	int m_TimeDelay; // in s
	int m_Falloff; // [0,255] // 0 - No falloff, 255 - full

	int m_PosEnv;
	int m_PosEnvOffset;
	int m_SoundEnv;
	int m_SoundEnvOffset;

	CSoundShape m_Shape;
};

class CMapItemLayerSounds
{
public:
	CMapItemLayer m_Layer;
	int m_Version;

	int m_NumSources;
	int m_Data;
	int m_Sound;

	int m_aName[3];
};

class CMapItemSound
{
public:
	int m_Version;

	int m_External;

	int m_SoundName;
	int m_SoundData;
	// Deprecated. Do not read this value, it could be wrong.
	// Use GetDataSize instead, which returns the de facto size.
	// Value must still be written for compatibility.
	int m_SoundDataSize;
};

// DDRace

class CTeleTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
};

class CSpeedupTile
{
public:
	unsigned char m_Force;
	unsigned char m_MaxSpeed;
	unsigned char m_Type;
	short m_Angle;
};

class CSwitchTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
	unsigned char m_Flags;
	unsigned char m_Delay;
};

class CDoorTile
{
public:
	unsigned char m_Index;
	unsigned char m_Flags;
	int m_Number;
};

class CTuneTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
};

bool IsValidGameTile(int Index);
bool IsValidFrontTile(int Index);
bool IsValidTeleTile(int Index);
bool IsTeleTileCheckpoint(int Index); // Assumes that Index is a valid tele tile index
bool IsTeleTileNumberUsed(int Index, bool Checkpoint); // Assumes that Index is a valid tele tile index
bool IsTeleTileNumberUsedAny(int Index); // Does not check for checkpoint only
bool IsValidSpeedupTile(int Index);
bool IsValidSwitchTile(int Index);
bool IsSwitchTileFlagsUsed(int Index); // Assumes that Index is a valid switch tile index
bool IsSwitchTileNumberUsed(int Index); // Assumes that Index is a valid switch tile index
bool IsSwitchTileDelayUsed(int Index); // Assumes that Index is a valid switch tile index
bool IsValidTuneTile(int Index);
bool IsValidEntity(int Index);
bool IsRotatableTile(int Index);
bool IsCreditsTile(int TileIndex);

int PackColor(const CColor &Color);
CColor UnpackColor(int PackedColor);

#endif
