#include "layer_tele.h"

#include <game/editor/editor.h>

CLayerTele::CLayerTele(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId) :
	CLayerTiles(pMap, w, h, RetainedId)
{
	str_copy(m_aName, "Tele");
	m_HasTele = true;

	m_TeleTiles.Resize(w, h);

	m_GotoTeleOffset = 0;
	m_GotoTeleLastPos = ivec2(-1, -1);
}

CLayerTele::CLayerTele(const CLayerTele &Other) :
	CLayerTiles(Other),
	CLayerTeleValues(Other)
{
	str_copy(m_aName, "Tele copy");
	m_HasTele = true;
}

CLayerTele::~CLayerTele() = default;

void CLayerTele::Resize(int NewW, int NewH)
{
	m_TeleTiles.Resize(NewW, NewH);

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(Map()->m_pGameLayer->Width() != NewW || Map()->m_pGameLayer->Height() != NewH)
		Map()->m_pGameLayer->Resize(NewW, NewH);
}

void CLayerTele::Shift(EShiftDirection Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_TeleTiles, Direction, Map()->m_ShiftBy);
}

bool CLayerTele::IsEmpty() const
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
			if(Editor()->IsAllowPlaceUnusedTiles() || IsValidTeleTile(Index))
			{
				return false;
			}
		}
	}
	return true;
}

void CLayerTele::BrushDraw(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerTele *pTeleLayer = static_cast<CLayerTele *>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
	if(str_comp(pTeleLayer->m_aFilename, pTeleLayer->Map()->m_aFilename))
		Editor()->m_TeleNumber = pTeleLayer->m_TeleNumber;

	bool Destructive = Editor()->m_BrushDrawDestructive || pTeleLayer->IsEmpty();

	for(int y = 0; y < pTeleLayer->Height(); y++)
		for(int x = 0; x < pTeleLayer->Width(); x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = y * pTeleLayer->Width() + x;
			const int TgtIndex = fy * Width() + fx;

			if((Editor()->IsAllowPlaceUnusedTiles() || IsValidTeleTile(pTeleLayer->m_Tiles[SrcIndex].m_Index)) && pTeleLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
			{
				bool IsCheckpoint = IsTeleTileCheckpoint(pTeleLayer->m_Tiles[SrcIndex].m_Index);
				if(!IsCheckpoint && !IsTeleTileNumberUsed(pTeleLayer->m_Tiles[SrcIndex].m_Index, false))
				{
					// Tele tile number is unused. Set a known value which is not 0,
					// as tiles with number 0 would be ignored by previous versions.
					m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 255; });
				}
				else if(pTeleLayer->m_TeleTiles[SrcIndex].m_Number)
				{
					m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pTeleLayer->m_TeleTiles[SrcIndex].m_Number; });
				}
				else
				{
					if((!IsCheckpoint && !Editor()->m_TeleNumber) || (IsCheckpoint && !Editor()->m_TeleCheckpointNumber))
					{
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
						m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

						continue;
					}
					else
					{
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = IsCheckpoint ? Editor()->m_TeleCheckpointNumber : Editor()->m_TeleNumber; });
					}
				}

				m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pTeleLayer->m_Tiles[SrcIndex].m_Index; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pTeleLayer->m_Tiles[SrcIndex].m_Index; });
			}
			else
			{
				m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

				if(pTeleLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}
		}
	FlagModified(sx, sy, pTeleLayer->Width(), pTeleLayer->Height());
}

void CLayerTele::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	m_TeleTiles.FlipX();
}

void CLayerTele::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	m_TeleTiles.FlipY();
}

