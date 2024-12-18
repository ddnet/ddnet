/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecontext.h"

#include <vector>

#include "teeinfo.h"
#include <antibot/antibot_data.h>
#include <base/logger.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/json.h>
#include <engine/shared/linereader.h>
#include <engine/shared/memheap.h>
#include <engine/shared/protocolglue.h>
#include <engine/storage.h>

#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>
#include <game/version.h>

#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include "entities/character.h"
#include "gamemodes/DDRace.h"
#include "gamemodes/mod.h"
#include "player.h"
#include "score.h"

// Not thread-safe!
class CClientChatLogger : public ILogger
{
	CGameContext *m_pGameServer;
	int m_ClientId;
	ILogger *m_pOuterLogger;

public:
	CClientChatLogger(CGameContext *pGameServer, int ClientId, ILogger *pOuterLogger) :
		m_pGameServer(pGameServer),
		m_ClientId(ClientId),
		m_pOuterLogger(pOuterLogger)
	{
	}
	void Log(const CLogMessage *pMessage) override;
};

void CClientChatLogger::Log(const CLogMessage *pMessage)
{
	if(str_comp(pMessage->m_aSystem, "chatresp") == 0)
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		m_pGameServer->SendChatTarget(m_ClientId, pMessage->Message());
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

	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_LastMapVote = 0;

	m_SqlRandomMapResult = nullptr;

	m_pScore = nullptr;
	m_NumMutes = 0;
	m_NumVoteMutes = 0;

	m_VoteCreator = -1;
	m_VoteType = VOTE_TYPE_UNKNOWN;
	m_VoteCloseTime = 0;
	m_VoteUpdate = false;
	m_VotePos = 0;
	m_aVoteDescription[0] = '\0';
	m_aSixupVoteDescription[0] = '\0';
	m_aVoteCommand[0] = '\0';
	m_aVoteReason[0] = '\0';
	m_NumVoteOptions = 0;
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;

	m_LatestLog = 0;
	mem_zero(&m_aLogs, sizeof(m_aLogs));

	if(Resetting == NO_RESET)
	{
		for(auto &pSavedTee : m_apSavedTees)
			pSavedTee = nullptr;

		for(auto &pSavedTeleTee : m_apSavedTeleTees)
			pSavedTeleTee = nullptr;

		for(auto &pSavedTeam : m_apSavedTeams)
			pSavedTeam = nullptr;

		std::fill(std::begin(m_aTeamMapping), std::end(m_aTeamMapping), -1);

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
	{
		for(auto &pSavedTee : m_apSavedTees)
			delete pSavedTee;

		for(auto &pSavedTeleTee : m_apSavedTeleTees)
			delete pSavedTeleTee;

		for(auto &pSavedTeam : m_apSavedTeams)
			delete pSavedTeam;

		delete m_pVoteOptionHeap;
	}

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

void CGameContext::CommandCallback(int ClientId, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	if(pSelf->m_TeeHistorianActive)
	{
		pSelf->m_TeeHistorian.RecordConsoleCommand(ClientId, FlagMask, pCmd, pResult);
	}
}

CNetObj_PlayerInput CGameContext::GetLastPlayerInput(int ClientId) const
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid ClientId");
	return m_aLastPlayerInput[ClientId];
}

class CCharacter *CGameContext::GetPlayerChar(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !m_apPlayers[ClientId])
		return 0;
	return m_apPlayers[ClientId]->GetCharacter();
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

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount, CClientMask Mask)
{
	float a = 3 * pi / 2 + Angle;
	//float a = get_angle(dir);
	float s = a - pi / 3;
	float e = a + pi / 3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, (i + 1) / (float)(Amount + 2));
		CNetEvent_DamageInd *pEvent = m_Events.Create<CNetEvent_DamageInd>(Mask);
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f * 256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos, CClientMask Mask)
{
	CNetEvent_HammerHit *pEvent = m_Events.Create<CNetEvent_HammerHit>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, CClientMask Mask)
{
	// create the event
	CNetEvent_Explosion *pEvent = m_Events.Create<CNetEvent_Explosion>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CEntity *apEnts[MAX_CLIENTS];
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int Num = m_World.FindEntities(Pos, Radius, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	CClientMask TeamMask = CClientMask().set();
	for(int i = 0; i < Num; i++)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);
		vec2 Diff = pChr->m_Pos - Pos;
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

		if((GetPlayerChar(Owner) ? !GetPlayerChar(Owner)->GrenadeHitDisabled() : g_Config.m_SvHit) || NoDamage || Owner == pChr->GetPlayer()->GetCid())
		{
			if(Owner != -1 && pChr->IsAlive() && !pChr->CanCollide(Owner))
				continue;
			if(Owner == -1 && ActivatedTeam != -1 && pChr->IsAlive() && pChr->Team() != ActivatedTeam)
				continue;

			// Explode at most once per team
			int PlayerTeam = pChr->Team();
			if((GetPlayerChar(Owner) ? GetPlayerChar(Owner)->GrenadeHitDisabled() : !g_Config.m_SvHit) || NoDamage)
			{
				if(PlayerTeam == TEAM_SUPER)
					continue;
				if(!TeamMask.test(PlayerTeam))
					continue;
				TeamMask.reset(PlayerTeam);
			}

			pChr->TakeDamage(ForceDir * Dmg * 2, (int)Dmg, Owner, Weapon);
		}
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos, CClientMask Mask)
{
	CNetEvent_Spawn *pEvent = m_Events.Create<CNetEvent_Spawn>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientId, CClientMask Mask)
{
	CNetEvent_Death *pEvent = m_Events.Create<CNetEvent_Death>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientId = ClientId;
	}
}

void CGameContext::CreateBirthdayEffect(vec2 Pos, CClientMask Mask)
{
	CNetEvent_Birthday *pEvent = m_Events.Create<CNetEvent_Birthday>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateFinishEffect(vec2 Pos, CClientMask Mask)
{
	CNetEvent_Finish *pEvent = m_Events.Create<CNetEvent_Finish>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, CClientMask Mask)
{
	if(Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = m_Events.Create<CNetEvent_SoundWorld>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundId = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target) const
{
	if(Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundId = Sound;
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

void CGameContext::SnapSwitchers(int SnappingClient)
{
	if(Switchers().empty())
		return;

	CPlayer *pPlayer = SnappingClient != SERVER_DEMO_CLIENT ? m_apPlayers[SnappingClient] : 0;
	int Team = pPlayer && pPlayer->GetCharacter() ? pPlayer->GetCharacter()->Team() : 0;

	if(pPlayer && (pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->m_SpectatorId != SPEC_FREEVIEW && m_apPlayers[pPlayer->m_SpectatorId] && m_apPlayers[pPlayer->m_SpectatorId]->GetCharacter())
		Team = m_apPlayers[pPlayer->m_SpectatorId]->GetCharacter()->Team();

	if(Team == TEAM_SUPER)
		return;

	int SentTeam = Team;
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
		SentTeam = 0;

	CNetObj_SwitchState *pSwitchState = Server()->SnapNewItem<CNetObj_SwitchState>(SentTeam);
	if(!pSwitchState)
		return;

	pSwitchState->m_HighestSwitchNumber = clamp((int)Switchers().size() - 1, 0, 255);
	mem_zero(pSwitchState->m_aStatus, sizeof(pSwitchState->m_aStatus));

	std::vector<std::pair<int, int>> vEndTicks; // <EndTick, SwitchNumber>

	for(int i = 0; i <= pSwitchState->m_HighestSwitchNumber; i++)
	{
		int Status = (int)Switchers()[i].m_aStatus[Team];
		pSwitchState->m_aStatus[i / 32] |= (Status << (i % 32));

		int EndTick = Switchers()[i].m_aEndTick[Team];
		if(EndTick > 0 && EndTick < Server()->Tick() + 3 * Server()->TickSpeed() && Switchers()[i].m_aLastUpdateTick[Team] < Server()->Tick())
		{
			// only keep track of EndTicks that have less than three second left and are not currently being updated by a player being present on a switch tile, to limit how often these are sent
			vEndTicks.emplace_back(Switchers()[i].m_aEndTick[Team], i);
		}
	}

	// send the endtick of switchers that are about to toggle back (up to four, prioritizing those with the earliest endticks)
	mem_zero(pSwitchState->m_aSwitchNumbers, sizeof(pSwitchState->m_aSwitchNumbers));
	mem_zero(pSwitchState->m_aEndTicks, sizeof(pSwitchState->m_aEndTicks));

	std::sort(vEndTicks.begin(), vEndTicks.end());
	const int NumTimedSwitchers = minimum((int)vEndTicks.size(), (int)std::size(pSwitchState->m_aEndTicks));

	for(int i = 0; i < NumTimedSwitchers; i++)
	{
		pSwitchState->m_aSwitchNumbers[i] = vEndTicks[i].second;
		pSwitchState->m_aEndTicks[i] = vEndTicks[i].first;
	}
}

bool CGameContext::SnapLaserObject(const CSnapContext &Context, int SnapId, const vec2 &To, const vec2 &From, int StartTick, int Owner, int LaserType, int Subtype, int SwitchNumber) const
{
	if(Context.GetClientVersion() >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(SnapId);
		if(!pObj)
			return false;

		pObj->m_ToX = (int)To.x;
		pObj->m_ToY = (int)To.y;
		pObj->m_FromX = (int)From.x;
		pObj->m_FromY = (int)From.y;
		pObj->m_StartTick = StartTick;
		pObj->m_Owner = Owner;
		pObj->m_Type = LaserType;
		pObj->m_Subtype = Subtype;
		pObj->m_SwitchNumber = SwitchNumber;
		pObj->m_Flags = 0;
	}
	else
	{
		CNetObj_Laser *pObj = Server()->SnapNewItem<CNetObj_Laser>(SnapId);
		if(!pObj)
			return false;

		pObj->m_X = (int)To.x;
		pObj->m_Y = (int)To.y;
		pObj->m_FromX = (int)From.x;
		pObj->m_FromY = (int)From.y;
		pObj->m_StartTick = StartTick;
	}

	return true;
}

bool CGameContext::SnapPickup(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType, int SwitchNumber) const
{
	if(Context.IsSixup())
	{
		protocol7::CNetObj_Pickup *pPickup = Server()->SnapNewItem<protocol7::CNetObj_Pickup>(SnapId);
		if(!pPickup)
			return false;

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = PickupType_SixToSeven(Type, SubType);
	}
	else if(Context.GetClientVersion() >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(SnapId);
		if(!pPickup)
			return false;

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = Type;
		pPickup->m_Subtype = SubType;
		pPickup->m_SwitchNumber = SwitchNumber;
	}
	else
	{
		CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(SnapId);
		if(!pPickup)
			return false;

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;

		pPickup->m_Type = Type;
		if(Context.GetClientVersion() < VERSION_DDNET_WEAPON_SHIELDS)
		{
			if(Type >= POWERUP_ARMOR_SHOTGUN && Type <= POWERUP_ARMOR_LASER)
			{
				pPickup->m_Type = POWERUP_ARMOR;
			}
		}
		pPickup->m_Subtype = SubType;
	}

	return true;
}

void CGameContext::CallVote(int ClientId, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64_t Now = Server()->Tick();
	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(!pPlayer)
		return;

	SendChat(-1, TEAM_ALL, pChatmsg, -1, FLAG_SIX);
	if(!pSixupDesc)
		pSixupDesc = pDesc;

	m_VoteCreator = ClientId;
	StartVote(pDesc, pCmd, pReason, pSixupDesc);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
	pPlayer->m_LastVoteCall = Now;
}

void CGameContext::SendChatTarget(int To, const char *pText, int VersionFlags) const
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientId = -1;
	Msg.m_pMessage = pText;

	if(g_Config.m_SvDemoChat)
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, SERVER_DEMO_CLIENT);

	if(To == -1)
	{
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!((Server()->IsSixup(i) && (VersionFlags & FLAG_SIXUP)) ||
				   (!Server()->IsSixup(i) && (VersionFlags & FLAG_SIX))))
				continue;

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		if(!((Server()->IsSixup(To) && (VersionFlags & FLAG_SIXUP)) ||
			   (!Server()->IsSixup(To) && (VersionFlags & FLAG_SIX))))
			return;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, To);
	}
}

void CGameContext::SendChatTeam(int Team, const char *pText) const
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_apPlayers[i] != nullptr && GetDDRaceTeam(i) == Team)
			SendChatTarget(i, pText);
}

void CGameContext::SendChat(int ChatterClientId, int Team, const char *pText, int SpamProtectionClientId, int VersionFlags)
{
	if(SpamProtectionClientId >= 0 && SpamProtectionClientId < MAX_CLIENTS)
		if(ProcessSpamProtection(SpamProtectionClientId))
			return;

	char aBuf[256], aText[256];
	str_copy(aText, pText, sizeof(aText));
	if(ChatterClientId >= 0 && ChatterClientId < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientId, Team, Server()->ClientName(ChatterClientId), aText);
	else if(ChatterClientId == -2)
	{
		str_format(aBuf, sizeof(aBuf), "### %s", aText);
		str_copy(aText, aBuf, sizeof(aText));
		ChatterClientId = -1;
	}
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", aText);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Team != TEAM_ALL ? "teamchat" : "chat", aBuf);

	if(Team == TEAM_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientId = ChatterClientId;
		Msg.m_pMessage = aText;

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, SERVER_DEMO_CLIENT);

		// send to the clients
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!m_apPlayers[i])
				continue;
			bool Send = (Server()->IsSixup(i) && (VersionFlags & FLAG_SIXUP)) ||
				    (!Server()->IsSixup(i) && (VersionFlags & FLAG_SIX));

			if(!m_apPlayers[i]->m_DND && Send)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}

		str_format(aBuf, sizeof(aBuf), "Chat: %s", aText);
		LogEvent(aBuf, ChatterClientId);
	}
	else
	{
		CTeamsCore *pTeams = &m_pController->Teams().m_Core;
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientId = ChatterClientId;
		Msg.m_pMessage = aText;

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, SERVER_DEMO_CLIENT);

		// send to the clients
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(m_apPlayers[i] != 0)
			{
				if(Team == TEAM_SPECTATORS)
				{
					if(m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
				else
				{
					if(pTeams->Team(i) == Team && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
			}
		}
	}
}

void CGameContext::SendStartWarning(int ClientId, const char *pMessage)
{
	CCharacter *pChr = GetPlayerChar(ClientId);
	if(pChr && pChr->m_LastStartWarning < Server()->Tick() - 3 * Server()->TickSpeed())
	{
		SendChatTarget(ClientId, pMessage);
		pChr->m_LastStartWarning = Server()->Tick();
	}
}

void CGameContext::SendEmoticon(int ClientId, int Emoticon, int TargetClientId) const
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientId = ClientId;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, TargetClientId);
}

void CGameContext::SendWeaponPickup(int ClientId, int Weapon) const
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::SendMotd(int ClientId) const
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::SendSettings(int ClientId) const
{
	protocol7::CNetMsg_Sv_ServerSettings Msg;
	Msg.m_KickVote = g_Config.m_SvVoteKick;
	Msg.m_KickMin = g_Config.m_SvVoteKickMin;
	Msg.m_SpecVote = g_Config.m_SvVoteSpectate;
	Msg.m_TeamLock = 0;
	Msg.m_TeamBalance = 0;
	Msg.m_PlayerSlots = Server()->MaxClients() - g_Config.m_SvSpectatorSlots;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
}

