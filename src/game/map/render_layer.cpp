#include "render_layer.h"

#include <base/log.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/localization.h>
#include <game/mapitems.h>

#include <array>
#include <chrono>

/************************
 * Render Buffer Helper *
 ************************/
class CTexCoords
{
public:
	std::array<uint8_t, 4> m_aTexX;
	std::array<uint8_t, 4> m_aTexY;
};

constexpr static CTexCoords CalculateTexCoords(unsigned int Flags)
{
	CTexCoords TexCoord;
	TexCoord.m_aTexX = {0, 1, 1, 0};
	TexCoord.m_aTexY = {0, 0, 1, 1};

	if(Flags & TILEFLAG_XFLIP)
		std::rotate(std::begin(TexCoord.m_aTexX), std::begin(TexCoord.m_aTexX) + 2, std::end(TexCoord.m_aTexX));

	if(Flags & TILEFLAG_YFLIP)
		std::rotate(std::begin(TexCoord.m_aTexY), std::begin(TexCoord.m_aTexY) + 2, std::end(TexCoord.m_aTexY));

	if(Flags & (TILEFLAG_ROTATE >> 1))
	{
		std::rotate(std::begin(TexCoord.m_aTexX), std::begin(TexCoord.m_aTexX) + 3, std::end(TexCoord.m_aTexX));
		std::rotate(std::begin(TexCoord.m_aTexY), std::begin(TexCoord.m_aTexY) + 3, std::end(TexCoord.m_aTexY));
	}
	return TexCoord;
}

template<std::size_t N>
constexpr static std::array<CTexCoords, N> MakeTexCoordsTable()
{
	std::array<CTexCoords, N> aTexCoords = {};
	for(std::size_t i = 0; i < N; ++i)
		aTexCoords[i] = CalculateTexCoords(i);
	return aTexCoords;
}

constexpr std::array<CTexCoords, 8> TEX_COORDS_TABLE = MakeTexCoordsTable<8>();

static void FillTmpTile(CGraphicTile *pTmpTile, CGraphicTileTextureCoords *pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, const ivec2 &Offset, int Scale)
{
	if(pTmpTex)
	{
		uint8_t TableFlag = (Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP)) + ((Flags & TILEFLAG_ROTATE) >> 1);
		const auto &aTexX = TEX_COORDS_TABLE[TableFlag].m_aTexX;
		const auto &aTexY = TEX_COORDS_TABLE[TableFlag].m_aTexY;

		pTmpTex->m_TexCoordTopLeft.x = aTexX[0];
		pTmpTex->m_TexCoordTopLeft.y = aTexY[0];
		pTmpTex->m_TexCoordBottomLeft.x = aTexX[3];
		pTmpTex->m_TexCoordBottomLeft.y = aTexY[3];
		pTmpTex->m_TexCoordTopRight.x = aTexX[1];
		pTmpTex->m_TexCoordTopRight.y = aTexY[1];
		pTmpTex->m_TexCoordBottomRight.x = aTexX[2];
		pTmpTex->m_TexCoordBottomRight.y = aTexY[2];

		pTmpTex->m_TexCoordTopLeft.z = Index;
		pTmpTex->m_TexCoordBottomLeft.z = Index;
		pTmpTex->m_TexCoordTopRight.z = Index;
		pTmpTex->m_TexCoordBottomRight.z = Index;

		bool HasRotation = (Flags & TILEFLAG_ROTATE) != 0;
		pTmpTex->m_TexCoordTopLeft.w = HasRotation;
		pTmpTex->m_TexCoordBottomLeft.w = HasRotation;
		pTmpTex->m_TexCoordTopRight.w = HasRotation;
		pTmpTex->m_TexCoordBottomRight.w = HasRotation;
	}

	vec2 TopLeft(x * Scale + Offset.x, y * Scale + Offset.y);
	vec2 BottomRight(x * Scale + Scale + Offset.x, y * Scale + Scale + Offset.y);
	pTmpTile->m_TopLeft = TopLeft;
	pTmpTile->m_BottomLeft.x = TopLeft.x;
	pTmpTile->m_BottomLeft.y = BottomRight.y;
	pTmpTile->m_TopRight.x = BottomRight.x;
	pTmpTile->m_TopRight.y = TopLeft.y;
	pTmpTile->m_BottomRight = BottomRight;
}

static void FillTmpTileSpeedup(CGraphicTile *pTmpTile, CGraphicTileTextureCoords *pTmpTex, unsigned char Flags, int x, int y, const ivec2 &Offset, int Scale, short AngleRotate)
{
	int Angle = AngleRotate % 360;
	FillTmpTile(pTmpTile, pTmpTex, Angle >= 270 ? ROTATION_270 : (Angle >= 180 ? ROTATION_180 : (Angle >= 90 ? ROTATION_90 : 0)), AngleRotate % 90, x, y, Offset, Scale);
}

static bool AddTile(std::vector<CGraphicTile> &vTmpTiles, std::vector<CGraphicTileTextureCoords> &vTmpTileTexCoords, unsigned char Index, unsigned char Flags, int x, int y, bool DoTextureCoords, bool FillSpeedup = false, int AngleRotate = -1, const ivec2 &Offset = ivec2{0, 0}, int Scale = 32)
{
	if(Index <= 0)
		return false;

	vTmpTiles.emplace_back();
	CGraphicTile &Tile = vTmpTiles.back();
	CGraphicTileTextureCoords *pTileTex = nullptr;
	if(DoTextureCoords)
	{
		vTmpTileTexCoords.emplace_back();
		CGraphicTileTextureCoords &TileTex = vTmpTileTexCoords.back();
		pTileTex = &TileTex;
	}
	if(FillSpeedup)
		FillTmpTileSpeedup(&Tile, pTileTex, Flags, x, y, Offset, Scale, AngleRotate);
	else
		FillTmpTile(&Tile, pTileTex, Flags, Index, x, y, Offset, Scale);

	return true;
}

class CTmpQuadVertexTextured
{
public:
	float m_X, m_Y, m_CenterX, m_CenterY;
	unsigned char m_R, m_G, m_B, m_A;
	float m_U, m_V;
};

class CTmpQuadVertex
{
public:
	float m_X, m_Y, m_CenterX, m_CenterY;
	unsigned char m_R, m_G, m_B, m_A;
};

class CTmpQuad
{
public:
	CTmpQuadVertex m_aVertices[4];
};

class CTmpQuadTextured
{
public:
	CTmpQuadVertexTextured m_aVertices[4];
};

static void mem_copy_special(void *pDest, void *pSource, size_t Size, size_t Count, size_t Steps)
{
	size_t CurStep = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		mem_copy(((char *)pDest) + CurStep + i * Size, ((char *)pSource) + i * Size, Size);
		CurStep += Steps;
	}
}

bool CRenderLayerTile::CTileLayerVisuals::Init(unsigned int Width, unsigned int Height)
{
	m_Width = Width;
	m_Height = Height;
	if(Width == 0 || Height == 0)
		return false;
	if constexpr(sizeof(unsigned int) >= sizeof(ptrdiff_t))
		if(Width >= std::numeric_limits<std::ptrdiff_t>::max() || Height >= std::numeric_limits<std::ptrdiff_t>::max())
			return false;

	m_vTilesOfLayer.resize((size_t)Height * (size_t)Width);
	return true;
}

/**************
 * Base Layer *
 **************/

CRenderLayer::CRenderLayer(int GroupId, int LayerId, int Flags) :
	m_GroupId(GroupId), m_LayerId(LayerId), m_Flags(Flags) {}

void CRenderLayer::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional)
{
	CRenderComponent::OnInit(pGraphics, pTextRender, pRenderMap);
	m_pMap = pMap;
	m_pMapImages = pMapImages;
	m_RenderUploadCallback = FRenderUploadCallbackOptional;
	m_pEnvelopeManager = pEnvelopeManager;
}

void CRenderLayer::UseTexture(IGraphics::CTextureHandle TextureHandle)
{
	if(TextureHandle.IsValid())
		Graphics()->TextureSet(TextureHandle);
	else
		Graphics()->TextureClear();
}

void CRenderLayer::RenderLoading() const
{
	const char *pLoadingTitle = Localize("Loading map");
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	if(m_RenderUploadCallback.has_value())
		(*m_RenderUploadCallback)(pLoadingTitle, pLoadingMessage, 0);
}

bool CRenderLayer::IsVisibleInClipRegion(const std::optional<CClipRegion> &ClipRegion) const
{
	// always show unclipped regions
	if(!ClipRegion.has_value())
		return true;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float Left = ClipRegion->m_X;
	float Top = ClipRegion->m_Y;
	float Right = ClipRegion->m_X + ClipRegion->m_Width;
	float Bottom = ClipRegion->m_Y + ClipRegion->m_Height;

	return Right >= ScreenX0 && Left <= ScreenX1 && Bottom >= ScreenY0 && Top <= ScreenY1;
}

