// Made by qxdFox
#ifndef GAME_SERVER_VEHICLES_UFO_H
#define GAME_SERVER_VEHICLES_UFO_H

#include <game/server/entity.h>

class CCharacter;

class CVUfo
{
	CGameContext *GameServer() const;
	IServer *Server() const;
	CCharacter *m_pCharacter;

	enum
	{
		NUM_PARTS = 13
	};

	struct VUfoVisual
	{
		int m_aIds[NUM_PARTS];
		vec2 m_From[NUM_PARTS];
		vec2 m_To[NUM_PARTS];
	};
	VUfoVisual m_Visual;

	CNetObj_PlayerInput m_Input;

	vec2 m_pPrevPos;
	vec2 m_pVel;

	void SetUfoVisual();
	bool HandleInput();

	bool m_Active;
	bool m_AllowHookColl;
	bool m_Immovable;

	bool m_CanMove;

public:

	bool Active() const { return m_Active; }
	bool AllowHookColl() const { return m_AllowHookColl; }

	void OnPlayerDeath();

	void SetActive(bool Active);
	void Reset();
	void Tick();
	void OnSpawn(CCharacter *pChr);
	void Snap(int SnappingClient);
	void OnInput(CNetObj_PlayerInput pNewInput);
};

#endif // GAME_SERVER_VEHICLES_UFO_H
