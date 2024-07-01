// made by fokkonaut

#include "teams.h"
#include "entities/character.h"
#include "gamecontroller.h"
#include "player.h"
#include <base/system.h>
#include "gamemodes/DDRace.h"

void CPlayerMapping::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aMap[i].Init(i, this);
}

void CPlayerMapping::Tick()
{
	UpdatePlayerMap(-1);

	// Translate StrongWeakID to clamp it to 64 players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!GameServer()->m_apPlayers[i] || GameServer()->Server()->IsDebugDummy(i))
			continue;

		int StrongWeakID = 0;
		for (CCharacter *pChar = (CCharacter *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
		{
			int ID = pChar->GetPlayer()->GetCid();
			if (Server()->Translate(ID, i))
			{
				GameServer()->m_apPlayers[i]->m_aStrongWeakID[ID] = StrongWeakID;
				StrongWeakID++;
			}
		}
	}
}

void CPlayerMapping::PlayerMap::Init(int ClientID, CPlayerMapping *pPlayerMapping)
{
	m_ClientID = ClientID;
	m_pPlayerMapping = pPlayerMapping;
	m_pMap = m_pPlayerMapping->Server()->GetIdMap(m_ClientID);
	m_pReverseMap = m_pPlayerMapping->Server()->GetReverseIdMap(m_ClientID);
	m_UpdateTeamsState = false;
	ResetSeeOthers();
}

CPlayer *CPlayerMapping::PlayerMap::GetPlayer()
{
	return m_pPlayerMapping->GameServer()->m_apPlayers[m_ClientID];
}

void CPlayerMapping::PlayerMap::InitPlayer(bool Rejoin)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aReserved[i] = false;

	// make sure no rests from before are in the client, so we can freshly start and insert our stuff
	if (Rejoin)
	{
		m_UpdateTeamsState = true; // to get flag spectators back and all teams aswell

		for (int i = 0; i < LEGACY_MAX_CLIENTS; i++)
			Remove(i);
	}

	for (int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		m_pMap[i] = -1;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_pReverseMap[i] = -1;

	if (m_pPlayerMapping->Server()->IsDebugDummy(m_ClientID))
		return; // just need to initialize the arrays

	int NextFreeID = 0;
	NETADDR OwnAddr, Addr;
	m_pPlayerMapping->Server()->GetClientAddr(m_ClientID, &OwnAddr);
	while (1)
	{
		bool Break = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_pPlayerMapping->Server()->IsDebugDummy(i) || i == m_ClientID)
				continue;

			m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp_noport(&OwnAddr, &Addr) == 0)
			{
				if (m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] == NextFreeID)
				{
					NextFreeID++;
					Break = false;
				}
			}
		}

		if (Break)
			break;
	}

	m_NumReserved = 1;
	m_pMap[LEGACY_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs

	// see others in spec menu
	m_NumReserved++;
	m_pMap[m_pPlayerMapping->GetSeeOthersID(m_ClientID)] = -1;
	m_TotalOverhang = 0;

	if (m_pPlayerMapping->Server()->IsSixup(m_ClientID))
	{
		protocol7::CNetMsg_Sv_ClientInfo FakeInfo;
		FakeInfo.m_ClientId = LEGACY_MAX_CLIENTS-1;
		FakeInfo.m_Local = 0;
		FakeInfo.m_Team = TEAM_BLUE;
		FakeInfo.m_pName = " ";
		FakeInfo.m_pClan = "";
		FakeInfo.m_Country = -1;
		FakeInfo.m_Silent = 1;
		for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
		{
			FakeInfo.m_apSkinPartNames[p] = "standard";
			FakeInfo.m_aUseCustomColors[p] = 0;
			FakeInfo.m_aSkinPartColors[p] = 0;
		}
		m_pPlayerMapping->Server()->SendPackMsg(&FakeInfo, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
		// see others
		UpdateSeeOthers();
	}
	else
	{
		if (m_pPlayerMapping->GameServer()->FlagsUsed())
		{
			m_NumReserved += 2;
			m_pMap[SPEC_SELECT_FLAG_RED] = -1;
			m_pMap[SPEC_SELECT_FLAG_BLUE] = -1;
		}
	}

	if (NextFreeID < GetMapSize())
	{
		m_aReserved[m_ClientID] = true;
		Add(NextFreeID, m_ClientID);
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!m_pPlayerMapping->GameServer()->m_apPlayers[i]  || m_pPlayerMapping->Server()->IsDebugDummy(i) || i == m_ClientID)
			continue;

		m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
		if (net_addr_comp_noport(&OwnAddr, &Addr) != 0)
			continue;

		// update us with other same ip player infos
		if (m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] < GetMapSize())
		{
			m_aReserved[i] = true;
			Add(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if (NextFreeID < m_pPlayerMapping->m_aMap[i].GetMapSize())
		{
			m_pPlayerMapping->m_aMap[i].m_aReserved[m_ClientID] = true;
			m_pPlayerMapping->m_aMap[i].Add(NextFreeID, m_ClientID);
		}
	}
}

void CPlayerMapping::PlayerMap::Add(int MapID, int ClientID)
{
	if (MapID == -1 || ClientID == -1 || m_pReverseMap[ClientID] == MapID)
		return;

	Remove(m_pReverseMap[ClientID]);

	int OldClientID = Remove(MapID);
	if ((OldClientID == -1 && m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID) > 0)
		|| (OldClientID != -1 && m_pPlayerMapping->GameServer()->GetDDRaceTeam(OldClientID) != m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID)))
		m_UpdateTeamsState = true;

	if (m_aReserved[ClientID])
		m_ResortReserved = true;

	m_pMap[MapID] = ClientID;
	m_pReverseMap[ClientID] = MapID;
	GetPlayer()->SendConnect(MapID, ClientID);
}

