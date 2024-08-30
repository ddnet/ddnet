#include <base/system.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol7.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "zcatch.h"

CGameControllerZcatch::CGameControllerZcatch(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = 0;
	m_AllowSkinChange = false;

	m_pGameType = "zCatch";

	for(auto &Color : m_aBodyColors)
		Color = 0;
}

CGameControllerZcatch::ECatchGameState CGameControllerZcatch::CatchGameState() const
{
	if(g_Config.m_SvReleaseGame)
		return ECatchGameState::RELEASE_GAME;
	return m_CatchGameState;
}

void CGameControllerZcatch::SetCatchGameState(ECatchGameState State)
{
	if(g_Config.m_SvReleaseGame)
	{
		m_CatchGameState = ECatchGameState::RELEASE_GAME;
		return;
	}
	m_CatchGameState = State;
}

void CGameControllerZcatch::OnRoundStart()
{
	CGameControllerInstagib::OnRoundStart();

	int ActivePlayers = NumActivePlayers();
	if(ActivePlayers < g_Config.m_SvZcatchMinPlayers && CatchGameState() != ECatchGameState::RELEASE_GAME)
	{
		SendChatTarget(-1, "Not enough players to start a round");
		SetCatchGameState(ECatchGameState::WAITING_FOR_PLAYERS);
	}

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		pPlayer->m_GotRespawnInfo = false;
		pPlayer->m_vVictimIds.clear();
		pPlayer->m_KillerId = -1;
	}
}

int CGameControllerZcatch::GetAutoTeam(int NotThisId)
{
	if(CatchGameState() == ECatchGameState::RUNNING)
		return TEAM_SPECTATORS;

	return CGameControllerInstagib::GetAutoTeam(NotThisId);
}

CGameControllerZcatch::~CGameControllerZcatch() = default;

void CGameControllerZcatch::Tick()
{
	CGameControllerInstagib::Tick();

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		int Color = GetBodyColor(pPlayer->m_Spree);
		if(GameState() == IGS_END_ROUND)
			Color = m_aBodyColors[pPlayer->GetCid()];

		// this is wasting a bit of clock cycles setting it every tick
		// it should be set on kill and then not be overwritten by info changes
		// but there is no git conflict free way of doing that
		pPlayer->m_TeeInfos.m_ColorBody = Color;
		pPlayer->m_TeeInfos.m_UseCustomColor = 1;

		if(m_aBodyColors[pPlayer->GetCid()] != pPlayer->m_TeeInfos.m_ColorBody)
		{
			m_aBodyColors[pPlayer->GetCid()] = pPlayer->m_TeeInfos.m_ColorBody;
			SendSkinBodyColor7(pPlayer->GetCid(), pPlayer->m_TeeInfos.m_ColorBody);
		}
	}
}

void CGameControllerZcatch::SendSkinBodyColor7(int ClientId, int Color)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	// also update 0.6 just to be sure
	pPlayer->m_TeeInfos.m_ColorBody = Color;
	pPlayer->m_TeeInfos.m_UseCustomColor = 1;

	// 0.7
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		pPlayer->m_TeeInfos.m_aSkinPartColors[p] = Color;
		pPlayer->m_TeeInfos.m_aUseCustomColors[p] = true;
	}

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

int CGameControllerZcatch::GetBodyColor(int Kills)
{
	if(Kills == 0)
		return 0xFFBB00;
	if(Kills == 1)
		return 0x00FF00;
	if(Kills == 2)
		return 0x11FF00;
	if(Kills == 3)
		return 0x22FF00;
	if(Kills == 4)
		return 0x33FF00;
	if(Kills == 5)
		return 0x44FF00;
	if(Kills == 6)
		return 0x55FF00;
	if(Kills == 7)
		return 0x66FF00;
	if(Kills == 8)
		return 0x77FF00;
	if(Kills == 9)
		return 0x88FF00;
	if(Kills == 10)
		return 0x99FF00;
	if(Kills == 11)
		return 0xAAFF00;
	if(Kills == 12)
		return 0xBBFF00;
	if(Kills == 13)
		return 0xCCFF00;
	if(Kills == 14)
		return 0xDDFF00;
	if(Kills == 15)
		return 0xEEFF00;
	return 0xFFBB00;
}

void CGameControllerZcatch::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);

	SetSpawnWeapons(pChr);
}

void CGameControllerZcatch::ReleasePlayer(class CPlayer *pPlayer, const char *pMsg)
{
	GameServer()->SendChatTarget(pPlayer->GetCid(), pMsg);
	pPlayer->m_KillerId = -1;
	pPlayer->m_IsDead = false;
	pPlayer->SetTeamRaw(TEAM_RED);
}

bool CGameControllerZcatch::OnSelfkill(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;
	if(pPlayer->m_vVictimIds.empty())
		return false;

	CPlayer *pVictim = nullptr;
	while(!pVictim)
	{
		if(pPlayer->m_vVictimIds.empty())
			return false;

		int ReleaseId = pPlayer->m_vVictimIds.back();
		pPlayer->m_vVictimIds.pop_back();

		pVictim = GameServer()->m_apPlayers[ReleaseId];
	}
	if(!pVictim)
		return false;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "You were released by '%s'", Server()->ClientName(pPlayer->GetCid()));
	ReleasePlayer(pVictim, aBuf);

	str_format(aBuf, sizeof(aBuf), "You released '%s' (%d players left)", Server()->ClientName(pVictim->GetCid()), pPlayer->m_vVictimIds.size());
	SendChatTarget(ClientId, aBuf);

	return true;
}