void CGameContext::SendBroadcast(const char *pText, int ClientId, bool IsImportant)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;

	if(ClientId == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);

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

	if(!m_apPlayers[ClientId])
		return;

	if(!IsImportant && m_apPlayers[ClientId]->m_LastBroadcastImportance && m_apPlayers[ClientId]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
	m_apPlayers[ClientId]->m_LastBroadcast = Server()->Tick();
	m_apPlayers[ClientId]->m_LastBroadcastImportance = IsImportant;
}

void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc)
{
	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
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

void CGameContext::SendVoteSet(int ClientId)
{
	::CNetMsg_Sv_VoteSet Msg6;
	protocol7::CNetMsg_Sv_VoteSet Msg7;

	Msg7.m_ClientId = m_VoteCreator;
	if(m_VoteCloseTime)
	{
		Msg6.m_Timeout = Msg7.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg6.m_pDescription = m_aVoteDescription;
		Msg6.m_pReason = Msg7.m_pReason = m_aVoteReason;

		Msg7.m_pDescription = m_aSixupVoteDescription;
		if(IsKickVote())
			Msg7.m_Type = protocol7::VOTE_START_KICK;
		else if(IsSpecVote())
			Msg7.m_Type = protocol7::VOTE_START_SPEC;
		else if(IsOptionVote())
			Msg7.m_Type = protocol7::VOTE_START_OP;
		else
			Msg7.m_Type = protocol7::VOTE_UNKNOWN;
	}
	else
	{
		Msg6.m_Timeout = Msg7.m_Timeout = 0;
		Msg6.m_pDescription = Msg7.m_pDescription = "";
		Msg6.m_pReason = Msg7.m_pReason = "";

		if(m_VoteEnforce == VOTE_ENFORCE_NO || m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			Msg7.m_Type = protocol7::VOTE_END_FAIL;
		else if(m_VoteEnforce == VOTE_ENFORCE_YES || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_Type = protocol7::VOTE_END_PASS;
		else if(m_VoteEnforce == VOTE_ENFORCE_ABORT || m_VoteEnforce == VOTE_ENFORCE_CANCEL)
			Msg7.m_Type = protocol7::VOTE_END_ABORT;
		else
			Msg7.m_Type = protocol7::VOTE_UNKNOWN;

		if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_ClientId = -1;
	}

	if(ClientId == -1)
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
		if(!Server()->IsSixup(ClientId))
			Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, ClientId);
		else
			Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, ClientId);
	}
}

void CGameContext::SendVoteStatus(int ClientId, int Total, int Yes, int No)
{
	if(ClientId == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(Server()->ClientIngame(i))
				SendVoteStatus(i, Total, Yes, No);
		return;
	}

	if(Total > VANILLA_MAX_CLIENTS && m_apPlayers[ClientId] && m_apPlayers[ClientId]->GetClientVersion() <= VERSION_DDRACE)
	{
		Yes = (Yes * VANILLA_MAX_CLIENTS) / (float)Total;
		No = (No * VANILLA_MAX_CLIENTS) / (float)Total;
		Total = VANILLA_MAX_CLIENTS;
	}

	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::AbortVoteKickOnDisconnect(int ClientId)
{
	if(m_VoteCloseTime && ((str_startswith(m_aVoteCommand, "kick ") && str_toint(&m_aVoteCommand[5]) == ClientId) ||
				      (str_startswith(m_aVoteCommand, "set_team ") && str_toint(&m_aVoteCommand[9]) == ClientId)))
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

void CGameContext::SendTuningParams(int ClientId, int Zone)
{
	if(ClientId == -1)
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
		if(m_apPlayers[ClientId] && m_apPlayers[ClientId]->GetCharacter())
		{
			if((i == 30) // laser_damage is removed from 0.7
				&& (Server()->IsSixup(ClientId)))
			{
				continue;
			}
			else if((i == 31) // collision
				&& (m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOCOLL))
			{
				Msg.AddInt(0);
			}
			else if((i == 32) // hooking
				&& (m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHOOK))
			{
				Msg.AddInt(0);
			}
			else if((i == 3) // ground jump impulse
				&& m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOJUMP)
			{
				Msg.AddInt(0);
			}
			else if((i == 33) // jetpack
				&& !(m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_JETPACK))
			{
				Msg.AddInt(0);
			}
			else if((i == 36) // hammer hit
				&& m_apPlayers[ClientId]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHAMMER)
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
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::OnPreTickTeehistorian()
{
	if(!m_TeeHistorianActive)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] != nullptr)
			m_TeeHistorian.RecordPlayerTeam(i, GetDDRaceTeam(i));
		else
			m_TeeHistorian.RecordPlayerTeam(i, 0);
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_TeeHistorian.RecordTeamPractice(i, m_pController->Teams().IsPractice(i));
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

	UpdatePlayerMaps();

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
			SendChat(-1, TEAM_ALL, "Vote aborted");
			EndVote();
		}
		else if(m_VoteEnforce == VOTE_ENFORCE_CANCEL)
		{
			char aBuf[64];
			if(m_VoteCreator == -1)
			{
				str_copy(aBuf, "Vote canceled");
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "'%s' canceled their vote", Server()->ClientName(m_VoteCreator));
			}
			SendChat(-1, TEAM_ALL, aBuf);
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			bool Veto = false, VetoStop = false;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}}, *pIp = NULL;
				bool SinglePlayer = true;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(m_apPlayers[i])
					{
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
						if(!pIp)
							pIp = aaBuf[i];
						else if(SinglePlayer && str_comp(pIp, aaBuf[i]))
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

					if(m_apPlayers[i]->IsAfk() && i != m_VoteCreator)
						continue;

					// can't vote in kick and spec votes in the beginning after joining
					if((IsKickVote() || IsSpecVote()) && Now < m_apPlayers[i]->m_FirstVoteTick)
						continue;

					// connecting clients with spoofed ips can clog slots without being ingame
					if(!Server()->ClientIngame(i))
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

							if(m_apPlayers[j] && !m_apPlayers[j]->IsAfk() && m_apPlayers[j]->GetTeam() != TEAM_SPECTATORS &&
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
				Server()->SetRconCid(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCid(IServer::RCON_CID_SERV);
				EndVote();
				SendChat(-1, TEAM_ALL, "Vote passed", -1, FLAG_SIX);

				if(m_VoteCreator != -1 && m_apPlayers[m_VoteCreator] && !IsKickVote() && !IsSpecVote())
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			{
				Console()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				SendChat(-1, TEAM_ALL, "Vote passed enforced by authorized player", -1, FLAG_SIX);
				EndVote();
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			{
				EndVote();
				SendChat(-1, TEAM_ALL, "Vote failed enforced by authorized player", -1, FLAG_SIX);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (time_get() > m_VoteCloseTime && g_Config.m_SvVoteMajority))
			{
				EndVote();
				if(VetoStop || (m_VoteWillPass && Veto))
					SendChat(-1, TEAM_ALL, "Vote failed because of veto. Find an empty server instead", -1, FLAG_SIX);
				else
					SendChat(-1, TEAM_ALL, "Vote failed", -1, FLAG_SIX);
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
		const char *pLine = Server()->GetAnnouncementLine();
		if(pLine)
			SendChat(-1, TEAM_ALL, pLine);
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
			if(m_SqlRandomMapResult->m_ClientId != -1 && m_apPlayers[m_SqlRandomMapResult->m_ClientId] && m_SqlRandomMapResult->m_aMessage[0] != '\0')
				SendChat(-1, TEAM_ALL, m_SqlRandomMapResult->m_aMessage);
			if(m_SqlRandomMapResult->m_aMap[0] != '\0')
				Server()->ChangeMap(m_SqlRandomMapResult->m_aMap);
			else
				m_LastMapVote = 0;
		}
		m_SqlRandomMapResult = nullptr;
	}

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

// Server hooks
void CGameContext::OnClientPrepareInput(int ClientId, void *pInput)
{
	auto *pPlayerInput = (CNetObj_PlayerInput *)pInput;
	if(Server()->IsSixup(ClientId))
		pPlayerInput->m_PlayerFlags = PlayerFlags_SevenToSix(pPlayerInput->m_PlayerFlags);
}

void CGameContext::OnClientDirectInput(int ClientId, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientId]->OnDirectInput((CNetObj_PlayerInput *)pInput);

	int Flags = ((CNetObj_PlayerInput *)pInput)->m_PlayerFlags;
	if((Flags & 256) || (Flags & 512))
	{
		Server()->Kick(ClientId, "please update your client or use DDNet client");
	}
}

void CGameContext::OnClientPredictedInput(int ClientId, void *pInput)
{
	// early return if no input at all has been sent by a player
	if(pInput == nullptr && !m_aPlayerHasInput[ClientId])
		return;

	// set to last sent input when no new input has been sent
	CNetObj_PlayerInput *pApplyInput = (CNetObj_PlayerInput *)pInput;
	if(pApplyInput == nullptr)
	{
		pApplyInput = &m_aLastPlayerInput[ClientId];
	}

	if(!m_World.m_Paused)
		m_apPlayers[ClientId]->OnPredictedInput(pApplyInput);
}

void CGameContext::OnClientPredictedEarlyInput(int ClientId, void *pInput)
{
	// early return if no input at all has been sent by a player
	if(pInput == nullptr && !m_aPlayerHasInput[ClientId])
		return;

	// set to last sent input when no new input has been sent
	CNetObj_PlayerInput *pApplyInput = (CNetObj_PlayerInput *)pInput;
	if(pApplyInput == nullptr)
	{
		pApplyInput = &m_aLastPlayerInput[ClientId];
	}
	else
	{
		// Store input in this function and not in `OnClientPredictedInput`,
		// because this function is called on all inputs, while
		// `OnClientPredictedInput` is only called on the first input of each
		// tick.
		mem_copy(&m_aLastPlayerInput[ClientId], pApplyInput, sizeof(m_aLastPlayerInput[ClientId]));
		m_aPlayerHasInput[ClientId] = true;
	}

	if(!m_World.m_Paused)
		m_apPlayers[ClientId]->OnPredictedEarlyInput(pApplyInput);

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerInput(ClientId, m_apPlayers[ClientId]->GetUniqueCid(), pApplyInput);
	}
}

const CVoteOptionServer *CGameContext::GetVoteOption(int Index) const
{
	const CVoteOptionServer *pCurrent;
	for(pCurrent = m_pVoteOptionFirst;
		Index > 0 && pCurrent;
		Index--, pCurrent = pCurrent->m_pNext)
		;

	if(Index > 0)
		return 0;
	return pCurrent;
}

void CGameContext::ProgressVoteOptions(int ClientId)
{
	CPlayer *pPl = m_apPlayers[ClientId];

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
	const CVoteOptionServer *pCurrent = GetVoteOption(pPl->m_SendVoteIndex);

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
	if(pPl->m_SendVoteIndex == 0)
	{
		CNetMsg_Sv_VoteOptionGroupStart StartMsg;
		Server()->SendPackMsg(&StartMsg, MSGFLAG_VITAL, ClientId);
	}

	OptionMsg.m_NumOptions = NumVotesToSend;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientId);

	pPl->m_SendVoteIndex += NumVotesToSend;

	if(pPl->m_SendVoteIndex == m_NumVoteOptions)
	{
		CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
		Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientId);
	}
}

void CGameContext::OnClientEnter(int ClientId)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerReady(ClientId);
	}
	m_pController->OnPlayerConnect(m_apPlayers[ClientId]);

	{
		CNetMsg_Sv_CommandInfoGroupStart Msg;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}
	for(const IConsole::CCommandInfo *pCmd = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT);
		pCmd; pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT))
	{
		const char *pName = pCmd->m_pName;

		if(Server()->IsSixup(ClientId))
		{
			if(!str_comp_nocase(pName, "w") || !str_comp_nocase(pName, "whisper"))
				continue;

			if(!str_comp_nocase(pName, "r"))
				pName = "rescue";

			protocol7::CNetMsg_Sv_CommandInfo Msg;
			Msg.m_pName = pName;
			Msg.m_pArgsFormat = pCmd->m_pParams;
			Msg.m_pHelpText = pCmd->m_pHelp;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
		}
		else
		{
			CNetMsg_Sv_CommandInfo Msg;
			Msg.m_pName = pName;
			Msg.m_pArgsFormat = pCmd->m_pParams;
			Msg.m_pHelpText = pCmd->m_pHelp;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
		}
	}
	{
		CNetMsg_Sv_CommandInfoGroupEnd Msg;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}

	{
		int Empty = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(Server()->ClientSlotEmpty(i))
			{
				Empty = i;
				break;
			}
		}
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientId = Empty;
		Msg.m_pMessage = "Do you know someone who uses a bot? Please report them to the moderators.";
		m_apPlayers[ClientId]->m_EligibleForFinishCheck = time_get();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}

	IServer::CClientInfo Info;
	if(Server()->GetClientInfo(ClientId, &Info) && Info.m_GotDDNetVersion)
	{
		if(OnClientDDNetVersionKnown(ClientId))
			return; // kicked
	}

	if(!Server()->ClientPrevIngame(ClientId))
	{
		if(g_Config.m_SvWelcome[0] != 0)
			SendChatTarget(ClientId, g_Config.m_SvWelcome);

		if(g_Config.m_SvShowOthersDefault > SHOW_OTHERS_OFF)
		{
			if(g_Config.m_SvShowOthers)
				SendChatTarget(ClientId, "You can see other players. To disable this use DDNet client and type /showothers");

			m_apPlayers[ClientId]->m_ShowOthers = g_Config.m_SvShowOthersDefault;
		}
	}
	m_VoteUpdate = true;

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientId);

	Server()->ExpireServerInfo();

	CPlayer *pNewPlayer = m_apPlayers[ClientId];
	mem_zero(&m_aLastPlayerInput[ClientId], sizeof(m_aLastPlayerInput[ClientId]));
	m_aPlayerHasInput[ClientId] = false;

	// new info for others
	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientId = ClientId;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = pNewPlayer->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientId);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientId);
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientId);
	NewClientInfoMsg.m_Silent = false;

	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = pNewPlayer->m_TeeInfos.m_apSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = pNewPlayer->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = pNewPlayer->m_TeeInfos.m_aSkinPartColors[p];
	}

	// update client infos (others before local)
	for(int i = 0; i < Server()->MaxClients(); ++i)
	{
		if(i == ClientId || !m_apPlayers[i] || !Server()->ClientIngame(i))
			continue;

		CPlayer *pPlayer = m_apPlayers[i];

		if(Server()->IsSixup(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);

		if(Server()->IsSixup(ClientId))
		{
			// existing infos for new player
			protocol7::CNetMsg_Sv_ClientInfo ClientInfoMsg;
			ClientInfoMsg.m_ClientId = i;
			ClientInfoMsg.m_Local = 0;
			ClientInfoMsg.m_Team = pPlayer->GetTeam();
			ClientInfoMsg.m_pName = Server()->ClientName(i);
			ClientInfoMsg.m_pClan = Server()->ClientClan(i);
			ClientInfoMsg.m_Country = Server()->ClientCountry(i);
			ClientInfoMsg.m_Silent = 0;

			for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
			{
				ClientInfoMsg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
				ClientInfoMsg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				ClientInfoMsg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
			}

			Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
		}
	}

	// local info
	if(Server()->IsSixup(ClientId))
	{
		NewClientInfoMsg.m_Local = 1;
		Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}

	// initial chat delay
	if(g_Config.m_SvChatInitialDelay != 0 && m_apPlayers[ClientId]->m_JoinTick > m_NonEmptySince + 10 * Server()->TickSpeed())
	{
		char aBuf[128];
		NETADDR Addr;
		Server()->GetClientAddr(ClientId, &Addr);
		str_format(aBuf, sizeof(aBuf), "This server has an initial chat delay, you will need to wait %d seconds before talking.", g_Config.m_SvChatInitialDelay);
		SendChatTarget(ClientId, aBuf);
		Mute(&Addr, g_Config.m_SvChatInitialDelay, Server()->ClientName(ClientId), "Initial chat delay", true);
	}

	LogEvent("Connect", ClientId);
}