int CPlayerMapping::PlayerMap::Remove(int MapID)
{
	if (MapID == -1)
		return -1;

	int ClientID = m_pMap[MapID];
	if (ClientID != -1)
	{
		if (m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID) > 0)
			m_UpdateTeamsState = true;

		if (m_aReserved[ClientID])
			m_ResortReserved = true;

		GetPlayer()->SendDisconnect(MapID);
		m_pReverseMap[ClientID] = -1;
		m_pMap[MapID] = -1;
	}
	return ClientID;
}

void CPlayerMapping::PlayerMap::Update()
{
	if (!m_pPlayerMapping->Server()->ClientIngame(m_ClientID) || !GetPlayer() || m_pPlayerMapping->Server()->IsDebugDummy(m_ClientID))
		return;

	bool ResortReserved = m_ResortReserved;
	m_ResortReserved = false;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == m_ClientID)
			continue;

		CPlayer *pPlayer = m_pPlayerMapping->GameServer()->m_apPlayers[i];

		if (!m_pPlayerMapping->Server()->ClientIngame(i) || !pPlayer)
		{
			Remove(m_pReverseMap[i]);
			m_aReserved[i] = false;
			continue;
		}

		if (m_aReserved[i])
		{
			NETADDR OwnAddr, Addr;
			m_pPlayerMapping->Server()->GetClientAddr(m_ClientID, &OwnAddr);
			m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp_noport(&OwnAddr, &Addr) != 0)
			{
				if (ResortReserved || !m_pPlayerMapping->GameServer()->GetDDRaceTeam(i)) // condition to unset reserved slot
					m_aReserved[i] = false;
			}
			continue;
		}
		else if (ResortReserved)
			continue;

		int Insert = -1;
		if (m_pPlayerMapping->GameServer()->GetDDRaceTeam(i))
		{
			for (int j = 0; j < GetMapSize()-m_NumSeeOthers; j++)
			{
				int CID = m_pMap[j];
				if (CID == -1 || !m_aReserved[CID])
				{
					Insert = j;
					m_aReserved[i] = true;
					break;
				}
			}
		}
		else if (m_pReverseMap[i] != -1)
		{
			Insert = m_pReverseMap[i];
		}
		else
		{
			for (int j = 0; j < GetMapSize()-m_NumSeeOthers; j++)
				if (m_pMap[j] == -1)
				{
					Insert = j;
					break;
				}
		}

		if (Insert != -1)
		{
			Add(Insert, i);
		}
		else if (pPlayer->GetCharacter() && !pPlayer->GetCharacter()->NetworkClipped(m_ClientID))
		{
			InsertNextEmpty(i);
		}
	}

	if (m_UpdateTeamsState)
	{
		((CGameControllerDDRace *)m_pPlayerMapping->GameServer()->m_pController)->Teams().SendTeamsState(m_ClientID);
		m_UpdateTeamsState = false;
	}
}

void CPlayerMapping::PlayerMap::InsertNextEmpty(int ClientID)
{
	if (ClientID == -1 || m_pReverseMap[ClientID] != -1)
		return;

	for (int i = 0; i < GetMapSize()-m_NumSeeOthers; i++)
	{
		int CID = m_pMap[i];
		if (CID != -1 && m_aReserved[CID])
			continue;

		if (CID == -1 || (!m_pPlayerMapping->GameServer()->GetPlayerChar(CID) || m_pPlayerMapping->GameServer()->GetPlayerChar(CID)->NetworkClipped(m_ClientID)))
		{
			Add(i, ClientID);
			break;
		}
	}
}

int CPlayerMapping::GetSeeOthersID(int ClientID)
{
	// 0.7 or if no flags been used on the map
	if (Server()->IsSixup(ClientID) || !GameServer()->FlagsUsed())
		return LEGACY_MAX_CLIENTS - 2;
	// 0.6 AND flags
	return SPEC_SELECT_FLAG_BLUE - 1;
}

void CPlayerMapping::DoSeeOthers(int ClientID)
{
	m_aMap[ClientID].DoSeeOthers();
}

void CPlayerMapping::ResetSeeOthers(int ClientID)
{
	m_aMap[ClientID].ResetSeeOthers();
}

int CPlayerMapping::GetTotalOverhang(int ClientID)
{
	return m_aMap[ClientID].m_TotalOverhang;
}

