/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <antibot/antibot_data.h>

#include <cmath>
#include <engine/map.h>

#include <game/collision.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <engine/shared/config.h>

vec2 ClampVel(int MoveRestriction, vec2 Vel)
{
	if(Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT))
	{
		Vel.x = 0;
	}
	if(Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT))
	{
		Vel.x = 0;
	}
	if(Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN))
	{
		Vel.y = 0;
	}
	if(Vel.y < 0 && (MoveRestriction & CANTMOVE_UP))
	{
		Vel.y = 0;
	}
	return Vel;
}

CCollision::CCollision()
{
	m_pDoor = nullptr;
	Unload();
}

CCollision::~CCollision()
{
	Unload();
}

void CCollision::Init(class CLayers *pLayers)
{
	Unload();

	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if(m_pLayers->TeleLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TeleLayer()->m_Tele);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTeleTile))
			m_pTele = static_cast<CTeleTile *>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Tele));
	}

	if(m_pLayers->SpeedupLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SpeedupLayer()->m_Speedup);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSpeedupTile))
			m_pSpeedup = static_cast<CSpeedupTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupLayer()->m_Speedup));
	}

	if(m_pLayers->SwitchLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SwitchLayer()->m_Switch);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSwitchTile))
			m_pSwitch = static_cast<CSwitchTile *>(m_pLayers->Map()->GetData(m_pLayers->SwitchLayer()->m_Switch));

		m_pDoor = new CDoorTile[m_Width * m_Height];
		mem_zero(m_pDoor, (size_t)m_Width * m_Height * sizeof(CDoorTile));
	}

	if(m_pLayers->TuneLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TuneLayer()->m_Tune);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTuneTile))
			m_pTune = static_cast<CTuneTile *>(m_pLayers->Map()->GetData(m_pLayers->TuneLayer()->m_Tune));
	}

	if(m_pLayers->FrontLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->FrontLayer()->m_Front);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTile))
			m_pFront = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->FrontLayer()->m_Front));
	}

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		int Index;
		if(m_pSwitch)
		{
			if(m_pSwitch[i].m_Number > m_HighestSwitchNumber)
				m_HighestSwitchNumber = m_pSwitch[i].m_Number;

			if(m_pSwitch[i].m_Number)
				m_pDoor[i].m_Number = m_pSwitch[i].m_Number;
			else
				m_pDoor[i].m_Number = 0;

			Index = m_pSwitch[i].m_Type;

			if(Index <= TILE_NPH_ENABLE)
			{
				if((Index >= TILE_JUMP && Index <= TILE_SUBTRACT_TIME) || Index == TILE_ALLOW_TELE_GUN || Index == TILE_ALLOW_BLUE_TELE_GUN)
					m_pSwitch[i].m_Type = Index;
				else
					m_pSwitch[i].m_Type = 0;
			}
		}
	}

	if(m_pTele)
	{
		for(int i = 0; i < m_Width * m_Height; i++)
		{
			int Number = m_pTele[i].m_Number;
			int Type = m_pTele[i].m_Type;
			if(Number > 0)
			{
				if(Type == TILE_TELEIN)
				{
					m_TeleIns[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type == TILE_TELEOUT)
				{
					m_TeleOuts[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type == TILE_TELECHECKOUT)
				{
					m_TeleCheckOuts[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
				else if(Type)
				{
					m_TeleOthers[Number - 1].emplace_back(i % m_Width * 32.0f + 16.0f, i / m_Width * 32.0f + 16.0f);
				}
			}
		}
	}
}

void CCollision::Unload()
{
	m_pTiles = nullptr;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = nullptr;

	m_HighestSwitchNumber = 0;

	m_TeleIns.clear();
	m_TeleOuts.clear();
	m_TeleCheckOuts.clear();
	m_TeleOthers.clear();

	m_pTele = nullptr;
	m_pSpeedup = nullptr;
	m_pFront = nullptr;
	m_pSwitch = nullptr;
	m_pTune = nullptr;
	delete[] m_pDoor;
	m_pDoor = nullptr;
}

void CCollision::FillAntibot(CAntibotMapData *pMapData) const
{
	pMapData->m_Width = m_Width;
	pMapData->m_Height = m_Height;
	pMapData->m_pTiles = (unsigned char *)malloc((size_t)m_Width * m_Height);
	for(int i = 0; i < m_Width * m_Height; i++)
	{
		pMapData->m_pTiles[i] = 0;
		if(m_pTiles[i].m_Index >= TILE_SOLID && m_pTiles[i].m_Index <= TILE_NOLASER)
		{
			pMapData->m_pTiles[i] = m_pTiles[i].m_Index;
		}
	}
}

enum
{
	MR_DIR_HERE = 0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
	switch(Tile)
	{
	case TILE_STOP:
		switch(Flags)
		{
		case ROTATION_0: return CANTMOVE_DOWN;
		case ROTATION_90: return CANTMOVE_LEFT;
		case ROTATION_180: return CANTMOVE_UP;
		case ROTATION_270: return CANTMOVE_RIGHT;

		case TILEFLAG_YFLIP ^ ROTATION_0: return CANTMOVE_UP;
		case TILEFLAG_YFLIP ^ ROTATION_90: return CANTMOVE_RIGHT;
		case TILEFLAG_YFLIP ^ ROTATION_180: return CANTMOVE_DOWN;
		case TILEFLAG_YFLIP ^ ROTATION_270: return CANTMOVE_LEFT;
		}
		break;
	case TILE_STOPS:
		switch(Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
		case TILEFLAG_YFLIP ^ ROTATION_0:
		case TILEFLAG_YFLIP ^ ROTATION_180:
			return CANTMOVE_DOWN | CANTMOVE_UP;
		case ROTATION_90:
		case ROTATION_270:
		case TILEFLAG_YFLIP ^ ROTATION_90:
		case TILEFLAG_YFLIP ^ ROTATION_270:
			return CANTMOVE_LEFT | CANTMOVE_RIGHT;
		}
		break;
	case TILE_STOPA:
		return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT;
	case MR_DIR_DOWN: return CANTMOVE_DOWN;
	case MR_DIR_LEFT: return CANTMOVE_LEFT;
	case MR_DIR_UP: return CANTMOVE_UP;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int GetMoveRestrictions(int Direction, int Tile, int Flags)
{
	int Result = GetMoveRestrictionsRaw(Direction, Tile, Flags);
	// Generally, stoppers only have an effect if they block us from moving
	// *onto* them. The one exception is one-way blockers, they can also
	// block us from moving if we're on top of them.
	if(Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}
	return Result & GetMoveRestrictionsMask(Direction);
}

int CCollision::GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex) const
{
	static const vec2 DIRECTIONS[NUM_MR_DIRS] =
		{
			vec2(0, 0),
			vec2(1, 0),
			vec2(0, 1),
			vec2(-1, 0),
			vec2(0, -1)};
	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "invalid distance");
	int Restrictions = 0;
	for(int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DIRECTIONS[d] * Distance;
		int ModMapIndex = GetPureMapIndex(ModPos);
		if(d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
		{
			ModMapIndex = OverrideCenterTileIndex;
		}
		for(int Front = 0; Front < 2; Front++)
		{
			int Tile;
			int Flags;
			if(!Front)
			{
				Tile = GetTileIndex(ModMapIndex);
				Flags = GetTileFlags(ModMapIndex);
			}
			else
			{
				Tile = GetFrontTileIndex(ModMapIndex);
				Flags = GetFrontTileFlags(ModMapIndex);
			}
			Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);
		}
		if(pfnSwitchActive)
		{
			CDoorTile DoorTile;
			GetDoorTile(ModMapIndex, &DoorTile);
			if(pfnSwitchActive(DoorTile.m_Number, pUser))
			{
				Restrictions |= ::GetMoveRestrictions(d, DoorTile.m_Index, DoorTile.m_Flags);
			}
		}
	}
	return Restrictions;
}

int CCollision::GetTile(int x, int y) const
{
	if(!m_pTiles)
		return 0;

	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int pos = Ny * m_Width + Nx;

	if(m_pTiles[pos].m_Index >= TILE_SOLID && m_pTiles[pos].m_Index <= TILE_NOLASER)
		return m_pTiles[pos].m_Index;
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	int dx = 0, dy = 0; // Offset for checking the "through" tile
	ThroughOffset(Pos0, Pos1, &dx, &dy);
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if(pTeleNr)
		{
			if(g_Config.m_SvOldTeleportHook)
				*pTeleNr = IsTeleport(Index);
			else
				*pTeleNr = IsTeleportHook(Index);
		}
		if(pTeleNr && *pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINHOOK;
		}

		int hit = 0;
		if(CheckPoint(ix, iy))
		{
			if(!IsThrough(ix, iy, dx, dy, Pos0, Pos1))
				hit = GetCollisionAt(ix, iy);
		}
		else if(IsHookBlocker(ix, iy, Pos0, Pos1))
		{
			hit = TILE_NOHOOK;
		}
		if(hit)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return hit;
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if(pTeleNr)
		{
			if(g_Config.m_SvOldTeleportWeapons)
				*pTeleNr = IsTeleport(Index);
			else
				*pTeleNr = IsTeleportWeapon(Index);
		}
		if(pTeleNr && *pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINWEAPON;
		}

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size) const
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x - Size.x, Pos.y - Size.y))
		return true;
	if(CheckPoint(Pos.x + Size.x, Pos.y - Size.y))
		return true;
	if(CheckPoint(Pos.x - Size.x, Pos.y + Size.y))
		return true;
	if(CheckPoint(Pos.x + Size.x, Pos.y + Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, vec2 Elasticity, bool *pGrounded) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		float Fraction = 1.0f / (float)(Max + 1);
		float ElasticityX = clamp(Elasticity.x, -1.0f, 1.0f);
		float ElasticityY = clamp(Elasticity.y, -1.0f, 1.0f);

		for(int i = 0; i <= Max; i++)
		{
			// Early break as optimization to stop checking for collisions for
			// large distances after the obstacles we have already hit reduced
			// our speed to exactly 0.
			if(Vel == vec2(0, 0))
			{
				break;
			}

			vec2 NewPos = Pos + Vel * Fraction; // TODO: this row is not nice

			// Fraction can be very small and thus the calculation has no effect, no
			// reason to continue calculating.
			if(NewPos == Pos)
			{
				break;
			}

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					if(pGrounded && ElasticityY > 0 && Vel.y > 0)
						*pGrounded = true;
					NewPos.y = Pos.y;
					Vel.y *= -ElasticityY;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -ElasticityX;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					if(pGrounded && ElasticityY > 0 && Vel.y > 0)
						*pGrounded = true;
					NewPos.y = Pos.y;
					Vel.y *= -ElasticityY;
					NewPos.x = Pos.x;
					Vel.x *= -ElasticityX;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

// DDRace

int CCollision::IsSolid(int x, int y) const
{
	int index = GetTile(x, y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

bool CCollision::IsThrough(int x, int y, int OffsetX, int OffsetY, vec2 Pos0, vec2 Pos1) const
{
	int pos = GetPureMapIndex(x, y);
	if(m_pFront && (m_pFront[pos].m_Index == TILE_THROUGH_ALL || m_pFront[pos].m_Index == TILE_THROUGH_CUT))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && ((m_pFront[pos].m_Flags == ROTATION_0 && Pos0.y > Pos1.y) || (m_pFront[pos].m_Flags == ROTATION_90 && Pos0.x < Pos1.x) || (m_pFront[pos].m_Flags == ROTATION_180 && Pos0.y < Pos1.y) || (m_pFront[pos].m_Flags == ROTATION_270 && Pos0.x > Pos1.x)))
		return true;
	int offpos = GetPureMapIndex(x + OffsetX, y + OffsetY);
	return m_pTiles[offpos].m_Index == TILE_THROUGH || (m_pFront && m_pFront[offpos].m_Index == TILE_THROUGH);
}

bool CCollision::IsHookBlocker(int x, int y, vec2 Pos0, vec2 Pos1) const
{
	int pos = GetPureMapIndex(x, y);
	if(m_pTiles[pos].m_Index == TILE_THROUGH_ALL || (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_ALL))
		return true;
	if(m_pTiles[pos].m_Index == TILE_THROUGH_DIR && ((m_pTiles[pos].m_Flags == ROTATION_0 && Pos0.y < Pos1.y) ||
								(m_pTiles[pos].m_Flags == ROTATION_90 && Pos0.x > Pos1.x) ||
								(m_pTiles[pos].m_Flags == ROTATION_180 && Pos0.y > Pos1.y) ||
								(m_pTiles[pos].m_Flags == ROTATION_270 && Pos0.x < Pos1.x)))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && ((m_pFront[pos].m_Flags == ROTATION_0 && Pos0.y < Pos1.y) || (m_pFront[pos].m_Flags == ROTATION_90 && Pos0.x > Pos1.x) || (m_pFront[pos].m_Flags == ROTATION_180 && Pos0.y > Pos1.y) || (m_pFront[pos].m_Flags == ROTATION_270 && Pos0.x < Pos1.x)))
		return true;
	return false;
}

int CCollision::IsWallJump(int Index) const
{
	if(Index < 0)
		return 0;

	return m_pTiles[Index].m_Index == TILE_WALLJUMP;
}

int CCollision::IsNoLaser(int x, int y) const
{
	return (CCollision::GetTile(x, y) == TILE_NOLASER);
}

int CCollision::IsFrontNoLaser(int x, int y) const
{
	return (CCollision::GetFrontTile(x, y) == TILE_NOLASER);
}

int CCollision::IsTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsEvilTeleport(int Index) const
{
	if(Index < 0)
		return 0;
	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

bool CCollision::IsCheckTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return false;
	return m_pTele[Index].m_Type == TILE_TELECHECKIN;
}

bool CCollision::IsCheckEvilTeleport(int Index) const
{
	if(Index < 0 || !m_pTele)
		return false;
	return m_pTele[Index].m_Type == TILE_TELECHECKINEVIL;
}

int CCollision::IsTeleCheckpoint(int Index) const
{
	if(Index < 0)
		return 0;

	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELECHECK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportWeapon(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINWEAPON)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportHook(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINHOOK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsSpeedup(int Index) const
{
	if(Index < 0 || !m_pSpeedup)
		return 0;

	if(m_pSpeedup[Index].m_Force > 0)
		return Index;

	return 0;
}

int CCollision::IsTune(int Index) const
{
	if(Index < 0 || !m_pTune)
		return 0;

	if(m_pTune[Index].m_Type)
		return m_pTune[Index].m_Number;

	return 0;
}

void CCollision::GetSpeedup(int Index, vec2 *pDir, int *pForce, int *pMaxSpeed) const
{
	if(Index < 0 || !m_pSpeedup)
		return;
	float Angle = m_pSpeedup[Index].m_Angle * (pi / 180.0f);
	*pForce = m_pSpeedup[Index].m_Force;
	*pDir = direction(Angle);
	if(pMaxSpeed)
		*pMaxSpeed = m_pSpeedup[Index].m_MaxSpeed;
}

int CCollision::GetSwitchType(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Type;

	return 0;
}

int CCollision::GetSwitchNumber(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0 && m_pSwitch[Index].m_Number > 0)
		return m_pSwitch[Index].m_Number;

	return 0;
}

int CCollision::GetSwitchDelay(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Delay;

	return 0;
}

int CCollision::MoverSpeed(int x, int y, vec2 *pSpeed) const
{
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int Index = m_pTiles[Ny * m_Width + Nx].m_Index;

	if(Index != TILE_CP && Index != TILE_CP_F)
	{
		return 0;
	}

	vec2 Target;
	switch(m_pTiles[Ny * m_Width + Nx].m_Flags)
	{
	case ROTATION_0:
		Target.x = 0.0f;
		Target.y = -4.0f;
		break;
	case ROTATION_90:
		Target.x = 4.0f;
		Target.y = 0.0f;
		break;
	case ROTATION_180:
		Target.x = 0.0f;
		Target.y = 4.0f;
		break;
	case ROTATION_270:
		Target.x = -4.0f;
		Target.y = 0.0f;
		break;
	default:
		Target = vec2(0.0f, 0.0f);
		break;
	}
	if(Index == TILE_CP_F)
	{
		Target *= 4.0f;
	}
	*pSpeed = Target;
	return Index;
}

int CCollision::GetPureMapIndex(float x, float y) const
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return Ny * m_Width + Nx;
}

bool CCollision::TileExists(int Index) const
{
	if(Index < 0)
		return false;

	if((m_pTiles[Index].m_Index >= TILE_FREEZE && m_pTiles[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pTiles[Index].m_Index >= TILE_LFREEZE && m_pTiles[Index].m_Index <= TILE_LUNFREEZE))
		return true;
	if(m_pFront && ((m_pFront[Index].m_Index >= TILE_FREEZE && m_pFront[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pFront[Index].m_Index >= TILE_LFREEZE && m_pFront[Index].m_Index <= TILE_LUNFREEZE)))
		return true;
	if(m_pTele && (m_pTele[Index].m_Type == TILE_TELEIN || m_pTele[Index].m_Type == TILE_TELEINEVIL || m_pTele[Index].m_Type == TILE_TELECHECKINEVIL || m_pTele[Index].m_Type == TILE_TELECHECK || m_pTele[Index].m_Type == TILE_TELECHECKIN))
		return true;
	if(m_pSpeedup && m_pSpeedup[Index].m_Force > 0)
		return true;
	if(m_pDoor && m_pDoor[Index].m_Index)
		return true;
	if(m_pSwitch && m_pSwitch[Index].m_Type)
		return true;
	if(m_pTune && m_pTune[Index].m_Type)
		return true;
	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index) const
{
	if(Index < 0)
		return false;
	int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRight = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelow = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAbove = (Index - m_Width > 0) ? Index - m_Width : Index;

	if((m_pTiles[TileOnTheRight].m_Index == TILE_STOP && m_pTiles[TileOnTheRight].m_Flags == ROTATION_270) || (m_pTiles[TileOnTheLeft].m_Index == TILE_STOP && m_pTiles[TileOnTheLeft].m_Flags == ROTATION_90))
		return true;
	if((m_pTiles[TileBelow].m_Index == TILE_STOP && m_pTiles[TileBelow].m_Flags == ROTATION_0) || (m_pTiles[TileAbove].m_Index == TILE_STOP && m_pTiles[TileAbove].m_Flags == ROTATION_180))
		return true;
	if(m_pTiles[TileOnTheRight].m_Index == TILE_STOPA || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pTiles[TileOnTheRight].m_Index == TILE_STOPS || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPS)))
		return true;
	if(m_pTiles[TileBelow].m_Index == TILE_STOPA || m_pTiles[TileAbove].m_Index == TILE_STOPA || ((m_pTiles[TileBelow].m_Index == TILE_STOPS || m_pTiles[TileAbove].m_Index == TILE_STOPS) && m_pTiles[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
		return true;
	if(m_pFront)
	{
		if(m_pFront[TileOnTheRight].m_Index == TILE_STOPA || m_pFront[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pFront[TileOnTheRight].m_Index == TILE_STOPS || m_pFront[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pFront[TileBelow].m_Index == TILE_STOPA || m_pFront[TileAbove].m_Index == TILE_STOPA || ((m_pFront[TileBelow].m_Index == TILE_STOPS || m_pFront[TileAbove].m_Index == TILE_STOPS) && m_pFront[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pFront[TileOnTheRight].m_Index == TILE_STOP && m_pFront[TileOnTheRight].m_Flags == ROTATION_270) || (m_pFront[TileOnTheLeft].m_Index == TILE_STOP && m_pFront[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pFront[TileBelow].m_Index == TILE_STOP && m_pFront[TileBelow].m_Flags == ROTATION_0) || (m_pFront[TileAbove].m_Index == TILE_STOP && m_pFront[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	if(m_pDoor)
	{
		if(m_pDoor[TileOnTheRight].m_Index == TILE_STOPA || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pDoor[TileOnTheRight].m_Index == TILE_STOPS || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pDoor[TileBelow].m_Index == TILE_STOPA || m_pDoor[TileAbove].m_Index == TILE_STOPA || ((m_pDoor[TileBelow].m_Index == TILE_STOPS || m_pDoor[TileAbove].m_Index == TILE_STOPS) && m_pDoor[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pDoor[TileOnTheRight].m_Index == TILE_STOP && m_pDoor[TileOnTheRight].m_Flags == ROTATION_270) || (m_pDoor[TileOnTheLeft].m_Index == TILE_STOP && m_pDoor[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pDoor[TileBelow].m_Index == TILE_STOP && m_pDoor[TileBelow].m_Flags == ROTATION_0) || (m_pDoor[TileAbove].m_Index == TILE_STOP && m_pDoor[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos) const
{
	int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	if(TileExists(Index))
		return Index;
	else
		return -1;
}

std::vector<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices) const
{
	std::vector<int> vIndices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if(!d)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if(TileExists(Index))
		{
			vIndices.push_back(Index);
			return vIndices;
		}
		else
			return vIndices;
	}
	else
	{
		int LastIndex = 0;
		for(int i = 0; i < End; i++)
		{
			float a = i / d;
			vec2 Tmp = mix(PrevPos, Pos, a);
			int Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
			int Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
			int Index = Ny * m_Width + Nx;
			if(TileExists(Index) && LastIndex != Index)
			{
				if(MaxIndices && vIndices.size() > MaxIndices)
					return vIndices;
				vIndices.push_back(Index);
				LastIndex = Index;
			}
		}

		return vIndices;
	}
}

vec2 CCollision::GetPos(int Index) const
{
	if(Index < 0)
		return vec2(0, 0);

	int x = Index % m_Width;
	int y = Index / m_Width;
	return vec2(x * 32 + 16, y * 32 + 16);
}

int CCollision::GetTileIndex(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetFrontTileIndex(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Index;
}

int CCollision::GetTileFlags(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

int CCollision::GetFrontTileFlags(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Flags;
}

int CCollision::GetIndex(int Nx, int Ny) const
{
	return m_pTiles[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetIndex(vec2 PrevPos, vec2 Pos) const
{
	float Distance = distance(PrevPos, Pos);

	if(!Distance)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);

		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	for(int i = 0, id = std::ceil(Distance); i < id; i++)
	{
		float a = (float)i / Distance;
		vec2 Tmp = mix(PrevPos, Pos, a);
		int Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	return -1;
}

int CCollision::GetFrontIndex(int Nx, int Ny) const
{
	if(!m_pFront)
		return 0;
	return m_pFront[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetFrontTile(int x, int y) const
{
	if(!m_pFront)
		return 0;
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	if(m_pFront[Ny * m_Width + Nx].m_Index == TILE_DEATH || m_pFront[Ny * m_Width + Nx].m_Index == TILE_NOLASER)
		return m_pFront[Ny * m_Width + Nx].m_Index;
	else
		return 0;
}

int CCollision::Entity(int x, int y, int Layer) const
{
	if(x < 0 || x >= m_Width || y < 0 || y >= m_Height)
		return 0;

	const int Index = y * m_Width + x;
	switch(Layer)
	{
	case LAYER_GAME:
		return m_pTiles[Index].m_Index - ENTITY_OFFSET;
	case LAYER_FRONT:
		return m_pFront[Index].m_Index - ENTITY_OFFSET;
	case LAYER_SWITCH:
		return m_pSwitch[Index].m_Type - ENTITY_OFFSET;
	case LAYER_TELE:
		return m_pTele[Index].m_Type - ENTITY_OFFSET;
	case LAYER_SPEEDUP:
		return m_pSpeedup[Index].m_Type - ENTITY_OFFSET;
	case LAYER_TUNE:
		return m_pTune[Index].m_Type - ENTITY_OFFSET;
	default:
		dbg_assert(false, "Layer invalid");
		dbg_break();
	}
}

void CCollision::SetCollisionAt(float x, float y, int Index)
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pTiles[Ny * m_Width + Nx].m_Index = Index;
}

void CCollision::SetDoorCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if(!m_pDoor)
		return;
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pDoor[Ny * m_Width + Nx].m_Index = Type;
	m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
	m_pDoor[Ny * m_Width + Nx].m_Number = Number;
}

void CCollision::GetDoorTile(int Index, CDoorTile *pDoorTile) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
	{
		pDoorTile->m_Index = 0;
		pDoorTile->m_Flags = 0;
		pDoorTile->m_Number = 0;
		return;
	}
	*pDoorTile = m_pDoor[Index];
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY)
{
	float x = Pos0.x - Pos1.x;
	float y = Pos0.y - Pos1.y;
	if(absolute(x) > absolute(y))
	{
		if(x < 0)
		{
			*pOffsetX = -32;
			*pOffsetY = 0;
		}
		else
		{
			*pOffsetX = 32;
			*pOffsetY = 0;
		}
	}
	else
	{
		if(y < 0)
		{
			*pOffsetX = 0;
			*pOffsetY = -32;
		}
		else
		{
			*pOffsetX = 0;
			*pOffsetY = 32;
		}
	}
}

int CCollision::IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		if(GetIndex(Nx, Ny) == TILE_SOLID || GetIndex(Nx, Ny) == TILE_NOHOOK || GetIndex(Nx, Ny) == TILE_NOLASER || GetFrontIndex(Nx, Ny) == TILE_NOLASER)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(GetFrontIndex(Nx, Ny) == TILE_NOLASER)
				return GetFrontCollisionAt(Pos.x, Pos.y);
			else
				return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectNoLaserNoWalls(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = (float)i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)) || IsFrontNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetCollisionAt(Pos.x, Pos.y);
			else
				return GetFrontCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = (float)i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsSolid(round_to_int(Pos.x), round_to_int(Pos.y)) || (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y))))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return -1;
			else if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetTile(round_to_int(Pos.x), round_to_int(Pos.y));
			else
				return GetFrontTile(round_to_int(Pos.x), round_to_int(Pos.y));
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IsTimeCheckpoint(int Index) const
{
	if(Index < 0)
		return -1;

	int z = m_pTiles[Index].m_Index;
	if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
		return z - TILE_TIME_CHECKPOINT_FIRST;
	return -1;
}

int CCollision::IsFrontTimeCheckpoint(int Index) const
{
	if(Index < 0 || !m_pFront)
		return -1;

	int z = m_pFront[Index].m_Index;
	if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
		return z - TILE_TIME_CHECKPOINT_FIRST;
	return -1;
}

vec2 CCollision::TeleAllGet(int Number, size_t Offset)
{
	if(m_TeleIns.count(Number) > 0)
	{
		if(m_TeleIns[Number].size() > Offset)
			return m_TeleIns[Number][Offset];
		else
			Offset -= m_TeleIns[Number].size();
	}
	if(m_TeleOuts.count(Number) > 0)
	{
		if(m_TeleOuts[Number].size() > Offset)
			return m_TeleOuts[Number][Offset];
		else
			Offset -= m_TeleOuts[Number].size();
	}
	if(m_TeleCheckOuts.count(Number) > 0)
	{
		if(m_TeleCheckOuts[Number].size() > Offset)
			return m_TeleCheckOuts[Number][Offset];
		else
			Offset -= m_TeleCheckOuts[Number].size();
	}
	if(m_TeleOthers.count(Number) > 0)
	{
		if(m_TeleOthers[Number].size() > Offset)
			return m_TeleOthers[Number][Offset];
	}
	return vec2(-1, -1);
}

size_t CCollision::TeleAllSize(int Number)
{
	size_t Total = 0;
	if(m_TeleIns.count(Number) > 0)
		Total += m_TeleIns[Number].size();
	if(m_TeleOuts.count(Number) > 0)
		Total += m_TeleOuts[Number].size();
	if(m_TeleCheckOuts.count(Number) > 0)
		Total += m_TeleCheckOuts[Number].size();
	if(m_TeleOthers.count(Number) > 0)
		Total += m_TeleOthers[Number].size();
	return Total;
}
