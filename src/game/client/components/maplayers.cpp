/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <game/layers.h>
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/localization.h>

#include "maplayers.h"

#include <chrono>

using namespace std::chrono_literals;

const int LAYER_DEFAULT_TILESET = -1;

CMapLayers::CMapLayers(int Type, bool OnlineOnly)
{
	m_Type = Type;
	m_OnlineOnly = OnlineOnly;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = &m_pClient->m_MapImages;
}

CCamera *CMapLayers::GetCurCamera()
{
	return &m_pClient->m_Camera;
}

const char *CMapLayers::LoadingTitle() const
{
	return GameClient()->DemoPlayer()->IsPlaying() ? Localize("Preparing demo playback") : Localize("Connected");
}

void CMapLayers::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;

	int EnvStart, EnvNum;
	pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)pThis->m_pLayers->Map()->GetItem(EnvStart + Env);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	CMapBasedEnvelopePointAccess EnvelopePoints(pThis->m_pLayers->Map());
	EnvelopePoints.SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(EnvelopePoints.NumPoints() == 0)
		return;

	static std::chrono::nanoseconds s_Time{0};
	static auto s_LastLocalTime = time_get_nanoseconds();
	if(pThis->m_OnlineOnly && (pItem->m_Version < 2 || pItem->m_Synchronized))
	{
		if(pThis->m_pClient->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const auto TickToNanoSeconds = std::chrono::nanoseconds(1s) / (int64_t)pThis->Client()->GameTickSpeed();
			const int MinTick = pThis->Client()->PrevGameTick(g_Config.m_ClDummy) - pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = pThis->Client()->GameTick(g_Config.m_ClDummy) - pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			s_Time = std::chrono::nanoseconds((int64_t)(mix<double>(
									    0,
									    (CurTick - MinTick),
									    (double)pThis->Client()->IntraGameTick(g_Config.m_ClDummy)) *
								    TickToNanoSeconds.count())) +
				 MinTick * TickToNanoSeconds;
		}
	}
	else
	{
		const auto CurTime = time_get_nanoseconds();
		s_Time += CurTime - s_LastLocalTime;
		s_LastLocalTime = CurTime;
	}
	CRenderTools::RenderEvalEnvelope(&EnvelopePoints, s_Time + std::chrono::nanoseconds(std::chrono::milliseconds(TimeOffsetMillis)), Result, Channels);
}

static void FillTmpTile(SGraphicTile *pTmpTile, SGraphicTileTexureCoords *pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, const ivec2 &Offset, int Scale)
{
	if(pTmpTex)
	{
		unsigned char x0 = 0;
		unsigned char y0 = 0;
		unsigned char x1 = x0 + 1;
		unsigned char y1 = y0;
		unsigned char x2 = x0 + 1;
		unsigned char y2 = y0 + 1;
		unsigned char x3 = x0;
		unsigned char y3 = y0 + 1;

		if(Flags & TILEFLAG_XFLIP)
		{
			x0 = x2;
			x1 = x3;
			x2 = x3;
			x3 = x0;
		}

		if(Flags & TILEFLAG_YFLIP)
		{
			y0 = y3;
			y2 = y1;
			y3 = y1;
			y1 = y0;
		}

		if(Flags & TILEFLAG_ROTATE)
		{
			unsigned char Tmp = x0;
			x0 = x3;
			x3 = x2;
			x2 = x1;
			x1 = Tmp;
			Tmp = y0;
			y0 = y3;
			y3 = y2;
			y2 = y1;
			y1 = Tmp;
		}

		pTmpTex->m_TexCoordTopLeft.x = x0;
		pTmpTex->m_TexCoordTopLeft.y = y0;
		pTmpTex->m_TexCoordBottomLeft.x = x3;
		pTmpTex->m_TexCoordBottomLeft.y = y3;
		pTmpTex->m_TexCoordTopRight.x = x1;
		pTmpTex->m_TexCoordTopRight.y = y1;
		pTmpTex->m_TexCoordBottomRight.x = x2;
		pTmpTex->m_TexCoordBottomRight.y = y2;

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

	pTmpTile->m_TopLeft.x = x * Scale + Offset.x;
	pTmpTile->m_TopLeft.y = y * Scale + Offset.y;
	pTmpTile->m_BottomLeft.x = x * Scale + Offset.x;
	pTmpTile->m_BottomLeft.y = y * Scale + Scale + Offset.y;
	pTmpTile->m_TopRight.x = x * Scale + Scale + Offset.x;
	pTmpTile->m_TopRight.y = y * Scale + Offset.y;
	pTmpTile->m_BottomRight.x = x * Scale + Scale + Offset.x;
	pTmpTile->m_BottomRight.y = y * Scale + Scale + Offset.y;
}

static void FillTmpTileSpeedup(SGraphicTile *pTmpTile, SGraphicTileTexureCoords *pTmpTex, unsigned char Flags, int x, int y, const ivec2 &Offset, int Scale, short AngleRotate)
{
	int Angle = AngleRotate % 360;
	FillTmpTile(pTmpTile, pTmpTex, Angle >= 270 ? ROTATION_270 : (Angle >= 180 ? ROTATION_180 : (Angle >= 90 ? ROTATION_90 : 0)), AngleRotate % 90, x, y, Offset, Scale);
}

bool CMapLayers::STileLayerVisuals::Init(unsigned int Width, unsigned int Height)
{
	m_Width = Width;
	m_Height = Height;
	if(Width == 0 || Height == 0)
		return false;
	if constexpr(sizeof(unsigned int) >= sizeof(ptrdiff_t))
		if(Width >= std::numeric_limits<std::ptrdiff_t>::max() || Height >= std::numeric_limits<std::ptrdiff_t>::max())
			return false;

	m_pTilesOfLayer = new CMapLayers::STileLayerVisuals::STileVisual[Height * Width];

	m_vBorderTop.resize(Width);
	m_vBorderBottom.resize(Width);

	m_vBorderLeft.resize(Height);
	m_vBorderRight.resize(Height);
	return true;
}

CMapLayers::STileLayerVisuals::~STileLayerVisuals()
{
	delete[] m_pTilesOfLayer;

	m_pTilesOfLayer = nullptr;
}

static bool AddTile(std::vector<SGraphicTile> &vTmpTiles, std::vector<SGraphicTileTexureCoords> &vTmpTileTexCoords, unsigned char Index, unsigned char Flags, int x, int y, bool DoTextureCoords, bool FillSpeedup = false, int AngleRotate = -1, const ivec2 &Offset = ivec2{0, 0}, int Scale = 32)
{
	if(Index)
	{
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
	return false;
}

struct STmpQuadVertexTextured
{
	float m_X, m_Y, m_CenterX, m_CenterY;
	unsigned char m_R, m_G, m_B, m_A;
	float m_U, m_V;
};

struct STmpQuadVertex
{
	float m_X, m_Y, m_CenterX, m_CenterY;
	unsigned char m_R, m_G, m_B, m_A;
};

struct STmpQuad
{
	STmpQuadVertex m_aVertices[4];
};

struct STmpQuadTextured
{
	STmpQuadVertexTextured m_aVertices[4];
};

void mem_copy_special(void *pDest, void *pSource, size_t Size, size_t Count, size_t Steps)
{
	size_t CurStep = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		mem_copy(((char *)pDest) + CurStep + i * Size, ((char *)pSource) + i * Size, Size);
		CurStep += Steps;
	}
}

CMapLayers::~CMapLayers()
{
	// clear everything and destroy all buffers
	if(!m_vpTileLayerVisuals.empty())
	{
		int s = m_vpTileLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			delete m_vpTileLayerVisuals[i];
		}
	}
	if(!m_vpQuadLayerVisuals.empty())
	{
		int s = m_vpQuadLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			delete m_vpQuadLayerVisuals[i];
		}
	}
}

