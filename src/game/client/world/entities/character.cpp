/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/mapitems.h>

#include "character.h"
#include "projectile.h"
#include "laser.h"

#include <stdio.h>
#include <string.h>

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ProximityRadius = ms_PhysSize;
	m_Health = 0;
	m_Armor = 0;
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(int ID, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_LastRefillJumps = false;
	m_LastPenalty = false;
	m_LastBonus = false;

	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameWorld()->Collision(), GameWorld()->Teams());
	m_Core.m_ActiveWeapon = WEAPON_GUN;
	m_Core.m_Pos = m_Pos;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	m_Core.m_Id = ID;
	GameWorld()->InsertEntity(this);
	m_Alive = true;

	IncreaseHealth(10);

	GiveWeapon(WEAPON_HAMMER, -1);
	GiveWeapon(WEAPON_GUN, -1);

	SetSolo(false);

	DDRaceInit();

	m_TuneZone = Collision()->IsTune(Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	m_NeededFaketuning = 0; // reset fake tunings on respawn and send the client

	return true;
}

void CCharacter::Destroy()
{
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_Core.m_ActiveWeapon)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_Core.m_ActiveWeapon = W;

	if(m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		m_Core.m_ActiveWeapon = 0;
}

void CCharacter::SetSolo(bool Solo)
{
	TeamsCore()->SetSolo(GetCID(), Solo);
	if(Solo)
		m_NeededFaketuning |= FAKETUNE_SOLO;
	else
		m_NeededFaketuning &= ~FAKETUNE_SOLO;
}

