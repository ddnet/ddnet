/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_tiles.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/map.h>
#include <engine/textrender.h>
#include <game/editor/editor.h>

#include "layer_front.h"
#include "layer_game.h"
#include "layer_group.h"
#include "layer_quads.h"
#include "layer_speedup.h"
#include "layer_switch.h"
#include "layer_tele.h"
#include "layer_tune.h"
#include <game/client/render.h>

#include <engine/input.h>
#include <engine/keys.h>

CLayerTiles::CLayerTiles(int w, int h)
{
	m_Type = LAYERTYPE_TILES;
	m_aName[0] = '\0';
	m_Width = w;
	m_Height = h;
	m_Image = -1;
	m_Game = 0;
	m_Color.r = 255;
	m_Color.g = 255;
	m_Color.b = 255;
	m_Color.a = 255;
	m_ColorEnv = -1;
	m_ColorEnvOffset = 0;

	m_Tele = 0;
	m_Speedup = 0;
	m_Front = 0;
	m_Switch = 0;
	m_Tune = 0;
	m_AutoMapperConfig = -1;
	m_Seed = 0;
	m_AutoAutoMap = false;

	m_pTiles = new CTile[m_Width * m_Height];
	mem_zero(m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
}

CLayerTiles::CLayerTiles(const CLayerTiles &Other) :
	CLayer(Other)
{
	m_Width = Other.m_Width;
	m_Height = Other.m_Height;
	m_pTiles = new CTile[m_Width * m_Height];
	mem_copy(m_pTiles, Other.m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));

	m_Image = Other.m_Image;
	m_Game = Other.m_Game;
	m_Color = Other.m_Color;
	m_ColorEnv = Other.m_ColorEnv;
	m_ColorEnvOffset = Other.m_ColorEnvOffset;

	m_AutoMapperConfig = Other.m_AutoMapperConfig;
	m_Seed = Other.m_Seed;
	m_AutoAutoMap = Other.m_AutoAutoMap;
	m_Tele = Other.m_Tele;
	m_Speedup = Other.m_Speedup;
	m_Front = Other.m_Front;
	m_Switch = Other.m_Switch;
	m_Tune = Other.m_Tune;

	mem_copy(m_aFileName, Other.m_aFileName, IO_MAX_PATH_LENGTH);
}

CLayerTiles::~CLayerTiles()
{
	delete[] m_pTiles;
}

CTile CLayerTiles::GetTile(int x, int y)
{
	return m_pTiles[y * m_Width + x];
}

void CLayerTiles::SetTile(int x, int y, CTile Tile)
{
	m_pTiles[y * m_Width + x] = Tile;
}

void CLayerTiles::PrepareForSave()
{
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pTiles[y * m_Width + x].m_Flags &= TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE;

	if(m_Image != -1 && m_Color.a == 255)
	{
		for(int y = 0; y < m_Height; y++)
			for(int x = 0; x < m_Width; x++)
				m_pTiles[y * m_Width + x].m_Flags |= Editor()->m_Map.m_vpImages[m_Image]->m_aTileFlags[m_pTiles[y * m_Width + x].m_Index];
	}
}

void CLayerTiles::ExtractTiles(int TilemapItemVersion, const CTile *pSavedTiles, size_t SavedTilesSize)
{
	const size_t DestSize = (size_t)m_Width * m_Height;
	if(TilemapItemVersion >= CMapItemLayerTilemap::TILE_SKIP_MIN_VERSION)
		CMap::ExtractTiles(m_pTiles, DestSize, pSavedTiles, SavedTilesSize);
	else if(SavedTilesSize >= DestSize)
		mem_copy(m_pTiles, pSavedTiles, DestSize * sizeof(CTile));
}

void CLayerTiles::MakePalette()
{
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pTiles[y * m_Width + x].m_Index = y * 16 + x;
}

