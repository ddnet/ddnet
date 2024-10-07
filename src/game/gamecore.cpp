/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecore.h"

#include "collision.h"
#include "mapitems.h"
#include "teamscore.h"

#include <base/system.h>
#include <engine/shared/config.h>

#include <limits>

const char *CTuningParams::ms_apNames[] =
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) #ScriptName,
#include "tuning.h"
#undef MACRO_TUNING_PARAM
};

bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, Name(i)) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, Name(i)) == 0)
			return Get(i, pValue);

	return false;
}

float CTuningParams::GetWeaponFireDelay(int Weapon) const
{
	switch(Weapon)
	{
	case WEAPON_HAMMER: return (float)m_HammerHitFireDelay / 1000.0f;
	case WEAPON_GUN: return (float)m_GunFireDelay / 1000.0f;
	case WEAPON_SHOTGUN: return (float)m_ShotgunFireDelay / 1000.0f;
	case WEAPON_GRENADE: return (float)m_GrenadeFireDelay / 1000.0f;
	case WEAPON_LASER: return (float)m_LaserFireDelay / 1000.0f;
	case WEAPON_NINJA: return (float)m_NinjaFireDelay / 1000.0f;
	default: dbg_assert(false, "invalid weapon"); return 0.0f; // this value should not be reached
	}
}

static_assert(std::numeric_limits<char>::is_signed, "char must be signed for StrToInts to work correctly");

void StrToInts(int *pInts, size_t NumInts, const char *pStr)
{
	dbg_assert(NumInts > 0, "StrToInts: NumInts invalid");
	const size_t StrSize = str_length(pStr) + 1;
	dbg_assert(StrSize <= NumInts * sizeof(int), "StrToInts: string truncated");

	for(size_t i = 0; i < NumInts; i++)
	{
		// Copy to temporary buffer to ensure we don't read past the end of the input string
		char aBuf[sizeof(int)] = {0, 0, 0, 0};
		for(size_t c = 0; c < sizeof(int) && i * sizeof(int) + c < StrSize; c++)
		{
			aBuf[c] = pStr[i * sizeof(int) + c];
		}
		pInts[i] = ((aBuf[0] + 128) << 24) | ((aBuf[1] + 128) << 16) | ((aBuf[2] + 128) << 8) | (aBuf[3] + 128);
	}
	// Last byte is always zero and unused in this format
	pInts[NumInts - 1] &= 0xFFFFFF00;
}

bool IntsToStr(const int *pInts, size_t NumInts, char *pStr, size_t StrSize)
{
	dbg_assert(NumInts > 0, "IntsToStr: NumInts invalid");
	dbg_assert(StrSize >= NumInts * sizeof(int), "IntsToStr: StrSize invalid");

	// Unpack string without validation
	size_t StrIndex = 0;
	for(size_t IntIndex = 0; IntIndex < NumInts; IntIndex++)
	{
		const int CurrentInt = pInts[IntIndex];
		pStr[StrIndex] = ((CurrentInt >> 24) & 0xff) - 128;
		StrIndex++;
		pStr[StrIndex] = ((CurrentInt >> 16) & 0xff) - 128;
		StrIndex++;
		pStr[StrIndex] = ((CurrentInt >> 8) & 0xff) - 128;
		StrIndex++;
		pStr[StrIndex] = (CurrentInt & 0xff) - 128;
		StrIndex++;
	}
	// Ensure null-termination
	pStr[StrIndex - 1] = '\0';

	// Ensure valid UTF-8
	if(str_utf8_check(pStr))
	{
		return true;
	}
	pStr[0] = '\0';
	return false;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f / std::pow(Curvature, (Value - Start) / Range);
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore *pTeams)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;

	m_pTeams = pTeams;
	m_Id = -1;

	// fail safe, if core's tuning didn't get updated at all, just fallback to world tuning.
	m_Tuning = m_pWorld->m_aTuning[g_Config.m_ClDummy];
}

void CCharacterCore::SetCoreWorld(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore *pTeams)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_pTeams = pTeams;
}

