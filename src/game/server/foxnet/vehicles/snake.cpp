#include "snake.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

CGameContext *CSnake::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CSnake::Server() const { return GameServer()->Server(); }

void CSnake::OnSpawn(CCharacter *pChr)
{
	m_pCharacter = pChr;
	m_Active = false;
	Reset();
}

void CSnake::Reset()
{
	m_MoveLifespan = 0;
	m_Dir = vec2(0, 0);
	m_WantedDir = vec2(0, 0);
}

bool CSnake::Active()
{
	return m_Active;
}

vec2 RoundPos(vec2 Pos)
{
	Pos = vec2((int)Pos.x, (int)Pos.y);
	Pos.x -= (int)Pos.x % 32 - 16;
	Pos.y -= (int)Pos.y % 32 - 16;
	return Pos;
}

bool CSnake::SetActive(bool Active)
{
	if(m_Active == Active || (Active && m_pCharacter->m_InSnake))
		return false;

	m_Active = Active;
	Reset();

	int Zone = m_pCharacter->GetOverriddenTuneZone();

	if(m_Active)
	{
		SSnakeData Data;
		Data.m_pChr = m_pCharacter;
		Data.m_Pos = RoundPos(m_pCharacter->Core()->m_Pos);
		m_vSnake.push_back(Data);

		//m_pCharacter->GetPlayer()->m_ShowName = false;
		m_pCharacter->m_InSnake = true;
		m_pCharacter->GetPlayer()->SetInitialAfk(false);
		m_pCharacter->GetPlayer()->SetAfk(false);

		GameServer()->UnsetTelekinesis(m_pCharacter);
		if(m_pCharacter->m_Ufo.Active())
			m_pCharacter->m_Ufo.Reset();

		CTuningParams FakeTuning = Zone > 0 ? GameServer()->TuningList()[Zone] : *GameServer()->Tuning();

		FakeTuning.m_Gravity = 0.0f;
		FakeTuning.m_HookFireSpeed = 0.0f;
		FakeTuning.m_HookDragAccel = 0.f;
		FakeTuning.m_HookDragSpeed = 0.f;
		FakeTuning.m_GroundControlSpeed = 0.f;
		FakeTuning.m_GroundJumpImpulse = 0.f;
		FakeTuning.m_GroundControlAccel = 0.f;
		FakeTuning.m_AirControlSpeed = 0.f;
		FakeTuning.m_AirJumpImpulse = 0.f;
		FakeTuning.m_AirControlAccel = 0.f;

		GameServer()->SendFakeTuningParams(m_pCharacter->GetPlayer()->GetCid(), FakeTuning);
	}
	else 
	{
		InvalidateTees();
		for(unsigned int i = 0; i < m_vSnake.size(); i++)
		{	
			Zone = m_vSnake[i].m_pChr->GetOverriddenTuneZone();
			m_vSnake[i].m_pChr->m_InSnake = false;
			GameServer()->ResetFakeTunes(i, Zone);
		}
		GameServer()->ResetFakeTunes(m_pCharacter->GetPlayer()->GetCid(), Zone);

		m_vSnake.clear();
	}
	return true;
}

void CSnake::OnInput(CNetObj_PlayerInput pNewInput)
{
	m_Input.m_Jump = pNewInput.m_Jump;
	m_Input.m_Hook = pNewInput.m_Hook;
	m_Input.m_Direction = pNewInput.m_Direction;
}

void CSnake::Tick()
{
	if (!Active())
		return;

	if (m_MoveLifespan)
		m_MoveLifespan--;
	InvalidateTees();
	if(HandleInput())
		return;

	if(g_Config.m_SvSnakeTeePickup)
		AddNewTees();

	UpdateTees();
}

void CSnake::InvalidateTees()
{
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
	{
		if (!m_vSnake[i].m_pChr || !m_vSnake[i].m_pChr->IsAlive())
		{
			m_vSnake.erase(m_vSnake.begin() + i);
			i--;
		}
	}
}