void CMapLayers::OnMapLoad()
{
	if(!Graphics()->IsTileBufferingEnabled() && !Graphics()->IsQuadBufferingEnabled())
	{
		// Find game group
		for(int g = 0; g < m_pLayers->NumGroups(); g++)
		{
			CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
			if(!pGroup)
			{
				dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
				dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
				dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
				continue;
			}

			for(int l = 0; l < pGroup->m_NumLayers; l++)
			{
				CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer + l);
				int LayerType = GetLayerType(pLayer);
				if(LayerType == LAYER_GAME)
				{
					m_GameGroup = g;
					return;
				}
			}
		}
		dbg_msg("maplayers", "failed to find game group, total groups = %d", m_pLayers->NumGroups());
		return;
	}

	const char *pLoadingTitle = LoadingTitle();
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	auto &&RenderLoading = [&]() {
		GameClient()->m_Menus.RenderLoading(pLoadingTitle, pLoadingMessage, 0);
	};

	// clear everything and destroy all buffers
	if(!m_vpTileLayerVisuals.empty())
	{
		int s = m_vpTileLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			Graphics()->DeleteBufferContainer(m_vpTileLayerVisuals[i]->m_BufferContainerIndex, true);
			delete m_vpTileLayerVisuals[i];
		}
		m_vpTileLayerVisuals.clear();
	}
	if(!m_vpQuadLayerVisuals.empty())
	{
		int s = m_vpQuadLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			Graphics()->DeleteBufferContainer(m_vpQuadLayerVisuals[i]->m_BufferContainerIndex, true);
			delete m_vpQuadLayerVisuals[i];
		}
		m_vpQuadLayerVisuals.clear();

		RenderLoading();
	}

	bool PassedGameLayer = false;
	// prepare all visuals for all tile layers
	std::vector<SGraphicTile> vtmpTiles;
	std::vector<SGraphicTileTexureCoords> vtmpTileTexCoords;
	std::vector<SGraphicTile> vtmpBorderTopTiles;
	std::vector<SGraphicTileTexureCoords> vtmpBorderTopTilesTexCoords;
	std::vector<SGraphicTile> vtmpBorderLeftTiles;
	std::vector<SGraphicTileTexureCoords> vtmpBorderLeftTilesTexCoords;
	std::vector<SGraphicTile> vtmpBorderRightTiles;
	std::vector<SGraphicTileTexureCoords> vtmpBorderRightTilesTexCoords;
	std::vector<SGraphicTile> vtmpBorderBottomTiles;
	std::vector<SGraphicTileTexureCoords> vtmpBorderBottomTilesTexCoords;
	std::vector<SGraphicTile> vtmpBorderCorners;
	std::vector<SGraphicTileTexureCoords> vtmpBorderCornersTexCoords;

	std::vector<STmpQuad> vtmpQuads;
	std::vector<STmpQuadTextured> vtmpQuadsTextured;

	m_vvLayerCount.clear();
	m_vvLayerCount.resize(m_pLayers->NumGroups());

	int TileLayerCounter = 0;
	int QuadLayerCounter = 0;

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		std::vector<int> vLayerCounter(pGroup->m_NumLayers, 0);

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer + l);
			int LayerType = GetLayerType(pLayer);
			PassedGameLayer |= LayerType == LAYER_GAME;
			bool IsEntityLayer = LayerType != LAYER_DEFAULT_TILESET;
			if(LayerType == LAYER_GAME)
				m_GameGroup = g;

			if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer)
				{
					m_vvLayerCount[g] = vLayerCounter;
					return;
				}
			}
			else if(m_Type == TYPE_FOREGROUND)
			{
				if(!PassedGameLayer)
					continue;
			}

			if(pLayer->m_Type == LAYERTYPE_TILES && Graphics()->IsTileBufferingEnabled())
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				const bool DoTextureCoords = IsEntityLayer || (pTMap->m_Image >= 0 && pTMap->m_Image < m_pImages->Num());

				void *pTiles;
				int TileLayerAndOverlayCount = GetTileLayerAndOverlayCount(pTMap, LayerType, &pTiles);

				if(TileLayerAndOverlayCount)
				{
					TileLayerCounter += TileLayerAndOverlayCount;
					vLayerCounter[l] = TileLayerCounter;

					int CurOverlay = 0;
					while(CurOverlay < TileLayerAndOverlayCount)
					{
						// We can later just count the tile layers to get the idx in the vector
						m_vpTileLayerVisuals.push_back(new STileLayerVisuals());
						STileLayerVisuals &Visuals = *m_vpTileLayerVisuals.back();
						if(!Visuals.Init(pTMap->m_Width, pTMap->m_Height))
						{
							++CurOverlay;
							continue;
						}
						Visuals.m_IsTextured = DoTextureCoords;

						vtmpTiles.clear();
						vtmpTileTexCoords.clear();

						vtmpBorderTopTiles.clear();
						vtmpBorderLeftTiles.clear();
						vtmpBorderRightTiles.clear();
						vtmpBorderBottomTiles.clear();
						vtmpBorderCorners.clear();
						vtmpBorderTopTilesTexCoords.clear();
						vtmpBorderLeftTilesTexCoords.clear();
						vtmpBorderRightTilesTexCoords.clear();
						vtmpBorderBottomTilesTexCoords.clear();
						vtmpBorderCornersTexCoords.clear();

						if(!DoTextureCoords)
						{
							vtmpTiles.reserve((size_t)pTMap->m_Width * pTMap->m_Height);
							vtmpBorderTopTiles.reserve((size_t)pTMap->m_Width);
							vtmpBorderBottomTiles.reserve((size_t)pTMap->m_Width);
							vtmpBorderLeftTiles.reserve((size_t)pTMap->m_Height);
							vtmpBorderRightTiles.reserve((size_t)pTMap->m_Height);
							vtmpBorderCorners.reserve((size_t)4);
						}
						else
						{
							vtmpTileTexCoords.reserve((size_t)pTMap->m_Width * pTMap->m_Height);
							vtmpBorderTopTilesTexCoords.reserve((size_t)pTMap->m_Width);
							vtmpBorderBottomTilesTexCoords.reserve((size_t)pTMap->m_Width);
							vtmpBorderLeftTilesTexCoords.reserve((size_t)pTMap->m_Height);
							vtmpBorderRightTilesTexCoords.reserve((size_t)pTMap->m_Height);
							vtmpBorderCornersTexCoords.reserve((size_t)4);
						}

						int x = 0;
						int y = 0;
						for(y = 0; y < pTMap->m_Height; ++y)
						{
							for(x = 0; x < pTMap->m_Width; ++x)
							{
								unsigned char Index = 0;
								unsigned char Flags = 0;
								int AngleRotate = -1;

								if(!IsEntityLayer || LayerType == LAYER_GAME || LayerType == LAYER_FRONT)
								{
									Index = ((CTile *)pTiles)[y * pTMap->m_Width + x].m_Index;
									Flags = ((CTile *)pTiles)[y * pTMap->m_Width + x].m_Flags;
								}
								else if(LayerType == LAYER_SWITCH)
								{
									Flags = 0;
									Index = ((CSwitchTile *)pTiles)[y * pTMap->m_Width + x].m_Type;
									if(CurOverlay == 0)
									{
										Flags = ((CSwitchTile *)pTiles)[y * pTMap->m_Width + x].m_Flags;
										if(Index == TILE_SWITCHTIMEDOPEN)
											Index = 8;
									}
									else if(CurOverlay == 1)
										Index = ((CSwitchTile *)pTiles)[y * pTMap->m_Width + x].m_Number;
									else if(CurOverlay == 2)
										Index = ((CSwitchTile *)pTiles)[y * pTMap->m_Width + x].m_Delay;
								}
								else if(LayerType == LAYER_TELE)
								{
									Index = ((CTeleTile *)pTiles)[y * pTMap->m_Width + x].m_Type;
									Flags = 0;
									if(CurOverlay == 1)
									{
										if(IsTeleTileNumberUsedAny(Index))
											Index = ((CTeleTile *)pTiles)[y * pTMap->m_Width + x].m_Number;
										else
											Index = 0;
									}
								}
								else if(LayerType == LAYER_SPEEDUP)
								{
									Index = ((CSpeedupTile *)pTiles)[y * pTMap->m_Width + x].m_Type;
									Flags = 0;
									AngleRotate = ((CSpeedupTile *)pTiles)[y * pTMap->m_Width + x].m_Angle;
									if(((CSpeedupTile *)pTiles)[y * pTMap->m_Width + x].m_Force == 0)
										Index = 0;
									else if(CurOverlay == 1)
										Index = ((CSpeedupTile *)pTiles)[y * pTMap->m_Width + x].m_Force;
									else if(CurOverlay == 2)
										Index = ((CSpeedupTile *)pTiles)[y * pTMap->m_Width + x].m_MaxSpeed;
								}
								else if(LayerType == LAYER_TUNE)
								{
									Index = ((CTuneTile *)pTiles)[y * pTMap->m_Width + x].m_Type;
									Flags = 0;
								}

								// the amount of tiles handled before this tile
								int TilesHandledCount = vtmpTiles.size();
								Visuals.m_pTilesOfLayer[y * pTMap->m_Width + x].SetIndexBufferByteOffset((offset_ptr32)(TilesHandledCount));

								bool AddAsSpeedup = false;
								if(LayerType == LAYER_SPEEDUP && CurOverlay == 0)
									AddAsSpeedup = true;

								if(AddTile(vtmpTiles, vtmpTileTexCoords, Index, Flags, x, y, DoTextureCoords, AddAsSpeedup, AngleRotate))
									Visuals.m_pTilesOfLayer[y * pTMap->m_Width + x].Draw(true);

								// do the border tiles
								if(x == 0)
								{
									if(y == 0)
									{
										Visuals.m_BorderTopLeft.SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderCorners.size()));
										if(AddTile(vtmpBorderCorners, vtmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, -32}))
											Visuals.m_BorderTopLeft.Draw(true);
									}
									else if(y == pTMap->m_Height - 1)
									{
										Visuals.m_BorderBottomLeft.SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderCorners.size()));
										if(AddTile(vtmpBorderCorners, vtmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
											Visuals.m_BorderBottomLeft.Draw(true);
									}
									Visuals.m_vBorderLeft[y].SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderLeftTiles.size()));
									if(AddTile(vtmpBorderLeftTiles, vtmpBorderLeftTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
										Visuals.m_vBorderLeft[y].Draw(true);
								}
								else if(x == pTMap->m_Width - 1)
								{
									if(y == 0)
									{
										Visuals.m_BorderTopRight.SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderCorners.size()));
										if(AddTile(vtmpBorderCorners, vtmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
											Visuals.m_BorderTopRight.Draw(true);
									}
									else if(y == pTMap->m_Height - 1)
									{
										Visuals.m_BorderBottomRight.SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderCorners.size()));
										if(AddTile(vtmpBorderCorners, vtmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
											Visuals.m_BorderBottomRight.Draw(true);
									}
									Visuals.m_vBorderRight[y].SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderRightTiles.size()));
									if(AddTile(vtmpBorderRightTiles, vtmpBorderRightTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
										Visuals.m_vBorderRight[y].Draw(true);
								}
								if(y == 0)
								{
									Visuals.m_vBorderTop[x].SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderTopTiles.size()));
									if(AddTile(vtmpBorderTopTiles, vtmpBorderTopTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
										Visuals.m_vBorderTop[x].Draw(true);
								}
								else if(y == pTMap->m_Height - 1)
								{
									Visuals.m_vBorderBottom[x].SetIndexBufferByteOffset((offset_ptr32)(vtmpBorderBottomTiles.size()));
									if(AddTile(vtmpBorderBottomTiles, vtmpBorderBottomTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
										Visuals.m_vBorderBottom[x].Draw(true);
								}
							}
						}

						// append one kill tile to the gamelayer
						if(LayerType == LAYER_GAME)
						{
							Visuals.m_BorderKillTile.SetIndexBufferByteOffset((offset_ptr32)(vtmpTiles.size()));
							if(AddTile(vtmpTiles, vtmpTileTexCoords, TILE_DEATH, 0, 0, 0, DoTextureCoords))
								Visuals.m_BorderKillTile.Draw(true);
						}

						// add the border corners, then the borders and fix their byte offsets
						int TilesHandledCount = vtmpTiles.size();
						Visuals.m_BorderTopLeft.AddIndexBufferByteOffset(TilesHandledCount);
						Visuals.m_BorderTopRight.AddIndexBufferByteOffset(TilesHandledCount);
						Visuals.m_BorderBottomLeft.AddIndexBufferByteOffset(TilesHandledCount);
						Visuals.m_BorderBottomRight.AddIndexBufferByteOffset(TilesHandledCount);
						// add the Corners to the tiles
						vtmpTiles.insert(vtmpTiles.end(), vtmpBorderCorners.begin(), vtmpBorderCorners.end());
						vtmpTileTexCoords.insert(vtmpTileTexCoords.end(), vtmpBorderCornersTexCoords.begin(), vtmpBorderCornersTexCoords.end());

						// now the borders
						TilesHandledCount = vtmpTiles.size();
						if(pTMap->m_Width > 0)
						{
							for(int i = 0; i < pTMap->m_Width; ++i)
							{
								Visuals.m_vBorderTop[i].AddIndexBufferByteOffset(TilesHandledCount);
							}
						}
						vtmpTiles.insert(vtmpTiles.end(), vtmpBorderTopTiles.begin(), vtmpBorderTopTiles.end());
						vtmpTileTexCoords.insert(vtmpTileTexCoords.end(), vtmpBorderTopTilesTexCoords.begin(), vtmpBorderTopTilesTexCoords.end());

						TilesHandledCount = vtmpTiles.size();
						if(pTMap->m_Width > 0)
						{
							for(int i = 0; i < pTMap->m_Width; ++i)
							{
								Visuals.m_vBorderBottom[i].AddIndexBufferByteOffset(TilesHandledCount);
							}
						}
						vtmpTiles.insert(vtmpTiles.end(), vtmpBorderBottomTiles.begin(), vtmpBorderBottomTiles.end());
						vtmpTileTexCoords.insert(vtmpTileTexCoords.end(), vtmpBorderBottomTilesTexCoords.begin(), vtmpBorderBottomTilesTexCoords.end());

						TilesHandledCount = vtmpTiles.size();
						if(pTMap->m_Height > 0)
						{
							for(int i = 0; i < pTMap->m_Height; ++i)
							{
								Visuals.m_vBorderLeft[i].AddIndexBufferByteOffset(TilesHandledCount);
							}
						}
						vtmpTiles.insert(vtmpTiles.end(), vtmpBorderLeftTiles.begin(), vtmpBorderLeftTiles.end());
						vtmpTileTexCoords.insert(vtmpTileTexCoords.end(), vtmpBorderLeftTilesTexCoords.begin(), vtmpBorderLeftTilesTexCoords.end());

						TilesHandledCount = vtmpTiles.size();
						if(pTMap->m_Height > 0)
						{
							for(int i = 0; i < pTMap->m_Height; ++i)
							{
								Visuals.m_vBorderRight[i].AddIndexBufferByteOffset(TilesHandledCount);
							}
						}
						vtmpTiles.insert(vtmpTiles.end(), vtmpBorderRightTiles.begin(), vtmpBorderRightTiles.end());
						vtmpTileTexCoords.insert(vtmpTileTexCoords.end(), vtmpBorderRightTilesTexCoords.begin(), vtmpBorderRightTilesTexCoords.end());

						// setup params
						float *pTmpTiles = vtmpTiles.empty() ? nullptr : (float *)vtmpTiles.data();
						unsigned char *pTmpTileTexCoords = vtmpTileTexCoords.empty() ? nullptr : (unsigned char *)vtmpTileTexCoords.data();

						Visuals.m_BufferContainerIndex = -1;
						size_t UploadDataSize = vtmpTileTexCoords.size() * sizeof(SGraphicTileTexureCoords) + vtmpTiles.size() * sizeof(SGraphicTile);
						if(UploadDataSize > 0)
						{
							char *pUploadData = (char *)malloc(sizeof(char) * UploadDataSize);

							mem_copy_special(pUploadData, pTmpTiles, sizeof(vec2), vtmpTiles.size() * 4, (DoTextureCoords ? sizeof(ubvec4) : 0));
							if(DoTextureCoords)
							{
								mem_copy_special(pUploadData + sizeof(vec2), pTmpTileTexCoords, sizeof(ubvec4), vtmpTiles.size() * 4, sizeof(vec2));
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
							Graphics()->IndicesNumRequiredNotify(vtmpTiles.size() * 6);

							RenderLoading();
						}

						++CurOverlay;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS && Graphics()->IsQuadBufferingEnabled())
			{
				++QuadLayerCounter;
				vLayerCounter[l] = QuadLayerCounter;

				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;

				m_vpQuadLayerVisuals.push_back(new SQuadLayerVisuals());
				SQuadLayerVisuals *pQLayerVisuals = m_vpQuadLayerVisuals.back();

				const bool Textured = pQLayer->m_Image >= 0 && pQLayer->m_Image < m_pImages->Num();

				vtmpQuads.clear();
				vtmpQuadsTextured.clear();

				if(Textured)
					vtmpQuadsTextured.resize(pQLayer->m_NumQuads);
				else
					vtmpQuads.resize(pQLayer->m_NumQuads);

				CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);
				for(int i = 0; i < pQLayer->m_NumQuads; ++i)
				{
					CQuad *pQuad = &pQuads[i];
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
							vtmpQuads[i].m_aVertices[j].m_X = (pQuad->m_aPoints[QuadIdX].x);
							vtmpQuads[i].m_aVertices[j].m_Y = (pQuad->m_aPoints[QuadIdX].y);
							vtmpQuads[i].m_aVertices[j].m_CenterX = (pQuad->m_aPoints[4].x);
							vtmpQuads[i].m_aVertices[j].m_CenterY = (pQuad->m_aPoints[4].y);
							vtmpQuads[i].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
							vtmpQuads[i].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
							vtmpQuads[i].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
							vtmpQuads[i].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
						}
						else
						{
							// ignore the conversion for the position coordinates
							vtmpQuadsTextured[i].m_aVertices[j].m_X = (pQuad->m_aPoints[QuadIdX].x);
							vtmpQuadsTextured[i].m_aVertices[j].m_Y = (pQuad->m_aPoints[QuadIdX].y);
							vtmpQuadsTextured[i].m_aVertices[j].m_CenterX = (pQuad->m_aPoints[4].x);
							vtmpQuadsTextured[i].m_aVertices[j].m_CenterY = (pQuad->m_aPoints[4].y);
							vtmpQuadsTextured[i].m_aVertices[j].m_U = fx2f(pQuad->m_aTexcoords[QuadIdX].x);
							vtmpQuadsTextured[i].m_aVertices[j].m_V = fx2f(pQuad->m_aTexcoords[QuadIdX].y);
							vtmpQuadsTextured[i].m_aVertices[j].m_R = (unsigned char)pQuad->m_aColors[QuadIdX].r;
							vtmpQuadsTextured[i].m_aVertices[j].m_G = (unsigned char)pQuad->m_aColors[QuadIdX].g;
							vtmpQuadsTextured[i].m_aVertices[j].m_B = (unsigned char)pQuad->m_aColors[QuadIdX].b;
							vtmpQuadsTextured[i].m_aVertices[j].m_A = (unsigned char)pQuad->m_aColors[QuadIdX].a;
						}
					}
				}

				size_t UploadDataSize = 0;
				if(Textured)
					UploadDataSize = vtmpQuadsTextured.size() * sizeof(STmpQuadTextured);
				else
					UploadDataSize = vtmpQuads.size() * sizeof(STmpQuad);

				if(UploadDataSize > 0)
				{
					void *pUploadData = nullptr;
					if(Textured)
						pUploadData = vtmpQuadsTextured.data();
					else
						pUploadData = vtmpQuads.data();
					// create the buffer object
					int BufferObjectIndex = Graphics()->CreateBufferObject(UploadDataSize, pUploadData, 0);
					// then create the buffer container
					SBufferContainerInfo ContainerInfo;
					ContainerInfo.m_Stride = (Textured ? (sizeof(STmpQuadTextured) / 4) : (sizeof(STmpQuad) / 4));
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
					Graphics()->IndicesNumRequiredNotify(pQLayer->m_NumQuads * 6);

					RenderLoading();
				}
			}
		}
		m_vvLayerCount[g] = vLayerCounter;
	}
}

