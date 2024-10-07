#include "layer_speedup.h"

#include <game/editor/editor.h>

CLayerSpeedup::CLayerSpeedup(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Speedup");
	m_Speedup = 1;

	m_pSpeedupTile = new CSpeedupTile[w * h];
	mem_zero(m_pSpeedupTile, (size_t)w * h * sizeof(CSpeedupTile));
}

CLayerSpeedup::CLayerSpeedup(const CLayerSpeedup &Other) :
	CLayerTiles(Other)
{
	str_copy(m_aName, "Speedup copy");
	m_Speedup = 1;

	m_pSpeedupTile = new CSpeedupTile[m_Width * m_Height];
	mem_copy(m_pSpeedupTile, Other.m_pSpeedupTile, (size_t)m_Width * m_Height * sizeof(CSpeedupTile));
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

bool CLayerSpeedup::IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidSpeedupTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerSpeedup::BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	std::shared_ptr<CLayerSpeedup> pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
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

			int Index = fy * m_Width + fx;
			SSpeedupTileStateChange::SData Previous{
				m_pSpeedupTile[Index].m_Force,
				m_pSpeedupTile[Index].m_Angle,
				m_pSpeedupTile[Index].m_MaxSpeed,
				m_pSpeedupTile[Index].m_Type,
				m_pTiles[Index].m_Index};

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidSpeedupTile(pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index)) && pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_SpeedupAngle != pSpeedupLayer->m_SpeedupAngle || m_pEditor->m_SpeedupForce != pSpeedupLayer->m_SpeedupForce || m_pEditor->m_SpeedupMaxSpeed != pSpeedupLayer->m_SpeedupMaxSpeed)
				{
					m_pSpeedupTile[Index].m_Force = m_pEditor->m_SpeedupForce;
					m_pSpeedupTile[Index].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					m_pSpeedupTile[Index].m_Angle = m_pEditor->m_SpeedupAngle;
					m_pSpeedupTile[Index].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[Index].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else if(pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Force)
				{
					m_pSpeedupTile[Index].m_Force = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Force;
					m_pSpeedupTile[Index].m_Angle = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_Angle;
					m_pSpeedupTile[Index].m_MaxSpeed = pSpeedupLayer->m_pSpeedupTile[y * pSpeedupLayer->m_Width + x].m_MaxSpeed;
					m_pSpeedupTile[Index].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[Index].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else if(m_pEditor->m_SpeedupForce)
				{
					m_pSpeedupTile[Index].m_Force = m_pEditor->m_SpeedupForce;
					m_pSpeedupTile[Index].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					m_pSpeedupTile[Index].m_Angle = m_pEditor->m_SpeedupAngle;
					m_pSpeedupTile[Index].m_Type = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
					m_pTiles[Index].m_Index = pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index;
				}
				else
				{
					m_pSpeedupTile[Index].m_Force = 0;
					m_pSpeedupTile[Index].m_MaxSpeed = 0;
					m_pSpeedupTile[Index].m_Angle = 0;
					m_pSpeedupTile[Index].m_Type = 0;
					m_pTiles[Index].m_Index = 0;
				}
			}
			else
			{
				m_pSpeedupTile[Index].m_Force = 0;
				m_pSpeedupTile[Index].m_MaxSpeed = 0;
				m_pSpeedupTile[Index].m_Angle = 0;
				m_pSpeedupTile[Index].m_Type = 0;
				m_pTiles[Index].m_Index = 0;

				if(pSpeedupLayer->m_pTiles[y * pSpeedupLayer->m_Width + x].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}

			SSpeedupTileStateChange::SData Current{
				m_pSpeedupTile[Index].m_Force,
				m_pSpeedupTile[Index].m_Angle,
				m_pSpeedupTile[Index].m_MaxSpeed,
				m_pSpeedupTile[Index].m_Type,
				m_pTiles[Index].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	FlagModified(sx, sy, pSpeedupLayer->m_Width, pSpeedupLayer->m_Height);
}

void CLayerSpeedup::RecordStateChange(int x, int y, SSpeedupTileStateChange::SData Previous, SSpeedupTileStateChange::SData Current)
{
	if(!m_History[y][x].m_Changed)
		m_History[y][x] = SSpeedupTileStateChange{true, Previous, Current};
	else
		m_History[y][x].m_Current = Current;
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

void CLayerSpeedup::FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerSpeedup> pLt = std::static_pointer_cast<CLayerSpeedup>(pBrush);

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
			const int TgtIndex = fy * m_Width + fx;

			SSpeedupTileStateChange::SData Previous{
				m_pSpeedupTile[TgtIndex].m_Force,
				m_pSpeedupTile[TgtIndex].m_Angle,
				m_pSpeedupTile[TgtIndex].m_MaxSpeed,
				m_pSpeedupTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidSpeedupTile((pLt->m_pTiles[SrcIndex]).m_Index))) // no speed up tile chosen: reset
			{
				m_pTiles[TgtIndex].m_Index = 0;
				m_pSpeedupTile[TgtIndex].m_Force = 0;
				m_pSpeedupTile[TgtIndex].m_Angle = 0;

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_pTiles[TgtIndex] = pLt->m_pTiles[SrcIndex];
				if(pLt->m_Speedup && m_pTiles[TgtIndex].m_Index > 0)
				{
					m_pSpeedupTile[TgtIndex].m_Type = m_pTiles[TgtIndex].m_Index;

					if((pLt->m_pSpeedupTile[SrcIndex].m_Force == 0 && m_pEditor->m_SpeedupForce) || m_pEditor->m_SpeedupForce != pLt->m_SpeedupForce)
						m_pSpeedupTile[TgtIndex].m_Force = m_pEditor->m_SpeedupForce;
					else
						m_pSpeedupTile[TgtIndex].m_Force = pLt->m_pSpeedupTile[SrcIndex].m_Force;

					if((pLt->m_pSpeedupTile[SrcIndex].m_Angle == 0 && m_pEditor->m_SpeedupAngle) || m_pEditor->m_SpeedupAngle != pLt->m_SpeedupAngle)
						m_pSpeedupTile[TgtIndex].m_Angle = m_pEditor->m_SpeedupAngle;
					else
						m_pSpeedupTile[TgtIndex].m_Angle = pLt->m_pSpeedupTile[SrcIndex].m_Angle;

					if((pLt->m_pSpeedupTile[SrcIndex].m_MaxSpeed == 0 && m_pEditor->m_SpeedupMaxSpeed) || m_pEditor->m_SpeedupMaxSpeed != pLt->m_SpeedupMaxSpeed)
						m_pSpeedupTile[TgtIndex].m_MaxSpeed = m_pEditor->m_SpeedupMaxSpeed;
					else
						m_pSpeedupTile[TgtIndex].m_MaxSpeed = pLt->m_pSpeedupTile[SrcIndex].m_MaxSpeed;
				}
			}

			SSpeedupTileStateChange::SData Current{
				m_pSpeedupTile[TgtIndex].m_Force,
				m_pSpeedupTile[TgtIndex].m_Angle,
				m_pSpeedupTile[TgtIndex].m_MaxSpeed,
				m_pSpeedupTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	}
	FlagModified(sx, sy, w, h);
}

std::shared_ptr<CLayer> CLayerSpeedup::Duplicate() const
{
	return std::make_shared<CLayerSpeedup>(*this);
}

const char *CLayerSpeedup::TypeName() const
{
	return "speedup";
}
