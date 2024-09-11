#ifndef GAME_EDITOR_PROMPT_H
#define GAME_EDITOR_PROMPT_H

#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>
#include <game/editor/quick_action.h>

#include "component.h"

class CPrompt : public CEditorComponent
{
	bool m_ResetFilterResults = true;
	CQuickAction *m_pLastAction = nullptr;
	int m_PromptSelectedIndex = -1;

	std::vector<CQuickAction *> m_vpFilteredPromptList;
	std::vector<CQuickAction *> m_vQuickActions;
	CLineInputBuffered<512> m_PromptInput;

public:
	void OnInit(CEditor *pEditor) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnRender(CUIRect _) override;
	bool IsActive();
	void SetActive();
	void SetInactive();
};

#endif
