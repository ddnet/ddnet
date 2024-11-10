/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_tiles.h"

#include <engine/keys.h>
#include <engine/shared/map.h>
#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>
#include <game/editor/enums.h>

#include <iterator>
#include <numeric>

#include "image.h"

CLayerTiles::CLayerTiles(CEditor *pEditor, int w, int h) :
	CLayer(pEditor)
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

	str_copy(m_aFileName, Other.m_aFileName);
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
	auto CurrentTile = m_pTiles[y * m_Width + x];
	SetTileIgnoreHistory(x, y, Tile);
	RecordStateChange(x, y, CurrentTile, Tile);
}

void CLayerTiles::SetTileIgnoreHistory(int x, int y, CTile Tile) const
{
	m_pTiles[y * m_Width + x] = Tile;
}

void CLayerTiles::RecordStateChange(int x, int y, CTile Previous, CTile Tile)
{
	if(!m_TilesHistory[y][x].m_Changed)
		m_TilesHistory[y][x] = STileStateChange{true, Previous, Tile};
	else
		m_TilesHistory[y][x].m_Current = Tile;
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
				m_pTiles[y * m_Width + x].m_Flags |= m_pEditor->m_Map.m_vpImages[m_Image]->m_aTileFlags[m_pTiles[y * m_Width + x].m_Index];
	}
}

void CLayerTiles::ExtractTiles(int TilemapItemVersion, const CTile *pSavedTiles, size_t SavedTilesSize) const
{
	const size_t DestSize = (size_t)m_Width * m_Height;
	if(TilemapItemVersion >= CMapItemLayerTilemap::TILE_SKIP_MIN_VERSION)
		CMap::ExtractTiles(m_pTiles, DestSize, pSavedTiles, SavedTilesSize);
	else if(SavedTilesSize >= DestSize)
		mem_copy(m_pTiles, pSavedTiles, DestSize * sizeof(CTile));
}

void CLayerTiles::MakePalette() const
{
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pTiles[y * m_Width + x].m_Index = y * 16 + x;
}

void CLayerTiles::Render(bool Tileset)
{
	IGraphics::CTextureHandle Texture;
	if(m_Image >= 0 && (size_t)m_Image < m_pEditor->m_Map.m_vpImages.size())
		Texture = m_pEditor->m_Map.m_vpImages[m_Image]->m_Texture;
	else if(m_Game)
		Texture = m_pEditor->GetEntitiesTexture();
	else if(m_Front)
		Texture = m_pEditor->GetFrontTexture();
	else if(m_Tele)
		Texture = m_pEditor->GetTeleTexture();
	else if(m_Speedup)
		Texture = m_pEditor->GetSpeedupTexture();
	else if(m_Switch)
		Texture = m_pEditor->GetSwitchTexture();
	else if(m_Tune)
		Texture = m_pEditor->GetTuneTexture();
	Graphics()->TextureSet(Texture);

	ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CEditor::EnvelopeEval(m_ColorEnvOffset, m_ColorEnv, ColorEnv, 4, m_pEditor);
	const ColorRGBA Color = ColorRGBA(m_Color.r / 255.0f, m_Color.g / 255.0f, m_Color.b / 255.0f, m_Color.a / 255.0f).Multiply(ColorEnv);

	Graphics()->BlendNone();
	m_pEditor->RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	m_pEditor->RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_TRANSPARENT);

	// Render DDRace Layers
	if(!Tileset)
	{
		if(m_Tele)
			m_pEditor->RenderTools()->RenderTeleOverlay(static_cast<CLayerTele *>(this)->m_pTeleTile, m_Width, m_Height, 32.0f);
		if(m_Speedup)
			m_pEditor->RenderTools()->RenderSpeedupOverlay(static_cast<CLayerSpeedup *>(this)->m_pSpeedupTile, m_Width, m_Height, 32.0f);
		if(m_Switch)
			m_pEditor->RenderTools()->RenderSwitchOverlay(static_cast<CLayerSwitch *>(this)->m_pSwitchTile, m_Width, m_Height, 32.0f);
		if(m_Tune)
			m_pEditor->RenderTools()->RenderTuneOverlay(static_cast<CLayerTune *>(this)->m_pTuneTile, m_Width, m_Height, 32.0f);
	}
}

int CLayerTiles::ConvertX(float x) const { return (int)(x / 32.0f); }
int CLayerTiles::ConvertY(float y) const { return (int)(y / 32.0f); }

void CLayerTiles::Convert(CUIRect Rect, RECTi *pOut) const
{
	pOut->x = ConvertX(Rect.x);
	pOut->y = ConvertY(Rect.y);
	pOut->w = ConvertX(Rect.x + Rect.w + 31) - pOut->x;
	pOut->h = ConvertY(Rect.y + Rect.h + 31) - pOut->y;
}

void CLayerTiles::Snap(CUIRect *pRect) const
{
	RECTi Out;
	Convert(*pRect, &Out);
	pRect->x = Out.x * 32.0f;
	pRect->y = Out.y * 32.0f;
	pRect->w = Out.w * 32.0f;
	pRect->h = Out.h * 32.0f;
}

