#ifndef GAME_SERVER_GAMEMODES_BLOCK_H
#define GAME_SERVER_GAMEMODES_BLOCK_H

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/server/gamemodes/DDRace.h>

constexpr int WEAPON_HOOK = NUM_WEAPONS;

// stores information about who interacted with who last
// this is used to know the killer
// when a player dies in the world
// we track more than the client id to support kills
// made after the killer left
class CLastToucher
{
public:
	// client id of the last toucher
	// touches from team mates reset it to -1
	int m_ClientId;

	// Use GetPlayerByUniqueId() if you think the m_ClientId
	// might be pointing to a new player after reconnect
	uint32_t m_UniqueClientId;

	// The team of the last toucher
	// TEAM_RED, TEAM_BLUE maybe also TEAM_SPECTATORS
	int m_Team;

	// the weapon that caused the touch
	int m_Weapon;

	// the server tick of when this interaction happend
	// can be used to determin the touch interaction age
	// only used in block mode for now
	int m_TouchTick;

	CLastToucher(int ClientId, uint32_t UniqueClientId, int Team, int Weapon, int ServerTick) :
		m_ClientId(ClientId), m_UniqueClientId(UniqueClientId), m_Team(Team), m_Weapon(Weapon), m_TouchTick(ServerTick)
	{
	}
};

class CGameControllerBlock : public CGameControllerDDRace
{
public:
	CGameControllerBlock(class CGameContext *pGameServer);
	~CGameControllerBlock() override;

	int SnapGameInfoExFlags(int SnappingClient) override;
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason) override;
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int &KillerId, int &Weapon) override;
	int SnapPlayerScore(int SnappingClient, class CPlayer *pPlayer) override;
	void Tick() override;

private:
	std::optional<CLastToucher> m_aLastToucher[MAX_CLIENTS];
	void SetLastToucher(int ToucherId, int TouchedId, int Weapon);
};
#endif // GAME_SERVER_GAMEMODES_BLOCK_H
