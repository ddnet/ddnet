#include "layer_switch.h"

#include <game/editor/editor.h>

CLayerSwitch::CLayerSwitch(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Switch");
	m_Switch = 1;

	m_pSwitchTile = new CSwitchTile[w * h];
	mem_zero(m_pSwitchTile, (size_t)w * h * sizeof(CSwitchTile));
	m_GotoSwitchLastPos = ivec2(-1, -1);
	m_GotoSwitchOffset = 0;
}

CLayerSwitch::CLayerSwitch(const CLayerSwitch &Other) :
	CLayerTiles(Other)
{
	str_copy(m_aName, "Switch copy");
	m_Switch = 1;

	m_pSwitchTile = new CSwitchTile[m_Width * m_Height];
	mem_copy(m_pSwitchTile, Other.m_pSwitchTile, (size_t)m_Width * m_Height * sizeof(CSwitchTile));
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

bool CLayerSwitch::IsEmpty(const std::shared_ptr<CLayerTiles> &pLayer)
{
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
			if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidSwitchTile(pLayer->GetTile(x, y).m_Index))
				return false;

	return true;
}

void CLayerSwitch::BrushDraw(std::shared_ptr<CLayer> pBrush, vec2 WorldPos)
{
	if(m_Readonly)
		return;

	std::shared_ptr<CLayerSwitch> pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(pBrush);
	int sx = ConvertX(WorldPos.x);
	int sy = ConvertY(WorldPos.y);
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

			int Index = fy * m_Width + fx;
			SSwitchTileStateChange::SData Previous{
				m_pSwitchTile[Index].m_Number,
				m_pSwitchTile[Index].m_Type,
				m_pSwitchTile[Index].m_Flags,
				m_pSwitchTile[Index].m_Delay,
				m_pTiles[Index].m_Index};

			if((m_pEditor->m_AllowPlaceUnusedTiles || IsValidSwitchTile(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index)) && pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index != TILE_AIR)
			{
				if(m_pEditor->m_SwitchNum != pSwitchLayer->m_SwitchNumber || m_pEditor->m_SwitchDelay != pSwitchLayer->m_SwitchDelay)
				{
					m_pSwitchTile[Index].m_Number = m_pEditor->m_SwitchNum;
					m_pSwitchTile[Index].m_Delay = m_pEditor->m_SwitchDelay;
				}
				else if(pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Number)
				{
					m_pSwitchTile[Index].m_Number = pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Number;
					m_pSwitchTile[Index].m_Delay = pSwitchLayer->m_pSwitchTile[y * pSwitchLayer->m_Width + x].m_Delay;
				}
				else
				{
					m_pSwitchTile[Index].m_Number = m_pEditor->m_SwitchNum;
					m_pSwitchTile[Index].m_Delay = m_pEditor->m_SwitchDelay;
				}

				m_pSwitchTile[Index].m_Type = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index;
				m_pSwitchTile[Index].m_Flags = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Flags;
				m_pTiles[Index].m_Index = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index;
				m_pTiles[Index].m_Flags = pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Flags;

				if(!IsSwitchTileFlagsUsed(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index))
				{
					m_pSwitchTile[Index].m_Flags = 0;
				}
				if(!IsSwitchTileNumberUsed(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index))
				{
					m_pSwitchTile[Index].m_Number = 0;
				}
				if(!IsSwitchTileDelayUsed(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index))
				{
					m_pSwitchTile[Index].m_Delay = 0;
				}
			}
			else
			{
				m_pSwitchTile[Index].m_Number = 0;
				m_pSwitchTile[Index].m_Type = 0;
				m_pSwitchTile[Index].m_Flags = 0;
				m_pSwitchTile[Index].m_Delay = 0;
				m_pTiles[Index].m_Index = 0;

				if(pSwitchLayer->m_pTiles[y * pSwitchLayer->m_Width + x].m_Index != TILE_AIR)
					ShowPreventUnusedTilesWarning();
			}

			SSwitchTileStateChange::SData Current{
				m_pSwitchTile[Index].m_Number,
				m_pSwitchTile[Index].m_Type,
				m_pSwitchTile[Index].m_Flags,
				m_pSwitchTile[Index].m_Delay,
				m_pTiles[Index].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
		}
	FlagModified(sx, sy, pSwitchLayer->m_Width, pSwitchLayer->m_Height);
}

