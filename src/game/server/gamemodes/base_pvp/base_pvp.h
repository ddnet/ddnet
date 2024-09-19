#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H
#define GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H

#include <game/server/instagib/extra_columns.h>
#include <game/server/instagib/sql_stats.h>

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
	void OnPlayerTick(class CPlayer *pPlayer);

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
	int GetDefaultWeapon(class CPlayer *pPlayer) override { return m_DefaultWeapon; }
	int GetPlayerTeam(class CPlayer *pPlayer, bool Sixup) override;
	void OnUpdateSpectatorVotesConfig() override;
	bool OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId) override;

	void ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove);

	void BangCommandVote(int ClientId, const char *pCommand, const char *pDesc);
	void ComCallShuffleVote(int ClientId);
	void ComCallSwapTeamsVote(int ClientId);
	void ComCallSwapTeamsRandomVote(int ClientId);
	void ComDropFlag(int ClientId);

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
	void UpdateSpawnWeapons(bool Silent) override;
	enum ESpawnWeapons
	{
		SPAWN_WEAPON_LASER,
		SPAWN_WEAPON_GRENADE,
		NUM_SPAWN_WEAPONS
	};
	ESpawnWeapons m_SpawnWeapons;
	ESpawnWeapons GetSpawnWeapons(int ClientId) const { return m_SpawnWeapons; }
	int GetDefaultWeaponBasedOnSpawnWeapons() const;
	void SetSpawnWeapons(class CCharacter *pChr) override;

	// ddnet-insta only
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
	bool OnChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer) override;
	bool OnFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos) override;
	void SetArmorProgress(CCharacter *pCharacer, int Progress) override{};
	bool OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId) override;
	void OnShowStatsAll(const CSqlStatsPlayer *pStats, class CPlayer *pRequestingPlayer, const char *pRequestedName) override;
	void OnShowRank(int Rank, int RankedScore, const char *pRankType, class CPlayer *pRequestingPlayer, const char *pRequestedName) override;
	void OnRoundStart() override;

	bool IsWinner(const CPlayer *pPlayer, char *pMessage, int SizeOfMessage) override;
	bool IsLoser(const CPlayer *pPlayer) override;

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

	/*
		m_pExtraColums

		Should be allocated in the gamemmodes constructor and will be freed by the base constructor.
		It holds a few methods that describe the extension of the base database layout.
		If a gamemode needs more columns it can implement one. Otherwise it will be a nullptr which is fine.

		Checkout gctf/gctf.h gctf/gctf.cpp and gctf/sql_columns.h for an example
	*/
	CExtraColumns *m_pExtraColumns = nullptr;
};
#endif // GAME_SERVER_GAMEMODES_BASE_PVP_BASE_PVP_H
