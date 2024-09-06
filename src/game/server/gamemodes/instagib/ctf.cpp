#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "ctf.h"

CGameControllerInstaBaseCTF::CGameControllerInstaBaseCTF(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
}

CGameControllerInstaBaseCTF::~CGameControllerInstaBaseCTF() = default;

void CGameControllerInstaBaseCTF::Tick()
{
	CGameControllerPvp::Tick();

	FlagTick(); // ddnet-insta
}

void CGameControllerInstaBaseCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerPvp::OnCharacterSpawn(pChr);
}

int CGameControllerInstaBaseCTF::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerPvp::OnCharacterDeath(pVictim, pKiller, WeaponId);
	int HadFlag = 0;

	// drop flags
	for(auto &F : m_apFlags)
	{
		if(F && pKiller && pKiller->GetCharacter() && F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if(F && F->GetCarrier() == pVictim)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
			GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_DROP, -1);
			/*pVictim->GetPlayer()->m_Rainbow = false;
			pVictim->GetPlayer()->m_TeeInfos.m_ColorBody = pVictim->GetPlayer()->m_ColorBodyOld;
			pVictim->GetPlayer()->m_TeeInfos.m_ColorFeet = pVictim->GetPlayer()->m_ColorFeetOld;*/
			F->m_DropTick = Server()->Tick();
			F->SetCarrier(0);
			F->m_Vel = vec2(0, 0);

			HadFlag |= 1;
		}
		if(F && F->GetCarrier() == pVictim)
			F->SetCarrier(0);
	}

	return HadFlag;
}

bool CGameControllerInstaBaseCTF::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);

	const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED)
		Team = TEAM_RED;
	if(Index == ENTITY_FLAGSTAND_BLUE)
		Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team);
	F->m_StandPos = Pos;
	F->m_Pos = Pos;
	m_apFlags[Team] = F;
	GameServer()->m_World.InsertEntity(F);
	return true;
}

void CGameControllerInstaBaseCTF::OnFlagReturn(CFlag *pFlag)
{
	CGameControllerPvp::OnFlagReturn(pFlag);

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
	GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
	GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
}

void CGameControllerInstaBaseCTF::OnFlagGrab(class CFlag *pFlag)
{
	if(!pFlag)
		return;
	if(!pFlag->IsAtStand())
		return;
	if(!pFlag->m_pCarrier)
		return;

	CPlayer *pPlayer = pFlag->m_pCarrier->GetPlayer();
	pPlayer->m_FlagGrabs++;

	if(g_Config.m_SvFastcap)
		Teams().OnCharacterStart(pPlayer->GetCid());
}

void CGameControllerInstaBaseCTF::OnFlagCapture(class CFlag *pFlag, float Time)
{
	if(!pFlag)
		return;
	if(!pFlag->m_pCarrier)
		return;

	CPlayer *pPlayer = pFlag->m_pCarrier->GetPlayer();
	pPlayer->m_FlagCaptures++;

	if(g_Config.m_SvFastcap)
		Teams().OnCharacterFinish(pPlayer->GetCid());
}

