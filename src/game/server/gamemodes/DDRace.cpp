/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "DDRace.h"
#include "gamemode.h"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
		IGameController(pGameServer), m_Teams(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_NAME : GAME_NAME;

	InitTeleporter();
}

CGameControllerDDRace::~CGameControllerDDRace()
{
	// Nothing to clean
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
}

void CGameControllerDDRace::InitTeleporter()
{
	if (!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for (int i = 0; i < Width * Height; i++)
	{
		if (GameServer()->Collision()->TeleLayer()[i].m_Number > 0)
		{
			if (GameServer()->Collision()->TeleLayer()[i].m_Type
					== TILE_TELEOUT)
			{
				m_TeleOuts[GameServer()->Collision()->TeleLayer()[i].m_Number
						- 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if (GameServer()->Collision()->TeleLayer()[i].m_Type
					== TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[GameServer()->Collision()->TeleLayer()[i].m_Number
						- 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
		}
	}
}
