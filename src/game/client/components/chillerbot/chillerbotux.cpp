// ChillerDragon 2020 - chillerbot ux

#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/version.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include "chillerbotux.h"

void CChillerBotUX::OnRender()
{
	if(time_get() % 10 == 0)
	{
		// if tabbing into tw and going afk set to inactive again over time
		if(m_AfkActivity && time_get() % 100 == 0)
			m_AfkActivity--;
	}
	if(m_AfkTill)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "(%d/%d)", m_AfkActivity, 200);
		SetComponentNoteShort("afk", aBuf);
	}
	RenderSpeedHud();
	RenderEnabledComponents();
	FinishRenameTick();
	m_ForceDir = 0;
	CampHackTick();
	if(!m_ForceDir && m_LastForceDir)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
	}
	m_LastForceDir = m_ForceDir;
}

void CChillerBotUX::RenderSpeedHud()
{
	if(!g_Config.m_ClShowSpeed || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);
	// float Velspeed = length(vec2(m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f, m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f)) * 50;

	const char *paStrings[] = {"Vel X:", "Vel Y:"};
	const int Num = sizeof(paStrings) / sizeof(char *);
	const float LineHeight = 12.0f;
	const float Fontsize = 15.0f;

	float x = Width - 100.0f, y = 50.0f;
	for(int i = 0; i < Num; ++i)
		TextRender()->Text(0, x, y + i * LineHeight, Fontsize, paStrings[i], -1.0f);

	x = Width - 10.0f;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%.0f", m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 32.f);
	float w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.0f", m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 32.f);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
}

void CChillerBotUX::RenderEnabledComponents()
{
	if(m_pClient->m_pMenus->IsActive())
		return;
	if(!g_Config.m_ClChillerbotHud)
		return;
	for(int i = 0; i < MAX_COMPONENTS_ENABLED; i++)
	{
		if(m_aEnabledComponents[i].m_aName[0] == '\0')
			continue;
		float TwName = TextRender()->TextWidth(0, 10.0f, m_aEnabledComponents[i].m_aName, -1, -1);
		float TwNoteShort = 2.f;
		if(m_aEnabledComponents[i].m_aNoteShort[0])
			TwNoteShort += TextRender()->TextWidth(0, 10.0f, m_aEnabledComponents[i].m_aNoteShort, -1, -1);
		Graphics()->BlendNormal();
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.5f);
		RenderTools()->DrawRoundRect(4.0f, 60.f + i * 15, TwName + TwNoteShort, 14.0f, 3.0f);
		Graphics()->QuadsEnd();
		TextRender()->Text(0, 5.0f, 60.f + i * 15, 10.0f, m_aEnabledComponents[i].m_aName, -1);
		TextRender()->Text(0, 5.0f + TwName + 2.f, 60.f + i * 15, 10.0f, m_aEnabledComponents[i].m_aNoteShort, -1);
		TextRender()->Text(0, 5.0f, 60.f + i * 15 + 10, 4.0f, m_aEnabledComponents[i].m_aNoteLong, -1);
	}
}

void CChillerBotUX::EnableComponent(const char *pComponent, const char *pNoteShort, const char *pNoteLong)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
	// insert
	for(auto &Component : m_aEnabledComponents)
	{
		if(Component.m_aName[0] != '\0')
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		Component.m_aNoteShort[0] = '\0';
		Component.m_aNoteLong[0] = '\0';
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
}

void CChillerBotUX::DisableComponent(const char *pComponent)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		Component.m_aName[0] = '\0';
		return;
	}
}

bool CChillerBotUX::SetComponentNoteShort(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteShort, pNote, sizeof(Component.m_aNoteShort));
		return true;
	}
	return false;
}

bool CChillerBotUX::SetComponentNoteLong(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteLong, pNote, sizeof(Component.m_aNoteLong));
		return true;
	}
	return false;
}