void CCharacterCore::Reset()
{
	m_Pos = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_NewHook = false;
	m_HookPos = vec2(0, 0);
	m_HookDir = vec2(0, 0);
	m_HookTeleBase = vec2(0, 0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	SetHookedPlayer(-1);
	m_AttachedPlayers.clear();
	m_Jumped = 0;
	m_JumpedTotal = 0;
	m_Jumps = 2;
	m_TriggeredEvents = 0;

	// DDNet Character
	m_Solo = false;
	m_Jetpack = false;
	m_CollisionDisabled = false;
	m_EndlessHook = false;
	m_EndlessJump = false;
	m_HammerHitDisabled = false;
	m_GrenadeHitDisabled = false;
	m_LaserHitDisabled = false;
	m_ShotgunHitDisabled = false;
	m_HookHitDisabled = false;
	m_Super = false;
	m_Invincible = false;
	m_HasTelegunGun = false;
	m_HasTelegunGrenade = false;
	m_HasTelegunLaser = false;
	m_FreezeStart = 0;
	m_FreezeEnd = 0;
	m_IsInFreeze = false;
	m_DeepFrozen = false;
	m_LiveFrozen = false;

	// never initialize both to 0
	m_Input.m_TargetX = 0;
	m_Input.m_TargetY = -1;
}

void CCharacterCore::Tick(bool UseInput, bool DoDeferredTick)
{
	m_MoveRestrictions = m_pCollision->GetMoveRestrictions(UseInput ? IsSwitchActiveCb : 0, this, m_Pos);
	m_TriggeredEvents = 0;

	// get ground state
	const bool Grounded = m_pCollision->CheckPoint(m_Pos.x + PhysicalSize() / 2, m_Pos.y + PhysicalSize() / 2 + 5) || m_pCollision->CheckPoint(m_Pos.x - PhysicalSize() / 2, m_Pos.y + PhysicalSize() / 2 + 5);
	vec2 TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	m_Vel.y += m_Tuning.m_Gravity;

	float MaxSpeed = Grounded ? m_Tuning.m_GroundControlSpeed : m_Tuning.m_AirControlSpeed;
	float Accel = Grounded ? m_Tuning.m_GroundControlAccel : m_Tuning.m_AirControlAccel;
	float Friction = Grounded ? m_Tuning.m_GroundFriction : m_Tuning.m_AirFriction;

	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;

		// setup angle
		float TmpAngle = std::atan2(m_Input.m_TargetY, m_Input.m_TargetX);
		if(TmpAngle < -(pi / 2.0f))
		{
			m_Angle = (int)((TmpAngle + (2.0f * pi)) * 256.0f);
		}
		else
		{
			m_Angle = (int)(TmpAngle * 256.0f);
		}

		// Special jump cases:
		// m_Jumps == -1: A tee may only make one ground jump. Second jumped bit is always set
		// m_Jumps == 0: A tee may not make a jump. Second jumped bit is always set
		// m_Jumps == 1: A tee may do either a ground jump or an air jump. Second jumped bit is set after the first jump
		// The second jumped bit can be overridden by special tiles so that the tee can nevertheless jump.

		// handle jump
		if(m_Input.m_Jump)
		{
			if(!(m_Jumped & 1))
			{
				if(Grounded && (!(m_Jumped & 2) || m_Jumps != 0))
				{
					m_TriggeredEvents |= COREEVENT_GROUND_JUMP;
					m_Vel.y = -m_Tuning.m_GroundJumpImpulse;
					if(m_Jumps > 1)
					{
						m_Jumped |= 1;
					}
					else
					{
						m_Jumped |= 3;
					}
					m_JumpedTotal = 0;
				}
				else if(!(m_Jumped & 2))
				{
					m_TriggeredEvents |= COREEVENT_AIR_JUMP;
					m_Vel.y = -m_Tuning.m_AirJumpImpulse;
					m_Jumped |= 3;
					m_JumpedTotal++;
				}
			}
		}
		else
		{
			m_Jumped &= ~1;
		}

		// handle hook
		if(m_Input.m_Hook)
		{
			if(m_HookState == HOOK_IDLE)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos + TargetDirection * PhysicalSize() * 1.5f;
				m_HookDir = TargetDirection;
				SetHookedPlayer(-1);
				m_HookTick = (float)SERVER_TICK_SPEED * (1.25f - m_Tuning.m_HookDuration);
				m_TriggeredEvents |= COREEVENT_HOOK_LAUNCH;
			}
		}
		else
		{
			SetHookedPlayer(-1);
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
	}

	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input (player is holding space bar)
	// 2 bit = to track if all air-jumps have been used up (tee gets dark feet)
	if(Grounded)
	{
		m_Jumped &= ~2;
		m_JumpedTotal = 0;
	}

	// add the speed modification according to players wanted direction
	if(m_Direction < 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	if(m_Direction > 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);
	if(m_Direction == 0)
		m_Vel.x *= Friction;

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		SetHookedPlayer(-1);
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_HookState = HOOK_RETRACTED;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 HookBase = m_Pos;
		if(m_NewHook)
		{
			HookBase = m_HookTeleBase;
		}
		vec2 NewPos = m_HookPos + m_HookDir * m_Tuning.m_HookFireSpeed;
		if(distance(HookBase, NewPos) > m_Tuning.m_HookLength)
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = HookBase + normalize(NewPos - HookBase) * m_Tuning.m_HookLength;
			m_Reset = true;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		bool GoingThroughTele = false;
		int teleNr = 0;
		int Hit = m_pCollision->IntersectLineTeleHook(m_HookPos, NewPos, &NewPos, 0, &teleNr);

		if(Hit)
		{
			if(Hit == TILE_NOHOOK)
				GoingToRetract = true;
			else if(Hit == TILE_TELEINHOOK)
				GoingThroughTele = true;
			else
				GoingToHitGround = true;
			m_Reset = true;
		}

		// Check against other players first
		if(!m_HookHitDisabled && m_pWorld && m_Tuning.m_PlayerHooking && (m_HookState == HOOK_FLYING || !m_NewHook))
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || pCharCore == this || (!(m_Super || pCharCore->m_Super) && ((m_Id != -1 && !m_pTeams->CanCollide(i, m_Id)) || pCharCore->m_Solo || m_Solo)))
					continue;

				vec2 ClosestPoint;
				if(closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos, ClosestPoint))
				{
					if(distance(pCharCore->m_Pos, ClosestPoint) < PhysicalSize() + 2.0f)
					{
						if(m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
						{
							m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER;
							m_HookState = HOOK_GRABBED;
							SetHookedPlayer(i);
							Distance = distance(m_HookPos, pCharCore->m_Pos);
						}
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_GROUND;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			if(GoingThroughTele && m_pWorld && !m_pCollision->TeleOuts(teleNr - 1).empty())
			{
				m_TriggeredEvents = 0;
				SetHookedPlayer(-1);

				m_NewHook = true;
				int RandomOut = m_pWorld->RandomOr0(m_pCollision->TeleOuts(teleNr - 1).size());
				m_HookPos = m_pCollision->TeleOuts(teleNr - 1)[RandomOut] + TargetDirection * PhysicalSize() * 1.5f;
				m_HookDir = TargetDirection;
				m_HookTeleBase = m_HookPos;
			}
			else
			{
				m_HookPos = NewPos;
			}
		}
	}

	if(m_HookState == HOOK_GRABBED)
	{
		if(m_HookedPlayer != -1 && m_pWorld)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
			if(pCharCore && m_Id != -1 && m_pTeams->CanKeepHook(m_Id, pCharCore->m_Id))
				m_HookPos = pCharCore->m_Pos;
			else
			{
				// release hook
				SetHookedPlayer(-1);
				m_HookState = HOOK_RETRACTED;
				m_HookPos = m_Pos;
			}
		}

		// don't do this hook routine when we are already hooked to a player
		if(m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f)
		{
			vec2 HookVel = normalize(m_HookPos - m_Pos) * m_Tuning.m_HookDragAccel;
			// the hook as more power to drag you up then down.
			// this makes it easier to get on top of an platform
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;

			// the hook will boost it's power if the player wants to move
			// in that direction. otherwise it will dampen everything abit
			if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 NewVel = m_Vel + HookVel;

			// check if we are under the legal limit for the hook
			const float NewVelLength = length(NewVel);
			if(NewVelLength < m_Tuning.m_HookDragSpeed || NewVelLength < length(m_Vel))
				m_Vel = NewVel; // no problem. apply
		}

		// release hook (max default hook time is 1.25 s)
		m_HookTick++;
		if(m_HookedPlayer != -1 && (m_HookTick > SERVER_TICK_SPEED + SERVER_TICK_SPEED / 5 || (m_pWorld && !m_pWorld->m_apCharacters[m_HookedPlayer])))
		{
			SetHookedPlayer(-1);
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(DoDeferredTick)
		TickDeferred();
}

void CCharacterCore::TickDeferred()
{
	if(m_pWorld)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			if(pCharCore == this || (m_Id != -1 && !m_pTeams->CanCollide(m_Id, i)))
				continue; // make sure that we don't nudge our self

			if(!(m_Super || pCharCore->m_Super) && (m_Solo || pCharCore->m_Solo))
				continue;

			// handle player <-> player collision
			float Distance = distance(m_Pos, pCharCore->m_Pos);
			if(Distance > 0)
			{
				vec2 Dir = normalize(m_Pos - pCharCore->m_Pos);

				bool CanCollide = (m_Super || pCharCore->m_Super) || (!m_CollisionDisabled && !pCharCore->m_CollisionDisabled && m_Tuning.m_PlayerCollision);

				if(CanCollide && Distance < PhysicalSize() * 1.25f)
				{
					float a = (PhysicalSize() * 1.45f - Distance);
					float Velocity = 0.5f;

					// make sure that we don't add excess force by checking the
					// direction against the current velocity. if not zero.
					if(length(m_Vel) > 0.0001f)
						Velocity = 1 - (dot(normalize(m_Vel), Dir) + 1) / 2; // Wdouble-promotion don't fix this as this might change game physics

					m_Vel += Dir * a * (Velocity * 0.75f);
					m_Vel *= 0.85f;
				}

				// handle hook influence
				if(!m_HookHitDisabled && m_HookedPlayer == i && m_Tuning.m_PlayerHooking)
				{
					if(Distance > PhysicalSize() * 1.50f)
					{
						float HookAccel = m_Tuning.m_HookDragAccel * (Distance / m_Tuning.m_HookLength);
						float DragSpeed = m_Tuning.m_HookDragSpeed;

						vec2 Temp;
						// add force to the hooked player
						Temp.x = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.x, HookAccel * Dir.x * 1.5f);
						Temp.y = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.y, HookAccel * Dir.y * 1.5f);
						pCharCore->m_Vel = ClampVel(pCharCore->m_MoveRestrictions, Temp);
						// add a little bit force to the guy who has the grip
						Temp.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, -HookAccel * Dir.x * 0.25f);
						Temp.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, -HookAccel * Dir.y * 0.25f);
						m_Vel = ClampVel(m_MoveRestrictions, Temp);
					}
				}
			}
		}

		if(m_HookState != HOOK_FLYING)
		{
			m_NewHook = false;
		}
	}

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;
}

