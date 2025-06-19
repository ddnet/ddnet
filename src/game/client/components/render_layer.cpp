#include "render_layer.h"
#include "maplayers.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <chrono>

/************************
 * Render Buffer Helper *
 ************************/
static void FillTmpTile(SGraphicTile *pTmpTile, SGraphicTileTexureCoords *pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, const ivec2 &Offset, int Scale)
{
	if(pTmpTex)
	{
		unsigned char aTexX[]{0, 1, 1, 0};
		unsigned char aTexY[]{0, 0, 1, 1};

		if(Flags & TILEFLAG_XFLIP)
			std::rotate(std::begin(aTexX), std::begin(aTexX) + 2, std::end(aTexX));

		if(Flags & TILEFLAG_YFLIP)
			std::rotate(std::begin(aTexY), std::begin(aTexY) + 2, std::end(aTexY));

		if(Flags & TILEFLAG_ROTATE)
		{
			std::rotate(std::begin(aTexX), std::begin(aTexX) + 3, std::end(aTexX));
			std::rotate(std::begin(aTexY), std::begin(aTexY) + 3, std::end(aTexY));
		}

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

static void FillTmpTileSpeedup(SGraphicTile *pTmpTile, SGraphicTileTexureCoords *pTmpTex, unsigned char Flags, int x, int y, const ivec2 &Offset, int Scale, short AngleRotate)
{
	int Angle = AngleRotate % 360;
	FillTmpTile(pTmpTile, pTmpTex, Angle >= 270 ? ROTATION_270 : (Angle >= 180 ? ROTATION_180 : (Angle >= 90 ? ROTATION_90 : 0)), AngleRotate % 90, x, y, Offset, Scale);
}

static bool AddTile(std::vector<SGraphicTile> &vTmpTiles, std::vector<SGraphicTileTexureCoords> &vTmpTileTexCoords, unsigned char Index, unsigned char Flags, int x, int y, bool DoTextureCoords, bool FillSpeedup = false, int AngleRotate = -1, const ivec2 &Offset = ivec2{0, 0}, int Scale = 32)
{
	if(Index <= 0)
		return false;

	vTmpTiles.emplace_back();
	SGraphicTile &Tile = vTmpTiles.back();
	SGraphicTileTexureCoords *pTileTex = nullptr;
	if(DoTextureCoords)
	{
		vTmpTileTexCoords.emplace_back();
		SGraphicTileTexureCoords &TileTex = vTmpTileTexCoords.back();
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

void CRenderLayer::EnvelopeEvalRenderLayer(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser)
{
	CRenderLayer *pRenderLayer = (CRenderLayer *)pUser;
	CMapLayers::EnvelopeEval(TimeOffsetMillis, Env, Result, Channels, pRenderLayer->m_pMap, pRenderLayer->m_pEnvelopePoints.get(), pRenderLayer->m_pClient, pRenderLayer->m_pGameClient, pRenderLayer->m_OnlineOnly);
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

	m_vBorderTop.resize(Width);
	m_vBorderBottom.resize(Width);

	m_vBorderLeft.resize(Height);
	m_vBorderRight.resize(Height);
	return true;
}

/*************
* Base Layer *
**************/

CRenderLayer::CRenderLayer(int GroupId, int LayerId, int Flags) :
	m_GroupId(GroupId), m_LayerId(LayerId), m_Flags(Flags) {}

void CRenderLayer::OnInit(IGraphics *pGraphics, IMap *pMap, CRenderTools *pRenderTools, CMapImages *pMapImages, std::shared_ptr<CMapBasedEnvelopePointAccess> &pEvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly)
{
	m_pGraphics = pGraphics;
	m_pMap = pMap;
	m_pRenderTools = pRenderTools;
	m_pMapImages = pMapImages;
	m_pClient = pClient;
	m_pGameClient = pGameClient;
	m_pEnvelopePoints = pEvelopePoints;
	m_OnlineOnly = OnlineOnly;
}

void CRenderLayer::UseTexture(IGraphics::CTextureHandle TextureHandle) const
{
	if(TextureHandle.IsValid())
		m_pGraphics->TextureSet(TextureHandle);
	else
		m_pGraphics->TextureClear();
}

void CRenderLayer::RenderLoading() const
{
	const char *pLoadingTitle = Localize("Loading map");
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	m_pGameClient->m_Menus.RenderLoading(pLoadingTitle, pLoadingMessage, 0);
}

/**************
 * Tile Layer *
 **************/

CRenderLayerTile::CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayer(GroupId, LayerId, Flags)
{
	m_pLayerTilemap = pLayerTilemap;
	m_Color = ColorRGBA(m_pLayerTilemap->m_Color.r / 255.0f, m_pLayerTilemap->m_Color.g / 255.0f, m_pLayerTilemap->m_Color.b / 255.0f, pLayerTilemap->m_Color.a / 255.0f);
}

void CRenderLayerTile::RenderTileLayer(const ColorRGBA &Color, CTileLayerVisuals *pTileLayerVisuals)
{
	CTileLayerVisuals &Visuals = pTileLayerVisuals ? *pTileLayerVisuals : m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	m_pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int ScreenRectY0 = std::floor(ScreenY0 / 32);
	int ScreenRectX0 = std::floor(ScreenX0 / 32);
	int ScreenRectY1 = std::ceil(ScreenY1 / 32);
	int ScreenRectX1 = std::ceil(ScreenX1 / 32);

	if(ScreenRectX1 > 0 && ScreenRectY1 > 0 && ScreenRectX0 < (int)Visuals.m_Width && ScreenRectY0 < (int)Visuals.m_Height)
	{
		// create the indice buffers we want to draw -- reuse them
		std::vector<char *> vpIndexOffsets;
		std::vector<unsigned int> vDrawCounts;

		int X0 = std::max(ScreenRectX0, 0);
		int X1 = std::min(ScreenRectX1, (int)Visuals.m_Width);
		if(X0 <= X1)
		{
			int Y0 = std::max(ScreenRectY0, 0);
			int Y1 = std::min(ScreenRectY1, (int)Visuals.m_Height);

			unsigned long long Reserve = absolute(Y1 - Y0) + 1;
			vpIndexOffsets.reserve(Reserve);
			vDrawCounts.reserve(Reserve);

			for(int y = Y0; y < Y1; ++y)
			{
				int XR = X1 - 1;

				dbg_assert(Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() >= Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset(), "Tile count wrong.");

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
				m_pGraphics->RenderTileLayer(Visuals.m_BufferContainerIndex, Color, vpIndexOffsets.data(), vDrawCounts.data(), DrawCount);
			}
		}
	}

	if(ScreenRectX1 > (int)Visuals.m_Width || ScreenRectY1 > (int)Visuals.m_Height || ScreenRectX0 < 0 || ScreenRectY0 < 0)
	{
		RenderTileBorder(Color, ScreenRectX0, ScreenRectY0, ScreenRectX1, ScreenRectY1);
	}
}

void CRenderLayerTile::RenderTileBorder(const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1)
{
	CTileLayerVisuals &Visuals = m_VisualTiles.value();

	int Y0 = std::max(0, BorderY0);
	int X0 = std::max(0, BorderX0);
	int Y1 = std::min((int)Visuals.m_Height, BorderY1);
	int X1 = std::min((int)Visuals.m_Width, BorderX1);

	// corners
	auto DrawCorner = [&](vec2 Offset, vec2 Scale, CTileLayerVisuals::CTileVisual &Visual) {
		Offset *= 32.0f;
		m_pGraphics->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, (offset_ptr_size)Visual.IndexBufferByteOffset(), Offset, Scale, 1);
	};

	if(BorderX0 < 0)
	{
		// Draw corners on left side
		if(BorderY0 < 0 && Visuals.m_BorderTopLeft.DoDraw())
		{
			DrawCorner(
				vec2(0, 0),
				vec2(std::abs(BorderX0), std::abs(BorderY0)),
				Visuals.m_BorderTopLeft);
		}
		if(BorderY1 > (int)Visuals.m_Height && Visuals.m_BorderBottomLeft.DoDraw())
		{
			DrawCorner(
				vec2(0, Visuals.m_Height),
				vec2(std::abs(BorderX0), BorderY1 - Visuals.m_Height),
				Visuals.m_BorderBottomLeft);
		}
	}
	if(BorderX1 > (int)Visuals.m_Width)
	{
		// Draw corners on right side
		if(BorderY0 < 0 && Visuals.m_BorderTopRight.DoDraw())
		{
			DrawCorner(
				vec2(Visuals.m_Width, 0),
				vec2(BorderX1 - Visuals.m_Width, std::abs(BorderY0)),
				Visuals.m_BorderTopRight);
		}
		if(BorderY1 > (int)Visuals.m_Height && Visuals.m_BorderBottomRight.DoDraw())
		{
			DrawCorner(
				vec2(Visuals.m_Width, Visuals.m_Height),
				vec2(BorderX1 - Visuals.m_Width, BorderY1 - Visuals.m_Height),
				Visuals.m_BorderBottomRight);
		}
	}

	// borders
	auto DrawBorder = [&](vec2 Offset, vec2 Scale, CTileLayerVisuals::CTileVisual &StartVisual, CTileLayerVisuals::CTileVisual &EndVisual) {
		unsigned int DrawNum = ((EndVisual.IndexBufferByteOffset() - StartVisual.IndexBufferByteOffset()) / (sizeof(unsigned int) * 6)) + (EndVisual.DoDraw() ? 1lu : 0lu);
		offset_ptr_size pOffset = (offset_ptr_size)StartVisual.IndexBufferByteOffset();
		Offset *= 32.0f;
		m_pGraphics->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, DrawNum);
	};

	if(Y0 < (int)Visuals.m_Height && Y1 > 0)
	{
		if(BorderX1 > (int)Visuals.m_Width)
		{
			// Draw right border
			DrawBorder(
				vec2(Visuals.m_Width, 0),
				vec2(BorderX1 - Visuals.m_Width, 1.f),
				Visuals.m_vBorderRight[Y0], Visuals.m_vBorderRight[Y1 - 1]);
		}
		if(BorderX0 < 0)
		{
			// Draw left border
			DrawBorder(
				vec2(0, 0),
				vec2(std::abs(BorderX0), 1),
				Visuals.m_vBorderLeft[Y0], Visuals.m_vBorderLeft[Y1 - 1]);
		}
	}

	if(X0 < (int)Visuals.m_Width && X1 > 0)
	{
		if(BorderY0 < 0)
		{
			// Draw top border
			DrawBorder(
				vec2(0, 0),
				vec2(1, std::abs(BorderY0)),
				Visuals.m_vBorderTop[X0], Visuals.m_vBorderTop[X1 - 1]);
		}
		if(BorderY1 > (int)Visuals.m_Height)
		{
			// Draw bottom border
			DrawBorder(
				vec2(0, Visuals.m_Height),
				vec2(1, BorderY1 - Visuals.m_Height),
				Visuals.m_vBorderBottom[X0], Visuals.m_vBorderBottom[X1 - 1]);
		}
	}
}

void CRenderLayerTile::RenderKillTileBorder(const ColorRGBA &Color)
{
	CTileLayerVisuals &Visuals = m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	m_pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

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
		m_pGraphics->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, 1);
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
	if(Params.EntityOverlayVal && Params.m_RenderType != CMapLayers::TYPE_BACKGROUND_FORCE)
		Color.a *= (100 - Params.EntityOverlayVal) / 100.0f;

	ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CMapLayers::EnvelopeEval(m_pLayerTilemap->m_ColorEnvOffset, m_pLayerTilemap->m_ColorEnv, ColorEnv, 4, m_pMap, m_pEnvelopePoints.get(), m_pClient, m_pGameClient, m_OnlineOnly);
	Color = Color.Multiply(ColorEnv);
	return Color;
}

void CRenderLayerTile::Render(const CRenderLayerParams &Params)
{
	if(!DoRender(Params))
		return;

	UseTexture(GetTexture());
	ColorRGBA Color = GetRenderColor(Params);
	if(m_pGraphics->IsTileBufferingEnabled())
	{
		RenderTileLayerWithTileBuffer(Color);
	}
	else
	{
		RenderTileLayerNoTileBuffer(Color);
	}
}

bool CRenderLayerTile::DoRender(const CRenderLayerParams &Params) const
{
	// skip rendering if we render background force, but deactivated tile layer and want to render a tilelayer
	if(!g_Config.m_ClBackgroundShowTilesLayers && Params.m_RenderType == CMapLayers::TYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering anything but entities if we only want to render entities
	if(Params.EntityOverlayVal == 100 && Params.m_RenderType != CMapLayers::TYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering if detail layers if not wanted
	if(m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && Params.m_RenderType != CMapLayers::TYPE_FULL_DESIGN) // detail but no details
		return false;
	return true;
}

void CRenderLayerTile::RenderTileLayerWithTileBuffer(const ColorRGBA &Color)
{
	m_pGraphics->BlendNormal();
	RenderTileLayer(Color);
}

void CRenderLayerTile::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CTile *pTiles = GetData<CTile>();
	m_pGraphics->BlendNone();
	m_pRenderTools->RenderTilemap(pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
	m_pGraphics->BlendNormal();
	m_pRenderTools->RenderTilemap(pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
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
	if(!m_pGraphics->IsTileBufferingEnabled())
		return;

	// prepare all visuals for all tile layers
	std::vector<SGraphicTile> vTmpTiles;
	std::vector<SGraphicTileTexureCoords> vTmpTileTexCoords;
	std::vector<SGraphicTile> vTmpBorderTopTiles;
	std::vector<SGraphicTileTexureCoords> vTmpBorderTopTilesTexCoords;
	std::vector<SGraphicTile> vTmpBorderLeftTiles;
	std::vector<SGraphicTileTexureCoords> vTmpBorderLeftTilesTexCoords;
	std::vector<SGraphicTile> vTmpBorderRightTiles;
	std::vector<SGraphicTileTexureCoords> vTmpBorderRightTilesTexCoords;
	std::vector<SGraphicTile> vTmpBorderBottomTiles;
	std::vector<SGraphicTileTexureCoords> vTmpBorderBottomTilesTexCoords;
	std::vector<SGraphicTile> vTmpBorderCorners;
	std::vector<SGraphicTileTexureCoords> vTmpBorderCornersTexCoords;

	const bool DoTextureCoords = GetTexture().IsValid();

	// create the visual and set it in the optional, afterwards get it
	CTileLayerVisuals v;
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
				Visuals.m_vTilesOfLayer[y * m_pLayerTilemap->m_Width + x].Draw(true);

			// do the border tiles
			if(x == 0)
			{
				if(y == 0)
				{
					Visuals.m_BorderTopLeft.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, -32}))
						Visuals.m_BorderTopLeft.Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					Visuals.m_BorderBottomLeft.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
						Visuals.m_BorderBottomLeft.Draw(true);
				}
				Visuals.m_vBorderLeft[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderLeftTiles.size()));
				if(AddTile(vTmpBorderLeftTiles, vTmpBorderLeftTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
					Visuals.m_vBorderLeft[y].Draw(true);
			}
			else if(x == m_pLayerTilemap->m_Width - 1)
			{
				if(y == 0)
				{
					Visuals.m_BorderTopRight.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
						Visuals.m_BorderTopRight.Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					Visuals.m_BorderBottomRight.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
						Visuals.m_BorderBottomRight.Draw(true);
				}
				Visuals.m_vBorderRight[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderRightTiles.size()));
				if(AddTile(vTmpBorderRightTiles, vTmpBorderRightTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					Visuals.m_vBorderRight[y].Draw(true);
			}
			if(y == 0)
			{
				Visuals.m_vBorderTop[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderTopTiles.size()));
				if(AddTile(vTmpBorderTopTiles, vTmpBorderTopTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
					Visuals.m_vBorderTop[x].Draw(true);
			}
			else if(y == m_pLayerTilemap->m_Height - 1)
			{
				Visuals.m_vBorderBottom[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderBottomTiles.size()));
				if(AddTile(vTmpBorderBottomTiles, vTmpBorderBottomTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					Visuals.m_vBorderBottom[x].Draw(true);
			}
		}
	}

	// append one kill tile to the gamelayer
	if(IsGameLayer)
	{
		Visuals.m_BorderKillTile.SetIndexBufferByteOffset((offset_ptr32)(vTmpTiles.size()));
		if(AddTile(vTmpTiles, vTmpTileTexCoords, TILE_DEATH, 0, 0, 0, DoTextureCoords))
			Visuals.m_BorderKillTile.Draw(true);
	}

	// add the border corners, then the borders and fix their byte offsets
	int TilesHandledCount = vTmpTiles.size();
	Visuals.m_BorderTopLeft.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderTopRight.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderBottomLeft.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderBottomRight.AddIndexBufferByteOffset(TilesHandledCount);
	// add the Corners to the tiles
	vTmpTiles.insert(vTmpTiles.end(), vTmpBorderCorners.begin(), vTmpBorderCorners.end());
	vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTmpBorderCornersTexCoords.begin(), vTmpBorderCornersTexCoords.end());

	// now the borders
	TilesHandledCount = vTmpTiles.size();
	if(m_pLayerTilemap->m_Width > 0)
	{
		for(int i = 0; i < m_pLayerTilemap->m_Width; ++i)
		{
			Visuals.m_vBorderTop[i].AddIndexBufferByteOffset(TilesHandledCount);
		}
	}
	vTmpTiles.insert(vTmpTiles.end(), vTmpBorderTopTiles.begin(), vTmpBorderTopTiles.end());
	vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTmpBorderTopTilesTexCoords.begin(), vTmpBorderTopTilesTexCoords.end());

	TilesHandledCount = vTmpTiles.size();
	if(m_pLayerTilemap->m_Width > 0)
	{
		for(int i = 0; i < m_pLayerTilemap->m_Width; ++i)
		{
			Visuals.m_vBorderBottom[i].AddIndexBufferByteOffset(TilesHandledCount);
		}
	}
	vTmpTiles.insert(vTmpTiles.end(), vTmpBorderBottomTiles.begin(), vTmpBorderBottomTiles.end());
	vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTmpBorderBottomTilesTexCoords.begin(), vTmpBorderBottomTilesTexCoords.end());

	TilesHandledCount = vTmpTiles.size();
	if(m_pLayerTilemap->m_Height > 0)
	{
		for(int i = 0; i < m_pLayerTilemap->m_Height; ++i)
		{
			Visuals.m_vBorderLeft[i].AddIndexBufferByteOffset(TilesHandledCount);
		}
	}
	vTmpTiles.insert(vTmpTiles.end(), vTmpBorderLeftTiles.begin(), vTmpBorderLeftTiles.end());
	vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTmpBorderLeftTilesTexCoords.begin(), vTmpBorderLeftTilesTexCoords.end());

	TilesHandledCount = vTmpTiles.size();
	if(m_pLayerTilemap->m_Height > 0)
	{
		for(int i = 0; i < m_pLayerTilemap->m_Height; ++i)
		{
			Visuals.m_vBorderRight[i].AddIndexBufferByteOffset(TilesHandledCount);
		}
	}
	vTmpTiles.insert(vTmpTiles.end(), vTmpBorderRightTiles.begin(), vTmpBorderRightTiles.end());
	vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTmpBorderRightTilesTexCoords.begin(), vTmpBorderRightTilesTexCoords.end());

	// setup params
	float *pTmpTiles = vTmpTiles.empty() ? nullptr : (float *)vTmpTiles.data();
	unsigned char *pTmpTileTexCoords = vTmpTileTexCoords.empty() ? nullptr : (unsigned char *)vTmpTileTexCoords.data();

	Visuals.m_BufferContainerIndex = -1;
	size_t UploadDataSize = vTmpTileTexCoords.size() * sizeof(SGraphicTileTexureCoords) + vTmpTiles.size() * sizeof(SGraphicTile);
	if(UploadDataSize > 0)
	{
		char *pUploadData = (char *)malloc(sizeof(char) * UploadDataSize);

		mem_copy_special(pUploadData, pTmpTiles, sizeof(vec2), vTmpTiles.size() * 4, (DoTextureCoords ? sizeof(ubvec4) : 0));
		if(DoTextureCoords)
		{
			mem_copy_special(pUploadData + sizeof(vec2), pTmpTileTexCoords, sizeof(ubvec4), vTmpTiles.size() * 4, sizeof(vec2));
		}

		// first create the buffer object
		int BufferObjectIndex = m_pGraphics->CreateBufferObject(UploadDataSize, pUploadData, 0, true);

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

		Visuals.m_BufferContainerIndex = m_pGraphics->CreateBufferContainer(&ContainerInfo);
		// and finally inform the backend how many indices are required
		m_pGraphics->IndicesNumRequiredNotify(vTmpTiles.size() * 6);
	}
	RenderLoading();
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

