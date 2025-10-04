// Made by qxdFox
#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>

#include <generated/protocol.h>

#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <base/math.h>
#include <base/str.h>
#include <base/system.h>
#include <base/vmath.h>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "roulette.h"
#include <game/teamscore.h>

// Its called powerup because i want to add more functionality later to it like giving custom weapons or abilities
// For now it just acts like the 0xf one
CRoulette::CRoulette(CGameWorld *pGameWorld, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTALS, Pos, 54)
{
	m_Pos = Pos;
	m_StartDelay = -1;
	SetState(RStates::IDLE);
	GameWorld()->InsertEntity(this);

	GameServer()->m_pRoulette = this;
}

bool CRoulette::CanBet(int ClientId) const
{
	if(m_State != RStates::IDLE && m_State != RStates::PREPARING)
		return false;
	if(m_aClients[ClientId].m_Active)
		return false;

	return true;
}

void CRoulette::ResetClients()
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
		if(pPl && m_aClients[ClientId].m_Active)
			pPl->m_BetAmount = -1;
			
		m_aClients[ClientId].m_BetAmount = -1;
		m_aClients[ClientId].m_aBetOption[0] = '\0';
		m_aClients[ClientId].m_Active = false;
	}
}

int CRoulette::AmountOfCloseClients()
{
	int Count = 0;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
		if(!pPl)
			continue;
		if(pPl->IsAfk())
			continue;
		if(!pPl->Acc()->m_LoggedIn)
			continue;
		if(!pPl->Acc()->m_Money)
			continue;
		CCharacter *pCharacter = pPl->GetCharacter();
		if(!pCharacter)
			continue;
		if(distance(pCharacter->m_Pos, m_Pos) > 32.0f * 13.0f)
			continue;
		if(pCharacter->Team() != TEAM_FLOCK)
			continue;

		Count++;
	}
	return Count;
}

bool CRoulette::AddClient(int ClientId, int BetAmount, const char *pBetOption)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];

	if(pPl->GetArea() != AREA_ROULETTE)
		return false;

	CCharacter *pChr = pPl->GetCharacter();
	if(pChr->Team() != TEAM_FLOCK)
		return false;

	if(!CanBet(ClientId))
	{
		GameServer()->SendChatTarget(ClientId, "Wait until the current round is over!");
		return false;
	}

	if(!pPl->Acc()->m_LoggedIn)
	{
		GameServer()->SendChatTarget(ClientId, "╭─────────       Rᴏᴜʟᴇᴛᴛᴇ");
		GameServer()->SendChatTarget(ClientId, "│ You need to be logged in to gamble");
		GameServer()->SendChatTarget(ClientId, "│ Use: /login <name> <pw>");
		GameServer()->SendChatTarget(ClientId, "│ Or create an account using:");
		GameServer()->SendChatTarget(ClientId, "│ /register <name> <pw> <pw>");
		GameServer()->SendChatTarget(ClientId, "╰──────────────────────────");
		return false;
	}

	if(BetAmount <= 0)
	{
		GameServer()->SendChatTarget(ClientId, "You need to set a wager first, use /bet <Amount>");
		return false;
	}

	bool ValidOption = false;
	for(const char *pOptions : RouletteOptions)
	{
		if(!str_comp(pOptions, pBetOption))
		{
			ValidOption = true;
			break;
		}
	}
	if(!ValidOption)
	{
		GameServer()->SendChatTarget(ClientId, "Something went wrong, please try again!");
		return false;
	}

	SetState(RStates::PREPARING);

	pPl->m_BetAmount = -1;
	m_aClients[ClientId].m_BetAmount = BetAmount;
	str_copy(m_aClients[ClientId].m_aBetOption, pBetOption);
	m_aClients[ClientId].m_Active = true;

	m_Betters++;
	m_TotalWager += BetAmount;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "You placed a bet of %d on %s", BetAmount, pBetOption);
	GameServer()->SendChatTarget(ClientId, aBuf);
	return true;
}

