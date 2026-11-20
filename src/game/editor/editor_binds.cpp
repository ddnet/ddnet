#include "editor_binds.h"

#include <base/str.h>

#include <engine/client/keyboard.h>
#include <engine/font_icons.h>
#include <engine/keys.h>

#include <game/editor/editor.h>

CEditorBind::CEditorBind(bool Shift, bool Modifier, bool Alt, int Key, const char *pDescription, EBindSection Section) :
	m_Shift(Shift), m_Modifier(Modifier), m_Alt(Alt), m_Key(Key), m_Section(Section)
{
	str_copy(m_aBindDescription, pDescription);
	m_aKeybindText[0] = '\0';
	BuildKeybind();
}

void CEditorBind::BuildKeybind()
{
	if(!IsValid())
		return;

	auto Append = [&](const char *pText) {
		str_append(m_aKeybindText, pText);
	};

	bool AnyKeyAdded = false;
	Append("[");

	auto AddKey = [&](const char *pKeyName) {
		if(AnyKeyAdded)
			Append("+");
		Append(pKeyName);
		AnyKeyAdded = true;
	};

	if(m_Modifier)
		AddKey("Ctrl");
	if(m_Shift)
		AddKey("Shift");
	if(m_Alt)
		AddKey("Alt");
	if(m_Key != KEY_UNKNOWN)
	{
		char aKeyName[20];
		str_copy(aKeyName, KeyName(m_Key));
		for(size_t CharId = 0; CharId < sizeof(aKeyName); ++CharId)
		{
			if(aKeyName[CharId] == '\0')
				break;
			else if((CharId > 0 && aKeyName[CharId - 1] == '_') || CharId == 0)
				aKeyName[CharId] = str_uppercase(aKeyName[CharId]);
		}
		AddKey(aKeyName);
	}
	Append("]");
}

bool CEditorBind::KeyPress(const IInput::CEvent &Event, const IInput *pInput) const
{
	// only handle key down and not also key up
	if(!(Event.m_Flags & IInput::FLAG_PRESS))
		return false;
	return (!m_Shift || pInput->ShiftIsPressed()) && (!m_Modifier || pInput->ModifierIsPressed()) && (!m_Alt || pInput->AltIsPressed()) && (m_Key == KEY_UNKNOWN || Event.m_Key == m_Key);
}

bool CEditorBind::KeyPress(const IInput *pInput) const
{
	return (!m_Shift || pInput->ShiftIsPressed()) && (!m_Modifier || pInput->ModifierIsPressed()) && (!m_Alt || pInput->AltIsPressed()) && (m_Key == KEY_UNKNOWN || pInput->KeyPress(m_Key));
}

bool CEditorBind::IsValid() const
{
	return m_Key != KEY_UNKNOWN || m_Shift || m_Modifier || m_Alt;
}
