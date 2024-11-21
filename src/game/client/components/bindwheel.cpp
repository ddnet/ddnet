#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "chat.h"
#include "emoticon.h"

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "bindwheel.h"
#include <game/client/gameclient.h>

CBindWheel::CBindWheel()
{
	OnReset();
}

void CBindWheel::ConOpenBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pSelf = (CBindWheel *)pUserData;
	if(pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CBindWheel::ConAddBindwheel_Legacy(IConsole::IResult *pResult, void *pUserData)
{
	int Bindpos_Legacy = pResult->GetInteger(0);
	const char *aName = pResult->GetString(1);
	const char *aCommand = pResult->GetString(2);

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->AddBind(aName, aCommand);
}

void CBindWheel::ConAddBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->AddBind(aName, aCommand);
}

void CBindWheel::ConRemoveBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->RemoveBind(aName, aCommand);
}

void CBindWheel::AddBind(const char *pName, const char *pCommand)
{
	if(pName[0] == 0 && pCommand[0] == 0 || m_vBinds.size() > MAX_BINDS)
		return;

	for(SBind &Bind : m_vBinds)
	{
		if(!str_comp(Bind.m_aName, pName) && !str_comp(Bind.m_aCommand, pCommand))
			return;
	}
	SBind TempBind;
	str_copy(TempBind.m_aName, pName);
	str_copy(TempBind.m_aCommand, pCommand);
	m_vBinds.push_back(TempBind);
}

void CBindWheel::RemoveBind(const char *pName, const char *pCommand)
{
	SBind TempBind;
	str_copy(TempBind.m_aName, pName);
	str_copy(TempBind.m_aCommand, pCommand);
	auto it = std::find(m_vBinds.begin(), m_vBinds.end(), TempBind);
	if(it != m_vBinds.end()) 
		m_vBinds.erase(it);
}

void CBindWheel::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterTCallback(ConfigSaveCallback, this);

	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConOpenBindwheel, this, "Open bindwheel selector");

	Console()->Register("bindwheel", "i[index] s[name] s[command]", CFGFLAG_CLIENT, ConAddBindwheel_Legacy, this, "USE add_bindwheel INSTEAD, DONT USE THIS!");
	Console()->Register("add_bindwheel", "s[name] s[command]", CFGFLAG_CLIENT, ConAddBindwheel, this, "Add a bind to the bindwheel");
	Console()->Register("remove_bindwheel", "s[name] s[command]", CFGFLAG_CLIENT, ConRemoveBindwheel, this, "Remove a bind from the bindwheel");
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

	Ui()->ConvertMouseMove(&x, &y, CursorType);
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
		if(g_Config.m_ClResetBindWheelMouse)
			m_SelectorMouse = vec2(0, 0);
		if(m_WasActive && m_SelectedBind != -1)
			Bindwheel(m_SelectedBind);
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
		m_SelectedBind = (int)(SelectedAngle / (2 * pi) * m_vBinds.size());

	CUIRect Screen = *Ui()->Screen();

	Ui()->MapScreen();

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.3f);
	DrawCircle(Screen.w / 2, Screen.h / 2, 190.0f, 64);
	Graphics()->QuadsEnd();
	Graphics()->WrapClamp();

	const float Theta = pi * 2 / m_vBinds.size();
	for(int i = 0; i < static_cast<int>(m_vBinds.size()); i++)
	{
		const SBind &Bind = m_vBinds[i];
		const float Angle = Theta * i;
		vec2 Pos = vec2(std::cosf(Angle), std::sinf(Angle));
		Pos *= 140.0f;
		const float FontSize = (i == m_SelectedBind) ? 20.0f : 12.0f;
		float Width = TextRender()->TextWidth(FontSize, Bind.m_aName);
		TextRender()->Text(Screen.w / 2.0f + Pos.x - Width / 2.0f, Screen.h / 2.0f + Pos.y - FontSize / 2.0f, FontSize, Bind.m_aName);
	}

	Graphics()->WrapNormal();

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0, 1.0, 1.0, 0.3f);
		DrawCircle(Screen.w / 2, Screen.h / 2, 100.0f, 64);
		Graphics()->QuadsEnd();

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.3f);
		DrawCircle(Screen.w / 2, Screen.h / 2, 30.0f, 64);
		Graphics()->QuadsEnd();
	}

	RenderTools()->RenderCursor(m_SelectorMouse + vec2(Screen.w, Screen.h) / 2, 24.0f);
}

void CBindWheel::Bindwheel(int Bind)
{
	Console()->ExecuteLine(m_vBinds[Bind].m_aCommand);
}

void CBindWheel::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindWheel *pSelf = (CBindWheel *)pUserData;

	char aBuf[128] = {};
	for(SBind &Bind : pSelf->m_vBinds)
	{
		str_format(aBuf, sizeof(aBuf), "add_bindwheel \"%s\" \"%s\"", Bind.m_aName, Bind.m_aCommand);
		pConfigManager->WriteLine(aBuf);
	}
}