void CCharacterCore::Move()
{
	float RampValue = VelocityRamp(length(m_Vel) * 50, m_Tuning.m_VelrampStart, m_Tuning.m_VelrampRange, m_Tuning.m_VelrampCurvature);

	m_Vel.x = m_Vel.x * RampValue;

	vec2 NewPos = m_Pos;

	vec2 OldVel = m_Vel;
	bool Grounded = false;
	m_pCollision->MoveBox(&NewPos, &m_Vel, PhysicalSizeVec2(),
		vec2(m_Tuning.m_GroundElasticityX,
			m_Tuning.m_GroundElasticityY),
		&Grounded);

	if(Grounded)
	{
		m_Jumped &= ~2;
		m_JumpedTotal = 0;
	}

	m_Colliding = 0;
	if(m_Vel.x < 0.001f && m_Vel.x > -0.001f)
	{
		if(OldVel.x > 0)
			m_Colliding = 1;
		else if(OldVel.x < 0)
			m_Colliding = 2;
	}
	else
		m_LeftWall = true;

	m_Vel.x = m_Vel.x * (1.0f / RampValue);

	if(m_pWorld && (m_Super || (m_Tuning.m_PlayerCollision && !m_CollisionDisabled && !m_Solo)))
	{
		// check player collision
		float Distance = distance(m_Pos, NewPos);
		if(Distance > 0)
		{
			int End = Distance + 1;
			vec2 LastPos = m_Pos;
			for(int i = 0; i < End; i++)
			{
				float a = i / Distance;
				vec2 Pos = mix(m_Pos, NewPos, a);
				for(int p = 0; p < MAX_CLIENTS; p++)
				{
					CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
					if(!pCharCore || pCharCore == this)
						continue;
					if((!(pCharCore->m_Super || m_Super) && (m_Solo || pCharCore->m_Solo || pCharCore->m_CollisionDisabled || (m_Id != -1 && !m_pTeams->CanCollide(m_Id, p)))))
						continue;
					float D = distance(Pos, pCharCore->m_Pos);
					if(D < PhysicalSize())
					{
						if(a > 0.0f)
							m_Pos = LastPos;
						else if(distance(NewPos, pCharCore->m_Pos) > D)
							m_Pos = NewPos;
						return;
					}
				}
				LastPos = Pos;
			}
		}
	}

	m_Pos = NewPos;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore) const
{
	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_VelX = round_to_int(m_Vel.x * 256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y * 256.0f);
	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round_to_int(m_HookPos.x);
	pObjCore->m_HookY = round_to_int(m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(m_HookDir.x * 256.0f);
	pObjCore->m_HookDy = round_to_int(m_HookDir.y * 256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Angle = m_Angle;
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;
	m_Vel.x = pObjCore->m_VelX / 256.0f;
	m_Vel.y = pObjCore->m_VelY / 256.0f;
	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx / 256.0f;
	m_HookDir.y = pObjCore->m_HookDy / 256.0f;
	SetHookedPlayer(pObjCore->m_HookedPlayer);
	m_Jumped = pObjCore->m_Jumped;
	m_Direction = pObjCore->m_Direction;
	m_Angle = pObjCore->m_Angle;
}

void CCharacterCore::ReadDDNet(const CNetObj_DDNetCharacter *pObjDDNet)
{
	// Collision
	m_Solo = pObjDDNet->m_Flags & CHARACTERFLAG_SOLO;
	m_Jetpack = pObjDDNet->m_Flags & CHARACTERFLAG_JETPACK;
	m_CollisionDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_COLLISION_DISABLED;
	m_HammerHitDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_HAMMER_HIT_DISABLED;
	m_ShotgunHitDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_SHOTGUN_HIT_DISABLED;
	m_GrenadeHitDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_GRENADE_HIT_DISABLED;
	m_LaserHitDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_LASER_HIT_DISABLED;
	m_HookHitDisabled = pObjDDNet->m_Flags & CHARACTERFLAG_HOOK_HIT_DISABLED;
	m_Super = pObjDDNet->m_Flags & CHARACTERFLAG_SUPER;
	m_Invincible = pObjDDNet->m_Flags & CHARACTERFLAG_INVINCIBLE;

	// Endless
	m_EndlessHook = pObjDDNet->m_Flags & CHARACTERFLAG_ENDLESS_HOOK;
	m_EndlessJump = pObjDDNet->m_Flags & CHARACTERFLAG_ENDLESS_JUMP;

	// Freeze
	m_FreezeEnd = pObjDDNet->m_FreezeEnd;
	m_DeepFrozen = pObjDDNet->m_FreezeEnd == -1;
	m_LiveFrozen = (pObjDDNet->m_Flags & CHARACTERFLAG_MOVEMENTS_DISABLED) != 0;

	// Telegun
	m_HasTelegunGrenade = pObjDDNet->m_Flags & CHARACTERFLAG_TELEGUN_GRENADE;
	m_HasTelegunGun = pObjDDNet->m_Flags & CHARACTERFLAG_TELEGUN_GUN;
	m_HasTelegunLaser = pObjDDNet->m_Flags & CHARACTERFLAG_TELEGUN_LASER;

	// Weapons
	m_aWeapons[WEAPON_HAMMER].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_HAMMER) != 0;
	m_aWeapons[WEAPON_GUN].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_GUN) != 0;
	m_aWeapons[WEAPON_SHOTGUN].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_SHOTGUN) != 0;
	m_aWeapons[WEAPON_GRENADE].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_GRENADE) != 0;
	m_aWeapons[WEAPON_LASER].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_LASER) != 0;
	m_aWeapons[WEAPON_NINJA].m_Got = (pObjDDNet->m_Flags & CHARACTERFLAG_WEAPON_NINJA) != 0;

	// Available jumps
	m_Jumps = pObjDDNet->m_Jumps;

	// Display Information
	// We only accept the display information when it is received, which means it is not -1 in each case.
	if(pObjDDNet->m_JumpedTotal != -1)
	{
		m_JumpedTotal = pObjDDNet->m_JumpedTotal;
	}
	if(pObjDDNet->m_NinjaActivationTick != -1)
	{
		m_Ninja.m_ActivationTick = pObjDDNet->m_NinjaActivationTick;
	}
	if(pObjDDNet->m_FreezeStart != -1)
	{
		m_FreezeStart = pObjDDNet->m_FreezeStart;
		m_IsInFreeze = pObjDDNet->m_Flags & CHARACTERFLAG_IN_FREEZE;
	}
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}

