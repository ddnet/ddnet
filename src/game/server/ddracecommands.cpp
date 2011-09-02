/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/server/server.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/generated/nethash.cpp>
#if defined(CONF_SQL)
	#include <game/server/score/sql_score.h>
#endif

bool CheckClientID(int ClientID);

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, -1, 0);
}

void CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, 1, 0);
}

void CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, 1);
}

void CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, -1);
}

void CGameContext::ConMove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0), pResult->GetInteger(1));
}

void CGameContext::ConMoveRaw(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0), pResult->GetInteger(1), true);
}

void CGameContext::MoveCharacter(int ClientID, int X, int Y, bool Raw)
{
	CCharacter* pChr = GetPlayerChar(ClientID);

	if(!pChr)
		return;

	pChr->Core()->m_Pos.x += ((Raw) ? 1 : 32) * X;
	pChr->Core()->m_Pos.y += ((Raw) ? 1 : 32) * Y;
	pChr->m_DDRaceState = DDRACE_CHEAT;
}


void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	int Victim = pResult->GetInteger(0);

	if(pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->KillCharacter(WEAPON_GAME);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s was killed by %s", pSelf->Server()->ClientName(Victim), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameContext::ConNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_NINJA, false);
}

void CGameContext::ConSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if(pChr && !pChr->m_Super)
	{
		pChr->m_Super = true;
		pChr->UnFreeze();
		pChr->m_TeamBeforeSuper = pChr->Team();
		pChr->Teams()->SetCharacterTeam(pResult->m_ClientID, TEAM_SUPER);
		pChr->m_DDRaceState = DDRACE_CHEAT;
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if(pChr && pChr->m_Super)
	{
		pChr->m_Super = false;
		pChr->Teams()->SetForceCharacterTeam(pResult->m_ClientID, pChr->m_TeamBeforeSuper);
	}
}

void CGameContext::ConShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, false);
}

void CGameContext::ConGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, false);
}

void CGameContext::ConRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, false);
}

void CGameContext::ConWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, false);
}

void CGameContext::ConUnShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, true);
}

void CGameContext::ConUnGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, true);
}

void CGameContext::ConUnRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, true);
}

void CGameContext::ConUnWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, true);
}

void CGameContext::ConAddWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), false);
}

void CGameContext::ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), true);
}

void CGameContext::ModifyWeapons(IConsole::IResult *pResult, void *pUserData, int Weapon, bool Remove)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	int ClientID = pResult->m_ClientID;
	if(clamp(Weapon, -1, NUM_WEAPONS - 1) != Weapon)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "invalid weapon id");
		return;
	}

	CCharacter* pChr = GetPlayerChar(ClientID);
	if(!pChr)
		return;

	if(Weapon == -1)
	{
		if(Remove && (pChr->GetActiveWeapon() == WEAPON_SHOTGUN || pChr->GetActiveWeapon() == WEAPON_GRENADE || pChr->GetActiveWeapon() == WEAPON_RIFLE))
			pChr->SetActiveWeapon(WEAPON_GUN);

		if(Remove)
		{
			pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
			pChr->SetWeaponGot(WEAPON_GRENADE, false);
			pChr->SetWeaponGot(WEAPON_RIFLE, false);
		}
		else
			pChr->GiveAllWeapons();
	}
	else if(Weapon != WEAPON_NINJA)
	{
		if(Remove && pChr->GetActiveWeapon() == Weapon)
			pChr->SetActiveWeapon(WEAPON_GUN);

		if(Remove)
			pChr->SetWeaponGot(Weapon, false);
		else
			pChr->GiveWeapon(Weapon, -1);
	}
	else
	{
		if(Remove)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "you can't remove ninja");
			return;
		}

		pChr->GiveNinja();
	}

	pChr->m_DDRaceState =	DDRACE_CHEAT;
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	int TeleTo = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[TeleTo])
	{
		{
			CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
			if(pChr)
			{
				pChr->Core()->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
				pChr->m_DDRaceState = DDRACE_CHEAT;
			}
		}
	}
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];

	if(!pPlayer || (pPlayer->m_LastKill && pPlayer->m_LastKill + pSelf->Server()->TickSpeed() * g_Config.m_SvKillDelay > pSelf->Server()->Tick()))
		return;

	pPlayer->m_LastKill = pSelf->Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	//pPlayer->m_RespawnTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * g_Config.m_SvSuicidePenalty;
}