bool CCharacter::IsGrounded()
{
	if(Collision()->CheckPoint(m_Pos.x+m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	if(Collision()->CheckPoint(m_Pos.x-m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;

	int index = Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y+m_ProximityRadius/2+4));
	int tile = Collision()->GetTileIndex(index);
	int flags = Collision()->GetTileFlags(index);
	if(tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile ==TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;
	tile = Collision()->GetFTileIndex(index);
	flags = Collision()->GetFTileFlags(index);
	if(tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile ==TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;

	return false;
}

void CCharacter::HandleJetpack()
{
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;
	if (m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		return;
	}

	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_GUN:
		{
			if (m_Jetpack)
			{
				float Strength = m_JetpackStrength;
				TakeDamage(Direction * -1.0f * (Strength / 100.0f / 6.11f), g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, GetCID(), m_Core.m_ActiveWeapon);
			}
		}
	}
}

void CCharacter::HandleNinja()
{
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		return;

	if ((GameWorld()->GameTick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * GameWorld()->GameTickSpeed() / 1000))
	{
		// time's up, return
		m_Ninja.m_CurrentMoveTime = 0;
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_Core.m_ActiveWeapon = m_LastWeapon;

		SetWeapon(m_Core.m_ActiveWeapon);
		return;
	}

	int NinjaTime = m_Ninja.m_ActivationTick + (g_pData->m_Weapons.m_Ninja.m_Duration * GameWorld()->GameTickSpeed() / 1000) - GameWorld()->GameTick();

	m_Armor = 10 - (NinjaTime / 15);

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = m_ProximityRadius * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameWorld()->FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			// check that we're not in solo part
			if (TeamsCore()->GetSolo(GetCID()))
				return;

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// Don't hit players in other teams
				if (Team() != aEnts[i]->Team())
					continue;

				// Don't hit players in solo parts
				if (TeamsCore()->GetSolo(aEnts[i]->GetCID()))
					return;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_aHitObjects[j] == aEnts[i]->GetCID())
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_aHitObjects[m_NumObjectsHit++] = aEnts[i]->GetCID();

				CCharacter *pChar = GameWorld()->GetCharacterByID(aEnts[i]->GetCID());
				if(pChar)
					pChar->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_Core.m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	 for(int i = 0; i < NUM_WEAPONS - 1; ++i)
		 if(m_aWeapons[i].m_Got)
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
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(!GameWorld()->m_PredictWeapons)
		return;

	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;
	if (m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		/*// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);*/
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;

	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			// reset objects Hit
			m_NumObjectsHit = 0;

			if (m_Hit&DISABLE_HIT_HAMMER) break;

			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			int Num = GameWorld()->FindEntities(ProjStartPos, m_ProximityRadius*0.5f, (CEntity**)apEnts,
														MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				CCharacter *pTarget = apEnts[i];

				if((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetCID()))))
					continue;

				// set his velocity to fast upward (for now)

				vec2 Dir;
				if (length(pTarget->m_Pos - m_Pos) > 0.0f)
					Dir = normalize(pTarget->m_Pos - m_Pos);
				else
					Dir = vec2(0.f, -1.f);

				float Strength = Tuning()->m_HammerStrength;

				vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
				pTarget->Core()->LimitForce(&Temp);
				Temp -= pTarget->m_Core.m_Vel;
				pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
					GetCID(), m_Core.m_ActiveWeapon);
				pTarget->UnFreeze();

				if(m_FreezeHammer)
					pTarget->Freeze();

				Hits++;
			}

			// if we Hit anything, we have to wait for the reload
			if(Hits)
				m_ReloadTimer = GameWorld()->GameTickSpeed()/3;

		} break;

		case WEAPON_GUN:
		{
			if (!m_Jetpack)
			{
				int Lifetime;
				Lifetime = (int)(GameWorld()->GameTickSpeed()*Tuning()->m_GunLifetime);
				new CProjectile
						(
						GameWorld(),
						WEAPON_GUN,//Type
						GetCID(),//Owner
						ProjStartPos,//Pos
						Direction,//Dir
						Lifetime,//Span
						0,//Freeze
						0,//Explosive
						0,//Force
						-1,//SoundImpact
						WEAPON_GUN//Weapon
						);
			}
		} break;

		case WEAPON_SHOTGUN:
		{
			if(GameWorld()->m_IsDDRace)
			{
				float LaserReach = Tuning()->m_LaserReach;
				new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCID(), WEAPON_SHOTGUN);
			}
		} break;

		case WEAPON_GRENADE:
		{
			int Lifetime = (int)(GameWorld()->GameTickSpeed()*Tuning()->m_GrenadeLifetime);
			new CProjectile
					(
					GameWorld(),
					WEAPON_GRENADE,//Type
					GetCID(),//Owner
					ProjStartPos,//Pos
					Direction,//Dir
					Lifetime,//Span
					0,//Freeze
					true,//Explosive
					0,//Force
					SOUND_GRENADE_EXPLODE,//SoundImpact
					WEAPON_GRENADE//Weapon
					);//SoundImpact
		} break;

		case WEAPON_RIFLE:
		{
			if(GameWorld()->m_IsDDRace)
			{
				float LaserReach = Tuning()->m_LaserReach;
				new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCID(), WEAPON_RIFLE);
			}
		} break;

		case WEAPON_NINJA:
		{
			// reset Hit objects
			m_NumObjectsHit = 0;

			m_Ninja.m_ActivationDir = Direction;
			m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * GameWorld()->GameTickSpeed() / 1000;
			m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);
		} break;

	}

	m_AttackTick = GameWorld()->GameTick();

	if(!m_ReloadTimer)
	{
		float FireDelay;
		if (!m_TuneZone)
		Tuning()->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		m_ReloadTimer = FireDelay * GameWorld()->GameTickSpeed() / 1000;
	}
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();
	HandleJetpack();

	if(m_PainSoundTimer > 0)
		m_PainSoundTimer--;

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();
/*
	// ammo regen
	int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Ammoregentime;
	if(AmmoRegenTime)
	{
		// If equipped and not active, regen ammo?
		if (m_ReloadTimer <= 0)
		{
			if (m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

			if ((Server()->Tick() - m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo + 1, 10);
				m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}*/

	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if(m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		if(!m_FreezeTime)
			m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = GameWorld()->GameTick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	if (!m_FreezeTime)
		m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_Core.m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_Core.m_ActiveWeapon;
	m_Core.m_ActiveWeapon = WEAPON_NINJA;
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = GameWorld()->GameTick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && Team() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	//m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	if (m_Paused)
		return;

	DDRaceTick();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// handle Weapons
	HandleWeapons();

	DDRacePostCoreTick();

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;
	return;
}

void CCharacter::TickDefered()
{
	m_Core.Move();
	m_Core.Quantize();
	m_Pos = m_Core.m_Pos;
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	m_Core.ApplyForce(Force);
	return true;
}

// DDRace

