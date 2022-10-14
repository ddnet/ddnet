/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <vector>

#include "base/system.h"
#include "gamecontext.h"
#include "teeinfo.h"
#include <antibot/antibot_data.h>
#include <base/logger.h>
#include <base/math.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/shared/memheap.h>
#include <engine/storage.h>

#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/version.h>

#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include "entities/character.h"
#include "gamemodes/DDRace.h"
#include "player.h"
#include "score.h"

// Not thread-safe!
class CClientChatLogger : public ILogger
{
	CGameContext *m_pGameServer;
	int m_ClientID;
	ILogger *m_pOuterLogger;

public:
	CClientChatLogger(CGameContext *pGameServer, int ClientID, ILogger *pOuterLogger) :
		m_pGameServer(pGameServer),
		m_ClientID(ClientID),
		m_pOuterLogger(pOuterLogger)
	{
	}
	void Log(const CLogMessage *pMessage) override;
};

void CClientChatLogger::Log(const CLogMessage *pMessage)
{
	if(str_comp(pMessage->m_aSystem, "chatresp") == 0)
	{
		m_pGameServer->SendChatTarget(m_ClientID, pMessage->Message());
	}
	else
	{
		m_pOuterLogger->Log(pMessage);
	}
}

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = false;
	m_pServer = 0;

	for(auto &pPlayer : m_apPlayers)
		pPlayer = 0;

	mem_zero(&m_aLastPlayerInput, sizeof(m_aLastPlayerInput));
	mem_zero(&m_aPlayerHasInput, sizeof(m_aPlayerHasInput));

	m_pController = 0;
	m_aVoteCommand[0] = 0;
	m_VoteType = VOTE_TYPE_UNKNOWN;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LastMapVote = 0;

	m_SqlRandomMapResult = nullptr;

	m_pScore = nullptr;
	m_NumMutes = 0;
	m_NumVoteMutes = 0;

	if(Resetting == NO_RESET)
	{
		m_NonEmptySince = 0;
		m_pVoteOptionHeap = new CHeap();
	}

	m_aDeleteTempfile[0] = 0;
	m_TeeHistorianActive = false;
}

void CGameContext::Destruct(int Resetting)
{
	for(auto &pPlayer : m_apPlayers)
		delete pPlayer;

	if(Resetting == NO_RESET)
		delete m_pVoteOptionHeap;

	if(m_pScore)
	{
		delete m_pScore;
		m_pScore = nullptr;
	}
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::CGameContext(int Reset)
{
	Construct(Reset);
}

CGameContext::~CGameContext()
{
	Destruct(m_Resetting ? RESET : NO_RESET);
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	new(this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}

void CGameContext::TeeHistorianWrite(const void *pData, int DataSize, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	aio_write(pSelf->m_pTeeHistorianFile, pData, DataSize);
}

void CGameContext::CommandCallback(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	if(pSelf->m_TeeHistorianActive)
	{
		pSelf->m_TeeHistorian.RecordConsoleCommand(ClientID, FlagMask, pCmd, pResult);
	}
}

CNetObj_PlayerInput CGameContext::GetLastPlayerInput(int ClientID) const
{
	dbg_assert(0 <= ClientID && ClientID < MAX_CLIENTS, "invalid ClientID");
	return m_aLastPlayerInput[ClientID];
}

class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

bool CGameContext::EmulateBug(int Bug)
{
	return m_MapBugs.Contains(Bug);
}

void CGameContext::FillAntibot(CAntibotRoundData *pData)
{
	if(!pData->m_Map.m_pTiles)
	{
		Collision()->FillAntibot(&pData->m_Map);
	}
	pData->m_Tick = Server()->Tick();
	mem_zero(pData->m_aCharacters, sizeof(pData->m_aCharacters));
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CAntibotCharacterData *pChar = &pData->m_aCharacters[i];
		for(auto &LatestInput : pChar->m_aLatestInputs)
		{
			LatestInput.m_TargetX = -1;
			LatestInput.m_TargetY = -1;
		}
		pChar->m_Alive = false;
		pChar->m_Pause = false;
		pChar->m_Team = -1;

		pChar->m_Pos = vec2(-1, -1);
		pChar->m_Vel = vec2(0, 0);
		pChar->m_Angle = -1;
		pChar->m_HookedPlayer = -1;
		pChar->m_SpawnTick = -1;
		pChar->m_WeaponChangeTick = -1;

		if(m_apPlayers[i])
		{
			str_copy(pChar->m_aName, Server()->ClientName(i), sizeof(pChar->m_aName));
			CCharacter *pGameChar = m_apPlayers[i]->GetCharacter();
			pChar->m_Alive = (bool)pGameChar;
			pChar->m_Pause = m_apPlayers[i]->IsPaused();
			pChar->m_Team = m_apPlayers[i]->GetTeam();
			if(pGameChar)
			{
				pGameChar->FillAntibot(pChar);
			}
		}
	}
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount, int64_t Mask)
{
	float a = 3 * 3.14159f / 2 + Angle;
	//float a = get_angle(dir);
	float s = a - pi / 3;
	float e = a + pi / 3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i + 1) / float(Amount + 2));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), Mask);
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f * 256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos, int64_t Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CCharacter *apEnts[MAX_CLIENTS];
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int Num = m_World.FindEntities(Pos, Radius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	int64_t TeamMask = -1;
	for(int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->m_Pos - Pos;
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Strength;
		if(Owner == -1 || !m_apPlayers[Owner] || !m_apPlayers[Owner]->m_TuneZone)
			Strength = Tuning()->m_ExplosionStrength;
		else
			Strength = TuningList()[m_apPlayers[Owner]->m_TuneZone].m_ExplosionStrength;

		float Dmg = Strength * l;
		if(!(int)Dmg)
			continue;

		if((GetPlayerChar(Owner) ? !GetPlayerChar(Owner)->GrenadeHitDisabled() : g_Config.m_SvHit) || NoDamage || Owner == apEnts[i]->GetPlayer()->GetCID())
		{
			if(Owner != -1 && apEnts[i]->IsAlive() && !apEnts[i]->CanCollide(Owner))
				continue;
			if(Owner == -1 && ActivatedTeam != -1 && apEnts[i]->IsAlive() && apEnts[i]->Team() != ActivatedTeam)
				continue;

			// Explode at most once per team
			int PlayerTeam = apEnts[i]->Team();
			if((GetPlayerChar(Owner) ? GetPlayerChar(Owner)->GrenadeHitDisabled() : !g_Config.m_SvHit) || NoDamage)
			{
				if(!CmaskIsSet(TeamMask, PlayerTeam))
					continue;
				TeamMask = CmaskUnset(TeamMask, PlayerTeam);
			}

			apEnts[i]->TakeDamage(ForceDir * Dmg * 2, (int)Dmg, Owner, Weapon);
		}
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos, int64_t Mask)
{
	// create the event
	CNetEvent_Spawn *pEvent = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID, int64_t Mask)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int64_t Mask)
{
	if(Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if(Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if(Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if(Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}

void CGameContext::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64_t Now = Server()->Tick();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pPlayer)
		return;

	SendChat(-1, CGameContext::CHAT_ALL, pChatmsg, -1, CHAT_SIX);
	if(!pSixupDesc)
		pSixupDesc = pDesc;

	m_VoteCreator = ClientID;
	StartVote(pDesc, pCmd, pReason, pSixupDesc);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
	pPlayer->m_LastVoteCall = Now;
}

void CGameContext::SendChatTarget(int To, const char *pText, int Flags)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;

	if(g_Config.m_SvDemoChat)
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

	if(To == -1)
	{
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!((Server()->IsSixup(i) && (Flags & CHAT_SIXUP)) ||
				   (!Server()->IsSixup(i) && (Flags & CHAT_SIX))))
				continue;

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		if(!((Server()->IsSixup(To) && (Flags & CHAT_SIXUP)) ||
			   (!Server()->IsSixup(To) && (Flags & CHAT_SIX))))
			return;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, To);
	}
}

void CGameContext::SendChatTeam(int Team, const char *pText)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(((CGameControllerDDRace *)m_pController)->m_Teams.m_Core.Team(i) == Team)
			SendChatTarget(i, pText);
}

void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText, int SpamProtectionClientID, int Flags)
{
	if(SpamProtectionClientID >= 0 && SpamProtectionClientID < MAX_CLIENTS)
		if(ProcessSpamProtection(SpamProtectionClientID))
			return;

	char aBuf[256], aText[256];
	str_copy(aText, pText, sizeof(aText));
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), aText);
	else if(ChatterClientID == -2)
	{
		str_format(aBuf, sizeof(aBuf), "### %s", aText);
		str_copy(aText, aBuf, sizeof(aText));
		ChatterClientID = -1;
	}
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", aText);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Team != CHAT_ALL ? "teamchat" : "chat", aBuf);

	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = aText;

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!m_apPlayers[i])
				continue;
			bool Send = (Server()->IsSixup(i) && (Flags & CHAT_SIXUP)) ||
				    (!Server()->IsSixup(i) && (Flags & CHAT_SIX));

			if(!m_apPlayers[i]->m_DND && Send)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		CTeamsCore *pTeams = &((CGameControllerDDRace *)m_pController)->m_Teams.m_Core;
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = aText;

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(m_apPlayers[i] != 0)
			{
				if(Team == CHAT_SPEC)
				{
					if(m_apPlayers[i]->GetTeam() == CHAT_SPEC)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
				else
				{
					if(pTeams->Team(i) == Team && m_apPlayers[i]->GetTeam() != CHAT_SPEC)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
			}
		}
	}
}