template<class T>
T *CRenderLayerTile::GetData() const
{
	return (T *)GetRawData();
}

void CRenderLayerTile::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	CTile *pTiles = GetData<CTile>();
	*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Index;
	*pFlags = pTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
}

/**************
 * Quad Layer *
 **************/

CRenderLayerQuads::CRenderLayerQuads(int GroupId, int LayerId, IGraphics::CTextureHandle TextureHandle, int Flags, CMapItemLayerQuads *pLayerQuads) :
	CRenderLayer(GroupId, LayerId, Flags)
{
	m_pLayerQuads = pLayerQuads;
	m_Grouped = false;
}

void CRenderLayerQuads::RenderQuadLayer(bool Force)
{
	CQuadLayerVisuals &Visuals = m_VisualQuad.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	if(!Force && (!g_Config.m_ClShowQuads || g_Config.m_ClOverlayEntities == 100))
		return;

	CQuad *pQuads = (CQuad *)m_pMap->GetDataSwapped(m_pLayerQuads->m_Data);

	size_t QuadsRenderCount = 0;
	size_t CurQuadOffset = 0;
	if(!m_Grouped)
	{
		for(int i = 0; i < m_pLayerQuads->m_NumQuads; ++i)
		{
			CQuad *pQuad = &pQuads[i];

			ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			CMapLayers::EnvelopeEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, 4, m_pMap, m_pEnvelopePoints.get(), m_pClient, m_pGameClient, m_OnlineOnly);

			const bool IsFullyTransparent = Color.a <= 0.0f;
			const bool NeedsFlush = QuadsRenderCount == gs_GraphicsMaxQuadsRenderCount || IsFullyTransparent;

			if(NeedsFlush)
			{
				// render quads of the current offset directly(cancel batching)
				m_pGraphics->RenderQuadLayer(Visuals.m_BufferContainerIndex, m_vQuadRenderInfo.data(), QuadsRenderCount, CurQuadOffset);
				QuadsRenderCount = 0;
				CurQuadOffset = i;
				if(IsFullyTransparent)
				{
					// since this quad is ignored, the offset is the next quad
					++CurQuadOffset;
				}
			}

			if(!IsFullyTransparent)
			{
				ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				CMapLayers::EnvelopeEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3, m_pMap, m_pEnvelopePoints.get(), m_pClient, m_pGameClient, m_OnlineOnly);

				SQuadRenderInfo &QInfo = m_vQuadRenderInfo[QuadsRenderCount++];
				QInfo.m_Color = Color;
				QInfo.m_Offsets.x = Position.r;
				QInfo.m_Offsets.y = Position.g;
				QInfo.m_Rotation = Position.b / 180.0f * pi;
			}
		}
		m_pGraphics->RenderQuadLayer(Visuals.m_BufferContainerIndex, m_vQuadRenderInfo.data(), QuadsRenderCount, CurQuadOffset);
	}
	else
	{
		SQuadRenderInfo &QInfo = m_vQuadRenderInfo[0];

		if(m_QuadRenderGroup.m_ColorEnv >= 0)
		{
			ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			CMapLayers::EnvelopeEval(m_QuadRenderGroup.m_ColorEnvOffset, m_QuadRenderGroup.m_ColorEnv, Color, 4, m_pMap, m_pEnvelopePoints.get(), m_pClient, m_pGameClient, m_OnlineOnly);

			if(Color.a <= 0.0f)
				return;
			QInfo.m_Color = Color;
		}

		if(m_QuadRenderGroup.m_PosEnv >= 0)
		{
			ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
			CMapLayers::EnvelopeEval(m_QuadRenderGroup.m_PosEnvOffset, m_QuadRenderGroup.m_PosEnv, Position, 3, m_pMap, m_pEnvelopePoints.get(), m_pClient, m_pGameClient, m_OnlineOnly);

			QInfo.m_Offsets.x = Position.r;
			QInfo.m_Offsets.y = Position.g;
			QInfo.m_Rotation = Position.b / 180.0f * pi;
		}
		m_pGraphics->RenderQuadLayer(Visuals.m_BufferContainerIndex, &QInfo, (size_t)m_pLayerQuads->m_NumQuads, 0, true);
	}
}

