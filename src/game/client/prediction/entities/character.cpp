/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/collision.h>
#include <game/generated/client_data.h>
#include <game/mapitems.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

// Character, "physical" player's part

void CCharacter::SetWeapon(int W)
{
	if(W == m_Core.m_ActiveWeapon)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	SetActiveWeapon(W);

	if(m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		SetActiveWeapon(0);
}

void CCharacter::SetSolo(bool Solo)
{
	m_Core.m_Solo = Solo;
	TeamsCore()->SetSolo(GetCid(), Solo);
}

void CCharacter::SetSuper(bool Super)
{
	m_Core.m_Super = Super;
	if(m_Core.m_Super)
		TeamsCore()->Team(GetCid(), TeamsCore()->m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER);
}

bool CCharacter::IsGrounded()
{
	if(Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

void CCharacter::HandleJetpack()
{
	if(m_NumInputs < 2)
		return;

	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;
	if(m_Core.m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo || m_FreezeTime)
	{
		return;
	}

	switch(m_Core.m_ActiveWeapon)
	{
	case WEAPON_GUN:
	{
		if(m_Core.m_Jetpack)
		{
			int TuneZone = GetOverriddenTuneZone();
			float Strength = TuneZone ? GetTuning(TuneZone)->m_JetpackStrength : m_LastJetpackStrength;
			TakeDamage(Direction * -1.0f * (Strength / 100.0f / 6.11f), 0, GetCid(), m_Core.m_ActiveWeapon);
		}
	}
	}
}

void CCharacter::RemoveNinja()
{
	m_Core.m_Ninja.m_CurrentMoveTime = 0;
	m_Core.m_aWeapons[WEAPON_NINJA].m_Got = false;
	m_Core.m_ActiveWeapon = m_LastWeapon;

	SetWeapon(m_Core.m_ActiveWeapon);
}

void CCharacter::HandleNinja()
{
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		return;

	if((GameWorld()->GameTick() - m_Core.m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * GameWorld()->GameTickSpeed() / 1000))
	{
		// time's up, return
		RemoveNinja();
		return;
	}

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Core.m_Ninja.m_CurrentMoveTime--;

	if(m_Core.m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Core.m_Ninja.m_ActivationDir * m_Core.m_Ninja.m_OldVelAmount;
	}

	if(m_Core.m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Core.m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), vec2(GetTuning(GetOverriddenTuneZone())->m_GroundElasticityX, GetTuning(GetOverriddenTuneZone())->m_GroundElasticityY));

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CEntity *apEnts[MAX_CLIENTS];
			float Radius = m_ProximityRadius * 2.0f;
			int Num = GameWorld()->FindEntities(OldPos, Radius, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			// check that we're not in solo part
			if(TeamsCore()->GetSolo(GetCid()))
				return;

			for(int i = 0; i < Num; ++i)
			{
				auto *pChr = static_cast<CCharacter *>(apEnts[i]);
				if(pChr == this)
					continue;

				// Don't hit players in other teams
				if(Team() != pChr->Team())
					continue;

				// Don't hit players in solo parts
				if(TeamsCore()->GetSolo(pChr->GetCid()))
					return;

				// make sure we haven't Hit this object before
				bool AlreadyHit = false;
				for(int j = 0; j < m_NumObjectsHit; j++)
				{
					if(m_aHitObjects[j] == pChr->GetCid())
						AlreadyHit = true;
				}
				if(AlreadyHit)
					continue;

				// check so we are sufficiently close
				if(distance(pChr->m_Pos, m_Pos) > (m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give them damage and stuffs...
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_aHitObjects[m_NumObjectsHit++] = pChr->GetCid();

				CCharacter *pChar = GameWorld()->GetCharacterById(pChr->GetCid());
				if(pChar)
					pChar->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, GetCid(), WEAPON_NINJA);
			}
		}

		return;
	}
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_Core.m_aWeapons[WEAPON_NINJA].m_Got || !m_Core.m_aWeapons[m_QueuedWeapon].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	if(m_NumInputs < 2)
		return;

	int WantedWeapon = m_Core.m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	for(int i = 0; i < NUM_WEAPONS - 1; ++i)
		if(m_Core.m_aWeapons[i].m_Got)
			Anything = true;
	if(!Anything)
		return;
	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_Core.m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_NumInputs < 2)
		return;

	if(!GameWorld()->m_WorldConfig.m_PredictWeapons)
		return;

	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;
	if(m_Core.m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;
	if(m_FrozenLastTick)
		FullAuto = true;

	// don't fire hammer when player is deep and sv_deepfly is disabled
	if(!g_Config.m_SvDeepfly && m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_Core.m_DeepFrozen)
		return;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo || m_FreezeTime)
	{
		return;
	}

	vec2 ProjStartPos = m_Pos + Direction * m_ProximityRadius * 0.75f;

	switch(m_Core.m_ActiveWeapon)
	{
	case WEAPON_HAMMER:
	{
		// reset objects Hit
		m_NumObjectsHit = 0;

		if(m_Core.m_HammerHitDisabled)
			break;

		CEntity *apEnts[MAX_CLIENTS];
		int Hits = 0;
		int Num = GameWorld()->FindEntities(ProjStartPos, m_ProximityRadius * 0.5f, apEnts,
			MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

		for(int i = 0; i < Num; ++i)
		{
			auto *pTarget = static_cast<CCharacter *>(apEnts[i]);

			if((pTarget == this || !CanCollide(pTarget->GetCid())))
				continue;

			// set his velocity to fast upward (for now)

			vec2 Dir;
			if(length(pTarget->m_Pos - m_Pos) > 0.0f)
				Dir = normalize(pTarget->m_Pos - m_Pos);
			else
				Dir = vec2(0.f, -1.f);

			float Strength = GetTuning(GetOverriddenTuneZone())->m_HammerStrength;

			vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
			Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
			Temp -= pTarget->m_Core.m_Vel;

			vec2 Force = vec2(0.f, -1.0f) + Temp;

			if(GameWorld()->m_WorldConfig.m_IsFNG)
			{
				if(m_GameTeam == pTarget->m_GameTeam && pTarget->m_LastSnapWeapon == WEAPON_NINJA) // melt hammer
				{
					Force.x *= 50 * 0.01f;
					Force.y *= 50 * 0.01f;
				}
				else
				{
					Force.x *= 320 * 0.01f;
					Force.y *= 120 * 0.01f;
				}
			}
			else
				Force *= Strength;

			pTarget->TakeDamage(Force, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
				GetCid(), m_Core.m_ActiveWeapon);
			pTarget->UnFreeze();

			Hits++;
		}

		// if we Hit anything, we have to wait for the reload
		if(Hits)
		{
			float FireDelay = GetTuning(GetOverriddenTuneZone())->m_HammerHitFireDelay;
			m_ReloadTimer = FireDelay * GameWorld()->GameTickSpeed() / 1000;
		}
	}
	break;

	case WEAPON_GUN:
	{
		if(!m_Core.m_Jetpack)
		{
			int Lifetime = (int)(GameWorld()->GameTickSpeed() * GetTuning(GetOverriddenTuneZone())->m_GunLifetime);

			new CProjectile(
				GameWorld(),
				WEAPON_GUN, //Type
				GetCid(), //Owner
				ProjStartPos, //Pos
				Direction, //Dir
				Lifetime, //Span
				false, //Freeze
				false, //Explosive
				0, //Force
				-1 //SoundImpact
			);
		}
	}
	break;

	case WEAPON_SHOTGUN:
	{
		if(GameWorld()->m_WorldConfig.m_IsVanilla)
		{
			int ShotSpread = 2;
			for(int i = -ShotSpread; i <= ShotSpread; ++i)
			{
				const float aSpreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
				float a = angle(Direction);
				a += aSpreading[i + 2];
				float v = 1 - (absolute(i) / (float)ShotSpread);
				float Speed = mix((float)Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
				new CProjectile(
					GameWorld(),
					WEAPON_SHOTGUN, //Type
					GetCid(), //Owner
					ProjStartPos, //Pos
					direction(a) * Speed, //Dir
					(int)(GameWorld()->GameTickSpeed() * Tuning()->m_ShotgunLifetime), //Span
					false, //Freeze
					false, //Explosive
					-1 //SoundImpact
				);
			}
		}
		else if(GameWorld()->m_WorldConfig.m_IsDDRace)
		{
			float LaserReach = GetTuning(GetOverriddenTuneZone())->m_LaserReach;

			new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCid(), WEAPON_SHOTGUN);
		}
	}
	break;

	case WEAPON_GRENADE:
	{
		int Lifetime = (int)(GameWorld()->GameTickSpeed() * GetTuning(GetOverriddenTuneZone())->m_GrenadeLifetime);

		new CProjectile(
			GameWorld(),
			WEAPON_GRENADE, //Type
			GetCid(), //Owner
			ProjStartPos, //Pos
			Direction, //Dir
			Lifetime, //Span
			false, //Freeze
			true, //Explosive
			SOUND_GRENADE_EXPLODE //SoundImpact
		); //SoundImpact
	}
	break;

	case WEAPON_LASER:
	{
		float LaserReach = GetTuning(GetOverriddenTuneZone())->m_LaserReach;

		new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCid(), WEAPON_LASER);
	}
	break;

	case WEAPON_NINJA:
	{
		// reset Hit objects
		m_NumObjectsHit = 0;

		m_Core.m_Ninja.m_ActivationDir = Direction;
		m_Core.m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * GameWorld()->GameTickSpeed() / 1000;
		m_Core.m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);
	}
	break;
	}

	m_AttackTick = GameWorld()->GameTick(); // NOLINT(clang-analyzer-unix.Malloc)

	if(!m_ReloadTimer)
	{
		float FireDelay;
		GetTuning(GetOverriddenTuneZone())->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);

		m_ReloadTimer = FireDelay * GameWorld()->GameTickSpeed() / 1000;
	}
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();
	HandleJetpack();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();
}

