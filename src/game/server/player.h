/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.h"
#include "gamecontext.h"

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()
	
public:
	CPlayer(CGameContext *pGameServer, int ClientID, int Team);
	~CPlayer();

	void Init(int CID);

	void TryRespawn();
	void Respawn();
	void SetTeam(int Team);
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };
	
	void Tick();
	void Snap(int SnappingClient);

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect();
	
	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();
	
	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	
	//
	int m_Vote;
	int m_VotePos;
	//
	int m_Last_VoteCall;
	int m_Last_VoteTry;
	int m_Last_Chat;
	int m_Last_SetTeam;
	int m_Last_ChangeInfo;
	int m_Last_Emote;
	int m_Last_Kill;
	
	// TODO: clean this up
	struct 
	{
		char m_SkinName[64];
		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;
	} m_TeeInfos;
	
	int m_RespawnTick;
	int m_DieTick;
	int m_Score;
	int m_ScoreStartTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;
	
private:
	CCharacter *Character;
	CGameContext *m_pGameServer;
	
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;
	
	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;

	// network latency calculations	
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;	
	} m_Latency;

	//DDRace
public:
	struct PauseInfo
	{
		CCharacterCore m_Core;
		int m_StartTime;
		int m_DDRaceState;
		//int m_RefreshTime;
		int m_FreezeTime;
		int m_Armor;
		int m_LastMove;
		vec2 m_PrevPos;
		int m_ActiveWeapon;
		int m_LastWeapon;
		bool m_Respawn;
		bool m_aHasWeapon[NUM_WEAPONS];
		int m_HammerType;
		bool m_Super;
		bool m_DeepFreeze;
		bool m_EndlessHook;
		int m_PauseTime;
		int m_Team;
	} m_PauseInfo;
	bool m_InfoSaved;
	void LoadCharacter();
	void SaveCharacter();
	int64 m_Last_Pause;
	int64 m_Last_KickVote;
	int64 m_Last_Team;
	bool m_Invisible;
	int m_Authed;
	bool m_IsUsingDDRaceClient;
	bool m_ShowOthers;
	bool m_RconFreeze;

	int m_ChatScore;

	// necessary for asking mutual exclusion
	int m_Asker; // who asked this player
	int m_AskedTick; // when was this player asked by another player
	int m_Asked; // who did this player ask
	int m_AskerTick; // when did this player ask another player
};

#endif
