/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#include "tas_state.h"

#include <game/server/entities/character.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <cstring>

CTasSnapshot::CTasSnapshot() :
	m_Tick(0),
	m_IsKeyframe(false),
	m_StateHash(0),
	m_WorldPaused(false)
{
}

void CTasSnapshot::Capture(CGameContext *pGameServer)
{
	m_Tick = pGameServer->Server()->Tick();
	m_WorldPaused = pGameServer->m_World.m_Paused;

	// Clear previous state
	m_vCharacters.clear();
	m_vProjectiles.clear();
	m_vLasers.clear();
	m_vSwitchers.clear();
	m_vTeamStates.clear();

	// Capture character states
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = pGameServer->GetPlayerChar(i);
		if(pChr && pChr->IsAlive())
		{
			SCharacterState State;
			CaptureCharacter(pChr, State);
			m_vCharacters.push_back(std::move(State));
		}
	}

	// Capture projectiles
	CEntity *pEnt = pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE);
	int ProjectileId = 0;
	while(pEnt)
	{
		CProjectile *pProj = static_cast<CProjectile *>(pEnt);
		SProjectileState State;
		State.m_Id = ProjectileId++;
		CaptureProjectile(pProj, State);
		m_vProjectiles.push_back(std::move(State));
		pEnt = pEnt->TypeNext();
	}

	// Capture lasers
	pEnt = pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_LASER);
	int LaserId = 0;
	while(pEnt)
	{
		CLaser *pLaser = static_cast<CLaser *>(pEnt);
		SLaserState State;
		State.m_Id = LaserId++;
		CaptureLaser(pLaser, State);
		m_vLasers.push_back(std::move(State));
		pEnt = pEnt->TypeNext();
	}

	// Capture switcher states
	auto &vSwitchers = pGameServer->Switchers();
	for(size_t i = 0; i < vSwitchers.size(); i++)
	{
		SSwitcherState State;
		State.m_Number = static_cast<int>(i);
		State.m_vStatus.resize(NUM_DDRACE_TEAMS);
		State.m_vEndTick.resize(NUM_DDRACE_TEAMS);
		State.m_vType.resize(NUM_DDRACE_TEAMS);
		State.m_vLastUpdateTick.resize(NUM_DDRACE_TEAMS);

		for(int j = 0; j < NUM_DDRACE_TEAMS; j++)
		{
			State.m_vStatus[j] = vSwitchers[i].m_aStatus[j];
			State.m_vEndTick[j] = vSwitchers[i].m_aEndTick[j];
			State.m_vType[j] = vSwitchers[i].m_aType[j];
			State.m_vLastUpdateTick[j] = vSwitchers[i].m_aLastUpdateTick[j];
		}
		m_vSwitchers.push_back(std::move(State));
	}

	// Capture team states
	CGameTeams *pTeams = &pGameServer->m_pController->Teams();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// Only capture teams that have players
		bool HasPlayers = false;
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			if(pTeams->m_Core.Team(j) == i && pGameServer->m_apPlayers[j])
			{
				HasPlayers = true;
				break;
			}
		}

		if(HasPlayers)
		{
			STeamState State;
			State.m_Team = i;
			State.m_TeamState = static_cast<int>(pTeams->GetTeamState(i));
			State.m_Practice = pTeams->IsPractice(i);
			State.m_Locked = pTeams->TeamLocked(i);
			m_vTeamStates.push_back(std::move(State));
		}
	}

	// Calculate hash for comparison
	m_StateHash = CalculateHash();
}

void CTasSnapshot::CaptureCharacter(CCharacter *pChr, SCharacterState &State)
{
	State.m_ClientId = pChr->GetPlayer()->GetCid();
	State.m_Alive = pChr->IsAlive();
	State.m_Pos = pChr->m_Pos;
	State.m_Vel = pChr->Core()->m_Vel;
	State.m_Super = pChr->IsSuper();
	State.m_Invincible = pChr->Core()->m_Invincible;

	// Use CSaveTee to capture the detailed state
	State.m_SaveTee.Save(pChr, false); // Don't add penalty for TAS saves
}

void CTasSnapshot::CaptureProjectile(CProjectile *pProj, SProjectileState &State)
{
	State.m_Type = pProj->GetOwnerId() >= 0 ? 0 : 0; // We need to access private members, will use reflection or friend
	State.m_Owner = pProj->GetOwnerId();
	State.m_Pos = pProj->m_Pos;

	// We need access to private members - for now store what we can via public interface
	// A more complete implementation would require making CTasSnapshot a friend of CProjectile
	// or adding getter methods to CProjectile

	// Store network object representation
	CNetObj_Projectile NetProj;
	pProj->FillInfo(&NetProj);
	State.m_Direction = vec2(NetProj.m_VelX / 100.0f, NetProj.m_VelY / 100.0f);
	State.m_StartTick = NetProj.m_StartTick;
	State.m_Type = NetProj.m_Type;
}

