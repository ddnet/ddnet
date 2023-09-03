/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/textrender.h>

#include "editor.h"
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
	m_Texture = Other.m_Texture;
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

void CLayerTiles::SetTile(int x, int y, CTile tile)
{
	m_pTiles[y * m_Width + x] = tile;
}

void CLayerTiles::PrepareForSave()
{
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pTiles[y * m_Width + x].m_Flags &= TILEFLAG_VFLIP | TILEFLAG_HFLIP | TILEFLAG_ROTATE;

	if(m_Image != -1 && m_Color.a == 255)
	{
		for(int y = 0; y < m_Height; y++)
			for(int x = 0; x < m_Width; x++)
				m_pTiles[y * m_Width + x].m_Flags |= m_pEditor->m_Map.m_vpImages[m_Image]->m_aTileFlags[m_pTiles[y * m_Width + x].m_Index];
	}
}

void CLayerTiles::MakePalette()
{
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pTiles[y * m_Width + x].m_Index = y * 16 + x;
}

void CLayerTiles::Render(bool Tileset)
{
	if(m_Image >= 0 && (size_t)m_Image < m_pEditor->m_Map.m_vpImages.size())
		m_Texture = m_pEditor->m_Map.m_vpImages[m_Image]->m_Texture;
	Graphics()->TextureSet(m_Texture);
	ColorRGBA Color = ColorRGBA(m_Color.r / 255.0f, m_Color.g / 255.0f, m_Color.b / 255.0f, m_Color.a / 255.0f);
	Graphics()->BlendNone();
	m_pEditor->RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_OPAQUE,
		m_pEditor->EnvelopeEval, m_pEditor, m_ColorEnv, m_ColorEnvOffset);
	Graphics()->BlendNormal();
	m_pEditor->RenderTools()->RenderTilemap(m_pTiles, m_Width, m_Height, 32.0f, Color, LAYERRENDERFLAG_TRANSPARENT,
		m_pEditor->EnvelopeEval, m_pEditor, m_ColorEnv, m_ColorEnvOffset);

	// Render DDRace Layers
	if(!Tileset)
	{
		if(m_Tele)
			m_pEditor->RenderTools()->RenderTeleOverlay(((CLayerTele *)this)->m_pTeleTile, m_Width, m_Height, 32.0f);
		if(m_Speedup)
			m_pEditor->RenderTools()->RenderSpeedupOverlay(((CLayerSpeedup *)this)->m_pSpeedupTile, m_Width, m_Height, 32.0f);
		if(m_Switch)
			m_pEditor->RenderTools()->RenderSwitchOverlay(((CLayerSwitch *)this)->m_pSwitchTile, m_Width, m_Height, 32.0f);
		if(m_Tune)
			m_pEditor->RenderTools()->RenderTuneOverlay(((CLayerTune *)this)->m_pTuneTile, m_Width, m_Height, 32.0f);
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

bool CLayerTiles::IsEmpty(CLayerTiles *pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
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

	return true;
}

void CLayerTiles::BrushSelecting(CUIRect Rect)
{
	Graphics()->TextureClear();
	m_pEditor->Graphics()->QuadsBegin();
	m_pEditor->Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	Snap(&Rect);
	IGraphics::CQuadItem QuadItem(Rect.x, Rect.y, Rect.w, Rect.h);
	m_pEditor->Graphics()->QuadsDrawTL(&QuadItem, 1);
	m_pEditor->Graphics()->QuadsEnd();
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d,%d", ConvertX(Rect.w), ConvertY(Rect.h));
	TextRender()->Text(nullptr, Rect.x + 3.0f, Rect.y + 3.0f, m_pEditor->m_ShowPicker ? 15.0f : 15.0f * m_pEditor->m_WorldZoom, aBuf, -1.0f);
}

int CLayerTiles::BrushGrab(CLayerGroup *pBrush, CUIRect Rect)
{
	RECTi r;
	Convert(Rect, &r);
	Clamp(&r);

	if(!r.w || !r.h)
		return 0;

	// create new layers
	if(this->m_Tele)
	{
		CLayerTele *pGrabbed = new CLayerTele(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);

		// copy the tele data
		if(!m_pEditor->Input()->KeyIsPressed(KEY_SPACE))
			for(int y = 0; y < r.h; y++)
				for(int x = 0; x < r.w; x++)
				{
					pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x] = ((CLayerTele *)this)->m_pTeleTile[(r.y + y) * m_Width + (r.x + x)];
					if(pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELEIN || pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELEOUT || pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELEINEVIL
						// || pGrabbed->m_pTeleTile[y*pGrabbed->m_Width+x].m_Type == TILE_TELECHECKINEVIL
						|| pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELECHECK || pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELECHECKOUT
						// || pGrabbed->m_pTeleTile[y*pGrabbed->m_Width+x].m_Type == TILE_TELECHECKIN
						|| pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELEINWEAPON || pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Type == TILE_TELEINHOOK)
					{
						m_pEditor->m_TeleNumber = pGrabbed->m_pTeleTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
		pGrabbed->m_TeleNum = m_pEditor->m_TeleNumber;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
	}
	else if(this->m_Speedup)
	{
		CLayerSpeedup *pGrabbed = new CLayerSpeedup(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

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
					pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x] = ((CLayerSpeedup *)this)->m_pSpeedupTile[(r.y + y) * m_Width + (r.x + x)];
					if(pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Type == TILE_BOOST)
					{
						m_pEditor->m_SpeedupAngle = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Angle;
						m_pEditor->m_SpeedupForce = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_Force;
						m_pEditor->m_SpeedupMaxSpeed = pGrabbed->m_pSpeedupTile[y * pGrabbed->m_Width + x].m_MaxSpeed;
					}
				}
		pGrabbed->m_SpeedupForce = m_pEditor->m_SpeedupForce;
		pGrabbed->m_SpeedupMaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
		pGrabbed->m_SpeedupAngle = m_pEditor->m_SpeedupAngle;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
	}
	else if(this->m_Switch)
	{
		CLayerSwitch *pGrabbed = new CLayerSwitch(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

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
					pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x] = ((CLayerSwitch *)this)->m_pSwitchTile[(r.y + y) * m_Width + (r.x + x)];
					if(pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == ENTITY_DOOR + ENTITY_OFFSET ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_HIT_ENABLE ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_HIT_DISABLE ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_SWITCHOPEN ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_SWITCHCLOSE ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_SWITCHTIMEDOPEN ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_SWITCHTIMEDCLOSE ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == ENTITY_LASER_LONG + ENTITY_OFFSET ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == ENTITY_LASER_MEDIUM + ENTITY_OFFSET ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == ENTITY_LASER_SHORT + ENTITY_OFFSET ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_JUMP ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_ADD_TIME ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_SUBTRACT_TIME ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_ALLOW_TELE_GUN ||
						pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Type == TILE_ALLOW_BLUE_TELE_GUN)
					{
						m_pEditor->m_SwitchNum = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Number;
						m_pEditor->m_SwitchDelay = pGrabbed->m_pSwitchTile[y * pGrabbed->m_Width + x].m_Delay;
					}
				}
		pGrabbed->m_SwitchNumber = m_pEditor->m_SwitchNum;
		pGrabbed->m_SwitchDelay = m_pEditor->m_SwitchDelay;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
	}

	else if(this->m_Tune)
	{
		CLayerTune *pGrabbed = new CLayerTune(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

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
					pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x] = ((CLayerTune *)this)->m_pTuneTile[(r.y + y) * m_Width + (r.x + x)];
					if(pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Type == TILE_TUNE)
					{
						m_pEditor->m_TuningNum = pGrabbed->m_pTuneTile[y * pGrabbed->m_Width + x].m_Number;
					}
				}
		pGrabbed->m_TuningNumber = m_pEditor->m_TuningNum;
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
	}
	else if(this->m_Front)
	{
		CLayerFront *pGrabbed = new CLayerFront(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
	}
	else
	{
		CLayerTiles *pGrabbed = new CLayerTiles(r.w, r.h);
		pGrabbed->m_pEditor = m_pEditor;
		pGrabbed->m_Texture = m_Texture;
		pGrabbed->m_Image = m_Image;
		pGrabbed->m_Game = m_Game;
		if(m_pEditor->m_BrushColorEnabled)
		{
			pGrabbed->m_Color = m_Color;
			pGrabbed->m_Color.a = 255;
		}

		pBrush->AddLayer(pGrabbed);

		// copy the tiles
		for(int y = 0; y < r.h; y++)
			for(int x = 0; x < r.w; x++)
				pGrabbed->m_pTiles[y * pGrabbed->m_Width + x] = GetTile(r.x + x, r.y + y);
		str_copy(pGrabbed->m_aFileName, m_pEditor->m_aFileName, sizeof(pGrabbed->m_aFileName));
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

			if(Empty)
				m_pTiles[fy * m_Width + fx].m_Index = 0;
			else
				SetTile(fx, fy, pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]);
		}
	}
	FlagModified(sx, sy, w, h);
}

