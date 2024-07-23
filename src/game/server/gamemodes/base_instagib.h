#ifndef GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H

#include "DDRace.h"

class CGameControllerInstagib : public CGameControllerDDRace
{
public:
	CGameControllerInstagib(class CGameContext *pGameServer);
	~CGameControllerInstagib() override;

	// convience accessors to copy code from gamecontext to the instagib controller
	class IServer *Server() const { return GameServer()->Server(); }
	class CConfig *Config() { return GameServer()->Config(); }
	class IConsole *Console() { return GameServer()->Console(); }
	class IStorage *Storage() { return GameServer()->Storage(); }
	void SendChatTarget(int To, const char *pText, int Flags = CGameContext::CHAT_SIX | CGameContext::CHAT_SIXUP) const;
	void SendChat(int ClientId, int Team, const char *pText, int SpamProtectionClientId = -1, int Flags = CGameContext::CHAT_SIX | CGameContext::CHAT_SIXUP);

	void OnPlayerConstruct(class CPlayer *pPlayer);
	void OnCharacterConstruct(class CCharacter *pChr);

	void SendChatSpectators(const char *pMessage, int Flags);
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;

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

	//Anticamper
	void Anticamper();
};
#endif // GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H
