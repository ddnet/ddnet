#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H
#define GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H

#include "../DDRace.h"

class CGameControllerPvp : public CGameControllerDDRace
{
public:
	CGameControllerPvp(class CGameContext *pGameServer);
	~CGameControllerPvp() override;

	// convience accessors to copy code from gamecontext to the instagib controller
	class IServer *Server() const { return GameServer()->Server(); }
	class CConfig *Config() { return GameServer()->Config(); }
	class IConsole *Console() { return GameServer()->Console(); }
	class IStorage *Storage() { return GameServer()->Storage(); }
	void SendChatTarget(int To, const char *pText, int Flags = CGameContext::FLAG_SIX | CGameContext::FLAG_SIXUP) const;
	void SendChat(int ClientId, int Team, const char *pText, int SpamProtectionClientId = -1, int Flags = CGameContext::FLAG_SIX | CGameContext::FLAG_SIXUP);

	void OnPlayerConstruct(class CPlayer *pPlayer);
	void OnCharacterConstruct(class CCharacter *pChr);

	void SendChatSpectators(const char *pMessage, int Flags);
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;
	int GetAutoTeam(int NotThisId) override;
	int GameInfoExFlags(int SnappingClient) override;
	int GameInfoExFlags2(int SnappingClient) override;

	void ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove);

	void BangCommandVote(int ClientId, const char *pCommand, const char *pDesc);
	void ComCallShuffleVote(int ClientId);
	void ComCallSwapTeamsVote(int ClientId);
	void ComCallSwapTeamsRandomVote(int ClientId);

	bool AllowPublicChat(const CPlayer *pPlayer);
	bool ParseChatCmd(char Prefix, int ClientId, const char *pCmdWithArgs);
	bool OnBangCommand(int ClientId, const char *pCmd, int NumArgs, const char **ppArgs);
	void AddSpree(CPlayer *pPlayer);
	void EndSpree(CPlayer *pPlayer, CPlayer *pKiller);
	void CheckForceUnpauseGame();

	/* UpdateSpawnWeapons
	 *
	 * called when sv_spawn_weapons is updated
	 */
	void UpdateSpawnWeapons();
	enum ESpawnWeapons
	{
		SPAWN_WEAPON_LASER,
		SPAWN_WEAPON_GRENADE,
		NUM_SPAWN_WEAPONS
	};
	ESpawnWeapons m_SpawnWeapons;
	ESpawnWeapons GetSpawnWeapons(int ClientId) const { return m_SpawnWeapons; }
	void SetSpawnWeapons(class CCharacter *pChr) const;

	// ddnet-insta only
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
	bool OnChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer) override;
	bool OnFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos) override;

	// Anticamper
	void Anticamper();

	// generic helpers

	// returns the amount of tee's that are not spectators
	int NumActivePlayers();

	// returns the amount of players that currently have a tee in the world
	int NumAlivePlayers();

	// different than NumAlivePlayers()
	// it does check m_IsDead which is set in OnCharacterDeath
	// instead of checking the character which only gets destroyed
	// after OnCharacterDeath
	//
	// needed for the wincheck in zcatch to get triggered on kill
	int NumNonDeadActivePlayers();

	// returns the client id of the player with the highest
	// killing spree (active spree not high score)
	// returns -1 if nobody made a kill since spawning
	int GetHighestSpreeClientId();

	// get the lowest client id that has a tee in the world
	// returns -1 if no player is alive
	int GetFirstAlivePlayerId();
};
#endif // GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H
