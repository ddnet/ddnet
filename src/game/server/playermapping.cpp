#include "entities/character.h"
#include "gamecontroller.h"
#include "gamemodes/DDRace.h"
#include "player.h"
#include "teams.h"
#include <base/system.h>

void CPlayerMapping::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aMap[i].Init(i, this);
}

void CPlayerMapping::Tick()
{
	UpdatePlayerMap(-1);

	// Translate StrongWeakId to clamp it to 64 players
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameServer()->m_apPlayers[i])
			continue;

		int StrongWeakId = 0;
		for(CCharacter *pChar = (CCharacter *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
		{
			int Id = pChar->GetPlayer()->GetCid();
			if(Server()->Translate(Id, i))
			{
				GameServer()->m_apPlayers[i]->m_aStrongWeakId[Id] = StrongWeakId;
				StrongWeakId++;
			}
		}
	}
}

void CPlayerMapping::CPlayerMap::Init(int ClientId, CPlayerMapping *pPlayerMapping)
{
	m_ClientId = ClientId;
	m_pPlayerMapping = pPlayerMapping;
	m_pMap = m_pPlayerMapping->Server()->GetIdMap(m_ClientId);
	m_pReverseMap = m_pPlayerMapping->Server()->GetReverseIdMap(m_ClientId);
	m_UpdateTeamsState = false;
	m_NumReserved = 0;
	m_TotalOverhang = 0;
	ResetSeeOthers();
}

CPlayer *CPlayerMapping::CPlayerMap::GetPlayer() const
{
	return m_pPlayerMapping->GameServer()->m_apPlayers[m_ClientId];
}

void CPlayerMapping::CPlayerMap::InitPlayer(bool Rejoin)
{
	std::fill(std::begin(m_aReserved), std::end(m_aReserved), false);

	// make sure no rests from before are in the client, so we can freshly start and insert our stuff
	if(Rejoin)
	{
		m_UpdateTeamsState = true; // to get flag spectators back and all teams aswell

		for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
			Remove(i);
	}

	for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		m_pMap[i] = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_pReverseMap[i] = -1;

	int NextFreeId = 0;
	const NETADDR *pOwnAddr = m_pPlayerMapping->Server()->ClientAddr(m_ClientId);
	for(bool Finished = false; !Finished;)
	{
		Finished = true;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_pPlayerMapping->GameServer()->m_apPlayers[i])
				continue;

			const NETADDR *pAddr = m_pPlayerMapping->Server()->ClientAddr(i);
			if(net_addr_comp_noport(pOwnAddr, pAddr) == 0)
			{
				if(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] == NextFreeId)
				{
					NextFreeId++;
					Finished = false;
				}
			}
		}
	}

	m_NumReserved = 1;
	m_pMap[LEGACY_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs

	// see others in spec menu
	m_NumReserved++;
	m_pMap[m_pPlayerMapping->GetSeeOthersId(m_ClientId)] = -1;
	m_TotalOverhang = 0;

	if(m_pPlayerMapping->Server()->IsSixup(m_ClientId))
	{
		protocol7::CNetMsg_Sv_ClientInfo FakeInfo;
		FakeInfo.m_ClientId = LEGACY_MAX_CLIENTS - 1;
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
		m_pPlayerMapping->Server()->SendPackMsg(&FakeInfo, MSGFLAG_VITAL | MSGFLAG_NORECORD | MSGFLAG_NOTRANSLATE, m_ClientId);
		// see others
		UpdateSeeOthers();
	}

	if(NextFreeId < GetMapSize())
	{
		m_aReserved[m_ClientId] = true;
		Add(NextFreeId, m_ClientId);
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pPlayerMapping->GameServer()->m_apPlayers[i] || i == m_ClientId)
			continue;

		const NETADDR *pAddr = m_pPlayerMapping->Server()->ClientAddr(i);
		if(net_addr_comp_noport(pOwnAddr, pAddr) != 0)
			continue;

		// update us with other same ip player infos
		if(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] < GetMapSize())
		{
			m_aReserved[i] = true;
			Add(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if(NextFreeId < m_pPlayerMapping->m_aMap[i].GetMapSize())
		{
			m_pPlayerMapping->m_aMap[i].m_aReserved[m_ClientId] = true;
			m_pPlayerMapping->m_aMap[i].Add(NextFreeId, m_ClientId);
		}
	}
}

void CPlayerMapping::CPlayerMap::Add(int MapId, int ClientId)
{
	if(MapId == -1 || ClientId == -1 || m_pReverseMap[ClientId] == MapId)
		return;

	Remove(m_pReverseMap[ClientId]);

	int OldClientId = Remove(MapId);
	if((OldClientId == -1 && m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientId) > 0) || (OldClientId != -1 && m_pPlayerMapping->GameServer()->GetDDRaceTeam(OldClientId) != m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientId)))
		m_UpdateTeamsState = true;

	if(m_aReserved[ClientId])
		m_ResortReserved = true;

	m_pMap[MapId] = ClientId;
	m_pReverseMap[ClientId] = MapId;
	GetPlayer()->SendConnect(MapId, ClientId);
}