/**************
 * Group *
 **************/

CRenderLayerGroup::CRenderLayerGroup(int GroupId, CMapItemGroup *pGroup) :
	CRenderLayer(GroupId, 0, 0), m_pGroup(pGroup) {}

bool CRenderLayerGroup::DoRender(const CRenderLayerParams &Params)
{
	if(!g_Config.m_GfxNoclip || Params.m_RenderType == ERenderType::RENDERTYPE_FULL_DESIGN)
	{
		Graphics()->ClipDisable();
		if(m_pGroup->m_Version >= 2 && m_pGroup->m_UseClipping)
		{
			// set clipping
			Graphics()->MapScreenToInterface(Params.m_Center.x, Params.m_Center.y, Params.m_Zoom);

			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			float ScreenWidth = ScreenX1 - ScreenX0;
			float ScreenHeight = ScreenY1 - ScreenY0;
			float Left = m_pGroup->m_ClipX - ScreenX0;
			float Top = m_pGroup->m_ClipY - ScreenY0;
			float Right = m_pGroup->m_ClipX + m_pGroup->m_ClipW - ScreenX0;
			float Bottom = m_pGroup->m_ClipY + m_pGroup->m_ClipH - ScreenY0;

			if(Right < 0.0f || Left > ScreenWidth || Bottom < 0.0f || Top > ScreenHeight)
				return false;

			// Render debug before enabling the clip
			if(Params.m_DebugRenderGroupClips)
			{
				char aDebugText[32];
				str_format(aDebugText, sizeof(aDebugText), "Group %d", m_GroupId);
				RenderMap()->RenderDebugClip(m_pGroup->m_ClipX, m_pGroup->m_ClipY, m_pGroup->m_ClipW, m_pGroup->m_ClipH, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), Params.m_Zoom, aDebugText);
			}

			int ClipX = (int)std::round(Left * Graphics()->ScreenWidth() / ScreenWidth);
			int ClipY = (int)std::round(Top * Graphics()->ScreenHeight() / ScreenHeight);

			Graphics()->ClipEnable(
				ClipX,
				ClipY,
				(int)std::round(Right * Graphics()->ScreenWidth() / ScreenWidth) - ClipX,
				(int)std::round(Bottom * Graphics()->ScreenHeight() / ScreenHeight) - ClipY);
		}
	}
	return true;
}

void CRenderLayerGroup::Render(const CRenderLayerParams &Params)
{
	int ParallaxZoom = std::clamp((maximum(m_pGroup->m_ParallaxX, m_pGroup->m_ParallaxY)), 0, 100);
	float aPoints[4];
	Graphics()->MapScreenToWorld(Params.m_Center.x, Params.m_Center.y, m_pGroup->m_ParallaxX, m_pGroup->m_ParallaxY, (float)ParallaxZoom,
		m_pGroup->m_OffsetX, m_pGroup->m_OffsetY, Graphics()->ScreenAspect(), Params.m_Zoom, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

/**************
 * Tile Layer *
 **************/

CRenderLayerTile::CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayer(GroupId, LayerId, Flags)
{
	m_pLayerTilemap = pLayerTilemap;
	m_Color = ColorRGBA(m_pLayerTilemap->m_Color.r / 255.0f, m_pLayerTilemap->m_Color.g / 255.0f, m_pLayerTilemap->m_Color.b / 255.0f, pLayerTilemap->m_Color.a / 255.0f);
	m_pTiles = nullptr;
}

void CRenderLayerTile::RenderTileLayer(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals)
{
	CTileLayerVisuals &Visuals = pTileLayerVisuals ? *pTileLayerVisuals : m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int ScreenRectY0 = std::floor(ScreenY0 / 32);
	int ScreenRectX0 = std::floor(ScreenX0 / 32);
	int ScreenRectY1 = std::ceil(ScreenY1 / 32);
	int ScreenRectX1 = std::ceil(ScreenX1 / 32);

	if(IsVisibleInClipRegion(m_LayerClip))
	{
		// create the indice buffers we want to draw -- reuse them
		std::vector<char *> vpIndexOffsets;
		std::vector<unsigned int> vDrawCounts;

		int X0 = std::max(ScreenRectX0, 0);
		int X1 = std::min(ScreenRectX1, (int)Visuals.m_Width);
		int XR = X1 == std::numeric_limits<int>::min() ? X1 : X1 - 1;
		if(X0 <= XR)
		{
			int Y0 = std::max(ScreenRectY0, 0);
			int Y1 = std::min(ScreenRectY1, (int)Visuals.m_Height);

			unsigned long long Reserve = absolute(Y1 - Y0) + 1;
			vpIndexOffsets.reserve(Reserve);
			vDrawCounts.reserve(Reserve);

			for(int y = Y0; y < Y1; ++y)
			{
				dbg_assert(Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() >= Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset(), "Tile offsets are not monotone.");
				unsigned int NumVertices = ((Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() - Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].DoDraw() ? 6lu : 0lu);

				if(NumVertices)
				{
					vpIndexOffsets.push_back((offset_ptr_size)Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset());
					vDrawCounts.push_back(NumVertices);
				}
			}

			int DrawCount = vpIndexOffsets.size();
			if(DrawCount != 0)
			{
				Graphics()->RenderTileLayer(Visuals.m_BufferContainerIndex, Color, vpIndexOffsets.data(), vDrawCounts.data(), DrawCount);
			}
		}
	}

	if(Params.m_RenderTileBorder && (ScreenRectX1 > (int)Visuals.m_Width || ScreenRectY1 > (int)Visuals.m_Height || ScreenRectX0 < 0 || ScreenRectY0 < 0))
	{
		RenderTileBorder(Color, Params, &Visuals);
	}

	if(Params.m_DebugRenderTileClips && m_LayerClip.has_value())
	{
		const CClipRegion &Clip = m_LayerClip.value();
		char aDebugText[32];
		str_format(aDebugText, sizeof(aDebugText), "Group %d LayerId %d", m_GroupId, m_LayerId);
		RenderMap()->RenderDebugClip(Clip.m_X, Clip.m_Y, Clip.m_Width, Clip.m_Height, ColorRGBA(1.0f, 0.5f, 0.0f, 1.0f), Params.m_Zoom, aDebugText);
	}
}

void CRenderLayerTile::RenderTileBorder(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals)
{
	for(CRenderTileLayerBorder &Border : pTileLayerVisuals->m_vBorders)
	{
		Border.Draw(pTileLayerVisuals, Color);
	}

	if(Params.m_DebugRenderTileBorderClips)
	{
		for(CRenderTileLayerBorder &Border : pTileLayerVisuals->m_vBorders)
		{
			const CClipRegion &Clip = Border.m_BorderClip;
			char aDebugText[32];
			str_format(aDebugText, sizeof(aDebugText), "Group %d LayerId %d BorderType %d", m_GroupId, m_LayerId, Border.GetType());
			RenderMap()->RenderDebugClip(Clip.m_X, Clip.m_Y, Clip.m_Width, Clip.m_Height, ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f), Params.m_Zoom, aDebugText);
		}
	}
}

void CRenderLayerTile::RenderKillTileBorder(const ColorRGBA &Color)
{
	CTileLayerVisuals &Visuals = m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int BorderY0 = std::floor(ScreenY0 / 32);
	int BorderX0 = std::floor(ScreenX0 / 32);
	int BorderY1 = std::ceil(ScreenY1 / 32);
	int BorderX1 = std::ceil(ScreenX1 / 32);

	if(BorderX0 >= -BorderRenderDistance && BorderY0 >= -BorderRenderDistance && BorderX1 <= (int)Visuals.m_Width + BorderRenderDistance && BorderY1 <= (int)Visuals.m_Height + BorderRenderDistance)
		return;
	if(!Visuals.m_BorderKillTile.DoDraw())
		return;

	BorderX0 = std::clamp(BorderX0, -300, (int)Visuals.m_Width + 299);
	BorderY0 = std::clamp(BorderY0, -300, (int)Visuals.m_Height + 299);
	BorderX1 = std::clamp(BorderX1, -300, (int)Visuals.m_Width + 299);
	BorderY1 = std::clamp(BorderY1, -300, (int)Visuals.m_Height + 299);

	auto DrawKillBorder = [&](vec2 Offset, vec2 Scale) {
		offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderKillTile.IndexBufferByteOffset();
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, 1);
	};

	// Draw left kill tile border
	if(BorderX0 < -BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(BorderX0, BorderY0),
			vec2(-BorderRenderDistance - BorderX0, BorderY1 - BorderY0));
	}
	// Draw top kill tile border
	if(BorderY0 < -BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderRenderDistance), BorderY0),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderRenderDistance) - std::max(BorderX0, -BorderRenderDistance), -BorderRenderDistance - BorderY0));
	}
	// Draw right kill tile border
	if(BorderX1 > (int)Visuals.m_Width + BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(Visuals.m_Width + BorderRenderDistance, BorderY0),
			vec2(BorderX1 - (Visuals.m_Width + BorderRenderDistance), BorderY1 - BorderY0));
	}
	// Draw bottom kill tile border
	if(BorderY1 > (int)Visuals.m_Height + BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderRenderDistance), Visuals.m_Height + BorderRenderDistance),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderRenderDistance) - std::max(BorderX0, -BorderRenderDistance), BorderY1 - (Visuals.m_Height + BorderRenderDistance)));
	}
}