void CGameContext::SendStartWarning(int ClientID, const char *pMessage)
{
	CCharacter *pChr = GetPlayerChar(ClientID);
	if(pChr && pChr->m_LastStartWarning < Server()->Tick() - 3 * Server()->TickSpeed())
	{
		SendChatTarget(ClientID, pMessage);
		pChr->m_LastStartWarning = Server()->Tick();
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendMotd(int ClientID)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSettings(int ClientID)
{
	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_ServerSettings Msg;
		Msg.m_KickVote = g_Config.m_SvVoteKick;
		Msg.m_KickMin = g_Config.m_SvVoteKickMin;
		Msg.m_SpecVote = g_Config.m_SvVoteSpectate;
		Msg.m_TeamLock = 0;
		Msg.m_TeamBalance = 0;
		Msg.m_PlayerSlots = g_Config.m_SvMaxClients - g_Config.m_SvSpectatorSlots;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

void CGameContext::SendBroadcast(const char *pText, int ClientID, bool IsImportant)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;

	if(ClientID == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

		for(auto &pPlayer : m_apPlayers)
		{
			if(pPlayer)
			{
				pPlayer->m_LastBroadcastImportance = true;
				pPlayer->m_LastBroadcast = Server()->Tick();
			}
		}
		return;
	}

	if(!m_apPlayers[ClientID])
		return;

	if(!IsImportant && m_apPlayers[ClientID]->m_LastBroadcastImportance && m_apPlayers[ClientID]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	m_apPlayers[ClientID]->m_LastBroadcast = Server()->Tick();
	m_apPlayers[ClientID]->m_LastBroadcastImportance = IsImportant;
}

void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc)
{
	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	m_VoteEnforcer = -1;
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
		{
			pPlayer->m_Vote = 0;
			pPlayer->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * g_Config.m_SvVoteTime;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aSixupVoteDescription, pSixupDesc, sizeof(m_aSixupVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}

void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	::CNetMsg_Sv_VoteSet Msg6;
	protocol7::CNetMsg_Sv_VoteSet Msg7;

	Msg7.m_ClientID = m_VoteCreator;
	if(m_VoteCloseTime)
	{
		Msg6.m_Timeout = Msg7.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg6.m_pDescription = m_aVoteDescription;
		Msg7.m_pDescription = m_aSixupVoteDescription;
		Msg6.m_pReason = Msg7.m_pReason = m_aVoteReason;

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(IsKickVote())
			Type = protocol7::VOTE_START_KICK;
		else if(IsSpecVote())
			Type = protocol7::VOTE_START_SPEC;
		else if(IsOptionVote())
			Type = protocol7::VOTE_START_OP;
	}
	else
	{
		Msg6.m_Timeout = Msg7.m_Timeout = 0;
		Msg6.m_pDescription = Msg7.m_pDescription = "";
		Msg6.m_pReason = Msg7.m_pReason = "";

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(m_VoteEnforce == VOTE_ENFORCE_NO || m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			Type = protocol7::VOTE_END_FAIL;
		else if(m_VoteEnforce == VOTE_ENFORCE_YES || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Type = protocol7::VOTE_END_PASS;
		else if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
			Type = protocol7::VOTE_END_ABORT;

		if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_ClientID = -1;
	}

	if(ClientID == -1)
	{
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!m_apPlayers[i])
				continue;
			if(!Server()->IsSixup(i))
				Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, i);
			else
				Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if(!Server()->IsSixup(ClientID))
			Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(Server()->ClientIngame(i))
				SendVoteStatus(i, Total, Yes, No);
		return;
	}

	if(Total > VANILLA_MAX_CLIENTS && m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetClientVersion() <= VERSION_DDRACE)
	{
		Yes = float(Yes * VANILLA_MAX_CLIENTS) / float(Total);
		No = float(No * VANILLA_MAX_CLIENTS) / float(Total);
		Total = VANILLA_MAX_CLIENTS;
	}

	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ((str_startswith(m_aVoteCommand, "kick ") && str_toint(&m_aVoteCommand[5]) == ClientID) ||
				      (str_startswith(m_aVoteCommand, "set_team ") && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteEnforce = VOTE_ENFORCE_ABORT;
}

void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;

	if(str_comp(m_pController->m_pGameType, "DM") == 0 ||
		str_comp(m_pController->m_pGameType, "TDM") == 0 ||
		str_comp(m_pController->m_pGameType, "CTF") == 0)
	{
		CTuningParams p;
		if(mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int ClientID, int Zone)
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_apPlayers[i])
			{
				if(m_apPlayers[i]->GetCharacter())
				{
					if(m_apPlayers[i]->GetCharacter()->m_TuneZone == Zone)
						SendTuningParams(i, Zone);
				}
				else if(m_apPlayers[i]->m_TuneZone == Zone)
				{
					SendTuningParams(i, Zone);
				}
			}
		}
		return;
	}

	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = 0;
	if(Zone == 0)
		pParams = (int *)&m_Tuning;
	else
		pParams = (int *)&(m_aTuningList[Zone]);

	for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
	{
		if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetCharacter())
		{
			if((i == 30) // laser_damage is removed from 0.7
				&& (Server()->IsSixup(ClientID)))
			{
				continue;
			}
			else if((i == 31) // collision
				&& (m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOCOLL))
			{
				Msg.AddInt(0);
			}
			else if((i == 32) // hooking
				&& (m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHOOK))
			{
				Msg.AddInt(0);
			}
			else if((i == 3) // ground jump impulse
				&& m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOJUMP)
			{
				Msg.AddInt(0);
			}
			else if((i == 33) // jetpack
				&& !(m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_JETPACK))
			{
				Msg.AddInt(0);
			}
			else if((i == 36) // hammer hit
				&& m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHAMMER)
			{
				Msg.AddInt(0);
			}
			else
			{
				Msg.AddInt(pParams[i]);
			}
		}
		else
			Msg.AddInt(pParams[i]); // if everything is normal just send true tunings
	}
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnPreTickTeehistorian()
{
	if(!m_TeeHistorianActive)
		return;

	auto *pController = ((CGameControllerDDRace *)m_pController);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] != nullptr)
			m_TeeHistorian.RecordPlayerTeam(i, pController->m_Teams.m_Core.Team(i));
		else
			m_TeeHistorian.RecordPlayerTeam(i, 0);
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_TeeHistorian.RecordTeamPractice(i, pController->m_Teams.IsPractice(i));
	}
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	if(m_TeeHistorianActive)
	{
		int Error = aio_error(m_pTeeHistorianFile);
		if(Error)
		{
			dbg_msg("teehistorian", "error writing to file, err=%d", Error);
			Server()->SetErrorShutdown("teehistorian io error");
		}

		if(!m_TeeHistorian.Starting())
		{
			m_TeeHistorian.EndInputs();
			m_TeeHistorian.EndTick();
		}
		m_TeeHistorian.BeginTick(Server()->Tick());
		m_TeeHistorian.BeginPlayers();
	}

	// copy tuning
	m_World.m_Core.m_aTuning[0] = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			// send vote options
			ProgressVoteOptions(i);

			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
			pPlayer->PostPostTick();
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			bool Veto = false, VetoStop = false;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}}, *pIP = NULL;
				bool SinglePlayer = true;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(m_apPlayers[i])
					{
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
						if(!pIP)
							pIP = aaBuf[i];
						else if(SinglePlayer && str_comp(pIP, aaBuf[i]))
							SinglePlayer = false;
					}
				}

				// remember checked players, only the first player with a specific ip will be handled
				bool aVoteChecked[MAX_CLIENTS] = {false};
				int64_t Now = Server()->Tick();
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || aVoteChecked[i])
						continue;

					if((IsKickVote() || IsSpecVote()) && (m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||
										     (GetPlayerChar(m_VoteCreator) && GetPlayerChar(i) &&
											     GetPlayerChar(m_VoteCreator)->Team() != GetPlayerChar(i)->Team())))
						continue;

					if(m_apPlayers[i]->m_Afk && i != m_VoteCreator)
						continue;

					// can't vote in kick and spec votes in the beginning after joining
					if((IsKickVote() || IsSpecVote()) && Now < m_apPlayers[i]->m_FirstVoteTick)
						continue;

					// connecting clients with spoofed ips can clog slots without being ingame
					if(((CServer *)Server())->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
						continue;

					// don't count votes by blacklisted clients
					if(g_Config.m_SvDnsblVote && !m_pServer->DnsblWhite(i) && !SinglePlayer)
						continue;

					int CurVote = m_apPlayers[i]->m_Vote;
					int CurVotePos = m_apPlayers[i]->m_VotePos;

					// only allow IPs to vote once, but keep veto ability
					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i + 1; j < MAX_CLIENTS; j++)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]) != 0)
							continue;

						// count the latest vote by this ip
						if(CurVotePos < m_apPlayers[j]->m_VotePos)
						{
							CurVote = m_apPlayers[j]->m_Vote;
							CurVotePos = m_apPlayers[j]->m_VotePos;
						}

						aVoteChecked[j] = true;
					}

					Total++;
					if(CurVote > 0)
						Yes++;
					else if(CurVote < 0)
						No++;

					// veto right for players who have been active on server for long and who're not afk
					if(!IsKickVote() && !IsSpecVote() && g_Config.m_SvVoteVetoTime)
					{
						// look through all players with same IP again, including the current player
						for(int j = i; j < MAX_CLIENTS; j++)
						{
							// no need to check ip address of current player
							if(i != j && (!m_apPlayers[j] || str_comp(aaBuf[j], aaBuf[i]) != 0))
								continue;

							if(m_apPlayers[j] && !m_apPlayers[j]->m_Afk && m_apPlayers[j]->GetTeam() != TEAM_SPECTATORS &&
								((Server()->Tick() - m_apPlayers[j]->m_JoinTick) / (Server()->TickSpeed() * 60) > g_Config.m_SvVoteVetoTime ||
									(m_apPlayers[j]->GetCharacter() && m_apPlayers[j]->GetCharacter()->m_DDRaceState == DDRACE_STARTED &&
										(Server()->Tick() - m_apPlayers[j]->GetCharacter()->m_StartTime) / (Server()->TickSpeed() * 60) > g_Config.m_SvVoteVetoTime)))
							{
								if(CurVote == 0)
									Veto = true;
								else if(CurVote < 0)
									VetoStop = true;
								break;
							}
						}
					}
				}

				if(g_Config.m_SvVoteMaxTotal && Total > g_Config.m_SvVoteMaxTotal &&
					(IsKickVote() || IsSpecVote()))
					Total = g_Config.m_SvVoteMaxTotal;

				if((Yes > Total / (100.0f / g_Config.m_SvVoteYesPercentage)) && !Veto)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= Total - Total / (100.0f / g_Config.m_SvVoteYesPercentage))
					m_VoteEnforce = VOTE_ENFORCE_NO;

				if(VetoStop)
					m_VoteEnforce = VOTE_ENFORCE_NO;

				m_VoteWillPass = Yes > (Yes + No) / (100.0f / g_Config.m_SvVoteYesPercentage);
			}

			if(time_get() > m_VoteCloseTime && !g_Config.m_SvVoteMajority)
				m_VoteEnforce = (m_VoteWillPass && !Veto) ? VOTE_ENFORCE_YES : VOTE_ENFORCE_NO;

			// / Ensure minimum time for vote to end when moderating.
			if(m_VoteEnforce == VOTE_ENFORCE_YES && !(PlayerModerating() &&
									(IsKickVote() || IsSpecVote()) && time_get() < m_VoteCloseTime))
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote passed", -1, CHAT_SIX);

				if(m_apPlayers[m_VoteCreator] && !IsKickVote() && !IsSpecVote())
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Vote passed enforced by authorized player");
				Console()->ExecuteLine(m_aVoteCommand, m_VoteEnforcer);
				SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CHAT_SIX);
				EndVote();
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Vote failed enforced by authorized player");
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CHAT_SIX);
			}
			//else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (time_get() > m_VoteCloseTime && g_Config.m_SvVoteMajority))
			{
				EndVote();
				if(VetoStop || (m_VoteWillPass && Veto))
					SendChat(-1, CGameContext::CHAT_ALL, "Vote failed because of veto. Find an empty server instead", -1, CHAT_SIX);
				else
					SendChat(-1, CGameContext::CHAT_ALL, "Vote failed", -1, CHAT_SIX);
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(m_aMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumMutes--;
			m_aMutes[i] = m_aMutes[m_NumMutes];
		}
	}
	for(int i = 0; i < m_NumVoteMutes; i++)
	{
		if(m_aVoteMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumVoteMutes--;
			m_aVoteMutes[i] = m_aVoteMutes[m_NumVoteMutes];
		}
	}

	if(Server()->Tick() % (g_Config.m_SvAnnouncementInterval * Server()->TickSpeed() * 60) == 0)
	{
		const char *pLine = Server()->GetAnnouncementLine(g_Config.m_SvAnnouncementFileName);
		if(pLine)
			SendChat(-1, CGameContext::CHAT_ALL, pLine);
	}

	for(auto &Switcher : Switchers())
	{
		for(int j = 0; j < MAX_CLIENTS; ++j)
		{
			if(Switcher.m_aEndTick[j] <= Server()->Tick() && Switcher.m_aType[j] == TILE_SWITCHTIMEDOPEN)
			{
				Switcher.m_aStatus[j] = false;
				Switcher.m_aEndTick[j] = 0;
				Switcher.m_aType[j] = TILE_SWITCHCLOSE;
			}
			else if(Switcher.m_aEndTick[j] <= Server()->Tick() && Switcher.m_aType[j] == TILE_SWITCHTIMEDCLOSE)
			{
				Switcher.m_aStatus[j] = true;
				Switcher.m_aEndTick[j] = 0;
				Switcher.m_aType[j] = TILE_SWITCHOPEN;
			}
		}
	}

	if(m_SqlRandomMapResult != nullptr && m_SqlRandomMapResult->m_Completed)
	{
		if(m_SqlRandomMapResult->m_Success)
		{
			if(PlayerExists(m_SqlRandomMapResult->m_ClientID) && m_SqlRandomMapResult->m_aMessage[0] != '\0')
				SendChatTarget(m_SqlRandomMapResult->m_ClientID, m_SqlRandomMapResult->m_aMessage);
			if(m_SqlRandomMapResult->m_aMap[0] != '\0')
				Server()->ChangeMap(m_SqlRandomMapResult->m_aMap);
			else
				m_LastMapVote = 0;
		}
		m_SqlRandomMapResult = nullptr;
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i & 1) ? -1 : 1;
			m_apPlayers[MAX_CLIENTS - i - 1]->OnPredictedInput(&Input);
		}
	}
#endif

	// Record player position at the end of the tick
	if(m_TeeHistorianActive)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetCharacter())
			{
				CNetObj_CharacterCore Char;
				m_apPlayers[i]->GetCharacter()->GetCore().Write(&Char);
				m_TeeHistorian.RecordPlayer(i, &Char);
			}
			else
			{
				m_TeeHistorian.RecordDeadPlayer(i);
			}
		}
		m_TeeHistorian.EndPlayers();
		m_TeeHistorian.BeginInputs();
	}
	// Warning: do not put code in this function directly above or below this comment
}

static int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags & protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;
	if(Flags & protocol7::PLAYERFLAG_AIM)
		Six |= PLAYERFLAG_AIM;
	return Six;
}

// Server hooks
void CGameContext::OnClientPrepareInput(int ClientID, void *pInput)
{
	auto *pPlayerInput = (CNetObj_PlayerInput *)pInput;
	if(Server()->IsSixup(ClientID))
		pPlayerInput->m_PlayerFlags = PlayerFlags_SevenToSix(pPlayerInput->m_PlayerFlags);
}

void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);

	int Flags = ((CNetObj_PlayerInput *)pInput)->m_PlayerFlags;
	if((Flags & 256) || (Flags & 512))
	{
		Server()->Kick(ClientID, "please update your client or use DDNet client");
	}
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	// early return if no input at all has been sent by a player
	if(pInput == nullptr && !m_aPlayerHasInput[ClientID])
		return;

	// set to last sent input when no new input has been sent
	CNetObj_PlayerInput *pApplyInput = (CNetObj_PlayerInput *)pInput;
	if(pApplyInput == nullptr)
	{
		pApplyInput = &m_aLastPlayerInput[ClientID];
	}

	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput(pApplyInput);
}

