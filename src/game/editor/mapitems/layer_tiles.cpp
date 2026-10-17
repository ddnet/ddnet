/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_tiles.h"

#include "image.h"

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/editor/editor.h>
#include <game/editor/enums.h>

#include <iterator>
#include <numeric>

CLayerTiles::CLayerTiles(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId) :
	CLayer(pMap, LAYERTYPE_TILES, RetainedId)
{
	m_aName[0] = '\0';
	m_Tiles.Resize(w, h);
	m_Image = Map()->ImageReference(-1);
	m_HasGame = false;
	m_Color.r = 255;
	m_Color.g = 255;
	m_Color.b = 255;
	m_Color.a = 255;
	m_ColorEnv = Map()->EnvelopeReference(-1);
	m_ColorEnvOffset = 0;

	m_HasTele = false;
	m_HasSpeedup = false;
	m_HasFront = false;
	m_HasSwitch = false;
	m_HasTune = false;
	m_AutomapperConfig = -1;
	m_AutomapperReference = -1;
	m_Seed = 0;
	m_AutoAutomapper = false;
}

CLayerTiles::CLayerTiles(const CLayerTiles &Other) :
	CLayer(Other),
	CLayerTilesValues(Other)
{
	str_copy(m_aFilename, Other.m_aFilename);
}

CLayerTiles::~CLayerTiles() = default;

CTile CLayerTiles::GetTile(int x, int y) const
{
	return m_Tiles[static_cast<std::size_t>(y) * Width() + x];
}

void CLayerTiles::SetTile(int x, int y, CTile Tile)
{
	m_Tiles.Set(static_cast<std::size_t>(y) * Width() + x, Tile);

	if(m_FillGameTile != -1 && m_LiveGameTiles)
	{
		std::shared_ptr<CLayerTiles> pLayer = Map()->m_pGameLayer;
		if(m_FillGameTile == TILE_TELECHECKIN || m_FillGameTile == TILE_TELECHECKINEVIL)
		{
			if(!Map()->m_pTeleLayer)
			{
				std::shared_ptr<CLayerTele> pLayerTele = std::make_shared<CLayerTele>(Map(), Width(), Height());
				Map()->MakeTeleLayer(pLayerTele);
				Map()->m_pGameGroup->AddLayer(pLayerTele);
			}

			pLayer = Map()->m_pTeleLayer;
		}

		bool HasTile = Tile.m_Index != 0;
		pLayer->SetTile(x, y, CTile{(unsigned char)(HasTile ? m_FillGameTile : TILE_AIR)});
	}
}

std::vector<CTile> CLayerTiles::TilesForSave() const
{
	std::array<unsigned char, 256> aFlags{};
	if(Map()->ImageIndex(m_Image) >= 0 && static_cast<std::size_t>(Map()->ImageIndex(m_Image)) < Map()->m_vpImages.size() && m_Color.a == 255)
		aFlags = Map()->m_vpImages[Map()->ImageIndex(m_Image)]->TileFlags();
	return ExportTilePlane(m_Tiles, aFlags);
}

CRenderTileSource<CTile> CLayerTiles::TilesForRender(const CEditorMap *pRenderMap)
{
	const int Image = pRenderMap->ImageIndex(m_Image);
	const unsigned char *pFlags = Image >= 0 && static_cast<std::size_t>(Image) < pRenderMap->m_vpImages.size() && m_Color.a == 255 ? pRenderMap->m_vpImages[Image]->TileFlags().data() : nullptr;
	return CRenderTileSource<CTile>(m_Tiles, pFlags);
}

void CLayerTiles::MakePalette()
{
	for(int y = 0; y < Height(); y++)
		for(int x = 0; x < Width(); x++)
			m_Tiles.Update(y * Width() + x, [&](auto &Cell) { Cell.m_Index = y * 16 + x; });
}

