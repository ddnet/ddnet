/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "gameworld.h"
#include "entities/character.h"
#include "entities/laser.h"
#include "entities/pickup.h"
#include "entities/projectile.h"
#include "entity.h"
#include <algorithm>
#include <engine/shared/config.h>
#include <game/client/laser_data.h>
#include <game/client/projectile_data.h>
#include <game/mapitems.h>
#include <utility>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		pFirstEntityType = 0;
	for(auto &pCharacter : m_apCharacters)
		pCharacter = 0;
	m_pCollision = 0;
	m_GameTick = 0;
	m_pParent = 0;
	m_pChild = 0;
}

CGameWorld::~CGameWorld()
{
	Clear();
	if(m_pChild && m_pChild->m_pParent == this)
	{
		OnModified();
		m_pChild->m_pParent = 0;
	}
	if(m_pParent && m_pParent->m_pChild == this)
		m_pParent->m_pChild = 0;
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

CEntity *CGameWorld::FindLast(int Type)
{
	CEntity *pLast = FindFirst(Type);
	if(pLast)
		while(pLast->TypeNext())
			pLast = pLast->TypeNext();
	return pLast;
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

void CGameWorld::InsertEntity(CEntity *pEnt, bool Last)
{
	pEnt->m_pGameWorld = this;
	pEnt->m_pNextTypeEntity = 0x0;
	pEnt->m_pPrevTypeEntity = 0x0;

	// insert it
	if(!Last)
	{
		if(m_apFirstEntityTypes[pEnt->m_ObjType])
			m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
		pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
		pEnt->m_pPrevTypeEntity = 0x0;
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
	}
	else
	{
		// insert it at the end of the list
		CEntity *pLast = m_apFirstEntityTypes[pEnt->m_ObjType];
		if(pLast)
		{
			while(pLast->m_pNextTypeEntity)
				pLast = pLast->m_pNextTypeEntity;
			pLast->m_pNextTypeEntity = pEnt;
		}
		else
			m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
		pEnt->m_pPrevTypeEntity = pLast;
		pEnt->m_pNextTypeEntity = 0x0;
	}

	if(pEnt->m_ObjType == ENTTYPE_CHARACTER)
	{
		auto *pChar = (CCharacter *)pEnt;
		int ID = pChar->GetCID();
		if(ID >= 0 && ID < MAX_CLIENTS)
		{
			m_apCharacters[ID] = pChar;
			m_Core.m_apCharacters[ID] = pChar->Core();
		}
		pChar->SetCoreWorld(this);
	}
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

	if(pEnt->m_pParent)
	{
		if(m_IsValidCopy && m_pParent && m_pParent->m_pChild == this)
			pEnt->m_pParent->m_DestroyTick = GameTick();
		pEnt->m_pParent->m_pChild = nullptr;
		pEnt->m_pParent = nullptr;
	}
	if(pEnt->m_pChild)
	{
		pEnt->m_pChild->m_pParent = nullptr;
		pEnt->m_pChild = nullptr;
	}
}

void CGameWorld::RemoveCharacter(CCharacter *pChar)
{
	int ID = pChar->GetCID();
	if(ID >= 0 && ID < MAX_CLIENTS)
	{
		m_apCharacters[ID] = 0;
		m_Core.m_apCharacters[ID] = 0;
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
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

bool distCompare(std::pair<float, int> a, std::pair<float, int> b)
{
	return (a.first < b.first);
}

void CGameWorld::Tick()
{
	// update all objects
	if(m_WorldConfig.m_NoWeakHookAndBounce)
	{
		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->PreTick();
				pEnt = m_pNextTraverseEntity;
			}
	}

	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Tick();
			pEnt = m_pNextTraverseEntity;
		}

	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->TickDeferred();
			pEnt->m_SnapTicks++;
			pEnt = m_pNextTraverseEntity;
		}

	RemoveEntities();

	// update switch state
	for(auto &Switcher : Switchers())
	{
		for(int j = 0; j < MAX_CLIENTS; ++j)
		{
			if(Switcher.m_aEndTick[j] <= GameTick() && Switcher.m_aType[j] == TILE_SWITCHTIMEDOPEN)
			{
				Switcher.m_aStatus[j] = false;
				Switcher.m_aEndTick[j] = 0;
				Switcher.m_aType[j] = TILE_SWITCHCLOSE;
			}
			else if(Switcher.m_aEndTick[j] <= GameTick() && Switcher.m_aType[j] == TILE_SWITCHTIMEDCLOSE)
			{
				Switcher.m_aStatus[j] = true;
				Switcher.m_aEndTick[j] = 0;
				Switcher.m_aType[j] = TILE_SWITCHOPEN;
			}
		}
	}

	OnModified();
}

// TODO: should be more general
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, CCharacter *pNotThis, int CollideWith, class CCharacter *pThisOnly)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

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