void CLayerTiles::BrushDraw(CLayer *pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	//
	CLayerTiles *pTileLayer = (CLayerTiles *)pBrush;
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);

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
				m_pTiles[y * m_Width + x].m_Flags ^= m_pTiles[y * m_Width + x].m_Flags & TILEFLAG_ROTATE ? TILEFLAG_HFLIP : TILEFLAG_VFLIP;
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
				m_pTiles[y * m_Width + x].m_Flags ^= m_pTiles[y * m_Width + x].m_Flags & TILEFLAG_ROTATE ? TILEFLAG_VFLIP : TILEFLAG_HFLIP;
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
						pDst->m_Flags ^= (TILEFLAG_HFLIP | TILEFLAG_VFLIP);
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

CLayer *CLayerTiles::Duplicate() const
{
	return new CLayerTiles(*this);
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
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%i", m_pTiles[c].m_Index);
				m_pEditor->Graphics()->QuadsText(x * 32, y * 32, 16.0f, aBuf);

				char aFlags[4] = {m_pTiles[c].m_Flags & TILEFLAG_VFLIP ? 'V' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_HFLIP ? 'H' : ' ',
					m_pTiles[c].m_Flags & TILEFLAG_ROTATE ? 'R' : ' ',
					0};
				m_pEditor->Graphics()->QuadsText(x * 32, y * 32 + 16, 16.0f, aFlags);
			}
			x += m_pTiles[c].m_Skip;
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

