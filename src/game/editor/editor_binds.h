#ifndef GAME_EDITOR_EDITOR_BINDS_H
#define GAME_EDITOR_EDITOR_BINDS_H

#include <engine/input.h>
#include <engine/keys.h>

constexpr int KEY_NONE = -1;

enum EBindSection
{
	GENERAL = 0,
	HISTORY,
	BRUSH,
	QUADS_AND_SOUNDS,
	FONT_TYPER,
	FILE_BROWSER,
	MAP_VIEW,
	LAYERS,
	SERVER_SETTINGS,
	NUM_SECTIONS,
};

class CEditorBind
{
public:
	CEditorBind() = default;
	CEditorBind(bool Shift, bool Modifier, bool Alt, int Key, const char *pDescription, EBindSection Section);
	virtual ~CEditorBind() = default;

	void OnInit(const IInput *pInput);
	bool KeyPress(const IInput::CEvent &Event, const IInput *pInput) const;
	bool KeyPress(const IInput *pInput) const;
	const char *KeyBindText() const { return m_aKeybindText; }
	const char *Description() const { return m_aBindDescription; }
	EBindSection Section() const { return m_Section; }
	bool IsValid() const;
	int GetKey() const { return m_Key; }

private:
	bool m_Shift = false;
	bool m_Modifier = false;
	bool m_Alt = false;
	int m_Key = KEY_NONE;
	char m_aKeybindText[64] = "";
	char m_aBindDescription[256] = "";
	EBindSection m_Section;
};

#endif
