/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "gameworld.h"
#include "entity.h"
#include "entities/character.h"
#include "entities/projectile.h"
#include "entities/laser.h"
#include "entities/pickup.h"
#include <algorithm>
#include <utility>
#include <engine/shared/config.h>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_Paused = false;
	m_ResetRequested = false;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apCharacters[i] = 0;
	m_pCollision = 0;
	m_pTeams = 0;
	m_GameTick = 0;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius+pEnt->m_ProximityRadius)
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
	pEnt->m_pGameWorld = this;
	if(pEnt->m_ObjType == ENTTYPE_CHARACTER)
	{
		CCharacter *pChar = (CCharacter*) pEnt;
		int ID = pChar->GetCID();
		if(ID >= 0 && ID < MAX_CLIENTS)
		{
			m_apCharacters[ID] = pChar;
			m_Core.m_apCharacters[ID] = pChar->Core();
		}
		pChar->SetGameWorld(this);
	}

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->m_MarkedForDestroy = true;
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

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;

	if(pEnt->m_ObjType == ENTTYPE_CHARACTER)
	{
		CCharacter *pChar = (CCharacter*) pEnt;
		int ID = pChar->GetCID();
		if(ID >= 0 && ID < MAX_CLIENTS)
		{
			m_apCharacters[ID] = 0;
			m_Core.m_apCharacters[ID] = 0;
		}
	}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	//GameServer()->m_pController->PostReset();
	//RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
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

bool distCompare(std::pair<float,int> a, std::pair<float,int> b)
{
	return (a.first < b.first);
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(!m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();
}


// TODO: should be more general
//CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis)
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CCharacter *pNotThis, int CollideWith, class CCharacter *pThisOnly)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		if(CollideWith != -1 && !p->CanCollide(CollideWith))
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		float Len = distance(p->m_Pos, IntersectPos);
		if(Len < p->m_ProximityRadius+Radius)
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

	return pClosest;
}

CCharacter *CGameWorld::ClosestCharacter(vec2 Pos, float Radius, CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius*2;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius+Radius)
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
std::list<class CCharacter *> CGameWorld::IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis)
{
	std::list< CCharacter * > listOfChars;

	CCharacter *pChr = (CCharacter *)FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		if(pChr == pNotThis)
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, pChr->m_Pos);
		float Len = distance(pChr->m_Pos, IntersectPos);
		if(Len < pChr->m_ProximityRadius+Radius)
		{
			pChr->m_Intersection = IntersectPos;
			listOfChars.push_back(pChr);
		}
	}
	return listOfChars;
}

void CGameWorld::ReleaseHooked(int ClientID)
{
	CCharacter *pChr = (CCharacter *)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
		for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
		{
			CCharacterCore* Core = pChr->Core();
			if(Core->m_HookedPlayer == ClientID && !pChr->m_Super)
			{
				Core->m_HookedPlayer = -1;
				Core->m_HookState = HOOK_RETRACTED;
				Core->m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				Core->m_HookState = HOOK_RETRACTED;
			}
		}
}

CTuningParams *CGameWorld::Tuning()
{
	return &m_Core.m_Tuning[g_Config.m_ClDummy];
}

