// ChillerDragon 2020 - chillerbot ux

#include <engine/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <game/client/render.h>
#include <game/client/race.h>
#include <game/generated/protocol.h>
#include <game/client/components/menus.h>

#include "chillerbotux.h"

void CChillerBotUX::OnTick()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClFinishRename)
		return;
	vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
	if(CRaceHelper::IsNearFinish(m_pClient, Pos))
	{
		if(Client()->State() == IClient::STATE_ONLINE && !m_pClient->m_pMenus->IsActive() && g_Config.m_ClEditor == 0)
		{
			Graphics()->BlendNormal();
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0,0,0,0.5f);
			RenderTools()->DrawRoundRect(10.0f, 30.0f, 150.0f, 50.0f, 10.0f);
			Graphics()->QuadsEnd();
			TextRender()->Text(0, 20.0f, 30.f, 20.0f, "chillerbot-ux", -1);
			TextRender()->Text(0, 50.0f, 60.f, 10.0f, "finish rename", -1);
		}
		if(!m_IsNearFinish)
		{
			m_IsNearFinish = true;
			m_pClient->SendFinishName();
		}
	}
	else
	{
		m_IsNearFinish = false;
	}
}
