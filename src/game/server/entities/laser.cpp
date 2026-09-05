/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser.h"

#include "character.h"

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/physics/laser.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/ddnet.h>
#include <game/server/player.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, true)
{
	m_Pos = Pos;
	m_Number = 0;
	m_Layer = LAYER_GAME;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_TelePos = vec2(0, 0);
	m_WasTele = false;
	m_Type = Type;
	m_TeleportCancelled = false;
	m_IsBlueTeleport = false;
	m_ZeroEnergyBounceInLastTick = false;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_BelongsToPracticeTeam = pOwnerChar && pOwnerChar->Teams()->IsPractice(pOwnerChar->Team());

	CPlayer *pOwnerPlayer = (Owner >= 0 && Owner < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Owner] : nullptr;
	m_InteractState.Init(Owner, pOwnerPlayer ? pOwnerPlayer->GetUniqueCid() : 0);
	SyncInteractState();
	GameWorld()->InsertEntity(this);
	DoBounce();
}

bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	SyncInteractState();
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit;
	bool pDontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);

	if(pOwnerChar ? (!pOwnerChar->LaserHitDisabled() && m_Type == WEAPON_LASER) || (!pOwnerChar->ShotgunHitDisabled() && m_Type == WEAPON_SHOTGUN) : g_Config.m_SvHit)
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : nullptr, m_Owner);
	else
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : nullptr, m_Owner, pOwnerChar);

	if(!pHit || !m_InteractState.CanHit(GameServer(), pHit->GetPlayer()->GetCid()))
		return false;
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if(m_Type == WEAPON_SHOTGUN)
	{
		CLaserPhysics<CLaser, CCharacter>::ShotgunKnockback(this, pHit, pOwnerChar);
	}
	else if(m_Type == WEAPON_LASER)
	{
		pHit->Unfreeze();
	}
	pHit->TakeDamage(vec2(0, 0), 0, m_Owner, m_Type);
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		m_MarkedForDestroy = true;
		return;
	}
	m_PrevPos = m_Pos;
	vec2 Coltile;

	int Res;
	int z;

	if(m_WasTele)
	{
		m_PrevPos = m_TelePos;
		m_Pos = m_TelePos;
		m_TelePos = vec2(0, 0);
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	Res = GameServer()->Collision()->IntersectLineTeleWeapon(m_Pos, To, &Coltile, &To, &z);

	if(Res)
	{
		if(!HitCharacter(m_Pos, To))
		{
			CLaserPhysics<CLaser, CCharacter>::Bounce(this, Res, Coltile, To);

			if(Res == TILE_TELEINWEAPON && !GameServer()->Collision()->TeleOuts(z - 1).empty())
			{
				int TeleOut = GameServer()->m_World.m_Core.RandomOr0(GameServer()->Collision()->TeleOuts(z - 1).size());
				m_TelePos = GameServer()->Collision()->TeleOuts(z - 1)[TeleOut];
				m_WasTele = true;
			}
			else
			{
				m_Bounces++;
				m_WasTele = false;
			}

			int BounceNum = TuningList()[m_TuneZone].m_LaserBounceNum;

			if(m_Bounces > BounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_InteractState.CanSeeMask(GameServer()));
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(m_Owner >= 0 && m_Energy <= 0 && !m_TeleportCancelled && pOwnerChar &&
		pOwnerChar->IsAlive() && pOwnerChar->HasTelegunLaser() && m_Type == WEAPON_LASER)
	{
		vec2 PossiblePos;
		bool Found = false;

		// Check if the laser hits a player.
		bool DontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);
		vec2 At;
		CCharacter *pHit;
		if(pOwnerChar ? (!pOwnerChar->LaserHitDisabled() && m_Type == WEAPON_LASER) : g_Config.m_SvHit)
			pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, DontHitSelf ? pOwnerChar : nullptr, m_Owner);
		else
			pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, DontHitSelf ? pOwnerChar : nullptr, m_Owner, pOwnerChar);

		if(pHit)
			Found = GetNearestAirPosPlayer(pHit->m_Pos, &PossiblePos);
		else
			Found = GetNearestAirPos(m_Pos, m_From, &PossiblePos);

		if(Found)
		{
			pOwnerChar->m_TeleGunPos = PossiblePos;
			pOwnerChar->m_TeleGunTeleport = true;
			pOwnerChar->m_IsBlueTeleGunTeleport = m_IsBlueTeleport;
		}
	}
	else if(m_Owner >= 0)
	{
		int MapIndex = GameServer()->Collision()->GetPureMapIndex(Coltile);
		int TileFIndex = GameServer()->Collision()->GetFrontTileIndex(MapIndex);
		bool IsSwitchTeleGun = GameServer()->Collision()->GetSwitchType(MapIndex) == TILE_ALLOW_TELE_GUN;
		bool IsBlueSwitchTeleGun = GameServer()->Collision()->GetSwitchType(MapIndex) == TILE_ALLOW_BLUE_TELE_GUN;
		int IsTeleInWeapon = GameServer()->Collision()->IsTeleportWeapon(MapIndex);

		if(!IsTeleInWeapon)
		{
			if(IsSwitchTeleGun || IsBlueSwitchTeleGun)
			{
				// Delay specifies which weapon the tile should work for.
				// Delay = 0 means all.
				const int Delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);

				if((Delay != 3 && Delay != 0) && m_Type == WEAPON_LASER)
				{
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
				}
			}

			m_IsBlueTeleport = TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsBlueSwitchTeleGun;

			// Teleport is canceled if the last bounce tile is not a TILE_ALLOW_TELE_GUN.
			// Teleport also works if laser didn't bounce.
			m_TeleportCancelled =
				m_Type == WEAPON_LASER && (TileFIndex != TILE_ALLOW_TELE_GUN && TileFIndex != TILE_ALLOW_BLUE_TELE_GUN && !IsSwitchTeleGun && !IsBlueSwitchTeleGun);
		}
	}
}