void CGameContext::OnClientPredictedEarlyInput(int ClientID, void *pInput)
{
	// early return if no input at all has been sent by a player
	if(pInput == nullptr && !m_aPlayerHasInput[ClientID])
		return;

	// set to last sent input when no new input has been sent
	CNetObj_PlayerInput *pApplyInput = (CNetObj_PlayerInput *)pInput;
	if(pApplyInput == nullptr)
	{
		pApplyInput = &m_aLastPlayerInput[ClientID];
	}
	else
	{
		// Store input in this function and not in `OnClientPredictedInput`,
		// because this function is called on all inputs, while
		// `OnClientPredictedInput` is only called on the first input of each
		// tick.
		mem_copy(&m_aLastPlayerInput[ClientID], pApplyInput, sizeof(m_aLastPlayerInput[ClientID]));
		m_aPlayerHasInput[ClientID] = true;
	}

	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedEarlyInput(pApplyInput);

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerInput(ClientID, m_apPlayers[ClientID]->GetUniqueCID(), pApplyInput);
	}
}

struct CVoteOptionServer *CGameContext::GetVoteOption(int Index)
{
	CVoteOptionServer *pCurrent;
	for(pCurrent = m_pVoteOptionFirst;
		Index > 0 && pCurrent;
		Index--, pCurrent = pCurrent->m_pNext)
		;

	if(Index > 0)
		return 0;
	return pCurrent;
}

void CGameContext::ProgressVoteOptions(int ClientID)
{
	CPlayer *pPl = m_apPlayers[ClientID];

	if(pPl->m_SendVoteIndex == -1)
		return; // we didn't start sending options yet

	if(pPl->m_SendVoteIndex > m_NumVoteOptions)
		return; // shouldn't happen / fail silently

	int VotesLeft = m_NumVoteOptions - pPl->m_SendVoteIndex;
	int NumVotesToSend = minimum(g_Config.m_SvSendVotesPerTick, VotesLeft);

	if(!VotesLeft)
	{
		// player has up to date vote option list
		return;
	}

	// build vote option list msg
	int CurIndex = 0;

	CNetMsg_Sv_VoteOptionListAdd OptionMsg;
	OptionMsg.m_pDescription0 = "";
	OptionMsg.m_pDescription1 = "";
	OptionMsg.m_pDescription2 = "";
	OptionMsg.m_pDescription3 = "";
	OptionMsg.m_pDescription4 = "";
	OptionMsg.m_pDescription5 = "";
	OptionMsg.m_pDescription6 = "";
	OptionMsg.m_pDescription7 = "";
	OptionMsg.m_pDescription8 = "";
	OptionMsg.m_pDescription9 = "";
	OptionMsg.m_pDescription10 = "";
	OptionMsg.m_pDescription11 = "";
	OptionMsg.m_pDescription12 = "";
	OptionMsg.m_pDescription13 = "";
	OptionMsg.m_pDescription14 = "";

	// get current vote option by index
	CVoteOptionServer *pCurrent = GetVoteOption(pPl->m_SendVoteIndex);

	while(CurIndex < NumVotesToSend && pCurrent != NULL)
	{
		switch(CurIndex)
		{
		case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
		case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
		case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
		case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
		case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
		case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
		case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
		case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
		case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
		case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
		case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
		case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
		case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
		case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
		case 14: OptionMsg.m_pDescription14 = pCurrent->m_aDescription; break;
		}

		CurIndex++;
		pCurrent = pCurrent->m_pNext;
	}

	// send msg
	OptionMsg.m_NumOptions = NumVotesToSend;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);

	pPl->m_SendVoteIndex += NumVotesToSend;
}

void CGameContext::OnClientEnter(int ClientID)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerReady(ClientID);
	}
	m_pController->OnPlayerConnect(m_apPlayers[ClientID]);

	if(Server()->IsSixup(ClientID))
	{
		{
			protocol7::CNetMsg_Sv_GameInfo Msg;
			Msg.m_GameFlags = protocol7::GAMEFLAG_RACE;
			Msg.m_MatchCurrent = 1;
			Msg.m_MatchNum = 0;
			Msg.m_ScoreLimit = 0;
			Msg.m_TimeLimit = 0;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}

		// /team is essential
		{
			protocol7::CNetMsg_Sv_CommandInfoRemove Msg;
			Msg.m_pName = "team";
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}

		for(const IConsole::CCommandInfo *pCmd = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT);
			pCmd; pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT))
		{
			if(!str_comp_nocase(pCmd->m_pName, "w") || !str_comp_nocase(pCmd->m_pName, "whisper"))
				continue;

			const char *pName = pCmd->m_pName;
			if(!str_comp_nocase(pCmd->m_pName, "r"))
				pName = "rescue";

			protocol7::CNetMsg_Sv_CommandInfo Msg;
			Msg.m_pName = pName;
			Msg.m_pArgsFormat = pCmd->m_pParams;
			Msg.m_pHelpText = pCmd->m_pHelp;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}

	{
		int Empty = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!Server()->ClientIngame(i))
			{
				Empty = i;
				break;
			}
		}
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = Empty;
		Msg.m_pMessage = "Do you know someone who uses a bot? Please report them to the moderators.";
		m_apPlayers[ClientID]->m_EligibleForFinishCheck = time_get();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}

	IServer::CClientInfo Info;
	if(Server()->GetClientInfo(ClientID, &Info) && Info.m_GotDDNetVersion)
	{
		if(OnClientDDNetVersionKnown(ClientID))
			return; // kicked
	}

	if(!Server()->ClientPrevIngame(ClientID))
	{
		if(g_Config.m_SvWelcome[0] != 0)
			SendChatTarget(ClientID, g_Config.m_SvWelcome);

		if(g_Config.m_SvShowOthersDefault > SHOW_OTHERS_OFF)
		{
			if(g_Config.m_SvShowOthers)
				SendChatTarget(ClientID, "You can see other players. To disable this use DDNet client and type /showothers");

			m_apPlayers[ClientID]->m_ShowOthers = g_Config.m_SvShowOthersDefault;
		}
	}
	m_VoteUpdate = true;

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	Server()->ExpireServerInfo();

	CPlayer *pNewPlayer = m_apPlayers[ClientID];
	mem_zero(&m_aLastPlayerInput[ClientID], sizeof(m_aLastPlayerInput[ClientID]));
	m_aPlayerHasInput[ClientID] = false;

	// new info for others
	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = pNewPlayer->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientID);
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
	NewClientInfoMsg.m_Silent = false;

	for(int p = 0; p < 6; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = pNewPlayer->m_TeeInfos.m_apSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = pNewPlayer->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = pNewPlayer->m_TeeInfos.m_aSkinPartColors[p];
	}

	// update client infos (others before local)
	for(int i = 0; i < Server()->MaxClients(); ++i)
	{
		if(i == ClientID || !m_apPlayers[i] || !Server()->ClientIngame(i))
			continue;

		CPlayer *pPlayer = m_apPlayers[i];

		if(Server()->IsSixup(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);

		if(Server()->IsSixup(ClientID))
		{
			// existing infos for new player
			protocol7::CNetMsg_Sv_ClientInfo ClientInfoMsg;
			ClientInfoMsg.m_ClientID = i;
			ClientInfoMsg.m_Local = 0;
			ClientInfoMsg.m_Team = pPlayer->GetTeam();
			ClientInfoMsg.m_pName = Server()->ClientName(i);
			ClientInfoMsg.m_pClan = Server()->ClientClan(i);
			ClientInfoMsg.m_Country = Server()->ClientCountry(i);
			ClientInfoMsg.m_Silent = 0;

			for(int p = 0; p < 6; p++)
			{
				ClientInfoMsg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
				ClientInfoMsg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				ClientInfoMsg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
			}

			Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}

	// local info
	if(Server()->IsSixup(ClientID))
	{
		NewClientInfoMsg.m_Local = 1;
		Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}

	// initial chat delay
	if(g_Config.m_SvChatInitialDelay != 0 && m_apPlayers[ClientID]->m_JoinTick > m_NonEmptySince + 10 * Server()->TickSpeed())
	{
		NETADDR Addr;
		Server()->GetClientAddr(ClientID, &Addr);
		Mute(&Addr, g_Config.m_SvChatInitialDelay, Server()->ClientName(ClientID), "Initial chat delay", true);
	}
}

bool CGameContext::OnClientDataPersist(int ClientID, void *pData)
{
	CPersistentClientData *pPersistent = (CPersistentClientData *)pData;
	if(!m_apPlayers[ClientID])
	{
		return false;
	}
	pPersistent->m_IsSpectator = m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS;
	return true;
}

void CGameContext::OnClientConnected(int ClientID, void *pData)
{
	CPersistentClientData *pPersistentData = (CPersistentClientData *)pData;
	bool Spec = false;
	if(pPersistentData)
	{
		Spec = pPersistentData->m_IsSpectator;
	}

	{
		bool Empty = true;
		for(auto &pPlayer : m_apPlayers)
		{
			if(pPlayer)
			{
				Empty = false;
				break;
			}
		}
		if(Empty)
		{
			m_NonEmptySince = Server()->Tick();
		}
	}

	// Check which team the player should be on
	const int StartTeam = (Spec || g_Config.m_SvTournamentMode) ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	if(m_apPlayers[ClientID])
		delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, NextUniqueClientID, ClientID, StartTeam);
	NextUniqueClientID += 1;

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
			return;
	}
#endif

	SendMotd(ClientID);
	SendSettings(ClientID);

	Server()->ExpireServerInfo();
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	AbortVoteKickOnDisconnect(ClientID);
	m_pController->OnPlayerDisconnect(m_apPlayers[ClientID], pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	//(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_SpectatorID == ClientID)
			pPlayer->m_SpectatorID = SPEC_FREEVIEW;
	}

	// update conversation targets
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_LastWhisperTo == ClientID)
			pPlayer->m_LastWhisperTo = -1;
	}

	protocol7::CNetMsg_Sv_ClientDrop Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_pReason = pReason;
	Msg.m_Silent = false;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	Server()->ExpireServerInfo();
}

void CGameContext::OnClientEngineJoin(int ClientID, bool Sixup)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerJoin(ClientID, !Sixup ? CTeeHistorian::PROTOCOL_6 : CTeeHistorian::PROTOCOL_7);
	}
}

void CGameContext::OnClientEngineDrop(int ClientID, const char *pReason)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerDrop(ClientID, pReason);
	}
}

bool CGameContext::OnClientDDNetVersionKnown(int ClientID)
{
	IServer::CClientInfo Info;
	dbg_assert(Server()->GetClientInfo(ClientID, &Info), "failed to get client info");
	int ClientVersion = Info.m_DDNetVersion;
	dbg_msg("ddnet", "cid=%d version=%d", ClientID, ClientVersion);

	if(m_TeeHistorianActive)
	{
		if(Info.m_pConnectionID && Info.m_pDDNetVersionStr)
		{
			m_TeeHistorian.RecordDDNetVersion(ClientID, *Info.m_pConnectionID, ClientVersion, Info.m_pDDNetVersionStr);
		}
		else
		{
			m_TeeHistorian.RecordDDNetVersionOld(ClientID, ClientVersion);
		}
	}

	// Autoban known bot versions.
	if(g_Config.m_SvBannedVersions[0] != '\0' && IsVersionBanned(ClientVersion))
	{
		Server()->Kick(ClientID, "unsupported client");
		return true;
	}

	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(ClientVersion >= VERSION_DDNET_GAMETICK)
		pPlayer->m_TimerType = g_Config.m_SvDefaultTimerType;

	// First update the teams state.
	((CGameControllerDDRace *)m_pController)->m_Teams.SendTeamsState(ClientID);

	// Then send records.
	SendRecord(ClientID);

	// And report correct tunings.
	if(ClientVersion < VERSION_DDNET_EARLY_VERSION)
		SendTuningParams(ClientID, pPlayer->m_TuneZone);

	// Tell old clients to update.
	if(ClientVersion < VERSION_DDNET_UPDATER_FIXED && g_Config.m_SvClientSuggestionOld[0] != '\0')
		SendBroadcast(g_Config.m_SvClientSuggestionOld, ClientID);
	// Tell known bot clients that they're botting and we know it.
	if(((ClientVersion >= 15 && ClientVersion < 100) || ClientVersion == 502) && g_Config.m_SvClientSuggestionBot[0] != '\0')
		SendBroadcast(g_Config.m_SvClientSuggestionBot, ClientID);

	return false;
}

