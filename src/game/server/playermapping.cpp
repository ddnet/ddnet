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
	std::fill(std::begin(m_aTeamSizes), std::end(m_aTeamSizes), 0);
	m_ReserveAnyTeamSlots = true;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aMap[i].Init(i, this);
}

void CPlayerMapping::Tick()
{
	UpdatePlayerMap(-1);

	// Translate StrongWeakId to clamp it to 64 players
	bool NeedsLegacyMapping = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && !Server()->ClientSupportsServerMaxClients(i) && GameServer()->GetClientVersion(i) >= VERSION_DDNET_OLD)
		{
			NeedsLegacyMapping = true;
			break;
		}
	}
	if(!NeedsLegacyMapping)
		return; // or continue past this block — nothing to do on modern-only servers

	// Walk the character list ONCE per tick, not once per legacy client
	int aCharacterIds[MAX_CLIENTS];
	int NumCharacters = 0;
	for(CCharacter *pChar = (CCharacter *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
	{
		aCharacterIds[NumCharacters++] = pChar->GetPlayer()->GetCid();
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || Server()->ClientSupportsServerMaxClients(i) || GameServer()->GetClientVersion(i) < VERSION_DDNET_OLD)
			continue;

		int StrongWeakId = 0;
		for(int c = 0; c < NumCharacters; c++)
		{
			int Id = aCharacterIds[c];
			if(Server()->Translate(Id, i))
				pPlayer->m_aStrongWeakId[Id] = StrongWeakId++;
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
	std::fill(std::begin(m_aReserved), std::end(m_aReserved), false);
	m_NumPages = 0;
	m_TotalOverhang = 0;
	m_NumReserved = 0;
	m_LastSeeOthersVoteTick = 0;
	m_DoSeeOthersByVote = false;
	ResetSeeOthers();
}

CPlayer *CPlayerMapping::CPlayerMap::Player() const
{
	return m_pPlayerMapping->GameServer()->m_apPlayers[m_ClientId];
}

void CPlayerMapping::CPlayerMap::InitPlayer(CSixupCfg SixupCfg)
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
				if((i != m_ClientId || SixupCfg.m_SkipTimeoutedId) && m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] == NextFreeId)
				{
					NextFreeId++;
					Finished = false;
				}
			}
		}
	}

	// make sure no outdated data is stored, so we can start and insert new values
	// after a timeout remove all players from the previous map in correct order (important for 0.7 net msgs...)
	if(SixupCfg.m_ClearSlots)
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

	m_NumReserved = 0;
	const bool PlayerMappingRequired = !m_pPlayerMapping->Server()->ClientSupportsServerMaxClients(m_ClientId);
	if(PlayerMappingRequired)
	{
		m_NumReserved = 2;
		m_pMap[m_pPlayerMapping->Server()->GetMaxClients(m_ClientId) - 1] = -1; // player with empty name to say chat msgs
		m_pMap[m_pPlayerMapping->SeeOthersId(m_ClientId)] = -1; // see others in spec menu
		m_TotalOverhang = 0;

		if(m_pPlayerMapping->Server()->IsSixup(m_ClientId))
		{
			protocol7::CNetMsg_Sv_ClientInfo FakeInfo;
			FakeInfo.m_ClientId = m_pPlayerMapping->Server()->GetMaxClients(m_ClientId) - 1;
			FakeInfo.m_Local = 0;
			FakeInfo.m_Team = TEAM_BLUE; // `TEAM_BLUE` to hide from ddrace scoreboards
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
	}

	// Breaks with more than `MapSize` tees from the same ip, but not a problem on official servers.
	// Required for other player maps, even when this specific one doesn't need playermapping and supports max_clients
	const bool NextIdValid = NextFreeId < LEGACY_MAX_CLIENTS;
	if(NextFreeId < MapSize() && NextIdValid)
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
		if(PlayerMappingRequired && m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] < MapSize())
		{
			m_aReserved[i] = true;
			Add(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if(NextIdValid && NextFreeId < m_pPlayerMapping->m_aMap[i].MapSize())
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
	if(m_pPlayerMapping->Server()->ClientSupportsServerMaxClients(m_ClientId))
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
		bool ReserveTeamSlots = m_pPlayerMapping->ReserveTeamSlots(DDTeam, m_ClientId);

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
			InsertNextEmptyOrReplace(i);
		}
	}

	if(m_UpdateTeamsState)
	{
		m_pPlayerMapping->GameServer()->m_pController->Teams().SendTeamsState(m_ClientId);
		m_UpdateTeamsState = false;
	}
}

void CPlayerMapping::CPlayerMap::InsertNextEmptyOrReplace(int ClientId)
{
	if(ClientId == -1 || m_pReverseMap[ClientId] != -1)
		return;

	// Fast path: find an empty slot or a slot occupied by a character-less player.
	for(int i = 0; i < MapSize() - m_NumSeeOthers; i++)
	{
		int MappedClientId = m_pMap[i];
		if(MappedClientId != -1 && m_aReserved[MappedClientId])
			continue;

		if(MappedClientId == -1 || (!m_pPlayerMapping->GameServer()->GetPlayerChar(MappedClientId) || m_pPlayerMapping->GameServer()->GetPlayerChar(MappedClientId)->NetworkClipped(m_ClientId)))
		{
			Add(i, ClientId);
			return;
		}
	}

	// Overflow fallback: all visible non-reserved slots are occupied.
	// Replace the farthest non-reserved player if the new player is closer.
	CCharacter *pNewChar = m_pPlayerMapping->GameServer()->GetPlayerChar(ClientId);
	if(!pNewChar || !Player())
		return;

	vec2 ViewPos = Player()->m_ViewPos;
	float NewDist = distance_squared(ViewPos, pNewChar->GetPos());

	int ReplaceIndex = -1;
	float MaxDist = NewDist;

	for(int i = 0; i < MapSize() - m_NumSeeOthers; i++)
	{
		int MappedClientId = m_pMap[i];
		if(MappedClientId == -1 || m_aReserved[MappedClientId])
			continue;

		CCharacter *pMappedChar = m_pPlayerMapping->GameServer()->GetPlayerChar(MappedClientId);
		if(!pMappedChar)
			continue;

		float Dist = distance_squared(ViewPos, pMappedChar->GetPos());
		if(Dist > MaxDist)
		{
			MaxDist = Dist;
			ReplaceIndex = i;
		}
	}

	if(ReplaceIndex != -1)
	{
		Add(ReplaceIndex, ClientId);
	}
}