std::list<class CCharacter *> CGameWorld::IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis)
{
	std::list<CCharacter *> listOfChars;

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
				listOfChars.push_back(pChr);
			}
		}
	}
	return listOfChars;
}

void CGameWorld::ReleaseHooked(int ClientID)
{
	CCharacter *pChr = (CCharacter *)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		CCharacterCore *pCore = pChr->Core();
		if(pCore->m_HookedPlayer == ClientID)
		{
			pCore->SetHookedPlayer(-1);
			pCore->m_HookState = HOOK_RETRACTED;
			pCore->m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		}
	}
}

CTuningParams *CGameWorld::Tuning()
{
	return &m_Core.m_aTuning[g_Config.m_ClDummy];
}

CEntity *CGameWorld::GetEntity(int ID, int EntityType)
{
	for(CEntity *pEnt = m_apFirstEntityTypes[EntityType]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
		if(pEnt->m_ID == ID)
			return pEnt;
	return 0;
}

void CGameWorld::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask)
{
	if(Owner < 0 && m_WorldConfig.m_IsSolo && !(Weapon == WEAPON_SHOTGUN && m_WorldConfig.m_IsDDRace))
		return;

	// deal damage
	CEntity *apEnts[MAX_CLIENTS];
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int Num = FindEntities(Pos, Radius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChar = (CCharacter *)apEnts[i];
		vec2 Diff = pChar->m_Pos - Pos;
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Strength;
		if(Owner == -1 || !GetCharacterByID(Owner))
			Strength = Tuning()->m_ExplosionStrength;
		else
			Strength = GetCharacterByID(Owner)->Tuning()->m_ExplosionStrength;

		float Dmg = Strength * l;
		if((int)Dmg)
			if((GetCharacterByID(Owner) ? !GetCharacterByID(Owner)->GrenadeHitDisabled() : g_Config.m_SvHit || NoDamage) || Owner == pChar->GetCID())
			{
				if(Owner != -1 && !pChar->CanCollide(Owner))
					continue;
				if(Owner == -1 && ActivatedTeam != -1 && pChar->Team() != ActivatedTeam)
					continue;
				pChar->TakeDamage(ForceDir * Dmg * 2, (int)Dmg, Owner, Weapon);
				if(GetCharacterByID(Owner) ? GetCharacterByID(Owner)->GrenadeHitDisabled() : !g_Config.m_SvHit || NoDamage)
					break;
			}
	}
}

void CGameWorld::NetObjBegin()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = FindFirst(i); pEnt; pEnt = pEnt->TypeNext())
		{
			pEnt->m_MarkedForDestroy = true;
			if(i == ENTTYPE_CHARACTER)
				((CCharacter *)pEnt)->m_KeepHooked = false;
		}
	OnModified();
}

void CGameWorld::NetCharAdd(int ObjID, CNetObj_Character *pCharObj, CNetObj_DDNetCharacter *pExtended, int GameTeam, bool IsLocal)
{
	CCharacter *pChar;
	if((pChar = (CCharacter *)GetEntity(ObjID, ENTTYPE_CHARACTER)))
	{
		pChar->Read(pCharObj, pExtended, IsLocal);
		pChar->Keep();
	}
	else
	{
		pChar = new CCharacter(this, ObjID, pCharObj, pExtended);
		InsertEntity(pChar);
	}

	if(pChar)
		pChar->m_GameTeam = GameTeam;
}

