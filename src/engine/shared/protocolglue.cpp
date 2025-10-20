#include "protocolglue.h"

#include <base/system.h>

#include <engine/shared/network.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>
#include <generated/protocolglue.h>

namespace protocol7
{
	enum
	{
		NET_PACKETFLAG_CONTROL = 1 << 0,
		NET_PACKETFLAG_RESEND = 1 << 1,
		NET_PACKETFLAG_COMPRESSION = 1 << 2,
	};

}

int PacketFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags & ::NET_PACKETFLAG_CONTROL)
		Seven |= protocol7::NET_PACKETFLAG_CONTROL;
	if(Flags & ::NET_PACKETFLAG_RESEND)
		Seven |= protocol7::NET_PACKETFLAG_RESEND;
	if(Flags & ::NET_PACKETFLAG_COMPRESSION)
		Seven |= protocol7::NET_PACKETFLAG_COMPRESSION;
	return Seven;
}

int PacketFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::NET_PACKETFLAG_CONTROL)
		Six |= ::NET_PACKETFLAG_CONTROL;
	if(Flags & protocol7::NET_PACKETFLAG_RESEND)
		Six |= ::NET_PACKETFLAG_RESEND;
	if(Flags & protocol7::NET_PACKETFLAG_COMPRESSION)
		Six |= ::NET_PACKETFLAG_COMPRESSION;
	return Six;
}

int GameFlags_ClampToSix(int Flags)
{
	int Six = 0;
	if(Flags & GAMEFLAG_TEAMS)
		Six |= GAMEFLAG_TEAMS;
	if(Flags & GAMEFLAG_FLAGS)
		Six |= GAMEFLAG_FLAGS;
	return Six;
}

int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags & protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;
	if(Flags & protocol7::PLAYERFLAG_AIM)
		Six |= PLAYERFLAG_AIM;
	return Six;
}

int PlayerFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags & PLAYERFLAG_CHATTING)
		Seven |= protocol7::PLAYERFLAG_CHATTING;
	if(Flags & PLAYERFLAG_SCOREBOARD)
		Seven |= protocol7::PLAYERFLAG_SCOREBOARD;
	if(Flags & PLAYERFLAG_AIM)
		Seven |= protocol7::PLAYERFLAG_AIM;
	return Seven;
}

void PickupType_SevenToSix(int Type7, int &Type6, int &SubType6)
{
	SubType6 = 0;
	Type6 = POWERUP_WEAPON;
	switch(Type7)
	{
	case protocol7::PICKUP_HEALTH:
	case protocol7::PICKUP_ARMOR:
		Type6 = Type7;
		break;
	case protocol7::PICKUP_GRENADE:
		SubType6 = WEAPON_GRENADE;
		break;
	case protocol7::PICKUP_SHOTGUN:
		SubType6 = WEAPON_SHOTGUN;
		break;
	case protocol7::PICKUP_LASER:
		SubType6 = WEAPON_LASER;
		break;
	case protocol7::PICKUP_GUN:
		SubType6 = WEAPON_GUN;
		break;
	case protocol7::PICKUP_HAMMER:
		SubType6 = WEAPON_HAMMER;
		break;
	case protocol7::PICKUP_NINJA:
		SubType6 = WEAPON_NINJA;
		Type6 = POWERUP_NINJA;
		break;
	default:
		// dbg_msg("sixup", "ERROR: failed to translate weapon=%d to 0.6", Type7);
		break;
	}
}

int PickupType_SixToSeven(int Type6, int SubType6)
{
	switch(Type6)
	{
	case POWERUP_WEAPON:
		switch(SubType6)
		{
		case WEAPON_HAMMER: return protocol7::PICKUP_HAMMER;
		case WEAPON_GUN: return protocol7::PICKUP_GUN;
		case WEAPON_SHOTGUN: return protocol7::PICKUP_SHOTGUN;
		case WEAPON_GRENADE: return protocol7::PICKUP_GRENADE;
		case WEAPON_LASER: return protocol7::PICKUP_LASER;
		case WEAPON_NINJA: return protocol7::PICKUP_NINJA;
		default:
			dbg_assert_failed("invalid subtype %d", SubType6);
		}
	case POWERUP_NINJA: return protocol7::PICKUP_NINJA;
	case POWERUP_HEALTH: return protocol7::PICKUP_HEALTH;
	case POWERUP_ARMOR:
	case POWERUP_ARMOR_SHOTGUN:
	case POWERUP_ARMOR_GRENADE:
	case POWERUP_ARMOR_NINJA:
	case POWERUP_ARMOR_LASER:
		return protocol7::PICKUP_ARMOR;
	default:
		dbg_assert_failed("invalid type %d", Type6);
	}
}