void CRenderLayerQuads::Init()
{
	if(m_pLayerQuads->m_Image >= 0 && m_pLayerQuads->m_Image < m_pMapImages->Num())
		m_TextureHandle = m_pMapImages->Get(m_pLayerQuads->m_Image);
	else
		m_TextureHandle.Invalidate();

	if(!m_pGraphics->IsQuadBufferingEnabled())
		return;

	std::vector<CTmpQuad> vTmpQuads;
	std::vector<CTmpQuadTextured> vTmpQuadsTextured;
	CQuadLayerVisuals v;
	m_VisualQuad = v;
	CQuadLayerVisuals *pQLayerVisuals = &(m_VisualQuad.value());

	const bool Textured = m_pLayerQuads->m_Image >= 0 && m_pLayerQuads->m_Image < m_pMapImages->Num();

	if(Textured)
		vTmpQuadsTextured.resize(m_pLayerQuads->m_NumQuads);
	else
		vTmpQuads.resize(m_pLayerQuads->m_NumQuads);

	m_vQuadRenderInfo.resize(m_pLayerQuads->m_NumQuads);
	CQuad *pQuads = (CQuad *)m_pMap->GetDataSwapped(m_pLayerQuads->m_Data);

	// try to create a quad render group
	m_Grouped = true;
	m_QuadRenderGroup.m_ColorEnv = pQuads[0].m_ColorEnv;
	m_QuadRenderGroup.m_ColorEnvOffset = pQuads[0].m_ColorEnvOffset;
	m_QuadRenderGroup.m_PosEnv = pQuads[0].m_PosEnv;
	m_QuadRenderGroup.m_PosEnvOffset = pQuads[0].m_PosEnvOffset;

	for(int i = 0; i < m_pLayerQuads->m_NumQuads; ++i)
	{
		CQuad *pQuad = &pQuads[i];

		// give up on grouping if envelopes missmatch
		if(m_Grouped && (pQuad->m_ColorEnv != m_QuadRenderGroup.m_ColorEnv || pQuad->m_ColorEnvOffset != m_QuadRenderGroup.m_ColorEnvOffset || pQuad->m_PosEnv != m_QuadRenderGroup.m_PosEnv || pQuad->m_PosEnvOffset != m_QuadRenderGroup.m_PosEnvOffset))
			m_Grouped = false;

		// init for envelopeless quad layers
		SQuadRenderInfo &QInfo = m_vQuadRenderInfo[i];
		QInfo.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		QInfo.m_Offsets.x = 0;
		QInfo.m_Offsets.y = 0;
		QInfo.m_Rotation = 0;

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
				vTmpQuads[i].m_aVertices[j].m_X = (pQuad->m_aPoints[QuadIdX].x);
				vTmpQuads[i].m_aVertices[j].m_Y = (pQuad->m_aPoints[QuadIdX].y);
				vTmpQuads[i].m_aVertices[j].m_CenterX = (pQuad->m_aPoints[4].x);
				vTmpQuads[i].m_aVertices[j].m_CenterY = (pQuad->m_aPoints[4].y);
				vTmpQuads[i].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
				vTmpQuads[i].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
				vTmpQuads[i].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
				vTmpQuads[i].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
			}
			else
			{
				// ignore the conversion for the position coordinates
				vTmpQuadsTextured[i].m_aVertices[j].m_X = (pQuad->m_aPoints[QuadIdX].x);
				vTmpQuadsTextured[i].m_aVertices[j].m_Y = (pQuad->m_aPoints[QuadIdX].y);
				vTmpQuadsTextured[i].m_aVertices[j].m_CenterX = (pQuad->m_aPoints[4].x);
				vTmpQuadsTextured[i].m_aVertices[j].m_CenterY = (pQuad->m_aPoints[4].y);
				vTmpQuadsTextured[i].m_aVertices[j].m_U = fx2f(pQuad->m_aTexcoords[QuadIdX].x);
				vTmpQuadsTextured[i].m_aVertices[j].m_V = fx2f(pQuad->m_aTexcoords[QuadIdX].y);
				vTmpQuadsTextured[i].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
				vTmpQuadsTextured[i].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
				vTmpQuadsTextured[i].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
				vTmpQuadsTextured[i].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
			}
		}
	}

	// we can directly push, only one render info is needed
	if(m_Grouped)
	{
		m_vQuadRenderInfo.resize(1);
	}

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
		int BufferObjectIndex = m_pGraphics->CreateBufferObject(UploadDataSize, pUploadData, 0);
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

		pQLayerVisuals->m_BufferContainerIndex = m_pGraphics->CreateBufferContainer(&ContainerInfo);
		// and finally inform the backend how many indices are required
		m_pGraphics->IndicesNumRequiredNotify(m_pLayerQuads->m_NumQuads * 6);
	}
	RenderLoading();
}