void CLayerTele::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		m_TeleTiles.RotateClockwise();
		m_Tiles.RotateClockwise();
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

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerTele *pLt = static_cast<CLayerTele *>(pBrush);

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

			if(Empty || (!Editor()->IsAllowPlaceUnusedTiles() && !IsValidTeleTile((pLt->m_Tiles[SrcIndex]).m_Index)))
			{
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
				m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_Tiles.Set(TgtIndex, pLt->m_Tiles[SrcIndex]);
				if(pLt->m_HasTele && m_Tiles[TgtIndex].m_Index > 0)
				{
					m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = m_Tiles[TgtIndex].m_Index; });
					bool IsCheckpoint = IsTeleTileCheckpoint(m_Tiles[TgtIndex].m_Index);

					if(!IsCheckpoint && !IsTeleTileNumberUsed(m_TeleTiles[TgtIndex].m_Type, false))
					{
						// Tele tile number is unused. Set a known value which is not 0,
						// as tiles with number 0 would be ignored by previous versions.
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 255; });
					}
					else if(!IsCheckpoint && ((pLt->m_TeleTiles[SrcIndex].m_Number == 0 && Editor()->m_TeleNumber) || Editor()->m_TeleNumber != pLt->m_TeleNumber))
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_TeleNumber; });
					else if(IsCheckpoint && ((pLt->m_TeleTiles[SrcIndex].m_Number == 0 && Editor()->m_TeleCheckpointNumber) || Editor()->m_TeleCheckpointNumber != pLt->m_TeleCheckpointNumber))
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_TeleCheckpointNumber; });
					else
						m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pLt->m_TeleTiles[SrcIndex].m_Number; });
				}
				else
				{
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
					m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
					m_TeleTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				}
			}
		}
	}
	FlagModified(sx, sy, w, h);
}

int CLayerTele::FindNextFreeNumber(bool Checkpoint) const
{
	for(int i = 1; i <= 255; i++)
	{
		if(!ContainsElementWithId(i, Checkpoint))
		{
			return i;
		}
	}
	return -1;
}

bool CLayerTele::ContainsElementWithId(int Id, bool Checkpoint) const
{
	for(int y = 0; y < Height(); ++y)
	{
		for(int x = 0; x < Width(); ++x)
		{
			if(IsTeleTileNumberUsed(m_TeleTiles[y * Width() + x].m_Type, Checkpoint) && m_TeleTiles[y * Width() + x].m_Number == Id)
			{
				return true;
			}
		}
	}

	return false;
}

void CLayerTele::GetPos(int Number, int Offset, int &TeleX, int &TeleY)
{
	TeleX = -1;
	TeleY = -1;

	auto IsMatchingTile = [&](int i) {
		if(m_TeleTiles[i].m_Number == 0 || m_TeleTiles[i].m_Number != Number)
			return false;
		return IsValidTeleTile(m_TeleTiles[i].m_Type) && IsTeleTileNumberUsedAny(m_TeleTiles[i].m_Type);
	};

	const float MinClusterDistance = 10.0f;

	int Match = -1;
	ivec2 MatchPos = ivec2(-1, -1);

	auto FindTile = [&]() {
		for(int y = 0; y < Height(); y++)
		{
			for(int x = 0; x < Width(); x++)
			{
				if(!IsMatchingTile(y * Width() + x))
					continue;
				Match++;
				if(Offset != -1)
				{
					if(Match == Offset)
					{
						MatchPos = ivec2(x, y);
						m_GotoTeleOffset = Match;
						return;
					}
					continue;
				}
				if(Match <= m_GotoTeleOffset)
					continue;
				bool FarEnough = m_GotoTeleLastPos == ivec2(-1, -1) || distance(vec2(m_GotoTeleLastPos.x, m_GotoTeleLastPos.y), vec2(x, y)) >= MinClusterDistance;
				if(FarEnough)
				{
					MatchPos = ivec2(x, y);
					m_GotoTeleOffset = Match;
					return;
				}
			}
		}
	};
	FindTile();
	// Wrap around when no further distinct location found
	if(MatchPos == ivec2(-1, -1) && Offset == -1 && Match != -1)
	{
		m_GotoTeleOffset = -1;
		m_GotoTeleLastPos = ivec2(-1, -1);
		Match = -1;
		FindTile();
	}

	if(MatchPos != ivec2(-1, -1))
	{
		TeleX = MatchPos.x;
		TeleY = MatchPos.y;
		m_GotoTeleLastPos = MatchPos;
	}
	else
	{
		m_GotoTeleLastPos = ivec2(-1, -1);
		m_GotoTeleOffset = 0;
	}
}

std::shared_ptr<CLayer> CLayerTele::Duplicate() const
{
	return std::make_shared<CLayerTele>(*this);
}

const char *CLayerTele::TypeName() const
{
	return "tele";
}