void *CGameContext::PreProcessMsg(int *pMsgID, CUnpacker *pUnpacker, int ClientID)
{
	if(Server()->IsSixup(ClientID) && *pMsgID < OFFSET_UUID)
	{
		void *pRawMsg = m_NetObjHandler7.SecureUnpackMsg(*pMsgID, pUnpacker);
		if(!pRawMsg)
			return 0;

		CPlayer *pPlayer = m_apPlayers[ClientID];
		static char s_aRawMsg[1024];

		if(*pMsgID == protocol7::NETMSGTYPE_CL_SAY)
		{
			protocol7::CNetMsg_Cl_Say *pMsg7 = (protocol7::CNetMsg_Cl_Say *)pRawMsg;
			// Should probably use a placement new to start the lifetime of the object to avoid future weirdness
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			if(pMsg7->m_Target >= 0)
			{
				if(ProcessSpamProtection(ClientID))
					return 0;

				// Should we maybe recraft the message so that it can go through the usual path?
				WhisperID(ClientID, pMsg7->m_Target, pMsg7->m_pMessage);
				return 0;
			}

			pMsg->m_Team = pMsg7->m_Mode == protocol7::CHAT_TEAM;
			pMsg->m_pMessage = pMsg7->m_pMessage;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_STARTINFO)
		{
			protocol7::CNetMsg_Cl_StartInfo *pMsg7 = (protocol7::CNetMsg_Cl_StartInfo *)pRawMsg;
			::CNetMsg_Cl_StartInfo *pMsg = (::CNetMsg_Cl_StartInfo *)s_aRawMsg;

			pMsg->m_pName = pMsg7->m_pName;
			pMsg->m_pClan = pMsg7->m_pClan;
			pMsg->m_Country = pMsg7->m_Country;

			CTeeInfo Info(pMsg7->m_apSkinPartNames, pMsg7->m_aUseCustomColors, pMsg7->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			str_copy(s_aRawMsg + sizeof(*pMsg), Info.m_aSkinName, sizeof(s_aRawMsg) - sizeof(*pMsg));

			pMsg->m_pSkin = s_aRawMsg + sizeof(*pMsg);
			pMsg->m_UseCustomColor = pPlayer->m_TeeInfos.m_UseCustomColor;
			pMsg->m_ColorBody = pPlayer->m_TeeInfos.m_ColorBody;
			pMsg->m_ColorFeet = pPlayer->m_TeeInfos.m_ColorFeet;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SKINCHANGE)
		{
			protocol7::CNetMsg_Cl_SkinChange *pMsg = (protocol7::CNetMsg_Cl_SkinChange *)pRawMsg;
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo &&
				pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
				return 0;

			pPlayer->m_LastChangeInfo = Server()->Tick();

			CTeeInfo Info(pMsg->m_apSkinPartNames, pMsg->m_aUseCustomColors, pMsg->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			protocol7::CNetMsg_Sv_SkinChange Msg;
			Msg.m_ClientID = ClientID;
			for(int p = 0; p < 6; p++)
			{
				Msg.m_apSkinPartNames[p] = pMsg->m_apSkinPartNames[p];
				Msg.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
				Msg.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
			}

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

			return 0;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			protocol7::CNetMsg_Cl_SetSpectatorMode *pMsg7 = (protocol7::CNetMsg_Cl_SetSpectatorMode *)pRawMsg;
			::CNetMsg_Cl_SetSpectatorMode *pMsg = (::CNetMsg_Cl_SetSpectatorMode *)s_aRawMsg;

			if(pMsg7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				pMsg->m_SpectatorID = SPEC_FREEVIEW;
			else if(pMsg7->m_SpecMode == protocol7::SPEC_PLAYER)
				pMsg->m_SpectatorID = pMsg7->m_SpectatorID;
			else
				pMsg->m_SpectatorID = SPEC_FREEVIEW; // Probably not needed
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SETTEAM)
		{
			protocol7::CNetMsg_Cl_SetTeam *pMsg7 = (protocol7::CNetMsg_Cl_SetTeam *)pRawMsg;
			::CNetMsg_Cl_SetTeam *pMsg = (::CNetMsg_Cl_SetTeam *)s_aRawMsg;

			pMsg->m_Team = pMsg7->m_Team;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_COMMAND)
		{
			protocol7::CNetMsg_Cl_Command *pMsg7 = (protocol7::CNetMsg_Cl_Command *)pRawMsg;
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			str_format(s_aRawMsg + sizeof(*pMsg), sizeof(s_aRawMsg) - sizeof(*pMsg), "/%s %s", pMsg7->m_pName, pMsg7->m_pArguments);
			pMsg->m_pMessage = s_aRawMsg + sizeof(*pMsg);
			dbg_msg("debug", "line='%s'", s_aRawMsg + sizeof(*pMsg));
			pMsg->m_Team = 0;

			*pMsgID = NETMSGTYPE_CL_SAY;
			return s_aRawMsg;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_CALLVOTE)
		{
			protocol7::CNetMsg_Cl_CallVote *pMsg7 = (protocol7::CNetMsg_Cl_CallVote *)pRawMsg;
			::CNetMsg_Cl_CallVote *pMsg = (::CNetMsg_Cl_CallVote *)s_aRawMsg;

			int Authed = Server()->GetAuthedState(ClientID);
			if(pMsg7->m_Force)
			{
				str_format(s_aRawMsg, sizeof(s_aRawMsg), "force_vote \"%s\" \"%s\" \"%s\"", pMsg7->m_pType, pMsg7->m_pValue, pMsg7->m_pReason);
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
				Console()->ExecuteLine(s_aRawMsg, ClientID, false);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				return 0;
			}

			pMsg->m_pValue = pMsg7->m_pValue;
			pMsg->m_pReason = pMsg7->m_pReason;
			pMsg->m_pType = pMsg7->m_pType;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_EMOTICON)
		{
			protocol7::CNetMsg_Cl_Emoticon *pMsg7 = (protocol7::CNetMsg_Cl_Emoticon *)pRawMsg;
			::CNetMsg_Cl_Emoticon *pMsg = (::CNetMsg_Cl_Emoticon *)s_aRawMsg;

			pMsg->m_Emoticon = pMsg7->m_Emoticon;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_VOTE)
		{
			protocol7::CNetMsg_Cl_Vote *pMsg7 = (protocol7::CNetMsg_Cl_Vote *)pRawMsg;
			::CNetMsg_Cl_Vote *pMsg = (::CNetMsg_Cl_Vote *)s_aRawMsg;

			pMsg->m_Vote = pMsg7->m_Vote;
		}

		*pMsgID = Msg_SevenToSix(*pMsgID);

		return s_aRawMsg;
	}
	else
		return m_NetObjHandler.SecureUnpackMsg(*pMsgID, pUnpacker);
}

void CGameContext::CensorMessage(char *pCensoredMessage, const char *pMessage, int Size)
{
	str_copy(pCensoredMessage, pMessage, Size);

	for(auto &Item : m_vCensorlist)
	{
		char *pCurLoc = pCensoredMessage;
		do
		{
			pCurLoc = (char *)str_utf8_find_nocase(pCurLoc, Item.c_str());
			if(pCurLoc)
			{
				for(int i = 0; i < (int)Item.length(); i++)
				{
					pCurLoc[i] = '*';
				}
				pCurLoc++;
			}
		} while(pCurLoc);
	}
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	if(m_TeeHistorianActive)
	{
		if(m_NetObjHandler.TeeHistorianRecordMsg(MsgID))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}

	void *pRawMsg = PreProcessMsg(&MsgID, pUnpacker, ClientID);

	if(!pRawMsg)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pMessage))
			{
				return;
			}
			bool Check = !pPlayer->m_NotEligibleForFinish && pPlayer->m_EligibleForFinishCheck + 10 * time_freq() >= time_get();
			if(Check && str_comp(pMsg->m_pMessage, "xd sure chillerbot.png is lyfe") == 0 && pMsg->m_Team == 0)
			{
				if(m_TeeHistorianActive)
				{
					m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
				}

				pPlayer->m_NotEligibleForFinish = true;
				dbg_msg("hack", "bot detected, cid=%d", ClientID);
				return;
			}
			int Team = pMsg->m_Team;

			// trim right and set maximum length to 256 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(!str_utf8_isspace(Code))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 256)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 32 characters per second)
			if(Length == 0 || (pMsg->m_pMessage[0] != '/' && (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() * ((31 + Length) / 32) > Server()->Tick())))
				return;

			int GameTeam = ((CGameControllerDDRace *)m_pController)->m_Teams.m_Core.Team(pPlayer->GetCID());
			if(Team)
				Team = ((pPlayer->GetTeam() == TEAM_SPECTATORS) ? CHAT_SPEC : GameTeam);
			else
				Team = CHAT_ALL;

			if(pMsg->m_pMessage[0] == '/')
			{
				if(str_startswith_nocase(pMsg->m_pMessage + 1, "w "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 3, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_startswith_nocase(pMsg->m_pMessage + 1, "whisper "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 9, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_startswith_nocase(pMsg->m_pMessage + 1, "c "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 3, 256);
					Converse(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_startswith_nocase(pMsg->m_pMessage + 1, "converse "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 10, 256);
					Converse(pPlayer->GetCID(), aWhisperMsg);
				}
				else
				{
					if(g_Config.m_SvSpamprotection && !str_startswith(pMsg->m_pMessage + 1, "timeout ") && pPlayer->m_aLastCommands[0] && pPlayer->m_aLastCommands[0] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[1] && pPlayer->m_aLastCommands[1] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[2] && pPlayer->m_aLastCommands[2] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[3] && pPlayer->m_aLastCommands[3] + Server()->TickSpeed() > Server()->Tick())
						return;

					int64_t Now = Server()->Tick();
					pPlayer->m_aLastCommands[pPlayer->m_LastCommandPos] = Now;
					pPlayer->m_LastCommandPos = (pPlayer->m_LastCommandPos + 1) % 4;

					Console()->SetFlagMask(CFGFLAG_CHAT);
					int Authed = Server()->GetAuthedState(ClientID);
					if(Authed)
						Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
					else
						Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);

					{
						CClientChatLogger Logger(this, ClientID, log_get_scope_logger());
						CLogScope Scope(&Logger);
						Console()->ExecuteLine(pMsg->m_pMessage + 1, ClientID, false);
					}
					// m_apPlayers[ClientID] can be NULL, if the player used a
					// timeout code and replaced another client.
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, pMsg->m_pMessage);
					Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "chat-command", aBuf);

					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
					Console()->SetFlagMask(CFGFLAG_SERVER);
				}
			}
			else
			{
				pPlayer->UpdatePlaytime();
				char aCensoredMessage[256];
				CensorMessage(aCensoredMessage, pMsg->m_pMessage, sizeof(aCensoredMessage));
				SendChat(ClientID, Team, aCensoredMessage, ClientID);
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(RateLimitPlayerVote(ClientID) || m_VoteCloseTime)
				return;

			m_apPlayers[ClientID]->UpdatePlaytime();

			m_VoteType = VOTE_TYPE_UNKNOWN;
			char aChatmsg[512] = {0};
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aSixupDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			char aReason[VOTE_REASON_LENGTH] = "No reason given";
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pType) || !str_utf8_check(pMsg->m_pReason) || !str_utf8_check(pMsg->m_pValue))
			{
				return;
			}
			if(pMsg->m_pReason[0])
			{
				str_copy(aReason, pMsg->m_pReason, sizeof(aReason));
			}

			if(str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				CVoteOptionServer *pOption = m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_pValue, pOption->m_aDescription) == 0)
					{
						if(!Console()->LineIsValid(pOption->m_aCommand))
						{
							SendChatTarget(ClientID, "Invalid option");
							return;
						}
						if((str_find(pOption->m_aCommand, "sv_map ") != 0 || str_find(pOption->m_aCommand, "change_map ") != 0 || str_find(pOption->m_aCommand, "random_map") != 0 || str_find(pOption->m_aCommand, "random_unfinished_map") != 0) && RateLimitPlayerMapVote(ClientID))
						{
							return;
						}

						str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
							pOption->m_aDescription, aReason);
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);

						if((str_endswith(pOption->m_aCommand, "random_map") || str_endswith(pOption->m_aCommand, "random_unfinished_map")) && str_length(aReason) == 1 && aReason[0] >= '0' && aReason[0] <= '5')
						{
							int Stars = aReason[0] - '0';
							str_format(aCmd, sizeof(aCmd), "%s %d", pOption->m_aCommand, Stars);
						}
						else
						{
							str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						}

						m_LastMapVote = time_get();
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
				{
					if(Authed != AUTHED_ADMIN) // allow admins to call any vote they want
					{
						str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_pValue);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
					else
					{
						str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientID), pMsg->m_pValue);
						str_format(aDesc, sizeof(aDesc), "%s", pMsg->m_pValue);
						str_format(aCmd, sizeof(aCmd), "%s", pMsg->m_pValue);
					}
				}

				m_VoteType = VOTE_TYPE_OPTION;
			}
			else if(str_comp_nocase(pMsg->m_pType, "kick") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				if(!Authed && time_get() < m_apPlayers[ClientID]->m_Last_KickVote + (time_freq() * 5))
					return;
				else if(!Authed && time_get() < m_apPlayers[ClientID]->m_Last_KickVote + (time_freq() * g_Config.m_SvVoteKickDelay))
				{
					str_format(aChatmsg, sizeof(aChatmsg), "There's a %d second wait time between kick votes for each player please wait %d second(s)",
						g_Config.m_SvVoteKickDelay,
						(int)(((m_apPlayers[ClientID]->m_Last_KickVote + (m_apPlayers[ClientID]->m_Last_KickVote * time_freq())) / time_freq()) - (time_get() / time_freq())));
					SendChatTarget(ClientID, aChatmsg);
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}
				//else if(!g_Config.m_SvVoteKick)
				else if(!g_Config.m_SvVoteKick && !Authed) // allow admins to call kick votes even if they are forbidden
				{
					SendChatTarget(ClientID, "Server does not allow voting to kick players");
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}

				if(g_Config.m_SvVoteKickMin && !GetDDRaceTeam(ClientID))
				{
					char aaAddresses[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(m_apPlayers[i])
						{
							Server()->GetClientAddr(i, aaAddresses[i], NETADDR_MAXSTRSIZE);
						}
					}
					int NumPlayers = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
					{
						if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(i))
						{
							NumPlayers++;
							for(int j = 0; j < i; j++)
							{
								if(m_apPlayers[j] && m_apPlayers[j]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(j))
								{
									if(str_comp(aaAddresses[i], aaAddresses[j]) == 0)
									{
										NumPlayers--;
										break;
									}
								}
							}
						}
					}

					if(NumPlayers < g_Config.m_SvVoteKickMin)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players", g_Config.m_SvVoteKickMin);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_pValue);

				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if(KickID == ClientID)
				{
					SendChatTarget(ClientID, "You can't kick yourself");
					return;
				}
				if(!Server()->ReverseTranslate(KickID, ClientID))
				{
					return;
				}
				int KickedAuthed = Server()->GetAuthedState(KickID);
				if(KickedAuthed > Authed)
				{
					SendChatTarget(ClientID, "You can't kick authorized players");
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					char aBufKick[128];
					str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you", Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				// Don't allow kicking if a player has no character
				if(!GetPlayerChar(ClientID) || !GetPlayerChar(KickID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(KickID))
				{
					SendChatTarget(ClientID, "You can kick only your team member");
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), aReason);
				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", KickID, Server()->ClientName(KickID));
				if(!GetDDRaceTeam(ClientID))
				{
					if(!g_Config.m_SvVoteKickBantime)
					{
						str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
						str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
					}
					else
					{
						char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
						Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
						str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
						str_format(aDesc, sizeof(aDesc), "Ban '%s'", Server()->ClientName(KickID));
					}
				}
				else
				{
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team_ddr %d 0", KickID, GetDDRaceTeam(KickID), KickID);
					str_format(aDesc, sizeof(aDesc), "Move '%s' to team 0", Server()->ClientName(KickID));
				}
				m_apPlayers[ClientID]->m_Last_KickVote = time_get();
				m_VoteType = VOTE_TYPE_KICK;
				m_VoteVictim = KickID;
			}
			else if(str_comp_nocase(pMsg->m_pType, "spectate") == 0)
			{
				if(!g_Config.m_SvVoteSpectate)
				{
					SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
					return;
				}

				int SpectateID = str_toint(pMsg->m_pValue);

				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget(ClientID, "Invalid client id to move");
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatTarget(ClientID, "You can't move yourself");
					return;
				}
				if(!Server()->ReverseTranslate(SpectateID, ClientID))
				{
					return;
				}

				if(!GetPlayerChar(ClientID) || !GetPlayerChar(SpectateID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(SpectateID))
				{
					SendChatTarget(ClientID, "You can only move your team member to spectators");
					return;
				}

				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				if(g_Config.m_SvPauseable && g_Config.m_SvVotePause)
				{
					str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to pause '%s' for %d seconds (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), g_Config.m_SvVotePauseTime, aReason);
					str_format(aDesc, sizeof(aDesc), "Pause '%s' (%ds)", Server()->ClientName(SpectateID), g_Config.m_SvVotePauseTime);
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; force_pause %d %d", SpectateID, GetDDRaceTeam(SpectateID), SpectateID, g_Config.m_SvVotePauseTime);
				}
				else
				{
					str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), aReason);
					str_format(aDesc, sizeof(aDesc), "Move '%s' to spectators", Server()->ClientName(SpectateID));
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team %d -1 %d", SpectateID, GetDDRaceTeam(SpectateID), SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
				}
				m_VoteType = VOTE_TYPE_SPECTATE;
				m_VoteVictim = SpectateID;
			}

			if(aCmd[0] && str_comp_nocase(aCmd, "info") != 0)
				CallVote(ClientID, aDesc, aCmd, aReason, aChatmsg, aSixupDesc[0] ? aSixupDesc : 0);
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(!m_VoteCloseTime)
				return;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 3 > Server()->Tick())
				return;

			int64_t Now = Server()->Tick();

			pPlayer->m_LastVoteTry = Now;
			pPlayer->UpdatePlaytime();

			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			if(!pMsg->m_Vote)
				return;

			pPlayer->m_Vote = pMsg->m_Vote;
			pPlayer->m_VotePos = ++m_VotePos;
			m_VoteUpdate = true;
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if(pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay > Server()->Tick()))
				return;

			//Kill Protection
			CCharacter *pChr = pPlayer->GetCharacter();
			if(pChr)
			{
				int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
				if(g_Config.m_SvKillProtection != 0 && CurrTime >= (60 * g_Config.m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
				{
					SendChatTarget(ClientID, "Kill Protection enabled. If you really want to join the spectators, first type /kill");
					return;
				}
			}

			if(pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick()) / Server()->TickSpeed();
				char aTime[32];
				str_time((int64_t)TimeLeft * 100, TIME_HOURS, aTime, sizeof(aTime));
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %s", aTime);
				SendBroadcast(aBuf, ClientID);
				return;
			}

			// Switch team on given client and kill/respawn them
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if(pPlayer->IsPaused())
					SendChatTarget(ClientID, "Use /pause first then you can kill");
				else
				{
					if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					m_pController->DoTeamChange(pPlayer, pMsg->m_Team);
					pPlayer->m_TeamChangeTick = Server()->Tick();
				}
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Server()->MaxClients() - g_Config.m_SvSpectatorSlots);
				SendBroadcast(aBuf, ClientID);
			}
		}
		else if(MsgID == NETMSGTYPE_CL_ISDDNETLEGACY)
		{
			IServer::CClientInfo Info;
			if(Server()->GetClientInfo(ClientID, &Info) && Info.m_GotDDNetVersion)
			{
				return;
			}
			int DDNetVersion = pUnpacker->GetInt();
			if(pUnpacker->Error() || DDNetVersion < 0)
			{
				DDNetVersion = VERSION_DDRACE;
			}
			Server()->SetClientDDNetVersion(ClientID, DDNetVersion);
			OnClientDDNetVersionKnown(ClientID);
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWOTHERSLEGACY)
		{
			if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
			{
				CNetMsg_Cl_ShowOthersLegacy *pMsg = (CNetMsg_Cl_ShowOthersLegacy *)pRawMsg;
				pPlayer->m_ShowOthers = pMsg->m_Show;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWOTHERS)
		{
			if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
			{
				CNetMsg_Cl_ShowOthers *pMsg = (CNetMsg_Cl_ShowOthers *)pRawMsg;
				pPlayer->m_ShowOthers = pMsg->m_Show;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			CNetMsg_Cl_ShowDistance *pMsg = (CNetMsg_Cl_ShowDistance *)pRawMsg;
			pPlayer->m_ShowDistance = vec2(pMsg->m_X, pMsg->m_Y);
		}
		else if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			pMsg->m_SpectatorID = clamp(pMsg->m_SpectatorID, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);

			if(pMsg->m_SpectatorID >= 0)
				if(!Server()->ReverseTranslate(pMsg->m_SpectatorID, ClientID))
					return;

			if((g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() / 4 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			pPlayer->UpdatePlaytime();
			if(pMsg->m_SpectatorID >= 0 && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
				SendChatTarget(ClientID, "Invalid spectator id used");
			else
				pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
		}
		else if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
				return;

			bool SixupNeedsUpdate = false;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pName) || !str_utf8_check(pMsg->m_pClan) || !str_utf8_check(pMsg->m_pSkin))
			{
				return;
			}
			pPlayer->m_LastChangeInfo = Server()->Tick();
			pPlayer->UpdatePlaytime();

			// set infos
			if(Server()->WouldClientNameChange(ClientID, pMsg->m_pName) && !ProcessSpamProtection(ClientID))
			{
				char aOldName[MAX_NAME_LENGTH];
				str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));

				Server()->SetClientName(ClientID, pMsg->m_pName);

				char aChatText[256];
				str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
				SendChat(-1, CGameContext::CHAT_ALL, aChatText);

				// reload scores
				Score()->PlayerData(ClientID)->Reset();
				m_apPlayers[ClientID]->m_Score = -9999;
				Score()->LoadPlayerData(ClientID);

				SixupNeedsUpdate = true;
			}

			if(str_comp(Server()->ClientClan(ClientID), pMsg->m_pClan))
				SixupNeedsUpdate = true;
			Server()->SetClientClan(ClientID, pMsg->m_pClan);

			if(Server()->ClientCountry(ClientID) != pMsg->m_Country)
				SixupNeedsUpdate = true;
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			str_copy(pPlayer->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_aSkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			if(!Server()->IsSixup(ClientID))
				pPlayer->m_TeeInfos.ToSixup();

			if(SixupNeedsUpdate)
			{
				protocol7::CNetMsg_Sv_ClientDrop Drop;
				Drop.m_ClientID = ClientID;
				Drop.m_pReason = "";
				Drop.m_Silent = true;

				protocol7::CNetMsg_Sv_ClientInfo Info;
				Info.m_ClientID = ClientID;
				Info.m_pName = Server()->ClientName(ClientID);
				Info.m_Country = pMsg->m_Country;
				Info.m_pClan = pMsg->m_pClan;
				Info.m_Local = 0;
				Info.m_Silent = true;
				Info.m_Team = pPlayer->GetTeam();

				for(int p = 0; p < 6; p++)
				{
					Info.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
					Info.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
					Info.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				}

				for(int i = 0; i < Server()->MaxClients(); i++)
				{
					if(i != ClientID)
					{
						Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
						Server()->SendPackMsg(&Info, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
			}
			else
			{
				protocol7::CNetMsg_Sv_SkinChange Msg;
				Msg.m_ClientID = ClientID;
				for(int p = 0; p < 6; p++)
				{
					Msg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
					Msg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
					Msg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				}

				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
			}

			Server()->ExpireServerInfo();
		}
		else if(MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote + Server()->TickSpeed() * g_Config.m_SvEmoticonDelay > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();
			pPlayer->UpdatePlaytime();

			SendEmoticon(ClientID, pMsg->m_Emoticon);
			CCharacter *pChr = pPlayer->GetCharacter();
			if(pChr && g_Config.m_SvEmotionalTees && pPlayer->m_EyeEmoteEnabled)
			{
				int EmoteType = EMOTE_NORMAL;
				switch(pMsg->m_Emoticon)
				{
				case EMOTICON_EXCLAMATION:
				case EMOTICON_GHOST:
				case EMOTICON_QUESTION:
				case EMOTICON_WTF:
					EmoteType = EMOTE_SURPRISE;
					break;
				case EMOTICON_DOTDOT:
				case EMOTICON_DROP:
				case EMOTICON_ZZZ:
					EmoteType = EMOTE_BLINK;
					break;
				case EMOTICON_EYES:
				case EMOTICON_HEARTS:
				case EMOTICON_MUSIC:
					EmoteType = EMOTE_HAPPY;
					break;
				case EMOTICON_OOP:
				case EMOTICON_SORRY:
				case EMOTICON_SUSHI:
					EmoteType = EMOTE_PAIN;
					break;
				case EMOTICON_DEVILTEE:
				case EMOTICON_SPLATTEE:
				case EMOTICON_ZOMG:
					EmoteType = EMOTE_ANGRY;
					break;
				default:
					break;
				}
				pChr->SetEmote(EmoteType, Server()->Tick() + 2 * Server()->TickSpeed());
			}
		}
		else if(MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if(m_VoteCloseTime && m_VoteCreator == ClientID && GetDDRaceTeam(ClientID) && (IsKickVote() || IsSpecVote()))
			{
				SendChatTarget(ClientID, "You are running a vote please try again after the vote is done!");
				return;
			}
			if(pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * g_Config.m_SvKillDelay > Server()->Tick())
				return;
			if(pPlayer->IsPaused())
				return;

			CCharacter *pChr = pPlayer->GetCharacter();
			if(!pChr)
				return;

			//Kill Protection
			int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
			if(g_Config.m_SvKillProtection != 0 && CurrTime >= (60 * g_Config.m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
			{
				SendChatTarget(ClientID, "Kill Protection enabled. If you really want to kill, type /kill");
				return;
			}

			pPlayer->m_LastKill = Server()->Tick();
			pPlayer->KillCharacter(WEAPON_SELF);
			pPlayer->Respawn();
		}
	}
	if(MsgID == NETMSGTYPE_CL_STARTINFO)
	{
		if(pPlayer->m_IsReady)
			return;

		CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;

		if(!str_utf8_check(pMsg->m_pName))
		{
			Server()->Kick(ClientID, "name is not valid utf8");
			return;
		}
		if(!str_utf8_check(pMsg->m_pClan))
		{
			Server()->Kick(ClientID, "clan is not valid utf8");
			return;
		}
		if(!str_utf8_check(pMsg->m_pSkin))
		{
			Server()->Kick(ClientID, "skin is not valid utf8");
			return;
		}

		pPlayer->m_LastChangeInfo = Server()->Tick();

		// set start infos
		Server()->SetClientName(ClientID, pMsg->m_pName);
		// trying to set client name can delete the player object, check if it still exists
		if(!m_apPlayers[ClientID])
		{
			return;
		}
		Server()->SetClientClan(ClientID, pMsg->m_pClan);
		Server()->SetClientCountry(ClientID, pMsg->m_Country);
		str_copy(pPlayer->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_aSkinName));
		pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
		pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
		pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
		if(!Server()->IsSixup(ClientID))
			pPlayer->m_TeeInfos.ToSixup();

		// send clear vote options
		CNetMsg_Sv_VoteClearOptions ClearMsg;
		Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

		// begin sending vote options
		pPlayer->m_SendVoteIndex = 0;

		// send tuning parameters to client
		SendTuningParams(ClientID, pPlayer->m_TuneZone);

		// client is ready to enter
		pPlayer->m_IsReady = true;
		CNetMsg_Sv_ReadyToEnter m;
		Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

		Server()->ExpireServerInfo();
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);

	char aBuf[256];
	if(pResult->NumArguments() == 2)
	{
		float NewValue = pResult->GetFloat(1);
		if(pSelf->Tuning()->Set(pParamName, NewValue) && pSelf->Tuning()->Get(pParamName, &NewValue))
		{
			str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
			pSelf->SendTuningParams(-1);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "No such tuning parameter: %s", pParamName);
		}
	}
	else
	{
		float Value;
		if(pSelf->Tuning()->Get(pParamName, &Value))
		{
			str_format(aBuf, sizeof(aBuf), "%s %.2f", pParamName, Value);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "No such tuning parameter: %s", pParamName);
		}
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
}

void CGameContext::ConToggleTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float OldValue;

	char aBuf[256];
	if(!pSelf->Tuning()->Get(pParamName, &OldValue))
	{
		str_format(aBuf, sizeof(aBuf), "No such tuning parameter: %s", pParamName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		return;
	}

	float NewValue = fabs(OldValue - pResult->GetFloat(1)) < 0.0001f ? pResult->GetFloat(2) : pResult->GetFloat(1);

	pSelf->Tuning()->Set(pParamName, NewValue);
	pSelf->Tuning()->Get(pParamName, &NewValue);

	str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	pSelf->SendTuningParams(-1);
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		const char *pParamName = pResult->GetString(0);
		float DefaultValue = 0.0f;
		char aBuf[256];
		CTuningParams TuningParams;
		if(TuningParams.Get(pParamName, &DefaultValue) && pSelf->Tuning()->Set(pParamName, DefaultValue) && pSelf->Tuning()->Get(pParamName, &DefaultValue))
		{
			str_format(aBuf, sizeof(aBuf), "%s reset to %.2f", pParamName, DefaultValue);
			pSelf->SendTuningParams(-1);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "No such tuning parameter: %s", pParamName);
		}
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
	else
	{
		pSelf->ResetTuning();
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
	}
}