void CLayerTiles::Render(const CEditorMap *pRenderMap)
{
	IGraphics::CTextureHandle Texture;
	if(m_HasGame)
	{
		Texture = Editor()->GetEntitiesTexture();
	}
	else if(m_HasFront)
	{
		Texture = Editor()->GetFrontTexture();
	}
	else if(m_HasTele)
	{
		Texture = Editor()->GetTeleTexture();
	}
	else if(m_HasSpeedup)
	{
		Texture = Editor()->GetSpeedupTexture();
	}
	else if(m_HasSwitch)
	{
		Texture = Editor()->GetSwitchTexture();
	}
	else if(m_HasTune)
	{
		Texture = Editor()->GetTuneTexture();
	}
	else if(pRenderMap->ImageIndex(m_Image) >= 0 && (size_t)pRenderMap->ImageIndex(m_Image) < pRenderMap->m_vpImages.size())
	{
		const auto &pImage = pRenderMap->m_vpImages[pRenderMap->ImageIndex(m_Image)];
		if(pImage->m_Width % 16 == 0 && pImage->m_Height % 16 == 0)
		{
			Texture = pImage->m_Texture;
		}
	}
	Graphics()->TextureSet(Texture);

	ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	pRenderMap->m_EnvelopeEvaluator.EnvelopeEval(m_ColorEnvOffset, pRenderMap->EnvelopeIndex(m_ColorEnv), ColorEnv, 4);
	const ColorRGBA Color = ColorRGBA(m_Color.r / 255.0f, m_Color.g / 255.0f, m_Color.b / 255.0f, m_Color.a / 255.0f).Multiply(ColorEnv);

	Graphics()->BlendNone();
	Editor()->RenderMap()->RenderTilemap(TilesForRender(pRenderMap), Width(), Height(), 32.0f, Color, LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	Editor()->RenderMap()->RenderTilemap(TilesForRender(pRenderMap), Width(), Height(), 32.0f, Color, LAYERRENDERFLAG_TRANSPARENT);

	// Render DDRace Layers
	if(m_RenderOverlays)
	{
		int OverlayRenderFlags = (g_Config.m_ClTextEntitiesEditor ? OVERLAYRENDERFLAG_TEXT : 0) | OVERLAYRENDERFLAG_EDITOR;
		if(m_HasTele)
			Editor()->RenderMap()->RenderTeleOverlay(static_cast<CLayerTele *>(this)->TeleTilesForRender(), Width(), Height(), 32.0f, OverlayRenderFlags);
		if(m_HasSpeedup)
			Editor()->RenderMap()->RenderSpeedupOverlay(static_cast<CLayerSpeedup *>(this)->SpeedupTilesForRender(), Width(), Height(), 32.0f, OverlayRenderFlags);
		if(m_HasSwitch)
			Editor()->RenderMap()->RenderSwitchOverlay(static_cast<CLayerSwitch *>(this)->SwitchTilesForRender(), Width(), Height(), 32.0f, OverlayRenderFlags);
		if(m_HasTune)
			Editor()->RenderMap()->RenderTuneOverlay(static_cast<CLayerTune *>(this)->TuneTilesForRender(), Width(), Height(), 32.0f, OverlayRenderFlags);
	}
}

int CLayerTiles::ConvertX(float x) const { return (int)(x / 32.0f); }
int CLayerTiles::ConvertY(float y) const { return (int)(y / 32.0f); }

void CLayerTiles::Convert(CUIRect Rect, CIntRect *pOut) const
{
	pOut->x = ConvertX(Rect.x);
	pOut->y = ConvertY(Rect.y);
	pOut->w = ConvertX(Rect.x + Rect.w + 31) - pOut->x;
	pOut->h = ConvertY(Rect.y + Rect.h + 31) - pOut->y;
}

void CLayerTiles::Snap(CUIRect *pRect) const
{
	CIntRect Out;
	Convert(*pRect, &Out);
	pRect->x = Out.x * 32.0f;
	pRect->y = Out.y * 32.0f;
	pRect->w = Out.w * 32.0f;
	pRect->h = Out.h * 32.0f;
}

void CLayerTiles::Clamp(CIntRect *pRect) const
{
	if(pRect->x < 0)
	{
		pRect->w += pRect->x;
		pRect->x = 0;
	}

	if(pRect->y < 0)
	{
		pRect->h += pRect->y;
		pRect->y = 0;
	}

	if(pRect->x + pRect->w > Width())
		pRect->w = Width() - pRect->x;

	if(pRect->y + pRect->h > Height())
		pRect->h = Height() - pRect->y;

	if(pRect->h < 0)
		pRect->h = 0;
	if(pRect->w < 0)
		pRect->w = 0;
}

bool CLayerTiles::IsEntitiesLayer() const
{
	return Map()->m_pGameLayer.get() == this || Map()->m_pTeleLayer.get() == this || Map()->m_pSpeedupLayer.get() == this || Map()->m_pFrontLayer.get() == this || Map()->m_pSwitchLayer.get() == this || Map()->m_pTuneLayer.get() == this;
}

bool CLayerTiles::IsEmpty() const
{
	for(int y = 0; y < Height(); y++)
	{
		for(int x = 0; x < Width(); x++)
		{
			if(GetTile(x, y).m_Index != 0)
			{
				return false;
			}
		}
	}
	return true;
}

void CLayerTiles::BrushSelecting(CUIRect Rect)
{
	Snap(&Rect);

	Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.4f), IGraphics::CORNER_NONE, 0.0f);

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d⨯%d", ConvertX(Rect.w), ConvertY(Rect.h));
	TextRender()->Text(Rect.x + 3.0f, Rect.y + 3.0f, Editor()->m_ShowPicker ? 15.0f : Editor()->MapView()->ScaleLength(15.0f), aBuf, -1.0f);
}

template<typename T>
static void InitGrabbedLayer(std::shared_ptr<T> &pLayer, CLayerTiles *pThisLayer)
{
	pLayer->m_Image = pThisLayer->m_Image;
	pLayer->m_HasGame = pThisLayer->m_HasGame;
	pLayer->m_HasFront = pThisLayer->m_HasFront;
	pLayer->m_HasTele = pThisLayer->m_HasTele;
	pLayer->m_HasSpeedup = pThisLayer->m_HasSpeedup;
	pLayer->m_HasSwitch = pThisLayer->m_HasSwitch;
	pLayer->m_HasTune = pThisLayer->m_HasTune;
	if(pThisLayer->Editor()->m_BrushColorEnabled)
	{
		pLayer->m_Color = pThisLayer->m_Color;
		pLayer->m_Color.a = 255;
	}
}

