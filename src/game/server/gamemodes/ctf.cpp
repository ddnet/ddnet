#include "ctf.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

CGameControllerCTF::CGameControllerCTF(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
}

CGameControllerCTF::~CGameControllerCTF() = default;

void CGameControllerCTF::Tick()
{
	CGameControllerInstagib::Tick();

	FlagTick(); // gctf
}

void CGameControllerCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);
}

int CGameControllerCTF::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponID);
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

bool CGameControllerCTF::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
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

void CGameControllerCTF::OnFlagReturn(CFlag *pFlag)
{
	CGameControllerInstagib::OnFlagReturn(pFlag);

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
	GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
	GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
}

void CGameControllerCTF::OnFlagGrab(class CFlag *pFlag)
{
	if(!g_Config.m_SvFastcap)
		return;
	if(!pFlag)
		return;
	if(!pFlag->IsAtStand())
		return;
	if(!pFlag->m_pCarrier)
		return;

	Teams().OnCharacterStart(pFlag->m_pCarrier->GetPlayer()->GetCID());
}

void CGameControllerCTF::OnFlagCapture(class CFlag *pFlag, float Time)
{
	if(!g_Config.m_SvFastcap)
		return;
	if(!pFlag)
		return;
	if(!pFlag->m_pCarrier)
		return;

	Teams().OnCharacterFinish(pFlag->m_pCarrier->GetPlayer()->GetCID());
}

void CGameControllerCTF::FlagTick()
{
	if(GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;

	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];

		if(!F)
			continue;

		//
		if(F->GetCarrier())
		{
			if(m_apFlags[fi ^ 1] && m_apFlags[fi ^ 1]->IsAtStand())
			{
				if(distance(F->GetPos(), m_apFlags[fi ^ 1]->GetPos()) < CFlag::ms_PhysSize + CCharacterCore::PhysicalSize())
				{
					// CAPTURE! \o/
					m_aTeamscore[fi ^ 1] += 100;
					F->GetCarrier()->GetPlayer()->AddScore(5);
					float Diff = Server()->Tick() - F->GetGrabTick();

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s' team=%d time=%.2f",
						F->GetCarrier()->GetPlayer()->GetCID(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
						F->GetCarrier()->GetPlayer()->GetTeam(),
						Diff / (float)Server()->TickSpeed());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					float CaptureTime = Diff / (float)Server()->TickSpeed();
					if(CaptureTime <= 60)
						str_format(aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s' (%d.%s%d seconds)", fi ? "blue" : "red",
							Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()), (int)CaptureTime % 60, ((int)(CaptureTime * 100) % 100) < 10 ? "0" : "", (int)(CaptureTime * 100) % 100);
					else
						str_format(
							aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s'", fi ? "blue" : "red",
							Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()));
					for(auto &pPlayer : GameServer()->m_apPlayers)
					{
						if(!pPlayer)
							continue;
						if(Server()->IsSixup(pPlayer->GetCID()))
							continue;

						GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
					}
					GameServer()->m_pController->OnFlagCapture(F, Diff);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_CAPTURE, fi, F->GetCarrier()->GetPlayer()->GetCID(), Diff, -1);
					GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
					for(int i = 0; i < 2; i++)
						for(auto &pFlag : m_apFlags)
							pFlag->Reset();
					// do a win check(capture could trigger win condition)
					if(DoWincheckMatch())
						return;
				}
			}
		}
		else
		{
			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(F->GetPos(), CFlag::ms_PhysSize, (CEntity **)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
					continue;

				if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == F->GetTeam())
				{
					// return the flag
					if(!F->IsAtStand())
					{
						CCharacter *pChr = apCloseCCharacters[i];
						pChr->GetPlayer()->IncrementScore();

						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s' team=%d",
							pChr->GetPlayer()->GetCID(),
							Server()->ClientName(pChr->GetPlayer()->GetCID()),
							pChr->GetPlayer()->GetTeam());
						GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
						GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
						GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
						F->Reset();
					}
				}
				else
				{
					// take the flag
					if(F->IsAtStand())
						m_aTeamscore[fi ^ 1]++;

					F->Grab(apCloseCCharacters[i]);

					F->GetCarrier()->GetPlayer()->IncrementScore();

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s' team=%d",
						F->GetCarrier()->GetPlayer()->GetCID(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
						F->GetCarrier()->GetPlayer()->GetTeam());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_GRAB, fi, -1);
					break;
				}
			}
		}
	}
	DoWincheckMatch();
}

void CGameControllerCTF::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);

	int FlagCarrierRed = FLAG_MISSING;
	if(m_apFlags[TEAM_RED])
	{
		if(m_apFlags[TEAM_RED]->m_AtStand)
			FlagCarrierRed = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		else
			FlagCarrierRed = FLAG_TAKEN;
	}

	int FlagCarrierBlue = FLAG_MISSING;
	if(m_apFlags[TEAM_BLUE])
	{
		if(m_apFlags[TEAM_BLUE]->m_AtStand)
			FlagCarrierBlue = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
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

bool CGameControllerCTF::DoWincheckMatch()
{
	CGameControllerInstagib::DoWincheckMatch();

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		if(m_SuddenDeath)
		{
			if(m_aTeamscore[TEAM_RED] / 100 != m_aTeamscore[TEAM_BLUE] / 100)
			{
				EndMatch();
				return true;
			}
		}
		else
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
			{
				EndMatch();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}
