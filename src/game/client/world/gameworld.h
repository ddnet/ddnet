/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_WORLD_GAMEWORLD_H
#define GAME_CLIENT_WORLD_GAMEWORLD_H

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

private:
	void Reset();
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CCharacter *m_apCharacters[MAX_CLIENTS];

public:
	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	CGameWorld();
	~CGameWorld();

	CEntity *FindFirst(int Type);
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);
	class CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, class CCharacter *pNotThis = 0, int CollideWith = -1, class CCharacter *pThisOnly = 0);
	class CCharacter *ClosestCharacter(vec2 Pos, float Radius, CEntity *ppNotThis);
	void InsertEntity(CEntity *pEntity);
	void RemoveEntity(CEntity *pEntity);
	void DestroyEntity(CEntity *pEntity);
	void Snap(int SnappingClient);
	void Tick();

	// DDRace

	std::list<class CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, class CEntity *pNotThis);
	void ReleaseHooked(int ClientID);
	std::list<class CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis = 0);

	int m_GameTick;
	int m_GameTickSpeed;
	CCollision *m_pCollision;
	CTeamsCore *m_pTeams;

	bool m_IsDDRace;
	bool m_InfiniteAmmo;
	bool m_PredictDDRace;
	bool m_PredictWeapons;

	int GameTick() { return m_GameTick; }
	int GameTickSpeed() { return m_GameTickSpeed; }
	class CCollision *Collision() { return m_pCollision; }
	CTeamsCore *Teams() { return m_pTeams; }
	class CCharacter *GetCharacterByID(int ID) { return (ID >= 0 && ID < MAX_CLIENTS) ? m_apCharacters[ID] : 0; }
	CTuningParams *Tuning();
	CEntity *GetEntity(int ID, int EntityType);

	// gamecontext functions
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask);

	// helper functions for client side prediction
	void NetObjBegin();
	void NetObjAdd(int ObjID, int ObjType, const void *pObjData, bool IsLocal = false);
	void NetObjEnd();
	void CopyWorld(CGameWorld *pFrom);
};

#endif
