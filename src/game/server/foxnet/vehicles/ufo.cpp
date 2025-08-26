// Made by qxdFox
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "game/server/entities/character.h"
#include "ufo.h"
#include <engine/shared/config.h>
#include <iterator>
#include <algorithm>

CGameContext *CVUfo::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CVUfo::Server() const { return GameServer()->Server(); }

void CVUfo::OnSpawn(CCharacter *pChr)
{
	m_pCharacter = pChr;
	m_Active = false;
}

void CVUfo::OnPlayerDeath()
{
	SetActive(false);
}

void CVUfo::SetActive(bool Active)
{;
	if(!Active)
	{
		Reset();
		return;
	}

	if(m_Active)
		return;

	for(int i = 0; i < NUM_PARTS; i++)
		m_Visual.m_aIds[i] = Server()->SnapNewId();

	std::sort(std::begin(m_Visual.m_aIds), std::end(m_Visual.m_aIds));

	m_pCharacter->SetPosition(m_pCharacter->m_Pos);

	int Zone = m_pCharacter->GetOverriddenTuneZone();
	CTuningParams FakeTuning = Zone > 0 ? GameServer()->TuningList()[Zone] : *GameServer()->Tuning();

	if(m_pCharacter->m_Snake.Active())
		m_pCharacter->m_Snake.SetActive(false);

	float Friction = (float)g_Config.m_SvUfoFriction / 100;

	FakeTuning.m_Gravity = 0.0f;
	FakeTuning.m_GroundControlSpeed = 0.f;
	FakeTuning.m_GroundJumpImpulse = 0.f;
	FakeTuning.m_GroundControlAccel = 0.f;
	FakeTuning.m_AirControlSpeed = 0.f;
	FakeTuning.m_AirJumpImpulse = 0.f;
	FakeTuning.m_AirControlAccel = 0.f;
	FakeTuning.m_GroundFriction = Friction;

	GameServer()->SendFakeTuningParams(m_pCharacter->GetPlayer()->GetCid(), FakeTuning, true);

	m_Active = true;
}

void CVUfo::Reset()
{
	if(!m_Active)
		return;

	for(int i = 0; i < NUM_PARTS; i++) 
   		Server()->SnapFreeId(m_Visual.m_aIds[i]);

	int Zone = m_pCharacter->GetOverriddenTuneZone();

	if(m_pCharacter->Core()->m_FakeTuned)
		GameServer()->ResetFakeTunes(m_pCharacter->GetPlayer()->GetCid(), Zone);

	m_Active = false;
}

void CVUfo::SetUfoVisual()
{
	// right dome
	m_Visual.m_From[0] = vec2(32.0f, -15.0f);
	m_Visual.m_From[1] = vec2(20.0f, -30.0f);
	// middle
	m_Visual.m_From[2] =  vec2(0, -37.0f);
	// left dome
	m_Visual.m_From[3] = vec2(-20.0f, -30.0f);
	m_Visual.m_From[4] = vec2(-32.0f, -15.0f);

	m_Visual.m_From[5] = vec2(-50.0f, -10.0f);

	m_Visual.m_From[6] = vec2(-60.0f, 0.0f);
	m_Visual.m_From[7] = vec2(-50.0f, 10.0f);
	m_Visual.m_From[8] = vec2(0.0f, 12.0f);

	m_Visual.m_From[9] = vec2(50.0f, 10.0f);
	m_Visual.m_From[10] = vec2(60.0f, 0.0f);

	m_Visual.m_From[11] = vec2(50.0f, -10.0f);
	m_Visual.m_From[12] = m_Visual.m_To[12] = vec2(50.0f, -10.0f);

	for(int i = 0; i < NUM_PARTS; i++)
	{
		int Offset = 1;
		if(i == 0)
			Offset = -NUM_PARTS + 1;
		if(i == NUM_PARTS - 1)
			continue;

		m_Visual.m_To[i] = m_Visual.m_From[i - Offset];
	}
}