int CLayerTiles::BrushGrab(CLayerGroup *pBrush, CUIRect Rect)
{
	CIntRect r;
	Convert(Rect, &r);
	Clamp(&r);

	if(!r.w || !r.h)
		return 0;

	// create new layers
	if(m_HasTele)
	{
		std::shared_ptr<CLayerTele> pGrabbed = std::make_shared<CLayerTele>(pBrush->Map(), r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		for(int y = 0; y < r.h; y++)
		{
			for(int x = 0; x < r.w; x++)
			{
				// copy the tiles
				pGrabbed->m_Tiles.Set(y * pGrabbed->Width() + x, GetTile(r.x + x, r.y + y));

				// copy the tele data
				if(!Editor()->m_ShowPicker)
				{
					pGrabbed->m_TeleTiles.Set(y * pGrabbed->Width() + x, static_cast<CLayerTele *>(this)->m_TeleTiles[(r.y + y) * Width() + (r.x + x)]);
					unsigned char TgtIndex = pGrabbed->m_TeleTiles[y * pGrabbed->Width() + x].m_Type;
					if(IsValidTeleTile(TgtIndex))
					{
						if(IsTeleTileNumberUsed(TgtIndex, false))
							Editor()->m_TeleNumber = pGrabbed->m_TeleTiles[y * pGrabbed->Width() + x].m_Number;
						else if(IsTeleTileNumberUsed(TgtIndex, true))
							Editor()->m_TeleCheckpointNumber = pGrabbed->m_TeleTiles[y * pGrabbed->Width() + x].m_Number;
					}
				}
				else
				{
					const CTile &Tile = pGrabbed->m_Tiles[y * pGrabbed->Width() + x];
					if(IsValidTeleTile(Tile.m_Index) && IsTeleTileNumberUsedAny(Tile.m_Index))
					{
						pGrabbed->m_TeleTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Type = Tile.m_Index; });
						pGrabbed->m_TeleTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Number = IsTeleTileCheckpoint(Tile.m_Index) ? Editor()->m_TeleCheckpointNumber : Editor()->m_TeleNumber; });
					}
				}
			}
		}

		pGrabbed->m_TeleNumber = Editor()->m_TeleNumber;
		pGrabbed->m_TeleCheckpointNumber = Editor()->m_TeleCheckpointNumber;

		str_copy(pGrabbed->m_aFilename, pGrabbed->Map()->m_aFilename);
	}
	else if(m_HasSpeedup)
	{
		std::shared_ptr<CLayerSpeedup> pGrabbed = std::make_shared<CLayerSpeedup>(pBrush->Map(), r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		for(int y = 0; y < r.h; y++)
		{
			for(int x = 0; x < r.w; x++)
			{
				// copy the tiles
				pGrabbed->m_Tiles.Set(y * pGrabbed->Width() + x, GetTile(r.x + x, r.y + y));

				// copy the speedup data
				if(!Editor()->m_ShowPicker)
				{
					pGrabbed->m_SpeedupTiles.Set(y * pGrabbed->Width() + x, static_cast<CLayerSpeedup *>(this)->m_SpeedupTiles[(r.y + y) * Width() + (r.x + x)]);
					if(IsValidSpeedupTile(pGrabbed->m_SpeedupTiles[y * pGrabbed->Width() + x].m_Type))
					{
						Editor()->m_SpeedupAngle = pGrabbed->m_SpeedupTiles[y * pGrabbed->Width() + x].m_Angle;
						Editor()->m_SpeedupForce = pGrabbed->m_SpeedupTiles[y * pGrabbed->Width() + x].m_Force;
						Editor()->m_SpeedupMaxSpeed = pGrabbed->m_SpeedupTiles[y * pGrabbed->Width() + x].m_MaxSpeed;
					}
				}
				else
				{
					const CTile &Tile = pGrabbed->m_Tiles[y * pGrabbed->Width() + x];
					if(IsValidSpeedupTile(Tile.m_Index))
					{
						pGrabbed->m_SpeedupTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Type = Tile.m_Index; });
						pGrabbed->m_SpeedupTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Angle = Editor()->m_SpeedupAngle; });
						pGrabbed->m_SpeedupTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Force = Editor()->m_SpeedupForce; });
						pGrabbed->m_SpeedupTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_MaxSpeed = Editor()->m_SpeedupMaxSpeed; });
					}
				}
			}
		}

		pGrabbed->m_SpeedupForce = Editor()->m_SpeedupForce;
		pGrabbed->m_SpeedupMaxSpeed = Editor()->m_SpeedupMaxSpeed;
		pGrabbed->m_SpeedupAngle = Editor()->m_SpeedupAngle;
		str_copy(pGrabbed->m_aFilename, pGrabbed->Map()->m_aFilename);
	}
	else if(m_HasSwitch)
	{
		std::shared_ptr<CLayerSwitch> pGrabbed = std::make_shared<CLayerSwitch>(pBrush->Map(), r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		for(int y = 0; y < r.h; y++)
		{
			for(int x = 0; x < r.w; x++)
			{
				// copy the tiles
				pGrabbed->m_Tiles.Set(y * pGrabbed->Width() + x, GetTile(r.x + x, r.y + y));

				// copy the switch data
				if(!Editor()->m_ShowPicker)
				{
					pGrabbed->m_SwitchTiles.Set(y * pGrabbed->Width() + x, static_cast<CLayerSwitch *>(this)->m_SwitchTiles[(r.y + y) * Width() + (r.x + x)]);
					if(IsValidSwitchTile(pGrabbed->m_SwitchTiles[y * pGrabbed->Width() + x].m_Type))
					{
						Editor()->m_SwitchNumber = pGrabbed->m_SwitchTiles[y * pGrabbed->Width() + x].m_Number;
						Editor()->m_SwitchDelay = pGrabbed->m_SwitchTiles[y * pGrabbed->Width() + x].m_Delay;
					}
				}
				else
				{
					const CTile &Tile = pGrabbed->m_Tiles[y * pGrabbed->Width() + x];
					if(IsValidSwitchTile(Tile.m_Index))
					{
						pGrabbed->m_SwitchTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Type = Tile.m_Index; });
						pGrabbed->m_SwitchTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Number = Editor()->m_SwitchNumber; });
						pGrabbed->m_SwitchTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Delay = Editor()->m_SwitchDelay; });
						pGrabbed->m_SwitchTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Flags = Tile.m_Flags; });
					}
				}
			}
		}

		pGrabbed->m_SwitchNumber = Editor()->m_SwitchNumber;
		pGrabbed->m_SwitchDelay = Editor()->m_SwitchDelay;
		str_copy(pGrabbed->m_aFilename, pGrabbed->Map()->m_aFilename);
	}

	else if(m_HasTune)
	{
		std::shared_ptr<CLayerTune> pGrabbed = std::make_shared<CLayerTune>(pBrush->Map(), r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
		{
			for(int x = 0; x < r.w; x++)
			{
				pGrabbed->m_Tiles.Set(y * pGrabbed->Width() + x, GetTile(r.x + x, r.y + y));

				if(!Editor()->m_ShowPicker)
				{
					pGrabbed->m_TuneTiles.Set(y * pGrabbed->Width() + x, static_cast<CLayerTune *>(this)->m_TuneTiles[(r.y + y) * Width() + (r.x + x)]);
					if(IsValidTuneTile(pGrabbed->m_TuneTiles[y * pGrabbed->Width() + x].m_Type))
					{
						Editor()->m_TuningNumber = pGrabbed->m_TuneTiles[y * pGrabbed->Width() + x].m_Number;
					}
				}
				else
				{
					const CTile &Tile = pGrabbed->m_Tiles[y * pGrabbed->Width() + x];
					if(IsValidTuneTile(Tile.m_Index))
					{
						pGrabbed->m_TuneTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Type = Tile.m_Index; });
						pGrabbed->m_TuneTiles.Update(y * pGrabbed->Width() + x, [&](auto &Cell) { Cell.m_Number = Editor()->m_TuningNumber; });
					}
				}
			}
		}

		pGrabbed->m_TuningNumber = Editor()->m_TuningNumber;
		str_copy(pGrabbed->m_aFilename, pGrabbed->Map()->m_aFilename);
	}
	else // game, front and tiles layers
	{
		std::shared_ptr<CLayerTiles> pGrabbed;
		if(m_HasGame)
		{
			pGrabbed = std::make_shared<CLayerGame>(pBrush->Map(), r.w, r.h);
		}
		else if(m_HasFront)
		{
			pGrabbed = std::make_shared<CLayerFront>(pBrush->Map(), r.w, r.h);
		}
		else
		{
			pGrabbed = std::make_shared<CLayerTiles>(pBrush->Map(), r.w, r.h);
		}
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_Tiles.Set(y * pGrabbed->Width() + x, GetTile(r.x + x, r.y + y));
		str_copy(pGrabbed->m_aFilename, pGrabbed->Map()->m_aFilename);
	}

	return 1;
}

