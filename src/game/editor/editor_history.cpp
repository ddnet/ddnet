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
	CEditorHistoryUiState &State = Map()->m_EditorHistoryUiState;
	CListBox &ListBox = State.m_aListBoxes[(int)State.m_HistoryType];
	int &SelectedActionIndex = State.m_aSelectedActionIndices[(int)State.m_HistoryType];

	ListBox.SetActive(m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen());

	const bool GotSelection = ListBox.Active() && SelectedActionIndex >= 0 && (size_t)SelectedActionIndex < Map()->m_vSettings.size();

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
		if(DoButton_Ex(&State.m_aHistoryTypeButtonIds[(int)EHistoryType::EDITOR], "Editor", State.m_HistoryType == EHistoryType::EDITOR, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show map editor history.", IGraphics::CORNER_L))
		{
			State.m_HistoryType = EHistoryType::EDITOR;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		if(DoButton_Ex(&State.m_aHistoryTypeButtonIds[(int)EHistoryType::ENVELOPE], "Envelope", State.m_HistoryType == EHistoryType::ENVELOPE, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show envelope editor history.", IGraphics::CORNER_NONE))
		{
			State.m_HistoryType = EHistoryType::ENVELOPE;
		}

		TypeButtons.VSplitLeft(HistoryTypeBtnSize, &HistoryTypeButton, &TypeButtons);
		if(DoButton_Ex(&State.m_aHistoryTypeButtonIds[(int)EHistoryType::SERVER_SETTINGS], "Settings", State.m_HistoryType == EHistoryType::SERVER_SETTINGS, &HistoryTypeButton, BUTTONFLAG_LEFT, "Show server settings editor history.", IGraphics::CORNER_R))
		{
			State.m_HistoryType = EHistoryType::SERVER_SETTINGS;
		}
	}

	SLabelProperties InfoProps;
	InfoProps.m_MaxWidth = ToolBar.w - 60.f;
	InfoProps.m_EllipsisAtEnd = true;
	Label.VSplitLeft(8.0f, nullptr, &Label);
	Ui()->DoLabel(&Label, "Editor history. Click on an action to undo all actions above.", 10.0f, TEXTALIGN_ML, InfoProps);

	CEditorHistory *pCurrentHistory;
	if(State.m_HistoryType == EHistoryType::EDITOR)
	{
		pCurrentHistory = &Map()->m_EditorHistory;
	}
	else if(State.m_HistoryType == EHistoryType::ENVELOPE)
	{
		pCurrentHistory = &Map()->m_EnvelopeEditorHistory;
	}
	else if(State.m_HistoryType == EHistoryType::SERVER_SETTINGS)
	{
		pCurrentHistory = &Map()->m_ServerSettingsHistory;
	}
	else
	{
		dbg_assert_failed("Invalid State.m_HistoryType: %d", (int)State.m_HistoryType);
	}

	// delete button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	if(DoButton_FontIcon(&State.m_DeleteButtonId, FontIcon::TRASH, (!pCurrentHistory->m_vpUndoActions.empty() || !pCurrentHistory->m_vpRedoActions.empty()) ? 0 : -1, &Button, BUTTONFLAG_LEFT, "Clear the history.", IGraphics::CORNER_ALL, 9.0f) ||
		(GotSelection && CLineInput::GetActiveInput() == nullptr && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
	{
		pCurrentHistory->Clear();
		SelectedActionIndex = 0;
	}

	// actions list
	int RedoSize = (int)pCurrentHistory->m_vpRedoActions.size();
	int UndoSize = (int)pCurrentHistory->m_vpUndoActions.size();
	SelectedActionIndex = RedoSize;
	ListBox.DoStart(15.0f, RedoSize + UndoSize, 1, 3, SelectedActionIndex, &List);

	for(int i = 0; i < RedoSize; i++)
	{
		const CListboxItem Item = ListBox.DoNextItem(&pCurrentHistory->m_vpRedoActions[i], SelectedActionIndex >= 0 && SelectedActionIndex == i);
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
		const CListboxItem Item = ListBox.DoNextItem(&pCurrentHistory->m_vpUndoActions[UndoSize - i - 1], SelectedActionIndex >= RedoSize && SelectedActionIndex == (i + RedoSize));
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Label, pCurrentHistory->m_vpUndoActions[UndoSize - i - 1]->DisplayText(), 10.0f, TEXTALIGN_ML, Props);
	}

	{ // Base action "Loaded map" that cannot be undone
		const CListboxItem Item = ListBox.DoNextItem(&State.m_BaseActionButtonId, SelectedActionIndex == RedoSize + UndoSize);
		if(Item.m_Visible)
		{
			Item.m_Rect.VMargin(5.0f, &Label);

			Ui()->DoLabel(&Label, "Loaded map", 10.0f, TEXTALIGN_ML);
		}
	}

	const int NewSelected = ListBox.DoEnd();
	if(SelectedActionIndex != NewSelected)
	{
		// Figure out if we should undo or redo some actions
		// Undo everything until the selected index
		if(NewSelected > SelectedActionIndex)
		{
			for(int i = 0; i < (NewSelected - SelectedActionIndex); i++)
			{
				pCurrentHistory->Undo();
			}
		}
		else
		{
			for(int i = 0; i < (SelectedActionIndex - NewSelected); i++)
			{
				pCurrentHistory->Redo();
			}
		}
		SelectedActionIndex = NewSelected;
	}
}