void CGameControllerInstaBaseCTF::FlagTick()
{
	if(GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;

	for(int FlagColor = 0; FlagColor < 2; FlagColor++)
	{
		CFlag *pFlag = m_apFlags[FlagColor];

		if(!pFlag)
			continue;

		//
		if(pFlag->GetCarrier())
		{
			if(m_apFlags[FlagColor ^ 1] && m_apFlags[FlagColor ^ 1]->IsAtStand())
			{
				if(distance(pFlag->GetPos(), m_apFlags[FlagColor ^ 1]->GetPos()) < CFlag::ms_PhysSize + CCharacterCore::PhysicalSize())
				{
					// CAPTURE! \o/
					m_aTeamscore[FlagColor ^ 1] += 100;
					pFlag->GetCarrier()->GetPlayer()->AddScore(5);
					float Diff = Server()->Tick() - pFlag->GetGrabTick();

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s' team=%d time=%.2f",
						pFlag->GetCarrier()->GetPlayer()->GetCid(),
						Server()->ClientName(pFlag->GetCarrier()->GetPlayer()->GetCid()),
						pFlag->GetCarrier()->GetPlayer()->GetTeam(),
						Diff / (float)Server()->TickSpeed());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					float CaptureTime = Diff / (float)Server()->TickSpeed();
					if(CaptureTime <= 60)
						str_format(aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s' (%d.%s%d seconds)", FlagColor ? "blue" : "red",
							Server()->ClientName(pFlag->GetCarrier()->GetPlayer()->GetCid()), (int)CaptureTime % 60, ((int)(CaptureTime * 100) % 100) < 10 ? "0" : "", (int)(CaptureTime * 100) % 100);
					else
						str_format(
							aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s'", FlagColor ? "blue" : "red",
							Server()->ClientName(pFlag->GetCarrier()->GetPlayer()->GetCid()));
					for(auto &pPlayer : GameServer()->m_apPlayers)
					{
						if(!pPlayer)
							continue;
						if(Server()->IsSixup(pPlayer->GetCid()))
							continue;

						GameServer()->SendChatTarget(pPlayer->GetCid(), aBuf);
					}
					GameServer()->m_pController->OnFlagCapture(pFlag, Diff);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_CAPTURE, FlagColor, pFlag->GetCarrier()->GetPlayer()->GetCid(), Diff, -1);
					GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
					for(int i = 0; i < 2; i++)
						for(auto &pFlag : m_apFlags)
							pFlag->Reset();
					// do a win check(capture could trigger win condition)
					if(DoWincheckRound())
						return;
				}
			}
		}
		else
		{
			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(pFlag->GetPos(), CFlag::ms_PhysSize, (CEntity **)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(pFlag->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
					continue;

				// only allow flag grabs in team 0
				if(GameServer()->GetDDRaceTeam(apCloseCCharacters[i]->GetPlayer()->GetCid()))
					continue;

				if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == pFlag->GetTeam())
				{
					// return the flag
					if(!pFlag->IsAtStand())
					{
						CCharacter *pChr = apCloseCCharacters[i];
						pChr->GetPlayer()->IncrementScore();

						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s' team=%d",
							pChr->GetPlayer()->GetCid(),
							Server()->ClientName(pChr->GetPlayer()->GetCid()),
							pChr->GetPlayer()->GetTeam());
						GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
						GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
						GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
						pFlag->Reset();
					}
				}
				else
				{
					// take the flag
					if(pFlag->IsAtStand())
						m_aTeamscore[FlagColor ^ 1]++;

					pFlag->Grab(apCloseCCharacters[i]);

					pFlag->GetCarrier()->GetPlayer()->IncrementScore();

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s' team=%d",
						pFlag->GetCarrier()->GetPlayer()->GetCid(),
						Server()->ClientName(pFlag->GetCarrier()->GetPlayer()->GetCid()),
						pFlag->GetCarrier()->GetPlayer()->GetTeam());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_GRAB, FlagColor, -1);
					break;
				}
			}
		}
	}
	DoWincheckRound();
}

void CGameControllerInstaBaseCTF::Snap(int SnappingClient)
{
	CGameControllerPvp::Snap(SnappingClient);

	int FlagCarrierRed = FLAG_MISSING;
	if(m_apFlags[TEAM_RED])
	{
		if(m_apFlags[TEAM_RED]->m_AtStand)
			FlagCarrierRed = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCid();
		else
			FlagCarrierRed = FLAG_TAKEN;
	}

	int FlagCarrierBlue = FLAG_MISSING;
	if(m_apFlags[TEAM_BLUE])
	{
		if(m_apFlags[TEAM_BLUE]->m_AtStand)
			FlagCarrierBlue = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCid();
		else
			FlagCarrierBlue = FLAG_TAKEN;
	}

	if(Server()->IsSixup(SnappingClient))
	{
		protocol7::CNetObj_GameDataFlag *pGameDataObj = Server()->SnapNewItem<protocol7::CNetObj_GameDataFlag>(0);
		if(!pGameDataObj)
			return;

		pGameDataObj->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataObj->m_FlagCarrierBlue = FlagCarrierBlue;
	}
	else
	{
		CNetObj_GameData *pGameDataObj = Server()->SnapNewItem<CNetObj_GameData>(0);
		if(!pGameDataObj)
			return;

		pGameDataObj->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataObj->m_FlagCarrierBlue = FlagCarrierBlue;

		pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
		pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];
	}
}

bool CGameControllerInstaBaseCTF::DoWincheckRound()
{
	CGameControllerPvp::DoWincheckRound();

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		if(m_SuddenDeath)
		{
			if(m_aTeamscore[TEAM_RED] / 100 != m_aTeamscore[TEAM_BLUE] / 100)
			{
				EndRound();
				return true;
			}
		}
		else
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
			{
				EndRound();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}