ColorRGBA CRenderLayerTile::GetRenderColor(const CRenderLayerParams &Params) const
{
	ColorRGBA Color = m_Color;
	if(Params.m_EntityOverlayVal && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		Color.a *= (100 - Params.m_EntityOverlayVal) / 100.0f;

	ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(m_pLayerTilemap->m_ColorEnvOffset, m_pLayerTilemap->m_ColorEnv, ColorEnv, 4);
	Color = Color.Multiply(ColorEnv);
	return Color;
}

void CRenderLayerTile::Render(const CRenderLayerParams &Params)
{
	UseTexture(GetTexture());
	ColorRGBA Color = GetRenderColor(Params);
	if(Graphics()->IsTileBufferingEnabled() && Params.m_TileAndQuadBuffering)
	{
		RenderTileLayerWithTileBuffer(Color, Params);
	}
	else
	{
		RenderTileLayerNoTileBuffer(Color, Params);
	}
}

bool CRenderLayerTile::DoRender(const CRenderLayerParams &Params)
{
	// skip rendering if we render background force, but deactivated tile layer and want to render a tilelayer
	if(!g_Config.m_ClBackgroundShowTilesLayers && Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering anything but entities if we only want to render entities
	if(Params.m_EntityOverlayVal == 100 && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering if detail layers if not wanted
	if(m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && Params.m_RenderType != ERenderType::RENDERTYPE_FULL_DESIGN) // detail but no details
		return false;
	return true;
}

void CRenderLayerTile::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
}

void CRenderLayerTile::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}

void CRenderLayerTile::Init()
{
	if(m_pLayerTilemap->m_Image >= 0 && m_pLayerTilemap->m_Image < m_pMapImages->Num())
		m_TextureHandle = m_pMapImages->Get(m_pLayerTilemap->m_Image);
	else
		m_TextureHandle.Invalidate();
	UploadTileData(m_VisualTiles, 0, false);
}

void CRenderLayerTile::UploadTileData(std::optional<CTileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer)
{
	if(!Graphics()->IsTileBufferingEnabled())
		return;

	// prepare all visuals for all tile layers
	std::vector<CGraphicTile> vTmpTiles;
	std::vector<CGraphicTileTextureCoords> vTmpTileTexCoords;
	std::vector<CGraphicTile> vTmpBorderTopTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderTopTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderLeftTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderLeftTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderRightTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderRightTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderBottomTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderBottomTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderCorners;
	std::vector<CGraphicTileTextureCoords> vTmpBorderCornersTexCoords;

	const bool DoTextureCoords = GetTexture().IsValid();

	// create the visual and set it in the optional, afterwards get it
	CTileLayerVisuals v;
	v.OnInit(this);
	VisualsOptional = v;
	CTileLayerVisuals &Visuals = VisualsOptional.value();

	if(!Visuals.Init(m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height))
		return;

	Visuals.m_IsTextured = DoTextureCoords;

	if(!DoTextureCoords)
	{
		vTmpTiles.reserve((size_t)m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height);
		vTmpBorderTopTiles.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderBottomTiles.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderLeftTiles.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderRightTiles.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderCorners.reserve((size_t)4);
	}
	else
	{
		vTmpTileTexCoords.reserve((size_t)m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height);
		vTmpBorderTopTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderBottomTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderLeftTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderRightTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderCornersTexCoords.reserve((size_t)4);
	}

	int DrawLeft = m_pLayerTilemap->m_Width;
	int DrawRight = 0;
	int DrawTop = m_pLayerTilemap->m_Height;
	int DrawBottom = 0;

	// borders
	vec2 VisualSize(Visuals.m_Width * 32.0f, Visuals.m_Height * 32.0f);

	// border is beyond the death border, because you can zoom out very much
	float BorderLength = (BorderRenderDistance + 100.0f) * 32.0f * 32.0f;

	// corners
	CRenderTileLayerBorder BorderTopLeft(
		vec2(0.0f, 0.0f),
		EBorderType::CORNER | EBorderType::LEFT | EBorderType::TOP,
		CClipRegion(-BorderLength, -BorderLength, BorderLength, BorderLength),
		1);
	CRenderTileLayerBorder BorderTopRight(
		vec2(VisualSize.x, 0.0f),
		EBorderType::CORNER | EBorderType::RIGHT | EBorderType::TOP,
		CClipRegion(VisualSize.x, -BorderLength, BorderLength, BorderLength),
		1);
	CRenderTileLayerBorder BorderBottomLeft(
		vec2(0.0f, VisualSize.y),
		EBorderType::CORNER | EBorderType::LEFT | EBorderType::BOTTOM,
		CClipRegion(-BorderLength, VisualSize.y, BorderLength, BorderLength),
		1);
	CRenderTileLayerBorder BorderBottomRight(
		VisualSize,
		EBorderType::CORNER | EBorderType::RIGHT | EBorderType::BOTTOM,
		CClipRegion(VisualSize.x, VisualSize.y, BorderLength, BorderLength),
		1);

	// horizontal and vertical borders
	CRenderTileLayerBorder BorderTop(
		vec2(0.0f, 0.0f),
		EBorderType::TOP,
		CClipRegion(0.0f, -BorderLength, VisualSize.x, BorderLength),
		Visuals.m_Width);
	CRenderTileLayerBorder BorderBottom(
		vec2(0.0f, VisualSize.y),
		EBorderType::BOTTOM,
		CClipRegion(0.0f, VisualSize.y, VisualSize.x, BorderLength),
		Visuals.m_Width);

	CRenderTileLayerBorder BorderLeft(
		vec2(0.0, 0.0f),
		EBorderType::LEFT,
		CClipRegion(-BorderLength, 0.0f, BorderLength, VisualSize.y),
		Visuals.m_Height);
	CRenderTileLayerBorder BorderRight(
		vec2(VisualSize.x, 0.0f),
		EBorderType::RIGHT,
		CClipRegion(VisualSize.x, 0.0f, BorderLength, VisualSize.y),
		Visuals.m_Height);

	int x = 0;
	int y = 0;
	for(y = 0; y < m_pLayerTilemap->m_Height; ++y)
	{
		for(x = 0; x < m_pLayerTilemap->m_Width; ++x)
		{
			unsigned char Index = 0;
			unsigned char Flags = 0;
			int AngleRotate = -1;
			GetTileData(&Index, &Flags, &AngleRotate, x, y, CurOverlay);

			// the amount of tiles handled before this tile
			int TilesHandledCount = vTmpTiles.size();
			Visuals.m_vTilesOfLayer[y * m_pLayerTilemap->m_Width + x].SetIndexBufferByteOffset((offset_ptr32)(TilesHandledCount));

			if(AddTile(vTmpTiles, vTmpTileTexCoords, Index, Flags, x, y, DoTextureCoords, AddAsSpeedup, AngleRotate))
			{
				Visuals.m_vTilesOfLayer[y * m_pLayerTilemap->m_Width + x].Draw(true);

				// calculate clip region boundaries based on draws
				DrawLeft = std::min(DrawLeft, x);
				DrawRight = std::max(DrawRight, x);
				DrawTop = std::min(DrawTop, y);
				DrawBottom = std::max(DrawBottom, y);
			}

			// do the border tiles
			if(x == 0)
			{
				if(y == 0)
				{
					BorderTopLeft.m_vBorder[0].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, -32}))
						BorderTopLeft.m_vBorder[0].Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					BorderBottomLeft.m_vBorder[0].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
						BorderBottomLeft.m_vBorder[0].Draw(true);
				}
				BorderLeft.m_vBorder[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderLeftTiles.size()));
				if(AddTile(vTmpBorderLeftTiles, vTmpBorderLeftTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
					BorderLeft.m_vBorder[y].Draw(true);
			}
			else if(x == m_pLayerTilemap->m_Width - 1)
			{
				if(y == 0)
				{
					BorderTopRight.m_vBorder[0].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
						BorderTopRight.m_vBorder[0].Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					BorderBottomRight.m_vBorder[0].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
						BorderBottomRight.m_vBorder[0].Draw(true);
				}
				BorderRight.m_vBorder[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderRightTiles.size()));
				if(AddTile(vTmpBorderRightTiles, vTmpBorderRightTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					BorderRight.m_vBorder[y].Draw(true);
			}
			if(y == 0)
			{
				BorderTop.m_vBorder[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderTopTiles.size()));
				if(AddTile(vTmpBorderTopTiles, vTmpBorderTopTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
					BorderTop.m_vBorder[x].Draw(true);
			}
			else if(y == m_pLayerTilemap->m_Height - 1)
			{
				BorderBottom.m_vBorder[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderBottomTiles.size()));
				if(AddTile(vTmpBorderBottomTiles, vTmpBorderBottomTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					BorderBottom.m_vBorder[x].Draw(true);
			}
		}
	}

	// shrink clip region
	// we only apply the clip once for the first overlay type (tile visuals). Physic layers can have multiple layers for text, e.g. speedup force
	// the first overlay is always the largest and you will never find an overlay, where the text is written over AIR
	if(CurOverlay == 0)
	{
		if(DrawLeft > DrawRight || DrawTop > DrawBottom)
		{
			// we are drawing nothing, layer is empty
			m_LayerClip->m_Height = 0.0f;
			m_LayerClip->m_Width = 0.0f;
		}
		else
		{
			m_LayerClip->m_X = DrawLeft * 32.0f;
			m_LayerClip->m_Y = DrawTop * 32.0f;
			m_LayerClip->m_Width = (DrawRight - DrawLeft + 1) * 32.0f;
			m_LayerClip->m_Height = (DrawBottom - DrawTop + 1) * 32.0f;
		}
	}

	// append one kill tile to the gamelayer
	if(IsGameLayer)
	{
		Visuals.m_BorderKillTile.SetIndexBufferByteOffset((offset_ptr32)(vTmpTiles.size()));
		if(AddTile(vTmpTiles, vTmpTileTexCoords, TILE_DEATH, 0, 0, 0, DoTextureCoords))
			Visuals.m_BorderKillTile.Draw(true);
	}

	// inserts and clears tiles and tile texture coords
	auto InsertTiles = [&](std::vector<CGraphicTile> &vTiles, std::vector<CGraphicTileTextureCoords> &vTexCoords) {
		vTmpTiles.insert(vTmpTiles.end(), vTiles.begin(), vTiles.end());
		vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTexCoords.begin(), vTexCoords.end());
		vTiles.clear();
		vTexCoords.clear();
	};

	// add the border corners, then the borders and fix their byte offsets
	int TilesHandledCount = vTmpTiles.size();
	BorderTopLeft.m_vBorder[0].AddIndexBufferByteOffset(TilesHandledCount);
	BorderTopRight.m_vBorder[0].AddIndexBufferByteOffset(TilesHandledCount);
	BorderBottomLeft.m_vBorder[0].AddIndexBufferByteOffset(TilesHandledCount);
	BorderBottomRight.m_vBorder[0].AddIndexBufferByteOffset(TilesHandledCount);

	// add the Corners to the tiles
	InsertTiles(vTmpBorderCorners, vTmpBorderCornersTexCoords);

	// add borders
	auto AddIfNotEmpty = [&](CRenderTileLayerBorder &BorderRegion, std::vector<CGraphicTile> *pvTmpBorderTiles, std::vector<CGraphicTileTextureCoords> *pvTmpBorderTextureTiles) {
		// if the border is empty, we can ignore it
		if(BorderRegion.Adjust())
		{
			if(pvTmpBorderTextureTiles != nullptr && pvTmpBorderTiles != nullptr)
				InsertTiles(*pvTmpBorderTiles, *pvTmpBorderTextureTiles);
			BorderRegion.OnInit(this);
			Visuals.m_vBorders.push_back(BorderRegion);
		}
		else if(pvTmpBorderTextureTiles != nullptr && pvTmpBorderTiles != nullptr)
		{
			pvTmpBorderTiles->clear();
			pvTmpBorderTextureTiles->clear();
		}
	};

	AddIfNotEmpty(BorderTopLeft, nullptr, nullptr);
	AddIfNotEmpty(BorderTopRight, nullptr, nullptr);
	AddIfNotEmpty(BorderBottomLeft, nullptr, nullptr);
	AddIfNotEmpty(BorderBottomRight, nullptr, nullptr);

	// now the borders
	int TilesHandledCountTop = vTmpTiles.size();
	int TilesHandledCountBottom = TilesHandledCountTop + vTmpBorderTopTiles.size();
	int TilesHandledCountLeft = TilesHandledCountBottom + vTmpBorderBottomTiles.size();
	int TilesHandledCountRight = TilesHandledCountLeft + vTmpBorderLeftTiles.size();

	if(m_pLayerTilemap->m_Width > 0 && m_pLayerTilemap->m_Height > 0)
	{
		for(int BorderTileId = 0; BorderTileId < std::max(m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height); ++BorderTileId)
		{
			if(BorderTileId < (int)BorderTop.m_vBorder.size())
				BorderTop.m_vBorder[BorderTileId].AddIndexBufferByteOffset(TilesHandledCountTop);
			if(BorderTileId < (int)BorderBottom.m_vBorder.size())
				BorderBottom.m_vBorder[BorderTileId].AddIndexBufferByteOffset(TilesHandledCountBottom);
			if(BorderTileId < (int)BorderLeft.m_vBorder.size())
				BorderLeft.m_vBorder[BorderTileId].AddIndexBufferByteOffset(TilesHandledCountLeft);
			if(BorderTileId < (int)BorderRight.m_vBorder.size())
				BorderRight.m_vBorder[BorderTileId].AddIndexBufferByteOffset(TilesHandledCountRight);
		}
	}

	AddIfNotEmpty(BorderTop, &vTmpBorderTopTiles, &vTmpBorderTopTilesTexCoords);
	AddIfNotEmpty(BorderBottom, &vTmpBorderBottomTiles, &vTmpBorderBottomTilesTexCoords);

	AddIfNotEmpty(BorderLeft, &vTmpBorderLeftTiles, &vTmpBorderLeftTilesTexCoords);
	AddIfNotEmpty(BorderRight, &vTmpBorderRightTiles, &vTmpBorderRightTilesTexCoords);

	// setup params
	float *pTmpTiles = vTmpTiles.empty() ? nullptr : (float *)vTmpTiles.data();
	unsigned char *pTmpTileTexCoords = vTmpTileTexCoords.empty() ? nullptr : (unsigned char *)vTmpTileTexCoords.data();

	Visuals.m_BufferContainerIndex = -1;
	size_t UploadDataSize = vTmpTileTexCoords.size() * sizeof(CGraphicTileTextureCoords) + vTmpTiles.size() * sizeof(CGraphicTile);
	if(UploadDataSize > 0)
	{
		char *pUploadData = (char *)malloc(sizeof(char) * UploadDataSize);

		mem_copy_special(pUploadData, pTmpTiles, sizeof(vec2), vTmpTiles.size() * 4, (DoTextureCoords ? sizeof(ubvec4) : 0));
		if(DoTextureCoords)
		{
			mem_copy_special(pUploadData + sizeof(vec2), pTmpTileTexCoords, sizeof(ubvec4), vTmpTiles.size() * 4, sizeof(vec2));
		}

		// first create the buffer object
		int BufferObjectIndex = Graphics()->CreateBufferObject(UploadDataSize, pUploadData, 0, true);

		// then create the buffer container
		SBufferContainerInfo ContainerInfo;
		ContainerInfo.m_Stride = (DoTextureCoords ? (sizeof(float) * 2 + sizeof(ubvec4)) : 0);
		ContainerInfo.m_VertBufferBindingIndex = BufferObjectIndex;
		ContainerInfo.m_vAttributes.emplace_back();
		SBufferContainerInfo::SAttribute *pAttr = &ContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = nullptr;
		pAttr->m_FuncType = 0;
		if(DoTextureCoords)
		{
			ContainerInfo.m_vAttributes.emplace_back();
			pAttr = &ContainerInfo.m_vAttributes.back();
			pAttr->m_DataTypeCount = 4;
			pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
			pAttr->m_Normalized = false;
			pAttr->m_pOffset = (void *)(sizeof(vec2));
			pAttr->m_FuncType = 1;
		}

		Visuals.m_BufferContainerIndex = Graphics()->CreateBufferContainer(&ContainerInfo);
		// and finally inform the backend how many indices are required
		Graphics()->IndicesNumRequiredNotify(vTmpTiles.size() * 6);
	}
	RenderLoading();
}

void CRenderLayerTile::Unload()
{
	if(m_VisualTiles.has_value())
	{
		m_VisualTiles->Unload();
		m_VisualTiles = std::nullopt;
	}
}

void CRenderLayerTile::CTileLayerVisuals::Unload()
{
	Graphics()->DeleteBufferContainer(m_BufferContainerIndex);
}

int CRenderLayerTile::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTile);
	return m_pLayerTilemap->m_Data;
}