void CLayerTiles::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerTiles *pLt = static_cast<CLayerTiles *>(pBrush);

	bool Destructive = Editor()->m_BrushDrawDestructive || Empty || pLt->IsEmpty();

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			bool HasTile = GetTile(fx, fy).m_Index;
			if(!Empty && pLt->GetTile(x % pLt->Width(), y % pLt->Height()).m_Index == TILE_THROUGH_CUT)
			{
				if(m_HasGame && Map()->m_pFrontLayer)
				{
					HasTile = HasTile || Map()->m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_HasFront)
				{
					HasTile = HasTile || Map()->m_pGameLayer->GetTile(fx, fy).m_Index;
				}
			}

			if(!Destructive && HasTile)
				continue;

			SetTile(fx, fy, Empty ? CTile{TILE_AIR} : static_cast<CTile>(pLt->m_Tiles[(y * pLt->Width() + x % pLt->Width()) % (pLt->Width() * pLt->Height())]));
		}
	}
	FlagModified(sx, sy, w, h);
}

void CLayerTiles::BrushDraw(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerTiles *pTileLayer = static_cast<CLayerTiles *>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);

	bool Destructive = Editor()->m_BrushDrawDestructive || pTileLayer->IsEmpty();

	for(int y = 0; y < pTileLayer->Height(); y++)
		for(int x = 0; x < pTileLayer->Width(); x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			bool HasTile = GetTile(fx, fy).m_Index;
			if(pTileLayer->CLayerTiles::GetTile(x, y).m_Index == TILE_THROUGH_CUT)
			{
				if(m_HasGame && Map()->m_pFrontLayer)
				{
					HasTile = HasTile || Map()->m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_HasFront)
				{
					HasTile = HasTile || Map()->m_pGameLayer->GetTile(fx, fy).m_Index;
				}
			}

			if(!Destructive && HasTile)
				continue;

			SetTile(fx, fy, pTileLayer->CLayerTiles::GetTile(x, y));
		}

	FlagModified(sx, sy, pTileLayer->Width(), pTileLayer->Height());
}

void CLayerTiles::BrushFlipX()
{
	m_Tiles.FlipX();

	if(m_HasTele || m_HasSpeedup || m_HasTune)
		return;

	bool Rotate = !(m_HasGame || m_HasFront || m_HasSwitch) || Editor()->IsAllowPlaceUnusedTiles();
	for(int y = 0; y < Height(); y++)
		for(int x = 0; x < Width(); x++)
			if(!Rotate && !IsRotatableTile(m_Tiles[y * Width() + x].m_Index))
				m_Tiles.Update(y * Width() + x, [&](auto &Cell) { Cell.m_Flags = 0; });
			else
				m_Tiles.Update(y * Width() + x, [&](auto &Cell) { Cell.m_Flags ^= (m_Tiles[y * Width() + x].m_Flags & TILEFLAG_ROTATE) ? TILEFLAG_YFLIP : TILEFLAG_XFLIP; });
}

void CLayerTiles::BrushFlipY()
{
	m_Tiles.FlipY();

	if(m_HasTele || m_HasSpeedup || m_HasTune)
		return;

	bool Rotate = !(m_HasGame || m_HasFront || m_HasSwitch) || Editor()->IsAllowPlaceUnusedTiles();
	for(int y = 0; y < Height(); y++)
		for(int x = 0; x < Width(); x++)
			if(!Rotate && !IsRotatableTile(m_Tiles[y * Width() + x].m_Index))
				m_Tiles.Update(y * Width() + x, [&](auto &Cell) { Cell.m_Flags = 0; });
			else
				m_Tiles.Update(y * Width() + x, [&](auto &Cell) { Cell.m_Flags ^= (m_Tiles[y * Width() + x].m_Flags & TILEFLAG_ROTATE) ? TILEFLAG_XFLIP : TILEFLAG_YFLIP; });
}