void CGameWorld::NetObjAdd(int ObjID, int ObjType, const void *pObjData, const CNetObj_EntityEx *pDataEx)
{
	if((ObjType == NETOBJTYPE_PROJECTILE || ObjType == NETOBJTYPE_DDNETPROJECTILE) && m_WorldConfig.m_PredictWeapons)
	{
		CProjectileData Data;
		if(ObjType == NETOBJTYPE_PROJECTILE)
		{
			Data = ExtractProjectileInfo((const CNetObj_Projectile *)pObjData, this);
		}
		else
		{
			Data = ExtractProjectileInfoDDNet((const CNetObj_DDNetProjectile *)pObjData, this);
		}
		CProjectile NetProj = CProjectile(this, ObjID, &Data, pDataEx);

		if(NetProj.m_Type != WEAPON_SHOTGUN && fabs(length(NetProj.m_Direction) - 1.f) > 0.02f) // workaround to skip grenades on ball mod
			return;

		if(CProjectile *pProj = (CProjectile *)GetEntity(ObjID, ENTTYPE_PROJECTILE))
		{
			if(NetProj.Match(pProj))
			{
				pProj->Keep();
				if(pProj->m_Type == WEAPON_SHOTGUN && m_WorldConfig.m_IsDDRace)
					pProj->m_LifeSpan = 20 * GameTickSpeed() - (GameTick() - pProj->m_StartTick);
				return;
			}
		}
		if(!Data.m_ExtraInfo)
		{
			// try to match the newly received (unrecognized) projectile with a locally fired one
			for(CProjectile *pProj = (CProjectile *)FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile *)pProj->TypeNext())
			{
				if(pProj->m_ID == -1 && NetProj.Match(pProj))
				{
					pProj->m_ID = ObjID;
					pProj->Keep();
					return;
				}
			}
			// otherwise try to determine its owner by checking if there is only one player nearby
			if(NetProj.m_StartTick >= GameTick() - 4)
			{
				const vec2 NetPos = NetProj.m_Pos - normalize(NetProj.m_Direction) * CCharacterCore::PhysicalSize() * 0.75;
				const bool Prev = (GameTick() - NetProj.m_StartTick) > 1;
				float First = 200.0f, Second = 200.0f;
				CCharacter *pClosest = 0;
				for(CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
				{
					float Dist = distance(Prev ? pChar->m_PrevPrevPos : pChar->m_PrevPos, NetPos);
					if(Dist < First)
					{
						pClosest = pChar;
						First = Dist;
					}
					else if(Dist < Second)
						Second = Dist;
				}
				if(pClosest && maximum(First, 2.f) * 1.2f < Second)
					NetProj.m_Owner = pClosest->m_ID;
			}
		}
		CProjectile *pProj = new CProjectile(NetProj);
		InsertEntity(pProj);
	}
	else if(ObjType == NETOBJTYPE_PICKUP && m_WorldConfig.m_PredictWeapons)
	{
		CPickup NetPickup = CPickup(this, ObjID, (CNetObj_Pickup *)pObjData, pDataEx);
		if(CPickup *pPickup = (CPickup *)GetEntity(ObjID, ENTTYPE_PICKUP))
		{
			if(NetPickup.Match(pPickup))
			{
				pPickup->m_Pos = NetPickup.m_Pos;
				pPickup->Keep();
				return;
			}
		}
		CEntity *pEnt = new CPickup(NetPickup);
		InsertEntity(pEnt, true);
	}
	else if((ObjType == NETOBJTYPE_LASER || ObjType == NETOBJTYPE_DDNETLASER) && m_WorldConfig.m_PredictWeapons)
	{
		CLaserData Data;
		if(ObjType == NETOBJTYPE_PROJECTILE)
		{
			Data = ExtractLaserInfo((const CNetObj_Laser *)pObjData, this);
		}
		else
		{
			Data = ExtractLaserInfoDDNet((const CNetObj_DDNetLaser *)pObjData, this);
		}
		CLaser NetLaser = CLaser(this, ObjID, &Data);
		CLaser *pMatching = 0;
		if(CLaser *pLaser = dynamic_cast<CLaser *>(GetEntity(ObjID, ENTTYPE_LASER)))
			if(NetLaser.Match(pLaser))
				pMatching = pLaser;
		if(!pMatching)
		{
			for(CEntity *pEnt = FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext())
			{
				auto *const pLaser = dynamic_cast<CLaser *>(pEnt);
				if(pLaser && pLaser->m_ID == -1 && NetLaser.Match(pLaser))
				{
					pMatching = pLaser;
					pMatching->m_ID = ObjID;
					break;
				}
			}
		}
		if(pMatching)
		{
			pMatching->Keep();
			if(distance(NetLaser.m_From, NetLaser.m_Pos) < distance(pMatching->m_From, pMatching->m_Pos) - 2.f)
			{
				// if the laser stopped earlier than predicted, set the energy to 0
				pMatching->m_Energy = 0.f;
				pMatching->m_Pos = NetLaser.m_Pos;
			}
		}
	}
}

void CGameWorld::NetObjEnd(int LocalID)
{
	// keep predicting hooked characters, based on hook position
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(CCharacter *pChar = GetCharacterByID(i))
			if(!pChar->m_MarkedForDestroy)
				if(CCharacter *pHookedChar = GetCharacterByID(pChar->m_Core.m_HookedPlayer))
					if(pHookedChar->m_MarkedForDestroy)
					{
						pHookedChar->m_Pos = pHookedChar->m_Core.m_Pos = pChar->m_Core.m_HookPos;
						pHookedChar->m_Core.m_Vel = vec2(0, 0);
						mem_zero(&pHookedChar->m_SavedInput, sizeof(pHookedChar->m_SavedInput));
						pHookedChar->m_SavedInput.m_TargetY = -1;
						pHookedChar->m_KeepHooked = true;
						pHookedChar->m_MarkedForDestroy = false;
					}
	RemoveEntities();

	// Update character IDs and pointers
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_apCharacters[i] = 0;
		m_Core.m_apCharacters[i] = 0;
	}
	for(CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
	{
		int ID = pChar->GetCID();
		if(ID >= 0 && ID < MAX_CLIENTS)
		{
			m_apCharacters[ID] = pChar;
			m_Core.m_apCharacters[ID] = pChar->Core();
		}
	}
}