int CLayerTiles::RenderProperties(CUIRect *pToolBox)
{
	CUIRect Button;

	bool IsGameLayer = (m_pEditor->m_Map.m_pGameLayer == this || m_pEditor->m_Map.m_pTeleLayer == this || m_pEditor->m_Map.m_pSpeedupLayer == this || m_pEditor->m_Map.m_pFrontLayer == this || m_pEditor->m_Map.m_pSwitchLayer == this || m_pEditor->m_Map.m_pTuneLayer == this);

	CLayerGroup *pGroup = m_pEditor->m_Map.m_vpGroups[m_pEditor->m_SelectedGroup];

	// Game tiles can only be constructed if the layer is relative to the game layer
	if(!IsGameLayer && !(pGroup->m_OffsetX % 32) && !(pGroup->m_OffsetY % 32) && pGroup->m_ParallaxX == 100 && pGroup->m_ParallaxY == 100)
	{
		pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
		static int s_ColclButton = 0;
		if(m_pEditor->DoButton_Editor(&s_ColclButton, "Game tiles", 0, &Button, 0, "Constructs game tiles from this layer"))
			m_pEditor->PopupSelectGametileOpInvoke(m_pEditor->UI()->MouseX(), m_pEditor->UI()->MouseY());

		int Result = m_pEditor->PopupSelectGameTileOpResult();
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
			int OffsetX = -pGroup->m_OffsetX / 32;
			int OffsetY = -pGroup->m_OffsetY / 32;

			if(Result != TILE_TELECHECKIN && Result != TILE_TELECHECKINEVIL)
			{
				CLayerTiles *pGLayer = m_pEditor->m_Map.m_pGameLayer;

				if(pGLayer->m_Width < m_Width + OffsetX || pGLayer->m_Height < m_Height + OffsetY)
				{
					int NewW = pGLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pGLayer->m_Width;
					int NewH = pGLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pGLayer->m_Height;
					pGLayer->Resize(NewW, NewH);
				}

				for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
					for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
						if(GetTile(x, y).m_Index)
						{
							CTile ResultTile = {(unsigned char)Result};
							pGLayer->SetTile(x + OffsetX, y + OffsetY, ResultTile);
						}
			}
			else
			{
				if(!m_pEditor->m_Map.m_pTeleLayer)
				{
					CLayer *pLayer = new CLayerTele(m_Width, m_Height);
					m_pEditor->m_Map.MakeTeleLayer(pLayer);
					m_pEditor->m_Map.m_pGameGroup->AddLayer(pLayer);
				}

				CLayerTele *pTLayer = m_pEditor->m_Map.m_pTeleLayer;

				if(pTLayer->m_Width < m_Width + OffsetX || pTLayer->m_Height < m_Height + OffsetY)
				{
					int NewW = pTLayer->m_Width < m_Width + OffsetX ? m_Width + OffsetX : pTLayer->m_Width;
					int NewH = pTLayer->m_Height < m_Height + OffsetY ? m_Height + OffsetY : pTLayer->m_Height;
					pTLayer->Resize(NewW, NewH);
				}

				for(int y = OffsetY < 0 ? -OffsetY : 0; y < m_Height; y++)
					for(int x = OffsetX < 0 ? -OffsetX : 0; x < m_Width; x++)
						if(GetTile(x, y).m_Index)
						{
							pTLayer->m_pTiles[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Index = TILE_AIR + Result;
							pTLayer->m_pTeleTile[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Number = 1;
							pTLayer->m_pTeleTile[(y + OffsetY) * pTLayer->m_Width + x + OffsetX].m_Type = TILE_AIR + Result;
						}
			}

			return 1;
		}
	}

	if(m_pEditor->m_Map.m_pGameLayer != this)
	{
		if(m_Image >= 0 && (size_t)m_Image < m_pEditor->m_Map.m_vpImages.size() && m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.IsLoaded() &&
			m_AutoMapperConfig != -1)
		{
			static int s_AutoMapperButton = 0;
			static int s_AutoMapperButtonAuto = 0;
			pToolBox->HSplitBottom(2.0f, pToolBox, nullptr);
			pToolBox->HSplitBottom(12.0f, pToolBox, &Button);
			if(m_Seed != 0)
			{
				CUIRect ButtonAuto;
				Button.VSplitRight(16.0f, &Button, &ButtonAuto);
				Button.VSplitRight(2.0f, &Button, nullptr);
				if(m_pEditor->DoButton_Editor(&s_AutoMapperButtonAuto, "A", m_AutoAutoMap, &ButtonAuto, 0, "Automatically run automap after modifications."))
				{
					m_AutoAutoMap = !m_AutoAutoMap;
					FlagModified(0, 0, m_Width, m_Height);
				}
			}
			if(m_pEditor->DoButton_Editor(&s_AutoMapperButton, "Automap", 0, &Button, 0, "Run the automapper"))
			{
				m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.Proceed(this, m_AutoMapperConfig, m_Seed);
				return 1;
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
		{"Shift by", m_pEditor->m_ShiftBy, PROPTYPE_INT_SCROLL, 1, 100000},
		{"Image", m_Image, PROPTYPE_IMAGE, 0, 0},
		{"Color", Color, PROPTYPE_COLOR, 0, 0},
		{"Color Env", m_ColorEnv + 1, PROPTYPE_ENVELOPE, 0, 0},
		{"Color TO", m_ColorEnvOffset, PROPTYPE_INT_SCROLL, -1000000, 1000000},
		{"Auto Rule", m_AutoMapperConfig, PROPTYPE_AUTOMAPPER, m_Image, 0},
		{"Seed", m_Seed, PROPTYPE_INT_SCROLL, 0, 1000000000},
		{nullptr},
	};

	if(IsGameLayer) // remove the image and color properties if this is a game layer
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
	int Prop = m_pEditor->DoProperties(pToolBox, aProps, s_aIds, &NewVal);

	if(Prop == PROP_WIDTH && NewVal > 1)
	{
		if(NewVal > 1000 && !m_pEditor->m_LargeLayerWasWarned)
		{
			m_pEditor->m_PopupEventType = m_pEditor->POPEVENT_LARGELAYER;
			m_pEditor->m_PopupEventActivated = true;
			m_pEditor->m_LargeLayerWasWarned = true;
		}
		Resize(NewVal, m_Height);
	}
	else if(Prop == PROP_HEIGHT && NewVal > 1)
	{
		if(NewVal > 1000 && !m_pEditor->m_LargeLayerWasWarned)
		{
			m_pEditor->m_PopupEventType = m_pEditor->POPEVENT_LARGELAYER;
			m_pEditor->m_PopupEventActivated = true;
			m_pEditor->m_LargeLayerWasWarned = true;
		}
		Resize(m_Width, NewVal);
	}
	else if(Prop == PROP_SHIFT)
		Shift(NewVal);
	else if(Prop == PROP_SHIFT_BY)
		m_pEditor->m_ShiftBy = NewVal;
	else if(Prop == PROP_IMAGE)
	{
		m_Image = NewVal;
		if(NewVal == -1)
		{
			m_Texture.Invalidate();
			m_Image = -1;
		}
		else
		{
			m_Image = NewVal % m_pEditor->m_Map.m_vpImages.size();
			m_AutoMapperConfig = -1;

			if(m_pEditor->m_Map.m_vpImages[m_Image]->m_Width % 16 != 0 || m_pEditor->m_Map.m_vpImages[m_Image]->m_Height % 16 != 0)
			{
				m_pEditor->m_PopupEventType = m_pEditor->POPEVENT_IMAGEDIV16;
				m_pEditor->m_PopupEventActivated = true;

				m_Texture = IGraphics::CTextureHandle();
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
	if(Prop == PROP_COLOR_ENV)
	{
		int Index = clamp(NewVal - 1, -1, (int)m_pEditor->m_Map.m_vpEnvelopes.size() - 1);
		int Step = (Index - m_ColorEnv) % 2;
		if(Step != 0)
		{
			for(; Index >= -1 && Index < (int)m_pEditor->m_Map.m_vpEnvelopes.size(); Index += Step)
				if(Index == -1 || m_pEditor->m_Map.m_vpEnvelopes[Index]->m_Channels == 4)
				{
					m_ColorEnv = Index;
					break;
				}
		}
	}
	if(Prop == PROP_COLOR_ENV_OFFSET)
		m_ColorEnvOffset = NewVal;
	else if(Prop == PROP_SEED)
		m_Seed = NewVal;
	else if(Prop == PROP_AUTOMAPPER)
	{
		if(m_Image >= 0 && m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum() > 0 && NewVal >= 0)
			m_AutoMapperConfig = NewVal % m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ConfigNamesNum();
		else
			m_AutoMapperConfig = -1;
	}
	if(Prop != -1)
	{
		FlagModified(0, 0, m_Width, m_Height);
	}

	return 0;
}

int CLayerTiles::RenderCommonProperties(SCommonPropState &State, CEditor *pEditor, CUIRect *pToolbox, std::vector<CLayerTiles *> &vpLayers)
{
	if(State.m_Modified)
	{
		CUIRect Commit;
		pToolbox->HSplitBottom(20.0f, pToolbox, &Commit);
		static int s_CommitButton = 0;
		if(pEditor->DoButton_Editor(&s_CommitButton, "Commit", 0, &Commit, 0, "Applies the changes"))
		{
			dbg_msg("editor", "applying changes");
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
		pEditor->UI()->DoLabel(&Warning, "Editing multiple layers", 9.0f, TEXTALIGN_LEFT, Props);
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
		pEditor->m_ShiftBy = NewVal;
	else if(Prop == PROP_COLOR)
	{
		State.m_Color = NewVal;
	}

	if(Prop == PROP_WIDTH || Prop == PROP_HEIGHT)
		State.m_Modified |= SCommonPropState::MODIFIED_SIZE;
	else if(Prop == PROP_COLOR)
		State.m_Modified |= SCommonPropState::MODIFIED_COLOR;

	return 0;
}

void CLayerTiles::FlagModified(int x, int y, int w, int h)
{
	m_pEditor->m_Map.m_Modified = true;
	if(m_Seed != 0 && m_AutoMapperConfig != -1 && m_AutoAutoMap && m_Image >= 0)
	{
		m_pEditor->m_Map.m_vpImages[m_Image]->m_AutoMapper.ProceedLocalized(this, m_AutoMapperConfig, m_Seed, x, y, w, h);
	}
}

void CLayerTiles::ModifyImageIndex(INDEX_MODIFY_FUNC Func)
{
	const auto ImgBefore = m_Image;
	Func(&m_Image);
	if(m_Image != ImgBefore)
		m_Texture.Invalidate();
}

void CLayerTiles::ModifyEnvelopeIndex(INDEX_MODIFY_FUNC Func)
{
	Func(&m_ColorEnv);
}

CLayerTele::CLayerTele(int w, int h) :
	CLayerTiles(w, h)
{
	//m_Type = LAYERTYPE_TELE;
	str_copy(m_aName, "Tele", sizeof(m_aName));
	m_Tele = 1;

	m_pTeleTile = new CTeleTile[w * h];
	mem_zero(m_pTeleTile, (size_t)w * h * sizeof(CTeleTile));
}

CLayerTele::~CLayerTele()
{
	delete[] m_pTeleTile;
}

void CLayerTele::Resize(int NewW, int NewH)
{
	// resize tele data
	CTeleTile *pNewTeleData = new CTeleTile[NewW * NewH];
	mem_zero(pNewTeleData, (size_t)NewW * NewH * sizeof(CTeleTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewTeleData[y * NewW], &m_pTeleTile[y * m_Width], minimum(m_Width, NewW) * sizeof(CTeleTile));

	// replace old
	delete[] m_pTeleTile;
	m_pTeleTile = pNewTeleData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerTele::Shift(int Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pTeleTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerTele::IsEmpty(CLayerTiles *pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidTeleTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerTele::BrushDraw(CLayer *pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	CLayerTele *pTeleLayer = (CLayerTele *)pBrush;
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);
	if(str_comp(pTeleLayer->m_aFileName, m_pEditor->m_aFileName))
		m_pEditor->m_TeleNumber = pTeleLayer->m_TeleNum;

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pTeleLayer);

	for(int y = 0; y < pTeleLayer->m_Height; y++)
		for(int x = 0; x < pTeleLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidTeleTile(pTeleLayer->m_pTiles[y * pTeleLayer->m_Width + x].m_Index)) && pTeleLayer->m_pTiles[y * pTeleLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_TeleNumber != pTeleLayer->m_TeleNum)
				{
					m_pTeleTile[fy * m_Width + fx].m_Number = m_pEditor->m_TeleNumber;
				}
				else if(pTeleLayer->m_pTeleTile[y * pTeleLayer->m_Width + x].m_Number)
					m_pTeleTile[fy * m_Width + fx].m_Number = pTeleLayer->m_pTeleTile[y * pTeleLayer->m_Width + x].m_Number;
				else
				{
					if(!m_pEditor->m_TeleNumber)
					{
						m_pTeleTile[fy * m_Width + fx].m_Number = 0;
						m_pTeleTile[fy * m_Width + fx].m_Type = 0;
						m_pTiles[fy * m_Width + fx].m_Index = 0;
						continue;
					}
					else
						m_pTeleTile[fy * m_Width + fx].m_Number = m_pEditor->m_TeleNumber;
				}

				m_pTeleTile[fy * m_Width + fx].m_Type = pTeleLayer->m_pTiles[y * pTeleLayer->m_Width + x].m_Index;
				m_pTiles[fy * m_Width + fx].m_Index = pTeleLayer->m_pTiles[y * pTeleLayer->m_Width + x].m_Index;
			}
			else
			{
				m_pTeleTile[fy * m_Width + fx].m_Number = 0;
				m_pTeleTile[fy * m_Width + fx].m_Type = 0;
				m_pTiles[fy * m_Width + fx].m_Index = 0;
			}
		}
	FlagModified(sx, sy, pTeleLayer->m_Width, pTeleLayer->m_Height);
}

void CLayerTele::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	BrushFlipXImpl(m_pTeleTile);
}

void CLayerTele::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	BrushFlipYImpl(m_pTeleTile);
}

void CLayerTele::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CTeleTile *pTempData1 = new CTeleTile[m_Width * m_Height];
		CTile *pTempData2 = new CTile[m_Width * m_Height];
		mem_copy(pTempData1, m_pTeleTile, (size_t)m_Width * m_Height * sizeof(CTeleTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CTeleTile *pDst1 = m_pTeleTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[y * m_Width + x];
				*pDst2 = pTempData2[y * m_Width + x];
			}

		std::swap(m_Width, m_Height);
		delete[] pTempData1;
		delete[] pTempData2;
	}

	if(Rotation == 2 || Rotation == 3)
	{
		BrushFlipX();
		BrushFlipY();
	}
}

void CLayerTele::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerTele *pLt = static_cast<CLayerTele *>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidTeleTile((pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]).m_Index)))
			{
				m_pTiles[fy * m_Width + fx].m_Index = 0;
				m_pTeleTile[fy * m_Width + fx].m_Type = 0;
				m_pTeleTile[fy * m_Width + fx].m_Number = 0;
			}
			else
			{
				m_pTiles[fy * m_Width + fx] = pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)];
				if(pLt->m_Tele && m_pTiles[fy * m_Width + fx].m_Index > 0)
				{
					if((!pLt->m_pTeleTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number && m_pEditor->m_TeleNumber) || m_pEditor->m_TeleNumber != pLt->m_TeleNum)
						m_pTeleTile[fy * m_Width + fx].m_Number = m_pEditor->m_TeleNumber;
					else
						m_pTeleTile[fy * m_Width + fx].m_Number = pLt->m_pTeleTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number;
					m_pTeleTile[fy * m_Width + fx].m_Type = m_pTiles[fy * m_Width + fx].m_Index;
				}
			}
		}
	}
	FlagModified(sx, sy, w, h);
}