void CCharacter::GiveNinja()
{
	m_Core.m_Ninja.m_ActivationTick = GameWorld()->GameTick();
	m_Core.m_aWeapons[WEAPON_NINJA].m_Got = true;
	if(m_FreezeTime == 0)
		m_Core.m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_Core.m_ActiveWeapon;
	SetActiveWeapon(WEAPON_NINJA);
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// skip the input if chat is active
	if(!GameWorld()->m_WorldConfig.m_BugDDRaceInput && pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		// save the reset input
		mem_copy(&m_SavedInput, &m_Input, sizeof(m_SavedInput));
		return;
	}

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	//m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;

	mem_copy(&m_SavedInput, &m_Input, sizeof(m_SavedInput));
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	// skip the input if chat is active
	if(!GameWorld()->m_WorldConfig.m_BugDDRaceInput && pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		// reset input
		ResetInput();
		// mods that do not allow inputs to be held while chatting also do not allow to hold hook
		m_Input.m_Hook = 0;
		return;
	}

	m_NumInputs++;
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 1 && Team() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ReleaseHook()
{
	m_Core.SetHookedPlayer(-1);
	m_Core.m_HookState = HOOK_RETRACTED;
	m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
}

void CCharacter::ResetHook()
{
	ReleaseHook();
	m_Core.m_HookPos = m_Core.m_Pos;
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	// m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::PreTick()
{
	DDRaceTick();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, !m_pGameWorld->m_WorldConfig.m_NoWeakHookAndBounce);
}

void CCharacter::Tick()
{
	if(m_pGameWorld->m_WorldConfig.m_NoWeakHookAndBounce)
	{
		m_Core.TickDeferred();
	}
	else
	{
		PreTick();
	}

	// handle Weapons
	HandleWeapons();

	DDRacePostCoreTick();

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPrevPos = m_PrevPos;
	m_PrevPos = m_Core.m_Pos;
}

void CCharacter::TickDeferred()
{
	m_Core.Move();
	m_Core.Quantize();
	m_Pos = m_Core.m_Pos;
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	vec2 Temp = m_Core.m_Vel + Force;
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);
	return true;
}

