/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VISUAL_PRIMITIVES_H
#define GAME_VISUAL_PRIMITIVES_H

// =============================================================================
// m_Flags bit layout (32 bits)
//
// DDNetVisualLine:
//   bits 0-1:   Line cap (LINECAP_*)
//   bits 2-3:   Line style (LINESTYLE_*)
//   bit  4:     VISUALFLAG_NO_INTERPOLATION
//   bit  5:     Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-12:  Spring omega (5b)
//   bits 13-31: Reserved (19)
//
// DDNetVisualCircle:
//   bit  0:     VISUALFLAG_FILLED
//   bits 1-3:   Circle style (CIRCLESTYLE_*, includes RADIAL_PROGRESS)
//   bit  4:     VISUALFLAG_NO_INTERPOLATION
//   bit  5:     Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-12:  Spring omega (5b)
//   bits 13-15: Reserved (3)
//   bits 16-23: Segments (style=SOLID..DASH_DOT) / Progress 0-255 (style=RADIAL_PROGRESS)
//   bits 24-31: Reserved (8)
//
// DDNetVisualQuad:
//   bit  0:     VISUALFLAG_FILLED
//   bits 1-2:   Line style (LINESTYLE_*)
//   bit  3:     Reserved
//   bit  4:     VISUALFLAG_NO_INTERPOLATION
//   bit  5:     Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-12:  Spring omega (5b)
//   bits 13-31: Reserved (19)
//
// DDNetVisualTile:
//   bit  0:     TILEFLAG_XFLIP
//   bit  1:     TILEFLAG_YFLIP
//   bit  2:     Reserved
//   bit  3:     TILEFLAG_ROTATE
//   bit  4:     VISUALFLAG_NO_INTERPOLATION
//   bit  5:     Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-12:  Spring omega (5b)
//   bits 13-31: Reserved (19)
// =============================================================================

// Line cap styles (bits 0-1 of m_Flags for DDNetVisualLine)
enum
{
	LINECAP_BUTT = 0,
	LINECAP_ROUND,
	LINECAP_SQUARE,
	NUM_LINECAPS,
};

// Line/shape drawing styles (bits 2-3 for Line, bits 1-2 for Quad)
enum
{
	LINESTYLE_SOLID = 0,
	LINESTYLE_DASHED,
	LINESTYLE_DOTTED,
	LINESTYLE_DASH_DOT,
	NUM_LINESTYLES,
};

// Circle styles (bits 1-3 for Circle, 3 bits = 8 values)
enum
{
	CIRCLESTYLE_SOLID = 0,
	CIRCLESTYLE_DASHED,
	CIRCLESTYLE_DOTTED,
	CIRCLESTYLE_DASH_DOT,
	CIRCLESTYLE_RADIAL_PROGRESS, // bits 16-23 = progress 0-255; FILLED=pie, !FILLED=arc
	NUM_CIRCLESTYLES,
};

// Visual flags (individual bits, shared across all types)
enum
{
	VISUALFLAG_FILLED = 1 << 0, // Circle/Quad: filled instead of outline
	VISUALFLAG_NO_INTERPOLATION = 1 << 4, // All types: skip snap interpolation (use current snap only)
	VISUALFLAG_CAMERA_RELATIVE = 1 << 6, // All types: coordinates in world units, relative to camera center
	VISUALFLAG_SCREEN_SPACE = 1 << 7, // All types: screen-space coords (0-1000 = top/left to bottom/right, width scaled by aspect)
};

// Spring omega (bits 8-12 of m_Flags, all types, 5 bits):
//   0 = default (20), 1-30 = value * 8 (range 8-240), 31 = no spring
enum
{
	SPRING_OMEGA_NO_SPRING = 31
};
inline int PackSpringOmega(int Omega) { return (Omega & 0x1F) << 8; }
inline int UnpackSpringOmega(int Flags) { return (Flags >> 8) & 0x1F; }
inline float GetSpringOmega(int Flags)
{
	int v = UnpackSpringOmega(Flags);
	return v == 0 ? 20.0f : (float)(v * 8);
}
// Spring is disabled for camera-relative items and when omega == NO_SPRING
inline bool SpringEnabled(int Flags) { return !(Flags & VISUALFLAG_CAMERA_RELATIVE) && UnpackSpringOmega(Flags) != SPRING_OMEGA_NO_SPRING; }

