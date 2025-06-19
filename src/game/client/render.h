/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_RENDER_H
#define GAME_CLIENT_RENDER_H

#include <engine/client/enums.h>

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/skin.h>
#include <game/client/ui_rect.h>
#include <game/generated/protocol7.h>

#include <functional>
#include <memory>

class CAnimState;
class CSpeedupTile;
class CSwitchTile;
class CTeleTile;
class CTile;
class CTuneTile;
namespace client_data7 {
struct CDataSprite;
}
struct CDataSprite;
class CEnvPoint;
class CEnvPointBezier;
class CEnvPointBezier_upstream;
class CMapItemGroup;
class CQuad;

#include <game/generated/protocol.h>

class CSkinDescriptor
{
public:
	enum
	{
		FLAG_SIX = 1,
		FLAG_SEVEN = 2,
	};
	unsigned m_Flags;

	char m_aSkinName[MAX_SKIN_LENGTH];

	class CSixup
	{
	public:
		char m_aaSkinPartNames[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_LENGTH];
		bool m_BotDecoration;
		bool m_XmasHat;

		void Reset();
		bool operator==(const CSixup &Other) const;
		bool operator!=(const CSixup &Other) const { return !(*this == Other); }
	};
	CSixup m_aSixup[NUM_DUMMIES];

	CSkinDescriptor();
	void Reset();
	bool IsValid() const;
	bool operator==(const CSkinDescriptor &Other) const;
	bool operator!=(const CSkinDescriptor &Other) const { return !(*this == Other); }
};

class CTeeRenderInfo
{
public:
	CTeeRenderInfo()
	{
		Reset();
	}

	void Reset()
	{
		m_OriginalRenderSkin.Reset();
		m_ColorableRenderSkin.Reset();
		m_SkinMetrics.Reset();
		m_CustomColoredSkin = false;
		m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_Size = 1.0f;
		m_GotAirJump = true;
		m_TeeRenderFlags = 0;
		m_FeetFlipped = false;

		for(auto &Sixup : m_aSixup)
			Sixup.Reset();
	}

	void Apply(const CSkin *pSkin)
	{
		m_OriginalRenderSkin = pSkin->m_OriginalSkin;
		m_ColorableRenderSkin = pSkin->m_ColorableSkin;
		m_BloodColor = pSkin->m_BloodColor;
		m_SkinMetrics = pSkin->m_Metrics;
	}

	void ApplySkin(const CTeeRenderInfo &TeeRenderInfo)
	{
		m_OriginalRenderSkin = TeeRenderInfo.m_OriginalRenderSkin;
		m_ColorableRenderSkin = TeeRenderInfo.m_ColorableRenderSkin;
		m_BloodColor = TeeRenderInfo.m_BloodColor;
		m_SkinMetrics = TeeRenderInfo.m_SkinMetrics;
	}

