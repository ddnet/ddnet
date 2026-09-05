/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_PHYSICS_CHARACTER_H
#define GAME_PHYSICS_CHARACTER_H

#include <base/math.h>
#include <base/vmath.h>

#include <generated/protocol.h>

#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <cmath>

// Shared character weapon handling, used by the server character and the
// client prediction character. TCharacter provides the state and the side
// specific pieces: SetWeapon, SetActiveWeapon, FireWeapon, HandleNinja,
// GiveNinja and RemoveNinja still differ per side, CurrentTuning resolves
// the tune zone and GetCid the own client id.
template<typename TCharacter>
class CCharacterPhysics
{
public:
	static void DoWeaponSwitch(TCharacter *pThis)
	{
		// make sure we can switch
		if(pThis->m_ReloadTimer != 0 || pThis->m_QueuedWeapon == -1)
			return;
		if(pThis->m_Core.m_aWeapons[WEAPON_NINJA].m_Got || !pThis->m_Core.m_aWeapons[pThis->m_QueuedWeapon].m_Got)
			return;

		// switch Weapon
		pThis->SetWeapon(pThis->m_QueuedWeapon);
	}

	static void HandleWeaponSwitch(TCharacter *pThis)
	{
		int WantedWeapon = pThis->m_Core.m_ActiveWeapon;
		if(pThis->m_QueuedWeapon != -1)
			WantedWeapon = pThis->m_QueuedWeapon;

		bool Anything = false;
		for(int i = 0; i < NUM_WEAPONS - 1; ++i)
			if(pThis->m_Core.m_aWeapons[i].m_Got)
				Anything = true;
		if(!Anything)
			return;
		// select Weapon
		int Next = CountInput(pThis->m_LatestPrevInput.m_NextWeapon, pThis->m_LatestInput.m_NextWeapon).m_Presses;
		int Prev = CountInput(pThis->m_LatestPrevInput.m_PrevWeapon, pThis->m_LatestInput.m_PrevWeapon).m_Presses;

		if(Next < 128) // make sure we only try sane stuff
		{
			while(Next) // Next Weapon selection
			{
				WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
				if(pThis->m_Core.m_aWeapons[WantedWeapon].m_Got)
					Next--;
			}
		}

		if(Prev < 128) // make sure we only try sane stuff
		{
			while(Prev) // Prev Weapon selection
			{
				WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
				if(pThis->m_Core.m_aWeapons[WantedWeapon].m_Got)
					Prev--;
			}
		}

		// Direct Weapon selection
		if(pThis->m_LatestInput.m_WantedWeapon)
			WantedWeapon = pThis->m_Input.m_WantedWeapon - 1;

		// check for insane values
		if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != pThis->m_Core.m_ActiveWeapon && pThis->m_Core.m_aWeapons[WantedWeapon].m_Got)
			pThis->m_QueuedWeapon = WantedWeapon;