bool CLayerTele::ContainsElementWithId(int Id)
{
	for(int y = 0; y < m_Height; ++y)
	{
		for(int x = 0; x < m_Width; ++x)
		{
			if(m_pTeleTile[y * m_Width + x].m_Number == Id)
			{
				return true;
			}
		}
	}

	return false;
}

CLayerSpeedup::CLayerSpeedup(int w, int h) :
	CLayerTiles(w, h)
{
	//m_Type = LAYERTYPE_SPEEDUP;
	str_copy(m_aName, "Speedup", sizeof(m_aName));
	m_Speedup = 1;

	m_pSpeedupTile = new CSpeedupTile[w * h];
	mem_zero(m_pSpeedupTile, (size_t)w * h * sizeof(CSpeedupTile));
}

CLayerSpeedup::~CLayerSpeedup()
{
	delete[] m_pSpeedupTile;
}

void CLayerSpeedup::Resize(int NewW, int NewH)
{
	// resize speedup data
	CSpeedupTile *pNewSpeedupData = new CSpeedupTile[NewW * NewH];
	mem_zero(pNewSpeedupData, (size_t)NewW * NewH * sizeof(CSpeedupTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewSpeedupData[y * NewW], &m_pSpeedupTile[y * m_Width], minimum(m_Width, NewW) * sizeof(CSpeedupTile));

	// replace old
	delete[] m_pSpeedupTile;
	m_pSpeedupTile = pNewSpeedupData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerSpeedup::Shift(int Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pSpeedupTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerSpeedup::IsEmpty(CLayerTiles *pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidSpeedupTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerSpeedup::BrushDraw(CLayer *pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	CLayerSpeedup *pSpeedupLayer = (CLayerSpeedup *)pBrush;
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);
	if(str_comp(pSpeedupLayer->m_aFileName, m_pEditor->m_aFileName))
	{
		m_pEditor->m_SpeedupAngle = pSpeedupLayer->m_SpeedupAngle;
		m_pEditor->m_SpeedupForce = pSpeedupLayer->m_SpeedupForce;
		m_pEditor->m_SpeedupMaxSpeed = pSpeedupLayer->m_SpeedupMaxSpeed;
	}

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pSpeedupLayer);

	for(int y = 0; y < pSpeedupLayer->m_Height; y++)
		for(int x = 0; x < pSpeedupLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidSpeedupTile(pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index)) && pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_SpeedupAngle != pSpeedupLayer->m_SpeedupAngle || m_pEditor->m_SpeedupForce != pSpeedupLayer->m_SpeedupForce || m_pEditor->m_SpeedupMaxSpeed != pSpeedupLayer->m_SpeedupMaxSpeed)
				{
					m_pSpeedupTile[fy * m_Width + fx].m_Force = m_pEditor->m_SpeedupForce;
					m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					m_pSpeedupTile[fy * m_Width + fx].m_Angle = m_pEditor->m_SpeedupAngle;
					m_pSpeedupTile[fy * m_Width + fx].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[fy * m_Width + fx].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else if(pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Force)
				{
					m_pSpeedupTile[fy * m_Width + fx].m_Force = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Force;
					m_pSpeedupTile[fy * m_Width + fx].m_Angle = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Angle;
					m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_MaxSpeed;
					m_pSpeedupTile[fy * m_Width + fx].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[fy * m_Width + fx].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else if(m_pEditor->m_SpeedupForce)
				{
					m_pSpeedupTile[fy * m_Width + fx].m_Force = m_pEditor->m_SpeedupForce;
					m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					m_pSpeedupTile[fy * m_Width + fx].m_Angle = m_pEditor->m_SpeedupAngle;
					m_pSpeedupTile[fy * m_Width + fx].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[fy * m_Width + fx].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else
				{
					m_pSpeedupTile[fy * m_Width + fx].m_Force = 0;
					m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = 0;
					m_pSpeedupTile[fy * m_Width + fx].m_Angle = 0;
					m_pSpeedupTile[fy * m_Width + fx].m_Type = 0;
					m_pTiles[fy * m_Width + fx].m_Index = 0;
				}
			}
			else
			{
				m_pSpeedupTile[fy * m_Width + fx].m_Force = 0;
				m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = 0;
				m_pSpeedupTile[fy * m_Width + fx].m_Angle = 0;
				m_pSpeedupTile[fy * m_Width + fx].m_Type = 0;
				m_pTiles[fy * m_Width + fx].m_Index = 0;
			}
		}
	FlagModified(sx, sy, pSpeedupLayer->m_Width, pSpeedupLayer->m_Height);
}

void CLayerSpeedup::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	BrushFlipXImpl(m_pSpeedupTile);
}

void CLayerSpeedup::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	BrushFlipYImpl(m_pSpeedupTile);
}

