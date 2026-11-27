/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "gameworld.h"

#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "gamecontroller.h"

#include <engine/shared/config.h>

#include <game/collision.h>

#include <algorithm>
#include <utility>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////

CGameWorld::CEntityIterator::CEntityIterator(CGameWorld *pWorld, CEntity *pFirstEntity) :
	m_pWorld(pWorld),
	m_pNext(pFirstEntity)
{
	m_pWorld->m_vpEntityIterators.push_back(this);
}

CGameWorld::CEntityIterator::CEntityIterator(CGameWorld *pWorld, int EntityType) :
	m_pWorld(pWorld)
{
	dbg_assert(EntityType >= 0 && EntityType < NUM_ENTTYPES, "Invalid EntityType: %d", EntityType);
	m_pNext = m_pWorld->m_apFirstEntityTypes[EntityType];
	m_pWorld->m_vpEntityIterators.push_back(this);
}

CGameWorld::CEntityIterator::~CEntityIterator()
{
	dbg_assert(m_pWorld->m_vpEntityIterators.back() == this, "Inconsistent entity iterator removal");
	m_pWorld->m_vpEntityIterators.pop_back();
}

void CGameWorld::CEntityIterator::AllowEntityRemoval()
{
	m_AllowEntityRemoval = true;
}

CEntity *CGameWorld::CEntityIterator::Next()
{
	CEntity *pResult = m_pNext;
	if(m_pNext != nullptr)
	{
		m_pNext = m_pNext->m_pNextTypeEntity;
	}
	return pResult;
}

CGameWorld::CGameWorld()
{
	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;

	m_Paused = false;
	m_ResetRequested = false;
	std::fill(std::begin(m_apFirstEntityTypes), std::end(m_apFirstEntityTypes), nullptr);
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

void CGameWorld::Init(CCollision *pCollision, CTuningParams *pTuningList)
{
	m_Core.InitSwitchers(pCollision->m_HighestSwitchNumber);
	m_pTuningList = pTuningList;
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	int Num = 0;
	CEntityIterator Iterator(this, Type);
	while(CEntity *pEnt = Iterator.Next())
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

	for(CEntityIterator *pEntityIterator : m_vpEntityIterators)
	{
		dbg_assert(pEntityIterator->m_AllowEntityRemoval, "Removing entities not allowed with current iterator");
	}

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	for(CEntityIterator *pEntityIterator : m_vpEntityIterators)
	{
		if(pEntityIterator->m_pNext == pEnt)
		{
			pEntityIterator->m_pNext = pEnt->m_pNextTypeEntity;
		}
	}

	pEnt->m_pNextTypeEntity = nullptr;
	pEnt->m_pPrevTypeEntity = nullptr;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	{
		CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
		while(CEntity *pEnt = Iterator.Next())
		{
			pEnt->Snap(SnappingClient);
		}
	}

	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		if(i == ENTTYPE_CHARACTER)
			continue;

		CEntityIterator Iterator(this, m_apFirstEntityTypes[i]);
		while(CEntity *pEnt = Iterator.Next())
		{
			pEnt->Snap(SnappingClient);
		}
	}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(CEntity *pFirstEntity : m_apFirstEntityTypes)
	{
		CEntityIterator Iterator(this, pFirstEntity);
		Iterator.AllowEntityRemoval();
		while(CEntity *pEnt = Iterator.Next())
		{
			pEnt->Reset();
		}
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
	for(CEntity *pFirstEntity : m_apFirstEntityTypes)
	{
		CEntityIterator Iterator(this, pFirstEntity);
		Iterator.AllowEntityRemoval();
		while(CEntity *pEnt = Iterator.Next())
		{
			for(int i = 0; i < NumPlayers; i++)
			{
				if(pEnt->GetOwnerId() == PlayerIds[i])
				{
					RemoveEntity(pEnt);
					pEnt->Destroy();
					break;
				}
			}
		}
	}
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(CEntity *pFirstEntity : m_apFirstEntityTypes)
	{
		CEntityIterator Iterator(this, pFirstEntity);
		Iterator.AllowEntityRemoval();
		while(CEntity *pEnt = Iterator.Next())
		{
			if(pEnt->m_MarkedForDestroy)
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
		}
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
				CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
				Iterator.AllowEntityRemoval();
				while(CCharacter *pCharacter = static_cast<CCharacter *>(Iterator.Next()))
				{
					pCharacter->PreTick();
				}
			}

			CEntityIterator Iterator(this, m_apFirstEntityTypes[i]);
			Iterator.AllowEntityRemoval();
			while(CEntity *pEnt = Iterator.Next())
			{
				pEnt->Tick();
			}
		}

		for(CEntity *pFirstEntity : m_apFirstEntityTypes)
		{
			CEntityIterator Iterator(this, pFirstEntity);
			Iterator.AllowEntityRemoval();
			while(CEntity *pEnt = Iterator.Next())
			{
				pEnt->TickDeferred();
			}
		}
	}
	else
	{
		// update all objects
		for(CEntity *pFirstEntity : m_apFirstEntityTypes)
		{
			CEntityIterator Iterator(this, pFirstEntity);
			Iterator.AllowEntityRemoval();
			while(CEntity *pEnt = Iterator.Next())
			{
				pEnt->TickPaused();
			}
		}
	}

	RemoveEntities();

	// find the characters' strong/weak id
	int StrongWeakId = 0;
	CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
	while(CCharacter *pChar = static_cast<CCharacter *>(Iterator.Next()))
	{
		pChar->m_StrongWeakId = StrongWeakId;
		StrongWeakId++;
	}
}

