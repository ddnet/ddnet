#include "ictf.h"

CGameControllerICTF::CGameControllerICTF(class CGameContext *pGameServer) :
	CGameControllerCTF(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? "iCTF-test" : "iCTF";
}

CGameControllerICTF::~CGameControllerICTF() = default;

void CGameControllerICTF::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	CGameControllerCTF::Tick();
}
