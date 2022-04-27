#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "player_indicator.h"

vec2 NormalizedDirection(vec2 src, vec2 dst)
{
	return normalize(vec2(dst.x - src.x, dst.y - src.y));
}

void CPlayerIndicator::OnRender()
{
	//Don't Render if we can't find our own tee
	if(m_pClient->m_Snap.m_LocalClientID == -1 || !m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_Active)
		return;

	//Don't Render if not race gamemode or in demo
	if(!GameClient()->m_GameInfo.m_Race || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	vec2 Position = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_RenderPos;

	if(g_Config.m_ClPlayerIndicator == 1)
	{
		Graphics()->TextureClear();
		float CircleSize = 7.0f;
		ColorRGBA col = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
		if(!(m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientID) == 0 && g_Config.m_ClIndicatorTeamOnly))
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CGameClient::CClientData enemy = m_pClient->m_aClients[i];
				if(
					enemy.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team &&
					!enemy.m_Paused &&
					!enemy.m_Spec &&
					m_pClient->m_Snap.m_aCharacters[i].m_Active)
				{
					vec2 norm = NormalizedDirection(m_pClient->m_aClients[i].m_RenderPos, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_RenderPos) * (-1);

					vec2 cPoint(norm.x * g_Config.m_ClIndicatorOffset + Position.x, norm.y * g_Config.m_ClIndicatorOffset + Position.y);
					Graphics()->QuadsBegin();

					if(enemy.m_RenderCur.m_Weapon == WEAPON_NINJA)
					{
						col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorFreeze));
					}
					else
					{
						col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorAlive));
					}

					Graphics()->SetColor(col);

					if(g_Config.m_ClPlayerIndicatorFreeze)
					{
						if(enemy.m_RenderCur.m_Weapon == WEAPON_NINJA)
						{
							RenderTools()->DrawCircle(cPoint.x, cPoint.y, g_Config.m_ClIndicatorRadius, 16);
						}
					}
					else
					{
						RenderTools()->DrawCircle(cPoint.x, cPoint.y, g_Config.m_ClIndicatorRadius, 16);
					}

					Graphics()->QuadsEnd();
				}
			}
		}
		Graphics()->QuadsBegin();
		ColorRGBA rgb = color_cast<ColorRGBA>(ColorHSLA(100.0f / 1000.0f, 1.0f, 0.5f, 0.8f));

		Graphics()->SetColor(rgb);

		// RenderTools()->DrawCircle(InitPos.x, InitPos.y, 32, 360);
		Graphics()->QuadsEnd();
	}
}