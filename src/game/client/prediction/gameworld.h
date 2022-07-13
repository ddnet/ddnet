/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_GAMEWORLD_H
#define GAME_CLIENT_PREDICTION_GAMEWORLD_H

#include <game/gamecore.h>
#include <game/teamscore.h>

#include <list>

class CCollision;
class CCharacter;
class CEntity;

class CGameWorld
{
	friend CCharacter;

public:
	enum
	{
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_PICKUP,
		ENTTYPE_FLAG,
		ENTTYPE_CHARACTER,
		NUM_ENTTYPES
	};

	CWorldCore m_Core;
	CTeamsCore m_Teams;

	CGameWorld();
	~CGameWorld();

	CEntity *FindFirst(int Type);
	CEntity *FindLast(int Type);
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);
	CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, CCharacter *pNotThis = nullptr, int CollideWith = -1, CCharacter *pThisOnly = nullptr);
	void InsertEntity(CEntity *pEntity, bool Last = false);
	void RemoveEntity(CEntity *pEntity);
	void RemoveCharacter(CCharacter *pChar);
	void Tick();

	// DDRace
	void ReleaseHooked(int ClientID);
	std::list<CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, CEntity *pNotThis = nullptr);

	int m_GameTick = 0;
	int m_GameTickSpeed = 0;
	CCollision *m_pCollision = nullptr;

	// getter for server variables
	int GameTick() { return m_GameTick; }
	int GameTickSpeed() { return m_GameTickSpeed; }
	CCollision *Collision() { return m_pCollision; }
	CTeamsCore *Teams() { return &m_Teams; }
	std::vector<SSwitchers> &Switchers() { return m_Core.m_vSwitchers; }
	CTuningParams *Tuning();
	CEntity *GetEntity(int ID, int EntityType);
	CCharacter *GetCharacterByID(int ID) { return (ID >= 0 && ID < MAX_CLIENTS) ? m_apCharacters[ID] : nullptr; }

	// from gamecontext
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask);

	// for client side prediction
	struct
	{
		bool m_IsDDRace = false;
		bool m_IsVanilla = false;
		bool m_IsFNG = false;
		bool m_InfiniteAmmo = false;
		bool m_PredictTiles = false;
		int m_PredictFreeze = 0;
		bool m_PredictWeapons = false;
		bool m_PredictDDRace = false;
		bool m_IsSolo = false;
		bool m_UseTuneZones = false;
		bool m_BugDDRaceInput = false;
	} m_WorldConfig;

	bool m_IsValidCopy = false;
	CGameWorld *m_pParent = nullptr;
	CGameWorld *m_pChild = nullptr;

	void OnModified();
	void NetObjBegin();
	void NetCharAdd(int ObjID, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, int GameTeam, bool IsLocal);
	void NetObjAdd(int ObjID, int ObjType, const void *pObjData, const CNetObj_EntityEx *pDataEx);
	void NetObjEnd(int LocalID);
	void CopyWorld(CGameWorld *pFrom);
	CEntity *FindMatch(int ObjID, int ObjType, const void *pObjData);
	void Clear();

	CTuningParams *m_pTuningList = nullptr;
	CTuningParams *TuningList() { return m_pTuningList; }
	CTuningParams *GetTuning(int i) { return &TuningList()[i]; }

private:
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity = nullptr;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES] = {nullptr};

	CCharacter *m_apCharacters[MAX_CLIENTS] = {nullptr};
};

class CCharOrder
{
public:
	std::list<int> m_IDs; // reverse of the order in the gameworld, since entities will be inserted in reverse
	CCharOrder()
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			m_IDs.push_back(i);
	}
	void GiveStrong(int c)
	{
		if(0 <= c && c < MAX_CLIENTS)
		{
			m_IDs.remove(c);
			m_IDs.push_front(c);
		}
	}
	void GiveWeak(int c)
	{
		if(0 <= c && c < MAX_CLIENTS)
		{
			m_IDs.remove(c);
			m_IDs.push_back(c);
		}
	}
	bool HasStrongAgainst(int From, int To)
	{
		for(int i : m_IDs)
		{
			if(i == To)
				return false;
			else if(i == From)
				return true;
		}
		return false;
	}
};

#endif