void CLayerTiles::Render(bool Tileset)
{
	IGraphics::CTextureHandle Texture;
	if(m_Image >= 0 && (size_t)m_Image < Editor()->m_Map.m_vpImages.size())
		Texture = Editor()->m_Map.m_vpImages[m_Image]->m_Texture;
	else if(m_Game)
		Texture = Editor()->GetEntitiesTexture();
	else if(m_Front)
		Texture = Editor()->GetFrontTexture();
	else if(m_Tele)
		Texture = Editor()->GetTeleTexture();
	else if(m_Speedup)
		Texture = Editor()->GetSpeedupTexture();
	else if(m_Switch)
		Texture = Editor()->GetSwitchTexture();
	else if(m_Tune)
		Texture = Editor()->GetTuneTexture();
	Graphics()->TextureSet(Texture);

	ColorRGBA Color = ColorRGBA(m_Color.r / 255.0f, m_Color.g / 255.0f, m_Color.b / 255.0f, m_Color.a / 255.0f);
	Graphics()->BlendNone();
	RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_OPAQUE,
		Editor()->EnvelopeEval, Editor(), m_ColorEnv, m_ColorEnvOffset);
	Graphics()->BlendNormal();
	RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_TRANSPARENT,
		Editor()->EnvelopeEval, Editor(), m_ColorEnv, m_ColorEnvOffset);

	// Render DDRace Layers
	if(!Tileset)
	{
		if(m_Tele)
			RenderTools()->RenderTeleOverlay(static_cast<CLayerTele *>(this)->m_pTeleTile, m_Width, m_Height, 32.0f);
		if(m_Speedup)
			RenderTools()->RenderSpeedupOverlay(static_cast<CLayerSpeedup *>(this)->m_pSpeedupTile, m_Width, m_Height, 32.0f);
		if(m_Switch)
			RenderTools()->RenderSwitchOverlay(static_cast<CLayerSwitch *>(this)->m_pSwitchTile, m_Width, m_Height, 32.0f);
		if(m_Tune)
			RenderTools()->RenderTuneOverlay(static_cast<CLayerTune *>(this)->m_pTuneTile, m_Width, m_Height, 32.0f);
	}
}

int CLayerTiles::ConvertX(float x) const { return (int)(x / 32.0f); }
int CLayerTiles::ConvertY(float y) const { return (int)(y / 32.0f); }

void CLayerTiles::Convert(CUIRect Rect, RECTi *pOut)
{
	pOut->x = ConvertX(Rect.x);
	pOut->y = ConvertY(Rect.y);
	pOut->w = ConvertX(Rect.x + Rect.w + 31) - pOut->x;
	pOut->h = ConvertY(Rect.y + Rect.h + 31) - pOut->y;
}

void CLayerTiles::Snap(CUIRect *pRect)
{
	RECTi Out;
	Convert(*pRect, &Out);
	pRect->x = Out.x * 32.0f;
	pRect->y = Out.y * 32.0f;
	pRect->w = Out.w * 32.0f;
	pRect->h = Out.h * 32.0f;
}

void CLayerTiles::Clamp(RECTi *pRect)
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

	if(pRect->x + pRect->w > m_Width)
		pRect->w = m_Width - pRect->x;

	if(pRect->y + pRect->h > m_Height)
		pRect->h = m_Height - pRect->y;

	if(pRect->h < 0)
		pRect->h = 0;
	if(pRect->w < 0)
		pRect->w = 0;
}

bool CLayerTiles::IsEntitiesLayer() const
{
	return Editor()->m_Map.m_pGameLayer.get() == this || Editor()->m_Map.m_pTeleLayer.get() == this || Editor()->m_Map.m_pSpeedupLayer.get() == this || Editor()->m_Map.m_pFrontLayer.get() == this || Editor()->m_Map.m_pSwitchLayer.get() == this || Editor()->m_Map.m_pTuneLayer.get() == this;
}

bool CLayerTiles::IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
	{
		for(int x = 0; x < pLayer->m_Width; x++)
		{
			int Index = pLayer->GetTile(x, y).m_Index;
			if(Index)
			{
				if(pLayer->m_Game)
				{
					if(Editor()->m_AllowPlaceUnusedTiles || IsValidGameTile(Index))
						return false;
				}
				else if(pLayer->m_Front)
				{
					if(Editor()->m_AllowPlaceUnusedTiles || IsValidFrontTile(Index))
						return false;
				}
				else
					return false;
			}
		}
	}

	return true;
}