void CRenderLayerQuads::Render(const CRenderLayerParams &Params)
{
	UseTexture(GetTexture());
	CQuad *pQuads = (CQuad *)m_pMap->GetDataSwapped(m_pLayerQuads->m_Data);
	if(Params.m_RenderType == CMapLayers::TYPE_BACKGROUND_FORCE || Params.m_RenderType == CMapLayers::TYPE_FULL_DESIGN)
	{
		if(g_Config.m_ClShowQuads || Params.m_RenderType == CMapLayers::TYPE_FULL_DESIGN)
		{
			if(!m_pGraphics->IsQuadBufferingEnabled())
			{
				m_pGraphics->BlendNormal();
				m_pRenderTools->ForceRenderQuads(pQuads, m_pLayerQuads->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEvalRenderLayer, this, 1.f);
			}
			else
			{
				RenderQuadLayer(true);
			}
		}
	}
	else
	{
		if(!m_pGraphics->IsQuadBufferingEnabled())
		{
			m_pGraphics->BlendNormal();
			m_pRenderTools->RenderQuads(pQuads, m_pLayerQuads->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEvalRenderLayer, this);
		}
		else
		{
			RenderQuadLayer(false);
		}
	}
}

bool CRenderLayerQuads::DoRender(const CRenderLayerParams &Params) const
{
	// skip rendering anything but entities if we only want to render entities
	if(Params.EntityOverlayVal == 100 && Params.m_RenderType != CMapLayers::TYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering if detail layers if not wanted
	if(m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && Params.m_RenderType != CMapLayers::TYPE_FULL_DESIGN) // detail but no details
		return false;
	return true;
}

/****************
 * Entity Layer *
 ****************/
// BASE
CRenderLayerEntityBase::CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerTile(GroupId, LayerId, Flags, pLayerTilemap) {}

bool CRenderLayerEntityBase::DoRender(const CRenderLayerParams &Params) const
{
	// skip rendering if we render background force or full design
	if(Params.m_RenderType == CMapLayers::TYPE_BACKGROUND_FORCE || Params.m_RenderType == CMapLayers::TYPE_FULL_DESIGN)
		return false;

	// skip rendering of entities if don't want them
	if(!Params.EntityOverlayVal)
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

void CRenderLayerEntityGame::RenderTileLayerWithTileBuffer(const ColorRGBA &Color)
{
	m_pGraphics->BlendNormal();
	RenderKillTileBorder(Color.Multiply(GetDeathBorderColor()));
	RenderTileLayer(Color);
}

void CRenderLayerEntityGame::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CTile *pGameTiles = GetData<CTile>();
	m_pGraphics->BlendNone();
	m_pRenderTools->RenderTilemap(pGameTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
	m_pGraphics->BlendNormal();
	m_pRenderTools->RenderTileRectangle(-BorderRenderDistance, -BorderRenderDistance, m_pLayerTilemap->m_Width + 2 * BorderRenderDistance, m_pLayerTilemap->m_Height + 2 * BorderRenderDistance,
		TILE_AIR, TILE_DEATH, // display air inside, death outside
		32.0f, Color.Multiply(GetDeathBorderColor()), TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);

	m_pRenderTools->RenderTilemap(pGameTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
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

void CRenderLayerEntityTele::RenderTileLayerWithTileBuffer(const ColorRGBA &Color)
{
	m_pGraphics->BlendNormal();
	RenderTileLayer(Color);
	if(g_Config.m_ClTextEntities)
	{
		m_pGraphics->TextureSet(m_pMapImages->GetOverlayCenter());
		RenderTileLayer(Color, &m_VisualTeleNumbers.value());
	}
}

void CRenderLayerEntityTele::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CTeleTile *pTeleTiles = GetData<CTeleTile>();
	m_pGraphics->BlendNone();
	m_pRenderTools->RenderTelemap(pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
	m_pGraphics->BlendNormal();
	m_pRenderTools->RenderTelemap(pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);

	int OverlayRenderFlags = g_Config.m_ClTextEntities ? OVERLAYRENDERFLAG_TEXT : 0;
	m_pRenderTools->RenderTeleOverlay(pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

void CRenderLayerEntityTele::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	CTeleTile *pTiles = GetData<CTeleTile>();
	*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
	if(CurOverlay == 1)
	{
		if(IsTeleTileNumberUsedAny(*pIndex))
			*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
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

void CRenderLayerEntitySpeedup::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	CSpeedupTile *pTiles = GetData<CSpeedupTile>();
	*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	unsigned char Force = pTiles[y * m_pLayerTilemap->m_Width + x].m_Force;
	unsigned char MaxSpeed = pTiles[y * m_pLayerTilemap->m_Width + x].m_MaxSpeed;
	*pFlags = 0;
	*pAngleRotate = pTiles[y * m_pLayerTilemap->m_Width + x].m_Angle;
	if((Force == 0 && *pIndex == TILE_SPEED_BOOST_OLD) || (Force == 0 && MaxSpeed == 0 && *pIndex == TILE_SPEED_BOOST) || !IsValidSpeedupTile(*pIndex))
		*pIndex = 0;
	else if(CurOverlay == 1)
		*pIndex = Force;
	else if(CurOverlay == 2)
		*pIndex = MaxSpeed;
}

void CRenderLayerEntitySpeedup::RenderTileLayerWithTileBuffer(const ColorRGBA &Color)
{
	// draw arrow -- clamp to the edge of the arrow image
	m_pGraphics->WrapClamp();
	UseTexture(GetTexture());
	RenderTileLayer(Color);
	m_pGraphics->WrapNormal();

	if(g_Config.m_ClTextEntities)
	{
		m_pGraphics->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, &m_VisualForce.value());
		m_pGraphics->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, &m_VisualMaxSpeed.value());
	}
}

void CRenderLayerEntitySpeedup::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CSpeedupTile *pSpeedupTiles = GetData<CSpeedupTile>();
	int OverlayRenderFlags = g_Config.m_ClTextEntities ? OVERLAYRENDERFLAG_TEXT : 0;
	m_pRenderTools->RenderSpeedupOverlay(pSpeedupTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
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

void CRenderLayerEntitySwitch::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	CSwitchTile *pTiles = GetData<CSwitchTile>();
	*pFlags = 0;
	*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	if(CurOverlay == 0)
	{
		*pFlags = pTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
		if(*pIndex == TILE_SWITCHTIMEDOPEN)
			*pIndex = 8;
	}
	else if(CurOverlay == 1)
		*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
	else if(CurOverlay == 2)
		*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Delay;
}

void CRenderLayerEntitySwitch::RenderTileLayerWithTileBuffer(const ColorRGBA &Color)
{
	m_pGraphics->BlendNormal();
	RenderTileLayer(Color);
	if(g_Config.m_ClTextEntities)
	{
		m_pGraphics->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, &m_VisualSwitchNumberTop.value());
		m_pGraphics->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, &m_VisualSwitchNumberBottom.value());
	}
}

void CRenderLayerEntitySwitch::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CSwitchTile *pSwitchTiles = GetData<CSwitchTile>();
	m_pGraphics->BlendNone();
	m_pRenderTools->RenderSwitchmap(pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
	m_pGraphics->BlendNormal();
	m_pRenderTools->RenderSwitchmap(pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	int OverlayRenderFlags = g_Config.m_ClTextEntities ? OVERLAYRENDERFLAG_TEXT : 0;
	m_pRenderTools->RenderSwitchOverlay(pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

// TUNE
CRenderLayerEntityTune::CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

void CRenderLayerEntityTune::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	CTuneTile *pTiles = GetData<CTuneTile>();
	*pIndex = pTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
}

int CRenderLayerEntityTune::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTuneTile);
	return m_pLayerTilemap->m_Tune;
}

void CRenderLayerEntityTune::RenderTileLayerNoTileBuffer(const ColorRGBA &Color)
{
	CTuneTile *pTuneTiles = GetData<CTuneTile>();
	m_pGraphics->BlendNone();
	m_pRenderTools->RenderTunemap(pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
	m_pGraphics->BlendNormal();
	m_pRenderTools->RenderTunemap(pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
}