void CMapLayers::RenderTileLayer(int LayerIndex, const ColorRGBA &Color)
{
	STileLayerVisuals &Visuals = *m_vpTileLayerVisuals[LayerIndex];
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int ScreenRectY0 = std::floor(ScreenY0 / 32);
	int ScreenRectX0 = std::floor(ScreenX0 / 32);
	int ScreenRectY1 = std::ceil(ScreenY1 / 32);
	int ScreenRectX1 = std::ceil(ScreenX1 / 32);

	if(ScreenRectX1 > 0 && ScreenRectY1 > 0 && ScreenRectX0 < (int)Visuals.m_Width && ScreenRectY0 < (int)Visuals.m_Height)
	{
		// create the indice buffers we want to draw -- reuse them
		static std::vector<char *> s_vpIndexOffsets;
		static std::vector<unsigned int> s_vDrawCounts;

		int X0 = std::max(ScreenRectX0, 0);
		int X1 = std::min(ScreenRectX1, (int)Visuals.m_Width);
		if(X0 <= X1)
		{
			int Y0 = std::max(ScreenRectY0, 0);
			int Y1 = std::min(ScreenRectY1, (int)Visuals.m_Height);

			s_vpIndexOffsets.clear();
			s_vDrawCounts.clear();

			unsigned long long Reserve = absolute(Y1 - Y0) + 1;
			s_vpIndexOffsets.reserve(Reserve);
			s_vDrawCounts.reserve(Reserve);

			for(int y = Y0; y < Y1; ++y)
			{
				int XR = X1 - 1;

				dbg_assert(Visuals.m_pTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() >= Visuals.m_pTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset(), "Tile count wrong.");

				unsigned int NumVertices = ((Visuals.m_pTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() - Visuals.m_pTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_pTilesOfLayer[y * Visuals.m_Width + XR].DoDraw() ? 6lu : 0lu);

				if(NumVertices)
				{
					s_vpIndexOffsets.push_back((offset_ptr_size)Visuals.m_pTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset());
					s_vDrawCounts.push_back(NumVertices);
				}
			}

			int DrawCount = s_vpIndexOffsets.size();
			if(DrawCount != 0)
			{
				Graphics()->RenderTileLayer(Visuals.m_BufferContainerIndex, Color, s_vpIndexOffsets.data(), s_vDrawCounts.data(), DrawCount);
			}
		}
	}

	if(ScreenRectX1 > (int)Visuals.m_Width || ScreenRectY1 > (int)Visuals.m_Height || ScreenRectX0 < 0 || ScreenRectY0 < 0)
	{
		RenderTileBorder(LayerIndex, Color, ScreenRectX0, ScreenRectY0, ScreenRectX1, ScreenRectY1);
	}
}

void CMapLayers::RenderTileBorder(int LayerIndex, const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1)
{
	STileLayerVisuals &Visuals = *m_vpTileLayerVisuals[LayerIndex];

	int Y0 = std::max(0, BorderY0);
	int X0 = std::max(0, BorderX0);
	int Y1 = std::min((int)Visuals.m_Height, BorderY1);
	int X1 = std::min((int)Visuals.m_Width, BorderX1);

	// corners
	auto DrawCorner = [this, &Visuals, Color](vec2 Offset, vec2 Scale, STileLayerVisuals::STileVisual &Visual) {
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, (offset_ptr_size)Visual.IndexBufferByteOffset(), Offset, Scale, 1);
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
	auto DrawBorder = [this, &Visuals, Color](vec2 Offset, vec2 Scale, STileLayerVisuals::STileVisual &StartVisual, STileLayerVisuals::STileVisual &EndVisual) {
		unsigned int DrawNum = ((EndVisual.IndexBufferByteOffset() - StartVisual.IndexBufferByteOffset()) / (sizeof(unsigned int) * 6)) + (EndVisual.DoDraw() ? 1lu : 0lu);
		offset_ptr_size pOffset = (offset_ptr_size)StartVisual.IndexBufferByteOffset();
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, DrawNum);
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

void CMapLayers::RenderKillTileBorder(int LayerIndex, const ColorRGBA &Color)
{
	STileLayerVisuals &Visuals = *m_vpTileLayerVisuals[LayerIndex];
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int BorderY0 = std::floor(ScreenY0 / 32);
	int BorderX0 = std::floor(ScreenX0 / 32);
	int BorderY1 = std::ceil(ScreenY1 / 32);
	int BorderX1 = std::ceil(ScreenX1 / 32);

	const int BorderDist = 201;

	if(BorderX0 >= -BorderDist && BorderY0 >= -BorderDist && BorderX1 <= (int)Visuals.m_Width + BorderDist && BorderY1 <= (int)Visuals.m_Height + BorderDist)
		return;
	if(!Visuals.m_BorderKillTile.DoDraw())
		return;

	BorderX0 = clamp(BorderX0, -300, (int)Visuals.m_Width + 299);
	BorderY0 = clamp(BorderY0, -300, (int)Visuals.m_Height + 299);
	BorderX1 = clamp(BorderX1, -300, (int)Visuals.m_Width + 299);
	BorderY1 = clamp(BorderY1, -300, (int)Visuals.m_Height + 299);

	auto DrawKillBorder = [Color, this, LayerIndex](vec2 Offset, vec2 Scale) {
		offset_ptr_size pOffset = (offset_ptr_size)m_vpTileLayerVisuals[LayerIndex]->m_BorderKillTile.IndexBufferByteOffset();
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(m_vpTileLayerVisuals[LayerIndex]->m_BufferContainerIndex, Color, pOffset, Offset, Scale, 1);
	};

	// Draw left kill tile border
	if(BorderX0 < -BorderDist)
	{
		DrawKillBorder(
			vec2(BorderX0, BorderY0),
			vec2(-BorderDist - BorderX0, BorderY1 - BorderY0));
	}
	// Draw top kill tile border
	if(BorderY0 < -BorderDist)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderDist), BorderY0),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderDist) - std::max(BorderX0, -BorderDist), -BorderDist - BorderY0));
	}
	// Draw right kill tile border
	if(BorderX1 > (int)Visuals.m_Width + BorderDist)
	{
		DrawKillBorder(
			vec2(Visuals.m_Width + BorderDist, BorderY0),
			vec2(BorderX1 - (Visuals.m_Width + BorderDist), BorderY1 - BorderY0));
	}
	// Draw bottom kill tile border
	if(BorderY1 > (int)Visuals.m_Height + BorderDist)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderDist), Visuals.m_Height + BorderDist),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderDist) - std::max(BorderX0, -BorderDist), BorderY1 - (Visuals.m_Height + BorderDist)));
	}
}