int CPlayerMapping::CPlayerMap::Remove(int MapId)
{
	if(MapId == -1)
		return -1;

	int ClientId = m_pMap[MapId];
	if(ClientId != -1)
	{
		if(m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientId) > 0)
			m_UpdateTeamsState = true;

		if(m_aReserved[ClientId])
			m_ResortReserved = true;

		GetPlayer()->SendDisconnect(MapId);
		m_pReverseMap[ClientId] = -1;
		m_pMap[MapId] = -1;
	}
	return ClientId;
}

void CPlayerMapping::CPlayerMap::Update()
{
	if(!m_pPlayerMapping->Server()->ClientIngame(m_ClientId) || !GetPlayer())
		return;

	bool ResortReserved = m_ResortReserved;
	m_ResortReserved = false;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == m_ClientId)
			continue;

		CPlayer *pPlayer = m_pPlayerMapping->GameServer()->m_apPlayers[i];

		if(!m_pPlayerMapping->Server()->ClientIngame(i) || !pPlayer)
		{
			Remove(m_pReverseMap[i]);
			m_aReserved[i] = false;
			continue;
		}

		if(m_aReserved[i])
		{
			const NETADDR *pOwnAddr = m_pPlayerMapping->Server()->ClientAddr(m_ClientId);
			const NETADDR *pAddr = m_pPlayerMapping->Server()->ClientAddr(i);
			if(net_addr_comp_noport(pOwnAddr, pAddr) != 0)
			{
				if(ResortReserved || !m_pPlayerMapping->GameServer()->GetDDRaceTeam(i)) // condition to unset reserved slot
					m_aReserved[i] = false;
			}
			continue;
		}
		else if(ResortReserved)
			continue;

		int Insert = -1;
		if(m_pPlayerMapping->GameServer()->GetDDRaceTeam(i))
		{
			for(int j = 0; j < GetMapSize() - m_NumSeeOthers; j++)
			{
				int CId = m_pMap[j];
				if(CId == -1 || !m_aReserved[CId])
				{
					Insert = j;
					m_aReserved[i] = true;
					break;
				}
			}
		}
		else if(m_pReverseMap[i] != -1)
		{
			Insert = m_pReverseMap[i];
		}
		else
		{
			for(int j = 0; j < GetMapSize() - m_NumSeeOthers; j++)
				if(m_pMap[j] == -1)
				{
					Insert = j;
					break;
				}
		}

		if(Insert != -1)
		{
			Add(Insert, i);
		}
		else if(pPlayer->GetCharacter() && !pPlayer->GetCharacter()->NetworkClipped(m_ClientId))
		{
			InsertNextEmpty(i);
		}
	}

	if(m_UpdateTeamsState)
	{
		((CGameControllerDDRace *)m_pPlayerMapping->GameServer()->m_pController)->Teams().SendTeamsState(m_ClientId);
		m_UpdateTeamsState = false;
	}
}

void CPlayerMapping::CPlayerMap::InsertNextEmpty(int ClientId)
{
	if(ClientId == -1 || m_pReverseMap[ClientId] != -1)
		return;

	for(int i = 0; i < GetMapSize() - m_NumSeeOthers; i++)
	{
		int MappedId = m_pMap[i];
		if(MappedId != -1 && m_aReserved[MappedId])
			continue;

		if(MappedId == -1 || (!m_pPlayerMapping->GameServer()->GetPlayerChar(MappedId) || m_pPlayerMapping->GameServer()->GetPlayerChar(MappedId)->NetworkClipped(m_ClientId)))
		{
			Add(i, ClientId);
			break;
		}
	}
}

int CPlayerMapping::GetSeeOthersId(int ClientId) const
{
	return LEGACY_MAX_CLIENTS - 2;
}

void CPlayerMapping::DoSeeOthers(int ClientId)
{
	m_aMap[ClientId].DoSeeOthers();
}

void CPlayerMapping::ResetSeeOthers(int ClientId)
{
	m_aMap[ClientId].ResetSeeOthers();
}

int CPlayerMapping::GetTotalOverhang(int ClientId) const
{
	return m_aMap[ClientId].m_TotalOverhang;
}

void CPlayerMapping::UpdatePlayerMap(int ClientId)
{
	if(ClientId == -1)
	{
		bool Update = Server()->Tick() % Config()->m_SvMapUpdateRate == 0;
		for(auto &Map : m_aMap)
		{
			// Calculate overhang every tick, not only when the map updates
			int Overhang = maximum(0, Server()->ClientCount() - Map.GetMapSize());
			if(Overhang != Map.m_TotalOverhang)
			{
				Map.m_TotalOverhang = Overhang;
				if(Map.m_TotalOverhang <= 0 && Map.m_SeeOthersState != (int)CPlayerMap::ESeeOthers::STATE_NONE)
					Map.ResetSeeOthers();

				Map.UpdateSeeOthers();
				Map.m_UpdateTeamsState = true;
			}

			if(Update)
			{
				Map.Update();
			}
		}
	}
	else
	{
		m_aMap[ClientId].Update();
	}
}

