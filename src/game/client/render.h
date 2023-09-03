/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_RENDER_H
#define GAME_CLIENT_RENDER_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/skin.h>
#include <game/client/ui_rect.h>

class CSpeedupTile;
class CSwitchTile;
class CTeleTile;
class CTile;
class CTuneTile;
namespace client_data7 {
struct CDataSprite;
}
struct CDataSprite;
struct CEnvPoint;
struct CMapItemGroup;
struct CMapItemGroupEx;
struct CQuad;

class CTeeRenderInfo
{
public:
	CTeeRenderInfo()
	{
		m_ColorBody = ColorRGBA(1, 1, 1);
		m_ColorFeet = ColorRGBA(1, 1, 1);
		m_Size = 1.0f;
		m_GotAirJump = 1;
		m_TeeRenderFlags = 0;
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
	int m_TeeRenderFlags;
	bool m_FeetFlipped;
};

// Tee Render Flags
enum
{
	TEE_EFFECT_FROZEN = 1,
	TEE_NO_WEAPON = 2,
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

typedef void (*ENVELOPE_EVAL)(int TimeOffsetMillis, int Env, ColorRGBA &Channels, void *pUser);

class CRenderTools
{
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;

	int m_TeeQuadContainerIndex;

	void GetRenderTeeBodyScale(float BaseSize, float &BodyScale);
	void GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight);

public:
	class IGraphics *Graphics() const { return m_pGraphics; }
	class ITextRender *TextRender() const { return m_pTextRender; }

	void Init(class IGraphics *pGraphics, class ITextRender *pTextRender);

	void SelectSprite(CDataSprite *pSprite, int Flags = 0, int sx = 0, int sy = 0);
	void SelectSprite(int Id, int Flags = 0, int sx = 0, int sy = 0);

	void GetSpriteScale(client_data7::CDataSprite *pSprite, float &ScaleX, float &ScaleY);
	void GetSpriteScale(CDataSprite *pSprite, float &ScaleX, float &ScaleY);
	void GetSpriteScale(int Id, float &ScaleX, float &ScaleY);
	void GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY);

	void DrawSprite(float x, float y, float size);
	void DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight);
	void RenderCursor(vec2 Center, float Size);
	void RenderIcon(int ImageId, int SpriteId, const CUIRect *pRect, const ColorRGBA *pColor = nullptr);
	int QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float size);
	int QuadContainerAddSprite(int QuadContainerIndex, float size);
	int QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height);
	int QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height);

	// larger rendering methods
	void GetRenderTeeBodySize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height);
	void GetRenderTeeFeetSize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height);
	void GetRenderTeeAnimScaleAndBaseSize(class CAnimState *pAnim, CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize);

	// returns the offset to use, to render the tee with @see RenderTee exactly in the mid
	void GetRenderTeeOffsetToRenderedTee(class CAnimState *pAnim, CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid);
	// object render methods
	void RenderTee(class CAnimState *pAnim, CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha = 1.0f);

	// map render methods (render_map.cpp)
	static void RenderEvalEnvelope(CEnvPoint *pPoints, int NumPoints, int Channels, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result);
	void RenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser);
	void ForceRenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha = 1.0f);
	void RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset);

	// render a rectangle made of IndexIn tiles, over a background made of IndexOut tiles
	// the rectangle include all tiles in [RectX, RectX+RectW-1] x [RectY, RectY+RectH-1]
	void RenderTileRectangle(int RectX, int RectY, int RectW, int RectH, unsigned char IndexIn, unsigned char IndexOut, float Scale, ColorRGBA Color, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset);

	// helpers
	void CalcScreenParams(float Aspect, float Zoom, float *pWidth, float *pHeight);
	void MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
		float ParallaxZoom, float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints);
	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, CMapItemGroupEx *pGroupEx, float Zoom);
	void MapScreenToInterface(float CenterX, float CenterY);

	// DDRace

	void RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, float Alpha = 1.0f);
	void RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderSpeedupmap(CSpeedupTile *pSpeedup, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderSwitchmap(CSwitchTile *pSwitch, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
};

#endif