void CLaser::Reset()
{
	m_MarkedForDestroy = true;
}

void CLaser::Tick()
{
	SyncInteractState();
	if((g_Config.m_SvDestroyLasersOnDeath || m_BelongsToPracticeTeam) && m_Owner >= 0)
	{
		CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
		if(!(pOwnerChar && pOwnerChar->IsAlive()))
		{
			Reset();
		}
	}

	float Delay = TuningList()[m_TuneZone].m_LaserBounceDelay;
	if((Server()->Tick() - m_EvalTick) > (Server()->TickSpeed() * Delay / 1000.0f))
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if((NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_From)) || !GetId().has_value())
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT && !m_InteractState.CanSee(GameServer(), SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	int LaserType = m_Type == WEAPON_LASER ? LASERTYPE_RIFLE : (m_Type == WEAPON_SHOTGUN ? LASERTYPE_SHOTGUN : -1);

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient), GetId().value(),
		m_Pos, m_From, m_EvalTick, m_Owner, LaserType, 0, m_Number);
}

void CLaser::SwapClients(int Client1, int Client2)
{
	m_Owner = m_Owner == Client1 ? Client2 : (m_Owner == Client2 ? Client1 : m_Owner);
}

bool CLaser::OldLaser()
{
	return g_Config.m_SvOldLaser;
}

void CLaser::SyncInteractState()
{
	CPlayer *pOwnerPlayer = (m_Owner >= 0 && m_Owner < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Owner] : nullptr;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	// as long as the owner is connected
	// refill the state on tick
	// as soon as the owner disconnects keep that state
	if(pOwnerPlayer)
	{
		bool NoHitOthers = g_Config.m_SvHit;
		if(pOwnerChar)
			NoHitOthers = (m_Type == WEAPON_LASER && pOwnerChar->LaserHitDisabled()) || (m_Type == WEAPON_SHOTGUN && pOwnerChar->ShotgunHitDisabled());
		bool NoHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);
		m_InteractState.FillOwnerConnected(
			pOwnerChar && pOwnerChar->IsAlive(),
			pOwnerPlayer ? GameServer()->GetDDRaceTeam(m_Owner) : 0,
			pOwnerChar && pOwnerChar->Core()->m_Solo,
			NoHitOthers,
			NoHitSelf);
	}
	else
	{
		m_InteractState.FillOwnerDisconnected();
	}
}