void *CRenderLayerTile::GetRawData() const
{
	unsigned int TileSize;
	unsigned int DataIndex = GetDataIndex(TileSize);
	void *pTiles = m_pMap->GetData(DataIndex);
	int Size = m_pMap->GetDataSize(DataIndex);

	if(!pTiles || Size < m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height * (int)TileSize)
		return nullptr;

	return pTiles;
}

void CRenderLayerTile::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional)
{
	CRenderLayer::OnInit(pGraphics, pTextRender, pRenderMap, pEnvelopeManager, pMap, pMapImages, FRenderUploadCallbackOptional);
	InitTileData();
	m_LayerClip = CClipRegion(0.0f, 0.0f, m_pLayerTilemap->m_Width * 32.0f, m_pLayerTilemap->m_Height * 32.0f);
}

void CRenderLayerTile::InitTileData()
{
	m_pTiles = GetData<CTile>();
}

template<class T>
T *CRenderLayerTile::GetData() const
{
	return (T *)GetRawData();
}

void CRenderLayerTile::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTiles[y * m_pLayerTilemap->m_Width + x].m_Index;
	*pFlags = m_pTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
}

/*********************
 * Tile Layer Border *
 *********************/

CRenderLayerTile::CRenderTileLayerBorder::CRenderTileLayerBorder(vec2 Offset, int Type, CClipRegion ClipRegion, size_t Size) :
	m_BorderClip(ClipRegion), m_Offset(Offset), m_Type(Type)
{
	m_vBorder.resize(Size);
	m_FrontOffset = 0;
}