bool CGameContext::OnClientDataPersist(int ClientId, void *pData)
{
	CPersistentClientData *pPersistent = (CPersistentClientData *)pData;
	if(!m_apPlayers[ClientId])
	{
		return false;
	}
	pPersistent->m_IsSpectator = m_apPlayers[ClientId]->GetTeam() == TEAM_SPECTATORS;
	pPersistent->m_IsAfk = m_apPlayers[ClientId]->IsAfk();
	return true;
}

void CGameContext::OnClientConnected(int ClientId, void *pData)
{
	CPersistentClientData *pPersistentData = (CPersistentClientData *)pData;
	bool Spec = false;
	bool Afk = true;
	if(pPersistentData)
	{
		Spec = pPersistentData->m_IsSpectator;
		Afk = pPersistentData->m_IsAfk;
	}

	{
		bool Empty = true;
		for(auto &pPlayer : m_apPlayers)
		{
			// connecting clients with spoofed ips can clog slots without being ingame
			if(pPlayer && Server()->ClientIngame(pPlayer->GetCid()))
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
	const int StartTeam = (Spec || g_Config.m_SvTournamentMode) ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientId);

	if(m_apPlayers[ClientId])
		delete m_apPlayers[ClientId];
	m_apPlayers[ClientId] = new(ClientId) CPlayer(this, NextUniqueClientId, ClientId, StartTeam);
	m_apPlayers[ClientId]->SetInitialAfk(Afk);
	NextUniqueClientId += 1;

	SendMotd(ClientId);
	SendSettings(ClientId);

	Server()->ExpireServerInfo();
}

void CGameContext::OnClientDrop(int ClientId, const char *pReason)
{
	LogEvent("Disconnect", ClientId);

	AbortVoteKickOnDisconnect(ClientId);
	m_pController->OnPlayerDisconnect(m_apPlayers[ClientId], pReason);
	delete m_apPlayers[ClientId];
	m_apPlayers[ClientId] = 0;

	delete m_apSavedTeams[ClientId];
	m_apSavedTeams[ClientId] = nullptr;

	delete m_apSavedTees[ClientId];
	m_apSavedTees[ClientId] = nullptr;

	delete m_apSavedTeleTees[ClientId];
	m_apSavedTeleTees[ClientId] = nullptr;

	m_aTeamMapping[ClientId] = -1;

	m_VoteUpdate = true;
	if(m_VoteCreator == ClientId)
	{
		m_VoteCreator = -1;
	}

	// update spectator modes
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_SpectatorId == ClientId)
			pPlayer->m_SpectatorId = SPEC_FREEVIEW;
	}

	// update conversation targets
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_LastWhisperTo == ClientId)
			pPlayer->m_LastWhisperTo = -1;
	}

	protocol7::CNetMsg_Sv_ClientDrop Msg;
	Msg.m_ClientId = ClientId;
	Msg.m_pReason = pReason;
	Msg.m_Silent = false;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	Server()->ExpireServerInfo();
}

void CGameContext::TeehistorianRecordAntibot(const void *pData, int DataSize)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordAntibot(pData, DataSize);
	}
}

void CGameContext::TeehistorianRecordPlayerJoin(int ClientId, bool Sixup)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerJoin(ClientId, !Sixup ? CTeeHistorian::PROTOCOL_6 : CTeeHistorian::PROTOCOL_7);
	}
}

void CGameContext::TeehistorianRecordPlayerDrop(int ClientId, const char *pReason)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerDrop(ClientId, pReason);
	}
}

void CGameContext::TeehistorianRecordPlayerRejoin(int ClientId)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerRejoin(ClientId);
	}
}

void CGameContext::TeehistorianRecordPlayerName(int ClientId, const char *pName)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerName(ClientId, pName);
	}
}

void CGameContext::TeehistorianRecordPlayerFinish(int ClientId, int TimeTicks)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerFinish(ClientId, TimeTicks);
	}
}

void CGameContext::TeehistorianRecordTeamFinish(int TeamId, int TimeTicks)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordTeamFinish(TeamId, TimeTicks);
	}
}

bool CGameContext::OnClientDDNetVersionKnown(int ClientId)
{
	IServer::CClientInfo Info;
	dbg_assert(Server()->GetClientInfo(ClientId, &Info), "failed to get client info");
	int ClientVersion = Info.m_DDNetVersion;
	dbg_msg("ddnet", "cid=%d version=%d", ClientId, ClientVersion);

	if(m_TeeHistorianActive)
	{
		if(Info.m_pConnectionId && Info.m_pDDNetVersionStr)
		{
			m_TeeHistorian.RecordDDNetVersion(ClientId, *Info.m_pConnectionId, ClientVersion, Info.m_pDDNetVersionStr);
		}
		else
		{
			m_TeeHistorian.RecordDDNetVersionOld(ClientId, ClientVersion);
		}
	}

	// Autoban known bot versions.
	if(g_Config.m_SvBannedVersions[0] != '\0' && IsVersionBanned(ClientVersion))
	{
		Server()->Kick(ClientId, "unsupported client");
		return true;
	}

	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(ClientVersion >= VERSION_DDNET_GAMETICK)
		pPlayer->m_TimerType = g_Config.m_SvDefaultTimerType;

	// First update the teams state.
	m_pController->Teams().SendTeamsState(ClientId);

	// Then send records.
	SendRecord(ClientId);

	// And report correct tunings.
	if(ClientVersion < VERSION_DDNET_EARLY_VERSION)
		SendTuningParams(ClientId, pPlayer->m_TuneZone);

	// Tell old clients to update.
	if(ClientVersion < VERSION_DDNET_UPDATER_FIXED && g_Config.m_SvClientSuggestionOld[0] != '\0')
		SendBroadcast(g_Config.m_SvClientSuggestionOld, ClientId);
	// Tell known bot clients that they're botting and we know it.
	if(((ClientVersion >= 15 && ClientVersion < 100) || ClientVersion == 502) && g_Config.m_SvClientSuggestionBot[0] != '\0')
		SendBroadcast(g_Config.m_SvClientSuggestionBot, ClientId);

	return false;
}

bool CheckClientId2(int ClientId)
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS;
}