void CMapLayers::RenderQuadLayer(int LayerIndex, CMapItemLayerQuads *pQuadLayer, bool Force)
{
	SQuadLayerVisuals &Visuals = *m_vpQuadLayerVisuals[LayerIndex];
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	if(!Force && (!g_Config.m_ClShowQuads || g_Config.m_ClOverlayEntities == 100))
		return;

	CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pQuadLayer->m_Data);

	static std::vector<SQuadRenderInfo> s_vQuadRenderInfo;

	s_vQuadRenderInfo.resize(pQuadLayer->m_NumQuads);
	size_t QuadsRenderCount = 0;
	size_t CurQuadOffset = 0;
	for(int i = 0; i < pQuadLayer->m_NumQuads; ++i)
	{
		CQuad *pQuad = &pQuads[i];

		ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		EnvelopeEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, 4, this);

		const bool IsFullyTransparent = Color.a <= 0.0f;
		const bool NeedsFlush = QuadsRenderCount == gs_GraphicsMaxQuadsRenderCount || IsFullyTransparent;

		if(NeedsFlush)
		{
			// render quads of the current offset directly(cancel batching)
			Graphics()->RenderQuadLayer(Visuals.m_BufferContainerIndex, s_vQuadRenderInfo.data(), QuadsRenderCount, CurQuadOffset);
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
			EnvelopeEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3, this);

			SQuadRenderInfo &QInfo = s_vQuadRenderInfo[QuadsRenderCount++];
			QInfo.m_Color = Color;
			QInfo.m_Offsets.x = Position.r;
			QInfo.m_Offsets.y = Position.g;
			QInfo.m_Rotation = Position.b / 180.0f * pi;
		}
	}
	Graphics()->RenderQuadLayer(Visuals.m_BufferContainerIndex, s_vQuadRenderInfo.data(), QuadsRenderCount, CurQuadOffset);
}