void CRoulette::SetState(RStates State)
{
	if(m_State == State)
		return;

	m_State = State;
	
	if(State == RStates::PREPARING)
	{
		int Close = AmountOfCloseClients();

		if(Close > 3)
			m_StartDelay = Server()->TickSpeed() * 7.5; // 7.5 seconds
		else if(Close > 1)
			m_StartDelay = Server()->TickSpeed() * 5; // 5 seconds
		else
			m_StartDelay = Server()->TickSpeed() * 1.5; // 1.5 seconds
	}
}

int CRoulette::GetField() const
{
	constexpr float TWO_PI = 2.f * pi;
	constexpr float FIELD_ANGLE = TWO_PI / MAX_FIELDS;
	constexpr float HALF_FIELD = FIELD_ANGLE * 0.5f;
	constexpr float ANGLE_TOP = 3.f * pi / 2.f;

	constexpr float FINE_TUNE = 0.0f;
	constexpr int SECTOR_OFFSET = 0;
	constexpr bool HORIZONTAL_FLIP = true;

	float rot = fmodf(m_Rotation, TWO_PI);
	if(rot < 0.f)
		rot += TWO_PI;

	float rel = ANGLE_TOP - 1 * rot;
	rel = fmodf(rel, TWO_PI);
	if(rel < 0.f)
		rel += TWO_PI;
	rel += HALF_FIELD + FINE_TUNE;
	if(rel >= TWO_PI)
		rel -= TWO_PI;

	int index = static_cast<int>(rel / FIELD_ANGLE) % MAX_FIELDS;

	index = (index + SECTOR_OFFSET + MAX_FIELDS) % MAX_FIELDS;

	if(HORIZONTAL_FLIP)
		index = (MAX_FIELDS - index) % MAX_FIELDS;

	return index;
}

void CRoulette::EvaluateBets()
{
	const int WinningField = GetField();
	const int Color = m_aFields[WinningField].m_Color;
	const int Number = m_aFields[WinningField].m_Number;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_aClients[i].m_Active)
			continue;
		float PayoutMultiplier = 0;
		if(str_comp(m_aClients[i].m_aBetOption, "Black") == 0 && Color == COLOR_BLACK)
			PayoutMultiplier = 2;
		else if(str_comp(m_aClients[i].m_aBetOption, "Red") == 0 && Color == COLOR_RED)
			PayoutMultiplier = 2;
		else if(str_comp(m_aClients[i].m_aBetOption, "Green") == 0 && Color == COLOR_GREEN)
			PayoutMultiplier = 10;
		else if(str_comp(m_aClients[i].m_aBetOption, "Even") == 0 && Number != 0 && Number % 2 == 0)
			PayoutMultiplier = 2;
		else if(str_comp(m_aClients[i].m_aBetOption, "Odd") == 0 && Number % 2 == 1)
			PayoutMultiplier = 2;
		else if(str_comp(m_aClients[i].m_aBetOption, "1-12") == 0 && Number >= 1 && Number <= 12)
			PayoutMultiplier = 3;
		else if(str_comp(m_aClients[i].m_aBetOption, "13-24") == 0 && Number >= 13 && Number <= 24)
			PayoutMultiplier = 3;
		else if(str_comp(m_aClients[i].m_aBetOption, "25-36") == 0 && Number >= 25 && Number <= 36)
			PayoutMultiplier = 3;

		bool Win = PayoutMultiplier > 0;

		int Amount = m_aClients[i].m_BetAmount;

		CPlayer *pPl = GameServer()->m_apPlayers[i];
		if(Win)
			pPl->GiveMoney((Amount * PayoutMultiplier) - Amount, "", false);
		else
			pPl->TakeMoney(Amount);
	}
	m_Betters = 0;
	m_TotalWager = 0;
	ResetClients();
}