void CVUfo::Snap(int SnappingClient)
{
	if(!m_Active)
		return;

	if(NetworkClipped(GameServer(), SnappingClient, m_pCharacter->m_Pos))
		return;

	int ClientId = m_pCharacter->GetPlayer()->GetCid();

	CCharacter *pChr = m_pCharacter;
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pChr || !pSnapPlayer)
		return;

	if(pSnapPlayer->GetCharacter() && pChr)
		if(!pChr->CanSnapCharacter(SnappingClient))
			return;

	if(pChr->GetPlayer()->m_Vanish && SnappingClient != ClientId && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	vec2 PredPos = pChr->Core()->m_Pos - m_pVel / 4.0f;

	if(SnappingClient == ClientId && !pChr->IsPaused() && !pChr->GetPlayer()->IsPaused())
	{
		int Ping = (pChr->GetPlayer()->m_Latency.m_Avg) / 2.0f;
		PredPos = pChr->Core()->m_Pos + m_pVel * Ping / g_Config.m_SvReversePrediction;
	}

	for(int i = 0; i < NUM_PARTS; i++)
	{
		CNetObj_DDNetLaser *pLaser = Server()->SnapNewItem<CNetObj_DDNetLaser>(m_Visual.m_aIds[i]);
		if(!pLaser)
			return;
		vec2 From = PredPos + m_Visual.m_From[i];
		vec2 To = PredPos + m_Visual.m_To[i];

		pLaser->m_FromX = round_to_int(From.x);
		pLaser->m_FromY = round_to_int(From.y);
		pLaser->m_ToX = round_to_int(To.x);
		pLaser->m_ToY = round_to_int(To.y);
		pLaser->m_StartTick = Server()->Tick() - 1;
		pLaser->m_Owner = ClientId;
		pLaser->m_Type = g_Config.m_SvUfoLaserType;
		pLaser->m_Flags = LASERFLAG_NO_PREDICT;
	}
}

void CVUfo::Tick()
{
	if(!m_Active)
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(m_pCharacter->GetPlayer()->GetCid());

	if(!pChr)
		return;

	m_CanMove = true;
	if(!g_Config.m_SvUfoDisableFreeze && (pChr->m_FreezeTime > 0 || pChr->Core()->m_DeepFrozen || pChr->Core()->m_LiveFrozen))
		m_CanMove = false;

	if(HandleInput())
		SetUfoVisual();
}

void CVUfo::OnInput(CNetObj_PlayerInput pInput)
{
	m_Input = pInput;
}

bool CVUfo::HandleInput()
{
	float Friction = (float)g_Config.m_SvUfoFriction / 100;
	float Accel = (float)(g_Config.m_SvUfoAccel / 10);
	float MaxSpeed = (float)g_Config.m_SvUfoMaxSpeed;

	CCharacter *pChr = m_pCharacter;
	if(!pChr)
		return false;

	const bool Up = m_Input.m_Jump;
	const bool Down = pChr->GetPlayer()->m_PlayerFlags & PLAYERFLAG_AIM;
	const bool HoldingFire = m_Input.m_Fire % 2 != 0;
	
	if(!m_Immovable)
	{
		m_pPrevPos = pChr->m_Pos;
		if(g_Config.m_SvUfoTranslateVel)
			m_pVel = pChr->Core()->m_Vel;
	}

	vec2 Dir = vec2(0, 0);

	m_Immovable = false;
	m_AllowHookColl = false;
	if(m_CanMove)
	{

		Dir = vec2(m_Input.m_Direction, 0);
		if(Up)
			Dir.y += -1;
		if(Down)
			Dir.y += 1;

		if(Up && Down)
			m_AllowHookColl = true;

		if(Up && Down && HoldingFire && g_Config.m_SvUfoBrakes)
		{
			pChr->SetPosition(m_pPrevPos);
			pChr->ResetVelocity(); 
			m_pVel = vec2(0, 0);
			m_Immovable = true;
			return true; // still update visuals
		}
	}
	else
	{
		m_AllowHookColl = true;
		m_pVel.x *= 0.99f;
		Dir = vec2(m_pVel.x > 0 ? 0.01f : -0.01f, 0.30f);
	}

	if(Dir.x != 0)
		m_pVel.x += Dir.x * Accel;
	else
		m_pVel.x *= Friction;

	if(Dir.y != 0)
		m_pVel.y += Dir.y * Accel;
	else
		m_pVel.y *= Friction;

	// Clamp velocity to max speed
	float speed = length(m_pVel);
	if(speed > MaxSpeed)
		m_pVel = normalize(m_pVel) * MaxSpeed;

	pChr->SetVelocity(m_pVel);

	return true;
}


