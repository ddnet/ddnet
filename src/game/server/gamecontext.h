/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/console.h>
#include <engine/server.h>

#include <game/collision.h>
#include <game/generated/protocol.h>
#include <game/layers.h>
#include <game/mapbugs.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gameworld.h"
#include "teehistorian.h"

#include <memory>
#include <string>

/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/

class CCharacter;
class IConfigManager;
class CConfig;
class CHeap;
class CPlayer;
class CScore;
class CUnpacker;
class IAntibot;
class IGameController;
class IEngine;
class IStorage;
struct CAntibotRoundData;
struct CScoreRandomMapResult;

struct CSnapContext
{
	CSnapContext(int Version, bool Sixup = false) :
		m_ClientVersion(Version), m_Sixup(Sixup)
	{
	}

	int GetClientVersion() const { return m_ClientVersion; }
	bool IsSixup() const { return m_Sixup; }

private:
	int m_ClientVersion;
	bool m_Sixup;
};

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	IConfigManager *m_pConfigManager;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IEngine *m_pEngine;
	IStorage *m_pStorage;
	IAntibot *m_pAntibot;
	CLayers m_Layers;
	CCollision m_Collision;
	protocol7::CNetObjHandler m_NetObjHandler7;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	CTuningParams m_aTuningList[NUM_TUNEZONES];
	std::vector<std::string> m_vCensorlist;

	bool m_TeeHistorianActive;
	CTeeHistorian m_TeeHistorian;
	ASYNCIO *m_pTeeHistorianFile;
	CUuid m_GameUuid;
	CMapBugs m_MapBugs;
	CPrng m_Prng;

	bool m_Resetting;

	static void CommandCallback(int ClientId, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser);
	static void TeeHistorianWrite(const void *pData, int DataSize, void *pUser);

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTunes(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDumpZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneResetZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgEnter(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgLeave(IConsole::IResult *pResult, void *pUserData);
	static void ConMapbug(IConsole::IResult *pResult, void *pUserData);
	static void ConSwitchOpen(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConHotReload(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConAddMapVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteNo(IConsole::IResult *pResult, void *pUserData);
	static void ConDrySave(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpAntibot(IConsole::IResult *pResult, void *pUserData);
	static void ConAntibot(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConDumpLog(IConsole::IResult *pResult, void *pUserData);

	void Construct(int Resetting);
	void Destruct(int Resetting);
	void AddVote(const char *pDescription, const char *pCommand);
	static int MapScan(const char *pName, int IsDir, int DirType, void *pUserData);

	struct CPersistentData
	{
		CUuid m_PrevGameUuid;
	};

	struct CPersistentClientData
	{
		bool m_IsSpectator;
		bool m_IsAfk;
		int m_LastWhisperTo;
	};

public:
	IServer *Server() const { return m_pServer; }
	IConfigManager *ConfigManager() const { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IConsole *Console() { return m_pConsole; }
	IEngine *Engine() { return m_pEngine; }
	IStorage *Storage() { return m_pStorage; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	CTuningParams *TuningList() { return &m_aTuningList[0]; }
	IAntibot *Antibot() { return m_pAntibot; }
	CTeeHistorian *TeeHistorian() { return &m_TeeHistorian; }
	bool TeeHistorianActive() const { return m_TeeHistorianActive; }

	CGameContext();
	CGameContext(int Reset);
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];
	// keep last input to always apply when none is sent
	CNetObj_PlayerInput m_aLastPlayerInput[MAX_CLIENTS];
	bool m_aPlayerHasInput[MAX_CLIENTS];
	CSaveTeam *m_apSavedTeams[MAX_CLIENTS];
	CSaveHotReloadTee *m_apSavedTees[MAX_CLIENTS];
	int m_aTeamMapping[MAX_CLIENTS];

	// returns last input if available otherwise nulled PlayerInput object
	// ClientId has to be valid
	CNetObj_PlayerInput GetLastPlayerInput(int ClientId) const;

	IGameController *m_pController;
	CGameWorld m_World;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientId);
	bool EmulateBug(int Bug);
	std::vector<SSwitchers> &Switchers() { return m_World.m_Core.m_vSwitchers; }

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc);
	void EndVote();
	void SendVoteSet(int ClientId);
	void SendVoteStatus(int ClientId, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientId);

	int m_VoteCreator;
	int m_VoteType;
	int64_t m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aSixupVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	char m_aaZoneEnterMsg[NUM_TUNEZONES][256]; // 0 is used for switching from or to area without tunings
	char m_aaZoneLeaveMsg[NUM_TUNEZONES][256];

	void CreateAllEntities(bool Initial);

	char m_aDeleteTempfile[128];
	void DeleteTempfile();

	enum
	{
		VOTE_ENFORCE_UNKNOWN = 0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
		VOTE_ENFORCE_NO_ADMIN,
		VOTE_ENFORCE_YES_ADMIN,
		VOTE_ENFORCE_ABORT,
		VOTE_ENFORCE_CANCEL,
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, CClientMask Mask = CClientMask().set());
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, CClientMask Mask = CClientMask().set());
	void CreateHammerHit(vec2 Pos, CClientMask Mask = CClientMask().set());
	void CreatePlayerSpawn(vec2 Pos, CClientMask Mask = CClientMask().set());
	void CreateDeath(vec2 Pos, int ClientId, CClientMask Mask = CClientMask().set());
	void CreateBirthdayEffect(vec2 Pos, CClientMask Mask = CClientMask().set());
	void CreateFinishEffect(vec2 Pos, CClientMask Mask = CClientMask().set());
	void CreateSound(vec2 Pos, int Sound, CClientMask Mask = CClientMask().set());
	void CreateSoundGlobal(int Sound, int Target = -1) const;

	void SnapSwitchers(int SnappingClient);
	bool SnapLaserObject(const CSnapContext &Context, int SnapId, const vec2 &To, const vec2 &From, int StartTick, int Owner = -1, int LaserType = -1, int Subtype = -1, int SwitchNumber = -1) const;
	bool SnapPickup(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType, int SwitchNumber) const;

	enum
	{
		FLAG_SIX = 1 << 0,
		FLAG_SIXUP = 1 << 1,
	};

	// network
	void CallVote(int ClientId, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc = nullptr);
	void SendChatTarget(int To, const char *pText, int VersionFlags = FLAG_SIX | FLAG_SIXUP) const;
	void SendChatTeam(int Team, const char *pText) const;
	void SendChat(int ClientId, int Team, const char *pText, int SpamProtectionClientId = -1, int VersionFlags = FLAG_SIX | FLAG_SIXUP);
	void SendStartWarning(int ClientId, const char *pMessage);
	void SendEmoticon(int ClientId, int Emoticon, int TargetClientId) const;
	void SendWeaponPickup(int ClientId, int Weapon) const;
	void SendMotd(int ClientId) const;
	void SendSettings(int ClientId) const;
	void SendBroadcast(const char *pText, int ClientId, bool IsImportant = true);

	void List(int ClientId, const char *pFilter);

	//
	void CheckPureTuning();
	void SendTuningParams(int ClientId, int Zone = 0);

	const CVoteOptionServer *GetVoteOption(int Index) const;
	void ProgressVoteOptions(int ClientId);

	//
	void LoadMapSettings();

	// engine events
	void OnInit(const void *pPersistentData) override;
	void OnConsoleInit() override;
	void RegisterDDRaceCommands();
	void RegisterChatCommands();
	void OnMapChange(char *pNewMapName, int MapNameSize) override;
	void OnShutdown(void *pPersistentData) override;

	void OnTick() override;
	void OnPreSnap() override;
	void OnSnap(int ClientId) override;
	void OnPostSnap() override;

	void UpdatePlayerMaps();

	void *PreProcessMsg(int *pMsgId, CUnpacker *pUnpacker, int ClientId);
	void CensorMessage(char *pCensoredMessage, const char *pMessage, int Size);
	void OnMessage(int MsgId, CUnpacker *pUnpacker, int ClientId) override;
	void OnSayNetMessage(const CNetMsg_Cl_Say *pMsg, int ClientId, const CUnpacker *pUnpacker);
	void OnCallVoteNetMessage(const CNetMsg_Cl_CallVote *pMsg, int ClientId);
	void OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId);
	void OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId);
	void OnIsDDNetLegacyNetMessage(const CNetMsg_Cl_IsDDNetLegacy *pMsg, int ClientId, CUnpacker *pUnpacker);
	void OnShowOthersLegacyNetMessage(const CNetMsg_Cl_ShowOthersLegacy *pMsg, int ClientId);
	void OnShowOthersNetMessage(const CNetMsg_Cl_ShowOthers *pMsg, int ClientId);
	void OnShowDistanceNetMessage(const CNetMsg_Cl_ShowDistance *pMsg, int ClientId);
	void OnCameraInfoNetMessage(const CNetMsg_Cl_CameraInfo *pMsg, int ClientId);
	void OnSetSpectatorModeNetMessage(const CNetMsg_Cl_SetSpectatorMode *pMsg, int ClientId);
	void OnChangeInfoNetMessage(const CNetMsg_Cl_ChangeInfo *pMsg, int ClientId);
	void OnEmoticonNetMessage(const CNetMsg_Cl_Emoticon *pMsg, int ClientId);
	void OnKillNetMessage(const CNetMsg_Cl_Kill *pMsg, int ClientId);
	void OnStartInfoNetMessage(const CNetMsg_Cl_StartInfo *pMsg, int ClientId);

	bool OnClientDataPersist(int ClientId, void *pData) override;
	void OnClientConnected(int ClientId, void *pData) override;
	void OnClientEnter(int ClientId) override;
	void OnClientDrop(int ClientId, const char *pReason) override;
	void OnClientPrepareInput(int ClientId, void *pInput) override;
	void OnClientDirectInput(int ClientId, void *pInput) override;
	void OnClientPredictedInput(int ClientId, void *pInput) override;
	void OnClientPredictedEarlyInput(int ClientId, void *pInput) override;

	void TeehistorianRecordAntibot(const void *pData, int DataSize) override;
	void TeehistorianRecordPlayerJoin(int ClientId, bool Sixup) override;
	void TeehistorianRecordPlayerDrop(int ClientId, const char *pReason) override;
	void TeehistorianRecordPlayerRejoin(int ClientId) override;
	void TeehistorianRecordPlayerName(int ClientId, const char *pName) override;
	void TeehistorianRecordPlayerFinish(int ClientId, int TimeTicks) override;
	void TeehistorianRecordTeamFinish(int TeamId, int TimeTicks) override;

	bool IsClientReady(int ClientId) const override;
	bool IsClientPlayer(int ClientId) const override;
	int PersistentDataSize() const override { return sizeof(CPersistentData); }
	int PersistentClientDataSize() const override { return sizeof(CPersistentClientData); }

	CUuid GameUuid() const override;
	const char *GameType() const override;
	const char *Version() const override;
	const char *NetVersion() const override;

	// DDRace
	void OnPreTickTeehistorian() override;
	bool OnClientDDNetVersionKnown(int ClientId);
	void FillAntibot(CAntibotRoundData *pData) override;
	bool ProcessSpamProtection(int ClientId, bool RespectChatInitialDelay = true);
	int GetDDRaceTeam(int ClientId) const;
	// Describes the time when the first player joined the server.
	int64_t m_NonEmptySince;
	int64_t m_LastMapVote;
	int GetClientVersion(int ClientId) const;
	CClientMask ClientsMaskExcludeClientVersionAndHigher(int Version) const;
	bool PlayerExists(int ClientId) const override { return m_apPlayers[ClientId]; }
	// Returns true if someone is actively moderating.
	bool PlayerModerating() const;
	void ForceVote(int EnforcerId, bool Success);

	// Checks if player can vote and notify them about the reason
	bool RateLimitPlayerVote(int ClientId);
	bool RateLimitPlayerMapVote(int ClientId) const;

	void OnUpdatePlayerServerInfo(CJsonStringWriter *pJSonWriter, int Id) override;
	void ReadCensorList();

	std::shared_ptr<CScoreRandomMapResult> m_SqlRandomMapResult;

private:
	// starting 1 to make 0 the special value "no client id"
	uint32_t NextUniqueClientId = 1;
	bool m_VoteWillPass;
	CScore *m_pScore;

	// DDRace Console Commands

	static void ConKillPlayer(IConsole::IResult *pResult, void *pUserData);

	static void ConNinja(IConsole::IResult *pResult, void *pUserData);
	static void ConUnNinja(IConsole::IResult *pResult, void *pUserData);
	static void ConEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConUnEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConSolo(IConsole::IResult *pResult, void *pUserData);
	static void ConUnSolo(IConsole::IResult *pResult, void *pUserData);
	static void ConFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConUnFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConDeep(IConsole::IResult *pResult, void *pUserData);
	static void ConUnDeep(IConsole::IResult *pResult, void *pUserData);
	static void ConLiveFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConUnLiveFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConUnSuper(IConsole::IResult *pResult, void *pUserData);
	static void ConSuper(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleInvincible(IConsole::IResult *pResult, void *pUserData);
	static void ConShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConEndlessJump(IConsole::IResult *pResult, void *pUserData);
	static void ConSetJumps(IConsole::IResult *pResult, void *pUserData);
	static void ConWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConUnShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConUnGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConUnLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConUnJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConUnEndlessJump(IConsole::IResult *pResult, void *pUserData);
	static void ConUnWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConAddWeapon(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData);
	void ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove);
	void MoveCharacter(int ClientId, int X, int Y, bool Raw = false);
	static void ConGoLeft(IConsole::IResult *pResult, void *pUserData);
	static void ConGoRight(IConsole::IResult *pResult, void *pUserData);
	static void ConGoUp(IConsole::IResult *pResult, void *pUserData);
	static void ConGoDown(IConsole::IResult *pResult, void *pUserData);
	static void ConMove(IConsole::IResult *pResult, void *pUserData);
	static void ConMoveRaw(IConsole::IResult *pResult, void *pUserData);

	static void ConToTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData);
	void Teleport(CCharacter *pChr, vec2 Pos);
	static void ConTeleport(IConsole::IResult *pResult, void *pUserData);

	static void ConCredits(IConsole::IResult *pResult, void *pUserData);
	static void ConInfo(IConsole::IResult *pResult, void *pUserData);
	static void ConHelp(IConsole::IResult *pResult, void *pUserData);
	static void ConSettings(IConsole::IResult *pResult, void *pUserData);
	static void ConRules(IConsole::IResult *pResult, void *pUserData);
	static void ConKill(IConsole::IResult *pResult, void *pUserData);
	static void ConTogglePause(IConsole::IResult *pResult, void *pUserData);
	static void ConTogglePauseVoted(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleSpec(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleSpecVoted(IConsole::IResult *pResult, void *pUserData);
	static void ConForcePause(IConsole::IResult *pResult, void *pUserData);
	static void ConTeamTop5(IConsole::IResult *pResult, void *pUserData);
	static void ConTop(IConsole::IResult *pResult, void *pUserData);
	static void ConTimes(IConsole::IResult *pResult, void *pUserData);
	static void ConPoints(IConsole::IResult *pResult, void *pUserData);
	static void ConTopPoints(IConsole::IResult *pResult, void *pUserData);
	static void ConTimeCP(IConsole::IResult *pResult, void *pUserData);

	static void ConDND(IConsole::IResult *pResult, void *pUserData);
	static void ConWhispers(IConsole::IResult *pResult, void *pUserData);
	static void ConMapInfo(IConsole::IResult *pResult, void *pUserData);
	static void ConTimeout(IConsole::IResult *pResult, void *pUserData);
	static void ConPractice(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeCmdList(IConsole::IResult *pResult, void *pUserData);
	static void ConSwap(IConsole::IResult *pResult, void *pUserData);
	static void ConCancelSwap(IConsole::IResult *pResult, void *pUserData);
	static void ConSave(IConsole::IResult *pResult, void *pUserData);
	static void ConLoad(IConsole::IResult *pResult, void *pUserData);
	static void ConMap(IConsole::IResult *pResult, void *pUserData);
	static void ConTeamRank(IConsole::IResult *pResult, void *pUserData);
	static void ConRank(IConsole::IResult *pResult, void *pUserData);
	static void ConTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConLock(IConsole::IResult *pResult, void *pUserData);
	static void ConUnlock(IConsole::IResult *pResult, void *pUserData);
	static void ConInvite(IConsole::IResult *pResult, void *pUserData);
	static void ConJoin(IConsole::IResult *pResult, void *pUserData);
	static void ConTeam0Mode(IConsole::IResult *pResult, void *pUserData);
	static void ConMe(IConsole::IResult *pResult, void *pUserData);
	static void ConWhisper(IConsole::IResult *pResult, void *pUserData);
	static void ConConverse(IConsole::IResult *pResult, void *pUserData);
	static void ConSetEyeEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConEyeEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConShowOthers(IConsole::IResult *pResult, void *pUserData);
	static void ConShowAll(IConsole::IResult *pResult, void *pUserData);
	static void ConSpecTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConNinjaJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTime(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTimeAll(IConsole::IResult *pResult, void *pUserData);
	static void ConTime(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTimerType(IConsole::IResult *pResult, void *pUserData);
	static void ConRescue(IConsole::IResult *pResult, void *pUserData);
	static void ConRescueMode(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleTo(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleXY(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleCursor(IConsole::IResult *pResult, void *pUserData);
	static void ConLastTele(IConsole::IResult *pResult, void *pUserData);

	// Chat commands for practice mode
	static void ConPracticeToTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeToCheckTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnSolo(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeSolo(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnDeep(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeDeep(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnLiveFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeLiveFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeEndlessJump(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeSetJumps(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnEndlessJump(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeNinja(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnNinja(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeUnEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeToggleInvincible(IConsole::IResult *pResult, void *pUserData);

	static void ConPracticeAddWeapon(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeRemoveWeapon(IConsole::IResult *pResult, void *pUserData);

	static void ConProtectedKill(IConsole::IResult *pResult, void *pUserData);

	static void ConVoteMute(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteMutes(IConsole::IResult *pResult, void *pUserData);
	static void ConMute(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteId(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteIp(IConsole::IResult *pResult, void *pUserData);
	static void ConUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConUnmuteId(IConsole::IResult *pResult, void *pUserData);
	static void ConMutes(IConsole::IResult *pResult, void *pUserData);
	static void ConModerate(IConsole::IResult *pResult, void *pUserData);

	static void ConList(IConsole::IResult *pResult, void *pUserData);
	static void ConSetDDRTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConUninvite(IConsole::IResult *pResult, void *pUserData);
	static void ConFreezeHammer(IConsole::IResult *pResult, void *pUserData);
	static void ConUnFreezeHammer(IConsole::IResult *pResult, void *pUserData);

	static void ConReloadCensorlist(IConsole::IResult *pResult, void *pUserData);

	CCharacter *GetPracticeCharacter(IConsole::IResult *pResult);

	enum
	{
		MAX_MUTES = 128,
		MAX_VOTE_MUTES = 128,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
		char m_aReason[128];
		bool m_InitialChatDelay;
	};

	CMute m_aMutes[MAX_MUTES];
	int m_NumMutes;
	CMute m_aVoteMutes[MAX_VOTE_MUTES];
	int m_NumVoteMutes;
	bool TryMute(const NETADDR *pAddr, int Secs, const char *pReason, bool InitialChatDelay);
	void Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName, const char *pReason = "", bool InitialChatDelay = false);
	bool TryVoteMute(const NETADDR *pAddr, int Secs, const char *pReason);
	void VoteMute(const NETADDR *pAddr, int Secs, const char *pReason, const char *pDisplayName, int AuthedId);
	bool VoteUnmute(const NETADDR *pAddr, const char *pDisplayName, int AuthedId);
	void Whisper(int ClientId, char *pStr);
	void WhisperId(int ClientId, int VictimId, const char *pMessage);
	void Converse(int ClientId, char *pStr);
	bool IsVersionBanned(int Version);
	void UnlockTeam(int ClientId, int Team) const;
	void AttemptJoinTeam(int ClientId, int Team);

	enum
	{
		MAX_LOG_SECONDS = 600,
		MAX_LOGS = 512,
	};
	struct CLog
	{
		int64_t m_Timestamp;
		bool m_FromServer;
		char m_aDescription[128];
		int m_ClientVersion;
		char m_aClientName[MAX_NAME_LENGTH];
		char m_aClientAddrStr[NETADDR_MAXSTRSIZE];
	};
	CLog m_aLogs[MAX_LOGS];
	int m_LatestLog;

	void LogEvent(const char *Description, int ClientId);

public:
	CLayers *Layers() { return &m_Layers; }
	CScore *Score() { return m_pScore; }

	enum
	{
		VOTE_TYPE_UNKNOWN = 0,
		VOTE_TYPE_OPTION,
		VOTE_TYPE_KICK,
		VOTE_TYPE_SPECTATE,
	};
	int m_VoteVictim;

	inline bool IsOptionVote() const { return m_VoteType == VOTE_TYPE_OPTION; }
	inline bool IsKickVote() const { return m_VoteType == VOTE_TYPE_KICK; }
	inline bool IsSpecVote() const { return m_VoteType == VOTE_TYPE_SPECTATE; }

	void SendRecord(int ClientId);
	void OnSetAuthed(int ClientId, int Level) override;

	void ResetTuning();
};

#endif
