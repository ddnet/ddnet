#include "layer_speedup.h"

#include <game/editor/editor.h>

CLayerSpeedup::CLayerSpeedup(CEditorMap *pMap, int w, int h, std::uint64_t RetainedId) :
	CLayerTiles(pMap, w, h, RetainedId)
{
	str_copy(m_aName, "Speedup");
	m_HasSpeedup = true;

	m_SpeedupTiles.Resize(w, h);
}

CLayerSpeedup::CLayerSpeedup(const CLayerSpeedup &Other) :
	CLayerTiles(Other),
	CLayerSpeedupValues(Other)
{
	str_copy(m_aName, "Speedup copy");
	m_HasSpeedup = true;
}

CLayerSpeedup::~CLayerSpeedup() = default;

void CLayerSpeedup::Resize(int NewW, int NewH)
{
	m_SpeedupTiles.Resize(NewW, NewH);

	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(Map()->m_pGameLayer->Width() != NewW || Map()->m_pGameLayer->Height() != NewH)
		Map()->m_pGameLayer->Resize(NewW, NewH);
}

void CLayerSpeedup::Shift(EShiftDirection Direction)
{
	CLayerTiles::Shift(Direction);
	ShiftImpl(m_SpeedupTiles, Direction, Map()->m_ShiftBy);
}

bool CLayerSpeedup::IsEmpty() const
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
			if(Editor()->IsAllowPlaceUnusedTiles() || IsValidSpeedupTile(Index))
			{
				return false;
			}
		}
	}
	return true;
}

void CLayerSpeedup::BrushDraw(CLayer *pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	CLayerSpeedup *pSpeedupLayer = static_cast<CLayerSpeedup *>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
	if(str_comp(pSpeedupLayer->m_aFilename, pSpeedupLayer->Map()->m_aFilename))
	{
		Editor()->m_SpeedupAngle = pSpeedupLayer->m_SpeedupAngle;
		Editor()->m_SpeedupForce = pSpeedupLayer->m_SpeedupForce;
		Editor()->m_SpeedupMaxSpeed = pSpeedupLayer->m_SpeedupMaxSpeed;
	}

	bool Destructive = Editor()->m_BrushDrawDestructive || pSpeedupLayer->IsEmpty();

	for(int y = 0; y < pSpeedupLayer->Height(); y++)
		for(int x = 0; x < pSpeedupLayer->Width(); x++)
		{
			int fx = x + sx;
			int fy = y + sy;

			if(fx < 0 || fx >= Width() || fy < 0 || fy >= Height())
				continue;

			if(!Destructive && GetTile(fx, fy).m_Index)
				continue;

			const int SrcIndex = y * pSpeedupLayer->Width() + x;
			const int TgtIndex = fy * Width() + fx;

			if((Editor()->IsAllowPlaceUnusedTiles() || IsValidSpeedupTile(pSpeedupLayer->m_Tiles[SrcIndex].m_Index)) && pSpeedupLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
			{
				if(Editor()->m_SpeedupAngle != pSpeedupLayer->m_SpeedupAngle || Editor()->m_SpeedupForce != pSpeedupLayer->m_SpeedupForce || Editor()->m_SpeedupMaxSpeed != pSpeedupLayer->m_SpeedupMaxSpeed)
				{
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = Editor()->m_SpeedupForce; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = Editor()->m_SpeedupMaxSpeed; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = Editor()->m_SpeedupAngle; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
				}
				else if(pSpeedupLayer->m_SpeedupTiles[SrcIndex].m_Force)
				{
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = pSpeedupLayer->m_SpeedupTiles[SrcIndex].m_Force; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = pSpeedupLayer->m_SpeedupTiles[SrcIndex].m_Angle; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = pSpeedupLayer->m_SpeedupTiles[SrcIndex].m_MaxSpeed; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
				}
				else if(Editor()->m_SpeedupForce)
				{
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = Editor()->m_SpeedupForce; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = Editor()->m_SpeedupMaxSpeed; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = Editor()->m_SpeedupAngle; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = pSpeedupLayer->m_Tiles[SrcIndex].m_Index; });
				}
				else
				{
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
				}
			}
			else
			{
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });

				if(pSpeedupLayer->m_Tiles[SrcIndex].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}
		}
	FlagModified(sx, sy, pSpeedupLayer->Width(), pSpeedupLayer->Height());
}

void CLayerSpeedup::BrushFlipX()
{
	CLayerTiles::BrushFlipX();
	FlipSpeedupTilesX(m_SpeedupTiles);
}

void CLayerSpeedup::BrushFlipY()
{
	CLayerTiles::BrushFlipY();
	FlipSpeedupTilesY(m_SpeedupTiles);
}

void CLayerSpeedup::BrushRotate(float Amount)
{
	int Rotation = (round_to_int(360.0f * Amount / (pi * 2)) / 90) % 4; // 0=0°, 1=90°, 2=180°, 3=270°
	if(Rotation < 0)
		Rotation += 4;

	if(Rotation == 1 || Rotation == 3)
	{
		RotateSpeedupTilesClockwise(m_SpeedupTiles);
		m_Tiles.RotateClockwise();
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

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	CLayerSpeedup *pLt = static_cast<CLayerSpeedup *>(pBrush);

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

			if(Empty || (!Editor()->IsAllowPlaceUnusedTiles() && !IsValidSpeedupTile((pLt->m_Tiles[SrcIndex]).m_Index))) // no speed up tile chosen: reset
			{
				m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = 0; });
				m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_Tiles.Set(TgtIndex, pLt->m_Tiles[SrcIndex]);
				if(pLt->m_HasSpeedup && m_Tiles[TgtIndex].m_Index > 0)
				{
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = m_Tiles[TgtIndex].m_Index; });

					if((pLt->m_SpeedupTiles[SrcIndex].m_Force == 0 && Editor()->m_SpeedupForce) || Editor()->m_SpeedupForce != pLt->m_SpeedupForce)
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = Editor()->m_SpeedupForce; });
					else
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = pLt->m_SpeedupTiles[SrcIndex].m_Force; });

					if((pLt->m_SpeedupTiles[SrcIndex].m_Angle == 0 && Editor()->m_SpeedupAngle) || Editor()->m_SpeedupAngle != pLt->m_SpeedupAngle)
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = Editor()->m_SpeedupAngle; });
					else
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = pLt->m_SpeedupTiles[SrcIndex].m_Angle; });

					if((pLt->m_SpeedupTiles[SrcIndex].m_MaxSpeed == 0 && Editor()->m_SpeedupMaxSpeed) || Editor()->m_SpeedupMaxSpeed != pLt->m_SpeedupMaxSpeed)
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = Editor()->m_SpeedupMaxSpeed; });
					else
						m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = pLt->m_SpeedupTiles[SrcIndex].m_MaxSpeed; });
				}
				else
				{
					m_Tiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Index = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Force = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Angle = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_MaxSpeed = 0; });
					m_SpeedupTiles.Update(TgtIndex, [&](auto &Cell) { Cell.m_Type = 0; });
				}
			}
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
