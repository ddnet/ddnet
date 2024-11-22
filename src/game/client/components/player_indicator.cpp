#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "player_indicator.h"

static vec2 NormalizedDirection(vec2 Src, vec2 Dst)
{
	return normalize(vec2(Dst.x - Src.x, Dst.y - Src.y));
}

static float DistanceBetweenTwoPoints(vec2 Src, vec2 Dst)
{
	return std::sqrt(std::pow(Dst.x - Src.x, 2.0f) + std::pow(Dst.y - Src.y, 2.0f));
}

void CPlayerIndicator::OnRender()
{
	// Don't render if we can't find our own tee
	if(m_pClient->m_Snap.m_LocalClientId == -1 || !m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientId].m_Active)
		return;

	// Don't render if not race gamemode or in demo
	if(!GameClient()->m_GameInfo.m_Race || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	vec2 Position = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_RenderPos;

	if(g_Config.m_ClPlayerIndicator != 1)
		return;

	Graphics()->TextureClear();
	ColorRGBA Col = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
	if(!(m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientId) == 0 && g_Config.m_ClIndicatorTeamOnly))
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(!m_pClient->m_Snap.m_apPlayerInfos[i] || i == m_pClient->m_Snap.m_LocalClientId)
				continue;

			CGameClient::CClientData OtherTee = m_pClient->m_aClients[i];
			CCharacterCore *pOtherCharacter = &m_pClient->m_aClients[i].m_Predicted;
			if(
				OtherTee.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_Team &&
				!OtherTee.m_Spec &&
				m_pClient->m_Snap.m_aCharacters[i].m_Active)
			{
				if(g_Config.m_ClPlayerIndicatorFreeze && !(OtherTee.m_FreezeEnd > 0 || OtherTee.m_DeepFrozen))
					continue;

				vec2 Norm = NormalizedDirection(m_pClient->m_aClients[i].m_RenderPos, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_RenderPos) * (-1);

				float Offset = g_Config.m_ClIndicatorOffset;
				if(g_Config.m_ClIndicatorVariableDistance)
				{
					Offset = mix((float)g_Config.m_ClIndicatorOffset, (float)g_Config.m_ClIndicatorOffsetMax,
						std::min(DistanceBetweenTwoPoints(Position, OtherTee.m_RenderPos) / g_Config.m_ClIndicatorMaxDistance, 1.0f));
				}

				vec2 IndicatorPos(Norm.x * Offset + Position.x, Norm.y * Offset + Position.y);
				CTeeRenderInfo TeeInfo = OtherTee.m_RenderInfo;
				float Alpha = g_Config.m_ClIndicatorOpacity / 100.0f;
				if(OtherTee.m_FreezeEnd > 0 || OtherTee.m_DeepFrozen)
				{
					// check if player is frozen or is getting saved
					if(pOtherCharacter->m_IsInFreeze == 0)
					{
						// player is on the way to get free again
						Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorSaved));
					}
					else
					{
						// player is frozen
						Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorFreeze));
					}
					if(g_Config.m_ClIndicatorTees)
					{
						TeeInfo.m_ColorBody.r *= 0.4f;
						TeeInfo.m_ColorBody.g *= 0.4f;
						TeeInfo.m_ColorBody.b *= 0.4f;
						TeeInfo.m_ColorFeet.r *= 0.4f;
						TeeInfo.m_ColorFeet.g *= 0.4f;
						TeeInfo.m_ColorFeet.b *= 0.4f;
						Alpha *= 0.8f;
					}
				}
				else
				{
					Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClIndicatorAlive));
				}
				Col.a = Alpha;

				TeeInfo.m_Size = g_Config.m_ClIndicatorRadius * 4.0f;

				if(g_Config.m_ClIndicatorTees)
				{
					RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, OtherTee.m_RenderCur.m_Emote, vec2(1.0f, 0.0f), IndicatorPos, Col.a);
				}
				else
				{
					Graphics()->QuadsBegin();
					Graphics()->SetColor(Col);
					Graphics()->DrawCircle(IndicatorPos.x, IndicatorPos.y, g_Config.m_ClIndicatorRadius, 16);
					Graphics()->QuadsEnd();
				}
			}
		}
	}
}