void CLayerTiles::Clamp(RECTi *pRect) const
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
	return m_pEditor->m_Map.m_pGameLayer.get() == this || m_pEditor->m_Map.m_pTeleLayer.get() == this || m_pEditor->m_Map.m_pSpeedupLayer.get() == this || m_pEditor->m_Map.m_pFrontLayer.get() == this || m_pEditor->m_Map.m_pSwitchLayer.get() == this || m_pEditor->m_Map.m_pTuneLayer.get() == this;
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
					if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidGameTile(Index))
						return false;
				}
				else if(pLayer->m_Front)
				{
					if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidFrontTile(Index))
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
	str_format(aBuf, sizeof(aBuf), "%d⨯%d", ConvertX(Rect.w), ConvertY(Rect.h));
	TextRender()->Text(Rect.x + 3.0f, Rect.y + 3.0f, m_pEditor->m_ShowPicker ? 15.0f : m_pEditor->MapView()->ScaleLength(15.0f), aBuf, -1.0f);
}

template<typename T>
static void InitGrabbedLayer(std::shared_ptr<T> pLayer, CLayerTiles *pThisLayer)
{
	pLayer->m_pEditor = pThisLayer->m_pEditor;
	pLayer->m_Image = pThisLayer->m_Image;
	pLayer->m_Game = pThisLayer->m_Game;
	pLayer->m_Front = pThisLayer->m_Front;
	pLayer->m_Tele = pThisLayer->m_Tele;
	pLayer->m_Speedup = pThisLayer->m_Speedup;
	pLayer->m_Switch = pThisLayer->m_Switch;
	pLayer->m_Tune = pThisLayer->m_Tune;
	if(pThisLayer->m_pEditor->m_BrushColorEnabled)
	{
		pLayer->m_Color = pThisLayer->m_Color;
		pLayer->m_Color.a = 255;
	}
};

int CLayerTiles::BrushGrab(std::shared_ptr<CLayerGroup> pBrush, CUIRect Rect)
{
	RECTi r;
	Convert(Rect, &r);
	Clamp(&r);

	if(!r.w || !r.h)
		return 0;

	// create new layers
	if(this->m_Tele)
	{
		std::shared_ptr<CLayerTele> pGrabbed = std::make_shared<CLayerTele>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		for(int y = 0; y < r.h; y++)
		{
			for(int x = 0; x < r.w; x++)
			{
				// copy the tiles
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

				// copy the tele data
				if(!m_pEditor->Input()->KeyIsPressed(KEY_SPACE))
				{
					pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x] = static_cast<CLayerTele *>(this)->m_pTeleTile[(r.y + y) * m_Width + (r.x + x)];
					unsigned char TgtIndex = pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type;
					if(IsValidTeleTile(TgtIndex) && IsTeleTileNumberUsedAny(TgtIndex))
					{
						m_pEditor->m_TeleNumbers[TgtIndex] = pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
				else
				{
					CTile Tile = pGrabbed->m_pTiles[y * pGrabbed->m_Width + x];
					if(IsValidTeleTile(Tile.m_Index) && IsTeleTileNumberUsedAny(Tile.m_Index))
					{
						pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Number = m_pEditor->m_TeleNumbers[Tile.m_Index];
					}
				}
			}
		}

		pGrabbed->m_TeleNumbers = m_pEditor->m_TeleNumbers;

		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}
	else if(this->m_Speedup)
	{
		std::shared_ptr<CLayerSpeedup> pGrabbed = std::make_shared<CLayerSpeedup>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the speedup data
		if(!m_pEditor->Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x] = static_cast<CLayerSpeedup *>(this)->m_pSpeedupTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidSpeedupTile(pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Type))
					{
						m_pEditor->m_SpeedupAngle = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Angle;
						m_pEditor->m_SpeedupForce = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Force;
						m_pEditor->m_SpeedupMaxSpeed = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_MaxSpeed;
					}
				}
		pGrabbed->m_SpeedupForce = m_pEditor->m_SpeedupForce;
		pGrabbed->m_SpeedupMaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
		pGrabbed->m_SpeedupAngle = m_pEditor->m_SpeedupAngle;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}
	else if(this->m_Switch)
	{
		std::shared_ptr<CLayerSwitch> pGrabbed = std::make_shared<CLayerSwitch>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the switch data
		if(!m_pEditor->Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x] = static_cast<CLayerSwitch *>(this)->m_pSwitchTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidSwitchTile(pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type))
					{
						m_pEditor->m_SwitchNum = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Number;
						m_pEditor->m_SwitchDelay = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Delay;
					}
				}
		pGrabbed->m_SwitchNumber = m_pEditor->m_SwitchNum;
		pGrabbed->m_SwitchDelay = m_pEditor->m_SwitchDelay;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}

	else if(this->m_Tune)
	{
		std::shared_ptr<CLayerTune> pGrabbed = std::make_shared<CLayerTune>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the tiles
		if(!m_pEditor->Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x] = static_cast<CLayerTune *>(this)->m_pTuneTile[(r.y + y) * m_Width + (r.x + x)];
					if(IsValidTuneTile(pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Type))
					{
						m_pEditor->m_TuningNum = pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
		pGrabbed->m_TuningNumber = m_pEditor->m_TuningNum;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}
	else if(this->m_Front)
	{
		std::shared_ptr<CLayerFront> pGrabbed = std::make_shared<CLayerFront>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}
	else
	{
		std::shared_ptr<CLayerTiles> pGrabbed = std::make_shared<CLayerFront>(m_pEditor, r.w, r.h);
		InitGrabbedLayer(pGrabbed, this);

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName);
	}

	return 1;
}