void CLayerSpeedup::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CSpeedupTile *pTempData1 = new CSpeedupTile[m_Width * m_Height];
		CTile *pTempData2 = new CTile[m_Width * m_Height];
		mem_copy(pTempData1, m_pSpeedupTile, (size_t)m_Width * m_Height * sizeof(CSpeedupTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CSpeedupTile *pDst1 = m_pSpeedupTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[y * m_Width + x];
				*pDst2 = pTempData2[y * m_Width + x];
			}

		std::swap(m_Width, m_Height);
		delete[] pTempData1;
		delete[] pTempData2;
	}

	if(Rotation == 2 || Rotation == 3)
	{
		BrushFlipX();
		BrushFlipY();
	}
}

void CLayerSpeedup::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerSpeedup *pLt = static_cast<CLayerSpeedup *>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidSpeedupTile((pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]).m_Index))) // no speed up tile chosen: reset
			{
				m_pTiles[fy * m_Width + fx].m_Index = 0;
				m_pSpeedupTile[fy * m_Width + fx].m_Force = 0;
				m_pSpeedupTile[fy * m_Width + fx].m_Angle = 0;
			}
			else
			{
				m_pTiles[fy * m_Width + fx] = pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)];
				if(pLt->m_Speedup && m_pTiles[fy * m_Width + fx].m_Index > 0)
				{
					if((!pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Force && m_pEditor->m_SpeedupForce) || m_pEditor->m_SpeedupForce != pLt->m_SpeedupForce)
						m_pSpeedupTile[fy * m_Width + fx].m_Force = m_pEditor->m_SpeedupForce;
					else
						m_pSpeedupTile[fy * m_Width + fx].m_Force = pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Force;
					if((!pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Angle && m_pEditor->m_SpeedupAngle) || m_pEditor->m_SpeedupAngle != pLt->m_SpeedupAngle)
						m_pSpeedupTile[fy * m_Width + fx].m_Angle = m_pEditor->m_SpeedupAngle;
					else
						m_pSpeedupTile[fy * m_Width + fx].m_Angle = pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Angle;
					if((!pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_MaxSpeed && m_pEditor->m_SpeedupMaxSpeed) || m_pEditor->m_SpeedupMaxSpeed != pLt->m_SpeedupMaxSpeed)
						m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					else
						m_pSpeedupTile[fy * m_Width + fx].m_MaxSpeed = pLt->m_pSpeedupTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_MaxSpeed;
					m_pSpeedupTile[fy * m_Width + fx].m_Type = m_pTiles[fy * m_Width + fx].m_Index;
				}
			}
		}
	}
	FlagModified(sx, sy, w, h);
}