void CChillerBotUX::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX, pGroup->m_ParallaxY,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), Zoom, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CChillerBotUX::CampHackTick()
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClCampHack)
		return;
	if(Layers()->GameGroup())
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		MapScreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup(), m_pClient->m_pCamera->m_Zoom);
		Graphics()->BlendNormal();
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.4f);
		RenderTools()->DrawRoundRect(m_CampHackX1, m_CampHackY1, 20.0f, 20.0f, 3.0f);
		RenderTools()->DrawRoundRect(m_CampHackX2, m_CampHackY2, 20.0f, 20.0f, 3.0f);
		if(m_CampHackX1 && m_CampHackX2 && m_CampHackY1 && m_CampHackY2)
		{
			if(m_CampHackX1 < m_CampHackX2)
				Graphics()->SetColor(0, 1, 0, 0.2f);
			else
				Graphics()->SetColor(1, 0, 0, 0.2f);
			RenderTools()->DrawRoundRect(m_CampHackX1, m_CampHackY1, m_CampHackX2 - m_CampHackX1, m_CampHackY2 - m_CampHackY1, 3.0f);
		}
		Graphics()->QuadsEnd();
		TextRender()->Text(0, m_CampHackX1, m_CampHackY1, 10.0f, "1", -1);
		TextRender()->Text(0, m_CampHackX2, m_CampHackY2, 10.0f, "2", -1);
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
	if(!m_CampHackX1 || !m_CampHackX2 || !m_CampHackY1 || !m_CampHackY2)
		return;
	if(g_Config.m_ClCampHack < 2 || GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER)
		return;
	if(m_CampHackX1 > GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 1;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
		m_ForceDir = 1;
	}
	else if(m_CampHackX2 < GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 1;
		m_ForceDir = -1;
	}
}

bool CChillerBotUX::OnMouseMove(float x, float y)
{
	if(time_get() % 10 == 0)
		ReturnFromAfk();
	return false;
}

bool CChillerBotUX::OnInput(IInput::CEvent e)
{
	ReturnFromAfk();
	SelectCampArea(e.m_Key);
	return false;
}

void CChillerBotUX::SelectCampArea(int Key)
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(g_Config.m_ClCampHack != 1)
		return;
	if(Key != KEY_MOUSE_1)
		return;
	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_GUN)
		return;
	m_CampClick++;
	if(m_CampClick % 2 == 0)
	{
		// UNSET IF CLOSE
		vec2 Current = vec2(GameClient()->m_Snap.m_pLocalCharacter->m_X, GameClient()->m_Snap.m_pLocalCharacter->m_Y);
		vec2 CrackPos1 = vec2(m_CampHackX1, m_CampHackY1);
		float dist = distance(CrackPos1, Current);
		if(dist < 100.0f)
		{
			m_CampHackX1 = 0;
			m_CampHackY1 = 0;
			GameClient()->m_pChat->AddLine(-2, 0, "Unset camp[1]");
			return;
		}
		vec2 CrackPos2 = vec2(m_CampHackX2, m_CampHackY2);
		dist = distance(CrackPos2, Current);
		if(dist < 100.0f)
		{
			m_CampHackX2 = 0;
			m_CampHackY2 = 0;
			GameClient()->m_pChat->AddLine(-2, 0, "Unset camp[2]");
			return;
		}
		// SET OTHERWISE
		if(m_CampClick == 2)
		{
			m_CampHackX1 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY1 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		else
		{
			m_CampHackX2 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY2 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf),
			"Set camp[%d] %d",
			m_CampClick == 2 ? 1 : 2,
			GameClient()->m_Snap.m_pLocalCharacter->m_X / 32);
		GameClient()->m_pChat->AddLine(-2, 0, aBuf);
	}
	if(m_CampClick > 3)
		m_CampClick = 0;
}

void CChillerBotUX::FinishRenameTick()
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
			Graphics()->SetColor(0, 0, 0, 0.5f);
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

void CChillerBotUX::OnInit()
{
	m_pChatHelper = m_pClient->m_pChatHelper;

	m_AfkTill = 0;
	m_AfkActivity = 0;
	m_aAfkMessage[0] = '\0';

	for(auto &c : m_aEnabledComponents)
	{
		c.m_aName[0] = '\0';
		c.m_aNoteShort[0] = '\0';
		c.m_aNoteLong[0] = '\0';
	}
	UpdateComponents();
}