// DDRace

bool CCharacter::CanCollide(int ClientId)
{
	return TeamsCore()->CanCollide(GetCid(), ClientId);
}

bool CCharacter::SameTeam(int ClientId)
{
	return TeamsCore()->SameTeam(GetCid(), ClientId);
}

int CCharacter::Team()
{
	return TeamsCore()->Team(GetCid());
}

void CCharacter::HandleSkippableTiles(int Index)
{
	if(Index < 0)
		return;

	// handle speedup tiles
	if(Collision()->IsSpeedup(Index))
	{
		vec2 Direction, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5)
				MaxSpeed = 5;
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -std::atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = std::atan(Direction.y / Direction.x) + 2.0f * std::asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = std::asin(1.0f);
				else
					SpeederAngle = std::asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * std::asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -std::atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = std::atan(TempVel.y / TempVel.x) + 2.0f * std::asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = std::asin(1.0f);
				else
					TeeAngle = std::asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * std::asin(1.0f) + TeeAngle;

				TeeSpeed = std::sqrt(std::pow(TempVel.x, 2) + std::pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - std::cos(DiffAngle) * TeeSpeed;
				if(absolute((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(absolute((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;
			m_Core.m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}
}

bool CCharacter::IsSwitchActiveCb(int Number, void *pUser)
{
	CCharacter *pThis = (CCharacter *)pUser;
	auto &aSwitchers = pThis->Switchers();
	return !aSwitchers.empty() && pThis->Team() != TEAM_SUPER && aSwitchers[Number].m_aStatus[pThis->Team()];
}

void CCharacter::HandleTiles(int Index)
{
	int MapIndex = Index;
	m_TileIndex = Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = Collision()->GetFrontTileIndex(MapIndex);
	m_MoveRestrictions = Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos);

	// stopper
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions & CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);

	if(!GameWorld()->m_WorldConfig.m_PredictTiles)
		return;

	if(Index < 0)
	{
		m_LastRefillJumps = false;
		return;
	}

	// handle switch tiles
	if(Collision()->GetSwitchType(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER && Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()] = true;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aEndTick[Team()] = 0;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aType[Team()] = TILE_SWITCHOPEN;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aLastUpdateTick[Team()] = GameWorld()->GameTick();
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER && Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()] = true;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aEndTick[Team()] = GameWorld()->GameTick() + 1 + Collision()->GetSwitchDelay(MapIndex) * GameWorld()->GameTickSpeed();
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aType[Team()] = TILE_SWITCHTIMEDOPEN;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aLastUpdateTick[Team()] = GameWorld()->GameTick();
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER && Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()] = false;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aEndTick[Team()] = GameWorld()->GameTick() + 1 + Collision()->GetSwitchDelay(MapIndex) * GameWorld()->GameTickSpeed();
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aType[Team()] = TILE_SWITCHTIMEDCLOSE;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aLastUpdateTick[Team()] = GameWorld()->GameTick();
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER && Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()] = false;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aEndTick[Team()] = 0;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aType[Team()] = TILE_SWITCHCLOSE;
		Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aLastUpdateTick[Team()] = GameWorld()->GameTick();
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER && !m_Core.m_Invincible)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()])
		{
			Freeze(Collision()->GetSwitchDelay(MapIndex));
		}
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER && !m_Core.m_Invincible)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()])
			m_Core.m_DeepFrozen = true;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER && !m_Core.m_Invincible)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()])
			m_Core.m_DeepFrozen = false;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_ENABLE && m_Core.m_HammerHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		m_Core.m_HammerHitDisabled = false;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_DISABLE && !m_Core.m_HammerHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		m_Core.m_HammerHitDisabled = true;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_ENABLE && m_Core.m_ShotgunHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		m_Core.m_ShotgunHitDisabled = false;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_DISABLE && !m_Core.m_ShotgunHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		m_Core.m_ShotgunHitDisabled = true;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_ENABLE && m_Core.m_GrenadeHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		m_Core.m_GrenadeHitDisabled = false;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_DISABLE && !m_Core.m_GrenadeHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		m_Core.m_GrenadeHitDisabled = true;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_ENABLE && m_Core.m_LaserHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		m_Core.m_LaserHitDisabled = false;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_HIT_DISABLE && !m_Core.m_LaserHitDisabled && Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		m_Core.m_LaserHitDisabled = true;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_JUMP)
	{
		int NewJumps = Collision()->GetSwitchDelay(MapIndex);
		if(NewJumps == 255)
		{
			NewJumps = -1;
		}

		if(NewJumps != m_Core.m_Jumps)
			m_Core.m_Jumps = NewJumps;
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_LFREEZE && Team() != TEAM_SUPER && !m_Core.m_Invincible)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()])
		{
			m_Core.m_LiveFrozen = true;
		}
	}
	else if(Collision()->GetSwitchType(MapIndex) == TILE_LUNFREEZE && Team() != TEAM_SUPER && !m_Core.m_Invincible)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Switchers()[Collision()->GetSwitchNumber(MapIndex)].m_aStatus[Team()])
		{
			m_Core.m_LiveFrozen = false;
		}
	}

	// freeze
	if(((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Core.m_Super && !m_Core.m_Invincible && !m_Core.m_DeepFrozen)
	{
		Freeze();
	}
	else if(((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_Core.m_DeepFrozen)
	{
		UnFreeze();
	}

	// deep freeze
	if(((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Core.m_Super && !m_Core.m_Invincible && !m_Core.m_DeepFrozen)
	{
		m_Core.m_DeepFrozen = true;
	}
	else if(((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Core.m_Super && !m_Core.m_Invincible && m_Core.m_DeepFrozen)
	{
		m_Core.m_DeepFrozen = false;
	}

	// live freeze
	if(((m_TileIndex == TILE_LFREEZE) || (m_TileFIndex == TILE_LFREEZE)) && !m_Core.m_Super && !m_Core.m_Invincible)
	{
		m_Core.m_LiveFrozen = true;
	}
	else if(((m_TileIndex == TILE_LUNFREEZE) || (m_TileFIndex == TILE_LUNFREEZE)) && !m_Core.m_Super && !m_Core.m_Invincible)
	{
		m_Core.m_LiveFrozen = false;
	}

	// endless hook
	if(((m_TileIndex == TILE_EHOOK_ENABLE) || (m_TileFIndex == TILE_EHOOK_ENABLE)) && !m_Core.m_EndlessHook)
	{
		m_Core.m_EndlessHook = true;
	}
	else if(((m_TileIndex == TILE_EHOOK_DISABLE) || (m_TileFIndex == TILE_EHOOK_DISABLE)) && m_Core.m_EndlessHook)
	{
		m_Core.m_EndlessHook = false;
	}

	// collide with others
	if(((m_TileIndex == TILE_NPC_DISABLE) || (m_TileFIndex == TILE_NPC_DISABLE)) && !m_Core.m_CollisionDisabled)
	{
		m_Core.m_CollisionDisabled = true;
	}
	else if(((m_TileIndex == TILE_NPC_ENABLE) || (m_TileFIndex == TILE_NPC_ENABLE)) && m_Core.m_CollisionDisabled)
	{
		m_Core.m_CollisionDisabled = false;
	}

	// hook others
	if(((m_TileIndex == TILE_NPH_DISABLE) || (m_TileFIndex == TILE_NPH_DISABLE)) && !m_Core.m_HookHitDisabled)
	{
		m_Core.m_HookHitDisabled = true;
	}
	else if(((m_TileIndex == TILE_NPH_ENABLE) || (m_TileFIndex == TILE_NPH_ENABLE)) && m_Core.m_HookHitDisabled)
	{
		m_Core.m_HookHitDisabled = false;
	}

	// unlimited air jumps
	if(((m_TileIndex == TILE_UNLIMITED_JUMPS_ENABLE) || (m_TileFIndex == TILE_UNLIMITED_JUMPS_ENABLE)) && !m_Core.m_EndlessJump)
	{
		m_Core.m_EndlessJump = true;
	}
	else if(((m_TileIndex == TILE_UNLIMITED_JUMPS_DISABLE) || (m_TileFIndex == TILE_UNLIMITED_JUMPS_DISABLE)) && m_Core.m_EndlessJump)
	{
		m_Core.m_EndlessJump = false;
	}

	// walljump
	if((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if(m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps >= 2 ? m_Core.m_Jumps - 2 : 0;
			m_Core.m_Jumped = 1;
		}
	}

	// jetpack gun
	if(((m_TileIndex == TILE_JETPACK_ENABLE) || (m_TileFIndex == TILE_JETPACK_ENABLE)) && !m_Core.m_Jetpack)
	{
		m_Core.m_Jetpack = true;
	}
	else if(((m_TileIndex == TILE_JETPACK_DISABLE) || (m_TileFIndex == TILE_JETPACK_DISABLE)) && m_Core.m_Jetpack)
	{
		m_Core.m_Jetpack = false;
	}

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !TeamsCore()->GetSolo(GetCid()))
	{
		SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && TeamsCore()->GetSolo(GetCid()))
	{
		SetSolo(false);
	}

	// refill jumps
	if(((m_TileIndex == TILE_REFILL_JUMPS) || (m_TileFIndex == TILE_REFILL_JUMPS)) && !m_LastRefillJumps)
	{
		m_Core.m_JumpedTotal = 0;
		m_Core.m_Jumped = 0;
		m_LastRefillJumps = true;
	}
	if((m_TileIndex != TILE_REFILL_JUMPS) && (m_TileFIndex != TILE_REFILL_JUMPS))
	{
		m_LastRefillJumps = false;
	}
}

void CCharacter::HandleTuneLayer()
{
	int CurrentIndex = Collision()->GetMapIndex(m_Pos);
	SetTuneZone(GameWorld()->m_WorldConfig.m_UseTuneZones ? Collision()->IsTune(CurrentIndex) : 0);

	if(m_IsLocal)
		GameWorld()->m_Core.m_aTuning[g_Config.m_ClDummy] = *GetTuning(GetOverriddenTuneZone()); // throw tunings (from specific zone if in a tunezone) into gamecore if the character is local
	m_Core.m_Tuning = *GetTuning(GetOverriddenTuneZone());
}

void CCharacter::DDRaceTick()
{
	mem_copy(&m_Input, &m_SavedInput, sizeof(m_Input));
	if(m_Core.m_LiveFrozen && !m_CanMoveInFreeze && !m_Core.m_Super && !m_Core.m_Invincible)
	{
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		//Hook and weapons are possible in live freeze
	}
	if(m_FreezeTime > 0)
	{
		m_FreezeTime--;
		if(!m_CanMoveInFreeze)
		{
			m_Input.m_Direction = 0;
			m_Input.m_Jump = 0;
			m_Input.m_Hook = 0;
		}
		if(m_FreezeTime == 1)
			UnFreeze();
	}

	HandleTuneLayer();

	// check if the tee is in any type of freeze
	int Index = Collision()->GetPureMapIndex(m_Pos);
	const int aTiles[] = {
		Collision()->GetTileIndex(Index),
		Collision()->GetFrontTileIndex(Index),
		Collision()->GetSwitchType(Index)};
	m_Core.m_IsInFreeze = false;
	for(const int Tile : aTiles)
	{
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE || Tile == TILE_DEATH)
		{
			m_Core.m_IsInFreeze = true;
			break;
		}
	}
	m_Core.m_IsInFreeze |= (Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetFrontCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetFrontCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetFrontCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
				Collision()->GetFrontCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH);
}

void CCharacter::DDRacePostCoreTick()
{
	if(!GameWorld()->m_WorldConfig.m_PredictDDRace)
		return;

	if(m_Core.m_EndlessHook)
		m_Core.m_HookTick = 0;

	m_FrozenLastTick = false;

	if(m_Core.m_DeepFrozen && !m_Core.m_Super && !m_Core.m_Invincible)
		Freeze();

	// following jump rules can be overridden by tiles, like Refill Jumps, Stopper and Wall Jump
	if(m_Core.m_Jumps == -1)
	{
		// The player has only one ground jump, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 0)
	{
		// The player has no jumps at all, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
	{
		// If the player has only one jump, each jump is the last one
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
	{
		// The player has not yet used up all his jumps, so his feet remain light
		m_Core.m_Jumped = 1;
	}

	if((m_Core.m_Super || m_Core.m_EndlessJump) && m_Core.m_Jumped > 1)
	{
		// Super players and players with infinite jumps always have light feet
		m_Core.m_Jumped = 1;
	}

	int CurrentIndex = Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);

	// handle Anti-Skip tiles
	std::vector<int> vIndices = Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!vIndices.empty())
		for(int Index : vIndices)
			HandleTiles(Index);
	else
	{
		HandleTiles(CurrentIndex);
	}
}

bool CCharacter::Freeze(int Seconds)
{
	if(!GameWorld()->m_WorldConfig.m_PredictFreeze)
		return false;
	if(Seconds <= 0 || m_Core.m_Super || m_Core.m_Invincible || m_FreezeTime > Seconds * GameWorld()->GameTickSpeed())
		return false;
	if(m_Core.m_FreezeStart < GameWorld()->GameTick() - GameWorld()->GameTickSpeed())
	{
		m_FreezeTime = Seconds * GameWorld()->GameTickSpeed();
		m_Core.m_FreezeStart = GameWorld()->GameTick();
		return true;
	}
	return false;
}

bool CCharacter::Freeze()
{
	return Freeze(g_Config.m_SvFreezeDelay);
}

bool CCharacter::UnFreeze()
{
	if(m_FreezeTime > 0)
	{
		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Got)
			m_Core.m_ActiveWeapon = WEAPON_GUN;
		m_FreezeTime = 0;
		m_Core.m_FreezeStart = 0;
		m_FrozenLastTick = true;
		return true;
	}
	return false;
}

void CCharacter::GiveWeapon(int Weapon, bool Remove)
{
	if(Weapon == WEAPON_NINJA)
	{
		if(Remove)
			RemoveNinja();
		else
			GiveNinja();
		return;
	}

	if(Remove)
	{
		if(GetActiveWeapon() == Weapon)
			SetActiveWeapon(WEAPON_GUN);
	}
	else
	{
		m_Core.m_aWeapons[Weapon].m_Ammo = -1;
	}

	m_Core.m_aWeapons[Weapon].m_Got = !Remove;
}

void CCharacter::GiveAllWeapons()
{
	for(int i = WEAPON_GUN; i < NUM_WEAPONS - 1; i++)
	{
		GiveWeapon(i);
	}
}

void CCharacter::ResetVelocity()
{
	m_Core.m_Vel = vec2(0, 0);
}

// The method is needed only to reproduce 'shotgun bug' ddnet#5258
// Use SetVelocity() instead.
void CCharacter::SetVelocity(const vec2 NewVelocity)
{
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, NewVelocity);
}

void CCharacter::SetRawVelocity(const vec2 NewVelocity)
{
	m_Core.m_Vel = NewVelocity;
}

void CCharacter::AddVelocity(const vec2 Addition)
{
	SetVelocity(m_Core.m_Vel + Addition);
}

void CCharacter::ApplyMoveRestrictions()
{
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);
}

CTeamsCore *CCharacter::TeamsCore()
{
	return GameWorld()->Teams();
}

CCharacter::CCharacter(CGameWorld *pGameWorld, int Id, CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), CCharacterCore::PhysicalSize())
{
	m_Id = Id;
	m_IsLocal = false;

	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_LastRefillJumps = false;
	m_PrevPrevPos = m_PrevPos = m_Pos = vec2(pChar->m_X, pChar->m_Y);
	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameWorld()->Collision(), GameWorld()->Teams());
	m_Core.m_Id = Id;
	mem_zero(&m_Core.m_Ninja, sizeof(m_Core.m_Ninja));
	m_Core.m_LeftWall = true;
	m_ReloadTimer = 0;
	m_NumObjectsHit = 0;
	m_LastRefillJumps = false;
	m_LastJetpackStrength = 400.0f;
	m_CanMoveInFreeze = false;
	m_TeleCheckpoint = 0;
	m_StrongWeakId = 0;

	mem_zero(&m_Input, sizeof(m_Input));
	// never initialize both to zero
	m_Input.m_TargetX = 0;
	m_Input.m_TargetY = -1;

	m_LatestPrevInput = m_LatestInput = m_PrevInput = m_SavedInput = m_Input;

	ResetPrediction();
	Read(pChar, pExtended, false);
}

void CCharacter::ResetPrediction()
{
	SetSolo(false);
	SetSuper(false);
	m_Core.m_EndlessHook = false;
	m_Core.m_HammerHitDisabled = false;
	m_Core.m_ShotgunHitDisabled = false;
	m_Core.m_GrenadeHitDisabled = false;
	m_Core.m_LaserHitDisabled = false;
	m_Core.m_EndlessJump = false;
	m_Core.m_Jetpack = false;
	m_NinjaJetpack = false;
	m_Core.m_Jumps = 2;
	m_Core.m_HookHitDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_NumInputs = 0;
	m_FreezeTime = 0;
	m_Core.m_FreezeStart = 0;
	m_Core.m_IsInFreeze = false;
	m_Core.m_DeepFrozen = false;
	m_Core.m_LiveFrozen = false;
	m_FrozenLastTick = false;
	for(int w = 0; w < NUM_WEAPONS; w++)
	{
		SetWeaponGot(w, false);
		SetWeaponAmmo(w, -1);
	}
	if(m_Core.HookedPlayer() >= 0)
	{
		m_Core.SetHookedPlayer(-1);
		m_Core.m_HookState = HOOK_IDLE;
	}
	m_LastWeaponSwitchTick = 0;
	m_LastTuneZoneTick = 0;
}

void CCharacter::Read(CNetObj_Character *pChar, CNetObj_DDNetCharacter *pExtended, bool IsLocal)
{
	m_Core.Read((const CNetObj_CharacterCore *)pChar);
	m_IsLocal = IsLocal;

	if(pExtended)
	{
		SetSolo(pExtended->m_Flags & CHARACTERFLAG_SOLO);
		SetSuper(pExtended->m_Flags & CHARACTERFLAG_SUPER);

		m_TeleCheckpoint = pExtended->m_TeleCheckpoint;
		m_StrongWeakId = pExtended->m_StrongWeakId;
		m_TuneZoneOverride = pExtended->m_TuneZoneOverride;

		const bool Ninja = (pExtended->m_Flags & CHARACTERFLAG_WEAPON_NINJA) != 0;
		if(Ninja && m_Core.m_ActiveWeapon != WEAPON_NINJA)
			GiveNinja();
		else if(!Ninja && m_Core.m_ActiveWeapon == WEAPON_NINJA)
			RemoveNinja();

		if(GameWorld()->m_WorldConfig.m_PredictFreeze && pExtended->m_FreezeEnd != 0)
		{
			if(pExtended->m_FreezeEnd > 0)
			{
				if(m_FreezeTime == 0)
					Freeze();
				m_FreezeTime = maximum(1, pExtended->m_FreezeEnd - GameWorld()->GameTick());
			}
			else if(pExtended->m_FreezeEnd == -1)
				m_Core.m_DeepFrozen = true;
		}
		else
			UnFreeze();

		m_Core.ReadDDNet(pExtended);

		if(!GameWorld()->m_WorldConfig.m_PredictFreeze)
		{
			UnFreeze();
		}
	}
	else
	{
		// ddnetcharacter is not available, try to get some info from the tunings and the character netobject instead.

		// remove weapons that are unavailable. if the current weapon is ninja just set ammo to zero in case the player is frozen
		if(pChar->m_Weapon != m_Core.m_ActiveWeapon)
		{
			if(pChar->m_Weapon == WEAPON_NINJA)
				m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = 0;
			else
			{
				if(m_Core.m_ActiveWeapon == WEAPON_NINJA)
				{
					SetNinjaActivationDir(vec2(0, 0));
					SetNinjaActivationTick(-500);
					SetNinjaCurrentMoveTime(0);
				}
				if(pChar->m_Weapon == m_LastSnapWeapon)
					m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Got = false;
			}
		}
		// add weapon
		if(pChar->m_Weapon != WEAPON_NINJA)
			m_Core.m_aWeapons[pChar->m_Weapon].m_Got = true;

		// jetpack
		if(GameWorld()->m_WorldConfig.m_PredictWeapons && Tuning()->m_JetpackStrength > 0)
		{
			m_LastJetpackStrength = Tuning()->m_JetpackStrength;
			m_Core.m_Jetpack = true;
			m_Core.m_aWeapons[WEAPON_GUN].m_Got = true;
			m_Core.m_aWeapons[WEAPON_GUN].m_Ammo = -1;
			m_NinjaJetpack = pChar->m_Weapon == WEAPON_NINJA;
		}
		else if(pChar->m_Weapon != WEAPON_NINJA)
		{
			m_Core.m_Jetpack = false;
		}

		// number of jumps
		if(GameWorld()->m_WorldConfig.m_PredictTiles)
		{
			if(pChar->m_Jumped & 2)
			{
				m_Core.m_EndlessJump = false;
				if(m_Core.m_Jumps > m_Core.m_JumpedTotal && m_Core.m_JumpedTotal > 0 && m_Core.m_Jumps > 2)
					m_Core.m_Jumps = m_Core.m_JumpedTotal + 1;
			}
			else if(m_Core.m_Jumps < 2)
				m_Core.m_Jumps = m_Core.m_JumpedTotal + 2;
			if(Tuning()->m_AirJumpImpulse == 0)
			{
				m_Core.m_Jumps = 0;
				m_Core.m_Jumped = 3;
			}
		}

		// set player collision
		SetSolo(!Tuning()->m_PlayerCollision && !Tuning()->m_PlayerHooking);
		m_Core.m_CollisionDisabled = !Tuning()->m_PlayerCollision;
		m_Core.m_HookHitDisabled = !Tuning()->m_PlayerHooking;

		if(m_Core.m_HookTick != 0)
			m_Core.m_EndlessHook = false;

		// detect unfreeze (in case the player was frozen in the tile prediction and not correctly unfrozen)
		if(pChar->m_Emote != EMOTE_PAIN && pChar->m_Emote != EMOTE_NORMAL)
			m_Core.m_DeepFrozen = false;
		if(pChar->m_Weapon != WEAPON_NINJA || pChar->m_AttackTick > m_Core.m_FreezeStart || absolute(pChar->m_VelX) == 256 * 10 || !GameWorld()->m_WorldConfig.m_PredictFreeze)
		{
			m_Core.m_DeepFrozen = false;
			UnFreeze();
		}

		m_TuneZoneOverride = -1;
	}

	vec2 PosBefore = m_Pos;
	m_Pos = m_Core.m_Pos;

	if(distance(PosBefore, m_Pos) > 2.f) // misprediction, don't use prevpos
		m_PrevPos = m_Pos;

	if(distance(m_PrevPos, m_Pos) > 10.f * 32.f) // reset prevpos if the distance is high
		m_PrevPos = m_Pos;

	if(pChar->m_Jumped & 2)
		m_Core.m_JumpedTotal = m_Core.m_Jumps;
	m_AttackTick = pChar->m_AttackTick;
	m_LastSnapWeapon = pChar->m_Weapon;

	SetTuneZone(GameWorld()->m_WorldConfig.m_UseTuneZones ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0);

	// set the current weapon
	if(pChar->m_Weapon != WEAPON_NINJA)
	{
		m_Core.m_aWeapons[pChar->m_Weapon].m_Ammo = (GameWorld()->m_WorldConfig.m_InfiniteAmmo || pChar->m_Weapon == WEAPON_HAMMER) ? -1 : pChar->m_AmmoCount;
		if(pChar->m_Weapon != m_Core.m_ActiveWeapon)
			SetActiveWeapon(pChar->m_Weapon);
	}

	// reset all input except direction and hook for non-local players (as in vanilla prediction)
	if(!IsLocal)
	{
		mem_zero(&m_Input, sizeof(m_Input));
		mem_zero(&m_SavedInput, sizeof(m_SavedInput));
		m_Input.m_Direction = m_SavedInput.m_Direction = m_Core.m_Direction;
		m_Input.m_Hook = m_SavedInput.m_Hook = (m_Core.m_HookState != HOOK_IDLE);

		if(pExtended && pExtended->m_TargetX != 0 && pExtended->m_TargetY != 0)
		{
			m_Input.m_TargetX = m_SavedInput.m_TargetX = pExtended->m_TargetX;
			m_Input.m_TargetY = m_SavedInput.m_TargetY = pExtended->m_TargetY;
		}
		else
		{
			m_Input.m_TargetX = m_SavedInput.m_TargetX = std::cos(pChar->m_Angle / 256.0f) * 256.0f;
			m_Input.m_TargetY = m_SavedInput.m_TargetY = std::sin(pChar->m_Angle / 256.0f) * 256.0f;
		}
	}

	// in most cases the reload timer can be determined from the last attack tick
	// (this is only needed for autofire weapons to prevent the predicted reload timer from desyncing)
	if(IsLocal && m_Core.m_ActiveWeapon != WEAPON_HAMMER && !m_Core.m_aWeapons[WEAPON_NINJA].m_Got)
	{
		if(maximum(m_LastTuneZoneTick, m_LastWeaponSwitchTick) + GameWorld()->GameTickSpeed() < GameWorld()->GameTick())
		{
			float FireDelay;
			GetTuning(GetOverriddenTuneZone())->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
			const int FireDelayTicks = FireDelay * GameWorld()->GameTickSpeed() / 1000;
			m_ReloadTimer = maximum(0, m_AttackTick + FireDelayTicks - GameWorld()->GameTick());
		}
	}
}

void CCharacter::SetCoreWorld(CGameWorld *pGameWorld)
{
	m_Core.SetCoreWorld(&pGameWorld->m_Core, pGameWorld->Collision(), pGameWorld->Teams());
}

bool CCharacter::Match(CCharacter *pChar) const
{
	return distance(pChar->m_Core.m_Pos, m_Core.m_Pos) <= 32.f;
}

void CCharacter::SetActiveWeapon(int ActiveWeap)
{
	m_Core.m_ActiveWeapon = ActiveWeap;
	m_LastWeaponSwitchTick = GameWorld()->GameTick();
}

void CCharacter::SetTuneZone(int Zone)
{
	if(Zone == m_TuneZone)
		return;
	m_TuneZone = Zone;
	m_LastTuneZoneTick = GameWorld()->GameTick();
}

int CCharacter::GetOverriddenTuneZone() const
{
	return m_TuneZoneOverride < 0 ? m_TuneZone : m_TuneZoneOverride;
}

int CCharacter::GetPureTuneZone() const
{
	return m_TuneZone;
}

CCharacter::~CCharacter()
{
	if(GameWorld())
		GameWorld()->RemoveCharacter(this);
}