void CMapLayers::OnRender()
{
	if(m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = GetCurCamera()->m_Center;

	// Hide Entities in full design mode
	int EntityOverlayVal = m_Type == TYPE_FULL_DESIGN ? 0 : g_Config.m_ClOverlayEntities;
	bool OnlyShowEntities = EntityOverlayVal == 100;

	bool PassedGameLayer = false;
	int StartGroup = m_Type == TYPE_FOREGROUND ? m_GameGroup : 0;
	int EndGroup = (m_Type == TYPE_BACKGROUND || m_Type == TYPE_BACKGROUND_FORCE) ? std::min(m_GameGroup + 1, m_pLayers->NumGroups()) : m_pLayers->NumGroups();

	for(int g = StartGroup; g < EndGroup; g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);

		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		if((!g_Config.m_GfxNoclip || m_Type == TYPE_FULL_DESIGN) && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float aPoints[4];
			RenderTools()->MapScreenToGroup(Center.x, Center.y, m_pLayers->GameGroup(), GetCurCamera()->m_Zoom);
			Graphics()->GetScreen(&aPoints[0], &aPoints[1], &aPoints[2], &aPoints[3]);
			float x0 = (pGroup->m_ClipX - aPoints[0]) / (aPoints[2] - aPoints[0]);
			float y0 = (pGroup->m_ClipY - aPoints[1]) / (aPoints[3] - aPoints[1]);
			float x1 = ((pGroup->m_ClipX + pGroup->m_ClipW) - aPoints[0]) / (aPoints[2] - aPoints[0]);
			float y1 = ((pGroup->m_ClipY + pGroup->m_ClipH) - aPoints[1]) / (aPoints[3] - aPoints[1]);

			if(x1 < 0.0f || x0 > 1.0f || y1 < 0.0f || y0 > 1.0f)
				continue;

			Graphics()->ClipEnable((int)(x0 * Graphics()->ScreenWidth()), (int)(y0 * Graphics()->ScreenHeight()),
				(int)((x1 - x0) * Graphics()->ScreenWidth()), (int)((y1 - y0) * Graphics()->ScreenHeight()));
		}

		RenderTools()->MapScreenToGroup(Center.x, Center.y, pGroup, GetCurCamera()->m_Zoom);

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer + l);
			int LayerType = GetLayerType(pLayer);
			PassedGameLayer |= LayerType == LAYER_GAME;
			bool IsEntityLayer = LayerType != LAYER_DEFAULT_TILESET;

			// stop rendering if we render background but reach the foreground
			if(PassedGameLayer && (m_Type == TYPE_BACKGROUND || m_Type == TYPE_BACKGROUND_FORCE))
				return;

			// skip rendering if we render foreground but are in background
			if(!PassedGameLayer && m_Type == TYPE_FOREGROUND)
				continue;

			// skip rendering if we render background force, but deactivated tile layer and want to render a tilelayer
			if(pLayer->m_Type == LAYERTYPE_TILES && !g_Config.m_ClBackgroundShowTilesLayers && m_Type == TYPE_BACKGROUND_FORCE)
				continue;

			// skip rendering if we render background but encounter an entity, e.g. speed layer infront of game layer or similar
			if(m_Type == TYPE_BACKGROUND_FORCE && IsEntityLayer)
				continue;

			// skip rendering entities if we want to render everything in it's full glory
			if(m_Type == TYPE_FULL_DESIGN && IsEntityLayer)
				continue;

			// skip rendering anything but entities if we only want to render entities
			if(!IsEntityLayer && OnlyShowEntities && m_Type != TYPE_BACKGROUND_FORCE)
				continue;

			// skip rendering of entities if don't want them
			if(IsEntityLayer && !EntityOverlayVal)
				continue;

			// skip rendering if detail layers if not wanted
			if(pLayer->m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && m_Type != TYPE_FULL_DESIGN && LayerType != LAYER_GAME) // detail but no details
				continue;

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				ColorRGBA Color;
				int TextureId = -1;
				CMapItemLayerTilemap *pLayerTilemap = (CMapItemLayerTilemap *)pLayer;
				if(!IsEntityLayer)
				{
					if(pLayerTilemap->m_Image >= 0 && pLayerTilemap->m_Image < m_pImages->Num())
						TextureId = pLayerTilemap->m_Image;

					Color = ColorRGBA(pLayerTilemap->m_Color.r / 255.0f, pLayerTilemap->m_Color.g / 255.0f, pLayerTilemap->m_Color.b / 255.0f, pLayerTilemap->m_Color.a / 255.0f);
					if(EntityOverlayVal && m_Type != TYPE_BACKGROUND_FORCE)
						Color.a *= (100 - EntityOverlayVal) / 100.0f;

					ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
					EnvelopeEval(pLayerTilemap->m_ColorEnvOffset, pLayerTilemap->m_ColorEnv, ColorEnv, 4, this);
					Color = Color.Multiply(ColorEnv);
				}
				else
				{
					Color = ColorRGBA(1.0f, 1.0f, 1.0f, EntityOverlayVal / 100.0f);
				}

				if(!Graphics()->IsTileBufferingEnabled())
				{
					void *pTilesData = nullptr;
					GetTileLayerAndOverlayCount(pLayerTilemap, LayerType, &pTilesData);
					RenderTilelayerNoTileBuffer(TextureId, LayerType, pTilesData, pLayerTilemap, Color);
				}
				else
				{
					RenderTilelayerWithTileBuffer(TextureId, LayerType, m_vvLayerCount[g][l], Color);
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pLayerQuads = (CMapItemLayerQuads *)pLayer;
				if(pLayerQuads->m_Image >= 0 && pLayerQuads->m_Image < m_pImages->Num())
					Graphics()->TextureSet(m_pImages->Get(pLayerQuads->m_Image));
				else
					Graphics()->TextureClear();

				CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pLayerQuads->m_Data);
				if(m_Type == TYPE_BACKGROUND_FORCE || m_Type == TYPE_FULL_DESIGN)
				{
					if(g_Config.m_ClShowQuads || m_Type == TYPE_FULL_DESIGN)
					{
						if(!Graphics()->IsQuadBufferingEnabled())
						{
							Graphics()->BlendNormal();
							RenderTools()->ForceRenderQuads(pQuads, pLayerQuads->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this, 1.f);
						}
						else
						{
							RenderQuadLayer(m_vvLayerCount[g][l] - 1, pLayerQuads, true);
						}
					}
				}
				else
				{
					if(!Graphics()->IsQuadBufferingEnabled())
					{
						Graphics()->BlendNormal();
						RenderTools()->RenderQuads(pQuads, pLayerQuads->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this);
					}
					else
					{
						RenderQuadLayer(m_vvLayerCount[g][l] - 1, pLayerQuads, false);
					}
				}
			}
		}
		if(!g_Config.m_GfxNoclip || m_Type == TYPE_FULL_DESIGN)
			Graphics()->ClipDisable();
	}

	if(!g_Config.m_GfxNoclip || m_Type == TYPE_FULL_DESIGN)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

