#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "../chat.h"
#include "../emoticon.h"

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "bindwheel.h"
#include <game/client/gameclient.h>

CBindwheel::CBindwheel()
{
	OnReset();
}

void CBindwheel::ConBindwheelExecuteHover(IConsole::IResult *pResult, void *pUserData)
{
	CBindwheel *pThis = (CBindwheel *)pUserData;
	pThis->ExecuteHoveredBind();
}

void CBindwheel::ConOpenBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	CBindwheel *pThis = (CBindwheel *)pUserData;
	if(pThis->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		pThis->m_Active = pResult->GetInteger(0) != 0;
}

void CBindwheel::ConAddBindwheelLegacy(IConsole::IResult *pResult, void *pUserData)
{
	int BindPos = pResult->GetInteger(0);
	if (BindPos < 0 || BindPos >= BINDWHEEL_MAX_BINDS)
		return;

	const char *aName = pResult->GetString(1);
	const char *aCommand = pResult->GetString(2);

	CBindwheel *pThis = static_cast<CBindwheel *>(pUserData);
	while(pThis->m_vBinds.size() <= (size_t)BindPos)
		pThis->AddBind("EMPTY", "");

	str_copy(pThis->m_vBinds[BindPos].m_aName, aName);
	str_copy(pThis->m_vBinds[BindPos].m_aCommand, aCommand);
}

void CBindwheel::ConAddBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CBindwheel *pThis = static_cast<CBindwheel *>(pUserData);
	pThis->AddBind(aName, aCommand);
}

void CBindwheel::ConRemoveBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CBindwheel *pThis = static_cast<CBindwheel *>(pUserData);
	pThis->RemoveBind(aName, aCommand);
}

void CBindwheel::ConRemoveAllBindwheelBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBindwheel *pThis = static_cast<CBindwheel *>(pUserData);
	pThis->RemoveAllBinds();
}

void CBindwheel::AddBind(const char *pName, const char *pCommand)
{
	if((pName[0] == '\0' && pCommand[0] == '\0') || m_vBinds.size() >= BINDWHEEL_MAX_BINDS)
		return;

	CBind Bind;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	m_vBinds.push_back(Bind);
}

void CBindwheel::RemoveBind(const char *pName, const char *pCommand)
{
	CBind Bind;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	auto it = std::find(m_vBinds.begin(), m_vBinds.end(), Bind);
	if(it != m_vBinds.end())
		m_vBinds.erase(it);
}

void CBindwheel::RemoveBind(int Index)
{
	if(Index >= static_cast<int>(m_vBinds.size()) || Index < 0)
		return;
	auto Pos = m_vBinds.begin() + Index;
	m_vBinds.erase(Pos);
}

void CBindwheel::RemoveAllBinds()
{
	m_vBinds.clear();
}

void CBindwheel::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterTCallback(ConfigSaveCallback, this);

	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConOpenBindwheel, this, "Open bindwheel selector");
	Console()->Register("+bindwheel_execute_hover", "", CFGFLAG_CLIENT, ConBindwheelExecuteHover, this, "Execute hovered bindwheel bind");

	Console()->Register("bindwheel", "i[index] s[name] s[command]", CFGFLAG_CLIENT, ConAddBindwheelLegacy, this, "DONT USE THIS! USE add_bindwheel INSTEAD!");
	Console()->Register("add_bindwheel", "s[name] s[command]", CFGFLAG_CLIENT, ConAddBindwheel, this, "Add a bind to the bindwheel");
	Console()->Register("remove_bindwheel", "s[name] s[command]", CFGFLAG_CLIENT, ConRemoveBindwheel, this, "Remove a bind from the bindwheel");
	Console()->Register("delete_all_bindwheel_binds", "", CFGFLAG_CLIENT, ConRemoveAllBindwheelBinds, this, "Removes all bindwheel binds");
}

void CBindwheel::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedBind = -1;
}

void CBindwheel::OnRelease()
{
	m_Active = false;
}

bool CBindwheel::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

void CBindwheel::DrawCircle(float x, float y, float r, int Segments)
{
	Graphics()->DrawCircle(x, y, r, Segments);
}

void CBindwheel::OnRender()
{
	if(!m_Active)
	{
		if(g_Config.m_ClResetBindWheelMouse)
			m_SelectorMouse = vec2(0.0f, 0.0f);
		if(m_WasActive && m_SelectedBind != -1)
			ExecuteBind(m_SelectedBind);
		m_WasActive = false;
		return;
	}

	m_WasActive = true;

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	int SegmentCount = m_vBinds.size();
	float SegmentAngle = 2.0f * pi / SegmentCount;

	float SelectedAngle = angle(m_SelectorMouse) + SegmentAngle / 2.0f;
	if(SelectedAngle < 0.0f)
		SelectedAngle += 2.0f * pi;
	if(length(m_SelectorMouse) > 110.0f)
		m_SelectedBind = (int)(SelectedAngle / (2.0f * pi) * SegmentCount);
	else
		m_SelectedBind = -1;

	CUIRect Screen = *Ui()->Screen();

	Ui()->MapScreen();

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, 190.0f, 64);
	Graphics()->QuadsEnd();
	Graphics()->WrapClamp();

	const float Theta = pi * 2.0f / m_vBinds.size();
	for(int i = 0; i < static_cast<int>(m_vBinds.size()); i++)
	{
		const CBind &Bind = m_vBinds[i];
		const float Angle = Theta * i;
		vec2 Pos = direction(Angle);
		Pos *= 140.0f;
		const float FontSize = (i == m_SelectedBind) ? 20.0f : 12.0f;
		float Width = TextRender()->TextWidth(FontSize, Bind.m_aName);
		TextRender()->Text(Screen.w / 2.0f + Pos.x - Width / 2.0f, Screen.h / 2.0f + Pos.y - FontSize / 2.0f, FontSize, Bind.m_aName);
	}

	Graphics()->WrapNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.3f);
	DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, 100.0f, 64);
	Graphics()->QuadsEnd();

	RenderTools()->RenderCursor(m_SelectorMouse + vec2(Screen.w, Screen.h) / 2.0f, 24.0f);
}

void CBindwheel::ExecuteBind(int Bind)
{
	Console()->ExecuteLine(m_vBinds[Bind].m_aCommand);
}
void CBindwheel::ExecuteHoveredBind()
{
	if(m_SelectedBind >= 0)
		Console()->ExecuteLine(m_vBinds[m_SelectedBind].m_aCommand);
}

void CBindwheel::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindwheel *pThis = (CBindwheel *)pUserData;

	for(CBind &Bind : pThis->m_vBinds)
	{
		char aBuf[BINDWHEEL_MAX_CMD * 2] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_bindwheel \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aName, pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aCommand, pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf);
	}
}
