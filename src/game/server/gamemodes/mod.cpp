#include "mod.h"

// Exchange this to a string that identifies your game mode.
// DM, TDM and CTF are reserved for teeworlds original modes.
// DDraceNetwork and TestDDraceNetwork are used by DDNet.
#define GAME_TYPE_NAME "Mod"
#define TEST_TYPE_NAME "TestMod"

#include <game/server/entities/character.h>
#include <game/server/player.h>

class CCharacterMod : public CCharacter
{
	MACRO_ALLOC_POOL_ID()
public:
	CCharacterMod(CGameControllerMod *pGameController, CNetObj_PlayerInput LastInput);

	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon) override;

protected:
	CGameControllerMod *m_pGameController = nullptr;
};

CCharacterMod::CCharacterMod(CGameControllerMod *pGameController, CNetObj_PlayerInput LastInput) :
	CCharacter(&pGameController->GameServer()->m_World, LastInput),
	m_pGameController(pGameController)

{
}

bool CCharacterMod::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	m_pGameController->OnCharacterTakeDamage(this, Dmg, From, Weapon);
	return CCharacter::TakeDamage(Force, Dmg, From, Weapon);
}

MACRO_ALLOC_POOL_ID_IMPL(CCharacterMod, MAX_CLIENTS)

class CPlayerMod : public CPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	CPlayerMod(CGameControllerMod *pGameController, uint32_t UniqueClientID, int ClientID, int Team);

protected:
	CCharacter *CreateCharacter() override;

	CGameControllerMod *m_pGameController = nullptr;
};

MACRO_ALLOC_POOL_ID_IMPL(CPlayerMod, MAX_CLIENTS)

CPlayerMod::CPlayerMod(CGameControllerMod *pGameController, uint32_t UniqueClientID, int ClientID, int Team) :
	CPlayer(pGameController->GameServer(), UniqueClientID, ClientID, Team),
	m_pGameController(pGameController)
{
}

CCharacter *CPlayerMod::CreateCharacter()
{
	return new(m_ClientID) CCharacterMod(m_pGameController, GameServer()->GetLastPlayerInput(m_ClientID));
}

CGameControllerMod::CGameControllerMod(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

CGameControllerMod::~CGameControllerMod() = default;

CPlayer *CGameControllerMod::CreatePlayer(int ClientID, int StartTeam)
{
	return new(ClientID) CPlayerMod(this, GameServer()->GetNextUniqueClientID(), ClientID, StartTeam);
}

void CGameControllerMod::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	IGameController::Tick();
}

void CGameControllerMod::OnCharacterTakeDamage(CCharacterMod *pCharacter, int Damage, int From, int Weapon)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "You've received %d damage points from player %d", Damage, From);
	GameServer()->SendChatTarget(pCharacter->GetPlayer()->GetCID(), aBuf);
}
