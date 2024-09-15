#include <base/system.h>
#include <engine/shared/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

void CGameControllerPvp::OnPlayerConstruct(class CPlayer *pPlayer)
{
	pPlayer->m_IsDead = false;
	pPlayer->m_KillerId = -1;
	pPlayer->m_Stats.m_Spree = 0;
}

int64_t CPlayer::HandleMulti()
{
	int64_t TimeNow = time_timestamp();
	if((TimeNow - m_LastKillTime) > 5)
	{
		m_Multi = 1;
		return TimeNow;
	}
	m_Multi++;
	if(m_Stats.m_BestMulti < m_Multi)
		m_Stats.m_BestMulti = m_Multi;
	int Index = m_Multi - 2;
	m_aMultis[Index > MAX_MULTIS ? MAX_MULTIS : Index]++;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' multi x%d!",
		Server()->ClientName(GetCid()), m_Multi);
	GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	return TimeNow;
}

void CPlayer::SetTeamSpoofed(int Team, bool DoChatMsg)
{
	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorId = SPEC_FREEVIEW;

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientId = m_ClientId;
	Msg.m_Team = GameServer()->m_pController->GetPlayerTeam(this, true); // might be a fake team
	Msg.m_Silent = !DoChatMsg;
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(pPlayer && pPlayer->m_SpectatorId == m_ClientId)
				pPlayer->m_SpectatorId = SPEC_FREEVIEW;
		}
	}

	Server()->ExpireServerInfo();
}

void CPlayer::SetTeamNoKill(int Team, bool DoChatMsg)
{
	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorId = SPEC_FREEVIEW;

	// dead spec mode for 0.7
	if(!m_IsDead)
	{
		protocol7::CNetMsg_Sv_Team Msg;
		Msg.m_ClientId = m_ClientId;
		Msg.m_Team = m_Team;
		Msg.m_Silent = !DoChatMsg;
		Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
	}

	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(pPlayer && pPlayer->m_SpectatorId == m_ClientId)
				pPlayer->m_SpectatorId = SPEC_FREEVIEW;
		}
	}

	Server()->ExpireServerInfo();
}

void CPlayer::UpdateLastToucher(int ClientId)
{
	if(ClientId == GetCid())
		return;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		// covers the reset case when -1 is passed explicitly
		// to reset the last toucher when being hammered by a team mate in fng
		m_LastToucherId = -1;
		return;
	}

	// TODO: should we really reset the last toucher when we get shot by a team mate?
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(
		pPlayer &&
		GameServer()->m_pController &&
		GameServer()->m_pController->IsTeamplay() &&
		pPlayer->GetTeam() == GetTeam())
	{
		m_LastToucherId = -1;
		return;
	}

	m_LastToucherId = ClientId;
}
