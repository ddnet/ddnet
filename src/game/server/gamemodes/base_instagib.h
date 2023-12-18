#ifndef GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H

#include "DDRace.h"

class CGameControllerInstagib : public CGameControllerDDRace
{
public:
	CGameControllerInstagib(class CGameContext *pGameServer);
	~CGameControllerInstagib();

	// convience accessors to copy code from gamecontext to the instagib controller
	class IServer *Server() const { return GameServer()->Server(); }
	class CConfig *Config() { return GameServer()->Config(); }
	class IConsole *Console() { return GameServer()->Console(); }
	class IStorage *Storage() { return GameServer()->Storage(); }
	void SendChatTarget(int To, const char *pText, int Flags = CGameContext::CHAT_SIX | CGameContext::CHAT_SIXUP) const;
	void SendChat(int ClientID, int Team, const char *pText, int SpamProtectionClientID = -1, int Flags = CGameContext::CHAT_SIX | CGameContext::CHAT_SIXUP);

	void OnPlayerConstruct(class CPlayer *pPlayer);
	void OnCharacterConstruct(class CCharacter *pChr);

	void SendChatSpectators(const char *pMessage, int Flags);
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;

	static void ConchainInstaSettingsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainResetInstasettingTees(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConHammer(IConsole::IResult *pResult, void *pUserData);
	static void ConGun(IConsole::IResult *pResult, void *pUserData);
	static void ConUnHammer(IConsole::IResult *pResult, void *pUserData);
	static void ConUnGun(IConsole::IResult *pResult, void *pUserData);
	static void ConGodmode(IConsole::IResult *pResult, void *pUserData);
	static void ConShuffleTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConSwapTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConSwapTeamsRandom(IConsole::IResult *pResult, void *pUserData);

	void SwapTeams();
	void ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove);

	bool AllowPublicChat(const CPlayer *pPlayer);
	bool ParseChatCmd(char Prefix, int ClientID, const char *pCmdWithArgs);
	bool OnBangCommand(int ClientID, const char *pCmd, int NumArgs, const char **ppArgs);
	void AddSpree(CPlayer *pPlayer);
	void EndSpree(CPlayer *pPlayer, CPlayer *pKiller);
	enum ESpawnWeapons
	{
		SPAWN_WEAPON_LASER,
		SPAWN_WEAPON_GRENADE,
		NUM_SPAWN_WEAPONS
	};
	ESpawnWeapons m_SpawnWeapons;
	ESpawnWeapons GetSpawnWeapons(int ClientID) { return m_SpawnWeapons; }
	void SetSpawnWeapons(class CCharacter *pChr);

	static void ConchainSpawnWeapons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// ddnet-insta only
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
	bool OnChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer) override;
};
#endif // GAME_SERVER_GAMEMODES_BASE_INSTAGIB_H