bool CRenderLayerTile::CRenderTileLayerBorder::Adjust()
{
	// remove empty tiles from the back of the border
	while(!m_vBorder.empty() && !m_vBorder.back().DoDraw())
	{
		m_vBorder.pop_back();
	}

	// no tiles left
	if(m_vBorder.empty())
		return false;

	// corners need no clip region adjustment
	if(m_Type & EBorderType::CORNER)
		return true;

	// adjust clip region
	// TODO in theory we can remove the first TileId-1 tiles, but this is not trivial
	int FirstDrawId;
	for(FirstDrawId = 0; FirstDrawId < (int)m_vBorder.size(); ++FirstDrawId)
	{
		if(m_vBorder[FirstDrawId].DoDraw())
			break;
	}

	// this is guaranteed to have draws, since we 'pop_back-ed' the back above
	int LastDrawId = m_vBorder.size() - 1;
	if(m_Type & (EBorderType::TOP | EBorderType::BOTTOM))
	{
		m_BorderClip.m_X += FirstDrawId * 32.0f;
		m_BorderClip.m_Width = (LastDrawId - FirstDrawId + 1) * 32.0f;
	}
	else
	{
		m_BorderClip.m_Y += FirstDrawId * 32.0f;
		m_BorderClip.m_Height = (LastDrawId - FirstDrawId + 1) * 32.0f;
	}

	// remove empty front indices
	if(FirstDrawId > 0)
	{
		m_vBorder.erase(m_vBorder.begin(), m_vBorder.begin() + FirstDrawId - 1);
	}
	m_FrontOffset = FirstDrawId;
	return true;
}

void CRenderLayerTile::CRenderTileLayerBorder::Draw(const CTileLayerVisuals *pVisual, const ColorRGBA &Color)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// screen rect <-> border rect intersection
	float BorderX0 = std::max(ScreenX0, m_BorderClip.m_X);
	float BorderX1 = std::min(ScreenX1, m_BorderClip.m_X + m_BorderClip.m_Width);

	if(BorderX0 >= BorderX1)
		return;

	float BorderY0 = std::max(ScreenY0, m_BorderClip.m_Y);
	float BorderY1 = std::min(ScreenY1, m_BorderClip.m_Y + m_BorderClip.m_Height);

	if(BorderY0 >= BorderY1)
		return;

	// calculate how many tiles are needed to be drawn
	auto GetNumTiles = [](int Border, int TileOffset) -> float {
		// add an extra tile, for mouse movements
		return std::ceil(std::abs(Border) / 32.0f) - (float)TileOffset + 1;
	};

	vec2 NumTiles(1.0f, 1.0f);
	if(m_Type & EBorderType::TOP)
		NumTiles.y = GetNumTiles(BorderY0, 0);
	if(m_Type & EBorderType::BOTTOM)
		NumTiles.y = GetNumTiles(BorderY1, pVisual->m_Height);
	if(m_Type & EBorderType::LEFT)
		NumTiles.x = GetNumTiles(BorderX0, 0);
	if(m_Type & EBorderType::RIGHT)
		NumTiles.x = GetNumTiles(BorderX1, pVisual->m_Width);

	if((m_Type & EBorderType::CORNER) != 0)
	{
		// corners only have one tile, which always is drawn
		offset_ptr_size pOffset = (offset_ptr_size)m_vBorder.data()->IndexBufferByteOffset();
		Graphics()->RenderBorderTiles(pVisual->m_BufferContainerIndex, Color, pOffset, m_Offset, NumTiles, 1u);
		return;
	}

	const bool IsHorizontal = (m_Type & (EBorderType::TOP | EBorderType::BOTTOM)) != 0;
	float StartScreen = IsHorizontal ? BorderX0 : BorderY0;
	float EndScreen = IsHorizontal ? BorderX1 : BorderY1;
	int StartIndex = (int)std::floor(StartScreen / 32.0f);
	int EndIndex = (int)std::ceil(EndScreen / 32.0f);

	// we deleted the front objects, because they contained no draws
	StartIndex -= m_FrontOffset;
	EndIndex -= m_FrontOffset;

	if(StartIndex >= (int)m_vBorder.size() || EndIndex < 0)
		return;

	StartIndex = std::max(StartIndex, 0);
	EndIndex = std::min(EndIndex, (int)m_vBorder.size() - 1);

	// probably impossible but safe
	if(EndIndex < StartIndex)
		return;

	const CTileVisual &StartVisual = m_vBorder[StartIndex];
	const CTileVisual &EndVisual = m_vBorder[EndIndex];

	unsigned int DrawNum = ((EndVisual.IndexBufferByteOffset() - StartVisual.IndexBufferByteOffset()) / (sizeof(unsigned int) * 6)) + (EndVisual.DoDraw() ? 1lu : 0lu);
	offset_ptr_size pOffset = (offset_ptr_size)StartVisual.IndexBufferByteOffset();
	Graphics()->RenderBorderTiles(pVisual->m_BufferContainerIndex, Color, pOffset, m_Offset, NumTiles, DrawNum);
}

/**************
 * Quad Layer *
 **************/

CRenderLayerQuads::CRenderLayerQuads(int GroupId, int LayerId, int Flags, CMapItemLayerQuads *pLayerQuads) :
	CRenderLayer(GroupId, LayerId, Flags)
{
	m_pLayerQuads = pLayerQuads;
	m_pQuads = nullptr;
}