void CCharacterCore::SetHookedPlayer(int HookedPlayer)
{
	if(HookedPlayer != m_HookedPlayer)
	{
		if(m_HookedPlayer != -1 && m_Id != -1 && m_pWorld)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
			if(pCharCore)
			{
				pCharCore->m_AttachedPlayers.erase(m_Id);
			}
		}
		if(HookedPlayer != -1 && m_Id != -1 && m_pWorld)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[HookedPlayer];
			if(pCharCore)
			{
				pCharCore->m_AttachedPlayers.insert(m_Id);
			}
		}
		m_HookedPlayer = HookedPlayer;
	}
}

// DDRace

void CCharacterCore::SetTeamsCore(CTeamsCore *pTeams)
{
	m_pTeams = pTeams;
}

bool CCharacterCore::IsSwitchActiveCb(int Number, void *pUser)
{
	CCharacterCore *pThis = (CCharacterCore *)pUser;
	if(pThis->m_pWorld && !pThis->m_pWorld->m_vSwitchers.empty())
		if(pThis->m_Id != -1 && pThis->m_pTeams->Team(pThis->m_Id) != (pThis->m_pTeams->m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER))
			return pThis->m_pWorld->m_vSwitchers[Number].m_aStatus[pThis->m_pTeams->Team(pThis->m_Id)];
	return false;
}

void CWorldCore::InitSwitchers(int HighestSwitchNumber)
{
	if(HighestSwitchNumber > 0)
		m_vSwitchers.resize(HighestSwitchNumber + 1);
	else
		m_vSwitchers.clear();

	for(auto &Switcher : m_vSwitchers)
	{
		Switcher.m_Initial = true;
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			Switcher.m_aStatus[j] = true;
			Switcher.m_aEndTick[j] = 0;
			Switcher.m_aType[j] = 0;
			Switcher.m_aLastUpdateTick[j] = 0;
		}
	}
}
