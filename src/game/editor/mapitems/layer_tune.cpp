#include "layer_tune.h"

#include <game/editor/editor.h>

CLayerTune::CLayerTune(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Tune");
	m_Tune = 1;

	m_pTuneTile = new CTuneTile[w * h];
	mem_zero(m_pTuneTile, (size_t)w * h * sizeof(CTuneTile));
}

CLayerTune::CLayerTune(const CLayerTune &Other) :
	CLayerTiles(Other)
{
	str_copy(m_aName, "Tune copy");
	m_Tune = 1;

	m_pTuneTile = new CTuneTile[m_Width * m_Height];
	mem_copy(m_pTuneTile, Other.m_pTuneTile, (size_t)m_Width * m_Height * sizeof(CTuneTile));
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

bool CLayerTune::IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidTuneTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerTune::BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	std::shared_ptr<CLayerTune> pTuneLayer = std::static_pointer_cast<CLayerTune>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
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

			int Index = fy * m_Width + fx;
			STuneTileStateChange::SData Previous{
				m_pTuneTile[Index].m_Number,
				m_pTuneTile[Index].m_Type,
				m_pTiles[Index].m_Index};

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidTuneTile(pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index)) && pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_TuningNum != pTuneLayer->m_TuningNumber)
				{
					m_pTuneTile[Index].m_Number = m_pEditor->m_TuningNum;
				}
				else if(pTuneLayer->m_pTuneTile[y * pTuneLayer->m_Width + x].m_Number)
					m_pTuneTile[Index].m_Number = pTuneLayer->m_pTuneTile[y * pTuneLayer->m_Width + x].m_Number;
				else
				{
					if(!m_pEditor->m_TuningNum)
					{
						m_pTuneTile[Index].m_Number = 0;
						m_pTuneTile[Index].m_Type = 0;
						m_pTiles[Index].m_Index = 0;
						continue;
					}
					else
						m_pTuneTile[Index].m_Number = m_pEditor->m_TuningNum;
				}

				m_pTuneTile[Index].m_Type = pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index;
				m_pTiles[Index].m_Index = pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index;
			}
			else
			{
				m_pTuneTile[Index].m_Number = 0;
				m_pTuneTile[Index].m_Type = 0;
				m_pTiles[Index].m_Index = 0;

				if(pTuneLayer->m_pTiles[y * pTuneLayer->m_Width + x].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}

			STuneTileStateChange::SData Current{
				m_pTuneTile[Index].m_Number,
				m_pTuneTile[Index].m_Type,
				m_pTiles[Index].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	FlagModified(sx, sy, pTuneLayer->m_Width, pTuneLayer->m_Height);
}

void CLayerTune::RecordStateChange(int x, int y, STuneTileStateChange::SData Previous, STuneTileStateChange::SData Current)
{
	if(!m_History[y][x].m_Changed)
		m_History[y][x] = STuneTileStateChange{true, Previous, Current};
	else
		m_History[y][x].m_Current = Current;
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

void CLayerTune::FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerTune> pLt = std::static_pointer_cast<CLayerTune>(pBrush);

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

			STuneTileStateChange::SData Previous{
				m_pTuneTile[TgtIndex].m_Number,
				m_pTuneTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidTuneTile((pLt->m_pTiles[SrcIndex]).m_Index)))
			{
				m_pTiles[TgtIndex].m_Index = 0;
				m_pTuneTile[TgtIndex].m_Type = 0;
				m_pTuneTile[TgtIndex].m_Number = 0;

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_pTiles[TgtIndex] = pLt->m_pTiles[SrcIndex];
				if(pLt->m_Tune && m_pTiles[TgtIndex].m_Index > 0)
				{
					m_pTuneTile[TgtIndex].m_Type = m_pTiles[fy * m_Width + fx].m_Index;

					if((pLt->m_pTuneTile[SrcIndex].m_Number == 0 && m_pEditor->m_TuningNum) || m_pEditor->m_TuningNum != pLt->m_TuningNumber)
						m_pTuneTile[TgtIndex].m_Number = m_pEditor->m_TuningNum;
					else
						m_pTuneTile[TgtIndex].m_Number = pLt->m_pTuneTile[SrcIndex].m_Number;
				}
			}

			STuneTileStateChange::SData Current{
				m_pTuneTile[TgtIndex].m_Number,
				m_pTuneTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	}

	FlagModified(sx, sy, w, h);
}

std::shared_ptr<CLayer> CLayerTune::Duplicate() const
{
	return std::make_shared<CLayerTune>(*this);
}

const char *CLayerTune::TypeName() const
{
	return "tune";
}