void CRenderLayerQuads::RenderQuadLayer(float Alpha, const CRenderLayerParams &Params)
{
	CQuadLayerVisuals &Visuals = m_VisualQuad.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	for(auto &QuadCluster : m_vQuadClusters)
	{
		if(!IsVisibleInClipRegion(QuadCluster.m_ClipRegion))
			continue;

		if(!QuadCluster.m_Grouped)
		{
			bool AnyVisible = false;
			for(int QuadClusterId = 0; QuadClusterId < QuadCluster.m_NumQuads; ++QuadClusterId)
			{
				CQuad *pQuad = &m_pQuads[QuadCluster.m_StartIndex + QuadClusterId];

				ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
				if(pQuad->m_ColorEnv >= 0)
				{
					m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, 4);
				}
				Color.a *= Alpha;

				SQuadRenderInfo &QInfo = QuadCluster.m_vQuadRenderInfo[QuadClusterId];
				if(Color.a < 0.0f)
					Color.a = 0.0f;
				QInfo.m_Color = Color;
				const bool IsVisible = Color.a >= 0.0f;
				AnyVisible |= IsVisible;

				if(IsVisible)
				{
					ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3);
					QInfo.m_Offsets.x = Position.r;
					QInfo.m_Offsets.y = Position.g;
					QInfo.m_Rotation = Position.b / 180.0f * pi;
				}
			}
			if(AnyVisible)
				Graphics()->RenderQuadLayer(Visuals.m_BufferContainerIndex, QuadCluster.m_vQuadRenderInfo.data(), QuadCluster.m_NumQuads, QuadCluster.m_StartIndex);
		}
		else
		{
			SQuadRenderInfo &QInfo = QuadCluster.m_vQuadRenderInfo[0];

			ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			if(QuadCluster.m_ColorEnv >= 0)
			{
				m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(QuadCluster.m_ColorEnvOffset, QuadCluster.m_ColorEnv, Color, 4);
			}

			Color.a *= Alpha;
			if(Color.a <= 0.0f)
				continue;
			QInfo.m_Color = Color;

			if(QuadCluster.m_PosEnv >= 0)
			{
				ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(QuadCluster.m_PosEnvOffset, QuadCluster.m_PosEnv, Position, 3);

				QInfo.m_Offsets.x = Position.r;
				QInfo.m_Offsets.y = Position.g;
				QInfo.m_Rotation = Position.b / 180.0f * pi;
			}
			Graphics()->RenderQuadLayer(Visuals.m_BufferContainerIndex, &QInfo, (size_t)QuadCluster.m_NumQuads, QuadCluster.m_StartIndex, true);
		}
	}

	if(Params.m_DebugRenderClusterClips)
	{
		for(auto &QuadCluster : m_vQuadClusters)
		{
			if(!IsVisibleInClipRegion(QuadCluster.m_ClipRegion) || !QuadCluster.m_ClipRegion.has_value())
				continue;

			char aDebugText[64];
			str_format(aDebugText, sizeof(aDebugText), "Group %d, quad layer %d, quad start %d, grouped %d", m_GroupId, m_LayerId, QuadCluster.m_StartIndex, QuadCluster.m_Grouped);
			RenderMap()->RenderDebugClip(QuadCluster.m_ClipRegion->m_X, QuadCluster.m_ClipRegion->m_Y, QuadCluster.m_ClipRegion->m_Width, QuadCluster.m_ClipRegion->m_Height, ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f), Params.m_Zoom, aDebugText);
		}
	}
}

void CRenderLayerQuads::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional)
{
	CRenderLayer::OnInit(pGraphics, pTextRender, pRenderMap, pEnvelopeManager, pMap, pMapImages, FRenderUploadCallbackOptional);
	int DataSize = m_pMap->GetDataSize(m_pLayerQuads->m_Data);
	if(m_pLayerQuads->m_NumQuads > 0 && DataSize / (int)sizeof(CQuad) >= m_pLayerQuads->m_NumQuads)
		m_pQuads = (CQuad *)m_pMap->GetDataSwapped(m_pLayerQuads->m_Data);
}

void CRenderLayerQuads::Init()
{
	if(m_pLayerQuads->m_Image >= 0 && m_pLayerQuads->m_Image < m_pMapImages->Num())
		m_TextureHandle = m_pMapImages->Get(m_pLayerQuads->m_Image);
	else
		m_TextureHandle.Invalidate();

	if(!Graphics()->IsQuadBufferingEnabled())
		return;

	std::vector<CTmpQuad> vTmpQuads;
	std::vector<CTmpQuadTextured> vTmpQuadsTextured;
	CQuadLayerVisuals v;
	v.OnInit(this);
	m_VisualQuad = v;
	CQuadLayerVisuals *pQLayerVisuals = &(m_VisualQuad.value());

	const bool Textured = m_pLayerQuads->m_Image >= 0 && m_pLayerQuads->m_Image < m_pMapImages->Num();

	if(Textured)
		vTmpQuadsTextured.resize(m_pLayerQuads->m_NumQuads);
	else
		vTmpQuads.resize(m_pLayerQuads->m_NumQuads);

	auto SetQuadRenderInfo = [&](SQuadRenderInfo &QInfo, int QuadId, bool InitInfo) {
		CQuad *pQuad = &m_pQuads[QuadId];

		// init for envelopeless quad layers
		if(InitInfo)
		{
			QInfo.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			QInfo.m_Offsets.x = 0;
			QInfo.m_Offsets.y = 0;
			QInfo.m_Rotation = 0;
		}

		for(int j = 0; j < 4; ++j)
		{
			int QuadIdX = j;
			if(j == 2)
				QuadIdX = 3;
			else if(j == 3)
				QuadIdX = 2;
			if(!Textured)
			{
				// ignore the conversion for the position coordinates
				vTmpQuads[QuadId].m_aVertices[j].m_X = fx2f(pQuad->m_aPoints[QuadIdX].x);
				vTmpQuads[QuadId].m_aVertices[j].m_Y = fx2f(pQuad->m_aPoints[QuadIdX].y);
				vTmpQuads[QuadId].m_aVertices[j].m_CenterX = fx2f(pQuad->m_aPoints[4].x);
				vTmpQuads[QuadId].m_aVertices[j].m_CenterY = fx2f(pQuad->m_aPoints[4].y);
				vTmpQuads[QuadId].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
				vTmpQuads[QuadId].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
				vTmpQuads[QuadId].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
				vTmpQuads[QuadId].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
			}
			else
			{
				// ignore the conversion for the position coordinates
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_X = fx2f(pQuad->m_aPoints[QuadIdX].x);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_Y = fx2f(pQuad->m_aPoints[QuadIdX].y);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_CenterX = fx2f(pQuad->m_aPoints[4].x);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_CenterY = fx2f(pQuad->m_aPoints[4].y);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_U = fx2f(pQuad->m_aTexcoords[QuadIdX].x);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_V = fx2f(pQuad->m_aTexcoords[QuadIdX].y);
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
				vTmpQuadsTextured[QuadId].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
			}
		}
	};

	m_vQuadClusters.clear();
	CQuadCluster QuadCluster;

	// create quad clusters
	int QuadStart = 0;
	while(QuadStart < m_pLayerQuads->m_NumQuads)
	{
		QuadCluster.m_StartIndex = QuadStart;
		QuadCluster.m_Grouped = true;
		QuadCluster.m_ColorEnv = m_pQuads[QuadStart].m_ColorEnv;
		QuadCluster.m_ColorEnvOffset = m_pQuads[QuadStart].m_ColorEnvOffset;
		QuadCluster.m_PosEnv = m_pQuads[QuadStart].m_PosEnv;
		QuadCluster.m_PosEnvOffset = m_pQuads[QuadStart].m_PosEnvOffset;

		int QuadOffset = 0;
		for(int QuadClusterId = 0; QuadClusterId < m_pLayerQuads->m_NumQuads - QuadStart; ++QuadClusterId)
		{
			const CQuad *pQuad = &m_pQuads[QuadStart + QuadClusterId];
			bool IsGrouped = QuadCluster.m_Grouped && pQuad->m_ColorEnv == QuadCluster.m_ColorEnv && pQuad->m_ColorEnvOffset == QuadCluster.m_ColorEnvOffset && pQuad->m_PosEnv == QuadCluster.m_PosEnv && pQuad->m_PosEnvOffset == QuadCluster.m_PosEnvOffset;

			// we are reaching gpu batch limit, here we break and close the QuadCluster if it's ungrouped
			if(QuadClusterId >= (int)GRAPHICS_MAX_QUADS_RENDER_COUNT)
			{
				// expand a cluster, if it's grouped
				if(!IsGrouped)
					break;
			}
			QuadOffset++;
			QuadCluster.m_Grouped = IsGrouped;
		}
		QuadCluster.m_NumQuads = QuadOffset;

		// fill cluster info
		if(QuadCluster.m_Grouped)
		{
			// grouped quads only need one render info, because all their envs and env offsets are equal
			QuadCluster.m_vQuadRenderInfo.resize(1);
			for(int QuadClusterId = 0; QuadClusterId < QuadCluster.m_NumQuads; ++QuadClusterId)
				SetQuadRenderInfo(QuadCluster.m_vQuadRenderInfo[0], QuadCluster.m_StartIndex + QuadClusterId, QuadClusterId == 0);
		}
		else
		{
			QuadCluster.m_vQuadRenderInfo.resize(QuadCluster.m_NumQuads);
			for(int QuadClusterId = 0; QuadClusterId < QuadCluster.m_NumQuads; ++QuadClusterId)
				SetQuadRenderInfo(QuadCluster.m_vQuadRenderInfo[QuadClusterId], QuadCluster.m_StartIndex + QuadClusterId, true);
		}

		CalculateClipping(QuadCluster);

		m_vQuadClusters.push_back(QuadCluster);
		QuadStart += QuadOffset;
	}

	// gpu upload
	size_t UploadDataSize = 0;
	if(Textured)
		UploadDataSize = vTmpQuadsTextured.size() * sizeof(CTmpQuadTextured);
	else
		UploadDataSize = vTmpQuads.size() * sizeof(CTmpQuad);

	if(UploadDataSize > 0)
	{
		void *pUploadData = nullptr;
		if(Textured)
			pUploadData = vTmpQuadsTextured.data();
		else
			pUploadData = vTmpQuads.data();
		// create the buffer object
		int BufferObjectIndex = Graphics()->CreateBufferObject(UploadDataSize, pUploadData, 0);
		// then create the buffer container
		SBufferContainerInfo ContainerInfo;
		ContainerInfo.m_Stride = (Textured ? (sizeof(CTmpQuadTextured) / 4) : (sizeof(CTmpQuad) / 4));
		ContainerInfo.m_VertBufferBindingIndex = BufferObjectIndex;
		ContainerInfo.m_vAttributes.emplace_back();
		SBufferContainerInfo::SAttribute *pAttr = &ContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 4;
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = nullptr;
		pAttr->m_FuncType = 0;
		ContainerInfo.m_vAttributes.emplace_back();
		pAttr = &ContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 4;
		pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
		pAttr->m_Normalized = true;
		pAttr->m_pOffset = (void *)(sizeof(float) * 4);
		pAttr->m_FuncType = 0;
		if(Textured)
		{
			ContainerInfo.m_vAttributes.emplace_back();
			pAttr = &ContainerInfo.m_vAttributes.back();
			pAttr->m_DataTypeCount = 2;
			pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
			pAttr->m_Normalized = false;
			pAttr->m_pOffset = (void *)(sizeof(float) * 4 + sizeof(unsigned char) * 4);
			pAttr->m_FuncType = 0;
		}

		pQLayerVisuals->m_BufferContainerIndex = Graphics()->CreateBufferContainer(&ContainerInfo);
		// and finally inform the backend how many indices are required
		Graphics()->IndicesNumRequiredNotify(m_pLayerQuads->m_NumQuads * 6);
	}
	RenderLoading();
}