int CMapLayers::GetLayerType(const CMapItemLayer *pLayer) const
{
	if(pLayer == (CMapItemLayer *)m_pLayers->GameLayer())
		return LAYER_GAME;
	else if(pLayer == (CMapItemLayer *)m_pLayers->FrontLayer())
		return LAYER_FRONT;
	else if(pLayer == (CMapItemLayer *)m_pLayers->SwitchLayer())
		return LAYER_SWITCH;
	else if(pLayer == (CMapItemLayer *)m_pLayers->TeleLayer())
		return LAYER_TELE;
	else if(pLayer == (CMapItemLayer *)m_pLayers->SpeedupLayer())
		return LAYER_SPEEDUP;
	else if(pLayer == (CMapItemLayer *)m_pLayers->TuneLayer())
		return LAYER_TUNE;
	return LAYER_DEFAULT_TILESET;
}

int CMapLayers::GetTileLayerAndOverlayCount(const CMapItemLayerTilemap *pLayerTilemap, int LayerType, void **ppTiles) const
{
	int DataIndex;
	unsigned int TileSize;
	int OverlayCount;
	switch(LayerType)
	{
	case LAYER_FRONT:
		DataIndex = pLayerTilemap->m_Front;
		TileSize = sizeof(CTile);
		OverlayCount = 0;
		break;
	case LAYER_SWITCH:
		DataIndex = pLayerTilemap->m_Switch;
		TileSize = sizeof(CSwitchTile);
		OverlayCount = 2;
		break;
	case LAYER_TELE:
		DataIndex = pLayerTilemap->m_Tele;
		TileSize = sizeof(CTeleTile);
		OverlayCount = 1;
		break;
	case LAYER_SPEEDUP:
		DataIndex = pLayerTilemap->m_Speedup;
		TileSize = sizeof(CSpeedupTile);
		OverlayCount = 2;
		break;
	case LAYER_TUNE:
		DataIndex = pLayerTilemap->m_Tune;
		TileSize = sizeof(CTuneTile);
		OverlayCount = 0;
		break;
	case LAYER_GAME:
	default:
		DataIndex = pLayerTilemap->m_Data;
		TileSize = sizeof(CTile);
		OverlayCount = 0;
		break;
	}

	void *pTiles = m_pLayers->Map()->GetData(DataIndex);
	int Size = m_pLayers->Map()->GetDataSize(DataIndex);

	if(!pTiles || Size < pLayerTilemap->m_Width * pLayerTilemap->m_Height * (int)TileSize)
		return 0;

	if(ppTiles)
		*ppTiles = pTiles;

	return OverlayCount + 1; // always add 1 tilelayer
}