void CChillerBotUX::UpdateComponents()
{
	// TODO: make this auto or nicer
	if(g_Config.m_ClChillerbotHud)
		EnableComponent("chillerbot hud");
	else
		DisableComponent("chillerbot hud");
	if(g_Config.m_ClCampHack)
		EnableComponent("camp hack");
	else
		DisableComponent("camp hack");
	if(g_Config.m_ClAutoReply)
		EnableComponent("auto reply");
	else
		DisableComponent("auto reply");
	if(g_Config.m_ClFinishRename)
		EnableComponent("finish rename");
	else
		DisableComponent("finish rename");
}

void CChillerBotUX::OnConsoleInit()
{
	Console()->Register("afk", "?i[minutes]?r[message]", CFGFLAG_CLIENT, ConAfk, this, "Activate afk mode (auto chat respond)");
	Console()->Register("camp", "?i[left]i[right]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHack, this, "Activate camp mode relative to tee");
	Console()->Register("camp_abs", "i[x1]i[y1]i[x2]i[y2]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHackAbs, this, "Activate camp mode absolute in the map");
	Console()->Register("uncamp", "", CFGFLAG_CLIENT, ConUnCampHack, this, "Same as cl_camp_hack 0 but resets walk input");

	Console()->Chain("cl_camp_hack", ConchainCampHack, this);
	Console()->Chain("cl_chillerbot_hud", ConchainChillerbotHud, this);
	Console()->Chain("cl_auto_reply", ConchainAutoReply, this);
	Console()->Chain("cL_finish_rename", ConchainFinishRename, this);
}

void CChillerBotUX::ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("camp hack");
	else
		pSelf->DisableComponent("camp hack");
}

void CChillerBotUX::ConchainChillerbotHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("chillerbot hud");
	else
		pSelf->DisableComponent("chillerbot hud");
	pSelf->UpdateComponents();
}

void CChillerBotUX::ConchainAutoReply(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("auto reply");
	else
		pSelf->DisableComponent("auto reply");
}

void CChillerBotUX::ConchainFinishRename(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("finish rename");
	else
		pSelf->DisableComponent("finish rename");
}

void CChillerBotUX::ConAfk(IConsole::IResult *pResult, void *pUserData)
{
	((CChillerBotUX *)pUserData)->GoAfk(pResult->NumArguments() ? pResult->GetInteger(0) : -1, pResult->GetString(1));
}

void CChillerBotUX::ConCampHackAbs(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	// absolute all coords
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = Tile * pResult->GetInteger(1);
			pSelf->m_CampHackX2 = Tile * pResult->GetInteger(2);
			pSelf->m_CampHackY2 = Tile * pResult->GetInteger(3);
		}
		return;
	}
}

void CChillerBotUX::ConCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	if(!pResult->NumArguments())
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - 32 * 3;
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + 32 * 3;
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
	// relative left and right
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + Tile * pResult->GetInteger(1);
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
}

void CChillerBotUX::ConUnCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	g_Config.m_ClCampHack = 0;
	pSelf->DisableComponent("camp hack");
	pSelf->m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
	pSelf->m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
}

void CChillerBotUX::GoAfk(int Minutes, const char *pMsg)
{
	if(pMsg)
	{
		str_copy(m_aAfkMessage, pMsg, sizeof(m_aAfkMessage));
		if((unsigned long)str_length(pMsg) > sizeof(m_aAfkMessage))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "error: afk message too long %d/%lu", str_length(pMsg), sizeof(m_aAfkMessage));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
			return;
		}
	}
	m_AfkTill = time_get() + time_freq() * 60 * Minutes;
	m_AfkActivity = 0;
	m_pChatHelper->ClearLastAfkPingMessage();
	EnableComponent("afk");
}

void CChillerBotUX::ReturnFromAfk(const char *pChatMessage)
{
	if(!m_AfkTill)
		return;
	if(pChatMessage && pChatMessage[0] != '/')
	{
		if(m_IgnoreChatAfk > 0)
			m_IgnoreChatAfk--;
		else
			m_AfkActivity += 400;
	}
	m_AfkActivity++;
	if(m_AfkActivity < 200)
		return;
	m_pClient->m_pChat->AddLine(-2, 0, "[chillerbot-ux] welcome back :)");
	m_AfkTill = 0;
	DisableComponent("afk");
}