void CRenderLayerQuads::Unload()
{
	if(m_VisualQuad.has_value())
	{
		m_VisualQuad->Unload();
		m_VisualQuad = std::nullopt;
	}
}

void CRenderLayerQuads::CQuadLayerVisuals::Unload()
{
	Graphics()->DeleteBufferContainer(m_BufferContainerIndex);
}

bool CRenderLayerQuads::CalculateQuadClipping(const CQuadCluster &QuadCluster, float aQuadOffsetMin[2], float aQuadOffsetMax[2]) const
{
	// check if the grouped clipping is available for early exit
	if(QuadCluster.m_Grouped)
	{
		const CEnvelopeExtrema::CEnvelopeExtremaItem &Extrema = m_pEnvelopeManager->EnvelopeExtrema()->GetExtrema(QuadCluster.m_PosEnv);
		if(!Extrema.m_Available)
			return false;
	}

	// calculate quad position offsets
	for(int Channel = 0; Channel < 2; ++Channel)
	{
		aQuadOffsetMin[Channel] = std::numeric_limits<float>::max(); // minimum of channel
		aQuadOffsetMax[Channel] = std::numeric_limits<float>::min(); // maximum of channel
	}

	for(int QuadId = QuadCluster.m_StartIndex; QuadId < QuadCluster.m_StartIndex + QuadCluster.m_NumQuads; ++QuadId)
	{
		const CQuad *pQuad = &m_pQuads[QuadId];

		const CEnvelopeExtrema::CEnvelopeExtremaItem &Extrema = m_pEnvelopeManager->EnvelopeExtrema()->GetExtrema(pQuad->m_PosEnv);
		if(!Extrema.m_Available)
			return false;

		// calculate clip region
		if(!Extrema.m_Rotating)
		{
			for(int QuadIdPoint = 0; QuadIdPoint < 4; ++QuadIdPoint)
			{
				for(int Channel = 0; Channel < 2; ++Channel)
				{
					float OffsetMinimum = fx2f(pQuad->m_aPoints[QuadIdPoint][Channel]);
					float OffsetMaximum = fx2f(pQuad->m_aPoints[QuadIdPoint][Channel]);

					// calculate env offsets for every ungrouped quad
					if(!QuadCluster.m_Grouped && pQuad->m_PosEnv >= 0)
					{
						OffsetMinimum += fx2f(Extrema.m_Minima[Channel]);
						OffsetMaximum += fx2f(Extrema.m_Maxima[Channel]);
					}
					aQuadOffsetMin[Channel] = std::min(aQuadOffsetMin[Channel], OffsetMinimum);
					aQuadOffsetMax[Channel] = std::max(aQuadOffsetMax[Channel], OffsetMaximum);
				}
			}
		}
		else
		{
			const CPoint &CenterFX = pQuad->m_aPoints[4];
			vec2 Center(fx2f(CenterFX.x), fx2f(CenterFX.y));
			float MaxDistance = 0;
			for(int QuadIdPoint = 0; QuadIdPoint < 4; ++QuadIdPoint)
			{
				const CPoint &QuadPointFX = pQuad->m_aPoints[QuadIdPoint];
				vec2 QuadPoint(fx2f(QuadPointFX.x), fx2f(QuadPointFX.y));
				float Distance = length(Center - QuadPoint);
				MaxDistance = std::max(Distance, MaxDistance);
			}

			for(int Channel = 0; Channel < 2; ++Channel)
			{
				float OffsetMinimum = Center[Channel] - MaxDistance;
				float OffsetMaximum = Center[Channel] + MaxDistance;
				if(!QuadCluster.m_Grouped && pQuad->m_PosEnv >= 0)
				{
					OffsetMinimum += fx2f(Extrema.m_Minima[Channel]);
					OffsetMaximum += fx2f(Extrema.m_Maxima[Channel]);
				}
				aQuadOffsetMin[Channel] = std::min(aQuadOffsetMin[Channel], OffsetMinimum);
				aQuadOffsetMax[Channel] = std::max(aQuadOffsetMax[Channel], OffsetMaximum);
			}
		}
	}

	// add env offsets for the quad group
	if(QuadCluster.m_Grouped && QuadCluster.m_PosEnv >= 0)
	{
		const CEnvelopeExtrema::CEnvelopeExtremaItem &Extrema = m_pEnvelopeManager->EnvelopeExtrema()->GetExtrema(QuadCluster.m_PosEnv);

		for(int Channel = 0; Channel < 2; ++Channel)
		{
			aQuadOffsetMin[Channel] += fx2f(Extrema.m_Minima[Channel]);
			aQuadOffsetMax[Channel] += fx2f(Extrema.m_Maxima[Channel]);
		}
	}
	return true;
}

void CRenderLayerQuads::CalculateClipping(CQuadCluster &QuadCluster)
{
	float aQuadOffsetMin[2];
	float aQuadOffsetMax[2];

	bool CreateClip = CalculateQuadClipping(QuadCluster, aQuadOffsetMin, aQuadOffsetMax);

	if(!CreateClip)
		return;

	QuadCluster.m_ClipRegion = std::make_optional<CClipRegion>();
	std::optional<CClipRegion> &ClipRegion = QuadCluster.m_ClipRegion;

	// X channel
	ClipRegion->m_X = aQuadOffsetMin[0];
	ClipRegion->m_Width = aQuadOffsetMax[0] - aQuadOffsetMin[0];

	// Y channel
	ClipRegion->m_Y = aQuadOffsetMin[1];
	ClipRegion->m_Height = aQuadOffsetMax[1] - aQuadOffsetMin[1];

	// update layer clip
	if(!m_LayerClip.has_value())
	{
		m_LayerClip = ClipRegion;
	}
	else
	{
		float ClipRight = std::max(ClipRegion->m_X + ClipRegion->m_Width, m_LayerClip->m_X + m_LayerClip->m_Width);
		float ClipBottom = std::max(ClipRegion->m_Y + ClipRegion->m_Height, m_LayerClip->m_Y + m_LayerClip->m_Height);
		m_LayerClip->m_X = std::min(ClipRegion->m_X, m_LayerClip->m_X);
		m_LayerClip->m_Y = std::min(ClipRegion->m_Y, m_LayerClip->m_Y);
		m_LayerClip->m_Width = ClipRight - m_LayerClip->m_X;
		m_LayerClip->m_Height = ClipBottom - m_LayerClip->m_Y;
	}
}

void CRenderLayerQuads::Render(const CRenderLayerParams &Params)
{
	UseTexture(GetTexture());

	bool Force = Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE || Params.m_RenderType == ERenderType::RENDERTYPE_FULL_DESIGN;
	float Alpha = Force ? 1.f : (100 - Params.m_EntityOverlayVal) / 100.0f;
	if(!Graphics()->IsQuadBufferingEnabled() || !Params.m_TileAndQuadBuffering)
	{
		Graphics()->BlendNormal();
		RenderMap()->ForceRenderQuads(m_pQuads, m_pLayerQuads->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, m_pEnvelopeManager->EnvelopeEval(), Alpha);
	}
	else
	{
		RenderQuadLayer(Alpha, Params);
	}

	if(Params.m_DebugRenderQuadClips && m_LayerClip.has_value())
	{
		char aDebugText[64];
		str_format(aDebugText, sizeof(aDebugText), "Group %d, quad layer %d", m_GroupId, m_LayerId);
		RenderMap()->RenderDebugClip(m_LayerClip->m_X, m_LayerClip->m_Y, m_LayerClip->m_Width, m_LayerClip->m_Height, ColorRGBA(1.0f, 0.0f, 0.5f, 1.0f), Params.m_Zoom, aDebugText);
	}
}

