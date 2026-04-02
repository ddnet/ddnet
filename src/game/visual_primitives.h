/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VISUAL_PRIMITIVES_H
#define GAME_VISUAL_PRIMITIVES_H

// =============================================================================
// m_Flags bit layout
//
// DDNetVisualLine:
//   bits 0-2:   Line cap (LINECAP_*)
//   bits 3-5:   Line style (LINESTYLE_*)
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-15:  Spring omega (0 = default 20, 1-255 = custom)
//   bits 16-31: Reserved
//
// DDNetVisualCircle:
//   bit  0:     VISUALFLAG_FILLED
//   bits 1-3:   Line style (LINESTYLE_*)
//   bits 4-5:   Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-15:  Spring omega
//   bits 16-23: Circle segments (0 = auto, 1-255 = explicit)
//   bits 24-31: Reserved
//
// DDNetVisualQuad:
//   bit  0:     VISUALFLAG_FILLED
//   bits 1-3:   Line style (LINESTYLE_*)
//   bits 4-5:   Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-15:  Spring omega
//   bits 16-31: Reserved
//
// DDNetVisualTile:
//   bit  0:     TILEFLAG_XFLIP
//   bit  1:     TILEFLAG_YFLIP
//   bit  2:     Reserved
//   bit  3:     TILEFLAG_ROTATE
//   bits 4-5:   Reserved
//   bit  6:     VISUALFLAG_CAMERA_RELATIVE
//   bit  7:     VISUALFLAG_SCREEN_SPACE
//   bits 8-15:  Spring omega
//   bits 16-31: Reserved
// =============================================================================

// Line cap styles (bits 0-2 of m_Flags for DDNetVisualLine)
enum
{
	LINECAP_BUTT = 0,
	LINECAP_ROUND,
	LINECAP_SQUARE,
	NUM_LINECAPS,
};

// Line drawing styles (bits 3-5 of m_Flags for DDNetVisualLine, bits 1-3 for Circle/Quad)
enum
{
	LINESTYLE_SOLID = 0,
	LINESTYLE_DASHED,
	LINESTYLE_DOTTED,
	LINESTYLE_DASH_DOT,
	NUM_LINESTYLES,
};

// Visual flags (individual bits)
enum
{
	VISUALFLAG_FILLED = 1 << 0, // Circle/Quad: filled instead of outline
	VISUALFLAG_CAMERA_RELATIVE = 1 << 6, // All types: coordinates in world units, relative to camera center
	VISUALFLAG_SCREEN_SPACE = 1 << 7, // All types: coordinates in HUD space (height=300)
};

// Spring omega (bits 8-15 of m_Flags, all types): 0 = default (20), 1-255 = custom
inline int PackSpringOmega(int Omega) { return (Omega & 0xFF) << 8; }
inline int UnpackSpringOmega(int Flags) { return (Flags >> 8) & 0xFF; }
inline float GetSpringOmega(int Flags) { int v = UnpackSpringOmega(Flags); return v == 0 ? 20.0f : (float)v; }

// Circle segments (bits 16-23 of m_Flags, DDNetVisualCircle only): 0 = auto, 1-255 = explicit
inline int PackCircleSegments(int Segments) { return (Segments & 0xFF) << 16; }
inline int UnpackCircleSegments(int Flags) { return (Flags >> 16) & 0xFF; }

// Helpers to pack/unpack m_Flags for DDNetVisualLine
inline int PackLineFlags(int Cap, int Style) { return (Cap & 0x7) | ((Style & 0x7) << 3); }
inline int UnpackLineCap(int Flags) { return Flags & 0x7; }
inline int UnpackLineStyle(int Flags) { return (Flags >> 3) & 0x7; }

// Helpers to pack/unpack m_Flags for DDNetVisualCircle/Quad
inline int PackShapeFlags(bool Filled, int Style) { return (Filled ? VISUALFLAG_FILLED : 0) | ((Style & 0x7) << 1); }
inline bool UnpackShapeFilled(int Flags) { return (Flags & VISUALFLAG_FILLED) != 0; }
inline int UnpackShapeStyle(int Flags) { return (Flags >> 1) & 0x7; }

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