void CGameContext::ConTunes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float Value;
		pSelf->Tuning()->Get(i, &Value);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", CTuningParams::Name(i), Value);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConTuneZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	const char *pParamName = pResult->GetString(1);
	float NewValue = pResult->GetFloat(2);

	if(List >= 0 && List < NUM_TUNEZONES)
	{
		char aBuf[256];
		if(pSelf->TuningList()[List].Set(pParamName, NewValue) && pSelf->TuningList()[List].Get(pParamName, &NewValue))
		{
			str_format(aBuf, sizeof(aBuf), "%s in zone %d changed to %.2f", pParamName, List, NewValue);
			pSelf->SendTuningParams(-1, List);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "No such tuning parameter: %s", pParamName);
		}
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConTuneDumpZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	char aBuf[256];
	if(List >= 0 && List < NUM_TUNEZONES)
	{
		for(int i = 0; i < pSelf->TuningList()[List].Num(); i++)
		{
			float Value;
			pSelf->TuningList()[List].Get(i, &Value);
			str_format(aBuf, sizeof(aBuf), "zone %d: %s %.2f", List, CTuningParams::Name(i), Value);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		}
	}
}

void CGameContext::ConTuneResetZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			pSelf->TuningList()[List] = TuningParams;
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Tunezone %d reset", List);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
			pSelf->SendTuningParams(-1, List);
		}
	}
	else
	{
		for(int i = 0; i < NUM_TUNEZONES; i++)
		{
			*(pSelf->TuningList() + i) = TuningParams;
			pSelf->SendTuningParams(-1, i);
		}
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "All Tunezones reset");
	}
}

