#include "layer_switch.h"

#include <game/editor/editor.h>

CLayerSwitch::CLayerSwitch(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId) :
	CLayerTiles(pMap, w, h, RetainedId)
{
	str_copy(m_aName, "Switch");
	m_HasSwitch = true;

	m_SwitchTiles.Resize(w, h);
	m_GotoSwitchLastPos = ivec2(-1, -1);
	m_GotoSwitchOffset = 0;
}

CLayerSwitch::CLayerSwitch(const CLayerSwitch &Other) :
	CLayerTiles(Other),
	CLayerSwitchValues(Other)
{
	str_copy(m_aName, "Switch copy");
	m_HasSwitch = true;
}

CLayerSwitch::~CLayerSwitch() = default;

void CLayerSwitch::Resize(int NewW, int NewH)
{
	m_SwitchTiles.Resize(NewW, NewH);

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(Map()->m_pGameLayer->Width() != NewW || Map()->m_pGameLayer->Height() != NewH)
		Map()->m_pGameLayer->Resize(NewW, NewH);
}

void CLayerSwitch::Shift(EShiftDirection Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_SwitchTiles, Direction, Map()->m_ShiftBy);
}

bool CLayerSwitch::IsEmpty() const
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
			if(Editor()->IsAllowPlaceUnusedTiles() || IsValidSwitchTile(Index))
			{
				return false;
			}
		}
	}
	return true;
}

void CLayerSwitch::BrushDraw(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerSwitch *pSwitchLayer = static_cast<CLayerSwitch *>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
	if(str_comp(pSwitchLayer->m_aFilename, pSwitchLayer->Map()->m_aFilename))
	{
		Editor()->m_SwitchNumber = pSwitchLayer->m_SwitchNumber;
		Editor()->m_SwitchDelay = pSwitchLayer->m_SwitchDelay;
	}

	bool Destructive = Editor()->m_BrushDrawDestructive || pSwitchLayer->IsEmpty();

	for(int y = 0; y < pSwitchLayer->Height(); y++)
		for(int x = 0; x < pSwitchLayer->Width(); x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = y * pSwitchLayer->Width() + x;
			const int TgtIndex = fy * Width() + fx;

			if((Editor()->IsAllowPlaceUnusedTiles() || IsValidSwitchTile(pSwitchLayer->m_Tiles[SrcIndex].m_Index)) && pSwitchLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
			{
				if(Editor()->m_SwitchNumber != pSwitchLayer->m_SwitchNumber || Editor()->m_SwitchDelay != pSwitchLayer->m_SwitchDelay)
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_SwitchNumber; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = Editor()->m_SwitchDelay; });
				}
				else if(pSwitchLayer->m_SwitchTiles[SrcIndex].m_Number)
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pSwitchLayer->m_SwitchTiles[SrcIndex].m_Number; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = pSwitchLayer->m_SwitchTiles[SrcIndex].m_Delay; });
				}
				else
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_SwitchNumber; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = Editor()->m_SwitchDelay; });
				}

				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pSwitchLayer->m_Tiles[SrcIndex].m_Index; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = pSwitchLayer->m_Tiles[SrcIndex].m_Flags; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pSwitchLayer->m_Tiles[SrcIndex].m_Index; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = pSwitchLayer->m_Tiles[SrcIndex].m_Flags; });

				if(!IsSwitchTileFlagsUsed(pSwitchLayer->m_Tiles[SrcIndex].m_Index))
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = 0; });
				}
				if(!IsSwitchTileNumberUsed(pSwitchLayer->m_Tiles[SrcIndex].m_Index))
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				}
				if(!IsSwitchTileDelayUsed(pSwitchLayer->m_Tiles[SrcIndex].m_Index))
				{
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = 0; });
				}
			}
			else
			{
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = 0; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

				if(pSwitchLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}
		}
	FlagModified(sx, sy, pSwitchLayer->Width(), pSwitchLayer->Height());
}

void CLayerSwitch::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	m_SwitchTiles.FlipX();
}

void CLayerSwitch::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	m_SwitchTiles.FlipY();
}