void CGameControllerZcatch::OnCaught(class CPlayer *pVictim, class CPlayer *pKiller)
{
	if(pVictim->GetCid() == pKiller->GetCid())
		return;

	if(CatchGameState() == ECatchGameState::WAITING_FOR_PLAYERS)
	{
		if(!pKiller->m_GotRespawnInfo)
			GameServer()->SendChatTarget(pKiller->GetCid(), "Kill respawned because there are not enough players.");
		pKiller->m_GotRespawnInfo = true;
		return;
	}
	if(CatchGameState() == ECatchGameState::RELEASE_GAME)
	{
		if(!pKiller->m_GotRespawnInfo)
			GameServer()->SendChatTarget(pKiller->GetCid(), "Kill respawned because this is a release game.");
		pKiller->m_GotRespawnInfo = true;
		return;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "You are spectator until '%s' dies", Server()->ClientName(pKiller->GetCid()));
	GameServer()->SendChatTarget(pVictim->GetCid(), aBuf);

	pVictim->SetTeamRaw(TEAM_SPECTATORS);
	pVictim->m_IsDead = true;
	pVictim->m_KillerId = pKiller->GetCid();

	pKiller->m_vVictimIds.emplace_back(pVictim->GetCid());
}

int CGameControllerZcatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);

	// TODO: revisit this edge case when zcatch is done
	//       a killer leaving while the bullet is flying
	if(!pKiller)
		return 0;

	OnCaught(pVictim->GetPlayer(), pKiller);

	char aBuf[512];
	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->m_KillerId == -1)
			continue;
		if(pPlayer->GetCid() == pVictim->GetPlayer()->GetCid())
			continue;
		if(pPlayer->m_KillerId != pVictim->GetPlayer()->GetCid())
			continue;

		// victim's victims
		str_format(aBuf, sizeof(aBuf), "You respawned because '%s' died", Server()->ClientName(pVictim->GetPlayer()->GetCid()));
		ReleasePlayer(pPlayer, aBuf);
	}

	DoWincheckRound();

	return 0;
}

bool CGameControllerZcatch::CanJoinTeam(int Team, int NotThisId, char *pErrorReason, int ErrorReasonSize)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[NotThisId];
	if(!pPlayer)
		return false;

	if(pPlayer->m_IsDead && Team != TEAM_SPECTATORS)
	{
		str_copy(pErrorReason, "Wait until someone dies", ErrorReasonSize);
		return false;
	}
	return true;
}

void CGameControllerZcatch::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	CGameControllerInstagib::DoTeamChange(pPlayer, Team, DoChatMsg);

	CheckGameState();
}

void CGameControllerZcatch::CheckGameState()
{
	int ActivePlayers = NumActivePlayers();

	if(ActivePlayers >= g_Config.m_SvZcatchMinPlayers && CatchGameState() == ECatchGameState::WAITING_FOR_PLAYERS)
	{
		SendChatTarget(-1, "Enough players connected. Starting game!");
		SetCatchGameState(ECatchGameState::RUNNING);
	}
}

void CGameControllerZcatch::OnPlayerConnect(CPlayer *pPlayer)
{
	CGameControllerInstagib::OnPlayerConnect(pPlayer);

	CheckGameState();

	pPlayer->m_TeeInfos.m_ColorBody = GetBodyColor(pPlayer->m_Spree);
	pPlayer->m_TeeInfos.m_UseCustomColor = 1;

	m_aBodyColors[pPlayer->GetCid()] = pPlayer->m_TeeInfos.m_ColorBody;
	SendSkinBodyColor7(pPlayer->GetCid(), pPlayer->m_TeeInfos.m_ColorBody);

	if(CatchGameState() == ECatchGameState::WAITING_FOR_PLAYERS)
		SendChatTarget(pPlayer->GetCid(), "Waiting for more players to start the round.");
	else if(CatchGameState() == ECatchGameState::RELEASE_GAME)
		SendChatTarget(pPlayer->GetCid(), "This is a release game.");
}

void CGameControllerZcatch::OnPlayerDisconnect(class CPlayer *pDisconnectingPlayer, const char *pReason)
{
	CGameControllerInstagib::OnPlayerDisconnect(pDisconnectingPlayer, pReason);

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		pPlayer->m_vVictimIds.erase(std::remove(pPlayer->m_vVictimIds.begin(), pPlayer->m_vVictimIds.end(), pDisconnectingPlayer->GetCid()), pPlayer->m_vVictimIds.end());
	}
}

bool CGameControllerZcatch::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

bool CGameControllerZcatch::DoWincheckRound()
{
	if(CatchGameState() == ECatchGameState::RUNNING && NumNonDeadActivePlayers() <= 1)
	{
		EndRound();

		for(CPlayer *pPlayer : GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;

			// only release players that actually died
			// not all spectators
			if(pPlayer->m_IsDead)
				pPlayer->SetTeamRaw(TEAM_RED);
			pPlayer->m_IsDead = false;
		}

		return true;
	}
	return false;
}

void CGameControllerZcatch::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);
}