ESaveResult CGameWorld::BlocksSave(int ClientId)
{
	// check all objects
	for(CEntity *pFirstEntity : m_apFirstEntityTypes)
	{
		CEntityIterator Iterator(this, pFirstEntity);
		while(CEntity *pEnt = Iterator.Next())
		{
			ESaveResult Result = pEnt->BlocksSave(ClientId);
			if(Result != ESaveResult::SUCCESS)
				return Result;
		}
	}
	return ESaveResult::SUCCESS;
}

void CGameWorld::SwapClients(int Client1, int Client2)
{
	// update all objects
	for(CEntity *pFirstEntity : m_apFirstEntityTypes)
	{
		CEntityIterator Iterator(this, pFirstEntity);
		while(CEntity *pEnt = Iterator.Next())
		{
			pEnt->SwapClients(Client1, Client2);
		}
	}
}

CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, const CCharacter *pNotThis, int CollideWith, const CCharacter *pThisOnly)
{
	return (CCharacter *)IntersectEntity(Pos0, Pos1, Radius, ENTTYPE_CHARACTER, NewPos, pNotThis, CollideWith, pThisOnly);
}

CEntity *CGameWorld::IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, int Type, vec2 &NewPos, const CEntity *pNotThis, int CollideWith, const CEntity *pThisOnly)
{
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CEntity *pClosest = nullptr;

	CEntityIterator Iterator(this, Type);
	while(CEntity *pEntity = Iterator.Next())
	{
		if(pEntity == pNotThis)
			continue;

		if(pThisOnly && pEntity != pThisOnly)
			continue;

		if(CollideWith != -1 && !pEntity->CanCollide(CollideWith))
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, pEntity->m_Pos, IntersectPos))
		{
			float Len = distance(pEntity->m_Pos, IntersectPos);
			if(Len < pEntity->m_ProximityRadius + Radius)
			{
				Len = distance(Pos0, IntersectPos);
				if(Len < ClosestLen)
				{
					NewPos = IntersectPos;
					ClosestLen = Len;
					pClosest = pEntity;
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

	CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
	while(CCharacter *p = static_cast<CCharacter *>(Iterator.Next()))
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
	CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
	while(CCharacter *pChr = static_cast<CCharacter *>(Iterator.Next()))
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
	CEntityIterator Iterator(this, m_apFirstEntityTypes[ENTTYPE_CHARACTER]);
	while(CCharacter *pChr = static_cast<CCharacter *>(Iterator.Next()))
	{
		if(pChr->Core()->HookedPlayer() == ClientId && !pChr->IsSuper())
		{
			pChr->ReleaseHook();
		}
	}
}