void *CGameContext::PreProcessMsg(int *pMsgId, CUnpacker *pUnpacker, int ClientId)
{
	if(Server()->IsSixup(ClientId) && *pMsgId < OFFSET_UUID)
	{
		void *pRawMsg = m_NetObjHandler7.SecureUnpackMsg(*pMsgId, pUnpacker);
		if(!pRawMsg)
			return nullptr;

		CPlayer *pPlayer = m_apPlayers[ClientId];
		static char s_aRawMsg[1024];

		if(*pMsgId == protocol7::NETMSGTYPE_CL_SAY)
		{
			protocol7::CNetMsg_Cl_Say *pMsg7 = (protocol7::CNetMsg_Cl_Say *)pRawMsg;
			// Should probably use a placement new to start the lifetime of the object to avoid future weirdness
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			if(pMsg7->m_Mode == protocol7::CHAT_WHISPER)
			{
				if(!CheckClientId2(pMsg7->m_Target) || !Server()->ClientIngame(pMsg7->m_Target))
					return nullptr;
				if(ProcessSpamProtection(ClientId))
					return nullptr;

				WhisperId(ClientId, pMsg7->m_Target, pMsg7->m_pMessage);
				return nullptr;
			}
			else
			{
				pMsg->m_Team = pMsg7->m_Mode == protocol7::CHAT_TEAM;
				pMsg->m_pMessage = pMsg7->m_pMessage;
			}
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_STARTINFO)
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
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_SKINCHANGE)
		{
			protocol7::CNetMsg_Cl_SkinChange *pMsg = (protocol7::CNetMsg_Cl_SkinChange *)pRawMsg;
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo &&
				pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
				return nullptr;

			pPlayer->m_LastChangeInfo = Server()->Tick();

			CTeeInfo Info(pMsg->m_apSkinPartNames, pMsg->m_aUseCustomColors, pMsg->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			protocol7::CNetMsg_Sv_SkinChange Msg;
			Msg.m_ClientId = ClientId;
			for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
			{
				Msg.m_apSkinPartNames[p] = pMsg->m_apSkinPartNames[p];
				Msg.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
				Msg.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
			}

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

			return nullptr;
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			protocol7::CNetMsg_Cl_SetSpectatorMode *pMsg7 = (protocol7::CNetMsg_Cl_SetSpectatorMode *)pRawMsg;
			::CNetMsg_Cl_SetSpectatorMode *pMsg = (::CNetMsg_Cl_SetSpectatorMode *)s_aRawMsg;

			if(pMsg7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				pMsg->m_SpectatorId = SPEC_FREEVIEW;
			else if(pMsg7->m_SpecMode == protocol7::SPEC_PLAYER)
				pMsg->m_SpectatorId = pMsg7->m_SpectatorId;
			else
				pMsg->m_SpectatorId = SPEC_FREEVIEW; // Probably not needed
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_SETTEAM)
		{
			protocol7::CNetMsg_Cl_SetTeam *pMsg7 = (protocol7::CNetMsg_Cl_SetTeam *)pRawMsg;
			::CNetMsg_Cl_SetTeam *pMsg = (::CNetMsg_Cl_SetTeam *)s_aRawMsg;

			pMsg->m_Team = pMsg7->m_Team;
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_COMMAND)
		{
			protocol7::CNetMsg_Cl_Command *pMsg7 = (protocol7::CNetMsg_Cl_Command *)pRawMsg;
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			str_format(s_aRawMsg + sizeof(*pMsg), sizeof(s_aRawMsg) - sizeof(*pMsg), "/%s %s", pMsg7->m_pName, pMsg7->m_pArguments);
			pMsg->m_pMessage = s_aRawMsg + sizeof(*pMsg);
			pMsg->m_Team = 0;

			*pMsgId = NETMSGTYPE_CL_SAY;
			return s_aRawMsg;
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_CALLVOTE)
		{
			protocol7::CNetMsg_Cl_CallVote *pMsg7 = (protocol7::CNetMsg_Cl_CallVote *)pRawMsg;
			::CNetMsg_Cl_CallVote *pMsg = (::CNetMsg_Cl_CallVote *)s_aRawMsg;

			int Authed = Server()->GetAuthedState(ClientId);
			if(pMsg7->m_Force)
			{
				str_format(s_aRawMsg, sizeof(s_aRawMsg), "force_vote \"%s\" \"%s\" \"%s\"", pMsg7->m_pType, pMsg7->m_pValue, pMsg7->m_pReason);
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
				Console()->ExecuteLine(s_aRawMsg, ClientId, false);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				return nullptr;
			}

			pMsg->m_pValue = pMsg7->m_pValue;
			pMsg->m_pReason = pMsg7->m_pReason;
			pMsg->m_pType = pMsg7->m_pType;
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_EMOTICON)
		{
			protocol7::CNetMsg_Cl_Emoticon *pMsg7 = (protocol7::CNetMsg_Cl_Emoticon *)pRawMsg;
			::CNetMsg_Cl_Emoticon *pMsg = (::CNetMsg_Cl_Emoticon *)s_aRawMsg;

			pMsg->m_Emoticon = pMsg7->m_Emoticon;
		}
		else if(*pMsgId == protocol7::NETMSGTYPE_CL_VOTE)
		{
			protocol7::CNetMsg_Cl_Vote *pMsg7 = (protocol7::CNetMsg_Cl_Vote *)pRawMsg;
			::CNetMsg_Cl_Vote *pMsg = (::CNetMsg_Cl_Vote *)s_aRawMsg;

			pMsg->m_Vote = pMsg7->m_Vote;
		}

		*pMsgId = Msg_SevenToSix(*pMsgId);

		return s_aRawMsg;
	}
	else
		return m_NetObjHandler.SecureUnpackMsg(*pMsgId, pUnpacker);
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

void CGameContext::OnMessage(int MsgId, CUnpacker *pUnpacker, int ClientId)
{
	if(m_TeeHistorianActive)
	{
		if(m_NetObjHandler.TeeHistorianRecordMsg(MsgId))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientId, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}

	void *pRawMsg = PreProcessMsg(&MsgId, pUnpacker, ClientId);

	if(!pRawMsg)
		return;

	if(Server()->ClientIngame(ClientId))
	{
		switch(MsgId)
		{
		case NETMSGTYPE_CL_SAY:
			OnSayNetMessage(static_cast<CNetMsg_Cl_Say *>(pRawMsg), ClientId, pUnpacker);
			break;
		case NETMSGTYPE_CL_CALLVOTE:
			OnCallVoteNetMessage(static_cast<CNetMsg_Cl_CallVote *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_VOTE:
			OnVoteNetMessage(static_cast<CNetMsg_Cl_Vote *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_SETTEAM:
			OnSetTeamNetMessage(static_cast<CNetMsg_Cl_SetTeam *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_ISDDNETLEGACY:
			OnIsDDNetLegacyNetMessage(static_cast<CNetMsg_Cl_IsDDNetLegacy *>(pRawMsg), ClientId, pUnpacker);
			break;
		case NETMSGTYPE_CL_SHOWOTHERSLEGACY:
			OnShowOthersLegacyNetMessage(static_cast<CNetMsg_Cl_ShowOthersLegacy *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_SHOWOTHERS:
			OnShowOthersNetMessage(static_cast<CNetMsg_Cl_ShowOthers *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_SHOWDISTANCE:
			OnShowDistanceNetMessage(static_cast<CNetMsg_Cl_ShowDistance *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_CAMERAINFO:
			OnCameraInfoNetMessage(static_cast<CNetMsg_Cl_CameraInfo *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_SETSPECTATORMODE:
			OnSetSpectatorModeNetMessage(static_cast<CNetMsg_Cl_SetSpectatorMode *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_CHANGEINFO:
			OnChangeInfoNetMessage(static_cast<CNetMsg_Cl_ChangeInfo *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_EMOTICON:
			OnEmoticonNetMessage(static_cast<CNetMsg_Cl_Emoticon *>(pRawMsg), ClientId);
			break;
		case NETMSGTYPE_CL_KILL:
			OnKillNetMessage(static_cast<CNetMsg_Cl_Kill *>(pRawMsg), ClientId);
			break;
		default:
			break;
		}
	}
	if(MsgId == NETMSGTYPE_CL_STARTINFO)
	{
		OnStartInfoNetMessage(static_cast<CNetMsg_Cl_StartInfo *>(pRawMsg), ClientId);
	}
}

void CGameContext::OnSayNetMessage(const CNetMsg_Cl_Say *pMsg, int ClientId, const CUnpacker *pUnpacker)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	bool Check = !pPlayer->m_NotEligibleForFinish && pPlayer->m_EligibleForFinishCheck + 10 * time_freq() >= time_get();
	if(Check && str_comp(pMsg->m_pMessage, "xd sure chillerbot.png is lyfe") == 0 && pMsg->m_Team == 0)
	{
		if(m_TeeHistorianActive)
		{
			m_TeeHistorian.RecordPlayerMessage(ClientId, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}

		pPlayer->m_NotEligibleForFinish = true;
		dbg_msg("hack", "bot detected, cid=%d", ClientId);
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

	int GameTeam = GetDDRaceTeam(pPlayer->GetCid());
	if(Team)
		Team = ((pPlayer->GetTeam() == TEAM_SPECTATORS) ? TEAM_SPECTATORS : GameTeam);
	else
		Team = TEAM_ALL;

	if(pMsg->m_pMessage[0] == '/')
	{
		if(str_startswith_nocase(pMsg->m_pMessage + 1, "w "))
		{
			char aWhisperMsg[256];
			str_copy(aWhisperMsg, pMsg->m_pMessage + sizeof("w "));
			Whisper(pPlayer->GetCid(), aWhisperMsg);
		}
		else if(str_startswith_nocase(pMsg->m_pMessage + 1, "whisper "))
		{
			char aWhisperMsg[256];
			str_copy(aWhisperMsg, pMsg->m_pMessage + sizeof("whisper "));
			Whisper(pPlayer->GetCid(), aWhisperMsg);
		}
		else if(str_startswith_nocase(pMsg->m_pMessage + 1, "c "))
		{
			char aWhisperMsg[256];
			str_copy(aWhisperMsg, pMsg->m_pMessage + sizeof("c "));
			Converse(pPlayer->GetCid(), aWhisperMsg);
		}
		else if(str_startswith_nocase(pMsg->m_pMessage + 1, "converse "))
		{
			char aWhisperMsg[256];
			str_copy(aWhisperMsg, pMsg->m_pMessage + sizeof("converse "));
			Converse(pPlayer->GetCid(), aWhisperMsg);
		}
		else
		{
			if(g_Config.m_SvSpamprotection && !str_startswith(pMsg->m_pMessage + 1, "timeout ") && pPlayer->m_aLastCommands[0] && pPlayer->m_aLastCommands[0] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[1] && pPlayer->m_aLastCommands[1] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[2] && pPlayer->m_aLastCommands[2] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_aLastCommands[3] && pPlayer->m_aLastCommands[3] + Server()->TickSpeed() > Server()->Tick())
				return;

			int64_t Now = Server()->Tick();
			pPlayer->m_aLastCommands[pPlayer->m_LastCommandPos] = Now;
			pPlayer->m_LastCommandPos = (pPlayer->m_LastCommandPos + 1) % 4;

			Console()->SetFlagMask(CFGFLAG_CHAT);
			int Authed = Server()->GetAuthedState(ClientId);
			if(Authed)
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
			else
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);

			{
				CClientChatLogger Logger(this, ClientId, log_get_scope_logger());
				CLogScope Scope(&Logger);
				Console()->ExecuteLine(pMsg->m_pMessage + 1, ClientId, false);
			}
			// m_apPlayers[ClientId] can be NULL, if the player used a
			// timeout code and replaced another client.
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%d used %s", ClientId, pMsg->m_pMessage);
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
		SendChat(ClientId, Team, aCensoredMessage, ClientId);
	}
}

void CGameContext::OnCallVoteNetMessage(const CNetMsg_Cl_CallVote *pMsg, int ClientId)
{
	if(RateLimitPlayerVote(ClientId) || m_VoteCloseTime)
		return;

	m_apPlayers[ClientId]->UpdatePlaytime();

	m_VoteType = VOTE_TYPE_UNKNOWN;
	char aChatmsg[512] = {0};
	char aDesc[VOTE_DESC_LENGTH] = {0};
	char aSixupDesc[VOTE_DESC_LENGTH] = {0};
	char aCmd[VOTE_CMD_LENGTH] = {0};
	char aReason[VOTE_REASON_LENGTH] = "No reason given";
	if(pMsg->m_pReason[0])
	{
		str_copy(aReason, pMsg->m_pReason, sizeof(aReason));
	}
	int Authed = Server()->GetAuthedState(ClientId);

	if(str_comp_nocase(pMsg->m_pType, "option") == 0)
	{
		CVoteOptionServer *pOption = m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pMsg->m_pValue, pOption->m_aDescription) == 0)
			{
				if(!Console()->LineIsValid(pOption->m_aCommand))
				{
					SendChatTarget(ClientId, "Invalid option");
					return;
				}
				if((str_find(pOption->m_aCommand, "sv_map ") != 0 || str_find(pOption->m_aCommand, "change_map ") != 0 || str_find(pOption->m_aCommand, "random_map") != 0 || str_find(pOption->m_aCommand, "random_unfinished_map") != 0) && RateLimitPlayerMapVote(ClientId))
				{
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientId),
					pOption->m_aDescription, aReason);
				str_copy(aDesc, pOption->m_aDescription);

				if((str_endswith(pOption->m_aCommand, "random_map") || str_endswith(pOption->m_aCommand, "random_unfinished_map")) && str_length(aReason) == 1 && aReason[0] >= '0' && aReason[0] <= '5')
				{
					int Stars = aReason[0] - '0';
					str_format(aCmd, sizeof(aCmd), "%s %d", pOption->m_aCommand, Stars);
				}
				else
				{
					str_copy(aCmd, pOption->m_aCommand);
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
				SendChatTarget(ClientId, aChatmsg);
				return;
			}
			else
			{
				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientId), pMsg->m_pValue);
				str_copy(aDesc, pMsg->m_pValue);
				str_copy(aCmd, pMsg->m_pValue);
			}
		}

		m_VoteType = VOTE_TYPE_OPTION;
	}
	else if(str_comp_nocase(pMsg->m_pType, "kick") == 0)
	{
		if(!g_Config.m_SvVoteKick && !Authed) // allow admins to call kick votes even if they are forbidden
		{
			SendChatTarget(ClientId, "Server does not allow voting to kick players");
			return;
		}
		if(!Authed && time_get() < m_apPlayers[ClientId]->m_Last_KickVote + (time_freq() * g_Config.m_SvVoteKickDelay))
		{
			str_format(aChatmsg, sizeof(aChatmsg), "There's a %d second wait time between kick votes for each player please wait %d second(s)",
				g_Config.m_SvVoteKickDelay,
				(int)((m_apPlayers[ClientId]->m_Last_KickVote + g_Config.m_SvVoteKickDelay * time_freq() - time_get()) / time_freq()));
			SendChatTarget(ClientId, aChatmsg);
			return;
		}

		if(g_Config.m_SvVoteKickMin && !GetDDRaceTeam(ClientId))
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
				SendChatTarget(ClientId, aChatmsg);
				return;
			}
		}

		int KickId = str_toint(pMsg->m_pValue);

		if(KickId < 0 || KickId >= MAX_CLIENTS || !m_apPlayers[KickId])
		{
			SendChatTarget(ClientId, "Invalid client id to kick");
			return;
		}
		if(KickId == ClientId)
		{
			SendChatTarget(ClientId, "You can't kick yourself");
			return;
		}
		if(!Server()->ReverseTranslate(KickId, ClientId))
		{
			return;
		}
		int KickedAuthed = Server()->GetAuthedState(KickId);
		if(KickedAuthed > Authed)
		{
			SendChatTarget(ClientId, "You can't kick authorized players");
			char aBufKick[128];
			str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you", Server()->ClientName(ClientId));
			SendChatTarget(KickId, aBufKick);
			return;
		}

		// Don't allow kicking if a player has no character
		if(!GetPlayerChar(ClientId) || !GetPlayerChar(KickId) || GetDDRaceTeam(ClientId) != GetDDRaceTeam(KickId))
		{
			SendChatTarget(ClientId, "You can kick only your team member");
			return;
		}

		str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientId), Server()->ClientName(KickId), aReason);
		str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", KickId, Server()->ClientName(KickId));
		if(!GetDDRaceTeam(ClientId))
		{
			if(!g_Config.m_SvVoteKickBantime)
			{
				str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickId);
				str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickId));
			}
			else
			{
				char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
				Server()->GetClientAddr(KickId, aAddrStr, sizeof(aAddrStr));
				str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
				str_format(aDesc, sizeof(aDesc), "Ban '%s'", Server()->ClientName(KickId));
			}
		}
		else
		{
			str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team_ddr %d 0", KickId, GetDDRaceTeam(KickId), KickId);
			str_format(aDesc, sizeof(aDesc), "Move '%s' to team 0", Server()->ClientName(KickId));
		}
		m_apPlayers[ClientId]->m_Last_KickVote = time_get();
		m_VoteType = VOTE_TYPE_KICK;
		m_VoteVictim = KickId;
	}
	else if(str_comp_nocase(pMsg->m_pType, "spectate") == 0)
	{
		if(!g_Config.m_SvVoteSpectate)
		{
			SendChatTarget(ClientId, "Server does not allow voting to move players to spectators");
			return;
		}

		int SpectateId = str_toint(pMsg->m_pValue);

		if(SpectateId < 0 || SpectateId >= MAX_CLIENTS || !m_apPlayers[SpectateId] || m_apPlayers[SpectateId]->GetTeam() == TEAM_SPECTATORS)
		{
			SendChatTarget(ClientId, "Invalid client id to move to spectators");
			return;
		}
		if(SpectateId == ClientId)
		{
			SendChatTarget(ClientId, "You can't move yourself to spectators");
			return;
		}
		int SpectateAuthed = Server()->GetAuthedState(SpectateId);
		if(SpectateAuthed > Authed)
		{
			SendChatTarget(ClientId, "You can't move authorized players to spectators");
			char aBufSpectate[128];
			str_format(aBufSpectate, sizeof(aBufSpectate), "'%s' called for vote to move you to spectators", Server()->ClientName(ClientId));
			SendChatTarget(SpectateId, aBufSpectate);
			return;
		}
		if(!Server()->ReverseTranslate(SpectateId, ClientId))
		{
			return;
		}

		if(!GetPlayerChar(ClientId) || !GetPlayerChar(SpectateId) || GetDDRaceTeam(ClientId) != GetDDRaceTeam(SpectateId))
		{
			SendChatTarget(ClientId, "You can only move your team member to spectators");
			return;
		}

		str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", SpectateId, Server()->ClientName(SpectateId));
		if(g_Config.m_SvPauseable && g_Config.m_SvVotePause)
		{
			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to pause '%s' for %d seconds (%s)", Server()->ClientName(ClientId), Server()->ClientName(SpectateId), g_Config.m_SvVotePauseTime, aReason);
			str_format(aDesc, sizeof(aDesc), "Pause '%s' (%ds)", Server()->ClientName(SpectateId), g_Config.m_SvVotePauseTime);
			str_format(aCmd, sizeof(aCmd), "uninvite %d %d; force_pause %d %d", SpectateId, GetDDRaceTeam(SpectateId), SpectateId, g_Config.m_SvVotePauseTime);
		}
		else
		{
			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientId), Server()->ClientName(SpectateId), aReason);
			str_format(aDesc, sizeof(aDesc), "Move '%s' to spectators", Server()->ClientName(SpectateId));
			str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team %d -1 %d", SpectateId, GetDDRaceTeam(SpectateId), SpectateId, g_Config.m_SvVoteSpectateRejoindelay);
		}
		m_VoteType = VOTE_TYPE_SPECTATE;
		m_VoteVictim = SpectateId;
	}

	if(aCmd[0] && str_comp_nocase(aCmd, "info") != 0)
		CallVote(ClientId, aDesc, aCmd, aReason, aChatmsg, aSixupDesc[0] ? aSixupDesc : 0);
}