void CPlayerMapping::UpdatePlayerMap(int ClientID)
{
	if (ClientID == -1)
	{
		bool Update = Server()->Tick() % Config()->m_SvMapUpdateRate == 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// Calculate overhang every tick, not only when the map updates
			int Overhang = maximum(0, Server()->ClientCount() - m_aMap[i].GetMapSize());
			if (Overhang != m_aMap[i].m_TotalOverhang)
			{
				m_aMap[i].m_TotalOverhang = Overhang;
				if (m_aMap[i].m_TotalOverhang <= 0 && m_aMap[i].m_SeeOthersState != PlayerMap::SSeeOthers::STATE_NONE)
					m_aMap[i].ResetSeeOthers();

				m_aMap[i].UpdateSeeOthers();
				m_aMap[i].m_UpdateTeamsState = true;
			}

			if (Update)
			{
				m_aMap[i].Update();
			}
		}
	}
	else
	{
		m_aMap[ClientID].Update();
	}
}

int CPlayerMapping::GetSeeOthersInd(int ClientID, int MapID)
{
	if (m_aMap[ClientID].m_TotalOverhang && MapID == GetSeeOthersID(ClientID))
		return SEE_OTHERS_IND_BUTTON;
	if (m_aMap[ClientID].m_NumSeeOthers && MapID >= m_aMap[ClientID].GetMapSize() - m_aMap[ClientID].m_NumSeeOthers)
		return SEE_OTHERS_IND_PLAYER;
	return -1;
}

const char *CPlayerMapping::GetSeeOthersName(int ClientID)
{
	static char aName[MAX_NAME_LENGTH];
	int State = m_aMap[ClientID].m_SeeOthersState;
	const char *pDot = "\xe2\x8b\x85";

	if (State == PlayerMap::SSeeOthers::STATE_PAGE_FIRST)
	{
		if (m_aMap[ClientID].m_TotalOverhang > PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS)
			str_format(aName, sizeof(aName), "%s 1/2", pDot);
		else
			str_format(aName, sizeof(aName), "%s Close", pDot);
	}
	else if (State == PlayerMap::SSeeOthers::STATE_PAGE_SECOND)
	{
		str_format(aName, sizeof(aName), "%s 2/2 | Close", pDot);
	}
	else
	{
		str_format(aName, sizeof(aName), "%s %d others", pDot, m_aMap[ClientID].m_TotalOverhang);
	}
	return aName;
}

void CPlayerMapping::PlayerMap::CycleSeeOthers()
{
	if (m_TotalOverhang <= 0)
		return;

	for (int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		if (m_pMap[i] != -1)
			m_aWasSeeOthers[m_pMap[i]] = true;

	int Size = minimum(m_TotalOverhang, (int)PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS);
	int Added = 0;
	int MapID = GetMapSize()-1;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_aWasSeeOthers[i])
			continue;

		Add(MapID, i);
		m_aWasSeeOthers[i] = true;
		Added++;
		MapID--;

		if (Added >= Size)
			break;
	}

	m_NumSeeOthers = Added;
}

void CPlayerMapping::PlayerMap::DoSeeOthers()
{
	if (m_TotalOverhang <= 0)
		return;

	m_SeeOthersState++;
	UpdateSeeOthers();

	CycleSeeOthers();

	// aggressively trigger reset now
	if (m_NumSeeOthers == 0)
	{
		// Reset these for the next cycle so we can get the fresh page we had before
		for (int i = 0; i < MAX_CLIENTS; i++)
			m_aWasSeeOthers[i] = false;
		CycleSeeOthers();
		ResetSeeOthers();
	}

	// instantly update so we dont have to wait for the map to be executed
	m_UpdateTeamsState = true;
	Update();
}

void CPlayerMapping::PlayerMap::ResetSeeOthers()
{
	m_SeeOthersState = SSeeOthers::STATE_NONE;
	m_NumSeeOthers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aWasSeeOthers[i] = false;
	m_UpdateTeamsState = true;
	UpdateSeeOthers();
}

void CPlayerMapping::PlayerMap::UpdateSeeOthers()
{
	if (!m_pPlayerMapping->Server()->IsSixup(m_ClientID))
		return;

	int SeeOthersID = m_pPlayerMapping->GetSeeOthersID(m_ClientID);
	protocol7::CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientId = SeeOthersID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientId = SeeOthersID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = TEAM_BLUE;
	NewClientInfoMsg.m_pName = m_pPlayerMapping->GetSeeOthersName(m_ClientID);
	NewClientInfoMsg.m_pClan = "";
	NewClientInfoMsg.m_Country = -1;
	NewClientInfoMsg.m_Silent = 1;
	for (int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		bool Colored = p == protocol7::SKINPART_BODY || p == protocol7::SKINPART_FEET;
		NewClientInfoMsg.m_apSkinPartNames[p] = "standard";
		NewClientInfoMsg.m_aUseCustomColors[p] = (int)Colored;
		NewClientInfoMsg.m_aSkinPartColors[p] = Colored ? 5963600 : 0;
	}

	m_pPlayerMapping->Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
	m_pPlayerMapping->Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
}