int CPlayerMapping::GetSeeOthersInd(int ClientId, int MapId) const
{
	if(m_aMap[ClientId].m_TotalOverhang && MapId == GetSeeOthersId(ClientId))
		return SEE_OTHERS_IND_BUTTON;
	if(m_aMap[ClientId].m_NumSeeOthers && MapId >= m_aMap[ClientId].GetMapSize() - m_aMap[ClientId].m_NumSeeOthers)
		return SEE_OTHERS_IND_PLAYER;
	return -1;
}

const char *CPlayerMapping::GetSeeOthersName(int ClientId, char (&aName)[MAX_NAME_LENGTH]) const
{
	CPlayerMap::ESeeOthers State = static_cast<CPlayerMap::ESeeOthers>(m_aMap[ClientId].m_SeeOthersState);
	if(State == CPlayerMap::ESeeOthers::STATE_PAGE_FIRST)
	{
		if(m_aMap[ClientId].m_TotalOverhang > (int)CPlayerMap::ESeeOthers::MAX_NUM_SEE_OTHERS)
			str_copy(aName, "⋅ 1/2");
		else
			str_copy(aName, "⋅ Close");
	}
	else if(State == CPlayerMap::ESeeOthers::STATE_PAGE_SECOND)
	{
		str_copy(aName, "⋅ 2/2 | Close");
	}
	else
	{
		str_format(aName, sizeof(aName), "⋅ %d others", m_aMap[ClientId].m_TotalOverhang);
	}

	return aName;
}

void CPlayerMapping::CPlayerMap::CycleSeeOthers()
{
	if(m_TotalOverhang <= 0)
		return;

	for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		if(m_pMap[i] != -1)
			m_aWasSeeOthers[m_pMap[i]] = true;

	int Size = minimum(m_TotalOverhang, (int)CPlayerMap::ESeeOthers::MAX_NUM_SEE_OTHERS);
	int Added = 0;
	int MapId = GetMapSize() - 1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_aWasSeeOthers[i])
			continue;

		Add(MapId, i);
		m_aWasSeeOthers[i] = true;
		Added++;
		MapId--;

		if(Added >= Size)
			break;
	}

	m_NumSeeOthers = Added;
}

void CPlayerMapping::CPlayerMap::DoSeeOthers()
{
	if(m_TotalOverhang <= 0)
		return;

	m_SeeOthersState++;
	UpdateSeeOthers();

	CycleSeeOthers();

	// aggressively trigger reset now
	if(m_NumSeeOthers == 0)
	{
		// Reset these for the next cycle so we can get the fresh page we had before
		for(bool &WasSeeOthers : m_aWasSeeOthers)
			WasSeeOthers = false;
		CycleSeeOthers();
		ResetSeeOthers();
	}

	// instantly update so we dont have to wait for the map to be executed
	m_UpdateTeamsState = true;
	Update();
}

void CPlayerMapping::CPlayerMap::ResetSeeOthers()
{
	m_SeeOthersState = (int)ESeeOthers::STATE_NONE;
	m_NumSeeOthers = 0;
	for(bool &WasSeeOthers : m_aWasSeeOthers)
		WasSeeOthers = false;
	m_UpdateTeamsState = true;
	UpdateSeeOthers();
}

void CPlayerMapping::CPlayerMap::UpdateSeeOthers() const
{
	if(!m_pPlayerMapping->Server()->IsSixup(m_ClientId))
		return;

	int SeeOthersId = m_pPlayerMapping->GetSeeOthersId(m_ClientId);
	protocol7::CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientId = SeeOthersId;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	char aName[MAX_NAME_LENGTH];
	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientId = SeeOthersId;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = TEAM_BLUE;
	NewClientInfoMsg.m_pName = m_pPlayerMapping->GetSeeOthersName(m_ClientId, aName);
	NewClientInfoMsg.m_pClan = "";
	NewClientInfoMsg.m_Country = -1;
	NewClientInfoMsg.m_Silent = 1;
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		bool Colored = p == protocol7::SKINPART_BODY || p == protocol7::SKINPART_FEET;
		NewClientInfoMsg.m_apSkinPartNames[p] = "standard";
		NewClientInfoMsg.m_aUseCustomColors[p] = (int)Colored;
		NewClientInfoMsg.m_aSkinPartColors[p] = Colored ? 5963600 : 0;
	}

	m_pPlayerMapping->Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD | MSGFLAG_NOTRANSLATE, m_ClientId);
	m_pPlayerMapping->Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD | MSGFLAG_NOTRANSLATE, m_ClientId);
}
