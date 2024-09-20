#include <engine/keys.h>
#include <game/client/ui_listbox.h>
#include <game/editor/quick_action.h>

#include "editor.h"

#include "prompt.h"

bool FuzzyMatch(const char *pHaystack, const char *pNeedle)
{
	if(!pNeedle || !pNeedle[0])
		return false;
	char aBuf[2] = {0};
	const char *pHit = pHaystack;
	int NeedleLen = str_length(pNeedle);
	for(int i = 0; i < NeedleLen; i++)
	{
		if(!pHit)
			return false;
		aBuf[0] = pNeedle[i];
		pHit = str_find_nocase(pHit, aBuf);
		if(pHit)
			pHit++;
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

	Ui()->SetActiveItem(&m_PromptInput);
}

void CPrompt::SetInactive()
{
	m_ResetFilterResults = true;
	m_PromptInput.Clear();
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

void CPrompt::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);

#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, button_color, description) m_vQuickActions.emplace_back(&Editor()->m_QuickAction##name);
#include <game/editor/quick_actions.h>
#undef REGISTER_QUICK_ACTION
}

void CPrompt::OnRender(CUIRect _)
{
	if(!IsActive())
		return;

	if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		SetInactive();
		return;
	}

	static CListBox s_ListBox;
	CUIRect Prompt, PromptBox;
	CUIRect Suggestions;

	Ui()->MapScreen();
	CUIRect Overlay = *Ui()->Screen();

	Overlay.Draw(ColorRGBA(0, 0, 0, 0.33f), IGraphics::CORNER_NONE, 0.0f);
	CUIRect Background;
	Overlay.VMargin(150.0f, &Background);
	Background.HMargin(50.0f, &Background);
	Background.Draw(ColorRGBA(0, 0, 0, 0.80f), IGraphics::CORNER_ALL, 5.0f);

	Background.Margin(10.0f, &Prompt);

	Prompt.VSplitMid(nullptr, &PromptBox);

	Prompt.HSplitTop(16.0f, &PromptBox, &Suggestions);
	PromptBox.Draw(ColorRGBA(0, 0, 0, 0.75f), IGraphics::CORNER_ALL, 2.0f);
	Suggestions.y += 6.0f;

	if(Ui()->DoClearableEditBox(&m_PromptInput, &PromptBox, 10.0f) || m_ResetFilterResults)
	{
		m_PromptSelectedIndex = 0;
		m_vpFilteredPromptList.clear();
		if(m_ResetFilterResults && m_pLastAction && !m_pLastAction->Disabled())
		{
			m_vpFilteredPromptList.push_back(m_pLastAction);
		}
		for(auto *pQuickAction : m_vQuickActions)
		{
			if(pQuickAction->Disabled())
				continue;

			if(m_PromptInput.IsEmpty() || FuzzyMatch(pQuickAction->Label(), m_PromptInput.GetString()))
			{
				if(!m_ResetFilterResults || pQuickAction != m_pLastAction)
					m_vpFilteredPromptList.push_back(pQuickAction);
			}
		}
		m_ResetFilterResults = false;
	}

	s_ListBox.SetActive(!Ui()->IsPopupOpen());
	s_ListBox.DoStart(15.0f, m_vpFilteredPromptList.size(), 1, 5, m_PromptSelectedIndex, &Suggestions, false);

	float LabelWidth = Overlay.w > 855.0f ? 200.0f : 100.0f;

	for(size_t i = 0; i < m_vpFilteredPromptList.size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(m_vpFilteredPromptList[i]->ActionButtonId(), m_PromptSelectedIndex >= 0 && (size_t)m_PromptSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		CUIRect LabelColumn, DescColumn;
		float Margin = 5.0f;
		Item.m_Rect.VMargin(Margin, &LabelColumn);
		LabelColumn.VSplitLeft(LabelWidth, &LabelColumn, &DescColumn);
		DescColumn.VSplitLeft(Margin, nullptr, &DescColumn);

		SLabelProperties Props;
		Props.m_MaxWidth = LabelColumn.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&LabelColumn, m_vpFilteredPromptList[i]->Label(), 10.0f, TEXTALIGN_ML, Props);

		Props.m_MaxWidth = DescColumn.w;
		TextRender()->TextColor(TextRender()->DefaultTextColor().WithAlpha(Item.m_Selected ? 1.0f : 0.8f));
		Ui()->DoLabel(&DescColumn, m_vpFilteredPromptList[i]->Description(), 10.0f, TEXTALIGN_MR, Props);
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
			CQuickAction *pBtn = m_vpFilteredPromptList[m_PromptSelectedIndex];
			SetInactive();
			pBtn->Call();
			m_pLastAction = pBtn;
		}
	}
}
