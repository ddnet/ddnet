/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "chat.h"
#include "emoticon.h"

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <game/client/gameclient.h>
#include "bindwheel.h"

CBindWheel::CBindWheel()
{
	OnReset();
}

void CBindWheel::ConBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pSelf = (CBindWheel *)pUserData;
	if(!pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CBindWheel::ConBind(IConsole::IResult *pResult, void *pUserData)
{
	int bindpos = pResult->GetInteger(0);
	char command[MAX_BINDWHEEL_CMD];
	char description[MAX_BINDWHEEL_DESC];
	str_format(description, sizeof(description), pResult->GetString(1));
	str_format(command, sizeof(command), pResult->GetString(2));

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->updateBinds(bindpos, description, command);

}

void CBindWheel::ConchainBindwheel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 3)
	{
		int bindpos = pResult->GetInteger(0);
		char command[MAX_BINDWHEEL_CMD];
		char description[MAX_BINDWHEEL_DESC];
		str_format(description, sizeof(description), pResult->GetString(1));
		str_format(command, sizeof(command), pResult->GetString(2));

		CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
		//pThis->updateBinds(bindpos, description, command);
	
	}
}
void CBindWheel::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterTCallback(ConfigSaveCallback, this);


	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConBindwheel, this, "Open bindwheel selector");
	Console()->Register("bindwheel", "i[bindwheel] s[description:128] s[command:10]", CFGFLAG_CLIENT, ConBind, this, "Edit the command");
	Console()->Chain("bindwheel", ConchainBindwheel, this);

	
	for(int i = 0; i < NUM_BINDWHEEL; i++)
	{
		if(!(str_comp(m_BindWheelList[i].description, "EMPTY") == 0))
		{
			str_format(m_BindWheelList[i].description, sizeof(m_BindWheelList[i].description), "EMPTY");
		}

		if(str_comp(m_BindWheelList[i].command, "") == 0)
		{
			str_format(m_BindWheelList[i].command, sizeof(m_BindWheelList[i].command), "");
		}
	}
	
	
}

void CBindWheel::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedBind = -1;
}

void CBindWheel::OnRelease()
{
	m_Active = false;
}

bool CBindWheel::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	UI()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

void CBindWheel::DrawCircle(float x, float y, float r, int Segments)
{
	Graphics()->DrawCircle(x, y, r, Segments);
}

void CBindWheel::OnRender()
{
	if(!m_Active)
	{
		if(m_WasActive && m_SelectedBind != -1)
			Binwheel(m_SelectedBind);
		m_WasActive = false;
		return;
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	float SelectedAngle = angle(m_SelectorMouse) + 2 * pi / 24;
	if(SelectedAngle < 0)
		SelectedAngle += 2 * pi;

	m_SelectedBind = -1;
	if(length(m_SelectorMouse) > 110.0f)
		m_SelectedBind = (int)(SelectedAngle / (2 * pi) * NUM_BINDWHEEL);

	CUIRect Screen = *UI()->Screen();

	UI()->MapScreen();

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.3f);
	DrawCircle(Screen.w / 2, Screen.h / 2, 190.0f, 64);
	Graphics()->QuadsEnd();

	Graphics()->WrapClamp();
	for(int i = 0; i < NUM_BINDWHEEL; i++)
	{
		float Angle = 2 * pi * i / NUM_BINDWHEEL;
		float margin = 140.0f;

		if(Angle > pi)
        {
            Angle -= 2 * pi;
        }
		bool Selected = m_SelectedBind == i;

		float Size = Selected ? 14.0 : 12.0f;

		float NudgeX = margin * cosf(Angle);
		float NudgeY = 150.0f * sinf(Angle);

		char aBuf[MAX_BINDWHEEL_DESC];
		str_format(aBuf, sizeof(aBuf), "%s", m_BindWheelList[i].description);
		//str_format(aBuf, sizeof(aBuf), "%d -> %d", inv, orgAngle);
		TextRender()->Text(0, Screen.w / 2 + NudgeX - TextRender()->TextWidth(0, Size, aBuf, -1, -1.0f)*0.5, Screen.h / 2 + NudgeY, Size, aBuf, -1.0f);
	}
	Graphics()->WrapNormal();

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0, 1.0, 1.0, 0.3f);
		DrawCircle(Screen.w / 2, Screen.h / 2, 100.0f, 64);
		Graphics()->QuadsEnd();

		CTeeRenderInfo *pTeeInfo = &m_pClient->m_aClients[m_pClient->m_aLocalIDs[g_Config.m_ClDummy]].m_RenderInfo;

	
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.3f);
		DrawCircle(Screen.w / 2, Screen.h / 2, 30.0f, 64);
		Graphics()->QuadsEnd();
	}

	RenderTools()->RenderCursor(m_SelectorMouse + vec2(Screen.w, Screen.h) / 2, 24.0f);
}

void CBindWheel::Binwheel(int Bind)
{
	//bindwheel 0 "123456789" "say hey"
	char *command = m_BindWheelList[Bind].command;
	char aBuf[256] = {};
	Console()->ExecuteLine(m_BindWheelList[Bind].command);
}

void CBindWheel::updateBinds(int Bindpos, char *Description, char *Command)
{
	str_format(m_BindWheelList[Bindpos].command, sizeof(m_BindWheelList[Bindpos].command), Command);
	str_format(m_BindWheelList[Bindpos].description, sizeof(m_BindWheelList[Bindpos].description), Description);
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "[%d]: '%s' -> '%s'", Bindpos, m_BindWheelList[Bindpos].description, m_BindWheelList[Bindpos].command);
	
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "updateBinds", aBuf);
}

void CBindWheel::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindWheel *pSelf = (CBindWheel *)pUserData;

	char aBuf[128] = {};
	const char *pEnd = aBuf + sizeof(aBuf) - 4;
	for(int i = 0; i < NUM_BINDWHEEL ; ++i)
	{
		char *command = pSelf->m_BindWheelList[i].command;
		char *description = pSelf->m_BindWheelList[i].description;
		str_format(aBuf, sizeof(aBuf), "bindwheel %d \"%s\" \"%s\"",i,description,command);

		pConfigManager->WriteLine(aBuf);
	}
}