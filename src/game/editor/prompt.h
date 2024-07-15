#ifndef GAME_EDITOR_PROMPT_H
#define GAME_EDITOR_PROMPT_H

#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>
#include <game/editor/quick_action.h>

#include "component.h"

class CPrompt : public CEditorComponent
{
	int m_PromptSelectedIndex = -1;

	std::vector<const CQuickAction *> m_vpFilteredPromptList;
	CLineInputBuffered<512> m_PromptInput;

public:
	bool OnInput(const IInput::CEvent &Event) override;
	void OnRender(CUIRect View) override;
	bool IsActive();
	void SetActive();
	void SetInactive();
};

#endif
