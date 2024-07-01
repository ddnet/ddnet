#include "playermapping.h"

#include <base/net.h>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

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
		if(!GameServer()->m_apPlayers[i] || GameServer()->GetClientVersion(i) >= VERSION_DDNET_128_PLAYERS)
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
	m_ResortReserved = false;
	m_NumPages = 0;
	m_TotalOverhang = 0;
	m_NumReserved = 0;
	m_DoSeeOthersByVote = false;
	ResetSeeOthers();
}

CPlayer *CPlayerMapping::CPlayerMap::Player() const
{
	return m_pPlayerMapping->GameServer()->m_apPlayers[m_ClientId];
}

void CPlayerMapping::CPlayerMap::InitPlayer(bool Timeout)
{
	std::fill(std::begin(m_aReserved), std::end(m_aReserved), false);

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
				// For 0.7 timeout: Rejoin has to check ourselves because it's the id of the old connection that we want to skip
				// Do not access our own reverse map on initial initialization, as it's only initialized below
				if((i != m_ClientId || Timeout) && m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] == NextFreeId)
				{
					NextFreeId++;
					Finished = false;
				}
			}
		}
	}

	// make sure no outdated data is stored, so we can start and insert new values
	// after a timeout remove all players from the previous map in correct order (important for 0.7 net msgs...)
	if(Timeout)
	{
		m_UpdateTeamsState = true; // to get back all teams
		for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
			Remove(i);
	}

	// Clear map, for 0.7 timeouts do this after we got our id back
	for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		m_pMap[i] = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_pReverseMap[i] = -1;

	m_NumReserved = 2;
	m_pMap[LEGACY_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs
	m_pMap[m_pPlayerMapping->SeeOthersId()] = -1; // see others in spec menu
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

	// Breaks with more than 64 tees from the same ip
	if(NextFreeId < MapSize())
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
		if(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] < MapSize())
		{
			m_aReserved[i] = true;
			Add(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if(NextFreeId < m_pPlayerMapping->m_aMap[i].MapSize())
		{
			m_pPlayerMapping->m_aMap[i].m_aReserved[m_ClientId] = true;
			m_pPlayerMapping->m_aMap[i].Add(NextFreeId, m_ClientId);
		}
	}
}

void CPlayerMapping::CPlayerMap::Add(int MapId, int ClientId)
{
	dbg_assert(Player(), "invalid player map insertion: player does not exist");
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
	Player()->SendConnect(MapId, ClientId);
}

int CPlayerMapping::CPlayerMap::Remove(int MapId)
{
	dbg_assert(Player(), "invalid player map removal: player does not exist");
	if(MapId == -1)
		return -1;

	int ClientId = m_pMap[MapId];
	if(ClientId != -1)
	{
		if(m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientId) > 0)
			m_UpdateTeamsState = true;

		if(m_aReserved[ClientId])
			m_ResortReserved = true;

		Player()->SendDisconnect(MapId);
		m_pReverseMap[ClientId] = -1;
		m_pMap[MapId] = -1;
	}
	return ClientId;
}

void CPlayerMapping::CPlayerMap::Update()
{
	if(!m_pPlayerMapping->Server()->ClientIngame(m_ClientId) || !Player())
		return;
	if(m_pPlayerMapping->GameServer()->GetClientVersion(m_ClientId) >= VERSION_DDNET_128_PLAYERS)
		return;

	if(m_DoSeeOthersByVote)
	{
		CCharacter *pChr = m_pPlayerMapping->GameServer()->GetPlayerChar(m_ClientId);
		if(pChr && !pChr->IsIdle())
		{
			ResetSeeOthers();
			m_DoSeeOthersByVote = false;
		}
	}

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

		// If a team (not 0) has more than 10 players, do not reserve their slots because it can get messy quickly if a few huge teams form.
		// To keep teams state the same on main and dummy big teams do not get highlighted at all.
		int DDTeam = m_pPlayerMapping->GameServer()->GetDDRaceTeam(i);
		bool ReserveTeamSlots = m_pPlayerMapping->ReserveTeamSlots(DDTeam);

		if(m_aReserved[i])
		{
			const NETADDR *pOwnAddr = m_pPlayerMapping->Server()->ClientAddr(m_ClientId);
			const NETADDR *pAddr = m_pPlayerMapping->Server()->ClientAddr(i);
			if(net_addr_comp_noport(pOwnAddr, pAddr) != 0)
			{
				if(ResortReserved || !ReserveTeamSlots) // condition to unset reserved slot
				{
					m_aReserved[i] = false;

					// reset our team to 0 when we are in a big team for example
					if(DDTeam != TEAM_FLOCK)
						m_UpdateTeamsState = true;
				}
			}
			continue;
		}
		else if(ResortReserved)
			continue;

		int Insert = -1;
		if(DDTeam != TEAM_FLOCK && ReserveTeamSlots)
		{
			for(int j = 0; j < MapSize() - m_NumSeeOthers; j++)
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
			for(int j = 0; j < MapSize() - m_NumSeeOthers; j++)
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
		m_pPlayerMapping->GameServer()->m_pController->Teams().SendTeamsState(m_ClientId);
		m_UpdateTeamsState = false;
	}
}

