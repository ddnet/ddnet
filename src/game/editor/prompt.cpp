#include <engine/keys.h>
#include <game/client/ui_listbox.h>
#include <game/editor/quick_action.h>

#include "editor.h"

#include "prompt.h"

bool FuzzyMatch(const char *pHaystack, const char *pNeedle)
{
	if(!pNeedle || !pNeedle[0])
		return false;
	const char *pHit = str_find_nocase_char(pHaystack, pNeedle[0]);
	for(int i = 0; i < str_length(pNeedle); i++)
	{
		if(!pHit)
			return false;
		char Search = pNeedle[i];
		pHit = str_find_nocase_char(pHit, Search);
	}
	return pHit;
}

bool CPrompt::IsActive()
{
	return CEditorComponent::IsActive() || Editor()->m_Dialog == DIALOG_QUICK_PROMPT;
}

void CPrompt::SetActive()
{
	Editor()->m_Dialog = DIALOG_QUICK_PROMPT;
	CEditorComponent::SetActive();
}

void CPrompt::SetInactive()
{
	if(Editor()->m_Dialog == DIALOG_QUICK_PROMPT)
		Editor()->m_Dialog = DIALOG_NONE;
	CEditorComponent::SetInactive();
}

bool CPrompt::OnInput(const IInput::CEvent &Event)
{
	if(Input()->ModifierIsPressed() && Input()->KeyIsPressed(KEY_P))
	{
		SetActive();
	}
	return false;
}

void CPrompt::OnRender(CUIRect View)
{
	if(!IsActive())
		return;

	if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		SetInactive();
	}

	static CListBox s_ListBox;
	CUIRect Prompt, PromptBox;

	static std::initializer_list<CQuickAction *> s_AllQuickActionsNullptr = {
		nullptr // unholy hack to make the macro work with trailing commas
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, description) , &Editor()->m_QuickAction##name
#include <game/editor/quick_actions.h>
#undef REGISTER_QUICK_ACTION
	};
	static std::vector<CQuickAction *> s_AllQuickActions = {s_AllQuickActionsNullptr.begin() + 1, s_AllQuickActionsNullptr.end()};

	// m_vpCompletePromptList.clear();
	// m_vpCompletePromptList.emplace_back((char *)"Add Quad");
	// m_vpCompletePromptList.emplace_back((char *)"Add Sound");
	// m_vpCompletePromptList.emplace_back((char *)"Add Group");
	// m_vpCompletePromptList.emplace_back((char *)"Add tele layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add speedup layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add tune layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add front layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add switch layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add quads layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add tile layer");
	// m_vpCompletePromptList.emplace_back((char *)"Add sound layer");
	// m_vpCompletePromptList.emplace_back((char *)"Save As");
	// m_vpCompletePromptList.emplace_back((char *)"Save Copy");
	// m_vpCompletePromptList.emplace_back((char *)"Toggle High Detail");
	// m_vpCompletePromptList.emplace_back((char *)"Refocus");
	// m_vpCompletePromptList.emplace_back((char *)"Envelopes");
	// m_vpCompletePromptList.emplace_back((char *)"Server settings");
	// m_vpCompletePromptList.emplace_back((char *)"History");
	// m_vpCompletePromptList.emplace_back((char *)"Toggle preview of how layers will be zoomed in-game");

	CUIRect Suggestions;

	View.HSplitMid(&Prompt, nullptr);

	Prompt.VSplitMid(nullptr, &PromptBox);

	Prompt.Draw(ColorRGBA(0, 0, 0, 0.75f), IGraphics::CORNER_ALL, 10.0f);
	Prompt.HSplitTop(32.0f, &PromptBox, &Suggestions);
	PromptBox.Draw(ColorRGBA(1, 0, 0, 0.75f), IGraphics::CORNER_ALL, 10.0f);

	if(Ui()->DoClearableEditBox(&m_PromptInput, &PromptBox, 10.0f))
	{
		m_PromptSelectedIndex = 0;
		m_vpFilteredPromptList.clear();
		for(auto *pQuickAction : s_AllQuickActions)
		{
			if(m_PromptInput.IsEmpty() || FuzzyMatch(pQuickAction->Label(), m_PromptInput.GetString()))
			{
				m_vpFilteredPromptList.push_back(pQuickAction);
			}
		}
	}
	Ui()->SetActiveItem(&m_PromptInput);

	s_ListBox.SetActive(!Ui()->IsPopupOpen());
	s_ListBox.DoStart(15.0f, m_vpFilteredPromptList.size(), 1, 5, m_PromptSelectedIndex, &Suggestions, false);

	for(size_t i = 0; i < m_vpFilteredPromptList.size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(m_vpFilteredPromptList[i], m_PromptSelectedIndex >= 0 && (size_t)m_PromptSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		CUIRect Button, FileIcon, TimeModified;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h, &FileIcon, &Button);
		Button.VSplitLeft(5.0f, nullptr, &Button);
		Button.VSplitRight(100.0f, &Button, &TimeModified);
		Button.VSplitRight(5.0f, &Button, nullptr);

		SLabelProperties Props;
		Props.m_MaxWidth = Button.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Button, m_vpFilteredPromptList[i]->Label(), 10.0f, TEXTALIGN_ML, Props);
		TextRender()->TextColor(Item.m_Selected ? ColorRGBA(0, 0, 0, 1) : TextRender()->DefaultTextSelectionColor());
		Ui()->DoLabel(&Button, m_vpFilteredPromptList[i]->Description(), 10.0f, TEXTALIGN_MR, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(m_PromptSelectedIndex != NewSelected)
	{
		m_PromptSelectedIndex = NewSelected;
	}

	if(s_ListBox.WasItemActivated())
	{
		if(m_PromptSelectedIndex >= 0)
		{
			const CQuickAction *pBtn = m_vpFilteredPromptList[m_PromptSelectedIndex];
			pBtn->Call();
			m_PromptInput.Clear();
			SetInactive();
		}
	}
}