void CTasSnapshot::CaptureLaser(CLaser *pLaser, SLaserState &State)
{
	State.m_Owner = pLaser->GetOwnerId();
	State.m_Pos = pLaser->m_Pos;

	// Similar to projectile, we need access to private members
	// For now, store position which is public through CEntity
}

bool CTasSnapshot::Restore(CGameContext *pGameServer)
{
	// Destroy all dynamic entities first
	CEntity *pEnt = pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE);
	while(pEnt)
	{
		CEntity *pNext = pEnt->TypeNext();
		pEnt->Destroy();
		pEnt = pNext;
	}

	pEnt = pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_LASER);
	while(pEnt)
	{
		CEntity *pNext = pEnt->TypeNext();
		pEnt->Destroy();
		pEnt = pNext;
	}

	// Remove entities marked for destruction
	pGameServer->m_World.RemoveEntities();

	// Restore character states
	for(const auto &CharState : m_vCharacters)
	{
		if(!RestoreCharacter(pGameServer, CharState))
			return false;
	}

	// Restore projectiles
	for(const auto &ProjState : m_vProjectiles)
	{
		RestoreProjectile(pGameServer, ProjState);
	}

	// Restore lasers
	for(const auto &LaserState : m_vLasers)
	{
		RestoreLaser(pGameServer, LaserState);
	}

	// Restore switcher states
	auto &vSwitchers = pGameServer->Switchers();
	for(const auto &SwitcherState : m_vSwitchers)
	{
		if(SwitcherState.m_Number >= 0 && SwitcherState.m_Number < (int)vSwitchers.size())
		{
			for(int j = 0; j < NUM_DDRACE_TEAMS && j < (int)SwitcherState.m_vStatus.size(); j++)
			{
				vSwitchers[SwitcherState.m_Number].m_aStatus[j] = SwitcherState.m_vStatus[j];
				vSwitchers[SwitcherState.m_Number].m_aEndTick[j] = SwitcherState.m_vEndTick[j];
				vSwitchers[SwitcherState.m_Number].m_aType[j] = SwitcherState.m_vType[j];
				if(j < (int)SwitcherState.m_vLastUpdateTick.size())
					vSwitchers[SwitcherState.m_Number].m_aLastUpdateTick[j] = SwitcherState.m_vLastUpdateTick[j];
			}
		}
	}

	// Restore world pause state
	pGameServer->m_World.m_Paused = m_WorldPaused;

	return true;
}

bool CTasSnapshot::RestoreCharacter(CGameContext *pGameServer, const SCharacterState &State)
{
	CPlayer *pPlayer = pGameServer->m_apPlayers[State.m_ClientId];
	if(!pPlayer)
		return false;

	CCharacter *pChr = pPlayer->GetCharacter();

	// If character doesn't exist but should be alive, spawn it
	if(!pChr && State.m_Alive)
	{
		// Use spawn position from saved state
		pPlayer->m_ViewPos = State.m_Pos;
		pChr = pPlayer->ForceSpawn(State.m_Pos);
		if(!pChr)
			return false;
	}

	if(pChr && State.m_Alive)
	{
		// Load the saved state
		CSaveTee SaveTee = State.m_SaveTee;
		SaveTee.Load(pChr, std::nullopt);

		// Restore additional state
		pChr->SetSuper(State.m_Super);
		pChr->Core()->m_Invincible = State.m_Invincible;
	}
	else if(pChr && !State.m_Alive)
	{
		// Character should be dead
		pChr->Die(State.m_ClientId, WEAPON_GAME, false);
	}

	return true;
}

void CTasSnapshot::RestoreProjectile(CGameContext *pGameServer, const SProjectileState &State)
{
	// Create projectile with saved state
	// Note: This is a simplified recreation - full implementation would need more state
	new CProjectile(
		&pGameServer->m_World,
		State.m_Type,
		State.m_Owner,
		State.m_Pos,
		State.m_Direction,
		State.m_LifeSpan,
		State.m_Freeze,
		State.m_Explosive,
		State.m_SoundImpact,
		State.m_Direction // InitDir
	);
}

void CTasSnapshot::RestoreLaser(CGameContext *pGameServer, const SLaserState &State)
{
	// Create laser with saved state
	new CLaser(
		&pGameServer->m_World,
		State.m_From,
		State.m_Dir,
		State.m_Energy,
		State.m_Owner,
		State.m_Type);
}