CEntity *CGameWorld::GetEntity(int ID, int EntType)
{
	for(CEntity *pEnt = m_apFirstEntityTypes[EntType]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
		if(pEnt->m_ID == ID)
			return pEnt;
	return 0;
}

void CGameWorld::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask)
{
	// deal damage
	CCharacter *apEnts[MAX_CLIENTS];
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int Num = FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->m_Pos - Pos;
		vec2 ForceDir(0,1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
		float Strength;
		if(Owner == -1 || !GetCharacterByID(Owner))
			Strength = Tuning()->m_ExplosionStrength;
		else
			Strength = GetCharacterByID(Owner)->Tuning()->m_ExplosionStrength;

		float Dmg = Strength * l;
		if((int)Dmg)
			if((GetCharacterByID(Owner) ? !(GetCharacterByID(Owner)->m_Hit&CCharacter::DISABLE_HIT_GRENADE) : g_Config.m_SvHit || NoDamage) || Owner == apEnts[i]->GetCID())
			{
				if(Owner != -1 && apEnts[i]->IsAlive() && !apEnts[i]->CanCollide(Owner)) continue;
				if(Owner == -1 && ActivatedTeam != -1 && apEnts[i]->IsAlive() && apEnts[i]->Team() != ActivatedTeam) continue;
				apEnts[i]->TakeDamage(ForceDir*Dmg*2, (int)Dmg, Owner, Weapon);
				if(GetCharacterByID(Owner) ? GetCharacterByID(Owner)->m_Hit&CCharacter::DISABLE_HIT_GRENADE : !g_Config.m_SvHit || NoDamage) break;
			}
	}
}

void CGameWorld::NetObjBegin()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = FindFirst(i); pEnt; pEnt = pEnt->TypeNext())
			pEnt->m_MarkedForDestroy = true;
}

void CGameWorld::NetObjAdd(int ObjID, int ObjType, const void *pObjData, bool IsLocal)
{
	if(ObjType == NETOBJTYPE_CHARACTER)
	{
		if(CCharacter *pChar = GetCharacterByID(ObjID))
		{
			pChar->Read((CNetObj_Character*) pObjData, IsLocal);
			pChar->m_MarkedForDestroy = false;
		}
		else
			new CCharacter(this, ObjID, (CNetObj_Character*) pObjData);
	}
	else if(ObjType == NETOBJTYPE_PROJECTILE && m_PredictWeapons)
	{
		CProjectile NetProj = CProjectile(this, ObjID, (CNetObj_Projectile*) pObjData);
		if(CEntity *pEnt = GetEntity(ObjID, ENTTYPE_PROJECTILE))
		{
			CProjectile *pProj = (CProjectile*) pEnt;
			if(distance(pProj->m_Pos, NetProj.m_Pos) < 2.f && distance(pProj->GetDirection(), NetProj.GetDirection()) < 2.f && pProj->GetStartTick() == NetProj.GetStartTick())
			{
				pEnt->m_MarkedForDestroy = false;
				if(pProj->m_Type == WEAPON_SHOTGUN && m_IsDDRace)
					pProj->m_LifeSpan = 20 * GameTickSpeed() - (GameTick() - pProj->m_StartTick);
				return;
			}
		}
		for(CProjectile *pProj = (CProjectile*) FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*) pProj->TypeNext())
		{
			if(pProj->m_ID == -1 && NetProj.GetOwner() < 0 && distance(pProj->m_Pos, NetProj.m_Pos) < 2.f && distance(pProj->GetDirection(), NetProj.GetDirection()) < 2.f && pProj->GetStartTick() == NetProj.GetStartTick())
			{
				pProj->m_ID = ObjID;
				pProj->m_MarkedForDestroy = false;
				return;
			}
		}
		if(NetProj.m_Explosive)
		{
			CEntity *pEnt = new CProjectile(NetProj);
			InsertEntity(pEnt);
		}
	}
	else if(ObjType == NETOBJTYPE_PICKUP && m_PredictWeapons)
	{
		CPickup NetPickup = CPickup(this, ObjID, (CNetObj_Pickup*) pObjData);
		if(CEntity *pEnt = GetEntity(ObjID, ENTTYPE_PICKUP))
		{
			pEnt->m_Pos = NetPickup.m_Pos;
			pEnt->m_MarkedForDestroy = false;
			return;
		}
		CEntity *pEnt = new CPickup(NetPickup);
		InsertEntity(pEnt);
	}
	else if(ObjType == NETOBJTYPE_LASER && m_PredictWeapons)
	{
		CLaser NetLaser = CLaser(this, ObjID, (CNetObj_Laser*) pObjData);
		if(CEntity *pEnt = GetEntity(ObjID, ENTTYPE_LASER))
		{
			CLaser *pLaser = (CLaser*) pEnt;
			const vec2 NetDiff = NetLaser.m_Pos - NetLaser.GetFrom();
			const vec2 EntDiff = pLaser->m_Pos - pLaser->GetFrom();
			const float DirError = distance(normalize(EntDiff)*length(NetDiff), NetDiff);
			if(distance(pLaser->GetFrom(), NetLaser.GetFrom()) < 2.f && pLaser->GetEvalTick() == NetLaser.GetEvalTick() && DirError < 2.f)
			{
				pEnt->m_MarkedForDestroy = false;
				if(distance(pLaser->m_Pos, NetLaser.m_Pos) > 2.f)
				{
					pLaser->m_Energy = 0.f;
					pLaser->m_Pos = NetLaser.m_Pos;
				}
			}
		}
		else
		{
			for(CLaser *pLaser = (CLaser*) FindFirst(CGameWorld::ENTTYPE_LASER); pLaser; pLaser = (CLaser*) pLaser->TypeNext())
			{
				if(distance(pLaser->m_Pos, NetLaser.m_Pos) < 2.f && distance(pLaser->GetFrom(), NetLaser.GetFrom()) < 2.f && pLaser->GetEvalTick() == NetLaser.GetEvalTick())
				{
					pLaser->m_ID = ObjID;
					pLaser->m_MarkedForDestroy = false;
					return;
				}
			}
		}
	}
}

