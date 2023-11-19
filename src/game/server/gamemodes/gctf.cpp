#include "gctf.h"

CGameControllerGCTF::CGameControllerGCTF(class CGameContext *pGameServer) :
	CGameControllerCTF(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? "gCTF-test" : "gCTF";
}

CGameControllerGCTF::~CGameControllerGCTF() = default;

void CGameControllerGCTF::Tick()
{
	CGameControllerCTF::Tick();
}