bool CCharacter::CanCollide(int ClientID)
{
	return TeamsCore()->CanCollide(GetCID(), ClientID);
}

bool CCharacter::SameTeam(int ClientID)
{
	return TeamsCore()->SameTeam(GetCID(), ClientID);
}

int CCharacter::Team()
{
	return TeamsCore()->Team(GetCID());
}

void CCharacter::HandleSkippableTiles(int Index)
{
	if(Index < 0)
		return;

	// handle speedup tiles
	if(Collision()->IsSpeedup(Index))
	{
		vec2 Direction, MaxVel, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed/5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5) MaxSpeed = 5;
			//dbg_msg("speedup tile start","Direction %f %f, Force %d, Max Speed %d", (Direction).x,(Direction).y, Force, MaxSpeed);
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				//dbg_msg("speedup tile debug","MaxSpeed %i, TeeSpeed %f, SpeedLeft %f, SpeederAngle %f, TeeAngle %f", MaxSpeed, TeeSpeed, SpeedLeft, SpeederAngle, TeeAngle);
				if(abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;
			m_Core.LimitForce(&TempVel);
			m_Core.m_Vel = TempVel;
		}
	}
}

void CCharacter::HandleTiles(int Index)
{
	//CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	float Offset = 4.0f;
	int MapIndexL = Collision()->GetPureMapIndex(vec2(m_Pos.x + (m_ProximityRadius / 2) + Offset, m_Pos.y));
	int MapIndexR = Collision()->GetPureMapIndex(vec2(m_Pos.x - (m_ProximityRadius / 2) - Offset, m_Pos.y));
	int MapIndexT = Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + (m_ProximityRadius / 2) + Offset));
	int MapIndexB = Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y - (m_ProximityRadius / 2) - Offset));
	//dbg_msg("","N%d L%d R%d B%d T%d",MapIndex,MapIndexL,MapIndexR,MapIndexB,MapIndexT);
	m_TileIndex = Collision()->GetTileIndex(MapIndex);
	m_TileFlags = Collision()->GetTileFlags(MapIndex);
	m_TileIndexL = Collision()->GetTileIndex(MapIndexL);
	m_TileFlagsL = Collision()->GetTileFlags(MapIndexL);
	m_TileIndexR = Collision()->GetTileIndex(MapIndexR);
	m_TileFlagsR = Collision()->GetTileFlags(MapIndexR);
	m_TileIndexB = Collision()->GetTileIndex(MapIndexB);
	m_TileFlagsB = Collision()->GetTileFlags(MapIndexB);
	m_TileIndexT = Collision()->GetTileIndex(MapIndexT);
	m_TileFlagsT = Collision()->GetTileFlags(MapIndexT);
	m_TileFIndex = Collision()->GetFTileIndex(MapIndex);
	m_TileFFlags = Collision()->GetFTileFlags(MapIndex);
	m_TileFIndexL = Collision()->GetFTileIndex(MapIndexL);
	m_TileFFlagsL = Collision()->GetFTileFlags(MapIndexL);
	m_TileFIndexR = Collision()->GetFTileIndex(MapIndexR);
	m_TileFFlagsR = Collision()->GetFTileFlags(MapIndexR);
	m_TileFIndexB = Collision()->GetFTileIndex(MapIndexB);
	m_TileFFlagsB = Collision()->GetFTileFlags(MapIndexB);
	m_TileFIndexT = Collision()->GetFTileIndex(MapIndexT);
	m_TileFFlagsT = Collision()->GetFTileFlags(MapIndexT);//
	m_TileSIndex = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndex)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileIndex(MapIndex) : 0 : 0;
	m_TileSFlags = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndex)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileFlags(MapIndex) : 0 : 0;
	m_TileSIndexL = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexL)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileIndex(MapIndexL) : 0 : 0;
	m_TileSFlagsL = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexL)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileFlags(MapIndexL) : 0 : 0;
	m_TileSIndexR = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexR)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileIndex(MapIndexR) : 0 : 0;
	m_TileSFlagsR = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexR)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileFlags(MapIndexR) : 0 : 0;
	m_TileSIndexB = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexB)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileIndex(MapIndexB) : 0 : 0;
	m_TileSFlagsB = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexB)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileFlags(MapIndexB) : 0 : 0;
	m_TileSIndexT = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexT)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileIndex(MapIndexT) : 0 : 0;
	m_TileSFlagsT = (Collision()->m_pSwitchers && Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndexT)].m_Status[Team()])?(Team() != TEAM_SUPER)? Collision()->GetDTileFlags(MapIndexT) : 0 : 0;
	//dbg_msg("Tiles","%d, %d, %d, %d, %d", m_TileSIndex, m_TileSIndexL, m_TileSIndexR, m_TileSIndexB, m_TileSIndexT);
	//dbg_msg("","N%d L%d R%d B%d T%d",m_TileIndex,m_TileIndexL,m_TileIndexR,m_TileIndexB,m_TileIndexT);
	//dbg_msg("","N%d L%d R%d B%d T%d",m_TileFIndex,m_TileFIndexL,m_TileFIndexR,m_TileFIndexB,m_TileFIndexT);
	if(Index < 0)
	{
		m_LastRefillJumps = false;
		m_LastPenalty = false;
		m_LastBonus = false;
		return;
	}

	// freeze
	if(((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Super && !m_DeepFreeze)
		Freeze();
	else if(((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_DeepFreeze)
		UnFreeze();

	// deep freeze
	if(((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Super && !m_DeepFreeze)
		m_DeepFreeze = true;
	else if(((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Super && m_DeepFreeze)
		m_DeepFreeze = false;

	// endless hook
	if(((m_TileIndex == TILE_EHOOK_START) || (m_TileFIndex == TILE_EHOOK_START)) && !m_EndlessHook)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Endless hook has been activated");
		m_EndlessHook = true;
	}
	else if(((m_TileIndex == TILE_EHOOK_END) || (m_TileFIndex == TILE_EHOOK_END)) && m_EndlessHook)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Endless hook has been deactivated");
		m_EndlessHook = false;
	}

	// hit others
	if(((m_TileIndex == TILE_HIT_END) || (m_TileFIndex == TILE_HIT_END)) && m_Hit != (DISABLE_HIT_GRENADE|DISABLE_HIT_HAMMER|DISABLE_HIT_RIFLE|DISABLE_HIT_SHOTGUN))
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hit others");
		m_Hit = DISABLE_HIT_GRENADE|DISABLE_HIT_HAMMER|DISABLE_HIT_RIFLE|DISABLE_HIT_SHOTGUN;
	}
	else if(((m_TileIndex == TILE_HIT_START) || (m_TileFIndex == TILE_HIT_START)) && m_Hit != HIT_ALL)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hit others");
		m_Hit = HIT_ALL;
	}

	// collide with others
	if(((m_TileIndex == TILE_NPC_END) || (m_TileFIndex == TILE_NPC_END)) && m_Core.m_Collision)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't collide with others");
		m_Core.m_Collision = false;
		//m_NeededFaketuning |= FAKETUNE_NOCOLL;
		//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(((m_TileIndex == TILE_NPC_START) || (m_TileFIndex == TILE_NPC_START)) && !m_Core.m_Collision)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can collide with others");
		m_Core.m_Collision = true;
		//m_NeededFaketuning &= ~FAKETUNE_NOCOLL;
		//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}

	// hook others
	if(((m_TileIndex == TILE_NPH_END) || (m_TileFIndex == TILE_NPH_END)) && m_Core.m_Hook)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hook others");
		m_Core.m_Hook = false;
		//m_NeededFaketuning |= FAKETUNE_NOHOOK;
		//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(((m_TileIndex == TILE_NPH_START) || (m_TileFIndex == TILE_NPH_START)) && !m_Core.m_Hook)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can hook others");
		m_Core.m_Hook = true;
		//m_NeededFaketuning &= ~FAKETUNE_NOHOOK;
		//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}

	// unlimited air jumps
	if(((m_TileIndex == TILE_SUPER_START) || (m_TileFIndex == TILE_SUPER_START)) && !m_SuperJump)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You have unlimited air jumps");
		m_SuperJump = true;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
			//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}
	else if(((m_TileIndex == TILE_SUPER_END) || (m_TileFIndex == TILE_SUPER_END)) && m_SuperJump)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You don't have unlimited air jumps");
		m_SuperJump = false;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning |= FAKETUNE_NOJUMP;
			//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}

	// walljump
	if((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if(m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps - 1;
			m_Core.m_Jumped = 1;
		}
	}

	// jetpack gun
	if(((m_TileIndex == TILE_JETPACK_START) || (m_TileFIndex == TILE_JETPACK_START)) && !m_Jetpack)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You have a jetpack gun");
		m_Jetpack = true;
	}
	else if(((m_TileIndex == TILE_JETPACK_END) || (m_TileFIndex == TILE_JETPACK_END)) && m_Jetpack)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You lost your jetpack gun");
		m_Jetpack = false;
	}

	// solo part
	if(((m_TileIndex == TILE_SOLO_START) || (m_TileFIndex == TILE_SOLO_START)) && !TeamsCore()->GetSolo(GetCID()))
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now in a solo part.");
		SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_END) || (m_TileFIndex == TILE_SOLO_END)) && TeamsCore()->GetSolo(GetCID()))
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now out of the solo part.");
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

	// stopper
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL ==ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)) && m_Core.m_Vel.x > 0)
	{
		if((int)Collision()->GetPos(MapIndexL).x)
			if((int)Collision()->GetPos(MapIndexL).x < (int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)) && m_Core.m_Vel.x < 0)
	{
		if((int)Collision()->GetPos(MapIndexR).x)
			if((int)Collision()->GetPos(MapIndexR).x > (int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)) && m_Core.m_Vel.y < 0)
	{
		if((int)Collision()->GetPos(MapIndexB).y)
			if((int)Collision()->GetPos(MapIndexB).y > (int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
	}
	if(((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)) && m_Core.m_Vel.y > 0)
	{
		//dbg_msg("","%f %f",Collision()->GetPos(MapIndex).y,m_Core.m_Pos.y);
		if((int)Collision()->GetPos(MapIndexT).y)
			if((int)Collision()->GetPos(MapIndexT).y < (int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}

	// handle switch tiles
	if(Collision()->IsSwitch(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER)
	{
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHOPEN;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER)
	{
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = GameWorld()->GameTick() + 1 + Collision()->GetSwitchDelay(MapIndex)*GameWorld()->GameTickSpeed() ;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDOPEN;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER)
	{
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = GameWorld()->GameTick() + 1 + Collision()->GetSwitchDelay(MapIndex)*GameWorld()->GameTickSpeed();
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDCLOSE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER)
	{
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHCLOSE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER)
	{
		if(Collision()->GetSwitchNumber(MapIndex) == 0 || Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			Freeze(Collision()->GetSwitchDelay(MapIndex));
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER && Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
	{
		m_DeepFreeze = true;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER && Collision()->m_pSwitchers[Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
	{
		m_DeepFreeze = false;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_HAMMER && Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can hammer hit others");
		m_Hit &= ~DISABLE_HIT_HAMMER;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_HAMMER) && Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can't hammer hit others");
		m_Hit |= DISABLE_HIT_HAMMER;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_SHOTGUN && Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can shoot others with shotgun");
		m_Hit &= ~DISABLE_HIT_SHOTGUN;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_SHOTGUN) && Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can't shoot others with shotgun");
		m_Hit |= DISABLE_HIT_SHOTGUN;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_GRENADE && Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can shoot others with grenade");
		m_Hit &= ~DISABLE_HIT_GRENADE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_GRENADE) && Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can't shoot others with grenade");
		m_Hit |= DISABLE_HIT_GRENADE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_RIFLE && Collision()->GetSwitchDelay(MapIndex) == WEAPON_RIFLE)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can shoot others with rifle");
		m_Hit &= ~DISABLE_HIT_RIFLE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_RIFLE) && Collision()->GetSwitchDelay(MapIndex) == WEAPON_RIFLE)
	{
		//GameServer()->SendChatTarget(GetPlayer()->GetCID(),"You can't shoot others with rifle");
		m_Hit |= DISABLE_HIT_RIFLE;
	}
	else if(Collision()->IsSwitch(MapIndex) == TILE_JUMP)
	{
		int newJumps = Collision()->GetSwitchDelay(MapIndex);

		if (newJumps != m_Core.m_Jumps)
		{
			char aBuf[256];
			if(newJumps == 1)
				str_format(aBuf, sizeof(aBuf), "You can jump %d time", newJumps);
			else
				str_format(aBuf, sizeof(aBuf), "You can jump %d times", newJumps);
			//GameServer()->SendChatTarget(GetPlayer()->GetCID(),aBuf);

			if (newJumps == 0 && !m_SuperJump)
			{
				m_NeededFaketuning |= FAKETUNE_NOJUMP;
				//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}
			else if (m_Core.m_Jumps == 0)
			{
				m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
				//GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}

			m_Core.m_Jumps = newJumps;
		}
	}

	if(Collision()->IsSwitch(MapIndex) != TILE_PENALTY)
	{
		m_LastPenalty = false;
	}

	if(Collision()->IsSwitch(MapIndex) != TILE_BONUS)
	{
		m_LastBonus = false;
	}
}

void CCharacter::DDRaceTick()
{
	if(!GameWorld()->m_PredictDDRace)
		return;

	m_Armor=(m_FreezeTime >= 0)?10-(m_FreezeTime/15):0;
	if(m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = GameWorld()->GameTick();

	if(m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if(m_FreezeTime > 0)
			m_FreezeTime--;
		else
			m_Ninja.m_ActivationTick = GameWorld()->GameTick();
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		m_Input.m_Hook = 0;
		if (m_FreezeTime == 1)
			UnFreeze();
	}

	//HandleTuneLayer(); // need this before coretick
}

void CCharacter::DDRacePostCoreTick()
{
	if(!GameWorld()->m_PredictDDRace)
		return;

	m_Time = (float)(GameWorld()->GameTick() - m_StartTime) / ((float)GameWorld()->GameTickSpeed());

	if (m_EndlessHook || (m_Super && g_Config.m_SvEndlessSuperHook))
		m_Core.m_HookTick = 0;

	if (m_DeepFreeze && !m_Super)
		Freeze();

	if (m_Core.m_Jumps == 0 && !m_Super)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	if ((m_Super || m_SuperJump) && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	int CurrentIndex = Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);

	// handle Anti-Skip tiles
	std::list < int > Indices = Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!Indices.empty())
		for(std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
		{
			HandleTiles(*i);
			//dbg_msg("Running","%d", *i);
		}
	else
	{
		HandleTiles(CurrentIndex);
		//dbg_msg("Running","%d", CurrentIndex);
	}
}

bool CCharacter::Freeze(int Seconds)
{
	if ((Seconds <= 0 || m_Super || m_FreezeTime == -1 || m_FreezeTime > Seconds * GameWorld()->GameTickSpeed()) && Seconds != -1)
		 return false;
	if (m_FreezeTick < GameWorld()->GameTick() - GameWorld()->GameTickSpeed() || Seconds == -1)
	{
		for(int i = 0; i < NUM_WEAPONS; i++)
			if(m_aWeapons[i].m_Got)
			 {
				 m_aWeapons[i].m_Ammo = 0;
			 }
		m_Armor = 0;
		m_FreezeTime = Seconds == -1 ? Seconds : Seconds * GameWorld()->GameTickSpeed();
		m_FreezeTick = GameWorld()->GameTick();
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
	if (m_FreezeTime > 0)
	{
		m_Armor=10;
		for(int i=0;i<NUM_WEAPONS;i++)
			if(m_aWeapons[i].m_Got)
			 {
				 m_aWeapons[i].m_Ammo = -1;
			 }
		if(!m_aWeapons[m_Core.m_ActiveWeapon].m_Got)
			m_Core.m_ActiveWeapon = WEAPON_GUN;
		m_FreezeTime = 0;
		m_FreezeTick = 0;
		if (m_Core.m_ActiveWeapon==WEAPON_HAMMER) m_ReloadTimer = 0;
		return true;
	}
	return false;
}

void CCharacter::GiveAllWeapons()
{
	for(int i=WEAPON_GUN;i<NUM_WEAPONS-1;i++)
	{
		m_aWeapons[i].m_Got = true;
		if(!m_FreezeTime) m_aWeapons[i].m_Ammo = -1;
	}
	return;
}

void CCharacter::DDRaceInit()
{
	m_Paused = false;
	m_DDRaceState = DDRACE_NONE;
	m_PrevPos = m_Pos;
	m_SetSavePos = false;
	m_LastBroadcast = 0;
	m_TeamBeforeSuper = 0;

	m_TeleCheckpoint = 0;
	m_EndlessHook = g_Config.m_SvEndlessDrag;
	m_Hit = g_Config.m_SvHit ? HIT_ALL : DISABLE_HIT_GRENADE|DISABLE_HIT_HAMMER|DISABLE_HIT_RIFLE|DISABLE_HIT_SHOTGUN;
	m_SuperJump = false;
	m_Jetpack = false;
	m_Core.m_Jumps = 2;
	m_FreezeHammer = false;
}

CTeamsCore* CCharacter::TeamsCore()
{
	return m_Core.m_pTeams;
}

CCharacter::CCharacter(CGameWorld *pGameWorld, int ID, CNetObj_Character *pChar)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ID = ID;
	Spawn(ID, vec2(pChar->m_X, pChar->m_Y));
	mem_zero(&m_Ninja, sizeof(m_Ninja));
	mem_zero(&m_Input, sizeof(m_Input));
	m_LatestInput = m_LatestPrevInput = m_PrevInput = m_Input;
	m_ProximityRadius = ms_PhysSize;
	m_Core.m_LeftWall = 1;
	m_TeamMask = -1LL;
	m_NumInputs = 3;
	m_Super = 0;
	m_FreezeTime = 0;
	m_FreezeTick = 0;
	m_DeepFreeze = 0;
	m_PainSoundTimer = 0;
	m_LastMove = 0;
	m_StartTime = 0;
	m_StrongWeakID = 0;
	m_PrevPos = m_Pos;
	m_Jetpack = 0;
	GiveAllWeapons();
	Read(pChar, false);
}

void CCharacter::Read(CNetObj_Character *pChar, bool IsLocal)
{
	vec2 PosBefore = m_Pos;

	m_Core.Read((CNetObj_CharacterCore*) pChar);
	m_EmoteType = pChar->m_Emote;
	m_Health = pChar->m_Health;
	m_Armor = pChar->m_Armor;
	m_AttackTick = pChar->m_AttackTick;
	m_Pos = m_Core.m_Pos;

	if(distance(PosBefore, m_Pos) > 2.f)
		m_PrevPos = m_Pos;

	if(pChar->m_Weapon != WEAPON_NINJA)
	{
		if(m_Core.m_ActiveWeapon != pChar->m_Weapon)
		{
			SetNinjaActivationDir(vec2(0,0));
			SetNinjaActivationTick(-500);
			SetNinjaCurrentMoveTime(0);
			m_aWeapons[m_Core.m_ActiveWeapon].m_Got = false;
			SetActiveWeapon(pChar->m_Weapon);
		}
		m_aWeapons[pChar->m_Weapon].m_Got = true;
		m_aWeapons[pChar->m_Weapon].m_Ammo = (GameWorld()->m_InfiniteAmmo || pChar->m_Weapon == WEAPON_HAMMER) ? -1 : pChar->m_AmmoCount;
	}

	if(m_FreezeTime && (pChar->m_Weapon != WEAPON_NINJA || pChar->m_AttackTick > m_FreezeTick || absolute(pChar->m_VelX) == 256*10))
	{
		m_DeepFreeze = false;
		UnFreeze();
	}

	if(!m_FreezeTime)
	{
		m_Jetpack = Tuning()->m_JetpackStrength > 0;
		m_JetpackStrength = Tuning()->m_JetpackStrength;
	}

	if(pChar->m_Jumped&2)
	{
		m_SuperJump = false;
		if(m_Core.m_Jumps > m_Core.m_JumpedTotal && m_Core.m_JumpedTotal > 0)
			m_Core.m_Jumps = m_Core.m_JumpedTotal + 1;
	}

	if(m_Core.m_HookTick != 0)
		m_EndlessHook = false;

	SetSolo(!Tuning()->m_PlayerCollision && !Tuning()->m_PlayerHooking);

	if(!IsLocal)
	{
		mem_zero(&m_Input, sizeof(m_Input));
		m_Input.m_Direction = m_Core.m_Direction;
		m_Input.m_Hook = (m_Core.m_HookState == HOOK_GRABBED);
	}
}

void CCharacter::SetGameWorld(CGameWorld *pGameWorld)
{
	m_Core.m_pWorld = &pGameWorld->m_Core;
	m_Core.m_pCollision = pGameWorld->Collision();
	m_Core.m_pTeams = pGameWorld->Teams();
}
