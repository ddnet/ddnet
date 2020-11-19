/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_GAMEWORLD_H
#define GAME_CLIENT_PREDICTION_GAMEWORLD_H

#include <game/gamecore.h>

#include <list>

class CEntity;
class CCharacter;

class CGameWorld
{
	friend class CCharacter;

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
	class CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, class CCharacter *pNotThis = 0, int CollideWith = -1, class CCharacter *pThisOnly = 0);
	void InsertEntity(CEntity *pEntity, bool Last = false);
	void RemoveEntity(CEntity *pEntity);
	void DestroyEntity(CEntity *pEntity);
	void Tick();

	// DDRace

	std::list<class CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, class CEntity *pNotThis);
	void ReleaseHooked(int ClientID);
	std::list<class CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis = 0);

	int m_GameTick;
	int m_GameTickSpeed;
	CCollision *m_pCollision;

	// getter for server variables
	int GameTick() { return m_GameTick; }
	int GameTickSpeed() { return m_GameTickSpeed; }
	class CCollision *Collision() { return m_pCollision; }
	CTeamsCore *Teams() { return &m_Teams; }
	CTuningParams *Tuning();
	CEntity *GetEntity(int ID, int EntityType);
	class CCharacter *GetCharacterByID(int ID) { return (ID >= 0 && ID < MAX_CLIENTS) ? m_apCharacters[ID] : 0; }

	// from gamecontext
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64 Mask);

	// for client side prediction
	struct
	{
		bool m_IsDDRace;
		bool m_IsVanilla;
		bool m_IsFNG;
		bool m_InfiniteAmmo;
		bool m_PredictTiles;
		int m_PredictFreeze;
		bool m_PredictWeapons;
		bool m_PredictDDRace;
		bool m_IsSolo;
	} m_WorldConfig;

	bool m_IsValidCopy;
	CGameWorld *m_pParent;
	CGameWorld *m_pChild;

	void OnModified();
	void NetObjBegin();
	void NetCharAdd(int ObjID, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, int GameTeam, bool IsLocal);
	void NetObjAdd(int ObjID, int ObjType, const void *pObjData);
	void NetObjEnd(int LocalID);
	void CopyWorld(CGameWorld *pFrom);
	CEntity *FindMatch(int ObjID, int ObjType, const void *pObjData);
	void Clear();

	CTuningParams m_Tuning[2];
	CTuningParams *m_pTuningList;
	CTuningParams *TuningList() { return m_pTuningList; }
	CTuningParams *GetTuning(int i) { return i == 0 ? Tuning() : &TuningList()[i]; }

private:
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CCharacter *m_apCharacters[MAX_CLIENTS];
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
