/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <game/layers.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"
#include "score.h"

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
class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSay(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConVote(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	
	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;
	bool m_VoteWillPass;
public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }

	CGameContext();
	~CGameContext();
	
	void Clear();
	
	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];

	IGameController *m_pController;
	CGameWorld m_World;
	
	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);
	
	// voting
	void StartVote(const char *pDesc, const char *pCommand);
	void EndVote();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientID);
	
	int m_VoteCreator;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[512];
	char m_aVoteCommand[512];
	int m_VoteEnforce;
	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
	};
	struct CVoteOption
	{
		CVoteOption *m_pNext;
		CVoteOption *m_pPrev;
		char m_aCommand[1];
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOption *m_pVoteOptionFirst;
	CVoteOption *m_pVoteOptionLast;

	// helper functions
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, int Mask=-1);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int Mask);
	void CreateSmoke(vec2 Pos, int Mask=-1);
	void CreateHammerHit(vec2 Pos, int Mask=-1);
	void CreatePlayerSpawn(vec2 Pos, int Mask=-1);
	void CreateDeath(vec2 Pos, int Who, int Mask=-1);
	void CreateSound(vec2 Pos, int Sound, int Mask=-1);
	void CreateSoundGlobal(int Sound, int Target=-1);	


	enum
	{
		CHAT_ALL=-2,
		CHAT_SPEC=-1,
		CHAT_RED=0,
		CHAT_BLUE=1
	};

	// network
	void SendChatTarget(int To, const char *pText);
	void SendChat(int ClientID, int Team, const char *pText, int SpamProtectionClientID = -1);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendBroadcast(const char *pText, int ClientID);
	int ProcessSpamProtection(int ClientID);
	
	
	//
	void CheckPureTuning();
	void SendTuningParams(int ClientID);
	
	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnShutdown();
	
	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();
	
	virtual void OnMessage(int MsgId, CUnpacker *pUnpacker, int ClientID);

	virtual void OnClientConnected(int ClientID);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual const char *Version();
	virtual const char *NetVersion();

	//DDRace
private:
	class IScore *m_pScore;
	int m_VoteEnforcer;

	//DDRace Console Commands

	//static void ConMute(IConsole::IResult *pResult, void *pUserData, int ClientID);
	//static void ConUnmute(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConLogOut(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSetlvl1(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSetlvl2(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSetlvl3(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConKillPlayer(IConsole::IResult *pResult, void *pUserData, int ClientID);

	static void ConNinja(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConHammer(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnSuper(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSuper(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConShotgun(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConGrenade(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConRifle(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConWeapons(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnShotgun(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnGrenade(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnRifle(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnWeapons(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConAddWeapon(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData, int ClientID);

	void ModifyWeapons(int ClientID, int Victim, int Weapon, bool Remove);
	void MoveCharacter(int ClientID, int Victim, int X, int Y, bool Raw = false);

	static void ConTeleport(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConPhook(IConsole::IResult *pResult, void *pUserData, int ClientID);

	static void ConFreeze(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnFreeze(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTimerStop(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTimerStart(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTimerReStart(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTimerZero(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConGoLeft(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConGoRight(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConGoUp(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConGoDown(IConsole::IResult *pResult, void *pUserData, int ClientID);

	static void ConMove(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMoveRaw(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConInvisMe(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConVisMe(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConInvis(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConVis(IConsole::IResult *pResult, void *pUserData, int ClientID);

	static void ConCredits(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConInfo(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConHelp(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConSettings(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConRules(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConKill(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTogglePause(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConTop5(IConsole::IResult *pResult, void *pUserData, int ClientID);
	#if defined(CONF_SQL)
	static void ConTimes(IConsole::IResult *pResult, void *pUserData, int ClientID);
	#endif

	static void ConUTF8(IConsole::IResult *pResult, void *pUserData, int ClientID);	
	static void ConRank(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConBroadTime(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConJoinTeam(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConToggleFly(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMe(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConToggleEyeEmote(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConToggleBroadcast(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConEyeEmote(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConShowOthers(IConsole::IResult *pResult, void *pUserData, int ClientID);

	static void ConAsk(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConYes(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConNo(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConInvite(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConToggleStrict(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMute(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMuteID(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMuteIP(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConUnmute(IConsole::IResult *pResult, void *pUserData, int ClientID);
	static void ConMutes(IConsole::IResult *pResult, void *pUserData, int ClientID);

	enum
	{
		MAX_MUTES=32,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
	};

	CMute m_aMutes[MAX_MUTES];
	int m_NumMutes;
	void Mute(NETADDR *Addr, int Secs, const char *pDisplayName);

public:
	CLayers *Layers() { return &m_Layers; }
	class IScore *Score() { return m_pScore; }
	bool m_VoteKick;
	enum
	{
		VOTE_ENFORCE_NO_ADMIN = VOTE_ENFORCE_YES + 1,
		VOTE_ENFORCE_YES_ADMIN
	};
	void SendRecord(int ClientID);
	static void SendChatResponse(const char *pLine, void *pUser);
	static void SendChatResponseAll(const char *pLine, void *pUser);
	struct ChatResponseInfo
	{
		CGameContext *m_GameContext;
		int m_To;
	};
	virtual void OnSetAuthed(int ClientID,int Level);
	virtual bool PlayerCollision();
	virtual bool PlayerHooking();
};

inline int CmaskAll() { return -1; }
inline int CmaskOne(int ClientID) { return 1<<ClientID; }
inline int CmaskAllExceptOne(int ClientID) { return 0x7fffffff^CmaskOne(ClientID); }
inline bool CmaskIsSet(int Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
