#include "editor_binds.h"

#include <base/str.h>

#include <engine/font_icons.h>

#include <game/editor/editor.h>

CEditorBind::CEditorBind(bool Shift, bool Modifier, bool Alt, int Key, const char *pDescription, EBindSection Section) :
	m_Shift(Shift), m_Modifier(Modifier), m_Alt(Alt), m_Key(Key), m_Section(Section)
{
	str_copy(m_aBindDescription, pDescription);
	m_aKeybindText[0] = '\0';
}

void CEditorBind::OnInit(const IInput *pInput)
{
	if(!IsValid())
		return;

	auto Append = [&](const char *pText) {
		str_append(m_aKeybindText, pText);
	};

	auto AppendChar = [&](const char Char) {
		char aBuf[2] = {Char, '\0'};
		Append(aBuf);
	};

	bool AnyKeyAdded = false;
	Append("[");

	auto AddKey = [&](const char *pKeyName) {
		if(AnyKeyAdded)
			Append("+");
		const char *pNumPad = str_startswith(pKeyName, "kp_");
		if(pNumPad)
		{
			Append("Numpad-");
			AppendChar(str_uppercase(pNumPad[0]));
			Append(pNumPad + 1);
		}
		else if(str_startswith(pKeyName, "mouse"))
		{
			const char *pMouse = str_startswith(pKeyName, "mouse");

			switch(pMouse[0])
			{
			case '1':
				Append("Left mouse");
				break;
			case '2':
				Append("Right mouse");
				break;
			case '3':
				Append("Middle mouse");
				break;
			default:
				Append("Mouse ");

				const char *pWheel = str_startswith(pMouse, "wheel");
				if(pWheel)
				{
					Append("wheel ");
					Append(pWheel);
				}
				else
					Append(pMouse);
			}
		}
		else
		{
			AppendChar(str_uppercase(pKeyName[0]));
			Append(pKeyName + 1);
		}
		AnyKeyAdded = true;
	};

	if(m_Modifier)
		AddKey("Ctrl");
	if(m_Shift)
		AddKey("Shift");
	if(m_Alt)
		AddKey("Alt");
	if(m_Key != KEY_NONE)
		AddKey(pInput->KeyName(m_Key));
	Append("]");
}

bool CEditorBind::KeyPress(const IInput::CEvent &Event, const IInput *pInput) const
{
	// only handle key down and not also key up
	if(!(Event.m_Flags & IInput::FLAG_PRESS))
		return false;
	return (!m_Shift || pInput->ShiftIsPressed()) && (!m_Modifier || pInput->ModifierIsPressed()) && (!m_Alt || pInput->AltIsPressed()) && (m_Key == KEY_NONE || Event.m_Key == m_Key);
}

bool CEditorBind::KeyPress(const IInput *pInput) const
{
	return (!m_Shift || pInput->ShiftIsPressed()) && (!m_Modifier || pInput->ModifierIsPressed()) && (!m_Alt || pInput->AltIsPressed()) && (m_Key == KEY_NONE || pInput->KeyPress(m_Key));
}

bool CEditorBind::IsValid() const
{
	return m_Key != KEY_NONE || m_Shift || m_Modifier || m_Alt;
}