// Circle segments / radial progress (bits 16-23, DDNetVisualCircle only):
//   When style != RADIAL_PROGRESS: 0 = auto, 1-255 = explicit segment count
//   When style == RADIAL_PROGRESS: 0-255 = progress (0% to 100%)
inline int PackCircleSegments(int Value) { return (Value & 0xFF) << 16; }
inline int UnpackCircleSegments(int Flags) { return (Flags >> 16) & 0xFF; }
// When style == RADIAL_PROGRESS, same field holds progress 0-255
inline int PackCircleProgress(int Progress) { return PackCircleSegments(Progress); }
inline int UnpackCircleProgress(int Flags) { return UnpackCircleSegments(Flags); }

// Helpers to pack/unpack m_Flags for DDNetVisualLine (cap: 2 bits, style: 2 bits)
inline int PackLineFlags(int Cap, int Style) { return (Cap & 0x3) | ((Style & 0x3) << 2); }
inline int UnpackLineCap(int Flags) { return Flags & 0x3; }
inline int UnpackLineStyle(int Flags) { return (Flags >> 2) & 0x3; }

// Helpers to pack/unpack m_Flags for DDNetVisualCircle (filled: 1 bit, style: 3 bits at 1-3)
inline int PackCircleFlags(bool Filled, int Style) { return (Filled ? VISUALFLAG_FILLED : 0) | ((Style & 0x7) << 1); }
inline bool UnpackCircleFilled(int Flags) { return (Flags & VISUALFLAG_FILLED) != 0; }
inline int UnpackCircleStyle(int Flags) { return (Flags >> 1) & 0x7; }

// Helpers to pack/unpack m_Flags for DDNetVisualQuad (filled: 1 bit, style: 2 bits at 1-2)
inline int PackShapeFlags(bool Filled, int Style) { return (Filled ? VISUALFLAG_FILLED : 0) | ((Style & 0x3) << 1); }
inline bool UnpackShapeFilled(int Flags) { return (Flags & VISUALFLAG_FILLED) != 0; }
inline int UnpackShapeStyle(int Flags) { return (Flags >> 1) & 0x3; }

// m_RenderOrder construction and extraction
// bits 0-15:  group index (0xFFFF = game group)
// bits 16-30: layer offset (0x7FFF = after all layers)
// bit 31:     1 = render BEFORE entities, 0 = AFTER entities
inline int MakeRenderOrder(int Group, int LayerOffset, bool BehindEntities)
{
	return (int)(((unsigned)(BehindEntities ? 1u : 0u) << 31) | (((unsigned)LayerOffset & 0x7FFF) << 16) | ((unsigned)Group & 0xFFFF));
}
inline int RenderOrderGroup(int RenderOrder) { return RenderOrder & 0xFFFF; }
inline int RenderOrderLayerOffset(int RenderOrder) { return (RenderOrder >> 16) & 0x7FFF; }
inline bool RenderOrderBehindEntities(int RenderOrder) { return ((unsigned)RenderOrder >> 31) & 1u; }

enum
{
	RENDERORDER_DEFAULT = 0x7FFFFFFF, // game group, after all layers, after entities
	RENDERORDER_BEHIND_PLAYERS = (int)0xFFFFFFFF, // game group, after all layers, before entities
	RENDERORDER_GROUP_GAME = 0xFFFF,
	RENDERORDER_LAYER_ALL = 0x7FFF,
};

// Pack color from RGBA components (0-255 each)
inline int PackVisualColor(int R, int G, int B, int A) { return (int)(((unsigned)R << 24) | ((unsigned)G << 16) | ((unsigned)B << 8) | (unsigned)A); }
inline int UnpackVisualColorR(int Color) { return (Color >> 24) & 0xFF; }
inline int UnpackVisualColorG(int Color) { return (Color >> 16) & 0xFF; }
inline int UnpackVisualColorB(int Color) { return (Color >> 8) & 0xFF; }
inline int UnpackVisualColorA(int Color) { return Color & 0xFF; }

#endif