CLayerFront::CLayerFront(int w, int h) :
	CLayerTiles(w, h)
{
	//m_Type = LAYERTYPE_FRONT;
	str_copy(m_aName, "Front", sizeof(m_aName));
	m_Front = 1;
}

void CLayerFront::SetTile(int x, int y, CTile tile)
{
	if(tile.m_Index == TILE_THROUGH_CUT)
	{
		CTile nohook = {TILE_NOHOOK};
		m_pEditor->m_Map.m_pGameLayer->CLayerTiles::SetTile(x, y, nohook); // NOLINT(bugprone-parent-virtual-call)
	}
	else if(tile.m_Index == TILE_AIR && CLayerTiles::GetTile(x, y).m_Index == TILE_THROUGH_CUT)
	{
		CTile air = {TILE_AIR};
		m_pEditor->m_Map.m_pGameLayer->CLayerTiles::SetTile(x, y, air); // NOLINT(bugprone-parent-virtual-call)
	}
	if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidFrontTile(tile.m_Index))
	{
		CLayerTiles::SetTile(x, y, tile);
	}
	else
	{
		CTile air = {TILE_AIR};
		CLayerTiles::SetTile(x, y, air);
		if(!m_pEditor->m_PreventUnusedTilesWasWarned)
		{
			m_pEditor->m_PopupEventType = m_pEditor->POPEVENT_PREVENTUNUSEDTILES;
			m_pEditor->m_PopupEventActivated = true;
			m_pEditor->m_PreventUnusedTilesWasWarned = true;
		}
	}
}