void CGameContext::ConForcePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID)) return;
	CServer* pServ = (CServer*)pSelf->Server();
	int Victim = pResult->GetInteger(0);
	int Seconds = 0;
	char aBuf[128];

	if(pResult->NumArguments() > 0 && pResult->m_ClientID < 0)
		Seconds = clamp(pResult->GetInteger(0), 0, 15);

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if(!pPlayer || (!Seconds && pResult->m_ClientID >= 0))
		return;

	CCharacter* pChr = pPlayer->GetCharacter();
	if(!pPlayer->GetTeam() && pChr && !pPlayer->m_InfoSaved && pResult->m_ClientID < 0)
	{
		pPlayer->SaveCharacter();
		pPlayer->m_InfoSaved = true;
		pPlayer->SetTeam(TEAM_SPECTATORS);
		pPlayer->m_ForcePauseTime = Seconds*pServ->TickSpeed();
	}
	else
	{
		pPlayer->m_ForcePauseTime = Seconds*pServ->TickSpeed();
	}
	if(pResult->m_ClientID < 0)
		str_format(aBuf, sizeof(aBuf), "'%s' has been force-paused for %d seconds", pServ->ClientName(Victim), Seconds);
	else
		str_format(aBuf, sizeof(aBuf), "Force-pause of '%s' have been removed by '%s'", pServ->ClientName(Victim), pServ->ClientName(pResult->m_ClientID));
	pSelf->SendChat(-1, CHAT_ALL, aBuf);
}

void CGameContext::Mute(IConsole::IResult *pResult, NETADDR *Addr, int Secs, const char *pDisplayName)
{
	char aBuf[128];
	int Found = 0;
	// find a matching mute for this ip, update expiration time if found
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(net_addr_comp(&m_aMutes[i].m_Addr, Addr) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
			Found = 1;
		}
	}

	if(!Found) // nothing found so far, find a free slot..
	{
		if(m_NumMutes < MAX_MUTES)
		{
			m_aMutes[m_NumMutes].m_Addr = *Addr;
			m_aMutes[m_NumMutes].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
			m_NumMutes++;
			Found = 1;
		}
	}
	if(Found)
	{
		str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds.", pDisplayName, Secs);
		SendChat(-1, CHAT_ALL, aBuf);
	}
	else if(pResult)// no free slot found
		// pResult->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
		// Todo(Shereef Marzouk: Fix this
		dbg_msg("mutes", "mute array is full");
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Use either 'muteid <client_id> <seconds>' or 'muteip <ip> <seconds>'");
}

// mute through client id
void CGameContext::ConMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetInteger(0);

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(0), 1, 86400), pSelf->Server()->ClientName(Victim));
}

// mute through ip, arguments reversed to workaround parsing
void CGameContext::ConMuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Invalid network address to mute");
	}
	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(1), 1, 86400), pResult->GetString(0));
}

// unmute by mute list index
void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[64];
	int Victim = pResult->GetInteger(0);

	if(Victim < 0 || Victim >= pSelf->m_NumMutes)
		return;
	
	pSelf->m_NumMutes--;
	pSelf->m_aMutes[Victim] = pSelf->m_aMutes[pSelf->m_NumMutes];

	net_addr_str(&pSelf->m_aMutes[Victim].m_Addr, aIpBuf, sizeof(aIpBuf));
	str_format(aBuf, sizeof(aBuf), "Unmuted %s" , aIpBuf);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
}

// list mutes
void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "Active mutes:");
	for(int i = 0; i < pSelf->m_NumMutes; i++)
	{
		net_addr_str(&pSelf->m_aMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf));
		str_format(aBuf, sizeof aBuf, "%d: \"%s\", %d seconds left", i, aIpBuf, (pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
	}
}