bool CRenderLayerQuads::DoRender(const CRenderLayerParams &Params)
{
	// skip rendering anything but entities if we only want to render entities
	if(Params.m_EntityOverlayVal == 100 && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering if detail layers if not wanted
	if(m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && Params.m_RenderType != ERenderType::RENDERTYPE_FULL_DESIGN) // detail but no details
		return false;

	// this option only deactivates quads in the background
	if(Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND || Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE)
	{
		if(!g_Config.m_ClShowQuads)
			return false;
	}

	return IsVisibleInClipRegion(m_LayerClip);
}

/****************
 * Entity Layer *
 ****************/
// BASE
CRenderLayerEntityBase::CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerTile(GroupId, LayerId, Flags, pLayerTilemap) {}

bool CRenderLayerEntityBase::DoRender(const CRenderLayerParams &Params)
{
	// skip rendering if we render background force or full design
	if(Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE || Params.m_RenderType == ERenderType::RENDERTYPE_FULL_DESIGN)
		return false;

	// skip rendering of entities if don't want them
	if(!Params.m_EntityOverlayVal)
		return false;

	return true;
}

IGraphics::CTextureHandle CRenderLayerEntityBase::GetTexture() const
{
	return m_pMapImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH);
}

// GAME
CRenderLayerEntityGame::CRenderLayerEntityGame(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

void CRenderLayerEntityGame::Init()
{
	UploadTileData(m_VisualTiles, 0, false, true);
}

void CRenderLayerEntityGame::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	if(Params.m_RenderTileBorder)
		RenderKillTileBorder(Color.Multiply(GetDeathBorderColor()));
	RenderTileLayer(Color, Params);
}

void CRenderLayerEntityGame::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();

	if(Params.m_RenderTileBorder)
	{
		RenderMap()->RenderTileRectangle(-BorderRenderDistance, -BorderRenderDistance, m_pLayerTilemap->m_Width + 2 * BorderRenderDistance, m_pLayerTilemap->m_Height + 2 * BorderRenderDistance,
			TILE_AIR, TILE_DEATH, // display air inside, death outside
			32.0f, Color.Multiply(GetDeathBorderColor()), TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}

	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}

ColorRGBA CRenderLayerEntityGame::GetDeathBorderColor() const
{
	// draw kill tiles outside the entity clipping rectangle
	// slow blinking to hint that it's not a part of the map
	float Seconds = time_get() / (float)time_freq();
	float Alpha = 0.3f + 0.35f * (1.f + std::sin(2.f * pi * Seconds / 3.f));
	return ColorRGBA(1.f, 1.f, 1.f, Alpha);
}

// FRONT
CRenderLayerEntityFront::CRenderLayerEntityFront(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

int CRenderLayerEntityFront::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTile);
	return m_pLayerTilemap->m_Front;
}

// TELE
CRenderLayerEntityTele::CRenderLayerEntityTele(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

int CRenderLayerEntityTele::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTeleTile);
	return m_pLayerTilemap->m_Tele;
}

void CRenderLayerEntityTele::Init()
{
	UploadTileData(m_VisualTiles, 0, false);
	UploadTileData(m_VisualTeleNumbers, 1, false);
}

void CRenderLayerEntityTele::InitTileData()
{
	m_pTeleTiles = GetData<CTeleTile>();
}

void CRenderLayerEntityTele::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualTeleNumbers.has_value())
	{
		m_VisualTeleNumbers->Unload();
		m_VisualTeleNumbers = std::nullopt;
	}
}

void CRenderLayerEntityTele::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayCenter());
		RenderTileLayer(Color, Params, &m_VisualTeleNumbers.value());
	}
}

void CRenderLayerEntityTele::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTelemap(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTelemap(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderTeleOverlay(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

void CRenderLayerEntityTele::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTeleTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
	if(CurOverlay == 1)
	{
		if(IsTeleTileNumberUsedAny(*pIndex))
			*pIndex = m_pTeleTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
		else
			*pIndex = 0;
	}
}

// SPEEDUP
CRenderLayerEntitySpeedup::CRenderLayerEntitySpeedup(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

IGraphics::CTextureHandle CRenderLayerEntitySpeedup::GetTexture() const
{
	return m_pMapImages->GetSpeedupArrow();
}

int CRenderLayerEntitySpeedup::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CSpeedupTile);
	return m_pLayerTilemap->m_Speedup;
}

void CRenderLayerEntitySpeedup::Init()
{
	UploadTileData(m_VisualTiles, 0, true);
	UploadTileData(m_VisualForce, 1, false);
	UploadTileData(m_VisualMaxSpeed, 2, false);
}

void CRenderLayerEntitySpeedup::InitTileData()
{
	m_pSpeedupTiles = GetData<CSpeedupTile>();
}

void CRenderLayerEntitySpeedup::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualForce.has_value())
	{
		m_VisualForce->Unload();
		m_VisualForce = std::nullopt;
	}
	if(m_VisualMaxSpeed.has_value())
	{
		m_VisualMaxSpeed->Unload();
		m_VisualMaxSpeed = std::nullopt;
	}
}

void CRenderLayerEntitySpeedup::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	unsigned char Force = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Force;
	unsigned char MaxSpeed = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_MaxSpeed;
	*pFlags = 0;
	*pAngleRotate = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Angle;
	if((Force == 0 && *pIndex == TILE_SPEED_BOOST_OLD) || (Force == 0 && MaxSpeed == 0 && *pIndex == TILE_SPEED_BOOST) || !IsValidSpeedupTile(*pIndex))
		*pIndex = 0;
	else if(CurOverlay == 1)
		*pIndex = Force;
	else if(CurOverlay == 2)
		*pIndex = MaxSpeed;
}

void CRenderLayerEntitySpeedup::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	// draw arrow -- clamp to the edge of the arrow image
	Graphics()->WrapClamp();
	UseTexture(GetTexture());
	RenderTileLayer(Color, Params);
	Graphics()->WrapNormal();

	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, Params, &m_VisualForce.value());
		Graphics()->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, Params, &m_VisualMaxSpeed.value());
	}
}

void CRenderLayerEntitySpeedup::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderSpeedupOverlay(m_pSpeedupTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

// SWITCH
CRenderLayerEntitySwitch::CRenderLayerEntitySwitch(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

IGraphics::CTextureHandle CRenderLayerEntitySwitch::GetTexture() const
{
	return m_pMapImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH);
}

int CRenderLayerEntitySwitch::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CSwitchTile);
	return m_pLayerTilemap->m_Switch;
}

void CRenderLayerEntitySwitch::Init()
{
	UploadTileData(m_VisualTiles, 0, false);
	UploadTileData(m_VisualSwitchNumberTop, 1, false);
	UploadTileData(m_VisualSwitchNumberBottom, 2, false);
}

void CRenderLayerEntitySwitch::InitTileData()
{
	m_pSwitchTiles = GetData<CSwitchTile>();
}

void CRenderLayerEntitySwitch::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualSwitchNumberTop.has_value())
	{
		m_VisualSwitchNumberTop->Unload();
		m_VisualSwitchNumberTop = std::nullopt;
	}
	if(m_VisualSwitchNumberBottom.has_value())
	{
		m_VisualSwitchNumberBottom->Unload();
		m_VisualSwitchNumberBottom = std::nullopt;
	}
}

void CRenderLayerEntitySwitch::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pFlags = 0;
	*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	if(CurOverlay == 0)
	{
		*pFlags = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
		if(*pIndex == TILE_SWITCHTIMEDOPEN)
			*pIndex = 8;
	}
	else if(CurOverlay == 1)
		*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
	else if(CurOverlay == 2)
		*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Delay;
}

void CRenderLayerEntitySwitch::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, Params, &m_VisualSwitchNumberTop.value());
		Graphics()->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, Params, &m_VisualSwitchNumberBottom.value());
	}
}

void CRenderLayerEntitySwitch::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderSwitchmap(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderSwitchmap(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderSwitchOverlay(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

// TUNE
CRenderLayerEntityTune::CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

void CRenderLayerEntityTune::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTuneTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
}

int CRenderLayerEntityTune::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTuneTile);
	return m_pLayerTilemap->m_Tune;
}

void CRenderLayerEntityTune::InitTileData()
{
	m_pTuneTiles = GetData<CTuneTile>();
}

void CRenderLayerEntityTune::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTunemap(m_pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTunemap(m_pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}
