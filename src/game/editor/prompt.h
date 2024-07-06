#ifndef GAME_EDITOR_PROMPT_H
#define GAME_EDITOR_PROMPT_H

#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>
#include <game/editor/editor_button.h>

#include "component.h"

class CPrompt : public CEditorComponent
{
	int m_PromptSelectedIndex = -1;

	std::vector<const CEditorButton *> m_vpFilteredPromptList;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_PromptInput;
	bool m_Active = false;

public:
	bool OnInput(const IInput::CEvent &Event) override;
	void OnRender(CUIRect View) override;
	bool IsActive() const { return m_Active; }
};

#endif