int CPlayerMapping::CPlayerMap::MapSize() const
{
	return m_pPlayerMapping->Server()->GetMaxClients(m_ClientId) - m_NumReserved;
}

bool CPlayerMapping::ReserveTeamSlots(int DDTeam, int ClientId) const
{
	const bool IsDDNet = m_pGameServer->GetClientVersion(ClientId) >= VERSION_DDNET_OLD;
	return !g_Config.m_SvSoloServer && m_ReserveAnyTeamSlots && DDTeam != TEAM_FLOCK && m_aTeamSizes[DDTeam] <= ms_MaxTeamSizePlayerMap && IsDDNet;
}

int CPlayerMapping::SeeOthersId(int ClientId) const
{
	return m_pServer->GetMaxClients(ClientId) - 2;
}

bool CPlayerMapping::DoSeeOthers(int ClientId, int SelectedId, bool DoByVote)
{
	if(Server()->ClientSupportsServerMaxClients(ClientId))
		return false;
	if(SelectedId == SeeOthersId(ClientId))
	{
		if(DoByVote)
		{
			// Less conservative rate limit than normal 3 seconds for voting. Let's settle for 1 second for now. Comparison: +spectate has 250ms.
			if(m_aMap[ClientId].m_LastSeeOthersVoteTick > Server()->Tick() - Server()->TickSpeed())
				return true;
			m_aMap[ClientId].m_LastSeeOthersVoteTick = Server()->Tick();
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
			int NumReservableTeamPlayers = 0;
			for(int Team = TEAM_FLOCK + 1; Team < NUM_DDRACE_TEAMS; Team++)
				if(m_aTeamSizes[Team] <= ms_MaxTeamSizePlayerMap)
					NumReservableTeamPlayers += m_aTeamSizes[Team];
			// Starvation is only possible when not every player fits into the legacy id map
			m_ReserveAnyTeamSlots = ClientCount <= LEGACY_MAX_CLIENTS - 2 || NumReservableTeamPlayers <= ms_MaxTotalTeamSizePlayerMap;
		}

		for(auto &Map : m_aMap)
		{
			if(!Map.Player() || Server()->ClientSupportsServerMaxClients(Map.m_ClientId))
				continue;

			// Calculate overhang every tick, not only when the map updates
			int Overhang = std::max(0, ClientCount - Map.MapSize());
			if(Overhang != Map.m_TotalOverhang)
			{
				const int MaxNumSeeOthers = Map.MaxNumSeeOthers();
				Map.m_TotalOverhang = Overhang;
				Map.m_NumPages = MaxNumSeeOthers > 0 ? std::max(1, (Overhang + MaxNumSeeOthers - 1) / MaxNumSeeOthers) : 1;
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
	if(m_aMap[ClientId].m_TotalOverhang && MapId == SeeOthersId(ClientId))
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
		if(m_aMap[ClientId].m_TotalOverhang > m_aMap[ClientId].MaxNumSeeOthers())
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

	for(int i = 0; i < m_pPlayerMapping->Server()->GetMaxClients(m_ClientId); i++)
		if(m_pMap[i] != -1)
			m_aWasSeeOthers[m_pMap[i]] = true;

	int Size = std::min(m_TotalOverhang, MaxNumSeeOthers());
	int Added = 0;
	int MapId = MapSize() - 1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_aWasSeeOthers[i])
			continue;
		if(Added >= Size)
			break;

		Add(MapId, i);
		m_aWasSeeOthers[i] = true;
		Added++;
		MapId--;
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

int CPlayerMapping::CPlayerMap::MaxNumSeeOthers()
{
	const int Max = m_pPlayerMapping->Server()->GetMaxClients(m_ClientId) == VANILLA_MAX_CLIENTS ? ms_MaxNumSeeOthersVanilla : ms_MaxNumSeeOthers;

	// count non-reserved slots
	int NumSeeOthersSlots = 0;
	for(int i = MapSize() - 1; i >= 0; i--)
	{
		int MappedClientId = m_pMap[i];
		if(MappedClientId != -1 && m_aReserved[MappedClientId])
			break;
		NumSeeOthersSlots++;
	}

	return std::min({Max, MapSize(), NumSeeOthersSlots});
}

void CPlayerMapping::CPlayerMap::UpdateSeeOthers() const
{
	if(!m_pPlayerMapping->Server()->IsSixup(m_ClientId))
		return;

	int SeeOthersId = m_pPlayerMapping->SeeOthersId(m_ClientId);
	protocol7::CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientId = SeeOthersId;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientId = SeeOthersId;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = TEAM_BLUE; // `TEAM_BLUE` to hide from ddrace scoreboards
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