void CLayerTiles::FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerTiles> pLt = std::static_pointer_cast<CLayerTiles>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

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
				if(m_Game && m_pEditor->m_Map.m_pFrontLayer)
				{
					HasTile = HasTile || m_pEditor->m_Map.m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_Front)
				{
					HasTile = HasTile || m_pEditor->m_Map.m_pGameLayer->GetTile(fx, fy).m_Index;
				}
			}

			if(!Destructive && HasTile)
				continue;

			SetTile(fx, fy, Empty ? CTile{TILE_AIR} : pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]);
		}
	}
	FlagModified(sx, sy, w, h);
}

void CLayerTiles::BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	//
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pTileLayer);

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
				if(m_Game && m_pEditor->m_Map.m_pFrontLayer)
				{
					HasTile = HasTile || m_pEditor->m_Map.m_pFrontLayer->GetTile(fx, fy).m_Index;
				}
				else if(m_Front)
				{
					HasTile = HasTile || m_pEditor->m_Map.m_pGameLayer->GetTile(fx, fy).m_Index;
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

	bool Rotate = !(m_Game || m_Front || m_Switch) || m_pEditor->m_AllowPlaceUnusedTiles;
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

	bool Rotate = !(m_Game || m_Front || m_Switch) || m_pEditor->m_AllowPlaceUnusedTiles;
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
		bool Rotate = !(m_Game || m_Front) || m_pEditor->m_AllowPlaceUnusedTiles;
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

const char *CLayerTiles::TypeName() const
{
	return "tiles";
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
	if(m_Game && m_pEditor->m_Map.m_pTeleLayer && (m_pEditor->m_Map.m_pTeleLayer->m_Width != NewW || m_pEditor->m_Map.m_pTeleLayer->m_Height != NewH))
		m_pEditor->m_Map.m_pTeleLayer->Resize(NewW, NewH);

	// resize speedup layer if available
	if(m_Game && m_pEditor->m_Map.m_pSpeedupLayer && (m_pEditor->m_Map.m_pSpeedupLayer->m_Width != NewW || m_pEditor->m_Map.m_pSpeedupLayer->m_Height != NewH))
		m_pEditor->m_Map.m_pSpeedupLayer->Resize(NewW, NewH);

	// resize front layer
	if(m_Game && m_pEditor->m_Map.m_pFrontLayer && (m_pEditor->m_Map.m_pFrontLayer->m_Width != NewW || m_pEditor->m_Map.m_pFrontLayer->m_Height != NewH))
		m_pEditor->m_Map.m_pFrontLayer->Resize(NewW, NewH);

	// resize switch layer if available
	if(m_Game && m_pEditor->m_Map.m_pSwitchLayer && (m_pEditor->m_Map.m_pSwitchLayer->m_Width != NewW || m_pEditor->m_Map.m_pSwitchLayer->m_Height != NewH))
		m_pEditor->m_Map.m_pSwitchLayer->Resize(NewW, NewH);

	// resize tune layer if available
	if(m_Game && m_pEditor->m_Map.m_pTuneLayer && (m_pEditor->m_Map.m_pTuneLayer->m_Width != NewW || m_pEditor->m_Map.m_pTuneLayer->m_Height != NewH))
		m_pEditor->m_Map.m_pTuneLayer->Resize(NewW, NewH);
}

void CLayerTiles::Shift(int Direction)
{
	ShiftImpl(m_pTiles, Direction, m_pEditor->m_ShiftBy);
}

void CLayerTiles::ShowInfo()
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->TextureSet(m_pEditor->Client()->GetDebugFont());
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
				if(m_pEditor->m_ShowTileInfo == CEditor::SHOW_TILE_HEXADECIMAL)
				{
					str_hex(aBuf, sizeof(aBuf), &m_pTiles[c].m_Index, 1);
					aBuf[2] = '\0'; // would otherwise be a space
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "%d", m_pTiles[c].m_Index);
				}
				m_pEditor->Graphics()->QuadsText(x * 32, y * 32, 16.0f, aBuf);

				char aFlags[4] = {m_pTiles[c].m_Flags & TILEFLAG_XFLIP ? 'X' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_YFLIP ? 'Y' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_ROTATE ? 'R' : ' ',
					0};
				m_pEditor->Graphics()->QuadsText(x * 32, y * 32 + 16, 16.0f, aFlags);
			}
			x += m_pTiles[c].m_Skip;
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CLayerTiles::FillGameTiles(EGameTileOp Fill)
{
	if(!CanFillGameTiles())
		return;

	std::shared_ptr<CLayerGroup> pGroup = m_pEditor->m_Map.m_vpGroups[m_pEditor->m_SelectedGroup];

	int Result = (int)Fill;
	switch(Fill)
	{
	case EGameTileOp::HOOKTHROUGH:
		Result = TILE_THROUGH_CUT;
		break;
	case EGameTileOp::FREEZE:
		Result = TILE_FREEZE;
		break;
	case EGameTileOp::UNFREEZE:
		Result = TILE_UNFREEZE;
		break;
	case EGameTileOp::DEEP_FREEZE:
		Result = TILE_DFREEZE;
		break;
	case EGameTileOp::DEEP_UNFREEZE:
		Result = TILE_DUNFREEZE;
		break;
	case EGameTileOp::BLUE_CHECK_TELE:
		Result = TILE_TELECHECKIN;
		break;
	case EGameTileOp::RED_CHECK_TELE:
		Result = TILE_TELECHECKINEVIL;
		break;
	case EGameTileOp::LIVE_FREEZE:
		Result = TILE_LFREEZE;
		break;
	case EGameTileOp::LIVE_UNFREEZE:
		Result = TILE_LUNFREEZE;
		break;
	default:
		break;
	}
	if(Result > -1)
	{
		const int OffsetX = -pGroup->m_OffsetX / 32;
		const int OffsetY = -pGroup->m_OffsetY / 32;

		std::vector<std::shared_ptr<IEditorAction>> vpActions;
		std::shared_ptr<CLayerTiles> pGLayer = m_pEditor->m_Map.m_pGameLayer;
		int GameLayerIndex = std::find(m_pEditor->m_Map.m_pGameGroup->m_vpLayers.begin(), m_pEditor->m_Map.m_pGameGroup->m_vpLayers.end(), pGLayer) - m_pEditor->m_Map.m_pGameGroup->m_vpLayers.begin();

		if(Result != TILE_TELECHECKIN && Result != TILE_TELECHECKINEVIL)
		{
			if(pGLayer->m_Width < m_Width + OffsetX || pGLayer->m_Height < m_Height + OffsetY)
			{
				std::map<int, std::shared_ptr<CLayer>> savedLayers;
				savedLayers[LAYERTYPE_TILES] = pGLayer->Duplicate();
				savedLayers[LAYERTYPE_GAME] = savedLayers[LAYERTYPE_TILES];

				int PrevW = pGLayer->m_Width;
				int PrevH = pGLayer->m_Height;
				const int NewW = pGLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pGLayer->m_Width;
				const int NewH = pGLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pGLayer->m_Height;
				pGLayer->Resize(NewW, NewH);
				vpActions.push_back(std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, GameLayerIndex, ETilesProp::PROP_WIDTH, PrevW, NewW));
				const std::shared_ptr<CEditorActionEditLayerTilesProp> &Action1 = std::static_pointer_cast<CEditorActionEditLayerTilesProp>(vpActions[vpActions.size() - 1]);
				vpActions.push_back(std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, GameLayerIndex, ETilesProp::PROP_HEIGHT, PrevH, NewH));
				const std::shared_ptr<CEditorActionEditLayerTilesProp> &Action2 = std::static_pointer_cast<CEditorActionEditLayerTilesProp>(vpActions[vpActions.size() - 1]);

				Action1->SetSavedLayers(savedLayers);
				Action2->SetSavedLayers(savedLayers);
			}

			int Changes = 0;
			for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
			{
				for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
				{
					if(GetTile(x, y).m_Index)
					{
						const CTile ResultTile = {(unsigned char)Result};
						pGLayer->SetTile(x + OffsetX, y + OffsetY, ResultTile);
						Changes++;
					}
				}
			}

			vpActions.push_back(std::make_shared<CEditorBrushDrawAction>(m_pEditor, m_pEditor->m_SelectedGroup));
			char aDisplay[256];
			str_format(aDisplay, sizeof(aDisplay), "Construct '%s' game tiles (x%d)", g_apGametileOpNames[(int)Fill], Changes);
			m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions, aDisplay, true));
		}
		else
		{
			if(!m_pEditor->m_Map.m_pTeleLayer)
			{
				std::shared_ptr<CLayerTele> pLayer = std::make_shared<CLayerTele>(m_pEditor, m_Width, m_Height);
				m_pEditor->m_Map.MakeTeleLayer(pLayer);
				m_pEditor->m_Map.m_pGameGroup->AddLayer(pLayer);

				vpActions.push_back(std::make_shared<CEditorActionAddLayer>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_Map.m_pGameGroup->m_vpLayers.size() - 1));

				if(m_Width != pGLayer->m_Width || m_Height > pGLayer->m_Height)
				{
					std::map<int, std::shared_ptr<CLayer>> savedLayers;
					savedLayers[LAYERTYPE_TILES] = pGLayer->Duplicate();
					savedLayers[LAYERTYPE_GAME] = savedLayers[LAYERTYPE_TILES];

					int NewW = pGLayer->m_Width;
					int NewH = pGLayer->m_Height;
					if(m_Width > pGLayer->m_Width)
					{
						NewW = m_Width;
					}
					if(m_Height > pGLayer->m_Height)
					{
						NewH = m_Height;
					}

					int PrevW = pGLayer->m_Width;
					int PrevH = pGLayer->m_Height;
					pLayer->Resize(NewW, NewH);
					vpActions.push_back(std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, GameLayerIndex, ETilesProp::PROP_WIDTH, PrevW, NewW));
					const std::shared_ptr<CEditorActionEditLayerTilesProp> &Action1 = std::static_pointer_cast<CEditorActionEditLayerTilesProp>(vpActions[vpActions.size() - 1]);
					vpActions.push_back(std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, GameLayerIndex, ETilesProp::PROP_HEIGHT, PrevH, NewH));
					const std::shared_ptr<CEditorActionEditLayerTilesProp> &Action2 = std::static_pointer_cast<CEditorActionEditLayerTilesProp>(vpActions[vpActions.size() - 1]);

					Action1->SetSavedLayers(savedLayers);
					Action2->SetSavedLayers(savedLayers);
				}
			}

			std::shared_ptr<CLayerTele> pTLayer = m_pEditor->m_Map.m_pTeleLayer;
			int TeleLayerIndex = std::find(m_pEditor->m_Map.m_pGameGroup->m_vpLayers.begin(), m_pEditor->m_Map.m_pGameGroup->m_vpLayers.end(), pTLayer) - m_pEditor->m_Map.m_pGameGroup->m_vpLayers.begin();

			if(pTLayer->m_Width < m_Width + OffsetX || pTLayer->m_Height < m_Height + OffsetY)
			{
				std::map<int, std::shared_ptr<CLayer>> savedLayers;
				savedLayers[LAYERTYPE_TILES] = pTLayer->Duplicate();
				savedLayers[LAYERTYPE_TELE] = savedLayers[LAYERTYPE_TILES];

				int PrevW = pTLayer->m_Width;
				int PrevH = pTLayer->m_Height;
				int NewW = pTLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pTLayer->m_Width;
				int NewH = pTLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pTLayer->m_Height;
				pTLayer->Resize(NewW, NewH);
				std::shared_ptr<CEditorActionEditLayerTilesProp> Action1, Action2;
				vpActions.push_back(Action1 = std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, TeleLayerIndex, ETilesProp::PROP_WIDTH, PrevW, NewW));
				vpActions.push_back(Action2 = std::make_shared<CEditorActionEditLayerTilesProp>(m_pEditor, m_pEditor->m_SelectedGroup, TeleLayerIndex, ETilesProp::PROP_HEIGHT, PrevH, NewH));

				Action1->SetSavedLayers(savedLayers);
				Action2->SetSavedLayers(savedLayers);
			}

			int Changes = 0;
			for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
			{
				for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
				{
					if(GetTile(x, y).m_Index)
					{
						auto TileIndex = (y + OffsetY) * pTLayer->m_Width + x + OffsetX;
						Changes++;

						STeleTileStateChange::SData Previous{
							pTLayer->m_pTeleTile[TileIndex].m_Number,
							pTLayer->m_pTeleTile[TileIndex].m_Type,
							pTLayer->m_pTiles[TileIndex].m_Index};

						pTLayer->m_pTiles[TileIndex].m_Index = TILE_AIR + Result;
						pTLayer->m_pTeleTile[TileIndex].m_Number = 1;
						pTLayer->m_pTeleTile[TileIndex].m_Type = TILE_AIR + Result;

						STeleTileStateChange::SData Current{
							pTLayer->m_pTeleTile[TileIndex].m_Number,
							pTLayer->m_pTeleTile[TileIndex].m_Type,
							pTLayer->m_pTiles[TileIndex].m_Index};

						pTLayer->RecordStateChange(x, y, Previous, Current);
					}
				}
			}

			vpActions.push_back(std::make_shared<CEditorBrushDrawAction>(m_pEditor, m_pEditor->m_SelectedGroup));
			char aDisplay[256];
			str_format(aDisplay, sizeof(aDisplay), "Construct 'tele' game tiles (x%d)", Changes);
			m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, vpActions, aDisplay, true));
		}
	}
}