		DoWeaponSwitch(pThis);
	}

	static void HandleWeapons(TCharacter *pThis)
	{
		//ninja
		pThis->HandleNinja();
		pThis->HandleJetpack();

		if(pThis->m_PainSoundTimer > 0)
			pThis->m_PainSoundTimer--;

		// check reload timer
		if(pThis->m_ReloadTimer)
		{
			pThis->m_ReloadTimer--;
			return;
		}

		// fire Weapon, if wanted
		pThis->FireWeapon();
	}

	static void HandleJetpack(TCharacter *pThis)
	{
		if(pThis->m_Core.m_ActiveWeapon < 0)
			return;

		vec2 Direction = normalize(vec2(pThis->m_LatestInput.m_TargetX, pThis->m_LatestInput.m_TargetY));

		bool FullAuto = false;
		if(pThis->m_Core.m_ActiveWeapon == WEAPON_GRENADE || pThis->m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || pThis->m_Core.m_ActiveWeapon == WEAPON_LASER)
			FullAuto = true;
		if(pThis->m_Core.m_Jetpack && pThis->m_Core.m_ActiveWeapon == WEAPON_GUN)
			FullAuto = true;

		// check if we gonna fire
		bool WillFire = false;
		if(CountInput(pThis->m_LatestPrevInput.m_Fire, pThis->m_LatestInput.m_Fire).m_Presses)
			WillFire = true;

		if(FullAuto && (pThis->m_LatestInput.m_Fire & 1) && pThis->m_Core.m_aWeapons[pThis->m_Core.m_ActiveWeapon].m_Ammo)
			WillFire = true;

		if(!WillFire)
			return;

		// check for ammo
		if(!pThis->m_Core.m_aWeapons[pThis->m_Core.m_ActiveWeapon].m_Ammo || pThis->m_FreezeTime)
		{
			return;
		}

		switch(pThis->m_Core.m_ActiveWeapon)
		{
		case WEAPON_GUN:
		{
			if(pThis->m_Core.m_Jetpack)
			{
				float Strength = pThis->CurrentTuning()->m_JetpackStrength;
				pThis->TakeDamage(Direction * -1.0f * (Strength / 100.0f / 6.11f), 0, pThis->GetCid(), pThis->m_Core.m_ActiveWeapon);
			}
		}
		}
	}

	static void GiveWeapon(TCharacter *pThis, int Weapon, bool Remove)
	{
		if(Weapon == WEAPON_NINJA)
		{
			if(Remove)
				pThis->RemoveNinja();
			else
				pThis->GiveNinja();
			return;
		}

		if(Remove)
		{
			if(pThis->GetActiveWeapon() == Weapon)
				pThis->SetActiveWeapon(WEAPON_GUN);
		}
		else
		{
			pThis->m_Core.m_aWeapons[Weapon].m_Ammo = -1;
		}

		pThis->m_Core.m_aWeapons[Weapon].m_Got = !Remove;
	}

	static void GiveAllWeapons(TCharacter *pThis)
	{
		for(int i = WEAPON_GUN; i < NUM_WEAPONS - 1; i++)
		{
			pThis->GiveWeapon(i);
		}
	}

	static void HandleSpeedupTiles(TCharacter *pThis, int Index)
	{
		if(!pThis->Collision()->IsSpeedup(Index))
			return;

		vec2 Direction, TempVel = pThis->m_Core.m_Vel;
		int Force, Type, MaxSpeed = 0;
		pThis->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed, &Type);

		if(Type == TILE_SPEED_BOOST_OLD)
		{
			float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
			if(Force == 255 && MaxSpeed)
			{
				pThis->m_Core.m_Vel = Direction * (MaxSpeed / 5);
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
				{
					TempVel += Direction * Force;
				}

				pThis->m_Core.m_Vel = ClampVel(pThis->m_MoveRestrictions, TempVel);
			}
		}
		else if(Type == TILE_SPEED_BOOST)
		{
			constexpr float MaxSpeedScale = 5.0f;
			if(MaxSpeed == 0)
			{
				float MaxRampSpeed = pThis->CurrentTuning()->m_VelrampRange / (50 * log(std::max((float)pThis->CurrentTuning()->m_VelrampCurvature, 1.01f)));
				MaxSpeed = std::max(MaxRampSpeed, pThis->CurrentTuning()->m_VelrampStart / 50) * MaxSpeedScale;
			}

			// (signed) length of projection
			float CurrentDirectionalSpeed = dot(Direction, pThis->m_Core.m_Vel);
			float TempMaxSpeed = MaxSpeed / MaxSpeedScale;
			if(CurrentDirectionalSpeed + Force > TempMaxSpeed)
				TempVel += Direction * (TempMaxSpeed - CurrentDirectionalSpeed);
			else
				TempVel += Direction * Force;

			pThis->m_Core.m_Vel = ClampVel(pThis->m_MoveRestrictions, TempVel);
		}
	}

	// The game and front layer tile toggles up to the solo tiles, which the
	// server handles in the game mode controller instead.
	static void HandleTileFlags(TCharacter *pThis)
	{
		// freeze
		if(((pThis->m_TileIndex == TILE_FREEZE) || (pThis->m_TileFIndex == TILE_FREEZE)) && !pThis->m_Core.m_Super && !pThis->m_Core.m_Invincible && !pThis->m_Core.m_DeepFrozen)
		{
			pThis->Freeze();
		}
		else if(((pThis->m_TileIndex == TILE_UNFREEZE) || (pThis->m_TileFIndex == TILE_UNFREEZE)) && !pThis->m_Core.m_DeepFrozen)
		{
			pThis->Unfreeze();
		}

		// deep freeze
		if(((pThis->m_TileIndex == TILE_DFREEZE) || (pThis->m_TileFIndex == TILE_DFREEZE)) && !pThis->m_Core.m_Super && !pThis->m_Core.m_Invincible && !pThis->m_Core.m_DeepFrozen)
		{
			pThis->m_Core.m_DeepFrozen = true;
		}
		else if(((pThis->m_TileIndex == TILE_DUNFREEZE) || (pThis->m_TileFIndex == TILE_DUNFREEZE)) && !pThis->m_Core.m_Super && !pThis->m_Core.m_Invincible && pThis->m_Core.m_DeepFrozen)
		{
			pThis->m_Core.m_DeepFrozen = false;
		}

		// live freeze
		if(((pThis->m_TileIndex == TILE_LFREEZE) || (pThis->m_TileFIndex == TILE_LFREEZE)) && !pThis->m_Core.m_Super && !pThis->m_Core.m_Invincible)
		{
			pThis->m_Core.m_LiveFrozen = true;
		}
		else if(((pThis->m_TileIndex == TILE_LUNFREEZE) || (pThis->m_TileFIndex == TILE_LUNFREEZE)) && !pThis->m_Core.m_Super && !pThis->m_Core.m_Invincible)
		{
			pThis->m_Core.m_LiveFrozen = false;
		}

		// endless hook
		if((pThis->m_TileIndex == TILE_EHOOK_ENABLE) || (pThis->m_TileFIndex == TILE_EHOOK_ENABLE))
		{
			if(!pThis->m_Core.m_EndlessHook)
			{
				pThis->SendTileChat("Endless hook has been activated");
				pThis->m_Core.m_EndlessHook = true;
			}
		}
		else if((pThis->m_TileIndex == TILE_EHOOK_DISABLE) || (pThis->m_TileFIndex == TILE_EHOOK_DISABLE))
		{
			if(pThis->m_Core.m_EndlessHook)
			{
				pThis->SendTileChat("Endless hook has been deactivated");
				pThis->m_Core.m_EndlessHook = false;
			}
		}

		// hit others
		if(((pThis->m_TileIndex == TILE_HIT_DISABLE) || (pThis->m_TileFIndex == TILE_HIT_DISABLE)) && (!pThis->m_Core.m_HammerHitDisabled || !pThis->m_Core.m_ShotgunHitDisabled || !pThis->m_Core.m_GrenadeHitDisabled || !pThis->m_Core.m_LaserHitDisabled))
		{
			pThis->SendTileChat("You can't hit others");
			pThis->m_Core.m_HammerHitDisabled = true;
			pThis->m_Core.m_ShotgunHitDisabled = true;
			pThis->m_Core.m_GrenadeHitDisabled = true;
			pThis->m_Core.m_LaserHitDisabled = true;
		}
		else if(((pThis->m_TileIndex == TILE_HIT_ENABLE) || (pThis->m_TileFIndex == TILE_HIT_ENABLE)) && (pThis->m_Core.m_HammerHitDisabled || pThis->m_Core.m_ShotgunHitDisabled || pThis->m_Core.m_GrenadeHitDisabled || pThis->m_Core.m_LaserHitDisabled))
		{
			pThis->SendTileChat("You can hit others");
			pThis->m_Core.m_ShotgunHitDisabled = false;
			pThis->m_Core.m_GrenadeHitDisabled = false;
			pThis->m_Core.m_HammerHitDisabled = false;
			pThis->m_Core.m_LaserHitDisabled = false;
		}

		// collide with others
		if(((pThis->m_TileIndex == TILE_NPC_DISABLE) || (pThis->m_TileFIndex == TILE_NPC_DISABLE)) && !pThis->m_Core.m_CollisionDisabled)
		{
			pThis->SendTileChat("You can't collide with others");
			pThis->m_Core.m_CollisionDisabled = true;
		}
		else if(((pThis->m_TileIndex == TILE_NPC_ENABLE) || (pThis->m_TileFIndex == TILE_NPC_ENABLE)) && pThis->m_Core.m_CollisionDisabled)
		{
			pThis->SendTileChat("You can collide with others");
			pThis->m_Core.m_CollisionDisabled = false;
		}

		// hook others
		if(((pThis->m_TileIndex == TILE_NPH_DISABLE) || (pThis->m_TileFIndex == TILE_NPH_DISABLE)) && !pThis->m_Core.m_HookHitDisabled)
		{
			pThis->SendTileChat("You can't hook others");
			pThis->m_Core.m_HookHitDisabled = true;
		}
		else if(((pThis->m_TileIndex == TILE_NPH_ENABLE) || (pThis->m_TileFIndex == TILE_NPH_ENABLE)) && pThis->m_Core.m_HookHitDisabled)
		{
			pThis->SendTileChat("You can hook others");
			pThis->m_Core.m_HookHitDisabled = false;
		}

		// unlimited air jumps
		if(((pThis->m_TileIndex == TILE_UNLIMITED_JUMPS_ENABLE) || (pThis->m_TileFIndex == TILE_UNLIMITED_JUMPS_ENABLE)) && !pThis->m_Core.m_EndlessJump)
		{
			pThis->SendTileChat("You have unlimited air jumps");
			pThis->m_Core.m_EndlessJump = true;
		}
		else if(((pThis->m_TileIndex == TILE_UNLIMITED_JUMPS_DISABLE) || (pThis->m_TileFIndex == TILE_UNLIMITED_JUMPS_DISABLE)) && pThis->m_Core.m_EndlessJump)
		{
			pThis->SendTileChat("You don't have unlimited air jumps");
			pThis->m_Core.m_EndlessJump = false;
		}

		// walljump
		if((pThis->m_TileIndex == TILE_WALLJUMP) || (pThis->m_TileFIndex == TILE_WALLJUMP))
		{
			if(pThis->m_Core.m_Vel.y > 0 && pThis->m_Core.m_Colliding && pThis->m_Core.m_LeftWall)
			{
				pThis->m_Core.m_LeftWall = false;
				pThis->m_Core.m_JumpedTotal = pThis->m_Core.m_Jumps >= 2 ? pThis->m_Core.m_Jumps - 2 : 0;
				pThis->m_Core.m_Jumped = 1;
			}
		}

		// jetpack gun
		if(((pThis->m_TileIndex == TILE_JETPACK_ENABLE) || (pThis->m_TileFIndex == TILE_JETPACK_ENABLE)) && !pThis->m_Core.m_Jetpack)
		{
			pThis->SendTileChat("You have a jetpack gun");
			pThis->m_Core.m_Jetpack = true;
		}
		else if(((pThis->m_TileIndex == TILE_JETPACK_DISABLE) || (pThis->m_TileFIndex == TILE_JETPACK_DISABLE)) && pThis->m_Core.m_Jetpack)
		{
			pThis->SendTileChat("You lost your jetpack gun");
			pThis->m_Core.m_Jetpack = false;
		}
	}

	// The refill jumps, telegun and stopper tiles and the switch layer
	// handling after the solo tiles.
	static void HandleSwitchTiles(TCharacter *pThis, int MapIndex)
	{
		// refill jumps
		if(((pThis->m_TileIndex == TILE_REFILL_JUMPS) || (pThis->m_TileFIndex == TILE_REFILL_JUMPS)) && !pThis->m_LastRefillJumps)
		{
			pThis->m_Core.m_JumpedTotal = 0;
			pThis->m_Core.m_Jumped = 0;
			pThis->m_LastRefillJumps = true;
		}
		if((pThis->m_TileIndex != TILE_REFILL_JUMPS) && (pThis->m_TileFIndex != TILE_REFILL_JUMPS))
		{
			pThis->m_LastRefillJumps = false;
		}

		// Teleport gun
		if(((pThis->m_TileIndex == TILE_TELE_GUN_ENABLE) || (pThis->m_TileFIndex == TILE_TELE_GUN_ENABLE)) && !pThis->m_Core.m_HasTelegunGun)
		{
			pThis->m_Core.m_HasTelegunGun = true;

			pThis->SendTileChat("Teleport gun enabled");
		}
		else if(((pThis->m_TileIndex == TILE_TELE_GUN_DISABLE) || (pThis->m_TileFIndex == TILE_TELE_GUN_DISABLE)) && pThis->m_Core.m_HasTelegunGun)
		{
			pThis->m_Core.m_HasTelegunGun = false;

			pThis->SendTileChat("Teleport gun disabled");
		}

		if(((pThis->m_TileIndex == TILE_TELE_GRENADE_ENABLE) || (pThis->m_TileFIndex == TILE_TELE_GRENADE_ENABLE)) && !pThis->m_Core.m_HasTelegunGrenade)
		{
			pThis->m_Core.m_HasTelegunGrenade = true;

			pThis->SendTileChat("Teleport grenade enabled");
		}
		else if(((pThis->m_TileIndex == TILE_TELE_GRENADE_DISABLE) || (pThis->m_TileFIndex == TILE_TELE_GRENADE_DISABLE)) && pThis->m_Core.m_HasTelegunGrenade)
		{
			pThis->m_Core.m_HasTelegunGrenade = false;

			pThis->SendTileChat("Teleport grenade disabled");
		}

		if(((pThis->m_TileIndex == TILE_TELE_LASER_ENABLE) || (pThis->m_TileFIndex == TILE_TELE_LASER_ENABLE)) && !pThis->m_Core.m_HasTelegunLaser)
		{
			pThis->m_Core.m_HasTelegunLaser = true;

			pThis->SendTileChat("Teleport laser enabled");
		}
		else if(((pThis->m_TileIndex == TILE_TELE_LASER_DISABLE) || (pThis->m_TileFIndex == TILE_TELE_LASER_DISABLE)) && pThis->m_Core.m_HasTelegunLaser)
		{
			pThis->m_Core.m_HasTelegunLaser = false;

			pThis->SendTileChat("Teleport laser disabled");
		}

		// stopper
		if(pThis->m_Core.m_Vel.y > 0 && (pThis->m_MoveRestrictions & CANTMOVE_DOWN))
		{
			pThis->m_Core.m_Jumped = 0;
			pThis->m_Core.m_JumpedTotal = 0;
		}
		pThis->ApplyMoveRestrictions();

		// handle switch tiles
		const int SwitchType = pThis->Collision()->GetSwitchType(MapIndex);
		const int SwitchNumber = pThis->Collision()->GetSwitchNumber(MapIndex);
		const int SwitchDelay = pThis->Collision()->GetSwitchDelay(MapIndex);
		if(SwitchType == TILE_SWITCHOPEN && pThis->Team() != pThis->TeamsCore()->TeamSuper() && SwitchNumber > 0)
		{
			pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()] = true;
			pThis->Switchers()[SwitchNumber].m_aEndTick[pThis->Team()] = 0;
			pThis->Switchers()[SwitchNumber].m_aType[pThis->Team()] = TILE_SWITCHOPEN;
			pThis->Switchers()[SwitchNumber].m_aLastUpdateTick[pThis->Team()] = pThis->GameWorld()->GameTick();
		}
		else if(SwitchType == TILE_SWITCHTIMEDOPEN && pThis->Team() != pThis->TeamsCore()->TeamSuper() && SwitchNumber > 0)
		{
			pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()] = true;
			pThis->Switchers()[SwitchNumber].m_aEndTick[pThis->Team()] = pThis->GameWorld()->GameTick() + 1 + SwitchDelay * pThis->GameWorld()->GameTickSpeed();
			pThis->Switchers()[SwitchNumber].m_aType[pThis->Team()] = TILE_SWITCHTIMEDOPEN;
			pThis->Switchers()[SwitchNumber].m_aLastUpdateTick[pThis->Team()] = pThis->GameWorld()->GameTick();
		}
		else if(SwitchType == TILE_SWITCHTIMEDCLOSE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && SwitchNumber > 0)
		{
			pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()] = false;
			pThis->Switchers()[SwitchNumber].m_aEndTick[pThis->Team()] = pThis->GameWorld()->GameTick() + 1 + SwitchDelay * pThis->GameWorld()->GameTickSpeed();
			pThis->Switchers()[SwitchNumber].m_aType[pThis->Team()] = TILE_SWITCHTIMEDCLOSE;
			pThis->Switchers()[SwitchNumber].m_aLastUpdateTick[pThis->Team()] = pThis->GameWorld()->GameTick();
		}
		else if(SwitchType == TILE_SWITCHCLOSE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && SwitchNumber > 0)
		{
			pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()] = false;
			pThis->Switchers()[SwitchNumber].m_aEndTick[pThis->Team()] = 0;
			pThis->Switchers()[SwitchNumber].m_aType[pThis->Team()] = TILE_SWITCHCLOSE;
			pThis->Switchers()[SwitchNumber].m_aLastUpdateTick[pThis->Team()] = pThis->GameWorld()->GameTick();
		}
		else if(SwitchType == TILE_FREEZE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && !pThis->m_Core.m_Invincible)
		{
			if(SwitchNumber == 0 || pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()])
			{
				pThis->Freeze(SwitchDelay);
			}
		}
		else if(SwitchType == TILE_DFREEZE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && !pThis->m_Core.m_Invincible)
		{
			if(SwitchNumber == 0 || pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()])
				pThis->m_Core.m_DeepFrozen = true;
		}
		else if(SwitchType == TILE_DUNFREEZE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && !pThis->m_Core.m_Invincible)
		{
			if(SwitchNumber == 0 || pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()])
				pThis->m_Core.m_DeepFrozen = false;
		}
		else if(SwitchType == TILE_LFREEZE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && !pThis->m_Core.m_Invincible)
		{
			if(SwitchNumber == 0 || pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()])
			{
				pThis->m_Core.m_LiveFrozen = true;
			}
		}
		else if(SwitchType == TILE_LUNFREEZE && pThis->Team() != pThis->TeamsCore()->TeamSuper() && !pThis->m_Core.m_Invincible)
		{
			if(SwitchNumber == 0 || pThis->Switchers()[SwitchNumber].m_aStatus[pThis->Team()])
			{
				pThis->m_Core.m_LiveFrozen = false;
			}
		}
		else if(SwitchType == TILE_HIT_ENABLE && pThis->m_Core.m_HammerHitDisabled && SwitchDelay == WEAPON_HAMMER)
		{
			pThis->SendTileChat("You can hammer hit others");
			pThis->m_Core.m_HammerHitDisabled = false;
		}
		else if(SwitchType == TILE_HIT_DISABLE && !pThis->m_Core.m_HammerHitDisabled && SwitchDelay == WEAPON_HAMMER)
		{
			pThis->SendTileChat("You can't hammer hit others");
			pThis->m_Core.m_HammerHitDisabled = true;
		}
		else if(SwitchType == TILE_HIT_ENABLE && pThis->m_Core.m_ShotgunHitDisabled && SwitchDelay == WEAPON_SHOTGUN)
		{
			pThis->SendTileChat("You can shoot others with shotgun");
			pThis->m_Core.m_ShotgunHitDisabled = false;
		}
		else if(SwitchType == TILE_HIT_DISABLE && !pThis->m_Core.m_ShotgunHitDisabled && SwitchDelay == WEAPON_SHOTGUN)
		{
			pThis->SendTileChat("You can't shoot others with shotgun");
			pThis->m_Core.m_ShotgunHitDisabled = true;
		}
		else if(SwitchType == TILE_HIT_ENABLE && pThis->m_Core.m_GrenadeHitDisabled && SwitchDelay == WEAPON_GRENADE)
		{
			pThis->SendTileChat("You can shoot others with grenade");
			pThis->m_Core.m_GrenadeHitDisabled = false;
		}
		else if(SwitchType == TILE_HIT_DISABLE && !pThis->m_Core.m_GrenadeHitDisabled && SwitchDelay == WEAPON_GRENADE)
		{
			pThis->SendTileChat("You can't shoot others with grenade");
			pThis->m_Core.m_GrenadeHitDisabled = true;
		}
		else if(SwitchType == TILE_HIT_ENABLE && pThis->m_Core.m_LaserHitDisabled && SwitchDelay == WEAPON_LASER)
		{
			pThis->SendTileChat("You can shoot others with laser");
			pThis->m_Core.m_LaserHitDisabled = false;
		}
		else if(SwitchType == TILE_HIT_DISABLE && !pThis->m_Core.m_LaserHitDisabled && SwitchDelay == WEAPON_LASER)
		{
			pThis->SendTileChat("You can't shoot others with laser");
			pThis->m_Core.m_LaserHitDisabled = true;
		}
		else if(SwitchType == TILE_JUMP)
		{
			int NewJumps = SwitchDelay;
			if(NewJumps == 255)
			{
				NewJumps = -1;
			}

			if(NewJumps != pThis->m_Core.m_Jumps)
			{
				pThis->SendJumpsChat(NewJumps);
				pThis->m_Core.m_Jumps = NewJumps;
			}
		}
	}

	static void UpdateIsInFreeze(TCharacter *pThis)
	{
		// check if the tee is in any type of freeze
		int Index = pThis->Collision()->GetPureMapIndex(pThis->m_Pos);
		const int aTiles[] = {
			pThis->Collision()->GetTileIndex(Index),
			pThis->Collision()->GetFrontTileIndex(Index),
			pThis->Collision()->GetSwitchType(Index)};
		pThis->m_Core.m_IsInFreeze = false;
		for(const int Tile : aTiles)
		{
			if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE || Tile == TILE_DEATH)
			{
				pThis->m_Core.m_IsInFreeze = true;
				break;
			}
		}
		pThis->m_Core.m_IsInFreeze |= (pThis->Collision()->GetCollisionAt(pThis->m_Pos.x + pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y - pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetCollisionAt(pThis->m_Pos.x + pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y + pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetCollisionAt(pThis->m_Pos.x - pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y - pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetCollisionAt(pThis->m_Pos.x - pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y + pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetFrontCollisionAt(pThis->m_Pos.x + pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y - pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetFrontCollisionAt(pThis->m_Pos.x + pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y + pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetFrontCollisionAt(pThis->m_Pos.x - pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y - pThis->GetProximityRadius() / 3.f) == TILE_DEATH ||
					       pThis->Collision()->GetFrontCollisionAt(pThis->m_Pos.x - pThis->GetProximityRadius() / 3.f, pThis->m_Pos.y + pThis->GetProximityRadius() / 3.f) == TILE_DEATH);
	}
};

#endif // GAME_PHYSICS_CHARACTER_H