void CLayerTiles::BrushSelecting(CUIRect Rect)
{
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	Snap(&Rect);
	IGraphics::CQuadItem QuadItem(Rect.x, Rect.y, Rect.w, Rect.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d,%d", ConvertX(Rect.w), ConvertY(Rect.h));
	TextRender()->Text(Rect.x + 3.0f, Rect.y + 3.0f, Editor()->m_ShowPicker ? 15.0f : Editor()->MapView()->ScaleLength(15.0f), aBuf, -1.0f);
}

template<typename T>
static void InitGrabbedLayer(std::shared_ptr<T> pLayer, CLayerTiles *pThisLayer)
{
	pLayer->Init(pThisLayer->Editor());
	pLayer->m_Image = pThisLayer->m_Image;
	pLayer->m_Game = pThisLayer->m_Game;
	pLayer->m_Front = pThisLayer->m_Front;
	pLayer->m_Tele = pThisLayer->m_Tele;
	pLayer->m_Speedup = pThisLayer->m_Speedup;
	pLayer->m_Switch = pThisLayer->m_Switch;
	pLayer->m_Tune = pThisLayer->m_Tune;
	if(pThisLayer->Editor()->m_BrushColorEnabled)
	{
		pLayer->m_Color = pThisLayer->m_Color;
		pLayer->m_Color.a = 255;
	}
};

int CLayerTiles::BrushGrab(const std::shared_ptr<CLayerGroup> &pBrush, CUIRect Rect)
{
	RECTi r;
	Convert(Rect, &r);
	Clamp(&r);

	if(!r.w || !r.h)
		return 0;

	// create new layers
	if(this->m_Tele)
	{
		std::shared_ptr<CLayerTele> pGrabbed = std::make_shared<CLayerTele>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the tele data
		if(!Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x] = static_cast<CLayerTele *>(this)->m_pTeleTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidTeleTile(pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type) && IsTeleTileNumberUsed(pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type))
					{
						Editor()->m_TeleNumber = pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
		pGrabbed->m_TeleNum = Editor()->m_TeleNumber;
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}
	else if(this->m_Speedup)
	{
		std::shared_ptr<CLayerSpeedup> pGrabbed = std::make_shared<CLayerSpeedup>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the speedup data
		if(!Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x] = static_cast<CLayerSpeedup *>(this)->m_pSpeedupTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidSpeedupTile(pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Type))
					{
						Editor()->m_SpeedupAngle = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Angle;
						Editor()->m_SpeedupForce = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Force;
						Editor()->m_SpeedupMaxSpeed = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_MaxSpeed;
					}
				}
		pGrabbed->m_SpeedupForce = Editor()->m_SpeedupForce;
		pGrabbed->m_SpeedupMaxSpeed = Editor()->m_SpeedupMaxSpeed;
		pGrabbed->m_SpeedupAngle = Editor()->m_SpeedupAngle;
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}
	else if(this->m_Switch)
	{
		std::shared_ptr<CLayerSwitch> pGrabbed = std::make_shared<CLayerSwitch>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the switch data
		if(!Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x] = static_cast<CLayerSwitch *>(this)->m_pSwitchTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidSwitchTile(pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type))
					{
						Editor()->m_SwitchNum = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Number;
						Editor()->m_SwitchDelay = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Delay;
					}
				}
		pGrabbed->m_SwitchNumber = Editor()->m_SwitchNum;
		pGrabbed->m_SwitchDelay = Editor()->m_SwitchDelay;
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}

	else if(this->m_Tune)
	{
		std::shared_ptr<CLayerTune> pGrabbed = std::make_shared<CLayerTune>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the tiles
		if(!Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x] = static_cast<CLayerTune *>(this)->m_pTuneTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidTuneTile(pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Type))
					{
						Editor()->m_TuningNum = pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
		pGrabbed->m_TuningNumber = Editor()->m_TuningNum;
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}
	else if(this->m_Front)
	{
		std::shared_ptr<CLayerFront> pGrabbed = std::make_shared<CLayerFront>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}
	else
	{
		std::shared_ptr<CLayerTiles> pGrabbed = std::make_shared<CLayerFront>(r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, Editor()->m_aFileName);
	}

	return 1;
}

void CLayerTiles::FillSelection(bool Empty, const std::shared_ptr<CLayer> &pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerTiles> pLt = std::static_pointer_cast<CLayerTiles>(pBrush);

	bool Destructive = Editor()->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			bool HasTile = GetTile(fx, fy).m_Index;
			if(!Empty && pLt->GetTile(x % pLt->m_Width, y % pLt->m_Height).m_Index == TILE_THROUGH_CUT)
			{
				if(m_Game && Editor()->m_Map.m_pFrontLayer)
				{
					HasTile = HasTile || Editor()->m_Map.m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_Front)
				{
					HasTile = HasTile || Editor()->m_Map.m_pGameLayer->GetTile(fx, fy).m_Index;
				}
			}

			if(!Destructive && HasTile)
				continue;

			SetTile(fx, fy, Empty ? CTile{TILE_AIR} : pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]);
		}
	}
	FlagModified(sx, sy, w, h);
}