void CMapLayers::RenderTilelayerWithTileBuffer(int ImageIndex, int LayerType, int TileLayerCounter, const ColorRGBA &Color)
{
	switch(LayerType)
	{
	case LAYER_GAME:
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->BlendNormal();
		RenderKillTileBorder(TileLayerCounter - 1, Color.Multiply(GetDeathBorderColor()));
		RenderTileLayer(TileLayerCounter - 1, Color);
		break;

	case LAYER_TELE:
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->BlendNormal();
		RenderTileLayer(TileLayerCounter - 2, Color);
		if(g_Config.m_ClTextEntities)
		{
			Graphics()->TextureSet(m_pImages->GetOverlayCenter());
			RenderTileLayer(TileLayerCounter - 1, Color);
		}
		break;

	case LAYER_SPEEDUP:
		// draw arrow -- clamp to the edge of the arrow image
		Graphics()->WrapClamp();
		Graphics()->TextureSet(m_pImages->GetSpeedupArrow());
		RenderTileLayer(TileLayerCounter - 3, Color);
		Graphics()->WrapNormal();

		if(g_Config.m_ClTextEntities)
		{
			Graphics()->TextureSet(m_pImages->GetOverlayBottom());
			RenderTileLayer(TileLayerCounter - 2, Color);
			Graphics()->TextureSet(m_pImages->GetOverlayTop());
			RenderTileLayer(TileLayerCounter - 1, Color);
		}
		break;

	case LAYER_SWITCH:
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH));
		Graphics()->BlendNormal();
		RenderTileLayer(TileLayerCounter - 3, Color);
		if(g_Config.m_ClTextEntities)
		{
			Graphics()->TextureSet(m_pImages->GetOverlayTop());
			RenderTileLayer(TileLayerCounter - 2, Color);
			Graphics()->TextureSet(m_pImages->GetOverlayBottom());
			RenderTileLayer(TileLayerCounter - 1, Color);
		}
		break;

	case LAYER_FRONT:
	case LAYER_TUNE:
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->BlendNormal();
		RenderTileLayer(TileLayerCounter - 1, Color);
		break;

	case LAYER_DEFAULT_TILESET:
		if(ImageIndex != -1)
			Graphics()->TextureSet(m_pImages->Get(ImageIndex));
		else
			Graphics()->TextureClear();
		Graphics()->BlendNormal();
		RenderTileLayer(TileLayerCounter - 1, Color);
		break;

	default:
		dbg_assert(false, "Unknown LayerType %d", LayerType);
	}
}