void CLayerFront::Resize(int NewW, int NewH)
{
	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

CLayerSwitch::CLayerSwitch(int w, int h) :
	CLayerTiles(w, h)
{
	//m_Type = LAYERTYPE_SWITCH;
	str_copy(m_aName, "Switch", sizeof(m_aName));
	m_Switch = 1;

	m_pSwitchTile = new CSwitchTile[w * h];
	mem_zero(m_pSwitchTile, (size_t)w * h * sizeof(CSwitchTile));
}

CLayerSwitch::~CLayerSwitch()
{
	delete[] m_pSwitchTile;
}

void CLayerSwitch::Resize(int NewW, int NewH)
{
	// resize switch data
	CSwitchTile *pNewSwitchData = new CSwitchTile[NewW * NewH];
	mem_zero(pNewSwitchData, (size_t)NewW * NewH * sizeof(CSwitchTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewSwitchData[y * NewW], &m_pSwitchTile[y * m_Width], minimum(m_Width, NewW) * sizeof(CSwitchTile));

	// replace old
	delete[] m_pSwitchTile;
	m_pSwitchTile = pNewSwitchData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerSwitch::Shift(int Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pSwitchTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerSwitch::IsEmpty(CLayerTiles *pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidSwitchTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerSwitch::BrushDraw(CLayer *pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	CLayerSwitch *pSwitchLayer = (CLayerSwitch *)pBrush;
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);
	if(str_comp(pSwitchLayer->m_aFileName, m_pEditor->m_aFileName))
	{
		m_pEditor->m_SwitchNum = pSwitchLayer->m_SwitchNumber;
		m_pEditor->m_SwitchDelay = pSwitchLayer->m_SwitchDelay;
	}

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pSwitchLayer);

	for(int y = 0; y < pSwitchLayer->m_Height; y++)
		for(int x = 0; x < pSwitchLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidSwitchTile(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index)) && pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_SwitchNum != pSwitchLayer->m_SwitchNumber || m_pEditor->m_SwitchDelay != pSwitchLayer->m_SwitchDelay)
				{
					m_pSwitchTile[fy * m_Width + fx].m_Number = m_pEditor->m_SwitchNum;
					m_pSwitchTile[fy * m_Width + fx].m_Delay = m_pEditor->m_SwitchDelay;
				}
				else if(pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Number)
				{
					m_pSwitchTile[fy * m_Width + fx].m_Number = pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Number;
					m_pSwitchTile[fy * m_Width + fx].m_Delay = pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Delay;
				}
				else
				{
					m_pSwitchTile[fy * m_Width + fx].m_Number = m_pEditor->m_SwitchNum;
					m_pSwitchTile[fy * m_Width + fx].m_Delay = m_pEditor->m_SwitchDelay;
				}

				m_pSwitchTile[fy * m_Width + fx].m_Type = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index;
				m_pSwitchTile[fy * m_Width + fx].m_Flags = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Flags;
				m_pTiles[fy * m_Width + fx].m_Index = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index;
				m_pTiles[fy * m_Width + fx].m_Flags = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Flags;
			}
			else
			{
				m_pSwitchTile[fy * m_Width + fx].m_Number = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Type = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Flags = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Delay = 0;
				m_pTiles[fy * m_Width + fx].m_Index = 0;
			}

			if(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index == TILE_FREEZE)
			{
				m_pSwitchTile[fy * m_Width + fx].m_Flags = 0;
			}
			else if(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index == TILE_DFREEZE || pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index == TILE_DUNFREEZE)
			{
				m_pSwitchTile[fy * m_Width + fx].m_Flags = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Delay = 0;
			}
		}
	FlagModified(sx, sy, pSwitchLayer->m_Width, pSwitchLayer->m_Height);
}

void CLayerSwitch::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	BrushFlipXImpl(m_pSwitchTile);
}

void CLayerSwitch::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	BrushFlipYImpl(m_pSwitchTile);
}

void CLayerSwitch::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CSwitchTile *pTempData1 = new CSwitchTile[m_Width * m_Height];
		CTile *pTempData2 = new CTile[m_Width * m_Height];
		mem_copy(pTempData1, m_pSwitchTile, (size_t)m_Width * m_Height * sizeof(CSwitchTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CSwitchTile *pDst1 = m_pSwitchTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[y * m_Width + x];
				*pDst2 = pTempData2[y * m_Width + x];
				if(IsRotatableTile(pDst2->m_Index))
				{
					if(pDst2->m_Flags & TILEFLAG_ROTATE)
						pDst2->m_Flags ^= (TILEFLAG_HFLIP | TILEFLAG_VFLIP);
					pDst2->m_Flags ^= TILEFLAG_ROTATE;
				}
			}

		std::swap(m_Width, m_Height);
		delete[] pTempData1;
		delete[] pTempData2;
	}

	if(Rotation == 2 || Rotation == 3)
	{
		BrushFlipX();
		BrushFlipY();
	}
}

void CLayerSwitch::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerSwitch *pLt = static_cast<CLayerSwitch *>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidSwitchTile((pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]).m_Index)))
			{
				m_pTiles[fy * m_Width + fx].m_Index = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Type = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Number = 0;
				m_pSwitchTile[fy * m_Width + fx].m_Delay = 0;
			}
			else
			{
				m_pTiles[fy * m_Width + fx] = pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)];
				m_pSwitchTile[fy * m_Width + fx].m_Type = m_pTiles[fy * m_Width + fx].m_Index;
				if(pLt->m_Switch && m_pEditor->m_SwitchNum && m_pTiles[fy * m_Width + fx].m_Index > 0)
				{
					if((!pLt->m_pSwitchTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number) || m_pEditor->m_SwitchNum != pLt->m_SwitchNumber)
						m_pSwitchTile[fy * m_Width + fx].m_Number = m_pEditor->m_SwitchNum;
					else
						m_pSwitchTile[fy * m_Width + fx].m_Number = pLt->m_pSwitchTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number;
					if((!pLt->m_pSwitchTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Delay) || m_pEditor->m_SwitchDelay != pLt->m_SwitchDelay)
						m_pSwitchTile[fy * m_Width + fx].m_Delay = m_pEditor->m_SwitchDelay;
					else
						m_pSwitchTile[fy * m_Width + fx].m_Delay = pLt->m_pSwitchTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Delay;
					m_pSwitchTile[fy * m_Width + fx].m_Flags = pLt->m_pSwitchTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Flags;
				}
			}
		}
	}
	FlagModified(sx, sy, w, h);
}