void CLayerSwitch::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		m_SwitchTiles.RotateClockwise();
		m_Tiles.RotateClockwise();
		for(std::size_t Index = 0; Index < m_Tiles.Size(); ++Index)
			m_Tiles.Update(Index, [&](auto &Tile) {
				if(IsRotatableTile(Tile.m_Index))
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

void CLayerSwitch::FillSelection(bool Empty, CLayer *pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerSwitch *pLt = static_cast<CLayerSwitch *>(pBrush);

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

			if(Empty || (!Editor()->IsAllowPlaceUnusedTiles() && !IsValidSwitchTile((pLt->m_Tiles[SrcIndex]).m_Index)))
			{
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = 0; });

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_Tiles.Set(TgtIndex, pLt->m_Tiles[SrcIndex]);
				m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = m_Tiles[TgtIndex].m_Index; });
				if(pLt->m_HasSwitch && m_Tiles[TgtIndex].m_Index > 0)
				{
					if(!IsSwitchTileNumberUsed(m_SwitchTiles[TgtIndex].m_Type))
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
					else if(pLt->m_SwitchTiles[SrcIndex].m_Number == 0 || Editor()->m_SwitchNumber != pLt->m_SwitchNumber)
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = Editor()->m_SwitchNumber; });
					else
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = pLt->m_SwitchTiles[SrcIndex].m_Number; });

					if(!IsSwitchTileDelayUsed(m_SwitchTiles[TgtIndex].m_Type))
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = 0; });
					else if(pLt->m_SwitchTiles[SrcIndex].m_Delay == 0 || Editor()->m_SwitchDelay != pLt->m_SwitchDelay)
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = Editor()->m_SwitchDelay; });
					else
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = pLt->m_SwitchTiles[SrcIndex].m_Delay; });

					if(!IsSwitchTileFlagsUsed(m_SwitchTiles[TgtIndex].m_Type))
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = 0; });
					else
						m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Flags = pLt->m_SwitchTiles[SrcIndex].m_Flags; });
				}
				else
				{
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Number = 0; });
					m_SwitchTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Delay = 0; });
				}
			}
		}
	}
	FlagModified(sx, sy, w, h);
}

int CLayerSwitch::FindNextFreeNumber() const
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

bool CLayerSwitch::ContainsElementWithId(int Id) const
{
	for(int y = 0; y < Height(); ++y)
	{
		for(int x = 0; x < Width(); ++x)
		{
			if(IsSwitchTileNumberUsed(m_SwitchTiles[y * Width() + x].m_Type) && m_SwitchTiles[y * Width() + x].m_Number == Id)
			{
				return true;
			}
		}
	}

	return false;
}

void CLayerSwitch::GetPos(int Number, int Offset, ivec2 &SwitchPos)
{
	int Match = -1;
	ivec2 MatchPos = ivec2(-1, -1);
	SwitchPos = ivec2(-1, -1);

	auto FindTile = [this, &Match, &MatchPos, &Number, &Offset]() {
		for(int x = 0; x < Width(); x++)
		{
			for(int y = 0; y < Height(); y++)
			{
				int i = y * Width() + x;
				int Switch = m_SwitchTiles[i].m_Number;
				if(Number == Switch)
				{
					Match++;
					if(Offset != -1)
					{
						if(Match == Offset)
						{
							MatchPos = ivec2(x, y);
							m_GotoSwitchOffset = Match;
							return;
						}
						continue;
					}
					MatchPos = ivec2(x, y);
					if(m_GotoSwitchLastPos != ivec2(-1, -1))
					{
						if(distance(m_GotoSwitchLastPos, MatchPos) < 10.0f)
						{
							m_GotoSwitchOffset++;
							continue;
						}
					}
					m_GotoSwitchLastPos = MatchPos;
					if(Match == m_GotoSwitchOffset)
						return;
				}
			}
		}
	};
	FindTile();

	if(MatchPos == ivec2(-1, -1))
		return;
	if(Match < m_GotoSwitchOffset)
		m_GotoSwitchOffset = -1;
	SwitchPos = MatchPos;
	m_GotoSwitchOffset++;
}

std::shared_ptr<CLayer> CLayerSwitch::Duplicate() const
{
	return std::make_shared<CLayerSwitch>(*this);
}

const char *CLayerSwitch::TypeName() const
{
	return "switch";
}
