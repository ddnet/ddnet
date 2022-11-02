#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "player_indicator.h"

vec2 NormalizedDirection(vec2 src, vec2 dst)
{
	return normalize(vec2(dst.x - src.x, dst.y - src.y));
}

float DistanceBetweenTwoPoints(vec2 src, vec2 dst)
{
	return sqrt(pow(dst.x - src.x, 2) + pow(dst.y - src.y, 2));
}

void CPlayerIndicator::OnRender()
{
	// Don't render if we can't find our own tee
	if(m_pClient->m_Snap.m_LocalClientID == -1 || !m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_Active)
		return;

	// Don't render if not race gamemode or in demo
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
				if(!m_pClient->m_Snap.m_apPlayerInfos[i] || i == m_pClient->m_Snap.m_LocalClientID)
					continue;

				CGameClient::CClientData OtherTee = m_pClient->m_aClients[i];
				if(
					OtherTee.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team &&
					!OtherTee.m_Spec &&
					m_pClient->m_Snap.m_aCharacters[i].m_Active)
				{
					if(g_Config.m_ClPlayerIndicatorFreeze && !(OtherTee.m_FreezeEnd > 0 || OtherTee.m_DeepFrozen))
						continue;

					vec2 norm = NormalizedDirection(m_pClient->m_aClients[i].m_RenderPos, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_RenderPos) * (-1);

					float Offset = g_Config.m_ClIndicatorOffset;
					if(g_Config.m_ClIndicatorVariableDistance)
					{
						Offset = mix((float)g_Config.m_ClIndicatorOffset, (float)g_Config.m_ClIndicatorOffsetMax,
							std::min(DistanceBetweenTwoPoints(Position, OtherTee.m_RenderPos) / g_Config.m_ClIndicatorMaxDistance, 1.0f));
					}

					vec2 IndicatorPos(norm.x * Offset + Position.x, norm.y * Offset + Position.y);
					CTeeRenderInfo TeeInfo = OtherTee.m_RenderInfo;
					float Alpha = g_Config.m_ClIndicatorOpacity / 100.0f;
					if(OtherTee.m_FreezeEnd > 0 || OtherTee.m_DeepFrozen)
					{
						col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorFreeze));
						if(g_Config.m_ClIndicatorTees)
						{
							TeeInfo.m_ColorBody.r *= 0.4;
							TeeInfo.m_ColorBody.g *= 0.4;
							TeeInfo.m_ColorBody.b *= 0.4;
							TeeInfo.m_ColorFeet.r *= 0.4;
							TeeInfo.m_ColorFeet.g *= 0.4;
							TeeInfo.m_ColorFeet.b *= 0.4;
							Alpha *= 0.8;
						}
					}
					else
					{
						col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorAlive));
					}
					col.a = Alpha;

					CAnimState *pIdleState = CAnimState::GetIdle();
					TeeInfo.m_Size = g_Config.m_ClIndicatorRadius * 4.f;

					if(g_Config.m_ClIndicatorTees)
					{
						RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, OtherTee.m_RenderCur.m_Emote, vec2(1.0f, 0.0f), IndicatorPos, col.a);
					}
					else
					{
						Graphics()->QuadsBegin();
						Graphics()->SetColor(col);
						Graphics()->DrawCircle(IndicatorPos.x, IndicatorPos.y, g_Config.m_ClIndicatorRadius, 16);
						Graphics()->QuadsEnd();
					}
				}
			}
		}
	}
}