void CLayerTiles::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		m_Tiles.RotateClockwise();
		bool Rotate = !(m_HasGame || m_HasFront) || Editor()->IsAllowPlaceUnusedTiles();
		for(std::size_t Index = 0; Index < m_Tiles.Size(); ++Index)
			m_Tiles.Update(Index, [&](auto &Tile) {
				if(!Rotate && !IsRotatableTile(Tile.m_Index))
					Tile.m_Flags = 0;
				else
				{
					RotateTileFlagsClockwise(Tile);
				}
			});
	}

	if(Rotation == 2 || Rotation == 3)
	{
		BrushFlipX();
		BrushFlipY();
	}
}

std::shared_ptr<CLayer> CLayerTiles::Duplicate() const
{
	return std::make_shared<CLayerTiles>(*this);
}

const char *CLayerTiles::TypeName() const
{
	return "tiles";
}

void CLayerTiles::Resize(int NewW, int NewH)
{
	m_Tiles.Resize(NewW, NewH);

	// resize tele layer if available
	if(m_HasGame && Map()->m_pTeleLayer && (Map()->m_pTeleLayer->Width() != NewW || Map()->m_pTeleLayer->Height() != NewH))
		Map()->m_pTeleLayer->Resize(NewW, NewH);

	// resize speedup layer if available
	if(m_HasGame && Map()->m_pSpeedupLayer && (Map()->m_pSpeedupLayer->Width() != NewW || Map()->m_pSpeedupLayer->Height() != NewH))
		Map()->m_pSpeedupLayer->Resize(NewW, NewH);

	// resize front layer
	if(m_HasGame && Map()->m_pFrontLayer && (Map()->m_pFrontLayer->Width() != NewW || Map()->m_pFrontLayer->Height() != NewH))
		Map()->m_pFrontLayer->Resize(NewW, NewH);

	// resize switch layer if available
	if(m_HasGame && Map()->m_pSwitchLayer && (Map()->m_pSwitchLayer->Width() != NewW || Map()->m_pSwitchLayer->Height() != NewH))
		Map()->m_pSwitchLayer->Resize(NewW, NewH);

	// resize tune layer if available
	if(m_HasGame && Map()->m_pTuneLayer && (Map()->m_pTuneLayer->Width() != NewW || Map()->m_pTuneLayer->Height() != NewH))
		Map()->m_pTuneLayer->Resize(NewW, NewH);
}

void CLayerTiles::Shift(EShiftDirection Direction)
{
	ShiftImpl(m_Tiles, Direction, Map()->m_ShiftBy);
}

