/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VISUAL_PRIMITIVES_H
#define GAME_VISUAL_PRIMITIVES_H

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

// Visual flags
enum
{
	VISUALFLAG_FILLED = 1 << 0, // Circle/Quad: filled instead of outline
};

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
	return ((BehindEntities ? 1 : 0) << 31) | ((LayerOffset & 0x7FFF) << 16) | (Group & 0xFFFF);
}
inline int RenderOrderGroup(int RenderOrder) { return RenderOrder & 0xFFFF; }
inline int RenderOrderLayerOffset(int RenderOrder) { return (RenderOrder >> 16) & 0x7FFF; }
inline bool RenderOrderBehindEntities(int RenderOrder) { return (RenderOrder >> 31) & 1; }

enum
{
	RENDERORDER_DEFAULT = 0x7FFFFFFF, // game group, after all layers, after entities
	RENDERORDER_BEHIND_PLAYERS = (int)0xFFFFFFFF, // game group, after all layers, before entities
	RENDERORDER_GROUP_GAME = 0xFFFF,
	RENDERORDER_LAYER_ALL = 0x7FFF,
};

// Pack color from RGBA components (0-255 each)
inline int PackVisualColor(int R, int G, int B, int A) { return (R << 24) | (G << 16) | (B << 8) | A; }
inline int UnpackVisualColorR(int Color) { return (Color >> 24) & 0xFF; }
inline int UnpackVisualColorG(int Color) { return (Color >> 16) & 0xFF; }
inline int UnpackVisualColorB(int Color) { return (Color >> 8) & 0xFF; }
inline int UnpackVisualColorA(int Color) { return Color & 0xFF; }

#endif