void CPlayerMapping::CPlayerMap::InsertNextEmpty(int ClientId)
{
	if(ClientId == -1 || m_pReverseMap[ClientId] != -1)
		return;

	for(int i = 0; i < MapSize() - m_NumSeeOthers; i++)
	{
		int MappedClientId = m_pMap[i];
		if(MappedClientId != -1 && m_aReserved[MappedClientId])
			continue;

		if(MappedClientId == -1 || (!m_pPlayerMapping->GameServer()->GetPlayerChar(MappedClientId) || m_pPlayerMapping->GameServer()->GetPlayerChar(MappedClientId)->NetworkClipped(m_ClientId)))
		{
			Add(i, ClientId);
			break;
		}
	}
}

bool CPlayerMapping::ReserveTeamSlots(int DDTeam)
{
	return !g_Config.m_SvSoloServer && DDTeam != TEAM_FLOCK && m_aTeamSizes[DDTeam] <= ms_MaxTeamSizePlayerMap;
}

int CPlayerMapping::SeeOthersId() const
{
	return LEGACY_MAX_CLIENTS - 2;
}

bool CPlayerMapping::DoSeeOthers(int ClientId, int SelectedId, bool DoByVote)
{
	if(GameServer()->GetClientVersion(ClientId) >= VERSION_DDNET_128_PLAYERS)
		return false;
	if(SelectedId == SeeOthersId())
	{
		if(DoByVote)
		{
			m_aMap[ClientId].m_DoSeeOthersByVote = true;
		}
		m_aMap[ClientId].DoSeeOthers();
		return true;
	}
	return false;
}

void CPlayerMapping::ResetSeeOthers(int ClientId)
{
	m_aMap[ClientId].ResetSeeOthers();
}

int CPlayerMapping::TotalOverhang(int ClientId) const
{
	return m_aMap[ClientId].m_TotalOverhang;
}

void CPlayerMapping::UpdatePlayerMap(int ClientId)
{
	if(ClientId == -1)
	{
		bool Update = Server()->Tick() % Config()->m_SvMapUpdateRate == 0;
		int ClientCount = Server()->ClientCount();

		if(Update)
		{
			// Cache team sizes to avoid more loops
			std::fill(std::begin(m_aTeamSizes), std::end(m_aTeamSizes), 0);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(!pPlayer)
					continue;
				int DDTeam = GameServer()->GetDDRaceTeam(i);
				m_aTeamSizes[DDTeam]++;
			}
		}

		for(auto &Map : m_aMap)
		{
			if(!Map.Player())
				continue;

			// Calculate overhang every tick, not only when the map updates
			int Overhang = std::max(0, ClientCount - Map.MapSize());
			if(Overhang != Map.m_TotalOverhang)
			{
				Map.m_TotalOverhang = Overhang;
				Map.m_NumPages = std::max(1, (Overhang + ms_MaxNumSeeOthers - 1) / ms_MaxNumSeeOthers);
				if(Map.m_TotalOverhang <= 0 && Map.m_SeeOthersPage != -1)
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

CPlayerMapping::ESeeOthersInd CPlayerMapping::SeeOthersInd(int ClientId, int MapId) const
{
	if(m_aMap[ClientId].m_TotalOverhang && MapId == SeeOthersId())
		return ESeeOthersInd::BUTTON;
	if(m_aMap[ClientId].m_NumSeeOthers && MapId >= m_aMap[ClientId].MapSize() - m_aMap[ClientId].m_NumSeeOthers && MapId < m_aMap[ClientId].MapSize())
		return ESeeOthersInd::PLAYER;
	return ESeeOthersInd::NONE;
}

const char *CPlayerMapping::SeeOthersName(int ClientId)
{
	int Page = m_aMap[ClientId].m_SeeOthersPage + 1;
	if(m_aMap[ClientId].m_NumPages > 1 && Page == m_aMap[ClientId].m_NumPages)
	{
		str_format(m_aSeeOthersName, sizeof(m_aSeeOthersName), "⋅ %d/%d | Close", Page, Page);
	}
	else if(m_aMap[ClientId].m_SeeOthersPage != -1)
	{
		if(m_aMap[ClientId].m_TotalOverhang > ms_MaxNumSeeOthers)
			str_format(m_aSeeOthersName, sizeof(m_aSeeOthersName), "⋅ %d/%d", Page, m_aMap[ClientId].m_NumPages);
		else
			str_copy(m_aSeeOthersName, "⋅ Close");
	}
	else
	{
		str_format(m_aSeeOthersName, sizeof(m_aSeeOthersName), "⋅ %d others", m_aMap[ClientId].m_TotalOverhang);
	}
	return m_aSeeOthersName;
}

void CPlayerMapping::CPlayerMap::CycleSeeOthers()
{
	if(m_TotalOverhang <= 0)
		return;

	for(int i = 0; i < LEGACY_MAX_CLIENTS; i++)
		if(m_pMap[i] != -1)
			m_aWasSeeOthers[m_pMap[i]] = true;

	int Size = std::min(m_TotalOverhang, ms_MaxNumSeeOthers);
	int Added = 0;
	int MapId = MapSize() - 1;
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

	// -1 (none) to 1 then 2, or even more if ms_MaxNumSeeOthers is lowered (currently 34 so there are only two pages at most)
	m_SeeOthersPage++;
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
	m_SeeOthersPage = -1;
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

	int SeeOthersId = m_pPlayerMapping->SeeOthersId();
	protocol7::CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientId = SeeOthersId;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientId = SeeOthersId;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = TEAM_BLUE;
	NewClientInfoMsg.m_pName = m_pPlayerMapping->SeeOthersName(m_ClientId);
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