void CLayerTiles::ShowInfo()
{
	CScreenRect ScreenRect = Graphics()->GetScreen();
	Graphics()->TextureSet(Editor()->Client()->GetDebugFont());
	Graphics()->QuadsBegin();

	int StartY = std::max(0, (int)(ScreenRect.m_TopLeft.y / 32.0f) - 1);
	int StartX = std::max(0, (int)(ScreenRect.m_TopLeft.x / 32.0f) - 1);
	int EndY = std::min((int)(ScreenRect.m_BottomRight.y / 32.0f) + 1, Height());
	int EndX = std::min((int)(ScreenRect.m_BottomRight.x / 32.0f) + 1, Width());

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int c = x + y * Width();
			if(m_Tiles[c].m_Index)
			{
				char aBuf[4];
				if(Editor()->m_ShowTileInfo == CEditor::SHOW_TILE_HEXADECIMAL)
				{
					str_hex(aBuf, sizeof(aBuf), &m_Tiles[c].m_Index, 1);
					aBuf[2] = '\0'; // would otherwise be a space
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "%d", m_Tiles[c].m_Index);
				}
				Graphics()->QuadsText(x * 32, y * 32, 16.0f, aBuf);

				char aFlags[4] = {m_Tiles[c].m_Flags & TILEFLAG_XFLIP ? 'X' : ' ',
					m_Tiles[c].m_Flags & TILEFLAG_YFLIP ? 'Y' : ' ',
					m_Tiles[c].m_Flags & TILEFLAG_ROTATE ? 'R' : ' ',
					0};
				Graphics()->QuadsText(x * 32, y * 32 + 16, 16.0f, aFlags);
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CLayerTiles::FillGameTiles(EGameTileOp Fill)
{
	if(!CanFillGameTiles())
		return;

	auto GameTileOpToIndex = [](EGameTileOp Op) -> int {
		switch(Op)
		{
		case EGameTileOp::AIR: return TILE_AIR;
		case EGameTileOp::HOOKABLE: return TILE_SOLID;
		case EGameTileOp::DEATH: return TILE_DEATH;
		case EGameTileOp::UNHOOKABLE: return TILE_NOHOOK;
		case EGameTileOp::HOOKTHROUGH: return TILE_THROUGH_CUT;
		case EGameTileOp::FREEZE: return TILE_FREEZE;
		case EGameTileOp::UNFREEZE: return TILE_UNFREEZE;
		case EGameTileOp::DEEP_FREEZE: return TILE_DFREEZE;
		case EGameTileOp::DEEP_UNFREEZE: return TILE_DUNFREEZE;
		case EGameTileOp::BLUE_CHECK_TELE: return TILE_TELECHECKIN;
		case EGameTileOp::RED_CHECK_TELE: return TILE_TELECHECKINEVIL;
		case EGameTileOp::LIVE_FREEZE: return TILE_LFREEZE;
		case EGameTileOp::LIVE_UNFREEZE: return TILE_LUNFREEZE;
		default: return -1;
		}
	};

	const int Result = GameTileOpToIndex(Fill);
	if(Result < 0)
		return;
	Map()->m_DocumentHistory.Edit(this, "Construct game tiles", editor_history::ECategory::MAP, [&] {
		const auto pGroup = Map()->SelectedGroup();
		m_FillGameTile = Result;
		const int OffsetX = -pGroup->m_OffsetX / 32;
		const int OffsetY = -pGroup->m_OffsetY / 32;
		const bool Tele = Result == TILE_TELECHECKIN || Result == TILE_TELECHECKINEVIL;
		if(Tele && !Map()->m_pTeleLayer)
		{
			auto pLayer = std::make_shared<CLayerTele>(Map(), Map()->m_pGameLayer->Width(), Map()->m_pGameLayer->Height());
			Map()->MakeTeleLayer(pLayer);
			Map()->m_pGameGroup->AddLayer(pLayer);
		}
		auto pTarget = Tele ? std::static_pointer_cast<CLayerTiles>(Map()->m_pTeleLayer) : Map()->m_pGameLayer;
		const int NewWidth = std::max(pTarget->Width(), Width() + OffsetX);
		const int NewHeight = std::max(pTarget->Height(), Height() + OffsetY);
		if(NewWidth != pTarget->Width() || NewHeight != pTarget->Height())
			pTarget->Resize(NewWidth, NewHeight);
		for(int y = std::max(0, -OffsetY); y < Height(); ++y)
			for(int x = std::max(0, -OffsetX); x < Width(); ++x)
				if(GetTile(x, y).m_Index != 0)
				{
					const int TileIndex = (y + OffsetY) * pTarget->Width() + x + OffsetX;
					pTarget->SetTile(x + OffsetX, y + OffsetY, CTile{static_cast<unsigned char>(Result)});
					if(Tele)
						Map()->m_pTeleLayer->m_TeleTiles.Update(TileIndex, [&](auto &Cell) {
							Cell.m_Number = 1;
							Cell.m_Type = Result;
						});
				}
		Map()->OnModify();
	});
}

bool CLayerTiles::CanFillGameTiles() const
{
	const bool EntitiesLayer = IsEntitiesLayer();
	if(EntitiesLayer)
		return false;

	std::shared_ptr<CLayerGroup> pGroup = Map()->m_vpGroups[Map()->m_SelectedGroup];

	// Game tiles can only be constructed if the layer is relative to the game layer
	return !(pGroup->m_OffsetX % 32) && !(pGroup->m_OffsetY % 32) && pGroup->m_ParallaxX == 100 && pGroup->m_ParallaxY == 100;
}

CUi::EPopupMenuFunctionResult CLayerTiles::RenderProperties(CUIRect *pToolBox)
{
	CUIRect Button;

	const bool EntitiesLayer = IsEntitiesLayer();

	if(CanFillGameTiles())
	{
		pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
		static int s_GameTilesButton = 0;

		auto GameTileToOp = [](int TileIndex) -> EGameTileOp {
			switch(TileIndex)
			{
			case TILE_AIR: return EGameTileOp::AIR;
			case TILE_SOLID: return EGameTileOp::HOOKABLE;
			case TILE_DEATH: return EGameTileOp::DEATH;
			case TILE_NOHOOK: return EGameTileOp::UNHOOKABLE;
			case TILE_THROUGH_CUT: return EGameTileOp::HOOKTHROUGH;
			case TILE_FREEZE: return EGameTileOp::FREEZE;
			case TILE_UNFREEZE: return EGameTileOp::UNFREEZE;
			case TILE_DFREEZE: return EGameTileOp::DEEP_FREEZE;
			case TILE_DUNFREEZE: return EGameTileOp::DEEP_UNFREEZE;
			case TILE_TELECHECKIN: return EGameTileOp::BLUE_CHECK_TELE;
			case TILE_TELECHECKINEVIL: return EGameTileOp::RED_CHECK_TELE;
			case TILE_LFREEZE: return EGameTileOp::LIVE_FREEZE;
			case TILE_LUNFREEZE: return EGameTileOp::LIVE_UNFREEZE;
			default: return EGameTileOp::AIR;
			}
		};

		char aBuf[128] = "Game tiles";
		if(m_LiveGameTiles)
		{
			auto TileOp = GameTileToOp(m_FillGameTile);
			if(TileOp != EGameTileOp::AIR)
				str_format(aBuf, sizeof(aBuf), "Game tiles: %s", GAME_TILE_OP_NAMES[(size_t)TileOp]);
		}
		if(Editor()->DoButton_Editor(&s_GameTilesButton, aBuf, 0, &Button, BUTTONFLAG_LEFT, "Construct game tiles from this layer."))
			Editor()->PopupSelectGametileOpInvoke(Editor()->Ui()->MouseX(), Editor()->Ui()->MouseY());
		const int Selected = Editor()->PopupSelectGameTileOpResult();
		FillGameTiles((EGameTileOp)Selected);
	}

	if(Map()->m_pGameLayer.get() != this)
	{
		if(Map()->ImageIndex(m_Image) >= 0 && (size_t)Map()->ImageIndex(m_Image) < Map()->m_vpImages.size() && Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Automapper.IsLoaded() && m_AutomapperConfig != -1)
		{
			pToolBox->HSplitBottom(2.0f, pToolBox, nullptr);
			pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
			if(m_Seed != 0)
			{
				CUIRect ButtonAuto;
				Button.VSplitRight(16.0f, &Button, &ButtonAuto);
				Button.VSplitRight(2.0f, &Button, nullptr);
				static int s_AutomapperButtonAuto = 0;
				if(Editor()->DoButton_Editor(&s_AutomapperButtonAuto, "A", m_AutoAutomapper, &ButtonAuto, BUTTONFLAG_LEFT, "Automatically run the automapper after modifications."))
				{
					Map()->m_DocumentHistory.Edit(&s_AutomapperButtonAuto, "Automatic automapping", editor_history::ECategory::MAP, [&] {
						m_AutoAutomapper = !m_AutoAutomapper;
						FlagModified(0, 0, Width(), Height());
					});
				}
			}

			static int s_AutomapperButton = 0;
			if(Editor()->DoButton_Editor(&s_AutomapperButton, "Automap", 0, &Button, BUTTONFLAG_LEFT, "Run the automapper."))
			{
				Map()->m_DocumentHistory.Edit(&s_AutomapperButton, "Automap", editor_history::ECategory::MAP, [&] {
					Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Automapper.Proceed(this, Map()->m_pGameLayer.get(), m_AutomapperReference, m_AutomapperConfig, m_Seed);
				});
				return CUi::POPUP_CLOSE_CURRENT;
			}
		}
	}

	CProperty aProps[] = {
		{"Width", Width(), PROPTYPE_INT, 2, 100000},
		{"Height", Height(), PROPTYPE_INT, 2, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", Map()->m_ShiftBy, PROPTYPE_INT, 1, 100000},
		{"Image", Map()->ImageIndex(m_Image), PROPTYPE_IMAGE, 0, 0},
		{"Color", PackColor(m_Color), PROPTYPE_COLOR, 0, 0},
		{"Color Env", Map()->EnvelopeIndex(m_ColorEnv) + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Color TO", m_ColorEnvOffset, PROPTYPE_INT, -1000000, 1000000},
		{"Auto Rule", m_AutomapperConfig, PROPTYPE_AUTOMAPPER, Map()->ImageIndex(m_Image), 0},
		{"Reference", m_AutomapperReference, PROPTYPE_AUTOMAPPER_REFERENCE, 0, 0},
		{"Live Gametiles", m_LiveGameTiles, PROPTYPE_BOOL, 0, 1},
		{"Seed", m_Seed, PROPTYPE_INT, 0, 1000000000},
		{nullptr},
	};

	if(EntitiesLayer) // remove the image and color properties if this is a game layer
	{
		aProps[(int)ETilesProp::IMAGE].m_pName = nullptr;
		aProps[(int)ETilesProp::COLOR].m_pName = nullptr;
		aProps[(int)ETilesProp::AUTOMAPPER].m_pName = nullptr;
		aProps[(int)ETilesProp::AUTOMAPPER_REFERENCE].m_pName = nullptr;
	}
	if(Map()->ImageIndex(m_Image) == -1)
	{
		aProps[(int)ETilesProp::AUTOMAPPER].m_pName = nullptr;
		aProps[(int)ETilesProp::AUTOMAPPER_REFERENCE].m_pName = nullptr;
		aProps[(int)ETilesProp::SEED].m_pName = nullptr;
	}

	static int s_aIds[(int)ETilesProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [State, Prop] = Editor()->DoPropertiesWithState<ETilesProp>(pToolBox, aProps, s_aIds, &NewVal);

	if(Map()->m_DocumentHistory.BeginControl(s_aIds, "Edit tile layer", State))
	{
		Map()->m_DocumentHistory.Update(s_aIds, [&] {
			if(Prop == ETilesProp::WIDTH)
			{
				if(NewVal > 1000 && !Editor()->m_LargeLayerWasWarned)
				{
					Editor()->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
					Editor()->m_PopupEventActivated = true;
					Editor()->m_LargeLayerWasWarned = true;
				}
				Resize(NewVal, Height());
			}
			else if(Prop == ETilesProp::HEIGHT)
			{
				if(NewVal > 1000 && !Editor()->m_LargeLayerWasWarned)
				{
					Editor()->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
					Editor()->m_PopupEventActivated = true;
					Editor()->m_LargeLayerWasWarned = true;
				}
				Resize(Width(), NewVal);
			}
			else if(Prop == ETilesProp::SHIFT)
			{
				Shift((EShiftDirection)NewVal);
			}
			else if(Prop == ETilesProp::SHIFT_BY)
			{
				Map()->m_ShiftBy = NewVal;
			}
			else if(Prop == ETilesProp::IMAGE)
			{
				m_Image = Map()->ImageReference(NewVal);
				if(NewVal == -1)
				{
					m_Image = Map()->ImageReference(-1);
				}
				else
				{
					m_Image = Map()->ImageReference(NewVal % Map()->m_vpImages.size());
					m_AutomapperConfig = -1;

					if(Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Width % 16 != 0 || Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Height % 16 != 0)
					{
						Editor()->m_PopupEventType = CEditor::POPEVENT_IMAGEDIV16;
						Editor()->m_PopupEventActivated = true;
						m_Image = Map()->ImageReference(-1);
					}
				}
			}
			else if(Prop == ETilesProp::COLOR)
			{
				m_Color = UnpackColor(NewVal);
			}
			else if(Prop == ETilesProp::COLOR_ENV)
			{
				int Index = std::clamp(NewVal - 1, -1, (int)Map()->m_vpEnvelopes.size() - 1);
				const int Step = (Index - Map()->EnvelopeIndex(m_ColorEnv)) % 2;
				if(Step != 0)
				{
					for(; Index >= -1 && Index < (int)Map()->m_vpEnvelopes.size(); Index += Step)
					{
						if(Index == -1 || Map()->m_vpEnvelopes[Index]->GetChannels() == 4)
						{
							m_ColorEnv = Map()->EnvelopeReference(Index);
							break;
						}
					}
				}
			}
			else if(Prop == ETilesProp::COLOR_ENV_OFFSET)
			{
				m_ColorEnvOffset = NewVal;
			}
			else if(Prop == ETilesProp::SEED)
			{
				m_Seed = NewVal;
			}
			else if(Prop == ETilesProp::AUTOMAPPER)
			{
				if(Map()->ImageIndex(m_Image) >= 0 && Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Automapper.ConfigNamesNum() > 0 && NewVal >= 0)
					m_AutomapperConfig = NewVal % Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Automapper.ConfigNamesNum();
				else
					m_AutomapperConfig = -1;
			}
			else if(Prop == ETilesProp::AUTOMAPPER_REFERENCE)
			{
				m_AutomapperReference = NewVal;
			}
			else if(Prop == ETilesProp::LIVE_GAMETILES)
			{
				m_LiveGameTiles = NewVal != 0;
			}

			// Check if modified property could have an effect on automapper
			if((State == EEditState::END || State == EEditState::ONE_GO) && HasAutomapEffect(Prop))
			{
				FlagModified(0, 0, Width(), Height());
			}
		});
		Map()->m_DocumentHistory.EndControl(s_aIds, State);
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CLayerTiles::RenderCommonProperties(SCommonPropState &State, CEditorMap *pEditorMap, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers, std::vector<int> &vLayerIndices)
{
	CEditor *pEditor = pEditorMap->Editor();
	if(State.m_Modified)
	{
		CUIRect Commit;
		pToolbox->HSplitBottom(20.0f, pToolbox, &Commit);
		static int s_CommitButton = 0;
		if(pEditor->DoButton_Editor(&s_CommitButton, "Commit", 0, &Commit, BUTTONFLAG_LEFT, "Apply the changes."))
		{
			pEditorMap->m_DocumentHistory.Edit(&s_CommitButton, "Edit selected layers", editor_history::ECategory::MAP, [&] {
				for(auto &pLayer : vpLayers)
				{
					if((State.m_Modified & SCommonPropState::MODIFIED_SIZE) != 0)
						pLayer->Resize(State.m_Width, State.m_Height);
					if((State.m_Modified & SCommonPropState::MODIFIED_COLOR) != 0 && !pLayer->IsEntitiesLayer())
						pLayer->m_Color = UnpackColor(State.m_Color);
					pLayer->FlagModified(0, 0, pLayer->Width(), pLayer->Height());
				}
			});
			State.m_Modified = 0;
		}
	}
	else
	{
		for(auto &pLayer : vpLayers)
		{
			if(pLayer->Width() > State.m_Width)
				State.m_Width = pLayer->Width();
			if(pLayer->Height() > State.m_Height)
				State.m_Height = pLayer->Height();
		}

		State.m_Color = PackColor(vpLayers[0]->m_Color);
	}

	{
		CUIRect Warning;
		pToolbox->HSplitTop(13.0f, &Warning, pToolbox);
		Warning.HMargin(0.5f, &Warning);

		pEditor->TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		SLabelProperties Props;
		Props.m_MaxWidth = Warning.w;
		pEditor->Ui()->DoLabel(&Warning, "Editing multiple layers", 9.0f, TEXTALIGN_ML, Props);
		pEditor->TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		pToolbox->HSplitTop(2.0f, nullptr, pToolbox);
	}

	CProperty aProps[] = {
		{"Width", State.m_Width, PROPTYPE_INT, 2, 100000},
		{"Height", State.m_Height, PROPTYPE_INT, 2, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", pEditorMap->m_ShiftBy, PROPTYPE_INT, 1, 100000},
		{"Color", State.m_Color, PROPTYPE_COLOR, 0, 0},
		{nullptr},
	};

	static int s_aIds[(int)ETilesCommonProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [PropState, Prop] = pEditor->DoPropertiesWithState<ETilesCommonProp>(pToolbox, aProps, s_aIds, &NewVal);

	if(Prop == ETilesCommonProp::WIDTH)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Width = NewVal;
	}
	else if(Prop == ETilesCommonProp::HEIGHT)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Height = NewVal;
	}
	else if(Prop == ETilesCommonProp::SHIFT)
	{
		pEditorMap->m_DocumentHistory.Edit(s_aIds, "Shift selected layers", editor_history::ECategory::MAP, [&] {
			for(auto &pLayer : vpLayers)
			{
				pLayer->Shift((EShiftDirection)NewVal);
				pLayer->FlagModified(0, 0, pLayer->Width(), pLayer->Height());
			}
		});
	}
	else if(Prop == ETilesCommonProp::SHIFT_BY)
	{
		pEditorMap->m_ShiftBy = NewVal;
	}
	else if(Prop == ETilesCommonProp::COLOR)
	{
		State.m_Color = NewVal;
	}

	if(PropState == EEditState::END || PropState == EEditState::ONE_GO)
	{
		if(Prop == ETilesCommonProp::WIDTH || Prop == ETilesCommonProp::HEIGHT)
		{
			State.m_Modified |= SCommonPropState::MODIFIED_SIZE;
		}
		else if(Prop == ETilesCommonProp::COLOR)
		{
			State.m_Modified |= SCommonPropState::MODIFIED_COLOR;
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CLayerTiles::FlagModified(int x, int y, int w, int h)
{
	Map()->OnModify();
	if(m_Seed != 0 && m_AutomapperConfig != -1 && m_AutoAutomapper && Map()->ImageIndex(m_Image) >= 0)
	{
		Map()->m_vpImages[Map()->ImageIndex(m_Image)]->m_Automapper.ProceedLocalized(this, Map()->m_pGameLayer.get(), m_AutomapperReference, m_AutomapperConfig, m_Seed, x, y, w, h);
	}
}

bool CLayerTiles::IsEnvelopeUsed(int EnvelopeIndex) const
{
	return Map()->EnvelopeIndex(m_ColorEnv) == EnvelopeIndex;
}

bool CLayerTiles::IsImageUsed(int ImageIndex) const
{
	return Map()->ImageIndex(m_Image) == ImageIndex;
}

void CLayerTiles::VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	ReferenceFunction(m_Image);
}

void CLayerTiles::VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	ReferenceFunction(m_ColorEnv);
}

void CLayerTiles::ShowPreventUnusedTilesWarning()
{
	if(!Editor()->m_PreventUnusedTilesWasWarned)
	{
		Editor()->m_PopupEventType = CEditor::POPEVENT_PREVENTUNUSEDTILES;
		Editor()->m_PopupEventActivated = true;
		Editor()->m_PreventUnusedTilesWasWarned = true;
	}
}

bool CLayerTiles::HasAutomapEffect(ETilesProp Prop)
{
	switch(Prop)
	{
	case ETilesProp::WIDTH:
	case ETilesProp::HEIGHT:
	case ETilesProp::SHIFT:
	case ETilesProp::IMAGE:
	case ETilesProp::AUTOMAPPER:
	case ETilesProp::SEED:
		return true;
	default:
		return false;
	}
	return false;
}