void CGameContext::ConTuneSetZoneMsgEnter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneEnterMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneEnterMsg[List]));
		}
	}
}

void CGameContext::ConTuneSetZoneMsgLeave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneLeaveMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneLeaveMsg[List]));
		}
	}
}

void CGameContext::ConMapbug(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pMapBugName = pResult->GetString(0);

	if(pSelf->m_pController)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapbugs", "can't add map bugs after the game started");
		return;
	}

	switch(pSelf->m_MapBugs.Update(pMapBugName))
	{
	case MAPBUGUPDATE_OK:
		break;
	case MAPBUGUPDATE_OVERRIDDEN:
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapbugs", "map-internal setting overridden by database");
		break;
	case MAPBUGUPDATE_NOTFOUND:
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "unknown map bug '%s', ignoring", pMapBugName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapbugs", aBuf);
	}
	break;
	default:
		dbg_assert(0, "unreachable");
	}
}

void CGameContext::ConSwitchOpen(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Switch = pResult->GetInteger(0);

	if(in_range(Switch, (int)pSelf->Switchers().size() - 1))
	{
		pSelf->Switchers()[Switch].m_Initial = false;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "switch %d opened by default", Switch);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_World.m_Paused ^= 1;
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRandomMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;

	pSelf->m_pScore->RandomMap(pSelf->m_VoteCreator, Stars);
}

void CGameContext::ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;

	pSelf->m_pScore->RandomUnfinishedMap(pSelf->m_VoteCreator, Stars);
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	char aBuf[1024];
	str_copy(aBuf, pResult->GetString(0), sizeof(aBuf));

	int i, j;
	for(i = 0, j = 0; aBuf[i]; i++, j++)
	{
		if(aBuf[i] == '\\' && aBuf[i + 1] == 'n')
		{
			aBuf[j] = '\n';
			i++;
		}
		else if(i != j)
		{
			aBuf[j] = aBuf[i];
		}
	}
	aBuf[j] = '\0';

	pSelf->SendBroadcast(aBuf, -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->Pause(CPlayer::PAUSE_NONE, false); // reset /spec and /pause to allow rejoin
	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[ClientID], Team);
	if(Team == TEAM_SPECTATORS)
		pSelf->m_apPlayers[ClientID]->Pause(CPlayer::PAUSE_NONE, true);
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	for(auto &pPlayer : pSelf->m_apPlayers)
		if(pPlayer)
			pSelf->m_pController->DoTeamChange(pPlayer, Team, false);
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	pSelf->AddVote(pDescription, pCommand);
}

void CGameContext::AddVote(const char *pDescription, const char *pCommand)
{
	if(m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len, alignof(CVoteOptionServer));
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if(!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// start reloading vote option list
	// clear vote options
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	// reset sending of vote options
	for(auto &pPlayer : pSelf->m_apPlayers)
	{
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
	}

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "authorized player forced server option '%s' (%s)", pValue, pReason);
				pSelf->SendChatTarget(-1, aBuf, CHAT_SIX);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if(!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "'%s' was moved to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;

	// reset sending of vote options
	for(auto &pPlayer : pSelf->m_apPlayers)
	{
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
	}
}

struct CMapNameItem
{
	char m_aName[IO_MAX_PATH_LENGTH - 4];

	bool operator<(const CMapNameItem &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }
};

void CGameContext::ConAddMapVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	std::vector<CMapNameItem> vMapList;
	pSelf->Storage()->ListDirectory(IStorage::TYPE_ALL, "maps", MapScan, &vMapList);
	std::sort(vMapList.begin(), vMapList.end());

	for(auto &Item : vMapList)
	{
		char aDescription[64];
		str_format(aDescription, sizeof(aDescription), "Map: %s", Item.m_aName);

		char aCommand[IO_MAX_PATH_LENGTH * 2 + 10];
		char aMapEscaped[IO_MAX_PATH_LENGTH * 2];
		char *pDst = aMapEscaped;
		str_escape(&pDst, Item.m_aName, aMapEscaped + sizeof(aMapEscaped));
		str_format(aCommand, sizeof(aCommand), "change_map \"%s\"", aMapEscaped);

		pSelf->AddVote(aDescription, aCommand);
	}

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "added maps to votes");
}