void CRoulette::StartSpin()
{
	if(m_State != RStates::PREPARING)
		return;

	// random number for spin duration
	static std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<> distr(MIN_SPIN_DURATION, MAX_SPIN_DURATION);
	std::uniform_real_distribution<> distrSlow(0.35f, 0.7f);

	if(m_State == RStates::PREPARING)
	{
		m_SpinDuration = distr(rng);
		m_SlowDownFactor = distrSlow(rng);
		m_StartDelay = -1;
		SetState(RStates::SPINNING);
	}
}

void CRoulette::Reset()
{
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CRoulette::Tick()
{
	if(m_State == RStates::SPINNING || m_State == RStates::STOPPING)
		m_SpinDuration--;
	if(m_StartDelay > 0)
		m_StartDelay--;
	else if(m_State == RStates::PREPARING && m_StartDelay == 0)
		StartSpin();

	if(m_State == RStates::SPINNING)
	{
		m_RotationSpeed += 0.005f;
		if(m_RotationSpeed > 0.5f)
			m_RotationSpeed = 0.5f;
	}
	else if(m_State == RStates::STOPPING)
	{
		m_RotationSpeed -= 0.005f * m_SlowDownFactor;
		if(m_RotationSpeed < 0.0f)
		{
			m_RotationSpeed = 0.0f;
			SetState(RStates::IDLE);
			EvaluateBets();
		}
	}
	m_Rotation += m_RotationSpeed;

	if(m_SpinDuration <= 0)
	{
		if(m_State == RStates::SPINNING)
			SetState(RStates::STOPPING);
	}

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		SendBroadcast(ClientId);
}

void CRoulette::SendBroadcast(int ClientId)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	if(!pPl)
		return;

	if(pPl->GetArea() != AREA_ROULETTE)
		return;

	CCharacter *pChr = pPl->GetCharacter();
	if(pChr->Team() != TEAM_FLOCK)
		return;

	float TimeLeft = (float)m_StartDelay / (float)Server()->TickSpeed();
	char aBuf[32];

	std::vector<std::string> Messages;

	if(pPl->Acc()->m_LoggedIn)
	{
		str_format(aBuf, sizeof(aBuf), "%ld %s", (long)pPl->Acc()->m_Money, g_Config.m_SvCurrencyName);
		Messages.push_back(aBuf);

		if(m_State == RStates::IDLE)
		{
			if(pPl->m_BetAmount <= 0)
				str_copy(aBuf, "Wager: Nothing");
			else
				str_format(aBuf, sizeof(aBuf), "Wager: %d", pPl->m_BetAmount);
			Messages.push_back(aBuf);
		}
	}

	if(m_State != RStates::IDLE)
	{
		if(m_aClients[ClientId].m_Active)
		{
			str_format(aBuf, sizeof(aBuf), "You bet on: %s", m_aClients[ClientId].m_aBetOption);
			Messages.push_back(aBuf);
		}

		str_format(aBuf, sizeof(aBuf), "\nPlayers: %d", m_Betters);
		Messages.push_back(aBuf);
		str_format(aBuf, sizeof(aBuf), "Total Bets: %d\n", m_TotalWager);
		Messages.push_back(aBuf);
		if(m_StartDelay > 0)
		{
			str_format(aBuf, sizeof(aBuf), "Starting in: %.1fs", TimeLeft);
			Messages.push_back(aBuf);
		}
	}

	pPl->SendBroadcastHud(Messages, 2);
}

void CRoulette::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	const int SnappingClientVersion = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);

	float RouletteLength = g_Config.m_SvRouletteLength;

	vec2 From = m_Pos + direction(m_Rotation) * RouletteLength;

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), GetId(), From, m_Pos, 0, -1, LASERTYPE_DRAGGER, LASERDRAGGERTYPE_WEAK, -1, LASERFLAG_NO_PREDICT);
}