bool CSnake::HandleInput()
{
	vec2 Dir = vec2(m_Input.m_Direction, 0);

	if(m_Input.m_Jump)
		Dir.y += -1;
	if(m_Input.m_Hook || m_pCharacter->GetPlayer()->m_PlayerFlags & PLAYERFLAG_AIM)
		Dir.y += 1;

	if(Dir != vec2(0, 0) && (!(Dir.x && Dir.y) || GameServer()->Config()->m_SvSnakeDiagonal))
	{
		m_WantedDir = Dir;
	}
	else if(!GameServer()->Config()->m_SvSnakeAutoMove)
	{
		m_WantedDir = vec2(0, 0);
	}

	if(m_MoveLifespan > 0)
		return false;

	m_Dir = m_WantedDir;
	if(m_Dir == vec2(0, 0))
		return false;

	m_MoveLifespan = Server()->TickSpeed() / GameServer()->Config()->m_SvSnakeSpeed;

	m_PrevLastPos = m_vSnake[m_vSnake.size() - 1].m_Pos;
	if(m_vSnake.size() > 1)
		for(unsigned int i = m_vSnake.size() - 1; i >= 1; i--)
			m_vSnake[i].m_Pos = m_vSnake[i - 1].m_Pos;

	m_vSnake[0].m_Pos = RoundPos(m_vSnake[0].m_Pos + m_Dir * 32.f);
	if(g_Config.m_SvSnakeCollision && GameServer()->Collision()->TestBox(m_vSnake[0].m_Pos, CCharacterCore::PhysicalSizeVec2()))
	{
		GameServer()->CreateExplosion(m_vSnake[0].m_Pos, m_pCharacter->GetPlayer()->GetCid(), WEAPON_GRENADE, true, m_pCharacter->Team(), m_pCharacter->TeamMask());
		SetActive(false);
		return true;
	}
	return false;
}

void CSnake::AddNewTees()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if (!pChr || pChr->m_InSnake || pChr->Team() != m_pCharacter->Team())
			continue;
		if(pChr->m_Ufo.Active())
			continue;

		if(distance(m_vSnake[0].m_pChr->Core()->m_Pos, pChr->Core()->m_Pos) <= 40.f)
		{
			pChr->SetPosition(m_vSnake[m_vSnake.size() - 1].m_pChr->Core()->m_Pos);
			SSnakeData Data;
			Data.m_pChr = pChr;
			Data.m_Pos = pChr->Core()->m_Pos;
			m_vSnake.push_back(Data);

			pChr->m_InSnake = true;
			GameServer()->SendTuningParams(i, pChr->m_TuneZone);
			GameServer()->UnsetTelekinesis(pChr);

			int Zone = m_pCharacter->GetOverriddenTuneZone();
			CTuningParams FakeTuning = Zone > 0 ? GameServer()->TuningList()[Zone] : *GameServer()->Tuning();

			FakeTuning.m_Gravity = 0.0f;
			FakeTuning.m_HookFireSpeed = 0.0f;
			FakeTuning.m_HookDragAccel = 0.f;
			FakeTuning.m_HookDragSpeed = 0.f;
			FakeTuning.m_GroundControlSpeed = 0.f;
			FakeTuning.m_GroundJumpImpulse = 0.f;
			FakeTuning.m_GroundControlAccel = 0.f;
			FakeTuning.m_AirControlSpeed = 0.f;
			FakeTuning.m_AirJumpImpulse = 0.f;
			FakeTuning.m_AirControlAccel = 0.f;

			GameServer()->SendFakeTuningParams(i, FakeTuning);
		}
	}
}

void CSnake::UpdateTees()
{
	float Amount = (float)m_MoveLifespan / (Server()->TickSpeed() / GameServer()->Config()->m_SvSnakeSpeed);
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
	{
		m_vSnake[i].m_pChr->SetVelocity(vec2(0, 0));
		if (GameServer()->Config()->m_SvSnakeSmooth)
		{
			vec2 PrevPos = i == (m_vSnake.size() - 1) ? m_PrevLastPos : m_vSnake[i + 1].m_Pos;
			vec2 NewPos = vec2(mix(m_vSnake[i].m_Pos.x, PrevPos.x, Amount), mix(m_vSnake[i].m_Pos.y, PrevPos.y, Amount));
			m_vSnake[i].m_pChr->SetPosition(NewPos);
		}
		else
		{
			m_vSnake[i].m_pChr->SetPosition(m_vSnake[i].m_Pos);
		}
	}
}

void CSnake::OnPlayerDeath()
{
	SetActive(false);
}
