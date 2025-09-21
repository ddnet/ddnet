#include "layer_tune.h"

#include <game/editor/editor.h>

CLayerTune::CLayerTune(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Tune");
	m_HasTune = true;

	m_pTuneTile = new CTuneTile[w * h];
	mem_zero(m_pTuneTile, (size_t)w * h * sizeof(CTuneTile));

	m_GotoTuneOffset = 0;
	m_GotoTuneLastPos = ivec2(-1, -1);
}

CLayerTune::CLayerTune(const CLayerTune &Other) :
	CLayerTiles(Other)
{
	str_copy(m_aName, "Tune copy");
	m_HasTune = true;

	m_pTuneTile = new CTuneTile[m_LayerTilemap.m_Width * m_LayerTilemap.m_Height];
	mem_copy(m_pTuneTile, Other.m_pTuneTile, (size_t)m_LayerTilemap.m_Width * m_LayerTilemap.m_Height * sizeof(CTuneTile));
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
	for(int y = 0; y < minimum(NewH, m_LayerTilemap.m_Height); y++)
		mem_copy(&pNewTuneData[y * NewW], &m_pTuneTile[y * m_LayerTilemap.m_Width], minimum(m_LayerTilemap.m_Width, NewW) * sizeof(CTuneTile));

	// replace old
	delete[] m_pTuneTile;
	m_pTuneTile = pNewTuneData;

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_LayerTilemap.m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_LayerTilemap.m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

void CLayerTune::Shift(EShiftDirection Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_pTuneTile, Direction, m_pEditor->m_ShiftBy);
}