void CLayerTiles::BrushDraw(const std::shared_ptr<CLayer> &pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	//
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(pBrush);
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);

	bool Destructive = Editor()->m_BrushDrawDestructive || IsEmpty(pTileLayer);

	for(int y = 0; y < pTileLayer->m_Height; y++)
		for(int x = 0; x < pTileLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			bool HasTile = GetTile(fx, fy).m_Index;
			if(pTileLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
			{
				if(m_Game && Editor()->m_Map.m_pFrontLayer)
				{
					HasTile = HasTile || Editor()->m_Map.m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_Front)
				{
					HasTile = HasTile || Editor()->m_Map.m_pGameLayer->GetTile(fx, fy).m_Index;
				}
			}

			if(!Destructive && HasTile)
				continue;

			SetTile(fx, fy, pTileLayer->GetTile(x, y));
		}

	FlagModified(sx, sy, pTileLayer->m_Width, pTileLayer->m_Height);
}

void CLayerTiles::BrushFlipX()
{
	BrushFlipXImpl(m_pTiles);

	if(m_Tele || m_Speedup || m_Tune)
		return;

	bool Rotate = !(m_Game || m_Front || m_Switch) || Editor()->m_AllowPlaceUnusedTiles;
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			if(!Rotate && !IsRotatableTile(m_pTiles[y * m_Width + x].m_Index))
				m_pTiles[y * m_Width + x].m_Flags = 0;
			else
				m_pTiles[y * m_Width + x].m_Flags ^= (m_pTiles[y * m_Width + x].m_Flags & TILEFLAG_ROTATE) ? TILEFLAG_YFLIP : TILEFLAG_XFLIP;
}

void CLayerTiles::BrushFlipY()
{
	BrushFlipYImpl(m_pTiles);

	if(m_Tele || m_Speedup || m_Tune)
		return;

	bool Rotate = !(m_Game || m_Front || m_Switch) || Editor()->m_AllowPlaceUnusedTiles;
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			if(!Rotate && !IsRotatableTile(m_pTiles[y * m_Width + x].m_Index))
				m_pTiles[y * m_Width + x].m_Flags = 0;
			else
				m_pTiles[y * m_Width + x].m_Flags ^= (m_pTiles[y * m_Width + x].m_Flags & TILEFLAG_ROTATE) ? TILEFLAG_XFLIP : TILEFLAG_YFLIP;
}

void CLayerTiles::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CTile *pTempData = new CTile[m_Width * m_Height];
		mem_copy(pTempData, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CTile *pDst = m_pTiles;
		bool Rotate = !(m_Game || m_Front) || Editor()->m_AllowPlaceUnusedTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst)
			{
				*pDst = pTempData[y * m_Width + x];
				if(!Rotate && !IsRotatableTile(pDst->m_Index))
					pDst->m_Flags = 0;
				else
				{
					if(pDst->m_Flags & TILEFLAG_ROTATE)
						pDst->m_Flags ^= (TILEFLAG_YFLIP | TILEFLAG_XFLIP);
					pDst->m_Flags ^= TILEFLAG_ROTATE;
				}
			}

		std::swap(m_Width, m_Height);
		delete[] pTempData;
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