void CGameWorld::CopyWorld(CGameWorld *pFrom)
{
	if(pFrom == this || !pFrom)
		return;
	m_IsValidCopy = false;
	m_pParent = pFrom;
	if(m_pParent && m_pParent->m_pChild && m_pParent->m_pChild != this)
		m_pParent->m_pChild->m_IsValidCopy = false;
	pFrom->m_pChild = this;

	m_GameTick = pFrom->m_GameTick;
	m_GameTickSpeed = pFrom->m_GameTickSpeed;
	m_pCollision = pFrom->m_pCollision;
	m_WorldConfig = pFrom->m_WorldConfig;
	for(int i = 0; i < 2; i++)
	{
		m_Core.m_aTuning[i] = pFrom->m_Core.m_aTuning[i];
	}
	m_pTuningList = pFrom->m_pTuningList;
	m_Teams = pFrom->m_Teams;
	m_Core.m_vSwitchers = pFrom->m_Core.m_vSwitchers;
	// delete the previous entities
	Clear();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_apCharacters[i] = 0;
		m_Core.m_apCharacters[i] = 0;
	}
	// copy and add the new entities
	for(int Type = 0; Type < NUM_ENTTYPES; Type++)
	{
		for(CEntity *pEnt = pFrom->FindLast(Type); pEnt; pEnt = pEnt->TypePrev())
		{
			CEntity *pCopy = 0;
			if(Type == ENTTYPE_PROJECTILE)
				pCopy = new CProjectile(*((CProjectile *)pEnt));
			else if(Type == ENTTYPE_LASER)
				pCopy = new CLaser(*((CLaser *)pEnt));
			else if(Type == ENTTYPE_CHARACTER)
				pCopy = new CCharacter(*((CCharacter *)pEnt));
			else if(Type == ENTTYPE_PICKUP)
				pCopy = new CPickup(*((CPickup *)pEnt));
			if(pCopy)
			{
				pCopy->m_pParent = pEnt;
				pEnt->m_pChild = pCopy;
				this->InsertEntity(pCopy);
			}
		}
	}
	m_IsValidCopy = true;
}

CEntity *CGameWorld::FindMatch(int ObjID, int ObjType, const void *pObjData)
{
#define FindType(EntType, EntClass, ObjClass) \
	{ \
		CEntity *pEnt = GetEntity(ObjID, EntType); \
		if(pEnt && EntClass(this, ObjID, (ObjClass *)pObjData).Match((EntClass *)pEnt)) \
			return pEnt; \
		return 0; \
	}
	switch(ObjType)
	{
	case NETOBJTYPE_CHARACTER: FindType(ENTTYPE_CHARACTER, CCharacter, CNetObj_Character);
	case NETOBJTYPE_PROJECTILE:
	case NETOBJTYPE_DDNETPROJECTILE:
	{
		CProjectileData Data;
		if(ObjType == NETOBJTYPE_PROJECTILE)
		{
			Data = ExtractProjectileInfo((const CNetObj_Projectile *)pObjData, this);
		}
		else
		{
			Data = ExtractProjectileInfoDDNet((const CNetObj_DDNetProjectile *)pObjData, this);
		}
		CProjectile *pEnt = (CProjectile *)GetEntity(ObjID, ENTTYPE_PROJECTILE);
		if(pEnt && CProjectile(this, ObjID, &Data).Match(pEnt))
		{
			return pEnt;
		}
		return 0;
	}
	case NETOBJTYPE_LASER:
	case NETOBJTYPE_DDNETLASER:
	{
		CLaserData Data;
		if(ObjType == NETOBJTYPE_LASER)
		{
			Data = ExtractLaserInfo((const CNetObj_Laser *)pObjData, this);
		}
		else
		{
			Data = ExtractLaserInfoDDNet((const CNetObj_DDNetLaser *)pObjData, this);
		}
		CLaser *pEnt = (CLaser *)GetEntity(ObjID, ENTTYPE_LASER);
		if(pEnt && CLaser(this, ObjID, &Data).Match(pEnt))
		{
			return pEnt;
		}
		return 0;
	}
	case NETOBJTYPE_PICKUP: FindType(ENTTYPE_PICKUP, CPickup, CNetObj_Pickup);
	}
	return 0;
}

void CGameWorld::OnModified()
{
	if(m_pChild)
		m_pChild->m_IsValidCopy = false;
}

void CGameWorld::Clear()
{
	// delete all entities
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		while(pFirstEntityType)
			delete pFirstEntityType;
}
