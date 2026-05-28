#include "editor_history.h"

#include "editor.h"
#include "editor_actions.h"

#include <engine/font_icons.h>
#include <engine/shared/config.h>

void CEditorHistory::RecordAction(const std::shared_ptr<IEditorAction> &pAction)
{
	RecordAction(pAction, nullptr);
}

void CEditorHistory::Execute(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay)
{
	pAction->Redo();
	RecordAction(pAction, pDisplay);
}

void CEditorHistory::RecordAction(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay)
{
	if(m_IsBulk)
	{
		m_vpBulkActions.push_back(pAction);
		return;
	}

	m_vpRedoActions.clear();

	if((int)m_vpUndoActions.size() >= g_Config.m_ClEditorMaxHistory)
	{
		m_vpUndoActions.pop_front();
	}

	if(pDisplay == nullptr)
		m_vpUndoActions.emplace_back(pAction);
	else
		m_vpUndoActions.emplace_back(std::make_shared<CEditorActionBulk>(Map(), std::vector<std::shared_ptr<IEditorAction>>{pAction}, pDisplay));
}

bool CEditorHistory::Undo()
{
	if(m_vpUndoActions.empty())
		return false;

	auto pLastAction = m_vpUndoActions.back();
	m_vpUndoActions.pop_back();

	pLastAction->Undo();

	m_vpRedoActions.emplace_back(pLastAction);
	return true;
}

bool CEditorHistory::Redo()
{
	if(m_vpRedoActions.empty())
		return false;

	auto pLastAction = m_vpRedoActions.back();
	m_vpRedoActions.pop_back();

	pLastAction->Redo();

	m_vpUndoActions.emplace_back(pLastAction);
	return true;
}

void CEditorHistory::Clear()
{
	m_vpUndoActions.clear();
	m_vpRedoActions.clear();
}

void CEditorHistory::BeginBulk()
{
	m_IsBulk = true;
	m_vpBulkActions.clear();
}

void CEditorHistory::EndBulk(const char *pDisplay)
{
	if(!m_IsBulk)
		return;
	m_IsBulk = false;

	// Record bulk action
	if(!m_vpBulkActions.empty())
		RecordAction(std::make_shared<CEditorActionBulk>(Map(), m_vpBulkActions, pDisplay, true));

	m_vpBulkActions.clear();
}

void CEditorHistory::EndBulk(int DisplayToUse)
{
	EndBulk((DisplayToUse < 0 || DisplayToUse >= (int)m_vpBulkActions.size()) ? nullptr : m_vpBulkActions[DisplayToUse]->DisplayText());
}

