/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamemodes/DDRace.h>
#include "autofire.h"

#include "projectile.h"
#include "laser.h"

CAutofire::CAutofire
	(
		CGameWorld *pGameWorld,
		int Weapon,
		vec2 Position,
		vec2 Direction,
		int Delay
	)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Weapon = Weapon;
	m_Position = Position;
	m_Direction = Direction;
	m_Delay = Delay;

	m_Time = 0;	
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Position));

	GameWorld()->InsertEntity(this);
}

void CAutofire::Tick()
{
	m_Time++;
	if (m_Time >= m_Delay) {
		m_Time = 0;

		switch(m_Weapon)
		{
			case WEAPON_SHOTGUN:
			{
				float LaserReach;
				if (!m_TuneZone)
					LaserReach = GameServer()->Tuning()->m_LaserReach;
				else
					LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

				new CLaser(&GameServer()->m_World, m_Position, m_Direction, LaserReach, -1, WEAPON_SHOTGUN);
				GameServer()->CreateSound(m_Position, SOUND_SHOTGUN_FIRE, -1);
			} break;

			case WEAPON_GRENADE:
			{
				int Lifetime;
				if (!m_TuneZone)
					Lifetime = (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime);
				else
					Lifetime = (int)(Server()->TickSpeed()*GameServer()->TuningList()[m_TuneZone].m_GrenadeLifetime);

				CProjectile *pProj = new CProjectile
					(
					&GameServer()->m_World,
					WEAPON_GRENADE,
					-1,
					m_Position,
					m_Direction,
					Lifetime,
					0,
					true,
					0,
					SOUND_GRENADE_EXPLODE,
					WEAPON_GRENADE
					);

				CNetObj_Projectile p;
				pProj->FillInfo(&p);
				CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
				Msg.AddInt(1);
				for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
					Msg.AddInt(((int *)&p)[i]);
				Server()->SendMsg(&Msg, 0, -1);

				GameServer()->CreateSound(m_Position, SOUND_GRENADE_FIRE, -1);
			} break;

			case WEAPON_RIFLE:
			{
				float LaserReach;
				if (!m_TuneZone)
					LaserReach = GameServer()->Tuning()->m_LaserReach;
				else
					LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;
				
				new CLaser(&GameServer()->m_World, m_Position, m_Direction, LaserReach, -1, WEAPON_RIFLE);
				GameServer()->CreateSound(m_Position, SOUND_RIFLE_FIRE, -1);
			} break;
		}
	}
}