int CGameContext::MapScan(const char *pName, int IsDir, int DirType, void *pUserData)
{
	if(IsDir || !str_endswith(pName, ".map"))
		return 0;

	CMapNameItem Item;
	str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - str_length(".map"));
	static_cast<std::vector<CMapNameItem> *>(pUserData)->push_back(Item);

	return 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->ForceVote(pResult->m_ClientID, true);
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->ForceVote(pResult->m_ClientID, false);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->SendMotd(-1);
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	Console()->Register("tune", "s[tuning] ?i[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneParam, this, "Tune variable to value or show current value");
	Console()->Register("toggle_tune", "s[tuning] i[value 1] i[value 2]", CFGFLAG_SERVER | CFGFLAG_GAME, ConToggleTuneParam, this, "Toggle tune variable");
	Console()->Register("tune_reset", "?s[tuning]", CFGFLAG_SERVER, ConTuneReset, this, "Reset all or one tuning variable to default");
	Console()->Register("tunes", "", CFGFLAG_SERVER, ConTunes, this, "List all tuning variables and their values");
	Console()->Register("tune_zone", "i[zone] s[tuning] i[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneZone, this, "Tune in zone a variable to value");
	Console()->Register("tune_zone_dump", "i[zone]", CFGFLAG_SERVER, ConTuneDumpZone, this, "Dump zone tuning in zone x");
	Console()->Register("tune_zone_reset", "?i[zone]", CFGFLAG_SERVER, ConTuneResetZone, this, "reset zone tuning in zone x or in all zones");
	Console()->Register("tune_zone_enter", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgEnter, this, "which message to display on zone enter; use 0 for normal area");
	Console()->Register("tune_zone_leave", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgLeave, this, "which message to display on zone leave; use 0 for normal area");
	Console()->Register("mapbug", "s[mapbug]", CFGFLAG_SERVER | CFGFLAG_GAME, ConMapbug, this, "Enable map compatibility mode using the specified bug (example: grenade-doubleexplosion@ddnet.tw)");
	Console()->Register("switch_open", "i[switch]", CFGFLAG_SERVER | CFGFLAG_GAME, ConSwitchOpen, this, "Whether a switch is deactivated by default (otherwise activated)");
	Console()->Register("pause_game", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r[map]", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("random_map", "?i[stars]", CFGFLAG_SERVER, ConRandomMap, this, "Random map");
	Console()->Register("random_unfinished_map", "?i[stars]", CFGFLAG_SERVER, ConRandomUnfinishedMap, this, "Random unfinished map");
	Console()->Register("restart", "?i[seconds]", CFGFLAG_SERVER | CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i[team-id]", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");

	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "r[name]", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "s[name] s[command] ?r[reason]", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("add_map_votes", "", CFGFLAG_SERVER, ConAddMapVotes, this, "Automatically adds voting options for all maps");
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");
	Console()->Register("dump_antibot", "", CFGFLAG_SERVER, ConDumpAntibot, this, "Dumps the antibot status");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, flags, callback, userdata, help);
#include <game/ddracecommands.h>
#define CHAT_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, flags, callback, userdata, help);
#include <game/ddracechat.h>
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pAntibot = Kernel()->RequestInterface<IAntibot>();
	m_pAntibot->RoundStart(this);
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	m_GameUuid = RandomUuid();
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);

	uint64_t aSeed[2];
	secure_random_fill(aSeed, sizeof(aSeed));
	m_Prng.Seed(aSeed);
	m_World.m_Core.m_pPrng = &m_Prng;

	DeleteTempfile();

	//if(!data) // only load once
	//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);
	m_World.m_Core.InitSwitchers(m_Collision.m_HighestSwitchNumber);

	char aMapName[IO_MAX_PATH_LENGTH];
	int MapSize;
	SHA256_DIGEST MapSha256;
	int MapCrc;
	Server()->GetMapInfo(aMapName, sizeof(aMapName), &MapSize, &MapSha256, &MapCrc);
	m_MapBugs = GetMapBugs(aMapName, MapSize, MapSha256);

	// reset everything here
	//world = new GAMEWORLD;
	//players = new CPlayer[MAX_CLIENTS];

	// Reset Tunezones
	CTuningParams TuningParams;
	for(int i = 0; i < NUM_TUNEZONES; i++)
	{
		TuningList()[i] = TuningParams;
		TuningList()[i].Set("gun_curvature", 0);
		TuningList()[i].Set("gun_speed", 1400);
		TuningList()[i].Set("shotgun_curvature", 0);
		TuningList()[i].Set("shotgun_speed", 500);
		TuningList()[i].Set("shotgun_speeddiff", 0);
	}

	for(int i = 0; i < NUM_TUNEZONES; i++)
	{
		// Send no text by default when changing tune zones.
		m_aaZoneEnterMsg[i][0] = 0;
		m_aaZoneLeaveMsg[i][0] = 0;
	}
	// Reset Tuning
	if(g_Config.m_SvTuneReset)
	{
		ResetTuning();
	}
	else
	{
		Tuning()->Set("gun_speed", 1400);
		Tuning()->Set("gun_curvature", 0);
		Tuning()->Set("shotgun_speed", 500);
		Tuning()->Set("shotgun_speeddiff", 0);
		Tuning()->Set("shotgun_curvature", 0);
	}

	if(g_Config.m_SvDDRaceTuneReset)
	{
		g_Config.m_SvHit = 1;
		g_Config.m_SvEndlessDrag = 0;
		g_Config.m_SvOldLaser = 0;
		g_Config.m_SvOldTeleportHook = 0;
		g_Config.m_SvOldTeleportWeapons = 0;
		g_Config.m_SvTeleportHoldHook = 0;
		g_Config.m_SvTeam = SV_TEAM_ALLOWED;
		g_Config.m_SvShowOthersDefault = SHOW_OTHERS_OFF;

		for(auto &Switcher : Switchers())
			Switcher.m_Initial = true;
	}

	Console()->ExecuteFile(g_Config.m_SvResetFile, -1);

	LoadMapSettings();

	m_MapBugs.Dump();

	if(g_Config.m_SvSoloServer)
	{
		g_Config.m_SvTeam = SV_TEAM_FORCED_SOLO;
		g_Config.m_SvShowOthersDefault = SHOW_OTHERS_ON;

		Tuning()->Set("player_collision", 0);
		Tuning()->Set("player_hooking", 0);

		for(int i = 0; i < NUM_TUNEZONES; i++)
		{
			TuningList()[i].Set("player_collision", 0);
			TuningList()[i].Set("player_hooking", 0);
		}
	}

	m_pController = new CGameControllerDDRace(this);

	const char *pCensorFilename = "censorlist.txt";
	IOHANDLE File = Storage()->OpenFile(pCensorFilename, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
	if(!File)
	{
		dbg_msg("censorlist", "failed to open '%s'", pCensorFilename);
	}
	else
	{
		CLineReader LineReader;
		LineReader.Init(File);
		char *pLine;
		while((pLine = LineReader.Get()))
		{
			m_vCensorlist.emplace_back(pLine);
		}
		io_close(File);
	}

	m_TeeHistorianActive = g_Config.m_SvTeeHistorian;
	if(m_TeeHistorianActive)
	{
		char aGameUuid[UUID_MAXSTRSIZE];
		FormatUuid(m_GameUuid, aGameUuid, sizeof(aGameUuid));

		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "teehistorian/%s.teehistorian", aGameUuid);

		IOHANDLE THFile = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!THFile)
		{
			dbg_msg("teehistorian", "failed to open '%s'", aFilename);
			Server()->SetErrorShutdown("teehistorian open error");
			return;
		}
		else
		{
			dbg_msg("teehistorian", "recording to '%s'", aFilename);
		}
		m_pTeeHistorianFile = aio_new(THFile);

		char aVersion[128];
		if(GIT_SHORTREV_HASH)
		{
			str_format(aVersion, sizeof(aVersion), "%s (%s)", GAME_VERSION, GIT_SHORTREV_HASH);
		}
		else
		{
			str_format(aVersion, sizeof(aVersion), "%s", GAME_VERSION);
		}
		CTeeHistorian::CGameInfo GameInfo;
		GameInfo.m_GameUuid = m_GameUuid;
		GameInfo.m_pServerVersion = aVersion;
		GameInfo.m_StartTime = time(0);
		GameInfo.m_pPrngDescription = m_Prng.Description();

		GameInfo.m_pServerName = g_Config.m_SvName;
		GameInfo.m_ServerPort = Server()->Port();
		GameInfo.m_pGameType = m_pController->m_pGameType;

		GameInfo.m_pConfig = &g_Config;
		GameInfo.m_pTuning = Tuning();
		GameInfo.m_pUuids = &g_UuidManager;

		GameInfo.m_pMapName = aMapName;
		GameInfo.m_MapSize = MapSize;
		GameInfo.m_MapSha256 = MapSha256;
		GameInfo.m_MapCrc = MapCrc;

		m_TeeHistorian.Reset(&GameInfo, TeeHistorianWrite, this);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			int Level = Server()->GetAuthedState(i);
			if(Level)
			{
				m_TeeHistorian.RecordAuthInitial(i, Level, Server()->GetAuthName(i));
			}
		}
	}

	if(!m_pScore)
	{
		m_pScore = new CScore(this, ((CServer *)Server())->DbPool());
	}

	// setup core world
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	CTile *pFront = 0;
	CSwitchTile *pSwitch = 0;
	if(m_Layers.FrontLayer())
		pFront = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.FrontLayer()->m_Front);
	if(m_Layers.SwitchLayer())
		pSwitch = (CSwitchTile *)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SwitchLayer()->m_Switch);

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y * pTileMap->m_Width + x].m_Index;

			if(Index == TILE_OLDLASER)
			{
				g_Config.m_SvOldLaser = 1;
				dbg_msg("game_layer", "found old laser tile");
			}
			else if(Index == TILE_NPC)
			{
				m_Tuning.Set("player_collision", 0);
				dbg_msg("game_layer", "found no collision tile");
			}
			else if(Index == TILE_EHOOK)
			{
				g_Config.m_SvEndlessDrag = 1;
				dbg_msg("game_layer", "found unlimited hook time tile");
			}
			else if(Index == TILE_NOHIT)
			{
				g_Config.m_SvHit = 0;
				dbg_msg("game_layer", "found no weapons hitting others tile");
			}
			else if(Index == TILE_NPH)
			{
				m_Tuning.Set("player_hooking", 0);
				dbg_msg("game_layer", "found no player hooking tile");
			}

			if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				m_pController->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_GAME, pTiles[y * pTileMap->m_Width + x].m_Flags);
			}

			if(pFront)
			{
				Index = pFront[y * pTileMap->m_Width + x].m_Index;
				if(Index == TILE_OLDLASER)
				{
					g_Config.m_SvOldLaser = 1;
					dbg_msg("front_layer", "found old laser tile");
				}
				else if(Index == TILE_NPC)
				{
					m_Tuning.Set("player_collision", 0);
					dbg_msg("front_layer", "found no collision tile");
				}
				else if(Index == TILE_EHOOK)
				{
					g_Config.m_SvEndlessDrag = 1;
					dbg_msg("front_layer", "found unlimited hook time tile");
				}
				else if(Index == TILE_NOHIT)
				{
					g_Config.m_SvHit = 0;
					dbg_msg("front_layer", "found no weapons hitting others tile");
				}
				else if(Index == TILE_NPH)
				{
					m_Tuning.Set("player_hooking", 0);
					dbg_msg("front_layer", "found no player hooking tile");
				}
				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					m_pController->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_FRONT, pFront[y * pTileMap->m_Width + x].m_Flags);
				}
			}
			if(pSwitch)
			{
				Index = pSwitch[y * pTileMap->m_Width + x].m_Type;
				// TODO: Add off by default door here
				// if (Index == TILE_DOOR_OFF)
				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					m_pController->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_SWITCH, pSwitch[y * pTileMap->m_Width + x].m_Flags, pSwitch[y * pTileMap->m_Width + x].m_Number);
				}
			}
		}
	}

	//game.world.insert_entity(game.Controller);

	if(GIT_SHORTREV_HASH)
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "git-revision", GIT_SHORTREV_HASH);

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			OnClientConnected(MAX_CLIENTS - i - 1, 0);
		}
	}
#endif
}

void CGameContext::DeleteTempfile()
{
	if(m_aDeleteTempfile[0] != 0)
	{
		Storage()->RemoveFile(m_aDeleteTempfile, IStorage::TYPE_SAVE);
		m_aDeleteTempfile[0] = 0;
	}
}

void CGameContext::OnMapChange(char *pNewMapName, int MapNameSize)
{
	char aConfig[IO_MAX_PATH_LENGTH];
	str_format(aConfig, sizeof(aConfig), "maps/%s.cfg", g_Config.m_SvMap);

	IOHANDLE File = Storage()->OpenFile(aConfig, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
	if(!File)
	{
		// No map-specific config, just return.
		return;
	}
	CLineReader LineReader;
	LineReader.Init(File);

	std::vector<char *> vLines;
	char *pLine;
	int TotalLength = 0;
	while((pLine = LineReader.Get()))
	{
		int Length = str_length(pLine) + 1;
		char *pCopy = (char *)malloc(Length);
		mem_copy(pCopy, pLine, Length);
		vLines.push_back(pCopy);
		TotalLength += Length;
	}
	io_close(File);

	char *pSettings = (char *)malloc(maximum(1, TotalLength));
	int Offset = 0;
	for(auto &Line : vLines)
	{
		int Length = str_length(Line) + 1;
		mem_copy(pSettings + Offset, Line, Length);
		Offset += Length;
		free(Line);
	}

	CDataFileReader Reader;
	Reader.Open(Storage(), pNewMapName, IStorage::TYPE_ALL);

	CDataFileWriter Writer;
	Writer.Init();

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int TypeID;
		int ItemID;
		int *pData = (int *)Reader.GetItem(i, &TypeID, &ItemID);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if(TypeID == MAPITEMTYPE_INFO && ItemID == 0)
		{
			FoundInfo = true;
			CMapItemInfoSettings *pInfo = (CMapItemInfoSettings *)pData;
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				if(pInfo->m_Settings > -1)
				{
					SettingsIndex = pInfo->m_Settings;
					char *pMapSettings = (char *)Reader.GetData(SettingsIndex);
					int DataSize = Reader.GetDataSize(SettingsIndex);
					if(DataSize == TotalLength && mem_comp(pSettings, pMapSettings, DataSize) == 0)
					{
						// Configs coincide, no need to update map.
						free(pSettings);
						return;
					}
					Reader.UnloadData(pInfo->m_Settings);
				}
				else
				{
					MapInfo = *pInfo;
					MapInfo.m_Settings = SettingsIndex;
					pData = (int *)&MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo *)&MapInfo = *(CMapItemInfo *)pInfo;
				MapInfo.m_Settings = SettingsIndex;
				pData = (int *)&MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(TypeID, ItemID, Size, pData);
	}

	if(!FoundInfo)
	{
		CMapItemInfoSettings Info;
		Info.m_Version = 1;
		Info.m_Author = -1;
		Info.m_MapVersion = -1;
		Info.m_Credits = -1;
		Info.m_License = -1;
		Info.m_Settings = SettingsIndex;
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
	}

	for(int i = 0; i < Reader.NumData() || i == SettingsIndex; i++)
	{
		if(i == SettingsIndex)
		{
			Writer.AddData(TotalLength, pSettings);
			continue;
		}
		unsigned char *pData = (unsigned char *)Reader.GetData(i);
		int Size = Reader.GetDataSize(i);
		Writer.AddData(Size, pData);
		Reader.UnloadData(i);
	}

	dbg_msg("mapchange", "imported settings");
	free(pSettings);
	Reader.Close();
	char aTemp[IO_MAX_PATH_LENGTH];
	Writer.OpenFile(Storage(), IStorage::FormatTmpPath(aTemp, sizeof(aTemp), pNewMapName));
	Writer.Finish();

	str_copy(pNewMapName, aTemp, MapNameSize);
	str_copy(m_aDeleteTempfile, aTemp, sizeof(m_aDeleteTempfile));
}

void CGameContext::OnShutdown()
{
	Antibot()->RoundEnd();

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.Finish();
		aio_close(m_pTeeHistorianFile);
		aio_wait(m_pTeeHistorianFile);
		int Error = aio_error(m_pTeeHistorianFile);
		if(Error)
		{
			dbg_msg("teehistorian", "error closing file, err=%d", Error);
			Server()->SetErrorShutdown("teehistorian close error");
		}
		aio_free(m_pTeeHistorianFile);
	}

	DeleteTempfile();
	Console()->ResetServerGameSettings();
	Collision()->Dest();
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::LoadMapSettings()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, 0, &ItemID);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || ItemID != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		int Size = pMap->GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			Console()->ExecuteLine(pNext, IConsole::CLIENT_ID_GAME);
			pNext += StrSize;
		}
		pMap->UnloadData(pItem->m_Settings);
		break;
	}

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map.cfg", g_Config.m_SvMap);
	Console()->ExecuteFile(aBuf, IConsole::CLIENT_ID_NO_GAME);
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(Server()->IsRecording(ClientID > -1 ? ClientID : MAX_CLIENTS) && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientID);
	}

	m_pController->Snap(ClientID);

	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
			pPlayer->Snap(ClientID);
	}

	if(ClientID > -1)
		m_apPlayers[ClientID]->FakeSnap();

	m_World.Snap(ClientID);
	m_Events.Snap(ClientID);
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady;
}

bool CGameContext::IsClientPlayer(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS;
}