void CLayerSwitch::RecordStateChange(int x, int y, SSwitchTileStateChange::SData Previous, SSwitchTileStateChange::SData Current)
{
	if(!m_History[y][x].m_Changed)
		m_History[y][x] = SSwitchTileStateChange{true, Previous, Current};
	else
		m_History[y][x].m_Current = Current;
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
						pDst2->m_Flags ^= (TILEFLAG_YFLIP | TILEFLAG_XFLIP);
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

void CLayerSwitch::FillSelection(bool Empty, std::shared_ptr<CLayer> pBrush, CUIRect Rect)
{
	if(m_Readonly || (!Empty && pBrush->m_Type != LAYERTYPE_TILES))
		return;

	Snap(&Rect); // corrects Rect; no need of <=

	Snap(&Rect);

	int sx = ConvertX(Rect.x);
	int sy = ConvertY(Rect.y);
	int w = ConvertX(Rect.w);
	int h = ConvertY(Rect.h);

	std::shared_ptr<CLayerSwitch> pLt = std::static_pointer_cast<CLayerSwitch>(pBrush);

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

			SSwitchTileStateChange::SData Previous{
				m_pSwitchTile[TgtIndex].m_Number,
				m_pSwitchTile[TgtIndex].m_Type,
				m_pSwitchTile[TgtIndex].m_Flags,
				m_pSwitchTile[TgtIndex].m_Delay,
				m_pTiles[TgtIndex].m_Index};

			if(Empty || (!m_pEditor->m_AllowPlaceUnusedTiles && !IsValidSwitchTile((pLt->m_pTiles[SrcIndex]).m_Index)))
			{
				m_pTiles[TgtIndex].m_Index = 0;
				m_pSwitchTile[TgtIndex].m_Type = 0;
				m_pSwitchTile[TgtIndex].m_Number = 0;
				m_pSwitchTile[TgtIndex].m_Delay = 0;

				if(!Empty)
					ShowPreventUnusedTilesWarning();
			}
			else
			{
				m_pTiles[TgtIndex] = pLt->m_pTiles[SrcIndex];
				m_pSwitchTile[TgtIndex].m_Type = m_pTiles[TgtIndex].m_Index;
				if(pLt->m_Switch && m_pTiles[TgtIndex].m_Index > 0)
				{
					if(!IsSwitchTileNumberUsed(m_pSwitchTile[TgtIndex].m_Type))
						m_pSwitchTile[TgtIndex].m_Number = 0;
					else if(pLt->m_pSwitchTile[SrcIndex].m_Number == 0 || m_pEditor->m_SwitchNum != pLt->m_SwitchNumber)
						m_pSwitchTile[TgtIndex].m_Number = m_pEditor->m_SwitchNum;
					else
						m_pSwitchTile[TgtIndex].m_Number = pLt->m_pSwitchTile[SrcIndex].m_Number;

					if(!IsSwitchTileDelayUsed(m_pSwitchTile[TgtIndex].m_Type))
						m_pSwitchTile[TgtIndex].m_Delay = 0;
					else if(pLt->m_pSwitchTile[SrcIndex].m_Delay == 0 || m_pEditor->m_SwitchDelay != pLt->m_SwitchDelay)
						m_pSwitchTile[TgtIndex].m_Delay = m_pEditor->m_SwitchDelay;
					else
						m_pSwitchTile[TgtIndex].m_Delay = pLt->m_pSwitchTile[SrcIndex].m_Delay;

					if(!IsSwitchTileFlagsUsed(m_pSwitchTile[TgtIndex].m_Type))
						m_pSwitchTile[TgtIndex].m_Flags = 0;
					else
						m_pSwitchTile[TgtIndex].m_Flags = pLt->m_pSwitchTile[SrcIndex].m_Flags;
				}
			}

			SSwitchTileStateChange::SData Current{
				m_pSwitchTile[TgtIndex].m_Number,
				m_pSwitchTile[TgtIndex].m_Type,
				m_pSwitchTile[TgtIndex].m_Flags,
				m_pSwitchTile[TgtIndex].m_Delay,
				m_pTiles[TgtIndex].m_Index};

			RecordStateChange(fx, fy, Previous, Current);
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
			if(IsSwitchTileNumberUsed(m_pSwitchTile[y * m_Width + x].m_Type) && m_pSwitchTile[y * m_Width + x].m_Number == Id)
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
		for(int x = 0; x < m_Width; x++)
		{
			for(int y = 0; y < m_Height; y++)
			{
				int i = y * m_Width + x;
				int Switch = m_pSwitchTile[i].m_Number;
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
