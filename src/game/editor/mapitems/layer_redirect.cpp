#include "layer_redirect.h"

#include <game/editor/editor.h>

CLayerRedirect::CLayerRedirect(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Redirect");
	m_Redirect = 1;

	m_pRedirectTile = new CRedirectTile[w * h];
	mem_zero(m_pRedirectTile, (size_t)w * h * sizeof(CRedirectTile));
}

CLayerRedirect::CLayerRedirect(const CLayerRedirect &Other) :
	CLayerTiles(Other)
{
	str_copy(m_aName, "Redirect copy");
	m_Redirect = 1;

	m_pRedirectTile = new CRedirectTile[m_Width * m_Height];
	mem_copy(m_pRedirectTile, Other.m_pRedirectTile, (size_t)m_Width * m_Height * sizeof(CRedirectTile));
}

CLayerRedirect::~CLayerRedirect()
{
	delete[] m_pRedirectTile;
}

void CLayerRedirect::Resize(int NewW, int NewH)
{
	// resize redirect data
	CRedirectTile *pNewRedirectData = new CRedirectTile[NewW * NewH];
	mem_zero(pNewRedirectData, (size_t)NewW * NewH * sizeof(CRedirectTile));

	// copy old data
	for(int y = 0; y < minimum(NewH, m_Height); y++)
		mem_copy(&pNewRedirectData[y * NewW], &m_pRedirectTile[y * m_Width], minimum(m_Width, NewW) * sizeof(CRedirectTile));

	// replace old
	delete[] m_pRedirectTile;
	m_pRedirectTile = pNewRedirectData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerRedirect::Shift(int Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pRedirectTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerRedirect::IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidRedirectTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerRedirect::BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	std::shared_ptr<CLayerRedirect> pRedirectLayer = std::static_pointer_cast<CLayerRedirect>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
	if(str_comp(pRedirectLayer->m_aFileName, m_pEditor->m_aFileName))
	{
		m_pEditor->m_RedirectPort = pRedirectLayer->m_RedirectPort;
	}

	bool Destructive = m_pEditor->m_BrushDrawDestructive || IsEmpty(pRedirectLayer);

	for(int y = 0; y < pRedirectLayer->m_Height; y++)
		for(int x = 0; x < pRedirectLayer->m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_Width || fy < 0 || fy >= m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			int Index = (fy * m_Width) + fx;
			SRedirectTileStateChange::SData Previous{
				m_pRedirectTile[Index].m_Type,
				m_pRedirectTile[Index].m_Port,
				m_pTiles[Index].m_Index};

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidRedirectTile(pRedirectLayer->m_pTiles[(y * pRedirectLayer->m_Width) + x].m_Index)) && pRedirectLayer->m_pTiles[(y * pRedirectLayer->m_Width) + x].m_Index != TILE_AIR)
			{
				// TODO: this code is more complicated in other layers
				m_pRedirectTile[Index] = {
					pRedirectLayer->m_pTiles[(y * pRedirectLayer->m_Width) + x].m_Index,
					m_pEditor->m_RedirectPort};
				m_pTiles[Index].m_Index = pRedirectLayer->m_pTiles[(y * pRedirectLayer->m_Width) + x].m_Index;
			}
			else
			{
				m_pRedirectTile[Index] = {};
				m_pTiles[Index].m_Index = 0;

				if(pRedirectLayer->m_pTiles[(y * pRedirectLayer->m_Width) + x].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}

			SRedirectTileStateChange::SData Current{
				m_pRedirectTile[Index].m_Type,
				m_pRedirectTile[Index].m_Port,
				m_pTiles[Index].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	FlagModified(sx, sy, pRedirectLayer->m_Width, pRedirectLayer->m_Height);
}

void CLayerRedirect::RecordStateChange(int x, int y, SRedirectTileStateChange::SData Previous, SRedirectTileStateChange::SData Current)
{
	if(!m_History[y][x].m_Changed)
		m_History[y][x] = SRedirectTileStateChange{true, Previous, Current};
	else
		m_History[y][x].m_Current = Current;
}

void CLayerRedirect::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	BrushFlipXImpl(m_pRedirectTile);
}

void CLayerRedirect::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	BrushFlipYImpl(m_pRedirectTile);
}

void CLayerRedirect::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		// 90° rotation
		CRedirectTile *pTempData1 = new CRedirectTile[m_Width * m_Height];
		CTile *pTempData2 = new CTile[m_Width * m_Height];
		mem_copy(pTempData1, m_pRedirectTile, (size_t)m_Width * m_Height * sizeof(CRedirectTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_Width * m_Height * sizeof(CTile));
		CRedirectTile *pDst1 = m_pRedirectTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_Width; ++x)
			for(int y = m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[(y * m_Width) + x];
				*pDst2 = pTempData2[(y * m_Width) + x];
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

void CLayerRedirect::FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerRedirect> pLt = std::static_pointer_cast<CLayerRedirect>(pBrush);

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

			const int SrcIndex = Empty ? 0 : (y * pLt->m_Width + x % pLt->m_Width) % (pLt->m_Width * pLt->m_Height);
			const int TgtIndex = (fy * m_Width) + fx;

			SRedirectTileStateChange::SData Previous{
				m_pRedirectTile[TgtIndex].m_Type,
				m_pRedirectTile[TgtIndex].m_Port,
				m_pTiles[TgtIndex].m_Index};

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidRedirectTile((pLt->m_pTiles[SrcIndex]).m_Index))) // no redirect tile chosen: reset
			{
				m_pRedirectTile[TgtIndex] = {};
				m_pTiles[TgtIndex].m_Index = 0;

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_pTiles[TgtIndex] = pLt->m_pTiles[SrcIndex];
				if(pLt->m_Redirect && m_pTiles[TgtIndex].m_Index > 0)
				{
					m_pRedirectTile[TgtIndex] = {
						m_pTiles[TgtIndex].m_Index,
						m_pEditor->m_RedirectPort};
				}
			}

			SRedirectTileStateChange::SData Current{
				m_pRedirectTile[TgtIndex].m_Type,
				m_pRedirectTile[TgtIndex].m_Port,
				m_pTiles[TgtIndex].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	}
	FlagModified(sx, sy, w, h);
}

std::shared_ptr<CLayer> CLayerRedirect::Duplicate() const
{
	return std::make_shared<CLayerRedirect>(*this);
}

const char *CLayerRedirect::TypeName() const
{
	return "redirect";
}