void CMapLayers::RenderTilelayerNoTileBuffer(int ImageIndex, int LayerType, void *pTilesData, CMapItemLayerTilemap *pLayerTilemap, const ColorRGBA &Color)
{
	int OverlayRenderFlags = g_Config.m_ClTextEntities ? OVERLAYRENDERFLAG_TEXT : 0;

	switch(LayerType)
	{
	case LAYER_GAME:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		CTile *pGameTiles = (CTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderTilemap(pGameTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderTileRectangle(-201, -201, pLayerTilemap->m_Width + 402, pLayerTilemap->m_Height + 402,
			TILE_AIR, TILE_DEATH, // display air inside, death outside
			32.0f, Color.Multiply(GetDeathBorderColor()), TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);

		RenderTools()->RenderTilemap(pGameTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}
	break;

	case LAYER_TELE:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		CTeleTile *pTeleTiles = (CTeleTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderTelemap(pTeleTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderTelemap(pTeleTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
		RenderTools()->RenderTeleOverlay(pTeleTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
	}
	break;

	case LAYER_SPEEDUP:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		CSpeedupTile *pSpeedupTiles = (CSpeedupTile *)pTilesData;
		RenderTools()->RenderSpeedupOverlay(pSpeedupTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
	}
	break;

	case LAYER_SWITCH:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH));
		CSwitchTile *pSwitchTiles = (CSwitchTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderSwitchmap(pSwitchTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderSwitchmap(pSwitchTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
		RenderTools()->RenderSwitchOverlay(pSwitchTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
	}
	break;

	case LAYER_TUNE:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		CTuneTile *pTuneTiles = (CTuneTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderTunemap(pTuneTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderTunemap(pTuneTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}
	break;

	case LAYER_FRONT:
	{
		Graphics()->TextureSet(m_pImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		CTile *pTiles = (CTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderTilemap(pTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderTilemap(pTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}
	break;

	case LAYER_DEFAULT_TILESET:
	{
		if(ImageIndex != -1)
			Graphics()->TextureSet(m_pImages->Get(ImageIndex));
		else
			Graphics()->TextureClear();
		CTile *pTiles = (CTile *)pTilesData;
		Graphics()->BlendNone();
		RenderTools()->RenderTilemap(pTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
		Graphics()->BlendNormal();
		RenderTools()->RenderTilemap(pTiles, pLayerTilemap->m_Width, pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}
	break;

	default:
		dbg_assert(false, "Unknown LayerType %d", LayerType);
	}
}

ColorRGBA CMapLayers::GetDeathBorderColor() const
{
	// draw kill tiles outside the entity clipping rectangle
	// slow blinking to hint that it's not a part of the map
	float Seconds = time_get() / (float)time_freq();
	float Alpha = 0.3f + 0.35f * (1.f + std::sin(2.f * pi * Seconds / 3.f));
	return ColorRGBA(1.f, 1.f, 1.f, Alpha);
}
