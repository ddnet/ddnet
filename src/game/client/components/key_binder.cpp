/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "key_binder.h"

#include <base/color.h>

#include <game/client/components/binds.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

using namespace FontIcons;

bool CKeyBinder::OnInput(const IInput::CEvent &Event)
{
	if(!m_TakeKey)
	{
		return false;
	}

	if(Event.m_Flags & IInput::FLAG_RELEASE)
	{
		int ModifierCombination = CBinds::GetModifierMask(Input());
		if(ModifierCombination == CBinds::GetModifierMaskOfKey(Event.m_Key))
		{
			ModifierCombination = KeyModifier::NONE;
		}
		m_Key = {Event.m_Key, ModifierCombination};
		m_TakeKey = false;
	}
	return true;
}

CKeyBinder::CKeyReaderResult CKeyBinder::DoKeyReader(CButtonContainer *pReaderButton, CButtonContainer *pClearButton, const CUIRect *pRect, const CBindSlot &CurrentBind, bool Activate)
{
	CKeyReaderResult Result = {CurrentBind, false};

	CUIRect KeyReaderButton, ClearButton;
	pRect->VSplitRight(pRect->h, &KeyReaderButton, &ClearButton);

	const int ClearButtonResult = Ui()->DoButton_FontIcon(
		pClearButton, FONT_ICON_TRASH,
		Result.m_Bind == CBindSlot(KEY_UNKNOWN, KeyModifier::NONE) ? 1 : 0,
		&ClearButton, BUTTONFLAG_LEFT, IGraphics::CORNER_R);

	const int ButtonResult = Ui()->DoButtonLogic(pReaderButton, 0, &KeyReaderButton, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
	if(ButtonResult == 1 || Activate)
	{
		m_pKeyReaderId = pReaderButton;
		m_TakeKey = true;
		m_Key = std::nullopt;
	}
	else if(ButtonResult == 2 || ClearButtonResult != 0)
	{
		Result.m_Bind = CBindSlot(KEY_UNKNOWN, KeyModifier::NONE);
	}

	if(m_pKeyReaderId == pReaderButton && m_Key.has_value())
	{
		if(m_Key.value().m_Key == KEY_ESCAPE)
		{
			Result.m_Aborted = true;
		}
		else
		{
			Result.m_Bind = m_Key.value();
		}
		m_pKeyReaderId = nullptr;
		m_Key = std::nullopt;
		Ui()->SetActiveItem(nullptr);
	}

	char aBuf[64];
	if(m_pKeyReaderId == pReaderButton && m_TakeKey)
	{
		str_copy(aBuf, Localize("Press a key…"));
	}
	else if(Result.m_Bind.m_Key == KEY_UNKNOWN)
	{
		aBuf[0] = '\0';
	}
	else
	{
		GameClient()->m_Binds.GetKeyBindName(Result.m_Bind.m_Key, Result.m_Bind.m_ModifierMask, aBuf, sizeof(aBuf));
	}

	const ColorRGBA Color = m_pKeyReaderId == pReaderButton && m_TakeKey ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.4f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * Ui()->ButtonColorMul(pReaderButton));
	KeyReaderButton.Draw(Color, IGraphics::CORNER_L, 5.0f);
	CUIRect Label;
	KeyReaderButton.HMargin(1.0f, &Label);
	Ui()->DoLabel(&Label, aBuf, Label.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	return Result;
}

bool CKeyBinder::IsActive() const
{
	return m_TakeKey;
}