uint32_t CTasSnapshot::CalculateHash() const
{
	// Simple hash combining character positions and velocities
	uint32_t Hash = 0;

	for(const auto &Chr : m_vCharacters)
	{
		Hash ^= static_cast<uint32_t>(Chr.m_Pos.x * 100) << 16;
		Hash ^= static_cast<uint32_t>(Chr.m_Pos.y * 100);
		Hash ^= static_cast<uint32_t>(Chr.m_Vel.x * 100) << 8;
		Hash ^= static_cast<uint32_t>(Chr.m_Vel.y * 100) << 24;
		Hash = (Hash << 5) | (Hash >> 27); // Rotate
	}

	Hash ^= static_cast<uint32_t>(m_vProjectiles.size()) << 16;
	Hash ^= static_cast<uint32_t>(m_vLasers.size());

	return Hash;
}

size_t CTasSnapshot::GetApproximateSize() const
{
	size_t Size = sizeof(CTasSnapshot);

	// Character states (rough estimate)
	Size += m_vCharacters.size() * (sizeof(SCharacterState) + 2048); // CSaveTee string is up to 2048

	// Projectiles
	Size += m_vProjectiles.size() * sizeof(SProjectileState);

	// Lasers
	Size += m_vLasers.size() * sizeof(SLaserState);

	// Switchers
	for(const auto &Switcher : m_vSwitchers)
	{
		Size += sizeof(SSwitcherState);
		Size += Switcher.m_vStatus.size() * sizeof(bool);
		Size += Switcher.m_vEndTick.size() * sizeof(int);
		Size += Switcher.m_vType.size() * sizeof(int);
	}

	// Team states
	Size += m_vTeamStates.size() * sizeof(STeamState);

	return Size;
}

bool CTasSnapshot::IsEquivalent(const CTasSnapshot &Other) const
{
	// Quick hash comparison first
	if(m_StateHash != Other.m_StateHash)
		return false;

	// Check character count
	if(m_vCharacters.size() != Other.m_vCharacters.size())
		return false;

	// Check each character position and velocity
	for(size_t i = 0; i < m_vCharacters.size(); i++)
	{
		const auto &A = m_vCharacters[i];
		const auto &B = Other.m_vCharacters[i];

		if(A.m_ClientId != B.m_ClientId)
			return false;
		if(A.m_Alive != B.m_Alive)
			return false;

		// Allow small floating point differences
		const float Epsilon = 0.001f;
		if(std::abs(A.m_Pos.x - B.m_Pos.x) > Epsilon ||
			std::abs(A.m_Pos.y - B.m_Pos.y) > Epsilon)
			return false;
		if(std::abs(A.m_Vel.x - B.m_Vel.x) > Epsilon ||
			std::abs(A.m_Vel.y - B.m_Vel.y) > Epsilon)
			return false;
	}

	// Check projectile and laser counts
	if(m_vProjectiles.size() != Other.m_vProjectiles.size())
		return false;
	if(m_vLasers.size() != Other.m_vLasers.size())
		return false;

	return true;
}

void CTasSnapshot::SaveToBuffer(std::vector<uint8_t> &vBuffer) const
{
	// Simple binary serialization
	// Format: [header][characters][projectiles][lasers][switchers][teams]

	auto WriteInt = [&vBuffer](int Value) {
		vBuffer.push_back((Value >> 0) & 0xFF);
		vBuffer.push_back((Value >> 8) & 0xFF);
		vBuffer.push_back((Value >> 16) & 0xFF);
		vBuffer.push_back((Value >> 24) & 0xFF);
	};

	auto WriteFloat = [&vBuffer](float Value) {
		uint32_t IntVal;
		std::memcpy(&IntVal, &Value, sizeof(float));
		vBuffer.push_back((IntVal >> 0) & 0xFF);
		vBuffer.push_back((IntVal >> 8) & 0xFF);
		vBuffer.push_back((IntVal >> 16) & 0xFF);
		vBuffer.push_back((IntVal >> 24) & 0xFF);
	};

	auto WriteBool = [&vBuffer](bool Value) {
		vBuffer.push_back(Value ? 1 : 0);
	};

	// Header
	WriteInt(m_Tick);
	WriteBool(m_IsKeyframe);
	WriteInt(m_StateHash);
	WriteBool(m_WorldPaused);

	// Character count and data
	WriteInt(static_cast<int>(m_vCharacters.size()));
	for(const auto &Chr : m_vCharacters)
	{
		WriteInt(Chr.m_ClientId);
		WriteBool(Chr.m_Alive);
		WriteFloat(Chr.m_Pos.x);
		WriteFloat(Chr.m_Pos.y);
		WriteFloat(Chr.m_Vel.x);
		WriteFloat(Chr.m_Vel.y);
		WriteBool(Chr.m_Super);
		WriteBool(Chr.m_Invincible);
		// CSaveTee would need its own binary serialization
	}

	// Projectile count and data
	WriteInt(static_cast<int>(m_vProjectiles.size()));
	for(const auto &Proj : m_vProjectiles)
	{
		WriteInt(Proj.m_Id);
		WriteInt(Proj.m_Type);
		WriteInt(Proj.m_Owner);
		WriteFloat(Proj.m_Pos.x);
		WriteFloat(Proj.m_Pos.y);
		WriteFloat(Proj.m_Direction.x);
		WriteFloat(Proj.m_Direction.y);
		WriteInt(Proj.m_LifeSpan);
		WriteInt(Proj.m_StartTick);
		WriteBool(Proj.m_Explosive);
		WriteBool(Proj.m_Freeze);
	}

	// Laser count and data
	WriteInt(static_cast<int>(m_vLasers.size()));
	for(const auto &Laser : m_vLasers)
	{
		WriteInt(Laser.m_Id);
		WriteInt(Laser.m_Owner);
		WriteFloat(Laser.m_From.x);
		WriteFloat(Laser.m_From.y);
		WriteFloat(Laser.m_Pos.x);
		WriteFloat(Laser.m_Pos.y);
		WriteFloat(Laser.m_Dir.x);
		WriteFloat(Laser.m_Dir.y);
		WriteInt(Laser.m_EvalTick);
		WriteFloat(Laser.m_Energy);
		WriteInt(Laser.m_Bounces);
		WriteInt(Laser.m_Type);
	}
}