	void ApplyColors(bool CustomColoredSkin, int ColorBody, int ColorFeet)
	{
		m_CustomColoredSkin = CustomColoredSkin;
		if(CustomColoredSkin)
		{
			m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(ColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT));
			m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(ColorFeet).UnclampLighting(ColorHSLA::DARKEST_LGT));
		}
		else
		{
			m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	CSkin::CSkinTextures m_OriginalRenderSkin;
	CSkin::CSkinTextures m_ColorableRenderSkin;

	CSkin::CSkinMetrics m_SkinMetrics;

	bool m_CustomColoredSkin;
	ColorRGBA m_BloodColor;

	ColorRGBA m_ColorBody;
	ColorRGBA m_ColorFeet;
	float m_Size;
	bool m_GotAirJump;
	int m_TeeRenderFlags;
	bool m_FeetFlipped;

	bool Valid() const
	{
		return m_CustomColoredSkin ? m_ColorableRenderSkin.m_Body.IsValid() : m_OriginalRenderSkin.m_Body.IsValid();
	}

	class CSixup
	{
	public:
		void Reset()
		{
			for(auto &Texture : m_aOriginalTextures)
			{
				Texture.Invalidate();
			}
			for(auto &Texture : m_aColorableTextures)
			{
				Texture.Invalidate();
			}
			std::fill(std::begin(m_aUseCustomColors), std::end(m_aUseCustomColors), false);
			std::fill(std::begin(m_aColors), std::end(m_aColors), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			m_HatTexture.Invalidate();
			m_BotTexture.Invalidate();
			m_HatSpriteIndex = 0;
			m_BotColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		}

		IGraphics::CTextureHandle m_aOriginalTextures[protocol7::NUM_SKINPARTS];
		IGraphics::CTextureHandle m_aColorableTextures[protocol7::NUM_SKINPARTS];
		bool m_aUseCustomColors[protocol7::NUM_SKINPARTS];
		ColorRGBA m_aColors[protocol7::NUM_SKINPARTS];
		ColorRGBA m_BloodColor;
		IGraphics::CTextureHandle m_HatTexture;
		IGraphics::CTextureHandle m_BotTexture;
		int m_HatSpriteIndex;
		ColorRGBA m_BotColor;

		const IGraphics::CTextureHandle &PartTexture(int Part) const
		{
			return (m_aUseCustomColors[Part] ? m_aColorableTextures : m_aOriginalTextures)[Part];
		}
	};

	CSixup m_aSixup[NUM_DUMMIES];
};

class CManagedTeeRenderInfo
{
	friend class CGameClient;
	CTeeRenderInfo m_TeeRenderInfo;
	CSkinDescriptor m_SkinDescriptor;
	std::function<void()> m_RefreshCallback = nullptr;

public:
	CManagedTeeRenderInfo(const CTeeRenderInfo &TeeRenderInfo, const CSkinDescriptor &SkinDescriptor) :
		m_TeeRenderInfo(TeeRenderInfo),
		m_SkinDescriptor(SkinDescriptor)
	{
	}

	CTeeRenderInfo &TeeRenderInfo() { return m_TeeRenderInfo; }
	const CTeeRenderInfo &TeeRenderInfo() const { return m_TeeRenderInfo; }
	const CSkinDescriptor &SkinDescriptor() const { return m_SkinDescriptor; }
	void SetRefreshCallback(const std::function<void()> &RefreshCallback) { m_RefreshCallback = RefreshCallback; }
};

// Tee Render Flags
enum
{
	TEE_EFFECT_FROZEN = 1,
	TEE_NO_WEAPON = 2,
	TEE_EFFECT_SPARKLE = 4,
};

// sprite renderings
enum
{
	SPRITE_FLAG_FLIP_Y = 1,
	SPRITE_FLAG_FLIP_X = 2,

	LAYERRENDERFLAG_OPAQUE = 1,
	LAYERRENDERFLAG_TRANSPARENT = 2,

	TILERENDERFLAG_EXTEND = 4,

	OVERLAYRENDERFLAG_TEXT = 1,
	OVERLAYRENDERFLAG_EDITOR = 2,
};

class IEnvelopePointAccess
{
public:
	virtual ~IEnvelopePointAccess() = default;
	virtual int NumPoints() const = 0;
	virtual const CEnvPoint *GetPoint(int Index) const = 0;
	virtual const CEnvPointBezier *GetBezier(int Index) const = 0;
	int FindPointIndex(double TimeMillis) const;
};

class CMapBasedEnvelopePointAccess : public IEnvelopePointAccess
{
	int m_StartPoint;
	int m_NumPoints;
	int m_NumPointsMax;
	CEnvPoint *m_pPoints;
	CEnvPointBezier *m_pPointsBezier;
	CEnvPointBezier_upstream *m_pPointsBezierUpstream;

public:
	CMapBasedEnvelopePointAccess(class CDataFileReader *pReader);
	CMapBasedEnvelopePointAccess(class IMap *pMap);
	void SetPointsRange(int StartPoint, int NumPoints);
	int StartPoint() const;
	int NumPoints() const override;
	int NumPointsMax() const;
	const CEnvPoint *GetPoint(int Index) const override;
	const CEnvPointBezier *GetBezier(int Index) const override;
};

typedef void (*ENVELOPE_EVAL)(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser);

class CRenderTools
{
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;

	int m_TeeQuadContainerIndex;

	vec2 m_SpriteScale = vec2(-1.0f, -1.0f);

	static void GetRenderTeeBodyScale(float BaseSize, float &BodyScale);
	static void GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight);

	void SelectSprite(const CDataSprite *pSprite, int Flags);

	void RenderTee6(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha = 1.0f) const;
	void RenderTee7(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha = 1.0f);

public:
	class IGraphics *Graphics() const { return m_pGraphics; }
	class ITextRender *TextRender() const { return m_pTextRender; }

	void Init(class IGraphics *pGraphics, class ITextRender *pTextRender);

	void SelectSprite(int Id, int Flags = 0);
	void SelectSprite7(int Id, int Flags = 0);

	void GetSpriteScale(const CDataSprite *pSprite, float &ScaleX, float &ScaleY) const;
	void GetSpriteScale(int Id, float &ScaleX, float &ScaleY) const;
	void GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY) const;

	void DrawSprite(float x, float y, float Size) const;
	void DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight) const;
	void RenderCursor(vec2 Center, float Size) const;
	void RenderIcon(int ImageId, int SpriteId, const CUIRect *pRect, const ColorRGBA *pColor = nullptr);
	int QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float Size) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float Size) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height) const;

	// larger rendering methods
	static void GetRenderTeeBodySize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height);
	static void GetRenderTeeFeetSize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height);
	static void GetRenderTeeAnimScaleAndBaseSize(const CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize);

	// returns the offset to use, to render the tee with @see RenderTee exactly in the mid
	static void GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid);
	// object render methods
	void RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha = 1.0f);

	// map render methods (render_map.cpp)
	static void RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels);
	void RenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser) const;
	void ForceRenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha = 1.0f) const;
	void RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;

	// render a rectangle made of IndexIn tiles, over a background made of IndexOut tiles
	// the rectangle include all tiles in [RectX, RectX+RectW-1] x [RectY, RectY+RectH-1]
	void RenderTileRectangle(int RectX, int RectY, int RectW, int RectH, unsigned char IndexIn, unsigned char IndexOut, float Scale, ColorRGBA Color, int RenderFlags) const;

	void RenderTile(int x, int y, unsigned char Index, float Scale, ColorRGBA Color) const;

	// helpers
	void CalcScreenParams(float Aspect, float Zoom, float *pWidth, float *pHeight);
	void MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
		float ParallaxZoom, float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints);
	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom);
	void MapScreenToInterface(float CenterX, float CenterY);

	// DDRace

	void RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f);
	void RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
	void RenderSwitchmap(CSwitchTile *pSwitch, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
	void RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
	void RenderCustomEntitiesTilemap(void *pTiles, int TileSize, int IndexOffset, int FlagsOffset, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
};

#endif