void CGameContext::OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId)
{
	if(!m_VoteCloseTime)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 3 > Server()->Tick())
		return;

	pPlayer->m_LastVoteTry = Server()->Tick();
	pPlayer->UpdatePlaytime();

	if(!pMsg->m_Vote)
		return;

	// Allow the vote creator to cancel the vote
	if(pPlayer->GetCid() == m_VoteCreator && pMsg->m_Vote == -1)
	{
		m_VoteEnforce = VOTE_ENFORCE_CANCEL;
		return;
	}

	pPlayer->m_Vote = pMsg->m_Vote;
	pPlayer->m_VotePos = ++m_VotePos;
	m_VoteUpdate = true;

	CNetMsg_Sv_YourVote Msg = {pMsg->m_Vote};
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId)
{
	if(m_World.m_Paused)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay > Server()->Tick()))
		return;

	// Kill Protection
	CCharacter *pChr = pPlayer->GetCharacter();
	if(pChr)
	{
		int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
		if(g_Config.m_SvKillProtection != 0 && CurrTime >= (60 * g_Config.m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
		{
			SendChatTarget(ClientId, "Kill Protection enabled. If you really want to join the spectators, first type /kill");
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
		SendBroadcast(aBuf, ClientId);
		return;
	}

	// Switch team on given client and kill/respawn them
	char aTeamJoinError[512];
	if(m_pController->CanJoinTeam(pMsg->m_Team, ClientId, aTeamJoinError, sizeof(aTeamJoinError)))
	{
		if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
			m_VoteUpdate = true;
		m_pController->DoTeamChange(pPlayer, pMsg->m_Team);
		pPlayer->m_TeamChangeTick = Server()->Tick();
	}
	else
		SendBroadcast(aTeamJoinError, ClientId);
}

void CGameContext::OnIsDDNetLegacyNetMessage(const CNetMsg_Cl_IsDDNetLegacy *pMsg, int ClientId, CUnpacker *pUnpacker)
{
	IServer::CClientInfo Info;
	if(Server()->GetClientInfo(ClientId, &Info) && Info.m_GotDDNetVersion)
	{
		return;
	}
	int DDNetVersion = pUnpacker->GetInt();
	if(pUnpacker->Error() || DDNetVersion < 0)
	{
		DDNetVersion = VERSION_DDRACE;
	}
	Server()->SetClientDDNetVersion(ClientId, DDNetVersion);
	OnClientDDNetVersionKnown(ClientId);
}

void CGameContext::OnShowOthersLegacyNetMessage(const CNetMsg_Cl_ShowOthersLegacy *pMsg, int ClientId)
{
	if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
	{
		CPlayer *pPlayer = m_apPlayers[ClientId];
		pPlayer->m_ShowOthers = pMsg->m_Show;
	}
}

void CGameContext::OnShowOthersNetMessage(const CNetMsg_Cl_ShowOthers *pMsg, int ClientId)
{
	if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
	{
		CPlayer *pPlayer = m_apPlayers[ClientId];
		pPlayer->m_ShowOthers = pMsg->m_Show;
	}
}

void CGameContext::OnShowDistanceNetMessage(const CNetMsg_Cl_ShowDistance *pMsg, int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	pPlayer->m_ShowDistance = vec2(pMsg->m_X, pMsg->m_Y);
}

void CGameContext::OnCameraInfoNetMessage(const CNetMsg_Cl_CameraInfo *pMsg, int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	pPlayer->m_CameraInfo.Write(pMsg);
}

void CGameContext::OnSetSpectatorModeNetMessage(const CNetMsg_Cl_SetSpectatorMode *pMsg, int ClientId)
{
	if(m_World.m_Paused)
		return;

	int SpectatorId = clamp(pMsg->m_SpectatorId, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);
	if(SpectatorId >= 0)
		if(!Server()->ReverseTranslate(SpectatorId, ClientId))
			return;

	CPlayer *pPlayer = m_apPlayers[ClientId];
	if((g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() / 4 > Server()->Tick()))
		return;

	pPlayer->m_LastSetSpectatorMode = Server()->Tick();
	pPlayer->UpdatePlaytime();
	if(SpectatorId >= 0 && (!m_apPlayers[SpectatorId] || m_apPlayers[SpectatorId]->GetTeam() == TEAM_SPECTATORS))
		SendChatTarget(ClientId, "Invalid spectator id used");
	else
		pPlayer->m_SpectatorId = SpectatorId;
}

void CGameContext::OnChangeInfoNetMessage(const CNetMsg_Cl_ChangeInfo *pMsg, int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
		return;

	bool SixupNeedsUpdate = false;

	pPlayer->m_LastChangeInfo = Server()->Tick();
	pPlayer->UpdatePlaytime();

	if(g_Config.m_SvSpamprotection)
	{
		CNetMsg_Sv_ChangeInfoCooldown ChangeInfoCooldownMsg;
		ChangeInfoCooldownMsg.m_WaitUntil = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay;
		Server()->SendPackMsg(&ChangeInfoCooldownMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}

	// set infos
	if(Server()->WouldClientNameChange(ClientId, pMsg->m_pName) && !ProcessSpamProtection(ClientId))
	{
		char aOldName[MAX_NAME_LENGTH];
		str_copy(aOldName, Server()->ClientName(ClientId), sizeof(aOldName));

		Server()->SetClientName(ClientId, pMsg->m_pName);

		char aChatText[256];
		str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientId));
		SendChat(-1, TEAM_ALL, aChatText);

		// reload scores
		Score()->PlayerData(ClientId)->Reset();
		m_apPlayers[ClientId]->m_Score.reset();
		Score()->LoadPlayerData(ClientId);

		SixupNeedsUpdate = true;

		LogEvent("Name change", ClientId);
	}

	if(Server()->WouldClientClanChange(ClientId, pMsg->m_pClan))
	{
		SixupNeedsUpdate = true;
		Server()->SetClientClan(ClientId, pMsg->m_pClan);
	}

	if(Server()->ClientCountry(ClientId) != pMsg->m_Country)
		SixupNeedsUpdate = true;
	Server()->SetClientCountry(ClientId, pMsg->m_Country);

	str_copy(pPlayer->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_aSkinName));
	pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
	pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
	pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
	if(!Server()->IsSixup(ClientId))
		pPlayer->m_TeeInfos.ToSixup();

	if(SixupNeedsUpdate)
	{
		protocol7::CNetMsg_Sv_ClientDrop Drop;
		Drop.m_ClientId = ClientId;
		Drop.m_pReason = "";
		Drop.m_Silent = true;

		protocol7::CNetMsg_Sv_ClientInfo Info;
		Info.m_ClientId = ClientId;
		Info.m_pName = Server()->ClientName(ClientId);
		Info.m_Country = pMsg->m_Country;
		Info.m_pClan = pMsg->m_pClan;
		Info.m_Local = 0;
		Info.m_Silent = true;
		Info.m_Team = pPlayer->GetTeam();

		for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
		{
			Info.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
			Info.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
			Info.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
		}

		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(i != ClientId)
			{
				Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
				Server()->SendPackMsg(&Info, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
			}
		}
	}
	else
	{
		protocol7::CNetMsg_Sv_SkinChange Msg;
		Msg.m_ClientId = ClientId;
		for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
		{
			Msg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
			Msg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
			Msg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
		}

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
	}

	Server()->ExpireServerInfo();
}

void CGameContext::OnEmoticonNetMessage(const CNetMsg_Cl_Emoticon *pMsg, int ClientId)
{
	if(m_World.m_Paused)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientId];

	auto &&CheckPreventEmote = [&](int64_t LastEmote, int64_t DelayInMs) {
		return (LastEmote * (int64_t)1000) + (int64_t)Server()->TickSpeed() * DelayInMs > ((int64_t)Server()->Tick() * (int64_t)1000);
	};

	if(g_Config.m_SvSpamprotection && CheckPreventEmote((int64_t)pPlayer->m_LastEmote, (int64_t)g_Config.m_SvEmoticonMsDelay))
		return;

	CCharacter *pChr = pPlayer->GetCharacter();

	// player needs a character to send emotes
	if(!pChr)
		return;

	pPlayer->m_LastEmote = Server()->Tick();
	pPlayer->UpdatePlaytime();

	// check if the global emoticon is prevented and emotes are only send to nearby players
	if(g_Config.m_SvSpamprotection && CheckPreventEmote((int64_t)pPlayer->m_LastEmoteGlobal, (int64_t)g_Config.m_SvGlobalEmoticonMsDelay))
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_apPlayers[i] && pChr->CanSnapCharacter(i) && pChr->IsSnappingCharacterInView(i))
			{
				SendEmoticon(ClientId, pMsg->m_Emoticon, i);
			}
		}
	}
	else
	{
		// else send emoticons to all players
		pPlayer->m_LastEmoteGlobal = Server()->Tick();
		SendEmoticon(ClientId, pMsg->m_Emoticon, -1);
	}

	if(g_Config.m_SvEmotionalTees == 1 && pPlayer->m_EyeEmoteEnabled)
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

void CGameContext::OnKillNetMessage(const CNetMsg_Cl_Kill *pMsg, int ClientId)
{
	if(m_World.m_Paused)
		return;

	if(m_VoteCloseTime && m_VoteCreator == ClientId && GetDDRaceTeam(ClientId) && (IsKickVote() || IsSpecVote()))
	{
		SendChatTarget(ClientId, "You are running a vote please try again after the vote is done!");
		return;
	}
	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * g_Config.m_SvKillDelay > Server()->Tick())
		return;
	if(pPlayer->IsPaused())
		return;

	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	// Kill Protection
	int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
	if(g_Config.m_SvKillProtection != 0 && CurrTime >= (60 * g_Config.m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
	{
		SendChatTarget(ClientId, "Kill Protection enabled. If you really want to kill, type /kill");
		return;
	}

	pPlayer->m_LastKill = Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	pPlayer->Respawn();
}

void CGameContext::OnStartInfoNetMessage(const CNetMsg_Cl_StartInfo *pMsg, int ClientId)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(pPlayer->m_IsReady)
		return;

	pPlayer->m_LastChangeInfo = Server()->Tick();

	// set start infos
	Server()->SetClientName(ClientId, pMsg->m_pName);
	// trying to set client name can delete the player object, check if it still exists
	if(!m_apPlayers[ClientId])
	{
		return;
	}
	Server()->SetClientClan(ClientId, pMsg->m_pClan);
	// trying to set client clan can delete the player object, check if it still exists
	if(!m_apPlayers[ClientId])
	{
		return;
	}
	Server()->SetClientCountry(ClientId, pMsg->m_Country);
	str_copy(pPlayer->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_aSkinName));
	pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
	pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
	pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
	if(!Server()->IsSixup(ClientId))
		pPlayer->m_TeeInfos.ToSixup();

	// send clear vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientId);

	// begin sending vote options
	pPlayer->m_SendVoteIndex = 0;

	// send tuning parameters to client
	SendTuningParams(ClientId, pPlayer->m_TuneZone);

	// client is ready to enter
	pPlayer->m_IsReady = true;
	CNetMsg_Sv_ReadyToEnter m;
	Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);

	Server()->ExpireServerInfo();
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

	float NewValue = absolute(OldValue - pResult->GetFloat(1)) < 0.0001f ? pResult->GetFloat(2) : pResult->GetFloat(1);

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
	for(int i = 0; i < CTuningParams::Num(); i++)
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
		for(int i = 0; i < CTuningParams::Num(); i++)
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
	pSelf->m_pController->ChangeMap(pResult->GetString(0));
}

void CGameContext::ConRandomMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int ClientId = pResult->m_ClientId == -1 ? pSelf->m_VoteCreator : pResult->m_ClientId;
	const int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;
	pSelf->m_pScore->RandomMap(ClientId, Stars);
}

void CGameContext::ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int ClientId = pResult->m_ClientId == -1 ? pSelf->m_VoteCreator : pResult->m_ClientId;
	const int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;
	pSelf->m_pScore->RandomUnfinishedMap(ClientId, Stars);
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
	pSelf->SendChat(-1, TEAM_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientId = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientId])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientId, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientId]->Pause(CPlayer::PAUSE_NONE, false); // reset /spec and /pause to allow rejoin
	pSelf->m_apPlayers[ClientId]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[ClientId], Team);
	if(Team == TEAM_SPECTATORS)
		pSelf->m_apPlayers[ClientId]->Pause(CPlayer::PAUSE_NONE, true);
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, TEAM_ALL, aBuf);

	for(auto &pPlayer : pSelf->m_apPlayers)
		if(pPlayer)
			pSelf->m_pController->DoTeamChange(pPlayer, Team, false);
}

void CGameContext::ConHotReload(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!pSelf->GetPlayerChar(i))
			continue;

		CCharacter *pChar = pSelf->GetPlayerChar(i);

		// Save the tee individually
		pSelf->m_apSavedTees[i] = new CSaveTee();
		pSelf->m_apSavedTees[i]->Save(pChar, false);

		if(pSelf->m_apPlayers[i])
			pSelf->m_apSavedTeleTees[i] = new CSaveTee(pSelf->m_apPlayers[i]->m_LastTeleTee);

		// Save the team state
		pSelf->m_aTeamMapping[i] = pSelf->GetDDRaceTeam(i);
		if(pSelf->m_aTeamMapping[i] == TEAM_SUPER)
			pSelf->m_aTeamMapping[i] = pChar->m_TeamBeforeSuper;

		if(pSelf->m_apSavedTeams[pSelf->m_aTeamMapping[i]])
			continue;

		pSelf->m_apSavedTeams[pSelf->m_aTeamMapping[i]] = new CSaveTeam();
		pSelf->m_apSavedTeams[pSelf->m_aTeamMapping[i]]->Save(pSelf, pSelf->m_aTeamMapping[i], true, true);
	}
	pSelf->Server()->ReloadMap();
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
	str_copy(pOption->m_aCommand, pCommand, Len + 1);
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
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len, alignof(CVoteOptionServer));
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		str_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
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
				pSelf->SendChatTarget(-1, aBuf, FLAG_SIX);
				pSelf->m_VoteCreator = pResult->m_ClientId;
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
		int KickId = str_toint(pValue);
		if(KickId < 0 || KickId >= MAX_CLIENTS || !pSelf->m_apPlayers[KickId])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if(!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickId, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickId, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateId = str_toint(pValue);
		if(SpectateId < 0 || SpectateId >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateId] || pSelf->m_apPlayers[SpectateId]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "'%s' was moved to spectator (%s)", pSelf->Server()->ClientName(SpectateId), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateId, g_Config.m_SvVoteSpectateRejoindelay);
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
	bool m_IsDirectory;

	static bool CompareFilenameAscending(const CMapNameItem Lhs, const CMapNameItem Rhs)
	{
		if(str_comp(Lhs.m_aName, "..") == 0)
			return true;
		if(str_comp(Rhs.m_aName, "..") == 0)
			return false;
		if(Lhs.m_IsDirectory != Rhs.m_IsDirectory)
			return Lhs.m_IsDirectory;
		return str_comp_filenames(Lhs.m_aName, Rhs.m_aName) < 0;
	}
};

void CGameContext::ConAddMapVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	std::vector<CMapNameItem> vMapList;
	const char *pDirectory = pResult->GetString(0);

	// Don't allow moving to parent directories
	if(str_find_nocase(pDirectory, ".."))
		return;

	char aPath[IO_MAX_PATH_LENGTH] = "maps/";
	str_append(aPath, pDirectory, sizeof(aPath));
	pSelf->Storage()->ListDirectory(IStorage::TYPE_ALL, aPath, MapScan, &vMapList);
	std::sort(vMapList.begin(), vMapList.end(), CMapNameItem::CompareFilenameAscending);

	for(auto &Item : vMapList)
	{
		if(!str_comp(Item.m_aName, "..") && (!str_comp(aPath, "maps/")))
			continue;

		char aDescription[VOTE_DESC_LENGTH];
		str_format(aDescription, sizeof(aDescription), "%s: %s%s", Item.m_IsDirectory ? "Directory" : "Map", Item.m_aName, Item.m_IsDirectory ? "/" : "");

		char aCommand[VOTE_CMD_LENGTH];
		char aOptionEscaped[IO_MAX_PATH_LENGTH * 2];
		char *pDst = aOptionEscaped;
		str_escape(&pDst, Item.m_aName, aOptionEscaped + sizeof(aOptionEscaped));

		char aDirectory[IO_MAX_PATH_LENGTH] = "";
		if(pResult->NumArguments())
			str_copy(aDirectory, pDirectory);

		if(!str_comp(Item.m_aName, ".."))
		{
			fs_parent_dir(aDirectory);
			str_format(aCommand, sizeof(aCommand), "clear_votes; add_map_votes \"%s\"", aDirectory);
		}
		else if(Item.m_IsDirectory)
		{
			str_append(aDirectory, "/", sizeof(aDirectory));
			str_append(aDirectory, aOptionEscaped, sizeof(aDirectory));

			str_format(aCommand, sizeof(aCommand), "clear_votes; add_map_votes \"%s\"", aDirectory);
		}
		else
			str_format(aCommand, sizeof(aCommand), "change_map \"%s%s%s\"", pDirectory, pDirectory[0] == '\0' ? "" : "/", aOptionEscaped);

		pSelf->AddVote(aDescription, aCommand);
	}

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "added maps to votes");
}

int CGameContext::MapScan(const char *pName, int IsDir, int DirType, void *pUserData)
{
	if((!IsDir && !str_endswith(pName, ".map")) || !str_comp(pName, "."))
		return 0;

	CMapNameItem Item;
	Item.m_IsDirectory = IsDir;
	if(!IsDir)
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - str_length(".map"));
	else
		str_copy(Item.m_aName, pName);
	static_cast<std::vector<CMapNameItem> *>(pUserData)->push_back(Item);

	return 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->ForceVote(pResult->m_ClientId, true);
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->ForceVote(pResult->m_ClientId, false);
}

void CGameContext::ConVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Page = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 0;
	static const int s_EntriesPerPage = 20;
	const int Start = Page * s_EntriesPerPage;
	const int End = (Page + 1) * s_EntriesPerPage;

	char aBuf[512];
	int Count = 0;
	for(CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst; pOption; pOption = pOption->m_pNext, Count++)
	{
		if(Count < Start || Count >= End)
		{
			continue;
		}

		str_copy(aBuf, "add_vote \"");
		char *pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pOption->m_aDescription, aBuf + sizeof(aBuf));
		str_append(aBuf, "\" \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pOption->m_aCommand, aBuf + sizeof(aBuf));
		str_append(aBuf, "\"");

		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votes", aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "%d %s, showing entries %d - %d", Count, Count == 1 ? "vote" : "votes", Start, End - 1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votes", aBuf);
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

