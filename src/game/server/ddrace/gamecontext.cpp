/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "gamecontext.h"

CGameContextDDRace::CGameContextDDRace(int Resetting) :
	CGameContext(Resetting)
{
}

void CGameContextDDRace::ResetContext()
{
	this->~CGameContextDDRace();
	mem_zero(this, sizeof(*this));
	new(this) CGameContextDDRace(RESET);
}

CGameContextDDRace::CGameContextDDRace() :
	CGameContext()
{
}

IGameServer *CreateGameServer() { return new CGameContextDDRace; }