void CLayerTiles::Resize(int NewW, int NewH)
{
	CTile *pNewData = new CTile[NewW * NewH];
	mem_zero(pNewData, (size_t)NewW * NewH * sizeof(CTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewData[y * NewW], &m_pTiles[y * m_Width], minimum(m_Width, NewW) * sizeof(CTile));

	// replace old
	delete[] m_pTiles;
	m_pTiles = pNewData;
	m_Width = NewW;
	m_Height = NewH;

	// resize tele layer if available
	if(m_Game && Editor()->m_Map.m_pTeleLayer && (Editor()->m_Map.m_pTeleLayer->m_Width != NewW || Editor()->m_Map.m_pTeleLayer->m_Height != NewH))
		Editor()->m_Map.m_pTeleLayer->Resize(NewW, NewH);

	// resize speedup layer if available
	if(m_Game && Editor()->m_Map.m_pSpeedupLayer && (Editor()->m_Map.m_pSpeedupLayer->m_Width != NewW || Editor()->m_Map.m_pSpeedupLayer->m_Height != NewH))
		Editor()->m_Map.m_pSpeedupLayer->Resize(NewW, NewH);

	// resize front layer
	if(m_Game && Editor()->m_Map.m_pFrontLayer && (Editor()->m_Map.m_pFrontLayer->m_Width != NewW || Editor()->m_Map.m_pFrontLayer->m_Height != NewH))
		Editor()->m_Map.m_pFrontLayer->Resize(NewW, NewH);

	// resize switch layer if available
	if(m_Game && Editor()->m_Map.m_pSwitchLayer && (Editor()->m_Map.m_pSwitchLayer->m_Width != NewW || Editor()->m_Map.m_pSwitchLayer->m_Height != NewH))
		Editor()->m_Map.m_pSwitchLayer->Resize(NewW, NewH);

	// resize tune layer if available
	if(m_Game && Editor()->m_Map.m_pTuneLayer && (Editor()->m_Map.m_pTuneLayer->m_Width != NewW || Editor()->m_Map.m_pTuneLayer->m_Height != NewH))
		Editor()->m_Map.m_pTuneLayer->Resize(NewW, NewH);
}

void CLayerTiles::Shift(int Direction)
{
	ShiftImpl(m_pTiles, Direction, Editor()->m_ShiftBy);
}

void CLayerTiles::ShowInfo()
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->TextureSet(Client()->GetDebugFont());
	Graphics()->QuadsBegin();

	int StartY = maximum(0, (int)(ScreenY0 / 32.0f) - 1);
	int StartX = maximum(0, (int)(ScreenX0 / 32.0f) - 1);
	int EndY = minimum((int)(ScreenY1 / 32.0f) + 1, m_Height);
	int EndX = minimum((int)(ScreenX1 / 32.0f) + 1, m_Width);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int c = x + y * m_Width;
			if(m_pTiles[c].m_Index)
			{
				char aBuf[4];
				if(Editor()->m_ShowTileInfo == CEditor::SHOW_TILE_HEXADECIMAL)
				{
					str_hex(aBuf, sizeof(aBuf), &m_pTiles[c].m_Index, 1);
					aBuf[2] = '\0'; // would otherwise be a space
				}
				else
				{
					str_from_int(m_pTiles[c].m_Index, aBuf);
				}
				Graphics()->QuadsText(x * 32, y * 32, 16.0f, aBuf);

				char aFlags[4] = {m_pTiles[c].m_Flags & TILEFLAG_XFLIP ? 'X' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_YFLIP ? 'Y' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_ROTATE ? 'R' : ' ',
					0};
				Graphics()->QuadsText(x * 32, y * 32 + 16, 16.0f, aFlags);
			}
			x += m_pTiles[c].m_Skip;
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

CUI::EPopupMenuFunctionResult CLayerTiles::RenderProperties(CUIRect *pToolBox)
{
	CUIRect Button;

	const bool EntitiesLayer = IsEntitiesLayer();

	std::shared_ptr<CLayerGroup> pGroup = Editor()->m_Map.m_vpGroups[Editor()->m_SelectedGroup];

	// Game tiles can only be constructed if the layer is relative to the game layer
	if(!EntitiesLayer && !(pGroup->m_OffsetX % 32) && !(pGroup->m_OffsetY % 32) && pGroup->m_ParallaxX == 100 && pGroup->m_ParallaxY == 100)
	{
		pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
		static int s_GameTilesButton = 0;
		if(Editor()->DoButton_Editor(&s_GameTilesButton, "Game tiles", 0, &Button, 0, "Constructs game tiles from this layer"))
			Editor()->PopupSelectGametileOpInvoke(Editor()->UI()->MouseX(), Editor()->UI()->MouseY());

		int Result = Editor()->PopupSelectGameTileOpResult();
		switch(Result)
		{
		case 4:
			Result = TILE_THROUGH_CUT;
			break;
		case 5:
			Result = TILE_FREEZE;
			break;
		case 6:
			Result = TILE_UNFREEZE;
			break;
		case 7:
			Result = TILE_DFREEZE;
			break;
		case 8:
			Result = TILE_DUNFREEZE;
			break;
		case 9:
			Result = TILE_TELECHECKIN;
			break;
		case 10:
			Result = TILE_TELECHECKINEVIL;
			break;
		case 11:
			Result = TILE_LFREEZE;
			break;
		case 12:
			Result = TILE_LUNFREEZE;
			break;
		default:
			break;
		}
		if(Result > -1)
		{
			const int OffsetX = -pGroup->m_OffsetX / 32;
			const int OffsetY = -pGroup->m_OffsetY / 32;

			if(Result != TILE_TELECHECKIN && Result != TILE_TELECHECKINEVIL)
			{
				std::shared_ptr<CLayerTiles> pGLayer = Editor()->m_Map.m_pGameLayer;

				if(pGLayer->m_Width < m_Width + OffsetX || pGLayer->m_Height < m_Height + OffsetY)
				{
					const int NewW = pGLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pGLayer->m_Width;
					const int NewH = pGLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pGLayer->m_Height;
					pGLayer->Resize(NewW, NewH);
				}

				for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
				{
					for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
					{
						if(GetTile(x, y).m_Index)
						{
							const CTile ResultTile = {(unsigned char)Result};
							pGLayer->SetTile(x + OffsetX, y + OffsetY, ResultTile);
						}
					}
				}
			}
			else
			{
				if(!Editor()->m_Map.m_pTeleLayer)
				{
					std::shared_ptr<CLayer> pLayer = std::make_shared<CLayerTele>(m_Width, m_Height);
					Editor()->m_Map.MakeTeleLayer(pLayer);
					Editor()->m_Map.m_pGameGroup->AddLayer(pLayer);
				}

				std::shared_ptr<CLayerTele> pTLayer = Editor()->m_Map.m_pTeleLayer;

				if(pTLayer->m_Width < m_Width + OffsetX || pTLayer->m_Height < m_Height + OffsetY)
				{
					int NewW = pTLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pTLayer->m_Width;
					int NewH = pTLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pTLayer->m_Height;
					pTLayer->Resize(NewW, NewH);
				}

				for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
				{
					for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
					{
						if(GetTile(x, y).m_Index)
						{
							pTLayer->m_pTiles[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Index = TILE_AIR + Result;
							pTLayer->m_pTeleTile[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Number = 1;
							pTLayer->m_pTeleTile[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Type = TILE_AIR + Result;
						}
					}
				}
			}
		}
	}

	if(Editor()->m_Map.m_pGameLayer.get() != this)
	{
		if(m_Image >= 0 && (size_t)m_Image < Editor()->m_Map.m_vpImages.size() && Editor()->m_Map.m_vpImages[m_Image]->m_AutoMapper.IsLoaded() && m_AutoMapperConfig != -1)
		{
			pToolBox->HSplitBottom(2.0f, pToolBox, nullptr);
			pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
			if(m_Seed != 0)
			{
				CUIRect ButtonAuto;
				Button.VSplitRight(16.0f, &Button, &ButtonAuto);
				Button.VSplitRight(2.0f, &Button, nullptr);
				static int s_AutoMapperButtonAuto = 0;
				if(Editor()->DoButton_Editor(&s_AutoMapperButtonAuto, "A", m_AutoAutoMap, &ButtonAuto, 0, "Automatically run automap after modifications."))
				{
					m_AutoAutoMap = !m_AutoAutoMap;
					FlagModified(0, 0, m_Width, m_Height);
				}
			}

			static int s_AutoMapperButton = 0;
			if(Editor()->DoButton_Editor(&s_AutoMapperButton, "Automap", 0, &Button, 0, "Run the automapper"))
			{
				Editor()->m_Map.m_vpImages[m_Image]->m_AutoMapper.Proceed(this, m_AutoMapperConfig, m_Seed);
				return CUI::POPUP_CLOSE_CURRENT;
			}
		}
	}

	enum
	{
		PROP_WIDTH = 0,
		PROP_HEIGHT,
		PROP_SHIFT,
		PROP_SHIFT_BY,
		PROP_IMAGE,
		PROP_COLOR,
		PROP_COLOR_ENV,
		PROP_COLOR_ENV_OFFSET,
		PROP_AUTOMAPPER,
		PROP_SEED,
		NUM_PROPS,
	};

	int Color = 0;
	Color |= m_Color.r << 24;
	Color |= m_Color.g << 16;
	Color |= m_Color.b << 8;
	Color |= m_Color.a;

	CProperty aProps[] = {
		{"Width", m_Width, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Height", m_Height, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", Editor()->m_ShiftBy, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Image", m_Image, PROPTYPE_IMAGE, 0, 0},
		{"Color", Color, PROPTYPE_COLOR, 0, 0},
		{"Color Env", m_ColorEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Color TO", m_ColorEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Auto Rule", m_AutoMapperConfig, PROPTYPE_AUTOMAPPER, m_Image, 0},
		{"Seed", m_Seed, PROPTYPE_INT_SCROLL, 0, 1000000000},
		{nullptr},
	};

	if(EntitiesLayer) // remove the image and color properties if this is a game layer
	{
		aProps[PROP_IMAGE].m_pName = nullptr;
		aProps[PROP_COLOR].m_pName = nullptr;
		aProps[PROP_AUTOMAPPER].m_pName = nullptr;
	}
	if(m_Image == -1)
	{
		aProps[PROP_AUTOMAPPER].m_pName = nullptr;
		aProps[PROP_SEED].m_pName = nullptr;
	}

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = Editor()->DoProperties(pToolBox, aProps, s_aIds, &NewVal);

	if(Prop == PROP_WIDTH && NewVal > 1)
	{
		if(NewVal > 1000 && !Editor()->m_LargeLayerWasWarned)
		{
			Editor()->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			Editor()->m_PopupEventActivated = true;
			Editor()->m_LargeLayerWasWarned = true;
		}
		Resize(NewVal, m_Height);
	}
	else if(Prop == PROP_HEIGHT && NewVal > 1)
	{
		if(NewVal > 1000 && !Editor()->m_LargeLayerWasWarned)
		{
			Editor()->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			Editor()->m_PopupEventActivated = true;
			Editor()->m_LargeLayerWasWarned = true;
		}
		Resize(m_Width, NewVal);
	}
	else if(Prop == PROP_SHIFT)
	{
		Shift(NewVal);
	}
	else if(Prop == PROP_SHIFT_BY)
	{
		Editor()->m_ShiftBy = NewVal;
	}
	else if(Prop == PROP_IMAGE)
	{
		m_Image = NewVal;
		if(NewVal == -1)
		{
			m_Image = -1;
		}
		else
		{
			m_Image = NewVal % Editor()->m_Map.m_vpImages.size();
			m_AutoMapperConfig = -1;

			if(Editor()->m_Map.m_vpImages[m_Image]->m_Width % 16 != 0 || Editor()->m_Map.m_vpImages[m_Image]->m_Height % 16 != 0)
			{
				Editor()->m_PopupEventType = CEditor::POPEVENT_IMAGEDIV16;
				Editor()->m_PopupEventActivated = true;
				m_Image = -1;
			}
		}
	}
	else if(Prop == PROP_COLOR)
	{
		m_Color.r = (NewVal >> 24) & 0xff;
		m_Color.g = (NewVal >> 16) & 0xff;
		m_Color.b = (NewVal >> 8) & 0xff;
		m_Color.a = NewVal & 0xff;
	}
	else if(Prop == PROP_COLOR_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)Editor()->m_Map.m_vpEnvelopes.size() - 1);
		const int Step = (Index - m_ColorEnv) % 2;
		if(Step != 0)
		{
			for(; Index >= -1 && Index < (int)Editor()->m_Map.m_vpEnvelopes.size(); Index += Step)
			{
				if(Index == -1 || Editor()->m_Map.m_vpEnvelopes[Index]->GetChannels() == 4)
				{
					m_ColorEnv = Index;
					break;
				}
			}
		}
	}
	else if(Prop == PROP_COLOR_ENV_OFFSET)
	{
		m_ColorEnvOffset = NewVal;
	}
	else if(Prop == PROP_SEED)
	{
		m_Seed = NewVal;
	}
	else if(Prop == PROP_AUTOMAPPER)
	{
		if(m_Image >= 0 && Editor()->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum() > 0 && NewVal >= 0)
			m_AutoMapperConfig = NewVal % Editor()->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum();
		else
			m_AutoMapperConfig = -1;
	}

	if(Prop != -1)
	{
		FlagModified(0, 0, m_Width, m_Height);
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::EPopupMenuFunctionResult CLayerTiles::RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers)
{
	if(State.m_Modified)
	{
		CUIRect Commit;
		pToolbox->HSplitBottom(20.0f, pToolbox, &Commit);
		static int s_CommitButton = 0;
		if(pEditor->DoButton_Editor(&s_CommitButton, "Commit", 0, &Commit, 0, "Applies the changes"))
		{
			for(auto &pLayer : vpLayers)
			{
				if((State.m_Modified & SCommonPropState::MODIFIED_SIZE) != 0)
					pLayer->Resize(State.m_Width, State.m_Height);

				if((State.m_Modified & SCommonPropState::MODIFIED_COLOR) != 0)
				{
					pLayer->m_Color.r = (State.m_Color >> 24) & 0xff;
					pLayer->m_Color.g = (State.m_Color >> 16) & 0xff;
					pLayer->m_Color.b = (State.m_Color >> 8) & 0xff;
					pLayer->m_Color.a = State.m_Color & 0xff;
				}

				pLayer->FlagModified(0, 0, pLayer->m_Width, pLayer->m_Height);
			}
			State.m_Modified = 0;
		}
	}
	else
	{
		for(auto &pLayer : vpLayers)
		{
			if(pLayer->m_Width > State.m_Width)
				State.m_Width = pLayer->m_Width;
			if(pLayer->m_Height > State.m_Height)
				State.m_Height = pLayer->m_Height;
		}
	}

	{
		CUIRect Warning;
		pToolbox->HSplitTop(13.0f, &Warning, pToolbox);
		Warning.HMargin(0.5f, &Warning);

		pEditor->TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		SLabelProperties Props;
		Props.m_MaxWidth = Warning.w;
		pEditor->UI()->DoLabel(&Warning, "Editing multiple layers", 9.0f, TEXTALIGN_ML, Props);
		pEditor->TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		pToolbox->HSplitTop(2.0f, nullptr, pToolbox);
	}

	enum
	{
		PROP_WIDTH = 0,
		PROP_HEIGHT,
		PROP_SHIFT,
		PROP_SHIFT_BY,
		PROP_COLOR,
		NUM_PROPS,
	};

	CProperty aProps[] = {
		{"Width", State.m_Width, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Height", State.m_Height, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", pEditor->m_ShiftBy, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Color", State.m_Color, PROPTYPE_COLOR, 0, 0},
		{nullptr},
	};

	static int s_aIds[NUM_PROPS] = {0};
	int NewVal = 0;
	int Prop = pEditor->DoProperties(pToolbox, aProps, s_aIds, &NewVal);

	if(Prop == PROP_WIDTH && NewVal > 1)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = pEditor->POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Width = NewVal;
	}
	else if(Prop == PROP_HEIGHT && NewVal > 1)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = pEditor->POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Height = NewVal;
	}
	else if(Prop == PROP_SHIFT)
	{
		for(auto &pLayer : vpLayers)
			pLayer->Shift(NewVal);
	}
	else if(Prop == PROP_SHIFT_BY)
	{
		pEditor->m_ShiftBy = NewVal;
	}
	else if(Prop == PROP_COLOR)
	{
		State.m_Color = NewVal;
	}

	if(Prop == PROP_WIDTH || Prop == PROP_HEIGHT)
	{
		State.m_Modified |= SCommonPropState::MODIFIED_SIZE;
	}
	else if(Prop == PROP_COLOR)
	{
		State.m_Modified |= SCommonPropState::MODIFIED_COLOR;
	}

	return CUI::POPUP_KEEP_OPEN;
}

void CLayerTiles::FlagModified(int x, int y, int w, int h)
{
	Editor()->m_Map.OnModify();
	if(m_Seed != 0 && m_AutoMapperConfig != -1 && m_AutoAutoMap && m_Image >= 0)
	{
		Editor()->m_Map.m_vpImages[m_Image]->m_AutoMapper.ProceedLocalized(this, m_AutoMapperConfig, m_Seed, x, y, w, h);
	}
}

void CLayerTiles::ModifyImageIndex(const FIndexModifyFunction &Func)
{
	Func(&m_Image);
}

void CLayerTiles::ModifyEnvelopeIndex(const FIndexModifyFunction &Func)
{
	Func(&m_ColorEnv);
}