bool CLayerTune::IsEmpty() const
{
	for(int y = 0; y < m_LayerTilemap.m_Height; y++)
	{
		for(int x = 0; x < m_LayerTilemap.m_Width; x++)
		{
			const int Index = GetTile(x, y).m_Index;
			if(Index == 0)
			{
				continue;
			}
			if(m_pEditor->IsAllowPlaceUnusedTiles() || IsValidTuneTile(Index))
			{
				return false;
			}
		}
	}
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

	bool Destructive = m_pEditor->m_BrushDrawDestructive || pTuneLayer->IsEmpty();

	for(int y = 0; y < pTuneLayer->m_LayerTilemap.m_Height; y++)
		for(int x = 0; x < pTuneLayer->m_LayerTilemap.m_Width; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_LayerTilemap.m_Width || fy < 0 || fy >= m_LayerTilemap.m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = y * pTuneLayer->m_LayerTilemap.m_Width + x;
			const int TgtIndex = fy * m_LayerTilemap.m_Width + fx;

			STuneTileStateChange::SData Previous{
				m_pTuneTile[TgtIndex].m_Number,
				m_pTuneTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			if((m_pEditor->IsAllowPlaceUnusedTiles() || IsValidTuneTile(pTuneLayer->m_pTiles[SrcIndex].m_Index)) && pTuneLayer->m_pTiles[SrcIndex].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_TuningNum != pTuneLayer->m_TuningNumber)
				{
					m_pTuneTile[TgtIndex].m_Number = m_pEditor->m_TuningNum;
				}
				else if(pTuneLayer->m_pTuneTile[SrcIndex].m_Number)
					m_pTuneTile[TgtIndex].m_Number = pTuneLayer->m_pTuneTile[SrcIndex].m_Number;
				else
				{
					if(!m_pEditor->m_TuningNum)
					{
						m_pTuneTile[TgtIndex].m_Number = 0;
						m_pTuneTile[TgtIndex].m_Type = 0;
						m_pTiles[TgtIndex].m_Index = 0;
						continue;
					}
					else
						m_pTuneTile[TgtIndex].m_Number = m_pEditor->m_TuningNum;
				}

				m_pTuneTile[TgtIndex].m_Type = pTuneLayer->m_pTiles[SrcIndex].m_Index;
				m_pTiles[TgtIndex].m_Index = pTuneLayer->m_pTiles[SrcIndex].m_Index;
			}
			else
			{
				m_pTuneTile[TgtIndex].m_Number = 0;
				m_pTuneTile[TgtIndex].m_Type = 0;
				m_pTiles[TgtIndex].m_Index = 0;

				if(pTuneLayer->m_pTiles[SrcIndex].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}

			STuneTileStateChange::SData Current{
				m_pTuneTile[TgtIndex].m_Number,
				m_pTuneTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	FlagModified(sx, sy, pTuneLayer->m_LayerTilemap.m_Width, pTuneLayer->m_LayerTilemap.m_Height);
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
		CTuneTile *pTempData1 = new CTuneTile[m_LayerTilemap.m_Width * m_LayerTilemap.m_Height];
		CTile *pTempData2 = new CTile[m_LayerTilemap.m_Width * m_LayerTilemap.m_Height];
		mem_copy(pTempData1, m_pTuneTile, (size_t)m_LayerTilemap.m_Width * m_LayerTilemap.m_Height * sizeof(CTuneTile));
		mem_copy(pTempData2, m_pTiles, (size_t)m_LayerTilemap.m_Width * m_LayerTilemap.m_Height * sizeof(CTile));
		CTuneTile *pDst1 = m_pTuneTile;
		CTile *pDst2 = m_pTiles;
		for(int x = 0; x < m_LayerTilemap.m_Width; ++x)
			for(int y = m_LayerTilemap.m_Height - 1; y >= 0; --y, ++pDst1, ++pDst2)
			{
				*pDst1 = pTempData1[y * m_LayerTilemap.m_Width + x];
				*pDst2 = pTempData2[y * m_LayerTilemap.m_Width + x];
			}

		std::swap(m_LayerTilemap.m_Width, m_LayerTilemap.m_Height);
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

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerTune> pLt = std::static_pointer_cast<CLayerTune>(pBrush);

	bool Destructive = m_pEditor->m_BrushDrawDestructive || Empty || pLt->IsEmpty();

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= m_LayerTilemap.m_Width || fy < 0 || fy >= m_LayerTilemap.m_Height)
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = Empty ? 0 : (y * pLt->m_LayerTilemap.m_Width + x % pLt->m_LayerTilemap.m_Width) % (pLt->m_LayerTilemap.m_Width * pLt->m_LayerTilemap.m_Height);
			const int TgtIndex = fy * m_LayerTilemap.m_Width + fx;

			STuneTileStateChange::SData Previous{
				m_pTuneTile[TgtIndex].m_Number,
				m_pTuneTile[TgtIndex].m_Type,
				m_pTiles[TgtIndex].m_Index};

			if(Empty || (!m_pEditor->IsAllowPlaceUnusedTiles() && !IsValidTuneTile((pLt->m_pTiles[SrcIndex]).m_Index)))
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
				if(pLt->m_HasTune && m_pTiles[TgtIndex].m_Index > 0)
				{
					m_pTuneTile[TgtIndex].m_Type = m_pTiles[fy * m_LayerTilemap.m_Width + fx].m_Index;

					if((pLt->m_pTuneTile[SrcIndex].m_Number == 0 && m_pEditor->m_TuningNum) || m_pEditor->m_TuningNum != pLt->m_TuningNumber)
						m_pTuneTile[TgtIndex].m_Number = m_pEditor->m_TuningNum;
					else
						m_pTuneTile[TgtIndex].m_Number = pLt->m_pTuneTile[SrcIndex].m_Number;
				}
				else
				{
					m_pTiles[TgtIndex].m_Index = 0;
					m_pTuneTile[TgtIndex].m_Type = 0;
					m_pTuneTile[TgtIndex].m_Number = 0;
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

int CLayerTune::FindNextFreeNumber() const
{
	for(int i = 1; i <= 255; i++)
	{
		if(!ContainsElementWithId(i))
		{
			return i;
		}
	}
	return -1;
}

bool CLayerTune::ContainsElementWithId(int Id) const
{
	for(int y = 0; y < m_LayerTilemap.m_Height; ++y)
	{
		for(int x = 0; x < m_LayerTilemap.m_Width; ++x)
		{
			if(IsValidTuneTile(m_pTuneTile[y * m_LayerTilemap.m_Width + x].m_Type) && m_pTuneTile[y * m_LayerTilemap.m_Width + x].m_Number == Id)
			{
				return true;
			}
		}
	}

	return false;
}

void CLayerTune::GetPos(int Number, int Offset, ivec2 &Pos)
{
	int Match = -1;
	ivec2 MatchPos = ivec2(-1, -1);
	Pos = ivec2(-1, -1);

	auto FindTile = [this, &Match, &MatchPos, &Number, &Offset]() {
		for(int x = 0; x < m_LayerTilemap.m_Width; x++)
		{
			for(int y = 0; y < m_LayerTilemap.m_Height; y++)
			{
				int i = y * m_LayerTilemap.m_Width + x;
				int Tune = m_pTuneTile[i].m_Number;
				if(Number == Tune)
				{
					Match++;
					if(Offset != -1)
					{
						if(Match == Offset)
						{
							MatchPos = ivec2(x, y);
							m_GotoTuneOffset = Match;
							return;
						}
						continue;
					}
					MatchPos = ivec2(x, y);
					if(m_GotoTuneLastPos != ivec2(-1, -1))
					{
						if(distance(m_GotoTuneLastPos, MatchPos) < 10.0f)
						{
							m_GotoTuneOffset++;
							continue;
						}
					}
					m_GotoTuneLastPos = MatchPos;
					if(Match == m_GotoTuneOffset)
						return;
				}
			}
		}
	};
	FindTile();

	if(MatchPos == ivec2(-1, -1))
		return;
	if(Match < m_GotoTuneOffset)
		m_GotoTuneOffset = -1;
	Pos = MatchPos;
	m_GotoTuneOffset++;
}

std::shared_ptr<CLayer> CLayerTune::Duplicate() const
{
	return std::make_shared<CLayerTune>(*this);
}

const char *CLayerTune::TypeName() const
{
	return "tune";
}
