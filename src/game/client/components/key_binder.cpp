/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "key_binder.h"

#include <base/color.h>

#include <game/client/components/binds.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

CKeyBinder::CKeyBinder()
{
	m_pKeyReaderId = nullptr;
	m_TakeKey = false;
	m_GotKey = false;
	m_ModifierCombination = CBinds::MODIFIER_NONE;
}

bool CKeyBinder::OnInput(const IInput::CEvent &Event)
{
	if(!m_TakeKey)
	{
		return false;
	}

	int TriggeringEvent = (Event.m_Key == KEY_MOUSE_1) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
	if(Event.m_Flags & TriggeringEvent)
	{
		m_Key = Event;
		m_GotKey = true;
		m_TakeKey = false;

		m_ModifierCombination = CBinds::GetModifierMask(Input());
		if(m_ModifierCombination == CBinds::GetModifierMaskOfKey(Event.m_Key))
		{
			m_ModifierCombination = CBinds::MODIFIER_NONE;
		}
	}
	return true;
}

int CKeyBinder::DoKeyReader(const void *pId, const CUIRect *pRect, int Key, int ModifierCombination, int *pNewModifierCombination)
{
	int NewKey = Key;
	*pNewModifierCombination = ModifierCombination;

	const int ButtonResult = Ui()->DoButtonLogic(pId, 0, pRect, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
	if(ButtonResult == 1)
	{
		m_pKeyReaderId = pId;
		m_TakeKey = true;
		m_GotKey = false;
	}
	else if(ButtonResult == 2)
	{
		NewKey = 0;
		*pNewModifierCombination = CBinds::MODIFIER_NONE;
	}

	if(m_pKeyReaderId == pId && m_GotKey)
	{
		// abort with escape key
		if(m_Key.m_Key != KEY_ESCAPE)
		{
			NewKey = m_Key.m_Key;
			*pNewModifierCombination = m_ModifierCombination;
		}
		m_pKeyReaderId = nullptr;
		m_GotKey = false;
		Ui()->SetActiveItem(nullptr);
	}

	char aBuf[64];
	if(m_pKeyReaderId == pId && m_TakeKey)
		str_copy(aBuf, Localize("Press a key…"));
	else if(NewKey == 0)
		aBuf[0] = '\0';
	else
	{
		GameClient()->m_Binds.GetKeyBindName(NewKey, *pNewModifierCombination, aBuf, sizeof(aBuf));
	}

	const ColorRGBA Color = m_pKeyReaderId == pId && m_TakeKey ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.4f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * Ui()->ButtonColorMul(pId));
	pRect->Draw(Color, IGraphics::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	Ui()->DoLabel(&Temp, aBuf, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	return NewKey;
}

bool CKeyBinder::IsActive() const
{
	return m_TakeKey;
}