void CGameContext::ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->SendSettings(-1);
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	Console()->Register("tune", "s[tuning] ?f[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneParam, this, "Tune variable to value or show current value");
	Console()->Register("toggle_tune", "s[tuning] f[value 1] f[value 2]", CFGFLAG_SERVER, ConToggleTuneParam, this, "Toggle tune variable");
	Console()->Register("tune_reset", "?s[tuning]", CFGFLAG_SERVER, ConTuneReset, this, "Reset all or one tuning variable to default");
	Console()->Register("tunes", "", CFGFLAG_SERVER, ConTunes, this, "List all tuning variables and their values");
	Console()->Register("tune_zone", "i[zone] s[tuning] f[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneZone, this, "Tune in zone a variable to value");
	Console()->Register("tune_zone_dump", "i[zone]", CFGFLAG_SERVER, ConTuneDumpZone, this, "Dump zone tuning in zone x");
	Console()->Register("tune_zone_reset", "?i[zone]", CFGFLAG_SERVER, ConTuneResetZone, this, "Reset zone tuning in zone x or in all zones");
	Console()->Register("tune_zone_enter", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgEnter, this, "Which message to display on zone enter; use 0 for normal area");
	Console()->Register("tune_zone_leave", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgLeave, this, "Which message to display on zone leave; use 0 for normal area");
	Console()->Register("mapbug", "s[mapbug]", CFGFLAG_SERVER | CFGFLAG_GAME, ConMapbug, this, "Enable map compatibility mode using the specified bug (example: grenade-doubleexplosion@ddnet.tw)");
	Console()->Register("switch_open", "i[switch]", CFGFLAG_SERVER | CFGFLAG_GAME, ConSwitchOpen, this, "Whether a switch is deactivated by default (otherwise activated)");
	Console()->Register("pause_game", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "r[map]", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("random_map", "?i[stars]", CFGFLAG_SERVER | CFGFLAG_STORE, ConRandomMap, this, "Random map");
	Console()->Register("random_unfinished_map", "?i[stars]", CFGFLAG_SERVER | CFGFLAG_STORE, ConRandomUnfinishedMap, this, "Random unfinished map");
	Console()->Register("restart", "?i[seconds]", CFGFLAG_SERVER | CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i[team-id]", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("hot_reload", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConHotReload, this, "Reload the map while preserving the state of tees and teams");
	Console()->Register("reload_censorlist", "", CFGFLAG_SERVER, ConReloadCensorlist, this, "Reload the censorlist");

	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "r[name]", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "s[name] s[command] ?r[reason]", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("add_map_votes", "?s[directory]", CFGFLAG_SERVER, ConAddMapVotes, this, "Automatically adds voting options for all maps");
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");
	Console()->Register("votes", "?i[page]", CFGFLAG_SERVER, ConVotes, this, "Show all votes (page 0 by default, 20 entries per page)");
	Console()->Register("dump_antibot", "", CFGFLAG_SERVER, ConDumpAntibot, this, "Dumps the antibot status");
	Console()->Register("antibot", "r[command]", CFGFLAG_SERVER, ConAntibot, this, "Sends a command to the antibot");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

	Console()->Chain("sv_vote_kick", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_kick_min", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_spectate", ConchainSettingUpdate, this);
	Console()->Chain("sv_spectator_slots", ConchainSettingUpdate, this);

	RegisterDDRaceCommands();
	RegisterChatCommands();
}

void CGameContext::RegisterDDRaceCommands()
{
	Console()->Register("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill");
	Console()->Register("totele", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToTeleporter, this, "Teleports you to teleporter i");
	Console()->Register("totelecp", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter i");
	Console()->Register("tele", "?i[id] ?i[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConTeleport, this, "Teleports player i (or you) to player i (or you to where you look at)");
	Console()->Register("addweapon", "i[weapon-id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)");
	Console()->Register("removeweapon", "i[weapon-id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)");
	Console()->Register("shotgun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConShotgun, this, "Gives a shotgun to you");
	Console()->Register("grenade", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGrenade, this, "Gives a grenade launcher to you");
	Console()->Register("laser", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLaser, this, "Gives a laser to you");
	Console()->Register("rifle", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLaser, this, "Gives a laser to you");
	Console()->Register("jetpack", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConJetpack, this, "Gives jetpack to you");
	Console()->Register("setjumps", "i[jumps]", CFGFLAG_SERVER | CMDFLAG_TEST, ConSetJumps, this, "Gives you as many jumps as you specify");
	Console()->Register("weapons", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConWeapons, this, "Gives all weapons to you");
	Console()->Register("unshotgun", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnShotgun, this, "Removes the shotgun from you");
	Console()->Register("ungrenade", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnGrenade, this, "Removes the grenade launcher from you");
	Console()->Register("unlaser", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLaser, this, "Removes the laser from you");
	Console()->Register("unrifle", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLaser, this, "Removes the laser from you");
	Console()->Register("unjetpack", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnJetpack, this, "Removes the jetpack from you");
	Console()->Register("unweapons", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnWeapons, this, "Removes all weapons from you");
	Console()->Register("ninja", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConNinja, this, "Makes you a ninja");
	Console()->Register("unninja", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnNinja, this, "Removes ninja from you");
	Console()->Register("super", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConSuper, this, "Makes you super");
	Console()->Register("unsuper", "", CFGFLAG_SERVER, ConUnSuper, this, "Removes super from you");
	Console()->Register("invincible", "?i['0'|'1']", CFGFLAG_SERVER | CMDFLAG_TEST, ConToggleInvincible, this, "Toggles invincible mode");
	Console()->Register("infinite_jump", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConEndlessJump, this, "Gives you infinite jump");
	Console()->Register("uninfinite_jump", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnEndlessJump, this, "Removes infinite jump from you");
	Console()->Register("endless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConEndlessHook, this, "Gives you endless hook");
	Console()->Register("unendless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnEndlessHook, this, "Removes endless hook from you");
	Console()->Register("solo", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConSolo, this, "Puts you into solo part");
	Console()->Register("unsolo", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnSolo, this, "Puts you out of solo part");
	Console()->Register("freeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConFreeze, this, "Puts you into freeze");
	Console()->Register("unfreeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnFreeze, this, "Puts you out of freeze");
	Console()->Register("deep", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConDeep, this, "Puts you into deep freeze");
	Console()->Register("undeep", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnDeep, this, "Puts you out of deep freeze");
	Console()->Register("livefreeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConLiveFreeze, this, "Makes you live frozen");
	Console()->Register("unlivefreeze", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnLiveFreeze, this, "Puts you out of live freeze");
	Console()->Register("left", "?i[tiles]", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left");
	Console()->Register("right", "?i[tiles]", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right");
	Console()->Register("up", "?i[tiles]", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up");
	Console()->Register("down", "?i[tiles]", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down");

	Console()->Register("move", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii");
	Console()->Register("move_raw", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii");
	Console()->Register("force_pause", "v[id] i[seconds]", CFGFLAG_SERVER, ConForcePause, this, "Force i to pause for i seconds");
	Console()->Register("force_unpause", "v[id]", CFGFLAG_SERVER, ConForcePause, this, "Set force-pause timer of i to 0.");

	Console()->Register("set_team_ddr", "v[id] i[team]", CFGFLAG_SERVER, ConSetDDRTeam, this, "Set ddrace team of a player");
	Console()->Register("uninvite", "v[id] i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team");

	Console()->Register("vote_mute", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConVoteMute, this, "Remove v's right to vote for i seconds");
	Console()->Register("vote_unmute", "v[id]", CFGFLAG_SERVER, ConVoteUnmute, this, "Give back v's right to vote.");
	Console()->Register("vote_mutes", "", CFGFLAG_SERVER, ConVoteMutes, this, "List the current active vote mutes.");
	Console()->Register("mute", "", CFGFLAG_SERVER, ConMute, this, "Use either 'muteid <client_id> <seconds> <reason>' or 'muteip <ip> <seconds> <reason>'");
	Console()->Register("muteid", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteId, this, "Mute player with id");
	Console()->Register("muteip", "s[ip] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteIp, this, "Mute player with IP address");
	Console()->Register("unmute", "i[muteid]", CFGFLAG_SERVER, ConUnmute, this, "Unmute mute with number from \"mutes\"");
	Console()->Register("unmuteid", "v[id]", CFGFLAG_SERVER, ConUnmuteId, this, "Unmute player with id");
	Console()->Register("mutes", "", CFGFLAG_SERVER, ConMutes, this, "Show all active mutes");
	Console()->Register("moderate", "", CFGFLAG_SERVER, ConModerate, this, "Enables/disables active moderator mode for the player");
	Console()->Register("vote_no", "", CFGFLAG_SERVER, ConVoteNo, this, "Same as \"vote no\"");
	Console()->Register("save_dry", "", CFGFLAG_SERVER, ConDrySave, this, "Dump the current savestring");
	Console()->Register("dump_log", "?i[seconds]", CFGFLAG_SERVER, ConDumpLog, this, "Show logs of the last i seconds");

	Console()->Register("freezehammer", "v[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConFreezeHammer, this, "Gives a player Freeze Hammer");
	Console()->Register("unfreezehammer", "v[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnFreezeHammer, this, "Removes Freeze Hammer from a player");
}

void CGameContext::RegisterChatCommands()
{
	Console()->Register("credits", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConCredits, this, "Shows the credits of the DDNet mod");
	Console()->Register("rules", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRules, this, "Shows the server rules");
	Console()->Register("emote", "?s[emote name] i[duration in seconds]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConEyeEmote, this, "Sets your tee's eye emote");
	Console()->Register("eyeemote", "?s['on'|'off'|'toggle']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSetEyeEmote, this, "Toggles use of standard eye-emotes on/off, eyeemote s, where s = on for on, off for off, toggle for toggle and nothing to show current status");
	Console()->Register("settings", "?s[configname]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSettings, this, "Shows gameplay information for this server");
	Console()->Register("help", "?r[command]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConHelp, this, "Shows help to command r, general help if left blank");
	Console()->Register("info", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConInfo, this, "Shows info about this server");
	Console()->Register("list", "?s[filter]", CFGFLAG_CHAT, ConList, this, "List connected players with optional case-insensitive substring matching filter");
	Console()->Register("me", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConMe, this, "Like the famous irc command '/me says hi' will display '<yourname> says hi'");
	Console()->Register("w", "s[player name] r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConWhisper, this, "Whisper something to someone (private message)");
	Console()->Register("whisper", "s[player name] r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConWhisper, this, "Whisper something to someone (private message)");
	Console()->Register("c", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConConverse, this, "Converse with the last person you whispered to (private message)");
	Console()->Register("converse", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConConverse, this, "Converse with the last person you whispered to (private message)");
	Console()->Register("pause", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTogglePause, this, "Toggles pause");
	Console()->Register("spec", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConToggleSpec, this, "Toggles spec (if not available behaves as /pause)");
	Console()->Register("pausevoted", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTogglePauseVoted, this, "Toggles pause on the currently voted player");
	Console()->Register("specvoted", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConToggleSpecVoted, this, "Toggles spec on the currently voted player");
	Console()->Register("dnd", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConDND, this, "Toggle Do Not Disturb (no chat and server messages)");
	Console()->Register("whispers", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConWhispers, this, "Toggle receiving whispers");
	Console()->Register("mapinfo", "?r[map]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConMapInfo, this, "Show info about the map with name r gives (current map by default)");
	Console()->Register("timeout", "?s[code]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTimeout, this, "Set timeout protection code s");
	Console()->Register("practice", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConPractice, this, "Enable cheats for your current team's run, but you can't earn a rank");
	Console()->Register("practicecmdlist", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConPracticeCmdList, this, "List all commands that are avaliable in practice mode");
	Console()->Register("swap", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSwap, this, "Request to swap your tee with another team member");
	Console()->Register("save", "?r[code]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSave, this, "Save team with code r.");
	Console()->Register("load", "?r[code]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLoad, this, "Load with code r. /load to check your existing saves");
	Console()->Register("map", "?r[map]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConMap, this, "Vote a map by name");

	Console()->Register("rankteam", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeamRank, this, "Shows the team rank of player with name r (your team rank by default)");
	Console()->Register("teamrank", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeamRank, this, "Shows the team rank of player with name r (your team rank by default)");

	Console()->Register("rank", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRank, this, "Shows the rank of player with name r (your rank by default)");
	Console()->Register("top5team", "?s[player name] ?i[rank to start with]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeamTop5, this, "Shows five team ranks of the ladder or of a player beginning with rank i (1 by default, -1 for worst)");
	Console()->Register("teamtop5", "?s[player name] ?i[rank to start with]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeamTop5, this, "Shows five team ranks of the ladder or of a player beginning with rank i (1 by default, -1 for worst)");
	Console()->Register("top", "?i[rank to start with]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTop, this, "Shows the top ranks of the global and regional ladder beginning with rank i (1 by default, -1 for worst)");
	Console()->Register("top5", "?i[rank to start with]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTop, this, "Shows the top ranks of the global and regional ladder beginning with rank i (1 by default, -1 for worst)");
	Console()->Register("times", "?s[player name] ?i[number of times to skip]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTimes, this, "/times ?s?i shows last 5 times of the server or of a player beginning with name s starting with time i (i = 1 by default, -1 for first)");
	Console()->Register("points", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConPoints, this, "Shows the global points of a player beginning with name r (your rank by default)");
	Console()->Register("top5points", "?i[number]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTopPoints, this, "Shows five points of the global point ladder beginning with rank i (1 by default)");
	Console()->Register("timecp", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTimeCP, this, "Set your checkpoints based on another player");

	Console()->Register("team", "?i[id]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeam, this, "Lets you join team i (shows your team if left blank)");
	Console()->Register("lock", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLock, this, "Toggle team lock so no one else can join and so the team restarts when a player dies. /lock 0 to unlock, /lock 1 to lock");
	Console()->Register("unlock", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConUnlock, this, "Unlock a team");
	Console()->Register("invite", "r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConInvite, this, "Invite a person to a locked team");
	Console()->Register("join", "r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConJoin, this, "Join the team of the specified player");
	Console()->Register("team0mode", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTeam0Mode, this, "Toggle team between team 0 and team mode. This mode will make your team behave like team 0.");

	Console()->Register("showothers", "?i['0'|'1'|'2']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConShowOthers, this, "Whether to show players from other teams or not (off by default), optional i = 0 for off, i = 1 for on, i = 2 for own team only");
	Console()->Register("showall", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConShowAll, this, "Whether to show players at any distance (off by default), optional i = 0 for off else for on");
	Console()->Register("specteam", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSpecTeam, this, "Whether to show players from other teams when spectating (on by default), optional i = 0 for off else for on");
	Console()->Register("ninjajetpack", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConNinjaJetpack, this, "Whether to use ninja jetpack or not. Makes jetpack look more awesome");
	Console()->Register("saytime", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConSayTime, this, "Privately messages someone's current time in this current running race (your time by default)");
	Console()->Register("saytimeall", "", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConSayTimeAll, this, "Publicly messages everyone your current time in this current running race");
	Console()->Register("time", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTime, this, "Privately shows you your current time in this current running race in the broadcast message");
	Console()->Register("timer", "?s['gametimer'|'broadcast'|'both'|'none'|'cycle']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSetTimerType, this, "Personal Setting of showing time in either broadcast or game/round timer, timer s, where s = broadcast for broadcast, gametimer for game/round timer, cycle for cycle, both for both, none for no timer and nothing to show current status");
	Console()->Register("r", "", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConRescue, this, "Teleport yourself out of freeze if auto rescue mode is enabled, otherwise it will set position for rescuing if grounded and teleport you out of freeze if not (use sv_rescue 1 to enable this feature)");
	Console()->Register("rescue", "", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConRescue, this, "Teleport yourself out of freeze if auto rescue mode is enabled, otherwise it will set position for rescuing if grounded and teleport you out of freeze if not (use sv_rescue 1 to enable this feature)");
	Console()->Register("rescuemode", "?r['auto'|'manual']", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConRescueMode, this, "Sets one of the two rescue modes (auto or manual). Prints current mode if no arguments provided");
	Console()->Register("tp", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConTeleTo, this, "Depending on the number of supplied arguments, teleport yourself to; (0.) where you are spectating or aiming; (1.) the specified player name");
	Console()->Register("teleport", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConTeleTo, this, "Depending on the number of supplied arguments, teleport yourself to; (0.) where you are spectating or aiming; (1.) the specified player name");
	Console()->Register("tpxy", "s[x] s[y]", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConTeleXY, this, "Teleport yourself to the specified coordinates. A tilde (~) can be used to denote your current position, e.g. '/tpxy ~1 ~' to teleport one tile to the right");
	Console()->Register("lasttp", "", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConLastTele, this, "Teleport yourself to the last location you teleported to");
	Console()->Register("tc", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConTeleCursor, this, "Teleport yourself to player or to where you are spectating/or looking if no player name is given");
	Console()->Register("telecursor", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER | CMDFLAG_PRACTICE, ConTeleCursor, this, "Teleport yourself to player or to where you are spectating/or looking if no player name is given");
	Console()->Register("totele", "i[number]", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeToTeleporter, this, "Teleports you to teleporter i");
	Console()->Register("totelecp", "i[number]", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeToCheckTeleporter, this, "Teleports you to checkpoint teleporter i");
	Console()->Register("unsolo", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnSolo, this, "Puts you out of solo part");
	Console()->Register("solo", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeSolo, this, "Puts you into solo part");
	Console()->Register("undeep", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnDeep, this, "Puts you out of deep freeze");
	Console()->Register("deep", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeDeep, this, "Puts you into deep freeze");
	Console()->Register("unlivefreeze", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnLiveFreeze, this, "Puts you out of live freeze");
	Console()->Register("livefreeze", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeLiveFreeze, this, "Makes you live frozen");
	Console()->Register("addweapon", "i[weapon-id]", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)");
	Console()->Register("removeweapon", "i[weapon-id]", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, laser = 4, ninja = 5)");
	Console()->Register("shotgun", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeShotgun, this, "Gives a shotgun to you");
	Console()->Register("grenade", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeGrenade, this, "Gives a grenade launcher to you");
	Console()->Register("laser", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeLaser, this, "Gives a laser to you");
	Console()->Register("rifle", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeLaser, this, "Gives a laser to you");
	Console()->Register("jetpack", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeJetpack, this, "Gives jetpack to you");
	Console()->Register("setjumps", "i[jumps]", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeSetJumps, this, "Gives you as many jumps as you specify");
	Console()->Register("weapons", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeWeapons, this, "Gives all weapons to you");
	Console()->Register("unshotgun", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnShotgun, this, "Removes the shotgun from you");
	Console()->Register("ungrenade", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnGrenade, this, "Removes the grenade launcher from you");
	Console()->Register("unlaser", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnLaser, this, "Removes the laser from you");
	Console()->Register("unrifle", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnLaser, this, "Removes the laser from you");
	Console()->Register("unjetpack", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnJetpack, this, "Removes the jetpack from you");
	Console()->Register("unweapons", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnWeapons, this, "Removes all weapons from you");
	Console()->Register("ninja", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeNinja, this, "Makes you a ninja");
	Console()->Register("unninja", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnNinja, this, "Removes ninja from you");
	Console()->Register("infjump", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeEndlessJump, this, "Gives you infinite jump");
	Console()->Register("uninfjump", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnEndlessJump, this, "Removes infinite jump from you");
	Console()->Register("endless", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeEndlessHook, this, "Gives you endless hook");
	Console()->Register("unendless", "", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeUnEndlessHook, this, "Removes endless hook from you");
	Console()->Register("invincible", "?i['0'|'1']", CFGFLAG_CHAT | CMDFLAG_PRACTICE, ConPracticeToggleInvincible, this, "Toggles invincible mode");
	Console()->Register("kill", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConProtectedKill, this, "Kill yourself when kill-protected during a long game (use f1, kill for regular kill)");
}

void CGameContext::OnInit(const void *pPersistentData)
{
	const CPersistentData *pPersistent = (const CPersistentData *)pPersistentData;

	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pAntibot = Kernel()->RequestInterface<IAntibot>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	m_GameUuid = RandomUuid();
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);

	uint64_t aSeed[2];
	secure_random_fill(aSeed, sizeof(aSeed));
	m_Prng.Seed(aSeed);
	m_World.m_Core.m_pPrng = &m_Prng;

	DeleteTempfile();

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel()->RequestInterface<IMap>(), false);
	m_Collision.Init(&m_Layers);
	m_World.m_pTuningList = m_aTuningList;
	m_World.m_Core.InitSwitchers(m_Collision.m_HighestSwitchNumber);

	char aMapName[IO_MAX_PATH_LENGTH];
	int MapSize;
	SHA256_DIGEST MapSha256;
	int MapCrc;
	Server()->GetMapInfo(aMapName, sizeof(aMapName), &MapSize, &MapSha256, &MapCrc);
	m_MapBugs = GetMapBugs(aMapName, MapSize, MapSha256);

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

	if(!str_comp(Config()->m_SvGametype, "mod"))
		m_pController = new CGameControllerMod(this);
	else
		m_pController = new CGameControllerDDRace(this);

	ReadCensorList();

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
			str_copy(aVersion, GAME_VERSION);
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

		if(pPersistent)
		{
			GameInfo.m_HavePrevGameUuid = true;
			GameInfo.m_PrevGameUuid = pPersistent->m_PrevGameUuid;
		}
		else
		{
			GameInfo.m_HavePrevGameUuid = false;
			mem_zero(&GameInfo.m_PrevGameUuid, sizeof(GameInfo.m_PrevGameUuid));
		}

		m_TeeHistorian.Reset(&GameInfo, TeeHistorianWrite, this);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(Server()->ClientSlotEmpty(i))
			{
				continue;
			}
			const int Level = Server()->GetAuthedState(i);
			if(Level == AUTHED_NO)
			{
				continue;
			}
			m_TeeHistorian.RecordAuthInitial(i, Level, Server()->GetAuthName(i));
		}
	}

	Server()->DemoRecorder_HandleAutoStart();

	if(!m_pScore)
	{
		m_pScore = new CScore(this, ((CServer *)Server())->DbPool());
	}

	// create all entities from the game layer
	CreateAllEntities(true);

	m_pAntibot->RoundStart(this);
}

void CGameContext::CreateAllEntities(bool Initial)
{
	const CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	const CTile *pTiles = static_cast<CTile *>(Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data));

	const CTile *pFront = nullptr;
	if(m_Layers.FrontLayer())
		pFront = static_cast<CTile *>(Kernel()->RequestInterface<IMap>()->GetData(m_Layers.FrontLayer()->m_Front));

	const CSwitchTile *pSwitch = nullptr;
	if(m_Layers.SwitchLayer())
		pSwitch = static_cast<CSwitchTile *>(Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SwitchLayer()->m_Switch));

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			const int Index = y * pTileMap->m_Width + x;

			// Game layer
			{
				const int GameIndex = pTiles[Index].m_Index;
				if(GameIndex == TILE_OLDLASER)
				{
					g_Config.m_SvOldLaser = 1;
					dbg_msg("game_layer", "found old laser tile");
				}
				else if(GameIndex == TILE_NPC)
				{
					m_Tuning.Set("player_collision", 0);
					dbg_msg("game_layer", "found no collision tile");
				}
				else if(GameIndex == TILE_EHOOK)
				{
					g_Config.m_SvEndlessDrag = 1;
					dbg_msg("game_layer", "found unlimited hook time tile");
				}
				else if(GameIndex == TILE_NOHIT)
				{
					g_Config.m_SvHit = 0;
					dbg_msg("game_layer", "found no weapons hitting others tile");
				}
				else if(GameIndex == TILE_NPH)
				{
					m_Tuning.Set("player_hooking", 0);
					dbg_msg("game_layer", "found no player hooking tile");
				}
				else if(GameIndex >= ENTITY_OFFSET)
				{
					m_pController->OnEntity(GameIndex - ENTITY_OFFSET, x, y, LAYER_GAME, pTiles[Index].m_Flags, Initial);
				}
			}

			if(pFront)
			{
				const int FrontIndex = pFront[Index].m_Index;
				if(FrontIndex == TILE_OLDLASER)
				{
					g_Config.m_SvOldLaser = 1;
					dbg_msg("front_layer", "found old laser tile");
				}
				else if(FrontIndex == TILE_NPC)
				{
					m_Tuning.Set("player_collision", 0);
					dbg_msg("front_layer", "found no collision tile");
				}
				else if(FrontIndex == TILE_EHOOK)
				{
					g_Config.m_SvEndlessDrag = 1;
					dbg_msg("front_layer", "found unlimited hook time tile");
				}
				else if(FrontIndex == TILE_NOHIT)
				{
					g_Config.m_SvHit = 0;
					dbg_msg("front_layer", "found no weapons hitting others tile");
				}
				else if(FrontIndex == TILE_NPH)
				{
					m_Tuning.Set("player_hooking", 0);
					dbg_msg("front_layer", "found no player hooking tile");
				}
				else if(FrontIndex >= ENTITY_OFFSET)
				{
					m_pController->OnEntity(FrontIndex - ENTITY_OFFSET, x, y, LAYER_FRONT, pFront[Index].m_Flags, Initial);
				}
			}

			if(pSwitch)
			{
				const int SwitchType = pSwitch[Index].m_Type;
				// TODO: Add off by default door here
				// if(SwitchType == TILE_DOOR_OFF)
				if(SwitchType >= ENTITY_OFFSET)
				{
					m_pController->OnEntity(SwitchType - ENTITY_OFFSET, x, y, LAYER_SWITCH, pSwitch[Index].m_Flags, Initial, pSwitch[Index].m_Number);
				}
			}
		}
	}
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

	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(aConfig, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		// No map-specific config, just return.
		return;
	}

	std::vector<const char *> vpLines;
	int TotalLength = 0;
	while(const char *pLine = LineReader.Get())
	{
		vpLines.push_back(pLine);
		TotalLength += str_length(pLine) + 1;
	}

	char *pSettings = (char *)malloc(maximum(1, TotalLength));
	int Offset = 0;
	for(const char *pLine : vpLines)
	{
		int Length = str_length(pLine) + 1;
		mem_copy(pSettings + Offset, pLine, Length);
		Offset += Length;
	}

	CDataFileReader Reader;
	Reader.Open(Storage(), pNewMapName, IStorage::TYPE_ALL);

	CDataFileWriter Writer;

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int TypeId;
		int ItemId;
		void *pData = Reader.GetItem(i, &TypeId, &ItemId);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if(TypeId == MAPITEMTYPE_INFO && ItemId == 0)
		{
			FoundInfo = true;
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				CMapItemInfoSettings *pInfo = (CMapItemInfoSettings *)pData;
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
					pData = &MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo *)&MapInfo = *(CMapItemInfo *)pData;
				MapInfo.m_Settings = SettingsIndex;
				pData = &MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(TypeId, ItemId, Size, pData);
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
		const void *pData = Reader.GetData(i);
		int Size = Reader.GetDataSize(i);
		Writer.AddData(Size, pData);
		Reader.UnloadData(i);
	}

	dbg_msg("mapchange", "imported settings");
	free(pSettings);
	Reader.Close();
	char aTemp[IO_MAX_PATH_LENGTH];
	Writer.Open(Storage(), IStorage::FormatTmpPath(aTemp, sizeof(aTemp), pNewMapName));
	Writer.Finish();

	str_copy(pNewMapName, aTemp, MapNameSize);
	str_copy(m_aDeleteTempfile, aTemp, sizeof(m_aDeleteTempfile));
}

void CGameContext::OnShutdown(void *pPersistentData)
{
	CPersistentData *pPersistent = (CPersistentData *)pPersistentData;

	if(pPersistent)
	{
		pPersistent->m_PrevGameUuid = m_GameUuid;
	}

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

	// Stop any demos being recorded.
	Server()->StopDemos();

	DeleteTempfile();
	ConfigManager()->ResetGameSettings();
	Collision()->Unload();
	Layers()->Unload();
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
		int ItemId;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, nullptr, &ItemId);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || ItemId != 0)
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

void CGameContext::OnSnap(int ClientId)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(Server()->IsRecording(ClientId > -1 ? ClientId : MAX_CLIENTS) && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientId);
	}

	m_pController->Snap(ClientId);

	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
			pPlayer->Snap(ClientId);
	}

	if(ClientId > -1)
		m_apPlayers[ClientId]->FakeSnap();

	m_World.Snap(ClientId);
	m_Events.Snap(ClientId);
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_World.PostSnap();
	m_Events.Clear();
}

void CGameContext::UpdatePlayerMaps()
{
	const auto DistCompare = [](std::pair<float, int> a, std::pair<float, int> b) -> bool {
		return (a.first < b.first);
	};

	if(Server()->Tick() % g_Config.m_SvMapUpdateRate != 0)
		return;

	std::pair<float, int> Dist[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i))
			continue;
		if(Server()->GetClientVersion(i) >= VERSION_DDNET_OLD)
			continue;
		int *pMap = Server()->GetIdMap(i);

		// compute distances
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			Dist[j].second = j;
			if(j == i)
				continue;
			if(!Server()->ClientIngame(j) || !m_apPlayers[j])
			{
				Dist[j].first = 1e10;
				continue;
			}
			CCharacter *pChr = m_apPlayers[j]->GetCharacter();
			if(!pChr)
			{
				Dist[j].first = 1e9;
				continue;
			}
			if(!pChr->CanSnapCharacter(i))
				Dist[j].first = 1e8;
			else
				Dist[j].first = length_squared(m_apPlayers[i]->m_ViewPos - pChr->GetPos());
		}

		// always send the player themselves, even if all in same position
		Dist[i].first = -1;

		std::nth_element(&Dist[0], &Dist[VANILLA_MAX_CLIENTS - 1], &Dist[MAX_CLIENTS], DistCompare);

		int Index = 1; // exclude self client id
		for(int j = 0; j < VANILLA_MAX_CLIENTS - 1; j++)
		{
			pMap[j + 1] = -1; // also fill player with empty name to say chat msgs
			if(Dist[j].second == i || Dist[j].first > 5e9f)
				continue;
			pMap[Index++] = Dist[j].second;
		}

		// sort by real client ids, guarantee order on distance changes, O(Nlog(N)) worst case
		// sort just clients in game always except first (self client id) and last (fake client id) indexes
		std::sort(&pMap[1], &pMap[minimum(Index, VANILLA_MAX_CLIENTS - 1)]);
	}
}

bool CGameContext::IsClientReady(int ClientId) const
{
	return m_apPlayers[ClientId] && m_apPlayers[ClientId]->m_IsReady;
}

bool CGameContext::IsClientPlayer(int ClientId) const
{
	return m_apPlayers[ClientId] && m_apPlayers[ClientId]->GetTeam() != TEAM_SPECTATORS;
}

CUuid CGameContext::GameUuid() const { return m_GameUuid; }
const char *CGameContext::GameType() const { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() const { return GAME_VERSION; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::OnSetAuthed(int ClientId, int Level)
{
	if(m_apPlayers[ClientId] && m_VoteCloseTime && Level != AUTHED_NO)
	{
		char aBuf[512], aIp[NETADDR_MAXSTRSIZE];
		Server()->GetClientAddr(ClientId, aIp, sizeof(aIp));
		str_format(aBuf, sizeof(aBuf), "ban %s %d Banned by vote", aIp, g_Config.m_SvVoteKickBantime);
		if(!str_comp_nocase(m_aVoteCommand, aBuf) && (m_VoteCreator == -1 || Level > Server()->GetAuthedState(m_VoteCreator)))
		{
			m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO_ADMIN;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", "Vote aborted by authorized login.");
		}
	}

	if(m_TeeHistorianActive)
	{
		if(Level != AUTHED_NO)
		{
			m_TeeHistorian.RecordAuthLogin(ClientId, Level, Server()->GetAuthName(ClientId));
		}
		else
		{
			m_TeeHistorian.RecordAuthLogout(ClientId);
		}
	}
}

void CGameContext::SendRecord(int ClientId)
{
	CNetMsg_Sv_Record Msg;
	CNetMsg_Sv_RecordLegacy MsgLegacy;
	MsgLegacy.m_PlayerTimeBest = Msg.m_PlayerTimeBest = Score()->PlayerData(ClientId)->m_BestTime * 100.0f;
	MsgLegacy.m_ServerTimeBest = Msg.m_ServerTimeBest = m_pController->m_CurrentRecord * 100.0f; //TODO: finish this
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
	if(!Server()->IsSixup(ClientId) && GetClientVersion(ClientId) < VERSION_DDNET_MSG_LEGACY)
	{
		Server()->SendPackMsg(&MsgLegacy, MSGFLAG_VITAL, ClientId);
	}
}

bool CGameContext::ProcessSpamProtection(int ClientId, bool RespectChatInitialDelay)
{
	if(!m_apPlayers[ClientId])
		return false;
	if(g_Config.m_SvSpamprotection && m_apPlayers[ClientId]->m_LastChat && m_apPlayers[ClientId]->m_LastChat + Server()->TickSpeed() * g_Config.m_SvChatDelay > Server()->Tick())
		return true;
	else if(g_Config.m_SvDnsblChat && Server()->DnsblBlack(ClientId))
	{
		SendChatTarget(ClientId, "Players are not allowed to chat from VPNs at this time");
		return true;
	}
	else
		m_apPlayers[ClientId]->m_LastChat = Server()->Tick();

	NETADDR Addr;
	Server()->GetClientAddr(ClientId, &Addr);

	CMute Muted;
	int Expires = 0;
	for(int i = 0; i < m_NumMutes && Expires <= 0; i++)
	{
		if(!net_addr_comp_noport(&Addr, &m_aMutes[i].m_Addr))
		{
			if(RespectChatInitialDelay || m_aMutes[i].m_InitialChatDelay)
			{
				Muted = m_aMutes[i];
				Expires = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
			}
		}
	}

	if(Expires > 0)
	{
		char aBuf[128];
		if(Muted.m_InitialChatDelay)
			str_format(aBuf, sizeof(aBuf), "This server has an initial chat delay, you will be able to talk in %d seconds.", Expires);
		else
			str_format(aBuf, sizeof(aBuf), "You are not permitted to talk for the next %d seconds.", Expires);
		SendChatTarget(ClientId, aBuf);
		return true;
	}

	if(g_Config.m_SvSpamMuteDuration && (m_apPlayers[ClientId]->m_ChatScore += g_Config.m_SvChatPenalty) > g_Config.m_SvChatThreshold)
	{
		Mute(&Addr, g_Config.m_SvSpamMuteDuration, Server()->ClientName(ClientId));
		m_apPlayers[ClientId]->m_ChatScore = 0;
		return true;
	}

	return false;
}

int CGameContext::GetDDRaceTeam(int ClientId) const
{
	return m_pController->Teams().m_Core.Team(ClientId);
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

void CGameContext::Whisper(int ClientId, char *pStr)
{
	if(ProcessSpamProtection(ClientId))
		return;

	pStr = str_skip_whitespaces(pStr);

	const char *pName;
	int Victim;
	bool Error = false;

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
				Error = true;
				break;
			}

			*pDst = *pStr;
			pDst++;
			pStr++;
		}

		if(!Error)
		{
			*pDst = '\0';
			pStr++;

			for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
			{
				if(Server()->ClientIngame(Victim) && str_comp(pName, Server()->ClientName(Victim)) == 0)
				{
					break;
				}
			}
		}
	}
	else
	{
		pName = pStr;
		while(true)
		{
			if(pStr[0] == '\0')
			{
				Error = true;
				break;
			}
			if(pStr[0] == ' ')
			{
				pStr[0] = '\0';
				for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
				{
					if(Server()->ClientIngame(Victim) && str_comp(pName, Server()->ClientName(Victim)) == 0)
					{
						break;
					}
				}

				pStr[0] = ' ';
				if(Victim < MAX_CLIENTS)
					break;
			}
			pStr++;
		}
	}

	if(pStr[0] != ' ')
	{
		Error = true;
	}

	*pStr = '\0';
	pStr++;

	if(Error)
	{
		SendChatTarget(ClientId, "Invalid whisper");
		return;
	}

	if(!CheckClientId2(Victim))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "No player with name \"%s\" found", pName);
		SendChatTarget(ClientId, aBuf);
		return;
	}

	WhisperId(ClientId, Victim, pStr);
}

void CGameContext::WhisperId(int ClientId, int VictimId, const char *pMessage)
{
	dbg_assert(CheckClientId2(ClientId) && m_apPlayers[ClientId] != nullptr, "ClientId invalid");
	dbg_assert(CheckClientId2(VictimId) && m_apPlayers[VictimId] != nullptr, "VictimId invalid");

	m_apPlayers[ClientId]->m_LastWhisperTo = VictimId;

	char aCensoredMessage[256];
	CensorMessage(aCensoredMessage, pMessage, sizeof(aCensoredMessage));

	char aBuf[256];

	if(Server()->IsSixup(ClientId))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientId = ClientId;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetId = VictimId;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}
	else if(GetClientVersion(ClientId) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = TEAM_WHISPER_SEND;
		Msg.m_ClientId = VictimId;
		Msg.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
		else
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[→ %s] %s", Server()->ClientName(VictimId), aCensoredMessage);
		SendChatTarget(ClientId, aBuf);
	}

	if(!m_apPlayers[VictimId]->m_Whispers)
	{
		SendChatTarget(ClientId, "This person has disabled receiving whispers");
		return;
	}

	if(Server()->IsSixup(VictimId))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientId = ClientId;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetId = VictimId;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimId);
	}
	else if(GetClientVersion(VictimId) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg2;
		Msg2.m_Team = TEAM_WHISPER_RECV;
		Msg2.m_ClientId = ClientId;
		Msg2.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL, VictimId);
		else
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimId);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[← %s] %s", Server()->ClientName(ClientId), aCensoredMessage);
		SendChatTarget(VictimId, aBuf);
	}
}

void CGameContext::Converse(int ClientId, char *pStr)
{
	CPlayer *pPlayer = m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	if(ProcessSpamProtection(ClientId))
		return;

	if(pPlayer->m_LastWhisperTo < 0)
		SendChatTarget(ClientId, "You do not have an ongoing conversation. Whisper to someone to start one");
	else
	{
		WhisperId(ClientId, pPlayer->m_LastWhisperTo, pStr);
	}
}

bool CGameContext::IsVersionBanned(int Version)
{
	char aVersion[16];
	str_format(aVersion, sizeof(aVersion), "%d", Version);

	return str_in_list(g_Config.m_SvBannedVersions, ",", aVersion);
}

void CGameContext::List(int ClientId, const char *pFilter)
{
	int Total = 0;
	char aBuf[256];
	int Bufcnt = 0;
	if(pFilter[0])
		str_format(aBuf, sizeof(aBuf), "Listing players with \"%s\" in name:", pFilter);
	else
		str_copy(aBuf, "Listing all players:");
	SendChatTarget(ClientId, aBuf);
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
				SendChatTarget(ClientId, aBuf);
				Bufcnt = 0;
			}
			if(Bufcnt != 0)
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, ", %s", pName);
				Bufcnt += 2 + str_length(pName);
			}
			else
			{
				str_copy(&aBuf[Bufcnt], pName, sizeof(aBuf) - Bufcnt);
				Bufcnt += str_length(pName);
			}
		}
	}
	if(Bufcnt != 0)
		SendChatTarget(ClientId, aBuf);
	str_format(aBuf, sizeof(aBuf), "%d players online", Total);
	SendChatTarget(ClientId, aBuf);
}

int CGameContext::GetClientVersion(int ClientId) const
{
	return Server()->GetClientVersion(ClientId);
}

CClientMask CGameContext::ClientsMaskExcludeClientVersionAndHigher(int Version) const
{
	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GetClientVersion(i) >= Version)
			continue;
		Mask.set(i);
	}
	return Mask;
}

bool CGameContext::PlayerModerating() const
{
	return std::any_of(std::begin(m_apPlayers), std::end(m_apPlayers), [](const CPlayer *pPlayer) { return pPlayer && pPlayer->m_Moderating; });
}

void CGameContext::ForceVote(int EnforcerId, bool Success)
{
	// check if there is a vote running
	if(!m_VoteCloseTime)
		return;

	m_VoteEnforce = Success ? CGameContext::VOTE_ENFORCE_YES_ADMIN : CGameContext::VOTE_ENFORCE_NO_ADMIN;

	char aBuf[256];
	const char *pOption = Success ? "yes" : "no";
	str_format(aBuf, sizeof(aBuf), "authorized player forced vote %s", pOption);
	SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pOption);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

bool CGameContext::RateLimitPlayerVote(int ClientId)
{
	int64_t Now = Server()->Tick();
	int64_t TickSpeed = Server()->TickSpeed();
	CPlayer *pPlayer = m_apPlayers[ClientId];

	if(g_Config.m_SvRconVote && !Server()->GetAuthedState(ClientId))
	{
		SendChatTarget(ClientId, "You can only vote after logging in.");
		return true;
	}

	if(g_Config.m_SvDnsblVote && Server()->DistinctClientCount() > 1)
	{
		if(m_pServer->DnsblPending(ClientId))
		{
			SendChatTarget(ClientId, "You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.");
			return true;
		}
		else if(m_pServer->DnsblBlack(ClientId))
		{
			SendChatTarget(ClientId, "You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.");
			return true;
		}
	}

	if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + TickSpeed * 3 > Now)
		return true;

	pPlayer->m_LastVoteTry = Now;
	if(m_VoteCloseTime)
	{
		SendChatTarget(ClientId, "Wait for current vote to end before calling a new one.");
		return true;
	}

	if(Now < pPlayer->m_FirstVoteTick)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "You must wait %d seconds before making your first vote.", (int)((pPlayer->m_FirstVoteTick - Now) / TickSpeed) + 1);
		SendChatTarget(ClientId, aBuf);
		return true;
	}

	int TimeLeft = pPlayer->m_LastVoteCall + TickSpeed * g_Config.m_SvVoteDelay - Now;
	if(pPlayer->m_LastVoteCall && TimeLeft > 0)
	{
		char aChatmsg[64];
		str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote.", (int)(TimeLeft / TickSpeed) + 1);
		SendChatTarget(ClientId, aChatmsg);
		return true;
	}

	NETADDR Addr;
	Server()->GetClientAddr(ClientId, &Addr);
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
		SendChatTarget(ClientId, aChatmsg);
		return true;
	}
	return false;
}

bool CGameContext::RateLimitPlayerMapVote(int ClientId) const
{
	if(!Server()->GetAuthedState(ClientId) && time_get() < m_LastMapVote + (time_freq() * g_Config.m_SvVoteMapTimeDelay))
	{
		char aChatmsg[512] = {0};
		str_format(aChatmsg, sizeof(aChatmsg), "There's a %d second delay between map-votes, please wait %d seconds.",
			g_Config.m_SvVoteMapTimeDelay, (int)((m_LastMapVote + g_Config.m_SvVoteMapTimeDelay * time_freq() - time_get()) / time_freq()));
		SendChatTarget(ClientId, aChatmsg);
		return true;
	}
	return false;
}

void CGameContext::OnUpdatePlayerServerInfo(CJsonStringWriter *pJSonWriter, int Id)
{
	if(!m_apPlayers[Id])
		return;

	CTeeInfo &TeeInfo = m_apPlayers[Id]->m_TeeInfos;

	pJSonWriter->WriteAttribute("skin");
	pJSonWriter->BeginObject();

	// 0.6
	if(!Server()->IsSixup(Id))
	{
		pJSonWriter->WriteAttribute("name");
		pJSonWriter->WriteStrValue(TeeInfo.m_aSkinName);

		if(TeeInfo.m_UseCustomColor)
		{
			pJSonWriter->WriteAttribute("color_body");
			pJSonWriter->WriteIntValue(TeeInfo.m_ColorBody);

			pJSonWriter->WriteAttribute("color_feet");
			pJSonWriter->WriteIntValue(TeeInfo.m_ColorFeet);
		}
	}
	// 0.7
	else
	{
		const char *apPartNames[protocol7::NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"};

		for(int i = 0; i < protocol7::NUM_SKINPARTS; ++i)
		{
			pJSonWriter->WriteAttribute(apPartNames[i]);
			pJSonWriter->BeginObject();

			pJSonWriter->WriteAttribute("name");
			pJSonWriter->WriteStrValue(TeeInfo.m_apSkinPartNames[i]);

			if(TeeInfo.m_aUseCustomColors[i])
			{
				pJSonWriter->WriteAttribute("color");
				pJSonWriter->WriteIntValue(TeeInfo.m_aSkinPartColors[i]);
			}

			pJSonWriter->EndObject();
		}
	}

	pJSonWriter->EndObject();

	pJSonWriter->WriteAttribute("afk");
	pJSonWriter->WriteBoolValue(m_apPlayers[Id]->IsAfk());

	const int Team = m_pController->IsTeamPlay() ? m_apPlayers[Id]->GetTeam() : m_apPlayers[Id]->GetTeam() == TEAM_SPECTATORS ? -1 : GetDDRaceTeam(Id);

	pJSonWriter->WriteAttribute("team");
	pJSonWriter->WriteIntValue(Team);
}

void CGameContext::ReadCensorList()
{
	const char *pCensorFilename = "censorlist.txt";
	CLineReader LineReader;
	m_vCensorlist.clear();
	if(LineReader.OpenFile(Storage()->OpenFile(pCensorFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		while(const char *pLine = LineReader.Get())
		{
			m_vCensorlist.emplace_back(pLine);
		}
	}
	else
	{
		dbg_msg("censorlist", "failed to open '%s'", pCensorFilename);
	}
}