bool CLayerTiles::CanFillGameTiles() const
{
	const bool EntitiesLayer = IsEntitiesLayer();
	if(EntitiesLayer)
		return false;

	std::shared_ptr<CLayerGroup> pGroup = m_pEditor->m_Map.m_vpGroups[m_pEditor->m_SelectedGroup];

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
		if(m_pEditor->DoButton_Editor(&s_GameTilesButton, "Game tiles", 0, &Button, 0, "Constructs game tiles from this layer"))
			m_pEditor->PopupSelectGametileOpInvoke(m_pEditor->Ui()->MouseX(), m_pEditor->Ui()->MouseY());
		const int Selected = m_pEditor->PopupSelectGameTileOpResult();
		FillGameTiles((EGameTileOp)Selected);
	}

	if(m_pEditor->m_Map.m_pGameLayer.get() != this)
	{
		if(m_Image >= 0 && (size_t)m_Image < m_pEditor->m_Map.m_vpImages.size() && m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.IsLoaded() && m_AutoMapperConfig != -1)
		{
			pToolBox->HSplitBottom(2.0f, pToolBox, nullptr);
			pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
			if(m_Seed != 0)
			{
				CUIRect ButtonAuto;
				Button.VSplitRight(16.0f, &Button, &ButtonAuto);
				Button.VSplitRight(2.0f, &Button, nullptr);
				static int s_AutoMapperButtonAuto = 0;
				if(m_pEditor->DoButton_Editor(&s_AutoMapperButtonAuto, "A", m_AutoAutoMap, &ButtonAuto, 0, "Automatically run automap after modifications."))
				{
					m_AutoAutoMap = !m_AutoAutoMap;
					FlagModified(0, 0, m_Width, m_Height);
					if(!m_TilesHistory.empty()) // Sometimes pressing that button causes the automap to run so we should be able to undo that
					{
						// record undo
						m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionTileChanges>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_vSelectedLayers[0], "Auto map", m_TilesHistory));
						ClearHistory();
					}
				}
			}

			static int s_AutoMapperButton = 0;
			if(m_pEditor->DoButton_Editor(&s_AutoMapperButton, "Automap", 0, &Button, 0, "Run the automapper"))
			{
				m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.Proceed(this, m_AutoMapperConfig, m_Seed);
				// record undo
				m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionTileChanges>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_vSelectedLayers[0], "Auto map", m_TilesHistory));
				ClearHistory();
				return CUi::POPUP_CLOSE_CURRENT;
			}
		}
	}

	int Color = PackColor(m_Color);

	CProperty aProps[] = {
		{"Width", m_Width, PROPTYPE_INT, 1, 100000},
		{"Height", m_Height, PROPTYPE_INT, 1, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", m_pEditor->m_ShiftBy, PROPTYPE_INT, 1, 100000},
		{"Image", m_Image, PROPTYPE_IMAGE, 0, 0},
		{"Color", Color, PROPTYPE_COLOR, 0, 0},
		{"Color Env", m_ColorEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Color TO", m_ColorEnvOffset, PROPTYPE_INT, -1000000, 1000000},
		{"Auto Rule", m_AutoMapperConfig, PROPTYPE_AUTOMAPPER, m_Image, 0},
		{"Seed", m_Seed, PROPTYPE_INT, 0, 1000000000},
		{nullptr},
	};

	if(EntitiesLayer) // remove the image and color properties if this is a game layer
	{
		aProps[(int)ETilesProp::PROP_IMAGE].m_pName = nullptr;
		aProps[(int)ETilesProp::PROP_COLOR].m_pName = nullptr;
		aProps[(int)ETilesProp::PROP_AUTOMAPPER].m_pName = nullptr;
	}
	if(m_Image == -1)
	{
		aProps[(int)ETilesProp::PROP_AUTOMAPPER].m_pName = nullptr;
		aProps[(int)ETilesProp::PROP_SEED].m_pName = nullptr;
	}

	static int s_aIds[(int)ETilesProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [State, Prop] = m_pEditor->DoPropertiesWithState<ETilesProp>(pToolBox, aProps, s_aIds, &NewVal);

	static CLayerTilesPropTracker s_Tracker(m_pEditor);
	s_Tracker.Begin(this, Prop, State);
	m_pEditor->m_EditorHistory.BeginBulk();

	if(Prop == ETilesProp::PROP_WIDTH && NewVal > 1)
	{
		if(NewVal > 1000 && !m_pEditor->m_LargeLayerWasWarned)
		{
			m_pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			m_pEditor->m_PopupEventActivated = true;
			m_pEditor->m_LargeLayerWasWarned = true;
		}
		Resize(NewVal, m_Height);
	}
	else if(Prop == ETilesProp::PROP_HEIGHT && NewVal > 1)
	{
		if(NewVal > 1000 && !m_pEditor->m_LargeLayerWasWarned)
		{
			m_pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			m_pEditor->m_PopupEventActivated = true;
			m_pEditor->m_LargeLayerWasWarned = true;
		}
		Resize(m_Width, NewVal);
	}
	else if(Prop == ETilesProp::PROP_SHIFT)
	{
		Shift(NewVal);
	}
	else if(Prop == ETilesProp::PROP_SHIFT_BY)
	{
		m_pEditor->m_ShiftBy = NewVal;
	}
	else if(Prop == ETilesProp::PROP_IMAGE)
	{
		m_Image = NewVal;
		if(NewVal == -1)
		{
			m_Image = -1;
		}
		else
		{
			m_Image = NewVal % m_pEditor->m_Map.m_vpImages.size();
			m_AutoMapperConfig = -1;

			if(m_pEditor->m_Map.m_vpImages[m_Image]->m_Width % 16 != 0 || m_pEditor->m_Map.m_vpImages[m_Image]->m_Height % 16 != 0)
			{
				m_pEditor->m_PopupEventType = CEditor::POPEVENT_IMAGEDIV16;
				m_pEditor->m_PopupEventActivated = true;
				m_Image = -1;
			}
		}
	}
	else if(Prop == ETilesProp::PROP_COLOR)
	{
		m_Color.r = (NewVal >> 24) & 0xff;
		m_Color.g = (NewVal >> 16) & 0xff;
		m_Color.b = (NewVal >> 8) & 0xff;
		m_Color.a = NewVal & 0xff;
	}
	else if(Prop == ETilesProp::PROP_COLOR_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)m_pEditor->m_Map.m_vpEnvelopes.size() - 1);
		const int Step = (Index - m_ColorEnv) % 2;
		if(Step != 0)
		{
			for(; Index >= -1 && Index < (int)m_pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
			{
				if(Index == -1 || m_pEditor->m_Map.m_vpEnvelopes[Index]->GetChannels() == 4)
				{
					m_ColorEnv = Index;
					break;
				}
			}
		}
	}
	else if(Prop == ETilesProp::PROP_COLOR_ENV_OFFSET)
	{
		m_ColorEnvOffset = NewVal;
	}
	else if(Prop == ETilesProp::PROP_SEED)
	{
		m_Seed = NewVal;
	}
	else if(Prop == ETilesProp::PROP_AUTOMAPPER)
	{
		if(m_Image >= 0 && m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum() > 0 && NewVal >= 0)
			m_AutoMapperConfig = NewVal % m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum();
		else
			m_AutoMapperConfig = -1;
	}

	s_Tracker.End(Prop, State);

	// Check if modified property could have an effect on automapper
	if((State == EEditState::END || State == EEditState::ONE_GO) && HasAutomapEffect(Prop))
	{
		FlagModified(0, 0, m_Width, m_Height);

		// Record undo if automapper was ran
		if(m_AutoAutoMap && !m_TilesHistory.empty())
		{
			m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionTileChanges>(m_pEditor, m_pEditor->m_SelectedGroup, m_pEditor->m_vSelectedLayers[0], "Auto map", m_TilesHistory));
			ClearHistory();
		}
	}

	// End undo bulk, taking the first action display as the displayed text in the history
	// This is usually the resulting text of the edit layer tiles prop action
	// Since we may also squeeze a tile changes action, we want both to appear as one, thus using a bulk
	m_pEditor->m_EditorHistory.EndBulk(0);

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CLayerTiles::RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<std::shared_ptr<CLayerTiles>> &vpLayers, std::vector<int> &vLayerIndices)
{
	if(State.m_Modified)
	{
		CUIRect Commit;
		pToolbox->HSplitBottom(20.0f, pToolbox, &Commit);
		static int s_CommitButton = 0;
		if(pEditor->DoButton_Editor(&s_CommitButton, "Commit", 0, &Commit, 0, "Applies the changes"))
		{
			bool HasModifiedSize = (State.m_Modified & SCommonPropState::MODIFIED_SIZE) != 0;
			bool HasModifiedColor = (State.m_Modified & SCommonPropState::MODIFIED_COLOR) != 0;

			std::vector<std::shared_ptr<IEditorAction>> vpActions;
			int j = 0;
			int GroupIndex = pEditor->m_SelectedGroup;
			for(auto &pLayer : vpLayers)
			{
				int LayerIndex = vLayerIndices[j++];
				if(HasModifiedSize)
				{
					std::map<int, std::shared_ptr<CLayer>> SavedLayers;
					SavedLayers[LAYERTYPE_TILES] = pLayer->Duplicate();
					if(pLayer->m_Game || pLayer->m_Front || pLayer->m_Switch || pLayer->m_Speedup || pLayer->m_Tune || pLayer->m_Tele)
					{ // Need to save all entities layers when any entity layer
						if(pEditor->m_Map.m_pFrontLayer && !pLayer->m_Front)
							SavedLayers[LAYERTYPE_FRONT] = pEditor->m_Map.m_pFrontLayer->Duplicate();
						if(pEditor->m_Map.m_pTeleLayer && !pLayer->m_Tele)
							SavedLayers[LAYERTYPE_TELE] = pEditor->m_Map.m_pTeleLayer->Duplicate();
						if(pEditor->m_Map.m_pSwitchLayer && !pLayer->m_Switch)
							SavedLayers[LAYERTYPE_SWITCH] = pEditor->m_Map.m_pSwitchLayer->Duplicate();
						if(pEditor->m_Map.m_pSpeedupLayer && !pLayer->m_Speedup)
							SavedLayers[LAYERTYPE_SPEEDUP] = pEditor->m_Map.m_pSpeedupLayer->Duplicate();
						if(pEditor->m_Map.m_pTuneLayer && !pLayer->m_Tune)
							SavedLayers[LAYERTYPE_TUNE] = pEditor->m_Map.m_pTuneLayer->Duplicate();
						if(!pLayer->m_Game)
							SavedLayers[LAYERTYPE_GAME] = pEditor->m_Map.m_pGameLayer->Duplicate();
					}

					int PrevW = pLayer->m_Width;
					int PrevH = pLayer->m_Height;
					pLayer->Resize(State.m_Width, State.m_Height);

					if(PrevW != State.m_Width)
					{
						std::shared_ptr<CEditorActionEditLayerTilesProp> pAction;
						vpActions.push_back(pAction = std::make_shared<CEditorActionEditLayerTilesProp>(pEditor, GroupIndex, LayerIndex, ETilesProp::PROP_WIDTH, PrevW, State.m_Width));
						pAction->SetSavedLayers(SavedLayers);
					}

					if(PrevH != State.m_Height)
					{
						std::shared_ptr<CEditorActionEditLayerTilesProp> pAction;
						vpActions.push_back(pAction = std::make_shared<CEditorActionEditLayerTilesProp>(pEditor, GroupIndex, LayerIndex, ETilesProp::PROP_HEIGHT, PrevH, State.m_Height));
						pAction->SetSavedLayers(SavedLayers);
					}
				}

				if(HasModifiedColor && !pLayer->IsEntitiesLayer())
				{
					int Color = 0;
					Color |= pLayer->m_Color.r << 24;
					Color |= pLayer->m_Color.g << 16;
					Color |= pLayer->m_Color.b << 8;
					Color |= pLayer->m_Color.a;

					pLayer->m_Color.r = (State.m_Color >> 24) & 0xff;
					pLayer->m_Color.g = (State.m_Color >> 16) & 0xff;
					pLayer->m_Color.b = (State.m_Color >> 8) & 0xff;
					pLayer->m_Color.a = State.m_Color & 0xff;

					vpActions.push_back(std::make_shared<CEditorActionEditLayerTilesProp>(pEditor, GroupIndex, LayerIndex, ETilesProp::PROP_COLOR, Color, State.m_Color));
				}

				pLayer->FlagModified(0, 0, pLayer->m_Width, pLayer->m_Height);
			}
			State.m_Modified = 0;

			char aDisplay[256];
			str_format(aDisplay, sizeof(aDisplay), "Edit %d layers common properties: %s", (int)vpLayers.size(), HasModifiedColor && HasModifiedSize ? "color, size" : (HasModifiedColor ? "color" : "size"));
			pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(pEditor, vpActions, aDisplay));
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
		pEditor->Ui()->DoLabel(&Warning, "Editing multiple layers", 9.0f, TEXTALIGN_ML, Props);
		pEditor->TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		pToolbox->HSplitTop(2.0f, nullptr, pToolbox);
	}

	CProperty aProps[] = {
		{"Width", State.m_Width, PROPTYPE_INT, 1, 100000},
		{"Height", State.m_Height, PROPTYPE_INT, 1, 100000},
		{"Shift", 0, PROPTYPE_SHIFT, 0, 0},
		{"Shift by", pEditor->m_ShiftBy, PROPTYPE_INT, 1, 100000},
		{"Color", State.m_Color, PROPTYPE_COLOR, 0, 0},
		{nullptr},
	};

	static int s_aIds[(int)ETilesCommonProp::NUM_PROPS] = {0};
	int NewVal = 0;
	auto [PropState, Prop] = pEditor->DoPropertiesWithState<ETilesCommonProp>(pToolbox, aProps, s_aIds, &NewVal);

	static CLayerTilesCommonPropTracker s_Tracker(pEditor);
	s_Tracker.m_vpLayers = vpLayers;
	s_Tracker.m_vLayerIndices = vLayerIndices;

	s_Tracker.Begin(nullptr, Prop, PropState);

	if(Prop == ETilesCommonProp::PROP_WIDTH && NewVal > 1)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Width = NewVal;
	}
	else if(Prop == ETilesCommonProp::PROP_HEIGHT && NewVal > 1)
	{
		if(NewVal > 1000 && !pEditor->m_LargeLayerWasWarned)
		{
			pEditor->m_PopupEventType = CEditor::POPEVENT_LARGELAYER;
			pEditor->m_PopupEventActivated = true;
			pEditor->m_LargeLayerWasWarned = true;
		}
		State.m_Height = NewVal;
	}
	else if(Prop == ETilesCommonProp::PROP_SHIFT)
	{
		for(auto &pLayer : vpLayers)
			pLayer->Shift(NewVal);
	}
	else if(Prop == ETilesCommonProp::PROP_SHIFT_BY)
	{
		pEditor->m_ShiftBy = NewVal;
	}
	else if(Prop == ETilesCommonProp::PROP_COLOR)
	{
		State.m_Color = NewVal;
	}

	s_Tracker.End(Prop, PropState);

	if(PropState == EEditState::END || PropState == EEditState::ONE_GO)
	{
		if(Prop == ETilesCommonProp::PROP_WIDTH || Prop == ETilesCommonProp::PROP_HEIGHT)
		{
			State.m_Modified |= SCommonPropState::MODIFIED_SIZE;
		}
		else if(Prop == ETilesCommonProp::PROP_COLOR)
		{
			State.m_Modified |= SCommonPropState::MODIFIED_COLOR;
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CLayerTiles::FlagModified(int x, int y, int w, int h)
{
	m_pEditor->m_Map.OnModify();
	if(m_Seed != 0 && m_AutoMapperConfig != -1 && m_AutoAutoMap && m_Image >= 0)
	{
		m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ProceedLocalized(this, m_AutoMapperConfig, m_Seed, x, y, w, h);
	}
}

void CLayerTiles::ModifyImageIndex(FIndexModifyFunction Func)
{
	Func(&m_Image);
}

void CLayerTiles::ModifyEnvelopeIndex(FIndexModifyFunction Func)
{
	Func(&m_ColorEnv);
}

void CLayerTiles::ShowPreventUnusedTilesWarning()
{
	if(!m_pEditor->m_PreventUnusedTilesWasWarned)
	{
		m_pEditor->m_PopupEventType = CEditor::POPEVENT_PREVENTUNUSEDTILES;
		m_pEditor->m_PopupEventActivated = true;
		m_pEditor->m_PreventUnusedTilesWasWarned = true;
	}
}

bool CLayerTiles::HasAutomapEffect(ETilesProp Prop)
{
	switch(Prop)
	{
	case ETilesProp::PROP_WIDTH:
	case ETilesProp::PROP_HEIGHT:
	case ETilesProp::PROP_SHIFT:
	case ETilesProp::PROP_IMAGE:
	case ETilesProp::PROP_AUTOMAPPER:
	case ETilesProp::PROP_SEED:
		return true;
	default:
		return false;
	}
	return false;
}