bool CLayerSwitch::ContainsElementWithId(int Id)
{
	for(int y = 0; y < m_Height; ++y)
	{
		for(int x = 0; x < m_Width; ++x)
		{
			if(m_pSwitchTile[y * m_Width + x].m_Number == Id)
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------

CLayerTune::CLayerTune(int w, int h) :
	CLayerTiles(w, h)
{
	//m_Type = LAYERTYPE_TUNE;
	str_copy(m_aName, "Tune", sizeof(m_aName));
	m_Tune = 1;

	m_pTuneTile = new CTuneTile[w * h];
	mem_zero(m_pTuneTile, (size_t)w * h * sizeof(CTuneTile));
}

CLayerTune::~CLayerTune()
{
	delete[] m_pTuneTile;
}

void CLayerTune::Resize(int NewW, int NewH)
{
	// resize Tune data
	CTuneTile *pNewTuneData = new CTuneTile[NewW * NewH];
	mem_zero(pNewTuneData, (size_t)NewW * NewH * sizeof(CTuneTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewTuneData[y * NewW], &m_pTuneTile[y * m_Width], minimum(m_Width, NewW) * sizeof(CTuneTile));

	// replace old
	delete[] m_pTuneTile;
	m_pTuneTile = pNewTuneData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerTune::Shift(int Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pTuneTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerTune::IsEmpty(CLayerTiles *pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidTuneTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerTune::BrushDraw(CLayer *pBrush, float wx, float wy)
{
	if(m_Readonly)
		return;

	CLayerTune *pTuneLayer = (CLayerTune *)pBrush;
	int sx = ConvertX(wx);
	int sy = ConvertY(wy);
	if(str_comp(pTuneLayer->m_aFileName, m_pEditor->m_aFileName))
	{
		m_pEditor->m_TuningNum = pTuneLayer->m_TuningNumber;
	}

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pTuneLayer);

	for(int y = 0; y < pTuneLayer->m_Height; y++)
		for(int x = 0; x < pTuneLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidTuneTile(pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index)) && pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_TuningNum != pTuneLayer->m_TuningNumber)
				{
					m_pTuneTile[fy * m_Width + fx].m_Number = m_pEditor->m_TuningNum;
				}
				else if(pTuneLayer->m_pTuneTile[y * pTuneLayer->m_Width + x].m_Number)
					m_pTuneTile[fy * m_Width + fx].m_Number = pTuneLayer->m_pTuneTile[y * pTuneLayer->m_Width + x].m_Number;
				else
				{
					if(!m_pEditor->m_TuningNum)
					{
						m_pTuneTile[fy * m_Width + fx].m_Number = 0;
						m_pTuneTile[fy * m_Width + fx].m_Type = 0;
						m_pTiles[fy * m_Width + fx].m_Index = 0;
						continue;
					}
					else
						m_pTuneTile[fy * m_Width + fx].m_Number = m_pEditor->m_TuningNum;
				}

				m_pTuneTile[fy * m_Width + fx].m_Type = pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index;
				m_pTiles[fy * m_Width + fx].m_Index = pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index;
			}
			else
			{
				m_pTuneTile[fy * m_Width + fx].m_Number = 0;
				m_pTuneTile[fy * m_Width + fx].m_Type = 0;
				m_pTiles[fy * m_Width + fx].m_Index = 0;
			}
		}
	FlagModified(sx, sy, pTuneLayer->m_Width, pTuneLayer->m_Height);
}

void CLayerTune::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	BrushFlipXImpl(m_pTuneTile);
}

void CLayerTune::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	BrushFlipYImpl(m_pTuneTile);
}

void CLayerTune::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CTuneTile *pTempData1 = new CTuneTile[m_Width * m_Height];
		CTile *pTempData2 = new CTile[m_Width * m_Height];
		mem_copy(pTempData1, m_pTuneTile, (size_t)m_Width * m_Height * sizeof(CTuneTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CTuneTile *pDst1 = m_pTuneTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[y * m_Width + x];
				*pDst2 = pTempData2[y * m_Width + x];
			}

		std::swap(m_Width, m_Height);
		delete[] pTempData1;
		delete[] pTempData2;
	}

	if(Rotation == 2 || Rotation == 3)
	{
		BrushFlipX();
		BrushFlipY();
	}
}

void CLayerTune::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerTune *pLt = static_cast<CLayerTune *>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || IsEmpty(pLt);

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidTuneTile((pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)]).m_Index))) // \o/ this fixes editor bug; TODO: use IsUsedInThisLayer here
			{
				m_pTiles[fy * m_Width + fx].m_Index = 0;
				m_pTuneTile[fy * m_Width + fx].m_Type = 0;
				m_pTuneTile[fy * m_Width + fx].m_Number = 0;
			}
			else
			{
				m_pTiles[fy * m_Width + fx] = pLt->m_pTiles[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)];
				if(pLt->m_Tune && m_pTiles[fy * m_Width + fx].m_Index > 0)
				{
					if((!pLt->m_pTuneTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number && m_pEditor->m_TuningNum) || m_pEditor->m_TuningNum != pLt->m_TuningNumber)
						m_pTuneTile[fy * m_Width + fx].m_Number = m_pEditor->m_TuningNum;
					else
						m_pTuneTile[fy * m_Width + fx].m_Number = pLt->m_pTuneTile[(y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height)].m_Number;
					m_pTuneTile[fy * m_Width + fx].m_Type = m_pTiles[fy * m_Width + fx].m_Index;
				}
			}
		}
	}

	FlagModified(sx, sy, w, h);
}