void CGameWorld::NetObjEnd()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(CCharacter *pChar = GetCharacterByID(i))
		{
			if(CCharacter *pHookedChar = GetCharacterByID(pChar->m_Core.m_HookedPlayer))
			{
				if(pHookedChar->m_MarkedForDestroy)
				{
					pHookedChar->m_Pos = pChar->m_Core.m_HookPos;
					mem_zero(&pHookedChar->m_Input, sizeof(pHookedChar->m_Input));
					pHookedChar->m_MarkedForDestroy = false;
				}
			}
		}
	}
	RemoveEntities();
}

void CGameWorld::CopyWorld(CGameWorld *pFrom)
{
	if(pFrom == this)
		return;
	m_ResetRequested = pFrom->m_ResetRequested;
	m_Paused = pFrom->m_Paused;
	m_GameTick = pFrom->m_GameTick;
	m_GameTickSpeed = pFrom->m_GameTickSpeed;
	m_IsDDRace = pFrom->m_IsDDRace;
	m_PredictDDRace = pFrom->m_PredictDDRace;
	m_PredictWeapons = pFrom->m_PredictWeapons;
	m_InfiniteAmmo = pFrom->m_InfiniteAmmo;
	m_pCollision = pFrom->m_pCollision;
	for(int i = 0; i < 2; i++)
		m_Core.m_Tuning[i] = pFrom->m_Core.m_Tuning[i];
	m_pTeams = pFrom->m_pTeams;
	// delete the previous entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
	// copy and insert the new entities
	for(int Type = 0; Type < NUM_ENTTYPES; Type++)
	{
		CEntity *pLast = pFrom->FindFirst(Type);
		if(!pLast)
			continue;
		while(pLast->TypeNext())
			pLast = pLast->TypeNext();
		for(CEntity *pEnt = pLast; pEnt; pEnt = pEnt->TypePrev())
		{
			CEntity *pCopy = 0;
			if(Type == ENTTYPE_PROJECTILE)
				pCopy = new CProjectile(*((CProjectile*)pEnt));
			else if(Type == ENTTYPE_LASER)
				pCopy = new CLaser(*((CLaser*)pEnt));
			else if(Type == ENTTYPE_CHARACTER)
				pCopy = new CCharacter(*((CCharacter*)pEnt));
			else if(Type == ENTTYPE_PICKUP)
				pCopy = new CPickup(*((CPickup*)pEnt));
			if(pCopy)
				this->InsertEntity(pCopy);
		}
	}
}