bool CTasSnapshot::LoadFromBuffer(const uint8_t *pData, size_t Size)
{
	if(Size < 16) // Minimum header size
		return false;

	size_t Offset = 0;

	auto ReadInt = [pData, Size, &Offset]() -> int {
		if(Offset + 4 > Size)
			return 0;
		int Value = pData[Offset] |
			    (pData[Offset + 1] << 8) |
			    (pData[Offset + 2] << 16) |
			    (pData[Offset + 3] << 24);
		Offset += 4;
		return Value;
	};

	auto ReadFloat = [pData, Size, &Offset]() -> float {
		if(Offset + 4 > Size)
			return 0.0f;
		uint32_t IntVal = pData[Offset] |
				  (pData[Offset + 1] << 8) |
				  (pData[Offset + 2] << 16) |
				  (pData[Offset + 3] << 24);
		Offset += 4;
		float Value;
		std::memcpy(&Value, &IntVal, sizeof(float));
		return Value;
	};

	auto ReadBool = [pData, Size, &Offset]() -> bool {
		if(Offset >= Size)
			return false;
		return pData[Offset++] != 0;
	};

	// Header
	m_Tick = ReadInt();
	m_IsKeyframe = ReadBool();
	m_StateHash = ReadInt();
	m_WorldPaused = ReadBool();

	// Characters
	int CharCount = ReadInt();
	m_vCharacters.clear();
	m_vCharacters.reserve(CharCount);
	for(int i = 0; i < CharCount; i++)
	{
		SCharacterState State;
		State.m_ClientId = ReadInt();
		State.m_Alive = ReadBool();
		State.m_Pos.x = ReadFloat();
		State.m_Pos.y = ReadFloat();
		State.m_Vel.x = ReadFloat();
		State.m_Vel.y = ReadFloat();
		State.m_Super = ReadBool();
		State.m_Invincible = ReadBool();
		m_vCharacters.push_back(std::move(State));
	}

	// Projectiles
	int ProjCount = ReadInt();
	m_vProjectiles.clear();
	m_vProjectiles.reserve(ProjCount);
	for(int i = 0; i < ProjCount; i++)
	{
		SProjectileState State;
		State.m_Id = ReadInt();
		State.m_Type = ReadInt();
		State.m_Owner = ReadInt();
		State.m_Pos.x = ReadFloat();
		State.m_Pos.y = ReadFloat();
		State.m_Direction.x = ReadFloat();
		State.m_Direction.y = ReadFloat();
		State.m_LifeSpan = ReadInt();
		State.m_StartTick = ReadInt();
		State.m_Explosive = ReadBool();
		State.m_Freeze = ReadBool();
		m_vProjectiles.push_back(std::move(State));
	}

	// Lasers
	int LaserCount = ReadInt();
	m_vLasers.clear();
	m_vLasers.reserve(LaserCount);
	for(int i = 0; i < LaserCount; i++)
	{
		SLaserState State;
		State.m_Id = ReadInt();
		State.m_Owner = ReadInt();
		State.m_From.x = ReadFloat();
		State.m_From.y = ReadFloat();
		State.m_Pos.x = ReadFloat();
		State.m_Pos.y = ReadFloat();
		State.m_Dir.x = ReadFloat();
		State.m_Dir.y = ReadFloat();
		State.m_EvalTick = ReadInt();
		State.m_Energy = ReadFloat();
		State.m_Bounces = ReadInt();
		State.m_Type = ReadInt();
		m_vLasers.push_back(std::move(State));
	}

	return Offset <= Size;
}