CUuid CGameContext::GameUuid() const { return m_GameUuid; }
const char *CGameContext::GameType() const { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() const { return GAME_VERSION; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

bool CGameContext::PlayerCollision()
{
	float Temp;
	m_Tuning.Get("player_collision", &Temp);
	return Temp != 0.0f;
}

bool CGameContext::PlayerHooking()
{
	float Temp;
	m_Tuning.Get("player_hooking", &Temp);
	return Temp != 0.0f;
}

float CGameContext::PlayerJetpack()
{
	float Temp;
	m_Tuning.Get("player_jetpack", &Temp);
	return Temp;
}

void CGameContext::OnSetAuthed(int ClientID, int Level)
{
	if(m_apPlayers[ClientID])
	{
		char aBuf[512], aIP[NETADDR_MAXSTRSIZE];
		Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
		str_format(aBuf, sizeof(aBuf), "ban %s %d Banned by vote", aIP, g_Config.m_SvVoteKickBantime);
		if(!str_comp_nocase(m_aVoteCommand, aBuf) && Level > Server()->GetAuthedState(m_VoteCreator))
		{
			m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO_ADMIN;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CGameContext", "Vote aborted by authorized login.");
		}
	}
	if(m_TeeHistorianActive)
	{
		if(Level)
		{
			m_TeeHistorian.RecordAuthLogin(ClientID, Level, Server()->GetAuthName(ClientID));
		}
		else
		{
			m_TeeHistorian.RecordAuthLogout(ClientID);
		}
	}
}

void CGameContext::SendRecord(int ClientID)
{
	CNetMsg_Sv_Record Msg;
	CNetMsg_Sv_RecordLegacy MsgLegacy;
	MsgLegacy.m_PlayerTimeBest = Msg.m_PlayerTimeBest = Score()->PlayerData(ClientID)->m_BestTime * 100.0f;
	MsgLegacy.m_ServerTimeBest = Msg.m_ServerTimeBest = m_pController->m_CurrentRecord * 100.0f; //TODO: finish this
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	if(!Server()->IsSixup(ClientID) && GetClientVersion(ClientID) < VERSION_DDNET_MSG_LEGACY)
	{
		Server()->SendPackMsg(&MsgLegacy, MSGFLAG_VITAL, ClientID);
	}
}

int CGameContext::ProcessSpamProtection(int ClientID, bool RespectChatInitialDelay)
{
	if(!m_apPlayers[ClientID])
		return 0;
	if(g_Config.m_SvSpamprotection && m_apPlayers[ClientID]->m_LastChat && m_apPlayers[ClientID]->m_LastChat + Server()->TickSpeed() * g_Config.m_SvChatDelay > Server()->Tick())
		return 1;
	else if(g_Config.m_SvDnsblChat && Server()->DnsblBlack(ClientID))
	{
		SendChatTarget(ClientID, "Players are not allowed to chat from VPNs at this time");
		return 1;
	}
	else
		m_apPlayers[ClientID]->m_LastChat = Server()->Tick();

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);

	int Muted = 0;
	for(int i = 0; i < m_NumMutes && Muted <= 0; i++)
	{
		if(!net_addr_comp_noport(&Addr, &m_aMutes[i].m_Addr))
		{
			if(RespectChatInitialDelay || m_aMutes[i].m_InitialChatDelay)
				Muted = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
		}
	}

	if(Muted > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "You are not permitted to talk for the next %d seconds.", Muted);
		SendChatTarget(ClientID, aBuf);
		return 1;
	}

	if(g_Config.m_SvSpamMuteDuration && (m_apPlayers[ClientID]->m_ChatScore += g_Config.m_SvChatPenalty) > g_Config.m_SvChatThreshold)
	{
		Mute(&Addr, g_Config.m_SvSpamMuteDuration, Server()->ClientName(ClientID));
		m_apPlayers[ClientID]->m_ChatScore = 0;
		return 1;
	}

	return 0;
}

int CGameContext::GetDDRaceTeam(int ClientID)
{
	CGameControllerDDRace *pController = (CGameControllerDDRace *)m_pController;
	return pController->m_Teams.m_Core.Team(ClientID);
}

void CGameContext::ResetTuning()
{
	CTuningParams TuningParams;
	m_Tuning = TuningParams;
	Tuning()->Set("gun_speed", 1400);
	Tuning()->Set("gun_curvature", 0);
	Tuning()->Set("shotgun_speed", 500);
	Tuning()->Set("shotgun_speeddiff", 0);
	Tuning()->Set("shotgun_curvature", 0);
	SendTuningParams(-1);
}

bool CheckClientID2(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS;
}

void CGameContext::Whisper(int ClientID, char *pStr)
{
	char *pName;
	char *pMessage;
	int Error = 0;

	if(ProcessSpamProtection(ClientID))
		return;

	pStr = str_skip_whitespaces(pStr);

	int Victim;

	// add token
	if(*pStr == '"')
	{
		pStr++;

		pName = pStr;
		char *pDst = pStr; // we might have to process escape data
		while(true)
		{
			if(pStr[0] == '"')
			{
				break;
			}
			else if(pStr[0] == '\\')
			{
				if(pStr[1] == '\\')
					pStr++; // skip due to escape
				else if(pStr[1] == '"')
					pStr++; // skip due to escape
			}
			else if(pStr[0] == 0)
			{
				Error = 1;
				break;
			}

			*pDst = *pStr;
			pDst++;
			pStr++;
		}

		if(!Error)
		{
			// write null termination
			*pDst = 0;

			pStr++;

			for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
				if(str_comp(pName, Server()->ClientName(Victim)) == 0)
					break;
		}
	}
	else
	{
		pName = pStr;
		while(true)
		{
			if(pStr[0] == 0)
			{
				Error = 1;
				break;
			}
			if(pStr[0] == ' ')
			{
				pStr[0] = 0;
				for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
					if(str_comp(pName, Server()->ClientName(Victim)) == 0)
						break;

				pStr[0] = ' ';

				if(Victim < MAX_CLIENTS)
					break;
			}
			pStr++;
		}
	}

	if(pStr[0] != ' ')
	{
		Error = 1;
	}

	*pStr = 0;
	pStr++;

	pMessage = pStr;

	char aBuf[256];

	if(Error)
	{
		str_format(aBuf, sizeof(aBuf), "Invalid whisper");
		SendChatTarget(ClientID, aBuf);
		return;
	}

	if(Victim >= MAX_CLIENTS || !CheckClientID2(Victim))
	{
		str_format(aBuf, sizeof(aBuf), "No player with name \"%s\" found", pName);
		SendChatTarget(ClientID, aBuf);
		return;
	}

	WhisperID(ClientID, Victim, pMessage);
}

void CGameContext::WhisperID(int ClientID, int VictimID, const char *pMessage)
{
	if(!CheckClientID2(ClientID))
		return;

	if(!CheckClientID2(VictimID))
		return;

	if(m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_LastWhisperTo = VictimID;

	char aCensoredMessage[256];
	CensorMessage(aCensoredMessage, pMessage, sizeof(aCensoredMessage));

	char aBuf[256];

	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else if(GetClientVersion(ClientID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = CHAT_WHISPER_SEND;
		Msg.m_ClientID = VictimID;
		Msg.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[→ %s] %s", Server()->ClientName(VictimID), aCensoredMessage);
		SendChatTarget(ClientID, aBuf);
	}

	if(Server()->IsSixup(VictimID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else if(GetClientVersion(VictimID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg2;
		Msg2.m_Team = CHAT_WHISPER_RECV;
		Msg2.m_ClientID = ClientID;
		Msg2.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL, VictimID);
		else
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[← %s] %s", Server()->ClientName(ClientID), aCensoredMessage);
		SendChatTarget(VictimID, aBuf);
	}
}

void CGameContext::Converse(int ClientID, char *pStr)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(ProcessSpamProtection(ClientID))
		return;

	if(pPlayer->m_LastWhisperTo < 0)
		SendChatTarget(ClientID, "You do not have an ongoing conversation. Whisper to someone to start one");
	else
	{
		WhisperID(ClientID, pPlayer->m_LastWhisperTo, pStr);
	}
}

bool CGameContext::IsVersionBanned(int Version)
{
	char aVersion[16];
	str_format(aVersion, sizeof(aVersion), "%d", Version);

	return str_in_list(g_Config.m_SvBannedVersions, ",", aVersion);
}

void CGameContext::List(int ClientID, const char *pFilter)
{
	int Total = 0;
	char aBuf[256];
	int Bufcnt = 0;
	if(pFilter[0])
		str_format(aBuf, sizeof(aBuf), "Listing players with \"%s\" in name:", pFilter);
	else
		str_format(aBuf, sizeof(aBuf), "Listing all players:");
	SendChatTarget(ClientID, aBuf);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			Total++;
			const char *pName = Server()->ClientName(i);
			if(str_utf8_find_nocase(pName, pFilter) == NULL)
				continue;
			if(Bufcnt + str_length(pName) + 4 > 256)
			{
				SendChatTarget(ClientID, aBuf);
				Bufcnt = 0;
			}
			if(Bufcnt != 0)
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, ", %s", pName);
				Bufcnt += 2 + str_length(pName);
			}
			else
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, "%s", pName);
				Bufcnt += str_length(pName);
			}
		}
	}
	if(Bufcnt != 0)
		SendChatTarget(ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%d players online", Total);
	SendChatTarget(ClientID, aBuf);
}

int CGameContext::GetClientVersion(int ClientID) const
{
	return Server()->GetClientVersion(ClientID);
}

int64_t CGameContext::ClientsMaskExcludeClientVersionAndHigher(int Version)
{
	int64_t Mask = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GetClientVersion(i) >= Version)
			continue;
		Mask |= 1LL << i;
	}
	return Mask;
}

bool CGameContext::PlayerModerating() const
{
	return std::any_of(std::begin(m_apPlayers), std::end(m_apPlayers), [](const CPlayer *pPlayer) { return pPlayer && pPlayer->m_Moderating; });
}

void CGameContext::ForceVote(int EnforcerID, bool Success)
{
	// check if there is a vote running
	if(!m_VoteCloseTime)
		return;

	m_VoteEnforce = Success ? CGameContext::VOTE_ENFORCE_YES_ADMIN : CGameContext::VOTE_ENFORCE_NO_ADMIN;
	m_VoteEnforcer = EnforcerID;

	char aBuf[256];
	const char *pOption = Success ? "yes" : "no";
	str_format(aBuf, sizeof(aBuf), "authorized player forced vote %s", pOption);
	SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pOption);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

bool CGameContext::RateLimitPlayerVote(int ClientID)
{
	int64_t Now = Server()->Tick();
	int64_t TickSpeed = Server()->TickSpeed();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(g_Config.m_SvRconVote && !Server()->GetAuthedState(ClientID))
	{
		SendChatTarget(ClientID, "You can only vote after logging in.");
		return true;
	}

	if(g_Config.m_SvDnsblVote && Server()->DistinctClientCount() > 1)
	{
		if(m_pServer->DnsblPending(ClientID))
		{
			SendChatTarget(ClientID, "You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.");
			return true;
		}
		else if(m_pServer->DnsblBlack(ClientID))
		{
			SendChatTarget(ClientID, "You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.");
			return true;
		}
	}

	if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + TickSpeed * 3 > Now)
		return true;

	pPlayer->m_LastVoteTry = Now;
	if(m_VoteCloseTime)
	{
		SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
		return true;
	}

	if(Now < pPlayer->m_FirstVoteTick)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "You must wait %d seconds before making your first vote.", (int)((pPlayer->m_FirstVoteTick - Now) / TickSpeed) + 1);
		SendChatTarget(ClientID, aBuf);
		return true;
	}

	int TimeLeft = pPlayer->m_LastVoteCall + TickSpeed * g_Config.m_SvVoteDelay - Now;
	if(pPlayer->m_LastVoteCall && TimeLeft > 0)
	{
		char aChatmsg[64];
		str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote.", (int)(TimeLeft / TickSpeed) + 1);
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int VoteMuted = 0;
	for(int i = 0; i < m_NumVoteMutes && !VoteMuted; i++)
		if(!net_addr_comp_noport(&Addr, &m_aVoteMutes[i].m_Addr))
			VoteMuted = (m_aVoteMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	for(int i = 0; i < m_NumMutes && VoteMuted == 0; i++)
	{
		if(!net_addr_comp_noport(&Addr, &m_aMutes[i].m_Addr))
			VoteMuted = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	}
	if(VoteMuted > 0)
	{
		char aChatmsg[64];
		str_format(aChatmsg, sizeof(aChatmsg), "You are not permitted to vote for the next %d seconds.", VoteMuted);
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}
	return false;
}

bool CGameContext::RateLimitPlayerMapVote(int ClientID)
{
	if(!Server()->GetAuthedState(ClientID) && time_get() < m_LastMapVote + (time_freq() * g_Config.m_SvVoteMapTimeDelay))
	{
		char aChatmsg[512] = {0};
		str_format(aChatmsg, sizeof(aChatmsg), "There's a %d second delay between map-votes, please wait %d seconds.",
			g_Config.m_SvVoteMapTimeDelay, (int)((m_LastMapVote + g_Config.m_SvVoteMapTimeDelay * time_freq() - time_get()) / time_freq()));
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}
	return false;
}