void CEditor::RenderEditorHistory(CUIRect View)
{
	enum EHistoryType
	{
		EDITOR_HISTORY,
		ENVELOPE_HISTORY,
		SERVER_SETTINGS_HISTORY
	};

	static EHistoryType s_HistoryType = EDITOR_HISTORY;
	static int s_ActionSelectedIndex = 0;
	static CListBox s_ListBox;
	s_ListBox.SetActive(m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen());

	const bool GotSelection = s_ListBox.Active() && s_ActionSelectedIndex >= 0 && (size_t)s_ActionSelectedIndex < Map()->m_vSettings.size();

	CUIRect ToolBar, Button, Label, List, DragBar;
	View.HSplitTop(22.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::TOP, &m_aExtraEditorSplits[EXTRAEDITOR_HISTORY]);
	View.HSplitTop(20.0f, &ToolBar, &View);
	View.HSplitTop(2.0f, nullptr, &List);
	ToolBar.HMargin(2.0f, &ToolBar);

	CUIRect TypeButtons, HistoryTypeButton;
	const int HistoryTypeBtnSize = 70.0f;
	ToolBar.VSplitLeft(3 * HistoryTypeBtnSize, &TypeButtons, &Label);

	// history type buttons
	{
		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_EditorHistoryButton = 0;
		if(DoButton_Ex(&s_EditorHistoryButton, "Editor", s_HistoryType == EDITOR_HISTORY, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show map editor history.", IGraphics::CORNER_L))
		{
			s_HistoryType = EDITOR_HISTORY;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_EnvelopeEditorHistoryButton = 0;
		if(DoButton_Ex(&s_EnvelopeEditorHistoryButton, "Envelope", s_HistoryType == ENVELOPE_HISTORY, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show envelope editor history.", IGraphics::CORNER_NONE))
		{
			s_HistoryType = ENVELOPE_HISTORY;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		static int s_ServerSettingsHistoryButton = 0;
		if(DoButton_Ex(&s_ServerSettingsHistoryButton, "Settings", s_HistoryType == SERVER_SETTINGS_HISTORY, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show server settings editor history.", IGraphics::CORNER_R))
		{
			s_HistoryType = SERVER_SETTINGS_HISTORY;
		}
	}

	SLabelProperties InfoProps;
	InfoProps.m_MaxWidth = ToolBar.w - 60.f;
	InfoProps.m_EllipsisAtEnd = true;
	Label.VSplitLeft(8.0f, nullptr, &Label);
	Ui()->DoLabel(&Label, "Editor history. Click on an action to undo all actions above.", 10.0f, TEXTALIGN_ML, InfoProps);

	CEditorHistory *pCurrentHistory;
	if(s_HistoryType == EDITOR_HISTORY)
		pCurrentHistory = &Map()->m_EditorHistory;
	else if(s_HistoryType == ENVELOPE_HISTORY)
		pCurrentHistory = &Map()->m_EnvelopeEditorHistory;
	else if(s_HistoryType == SERVER_SETTINGS_HISTORY)
		pCurrentHistory = &Map()->m_ServerSettingsHistory;
	else
		return;

	// delete button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	static int s_DeleteButton = 0;
	if(DoButton_FontIcon(&s_DeleteButton, FontIcon::TRASH, (!pCurrentHistory->m_vpUndoActions.empty() || !pCurrentHistory->m_vpRedoActions.empty()) ? 0 : -1, &Button, BUTTONFLAG_LEFT, "Clear the history.", IGraphics::CORNER_ALL, 9.0f) || (GotSelection && CLineInput::GetActiveInput() == nullptr && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
	{
		pCurrentHistory->Clear();
		s_ActionSelectedIndex = 0;
	}

	// actions list
	int RedoSize = (int)pCurrentHistory->m_vpRedoActions.size();
	int UndoSize = (int)pCurrentHistory->m_vpUndoActions.size();
	s_ActionSelectedIndex = RedoSize;
	s_ListBox.DoStart(15.0f, RedoSize + UndoSize, 1, 3, s_ActionSelectedIndex, &List);

	for(int i = 0; i < RedoSize; i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&pCurrentHistory->m_vpRedoActions[i], s_ActionSelectedIndex >= 0 && s_ActionSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		TextRender()->TextColor({.5f, .5f, .5f});
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		Ui()->DoLabel(&Label, pCurrentHistory->m_vpRedoActions[i]->DisplayText(), 10.0f, TEXTALIGN_ML, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	for(int i = 0; i < UndoSize; i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&pCurrentHistory->m_vpUndoActions[UndoSize - i - 1], s_ActionSelectedIndex >= RedoSize && s_ActionSelectedIndex == (i + RedoSize));
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Label, pCurrentHistory->m_vpUndoActions[UndoSize - i - 1]->DisplayText(), 10.0f, TEXTALIGN_ML, Props);
	}

	{ // Base action "Loaded map" that cannot be undone
		static int s_BaseAction;
		const CListboxItem Item = s_ListBox.DoNextItem(&s_BaseAction, s_ActionSelectedIndex == RedoSize + UndoSize);
		if(Item.m_Visible)
		{
			Item.m_Rect.VMargin(5.0f, &Label);

			Ui()->DoLabel(&Label, "Loaded map", 10.0f, TEXTALIGN_ML);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(s_ActionSelectedIndex != NewSelected)
	{
		// Figure out if we should undo or redo some actions
		// Undo everything until the selected index
		if(NewSelected > s_ActionSelectedIndex)
		{
			for(int i = 0; i < (NewSelected - s_ActionSelectedIndex); i++)
			{
				pCurrentHistory->Undo();
			}
		}
		else
		{
			for(int i = 0; i < (s_ActionSelectedIndex - NewSelected); i++)
			{
				pCurrentHistory->Redo();
			}
		}
		s_ActionSelectedIndex = NewSelected;
	}
}
