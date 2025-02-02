/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "gameworld.h"
#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "gamecontroller.h"

#include <engine/shared/config.h>

#include <algorithm>
#include <utility>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;

	m_Paused = false;
	m_ResetRequested = false;
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		pFirstEntityType = nullptr;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		while(pFirstEntityType)
			delete pFirstEntityType; // NOLINT(clang-analyzer-cplusplus.NewDelete)
}

void CGameWorld::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? nullptr : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius + pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = nullptr;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = nullptr;
	pEnt->m_pPrevTypeEntity = nullptr;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	for(CEntity *pEnt = m_apFirstEntityTypes[ENTTYPE_CHARACTER]; pEnt;)
	{
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
		pEnt->Snap(SnappingClient);
		pEnt = m_pNextTraverseEntity;
	}

	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		if(i == ENTTYPE_CHARACTER)
			continue;

		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
	}
}

void CGameWorld::PostSnap()
{
	for(auto *pEnt : m_apFirstEntityTypes)
	{
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
	}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	GameServer()->m_pController->OnReset();
	RemoveEntities();

	m_ResetRequested = false;

	GameServer()->CreateAllEntities(false);
}

void CGameWorld::RemoveEntitiesFromPlayer(int PlayerId)
{
	RemoveEntitiesFromPlayers(&PlayerId, 1);
}

void CGameWorld::RemoveEntitiesFromPlayers(int PlayerIds[], int NumPlayers)
{
	for(auto *pEnt : m_apFirstEntityTypes)
	{
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			for(int i = 0; i < NumPlayers; i++)
			{
				if(pEnt->GetOwnerId() == PlayerIds[i])
				{
					RemoveEntity(pEnt);
					pEnt->Destroy();
					break;
				}
			}
			pEnt = m_pNextTraverseEntity;
		}
	}
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->m_MarkedForDestroy)
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(!m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
		{
			// It's important to call PreTick() and Tick() after each other.
			// If we call PreTick() before, and Tick() after other entities have been processed, it causes physics changes such as a stronger shotgun or grenade.
			if(g_Config.m_SvNoWeakHook && i == ENTTYPE_CHARACTER)
			{
				auto *pEnt = m_apFirstEntityTypes[i];
				for(; pEnt;)
				{
					m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
					((CCharacter *)pEnt)->PreTick();
					pEnt = m_pNextTraverseEntity;
				}
			}

			auto *pEnt = m_apFirstEntityTypes[i];
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}
		}

		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDeferred();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();

	// find the characters' strong/weak id
	int StrongWeakId = 0;
	for(CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
	{
		pChar->m_StrongWeakId = StrongWeakId;
		StrongWeakId++;
	}
}

ESaveResult CGameWorld::BlocksSave(int ClientId)
{
	// check all objects
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			ESaveResult Result = pEnt->BlocksSave(ClientId);
			if(Result != ESaveResult::SUCCESS)
				return Result;
			pEnt = m_pNextTraverseEntity;
		}
	return ESaveResult::SUCCESS;
}

void CGameWorld::SwapClients(int Client1, int Client2)
{
	// update all objects
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->SwapClients(Client1, Client2);
			pEnt = m_pNextTraverseEntity;
		}
}

// TODO: should be more general
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, const CCharacter *pNotThis, int CollideWith, const CCharacter *pThisOnly)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = nullptr;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		if(pThisOnly && p != pThisOnly)
			continue;

		if(CollideWith != -1 && !p->CanCollide(CollideWith))
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, p->m_Pos, IntersectPos))
		{
			float Len = distance(p->m_Pos, IntersectPos);
			if(Len < p->m_ProximityRadius + Radius)
			{
				Len = distance(Pos0, IntersectPos);
				if(Len < ClosestLen)
				{
					NewPos = IntersectPos;
					ClosestLen = Len;
					pClosest = p;
				}
			}
		}
	}

	return pClosest;
}

CCharacter *CGameWorld::ClosestCharacter(vec2 Pos, float Radius, const CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius * 2;
	CCharacter *pClosest = nullptr;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius + Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

std::vector<CCharacter *> CGameWorld::IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, const CEntity *pNotThis)
{
	std::vector<CCharacter *> vpCharacters;
	CCharacter *pChr = (CCharacter *)FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		if(pChr == pNotThis)
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, pChr->m_Pos, IntersectPos))
		{
			float Len = distance(pChr->m_Pos, IntersectPos);
			if(Len < pChr->m_ProximityRadius + Radius)
			{
				vpCharacters.push_back(pChr);
			}
		}
	}
	return vpCharacters;
}

void CGameWorld::ReleaseHooked(int ClientId)
{
	CCharacter *pChr = (CCharacter *)FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		if(pChr->Core()->HookedPlayer() == ClientId && !pChr->IsSuper())
		{
			pChr->ReleaseHook();
		}
	}
}

CTuningParams *CGameWorld::Tuning()
{
	return &m_Core.m_aTuning[0];
}
