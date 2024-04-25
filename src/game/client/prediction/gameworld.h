/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_GAMEWORLD_H
#define GAME_CLIENT_PREDICTION_GAMEWORLD_H

#include <game/gamecore.h>
#include <game/teamscore.h>

#include <list>
#include <vector>

class CCollision;
class CCharacter;
class CEntity;

class CGameWorld
{
public:
	enum
	{
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_DOOR,
		ENTTYPE_DRAGGER,
		ENTTYPE_LIGHT,
		ENTTYPE_GUN,
		ENTTYPE_PLASMA,
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
	CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, const CCharacter *pNotThis = nullptr, int CollideWith = -1, const CCharacter *pThisOnly = nullptr);
	void InsertEntity(CEntity *pEntity, bool Last = false);
	void RemoveEntity(CEntity *pEntity);
	void RemoveCharacter(CCharacter *pChar);
	void Tick();

	// DDRace
	void ReleaseHooked(int ClientId);
	std::vector<CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, const CEntity *pNotThis = nullptr);

	int m_GameTick;
	CCollision *m_pCollision;

	// getter for server variables
	int GameTick() { return m_GameTick; }
	int GameTickSpeed() { return SERVER_TICK_SPEED; }
	CCollision *Collision() { return m_pCollision; }
	CTeamsCore *Teams() { return &m_Teams; }
	std::vector<SSwitchers> &Switchers() { return m_Core.m_vSwitchers; }
	CTuningParams *Tuning();
	CEntity *GetEntity(int Id, int EntityType);
	CCharacter *GetCharacterById(int Id) { return (Id >= 0 && Id < MAX_CLIENTS) ? m_apCharacters[Id] : nullptr; }

	// from gamecontext
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, CClientMask Mask);

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
		bool m_UseTuneZones;
		bool m_BugDDRaceInput;
		bool m_NoWeakHookAndBounce;
	} m_WorldConfig;

	bool m_IsValidCopy;
	CGameWorld *m_pParent;
	CGameWorld *m_pChild;

	int m_LocalClientId;

	bool IsLocalTeam(int OwnerId) const;
	void OnModified() const;
	void NetObjBegin(CTeamsCore Teams, int LocalClientId);
	void NetCharAdd(int ObjId, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, int GameTeam, bool IsLocal);
	void NetObjAdd(int ObjId, int ObjType, const void *pObjData, const CNetObj_EntityEx *pDataEx);
	void NetObjEnd();
	void CopyWorld(CGameWorld *pFrom);
	CEntity *FindMatch(int ObjId, int ObjType, const void *pObjData);
	void Clear();

	CTuningParams *m_pTuningList;
	CTuningParams *TuningList() { return m_pTuningList; }
	CTuningParams *GetTuning(int i) { return &TuningList()[i]; }

private:
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity = nullptr;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	CCharacter *m_apCharacters[MAX_CLIENTS];
};

class CCharOrder
{
public:
	std::list<int> m_Ids; // reverse of the order in the gameworld, since entities will be inserted in reverse
	CCharOrder()
	{
		Reset();
	}
	void Reset()
	{
		m_Ids.clear();
		for(int i = 0; i < MAX_CLIENTS; i++)
			m_Ids.push_back(i);
	}
	void GiveStrong(int c)
	{
		if(0 <= c && c < MAX_CLIENTS)
		{
			m_Ids.remove(c);
			m_Ids.push_front(c);
		}
	}
	void GiveWeak(int c)
	{
		if(0 <= c && c < MAX_CLIENTS)
		{
			m_Ids.remove(c);
			m_Ids.push_back(c);
		}
	}
	bool HasStrongAgainst(int From, int To)
	{
		for(int i : m_Ids)
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
