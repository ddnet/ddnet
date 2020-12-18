/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_RENDER_H
#define GAME_CLIENT_RENDER_H

#include "ui.h"
#include <base/color.h>
#include <base/vmath.h>
#include <engine/graphics.h>
#include <game/client/skin.h>
#include <game/mapitems.h>

class CTeeRenderInfo
{
public:
	CTeeRenderInfo()
	{
		m_ColorBody = ColorRGBA(1, 1, 1);
		m_ColorFeet = ColorRGBA(1, 1, 1);
		m_Size = 1.0f;
		m_GotAirJump = 1;
	};

	CSkin::SSkinTextures m_OriginalRenderSkin;
	CSkin::SSkinTextures m_ColorableRenderSkin;

	CSkin::SSkinMetrics m_SkinMetrics;

	bool m_CustomColoredSkin;
	ColorRGBA m_BloodColor;

	ColorRGBA m_ColorBody;
	ColorRGBA m_ColorFeet;
	float m_Size;
	int m_GotAirJump;
};

// sprite renderings
enum
{
	SPRITE_FLAG_FLIP_Y = 1,
	SPRITE_FLAG_FLIP_X = 2,

	LAYERRENDERFLAG_OPAQUE = 1,
	LAYERRENDERFLAG_TRANSPARENT = 2,

	TILERENDERFLAG_EXTEND = 4,
};

typedef void (*ENVELOPE_EVAL)(int TimeOffsetMillis, int Env, float *pChannels, void *pUser);

class CRenderTools
{
	int m_TeeQuadContainerIndex;

	void GetRenderTeeAnimScaleAndBaseSize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize);
	void GetRenderTeeBodyScale(float BaseSize, float &BodyScale);
	void GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight);

public:
	class IGraphics *m_pGraphics;
	class CUI *m_pUI;
	class CGameClient *m_pGameClient;

	class IGraphics *Graphics() const { return m_pGraphics; }
	class CUI *UI() const { return m_pUI; }
	class CGameClient *GameClient() const { return m_pGameClient; }

	void Init(class IGraphics *pGraphics, class CUI *pUI, class CGameClient *pGameClient);

	//typedef struct SPRITE;

	void SelectSprite(struct CDataSprite *pSprite, int Flags = 0, int sx = 0, int sy = 0);
	void SelectSprite(int id, int Flags = 0, int sx = 0, int sy = 0);

	void GetSpriteScale(client_data7::CDataSprite *pSprite, float &ScaleX, float &ScaleY);
	void GetSpriteScale(struct CDataSprite *pSprite, float &ScaleX, float &ScaleY);
	void GetSpriteScale(int id, float &ScaleX, float &ScaleY);
	void GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY);

	void DrawSprite(float x, float y, float size);
	void DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight);
	void QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float size);
	void QuadContainerAddSprite(int QuadContainerIndex, float size);
	void QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height);
	void QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height);

	// rects
	void DrawRoundRect(float x, float y, float w, float h, float r);
	void DrawRoundRectExt(float x, float y, float w, float h, float r, int Corners);
	void DrawRoundRectExt4(float x, float y, float w, float h, vec4 ColorTopLeft, vec4 ColorTopRight, vec4 ColorBottomLeft, vec4 ColorBottomRight, float r, int Corners);

	int CreateRoundRectQuadContainer(float x, float y, float w, float h, float r, int Corners);

	void DrawUIElRect(CUIElement::SUIElementRect &ElUIRect, const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding);

	void DrawUIRect(const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding);
	void DrawUIRect4(const CUIRect *pRect, vec4 ColorTopLeft, vec4 ColorTopRight, vec4 ColorBottomLeft, vec4 ColorBottomRight, int Corners, float Rounding);
	void DrawUIRect4NoRounding(const CUIRect *pRect, vec4 ColorTopLeft, vec4 ColorTopRight, vec4 ColorBottomLeft, vec4 ColorBottomRight);

	void DrawCircle(float x, float y, float r, int Segments);

	// larger rendering methods
	void RenderTilemapGenerateSkip(class CLayers *pLayers);

	void GetRenderTeeBodySize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height);
	void GetRenderTeeFeetSize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height);

	// returns the offset to use, to render the tee with @see RenderTee exactly in the mid
	void GetRenderTeeOffsetToRenderedTee(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid);
	// object render methods (gc_render_obj.cpp)
	void RenderTee(class CAnimState *pAnim, CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha = 1.0f);

	// map render methods (gc_render_map.cpp)
	static void RenderEvalEnvelope(CEnvPoint *pPoints, int NumPoints, int Channels, int64 TimeMicros, float *pResult);
	void RenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser);
	void ForceRenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha = 1.0f);
	void RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset);

	// render a rectangle made of IndexIn tiles, over a background made of IndexOut tiles
	// the rectangle include all tiles in [RectX, RectX+RectW-1] x [RectY, RectY+RectH-1]
	void RenderTileRectangle(int RectX, int RectY, int RectW, int RectH, unsigned char IndexIn, unsigned char IndexOut, float Scale, ColorRGBA Color, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset);

	// helpers
	void CalcScreenParams(float Aspect, float Zoom, float *w, float *h);
	void MapscreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
		float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints);

	// DDRace

	void RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderSpeedupOverlay(CSpeedupTile *pTele, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderSpeedupmap(CSpeedupTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderSwitchmap(CSwitchTile *pSwitch, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
};

#endif
