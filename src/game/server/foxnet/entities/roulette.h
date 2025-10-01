// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_ENTITIES_ROULETTE_H
#define GAME_SERVER_FOXNET_ENTITIES_ROULETTE_H

#include <base/vmath.h>
#include <cstdint>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

constexpr int MAX_FIELDS = 37; // 0-36
constexpr int MIN_SPIN_DURATION = 2 * SERVER_TICK_SPEED; // seconds
constexpr int MAX_SPIN_DURATION = 3 * SERVER_TICK_SPEED; // seconds

struct SClients
{
	int64_t m_BetAmount = 0;
	char m_aBetOption[24] = "";
	bool m_Active = false;
};

enum Colors
{
	COLOR_BLACK = 0,
	COLOR_RED = 1,
	COLOR_GREEN = 2
};

struct SFields
{
	int m_Number;
	int m_Color; // 0 = black, 1 = red, 2 = green
};

constexpr const char *RouletteOptions[] = {
	"Black",
	"Red",
	"Green",
	"Even",
	"Odd",
	"1-12",
	"13-24", 
	"25-36"
};

enum class RStates
{
	IDLE = 0,
	PREPARING,
	SPINNING,
	STOPPING
};

class CRoulette : public CEntity
{

	int m_SpinDuration = 0;

	float m_SlowDownFactor = 1.0f;

	float m_RotationSpeed = 0.0f;
	float m_Rotation = 0.0f;
	RStates m_State = RStates::IDLE;

	int m_StartDelay = 0;

	int m_Betters = 0;
	int m_TotalWager = 0;

	SClients m_aClients[MAX_CLIENTS];
	const SFields m_aFields[MAX_FIELDS] = {
		{0, COLOR_GREEN},
		{32, COLOR_RED}, {15, COLOR_BLACK}, {19, COLOR_RED}, {4, COLOR_BLACK},
		{21, COLOR_RED}, {2, COLOR_BLACK}, {25, COLOR_RED}, {17, COLOR_BLACK},
		{34, COLOR_RED}, {6, COLOR_BLACK}, {27, COLOR_RED}, {13, COLOR_BLACK},
		{36, COLOR_RED}, {11, COLOR_BLACK}, {30, COLOR_RED}, {8, COLOR_BLACK},
		{23, COLOR_RED}, {10, COLOR_BLACK}, {5, COLOR_RED}, {24, COLOR_BLACK},
		{16, COLOR_RED}, {33, COLOR_BLACK}, {1, COLOR_RED}, {20, COLOR_BLACK},
		{14, COLOR_RED}, {31, COLOR_BLACK}, {9, COLOR_RED}, {22, COLOR_BLACK},
		{18, COLOR_RED}, {29, COLOR_BLACK}, {7, COLOR_RED}, {28, COLOR_BLACK},
		{12, COLOR_RED}, {35, COLOR_BLACK}, {3, COLOR_RED}, {26, COLOR_BLACK}};

	void SetState(RStates State);
	void EvaluateBets();

	int GetField() const;

	void ResetClients();

	bool ClientCloseEnough(int ClientId);

	int AmountOfCloseClients();

	bool CanBet(int ClientId) const;
	void StartSpin();

public:
	CRoulette(CGameWorld *pGameWorld, vec2 Pos);

	bool AddClient(int ClientId, int BetAmount, const char *pBetOption);
	RStates State() const { return m_State; }

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_ROULETTE_H
