#include "layer_tune.h"

#include <game/editor/editor.h>

CLayerTune::CLayerTune(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId) :
	CLayerTiles(pMap, w, h, RetainedId)
{
	str_copy(m_aName, "Tune");
	m_HasTune = true;

	m_TuneTiles.Resize(w, h);

	m_GotoTuneOffset = 0;
	m_GotoTuneLastPos = ivec2(-1, -1);
}

CLayerTune::CLayerTune(const CLayerTune &Other) :
	CLayerTiles(Other),
	CLayerTuneValues(Other)
{
	str_copy(m_aName, "Tune copy");
	m_HasTune = true;
}

CLayerTune::~CLayerTune() = default;

void CLayerTune::Resize(int NewW, int NewH)
{
	m_TuneTiles.Resize(NewW, NewH);

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(Map()->m_pGameLayer->Width() != NewW || Map()->m_pGameLayer->Height() != NewH)
		Map()->m_pGameLayer->Resize(NewW, NewH);
}

void CLayerTune::Shift(EShiftDirection Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_TuneTiles, Direction, Map()->m_ShiftBy);
}

bool CLayerTune::IsEmpty() const
{
	for(int y = 0; y < Height(); y++)
	{
		for(int x = 0; x < Width(); x++)
		{
			const int Index = GetTile(x, y).m_Index;
			if(Index == 0)
			{
				continue;
			}
			if(Editor()->IsAllowPlaceUnusedTiles() || IsValidTuneTile(Index))
			{
				return false;
			}
		}
	}
	return true;
}

void CLayerTune::BrushDraw(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerTune *pTuneLayer = static_cast<CLayerTune *>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
	if(str_comp(pTuneLayer->m_aFilename, pTuneLayer->Map()->m_aFilename))
	{
		Editor()->m_TuningNumber = pTuneLayer->m_TuningNumber;
	}

	bool Destructive = Editor()->m_BrushDrawDestructive || pTuneLayer->IsEmpty();

	for(int y = 0; y < pTuneLayer->Height(); y++)
		for(int x = 0; x < pTuneLayer->Width(); x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = y * pTuneLayer->Width() + x;
			const int TgtIndex = fy * Width() + fx;

			if((Editor()->IsAllowPlaceUnusedTiles() || IsValidTuneTile(pTuneLayer->m_Tiles[SrcIndex].m_Index)) && pTuneLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
			{
				if(Editor()->m_TuningNumber != pTuneLayer->m_TuningNumber)
				{
					m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_TuningNumber; });
				}
				else if(pTuneLayer->m_TuneTiles[SrcIndex].m_Number)
					m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pTuneLayer->m_TuneTiles[SrcIndex].m_Number; });
				else
				{
					if(!Editor()->m_TuningNumber)
					{
						m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
						m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
						m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

						continue;
					}
					else
						m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_TuningNumber; });
				}

				m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pTuneLayer->m_Tiles[SrcIndex].m_Index; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pTuneLayer->m_Tiles[SrcIndex].m_Index; });
			}
			else
			{
				m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

				if(pTuneLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}
		}
	FlagModified(sx, sy, pTuneLayer->Width(), pTuneLayer->Height());
}

void CLayerTune::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	m_TuneTiles.FlipX();
}

void CLayerTune::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	m_TuneTiles.FlipY();
}

void CLayerTune::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		m_TuneTiles.RotateClockwise();
		m_Tiles.RotateClockwise();
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

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerTune *pLt = static_cast<CLayerTune *>(pBrush);

	bool Destructive = Editor()->m_BrushDrawDestructive || Empty || pLt->IsEmpty();

	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = Empty ? 0 : (y * pLt->Width() + x % pLt->Width()) % (pLt->Width() * pLt->Height());
			const int TgtIndex = fy * Width() + fx;

			if(Empty || (!Editor()->IsAllowPlaceUnusedTiles() && !IsValidTuneTile((pLt->m_Tiles[SrcIndex]).m_Index)))
			{
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
				m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_Tiles.Set(TgtIndex, pLt->m_Tiles[SrcIndex]);
				if(pLt->m_HasTune && m_Tiles[TgtIndex].m_Index > 0)
				{
					m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = m_Tiles[fy * Width() + fx].m_Index; });

					if((pLt->m_TuneTiles[SrcIndex].m_Number == 0 && Editor()->m_TuningNumber) || Editor()->m_TuningNumber != pLt->m_TuningNumber)
						m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_TuningNumber; });
					else
						m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pLt->m_TuneTiles[SrcIndex].m_Number; });
				}
				else
				{
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
					m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
					m_TuneTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				}
			}
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
	for(int y = 0; y < Height(); ++y)
	{
		for(int x = 0; x < Width(); ++x)
		{
			if(IsValidTuneTile(m_TuneTiles[y * Width() + x].m_Type) && m_TuneTiles[y * Width() + x].m_Number == Id)
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
		for(int x = 0; x < Width(); x++)
		{
			for(int y = 0; y < Height(); y++)
			{
				int i = y * Width() + x;
				int Tune = m_TuneTiles[i].m_Number;
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
