/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include <game/gamecore.h>

#include "save.h"

#include <vector>

class CEntity;
class CCharacter;

/*
	Class: Game World
		Tracks all entities in the game. Propagates tick and
		snap calls to all entities.
*/
class CGameWorld
{
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

	CEntity *m_pNextTraverseEntity = nullptr;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	CGameWorld();
	~CGameWorld();

	void SetGameServer(CGameContext *pGameServer);

	CEntity *FindFirst(int Type);

	/*
		Function: FindEntities
			Finds entities close to a position and returns them in a list.

		Arguments:
			Pos - Position.
			Radius - How close the entities have to be.
			ppEnts - Pointer to a list that should be filled with the pointers
				to the entities.
			Max - Number of entities that fits into the ents array.
			Type - Type of the entities to find.

		Returns:
			Number of entities found and added to the ents array.
	*/
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);

	/**
	 * Finds the CCharacter that intersects the line.
	 *
	 * @see IntersectEntity
	 *
	 * @param Pos0 Start position
	 * @param Pos1 End position
	 * @param Radius How far from the line the @link CCharacter @endlink is allowed to be
	 * @param NewPos Intersection position
	 * @param pNotThis Character to ignore intersecting with
	 * @param CollideWith Only find entitys that can collide with that Client Id (pass -1 to ignore this check)
	 * @param pThisOnly Only search this specific character and ignore all others
	 *
	 * @return Pointer to the closest hit or `nullptr` if there is no intersection.
	 */
	CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, const CCharacter *pNotThis = nullptr, int CollideWith = -1, const CCharacter *pThisOnly = nullptr);

	/**
	 * Finds the CEntity that intersects the line.
	 *
	 * @see IntersectCharacter
	 *
	 * @param Pos0 Start position
	 * @param Pos1 End position
	 * @param Radius How far from the line the @link CEntity @endlink is allowed to be
	 * @param Type Type of the entity to intersect
	 * @param NewPos Intersection position
	 * @param pNotThis Entity to ignore intersecting with
	 * @param CollideWith Only find entitys that can collide with that Client Id (pass -1 to ignore this check)
	 * @param pThisOnly Only search this specific entity and ignore all others
	 *
	 * @return Pointer to the closest hit or `nullptr` if there is no intersection.
	 */
	CEntity *IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, int Type, vec2 &NewPos, const CEntity *pNotThis = nullptr, int CollideWith = -1, const CEntity *pThisOnly = nullptr);

	/*
		Function: ClosestCharacter
			Finds the closest CCharacter to a specific point.

		Arguments:
			Pos - The center position.
			Radius - How far off the CCharacter is allowed to be
			pNotThis - Entity to ignore

		Returns:
			Returns a pointer to the closest CCharacter or NULL if no CCharacter is close enough.
	*/
	CCharacter *ClosestCharacter(vec2 Pos, float Radius, const CEntity *pNotThis);

	/*
		Function: InsertEntity
			Adds an entity to the world.

		Arguments:
			pEntity - Entity to add
	*/
	void InsertEntity(CEntity *pEntity);

	/*
		Function: RemoveEntity
			Removes an entity from the world.

		Arguments:
			pEntity - Entity to remove
	*/
	void RemoveEntity(CEntity *pEntity);

	void RemoveEntitiesFromPlayer(int PlayerId);
	void RemoveEntitiesFromPlayers(int PlayerIds[], int NumPlayers);

	/*
		Function: Snap
			Calls Snap on all the entities in the world to create
			the snapshot.

		Arguments:
			SnappingClient - ID of the client which snapshot
			is being created.
	*/
	void Snap(int SnappingClient);

	/*
		Function: PostSnap
			Called after all clients received their snapshot.
	*/
	void PostSnap();

	/*
		Function: Tick
			Calls Tick on all the entities in the world to progress
			the world to the next tick.
	*/
	void Tick();

	/*
		Function: SwapClients
			Calls SwapClients on all the entities in the world to ensure that /swap
			command is handled safely.
	*/
	void SwapClients(int Client1, int Client2);

	/*
		Function: BlocksSave
			Checks if any entity would block /save
	*/
	ESaveResult BlocksSave(int ClientId);

	// DDRace
	void ReleaseHooked(int ClientId);

	/*
		Function: IntersectedCharacters
			Finds all CCharacters that intersect the line.

		Arguments:
			Pos0 - Start position
			Pos1 - End position
			Radius - How for from the line the CCharacter is allowed to be.
			pNotThis - Entity to ignore intersecting with

		Returns:
			Returns list with all Characters on line.
	*/
	std::vector<CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, const CEntity *pNotThis = nullptr);

	CTuningParams *Tuning();

	CTuningParams *m_pTuningList;
	CTuningParams *TuningList() { return m_pTuningList; }
	CTuningParams *GetTuning(int i) { return &TuningList()[i]; }
};

#endif
