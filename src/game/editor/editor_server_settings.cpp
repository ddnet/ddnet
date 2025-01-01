#include "editor_server_settings.h"
#include "editor.h"

#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/editor/editor_actions.h>
#include <game/editor/editor_history.h>

#include <base/color.h>
#include <base/system.h>

#include <iterator>

using namespace FontIcons;

static const int FONT_SIZE = 12.0f;

struct IMapSetting
{
	enum EType
	{
		SETTING_INT,
		SETTING_COMMAND,
	};
	const char *m_pName;
	const char *m_pHelp;
	EType m_Type;

	IMapSetting(const char *pName, const char *pHelp, EType Type) :
		m_pName(pName), m_pHelp(pHelp), m_Type(Type) {}
};
struct SMapSettingInt : public IMapSetting
{
	int m_Default;
	int m_Min;
	int m_Max;

	SMapSettingInt(const char *pName, const char *pHelp, int Default, int Min, int Max) :
		IMapSetting(pName, pHelp, IMapSetting::SETTING_INT), m_Default(Default), m_Min(Min), m_Max(Max) {}
};
struct SMapSettingCommand : public IMapSetting
{
	const char *m_pArgs;

	SMapSettingCommand(const char *pName, const char *pHelp, const char *pArgs) :
		IMapSetting(pName, pHelp, IMapSetting::SETTING_COMMAND), m_pArgs(pArgs) {}
};

void CEditor::RenderServerSettingsEditor(CUIRect View, bool ShowServerSettingsEditorLast)
{
	static int s_CommandSelectedIndex = -1;
	static CListBox s_ListBox;
	s_ListBox.SetActive(!m_MapSettingsCommandContext.m_DropdownContext.m_ListBox.Active() && m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen());

	bool GotSelection = s_ListBox.Active() && s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex < m_Map.m_vSettings.size();
	const bool CurrentInputValid = m_MapSettingsCommandContext.Valid(); // Use the context to validate the input

	CUIRect ToolBar, Button, Label, List, DragBar;
	View.HSplitTop(22.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::SIDE_TOP, &m_aExtraEditorSplits[EXTRAEDITOR_SERVER_SETTINGS]);
	View.HSplitTop(20.0f, &ToolBar, &View);
	View.HSplitTop(2.0f, nullptr, &List);
	ToolBar.HMargin(2.0f, &ToolBar);

	// delete button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	static int s_DeleteButton = 0;
	if(DoButton_FontIcon(&s_DeleteButton, FONT_ICON_TRASH, GotSelection ? 0 : -1, &Button, 0, "[Delete] Delete the selected command from the command list.", IGraphics::CORNER_ALL, 9.0f) == 1 || (GotSelection && CLineInput::GetActiveInput() == nullptr && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
	{
		m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::DELETE, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand));

		m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
		if(s_CommandSelectedIndex >= (int)m_Map.m_vSettings.size())
			s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
		if(s_CommandSelectedIndex >= 0)
			m_SettingsCommandInput.Set(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand);
		else
			m_SettingsCommandInput.Clear();
		m_Map.OnModify();
		m_MapSettingsCommandContext.Update();
		s_ListBox.ScrollToSelected();
	}

	// move down button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	const bool CanMoveDown = GotSelection && s_CommandSelectedIndex < (int)m_Map.m_vSettings.size() - 1;
	static int s_DownButton = 0;
	if(DoButton_FontIcon(&s_DownButton, FONT_ICON_SORT_DOWN, CanMoveDown ? 0 : -1, &Button, 0, "[Alt+Down] Move the selected command down.", IGraphics::CORNER_R, 11.0f) == 1 || (CanMoveDown && Input()->AltIsPressed() && Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN)))
	{
		m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::MOVE_DOWN, &s_CommandSelectedIndex, s_CommandSelectedIndex));

		std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex + 1]);
		s_CommandSelectedIndex++;
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
	}

	// move up button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	const bool CanMoveUp = GotSelection && s_CommandSelectedIndex > 0;
	static int s_UpButton = 0;
	if(DoButton_FontIcon(&s_UpButton, FONT_ICON_SORT_UP, CanMoveUp ? 0 : -1, &Button, 0, "[Alt+Up] Move the selected command up.", IGraphics::CORNER_L, 11.0f) == 1 || (CanMoveUp && Input()->AltIsPressed() && Ui()->ConsumeHotkey(CUi::HOTKEY_UP)))
	{
		m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::MOVE_UP, &s_CommandSelectedIndex, s_CommandSelectedIndex));

		std::swap(m_Map.m_vSettings[s_CommandSelectedIndex], m_Map.m_vSettings[s_CommandSelectedIndex - 1]);
		s_CommandSelectedIndex--;
		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
	}

	// redo button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	static int s_RedoButton = 0;
	if(DoButton_FontIcon(&s_RedoButton, FONT_ICON_REDO, m_ServerSettingsHistory.CanRedo() ? 0 : -1, &Button, 0, "[Ctrl+Y] Redo the last command edit.", IGraphics::CORNER_R, 11.0f) == 1 || (CanMoveDown && Input()->AltIsPressed() && Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN)))
	{
		m_ServerSettingsHistory.Redo();
	}

	// undo button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
	static int s_UndoButton = 0;
	if(DoButton_FontIcon(&s_UndoButton, FONT_ICON_UNDO, m_ServerSettingsHistory.CanUndo() ? 0 : -1, &Button, 0, "[Ctrl+Z] Undo the last command edit.", IGraphics::CORNER_L, 11.0f) == 1 || (CanMoveUp && Input()->AltIsPressed() && Ui()->ConsumeHotkey(CUi::HOTKEY_UP)))
	{
		m_ServerSettingsHistory.Undo();
	}

	GotSelection = s_ListBox.Active() && s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex < m_Map.m_vSettings.size();

	int CollidingCommandIndex = -1;
	ECollisionCheckResult CheckResult = ECollisionCheckResult::ERROR;
	if(CurrentInputValid)
		CollidingCommandIndex = m_MapSettingsCommandContext.CheckCollision(CheckResult);

	// update button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	const bool CanAdd = CheckResult == ECollisionCheckResult::ADD;
	const bool CanReplace = CheckResult == ECollisionCheckResult::REPLACE;

	const bool CanUpdate = GotSelection && CurrentInputValid && str_comp(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, m_SettingsCommandInput.GetString()) != 0;

	static int s_UpdateButton = 0;
	if(DoButton_FontIcon(&s_UpdateButton, FONT_ICON_PENCIL, CanUpdate ? 0 : -1, &Button, 0, "[Alt+Enter] Update the selected command based on the entered value.", IGraphics::CORNER_R, 9.0f) == 1 || (CanUpdate && Input()->AltIsPressed() && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(CollidingCommandIndex == -1)
		{
			bool Found = false;
			int i;
			for(i = 0; i < (int)m_Map.m_vSettings.size(); ++i)
			{
				if(i != s_CommandSelectedIndex && !str_comp(m_Map.m_vSettings[i].m_aCommand, m_SettingsCommandInput.GetString()))
				{
					Found = true;
					break;
				}
			}
			if(Found)
			{
				m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::DELETE, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand));
				m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + s_CommandSelectedIndex);
				s_CommandSelectedIndex = i > s_CommandSelectedIndex ? i - 1 : i;
			}
			else
			{
				const char *pStr = m_SettingsCommandInput.GetString();
				m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::EDIT, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr));
				str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr);
			}
		}
		else
		{
			if(s_CommandSelectedIndex == CollidingCommandIndex)
			{ // If we are editing the currently collinding line, then we can just call EDIT on it
				const char *pStr = m_SettingsCommandInput.GetString();
				m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::EDIT, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr));
				str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr);
			}
			else
			{ // If not, then editing the current selected line will result in the deletion of the colliding line, and the editing of the selected line
				const char *pStr = m_SettingsCommandInput.GetString();

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Delete command %d; Edit command %d", CollidingCommandIndex, s_CommandSelectedIndex);

				m_ServerSettingsHistory.BeginBulk();
				// Delete the colliding command
				m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::DELETE, &s_CommandSelectedIndex, CollidingCommandIndex, m_Map.m_vSettings[CollidingCommandIndex].m_aCommand));
				m_Map.m_vSettings.erase(m_Map.m_vSettings.begin() + CollidingCommandIndex);
				// Edit the selected command
				s_CommandSelectedIndex = s_CommandSelectedIndex > CollidingCommandIndex ? s_CommandSelectedIndex - 1 : s_CommandSelectedIndex;
				m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::EDIT, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr));
				str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr);

				m_ServerSettingsHistory.EndBulk(aBuf);
			}
		}

		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
		m_SettingsCommandInput.Clear();
		m_MapSettingsCommandContext.Reset(); // Reset context
		Ui()->SetActiveItem(&m_SettingsCommandInput);
	}

	// add button
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	ToolBar.VSplitRight(100.0f, &ToolBar, nullptr);

	static int s_AddButton = 0;
	if(DoButton_FontIcon(&s_AddButton, CanReplace ? FONT_ICON_ARROWS_ROTATE : FONT_ICON_PLUS, CanAdd || CanReplace ? 0 : -1, &Button, 0, CanReplace ? "[Enter] Replace the corresponding command in the command list." : "[Enter] Add a command to the command list.", IGraphics::CORNER_L) == 1 || ((CanAdd || CanReplace) && !Input()->AltIsPressed() && m_Dialog == DIALOG_NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(CanReplace)
		{
			dbg_assert(CollidingCommandIndex != -1, "Could not replace command");
			s_CommandSelectedIndex = CollidingCommandIndex;

			const char *pStr = m_SettingsCommandInput.GetString();
			m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::EDIT, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr));
			str_copy(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand, pStr);
		}
		else if(CanAdd)
		{
			m_Map.m_vSettings.emplace_back(m_SettingsCommandInput.GetString());
			s_CommandSelectedIndex = m_Map.m_vSettings.size() - 1;
			m_ServerSettingsHistory.RecordAction(std::make_shared<CEditorCommandAction>(this, CEditorCommandAction::EType::ADD, &s_CommandSelectedIndex, s_CommandSelectedIndex, m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand));
		}

		m_Map.OnModify();
		s_ListBox.ScrollToSelected();
		m_SettingsCommandInput.Clear();
		m_MapSettingsCommandContext.Reset(); // Reset context
		Ui()->SetActiveItem(&m_SettingsCommandInput);
	}

	// command input (use remaining toolbar width)
	if(!ShowServerSettingsEditorLast) // Just activated
		Ui()->SetActiveItem(&m_SettingsCommandInput);
	m_SettingsCommandInput.SetEmptyText("Command");

	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// command list
	s_ListBox.DoStart(15.0f, m_Map.m_vSettings.size(), 1, 3, s_CommandSelectedIndex, &List);

	for(size_t i = 0; i < m_Map.m_vSettings.size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&m_Map.m_vSettings[i], s_CommandSelectedIndex >= 0 && (size_t)s_CommandSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VMargin(5.0f, &Label);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Label, m_Map.m_vSettings[i].m_aCommand, 10.0f, TEXTALIGN_ML, Props);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(s_CommandSelectedIndex != NewSelected || s_ListBox.WasItemSelected())
	{
		s_CommandSelectedIndex = NewSelected;
		if(m_SettingsCommandInput.IsEmpty() || !Input()->ModifierIsPressed()) // Allow ctrl+click to only change selection
		{
			m_SettingsCommandInput.Set(m_Map.m_vSettings[s_CommandSelectedIndex].m_aCommand);
			m_MapSettingsCommandContext.Update();
			m_MapSettingsCommandContext.UpdateCursor(true);
		}
		m_MapSettingsCommandContext.m_DropdownContext.m_ShouldHide = true;
		Ui()->SetActiveItem(&m_SettingsCommandInput);
	}

	// Map setting input
	DoMapSettingsEditBox(&m_MapSettingsCommandContext, &ToolBar, FONT_SIZE, List.h);
}

void CEditor::DoMapSettingsEditBox(CMapSettingsBackend::CContext *pContext, const CUIRect *pRect, float FontSize, float DropdownMaxHeight, int Corners, const char *pToolTip)
{
	// Main method to do the full featured map settings edit box

	auto *pLineInput = pContext->LineInput();
	auto &Context = *pContext;
	Context.SetFontSize(FontSize);

	// Set current active context if input is active
	if(pLineInput->IsActive())
		CMapSettingsBackend::ms_pActiveContext = pContext;

	// Small utility to render a floating part above the input rect.
	// Use to display either the error or the current argument name
	const float PartMargin = 4.0f;
	auto &&RenderFloatingPart = [&](CUIRect *pInputRect, float x, const char *pStr) {
		CUIRect Background;
		Background.x = x - PartMargin;
		Background.y = pInputRect->y - pInputRect->h - 6.0f;
		Background.w = TextRender()->TextWidth(FontSize, pStr) + 2 * PartMargin;
		Background.h = pInputRect->h;
		Background.Draw(ColorRGBA(0, 0, 0, 0.9f), IGraphics::CORNER_ALL, 3.0f);

		CUIRect Label;
		Background.VSplitLeft(PartMargin, nullptr, &Label);
		TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
		Ui()->DoLabel(&Label, pStr, FontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	// If we have a valid command, display the help in the tooltip
	if(Context.CommandIsValid() && pLineInput->IsActive() && Ui()->HotItem() == nullptr)
	{
		Context.GetCommandHelpText(m_aTooltip, sizeof(m_aTooltip));
		str_append(m_aTooltip, ".");
	}

	CUIRect ToolBar = *pRect;
	CUIRect Button;
	ToolBar.VSplitRight(ToolBar.h, &ToolBar, &Button);

	// Do the unknown command toggle button
	if(DoButton_FontIcon(&Context.m_AllowUnknownCommands, FONT_ICON_QUESTION, Context.m_AllowUnknownCommands, &Button, 0, "Disallow/allow unknown or invalid commands.", IGraphics::CORNER_R))
	{
		Context.m_AllowUnknownCommands = !Context.m_AllowUnknownCommands;
		Context.Update();
	}

	// Color the arguments
	std::vector<STextColorSplit> vColorSplits;
	Context.ColorArguments(vColorSplits);

	// Do and render clearable edit box with the colors
	if(DoClearableEditBox(pLineInput, &ToolBar, FontSize, IGraphics::CORNER_L, "Enter a server setting. Press ctrl+space to show available settings.", vColorSplits))
	{
		Context.Update(); // Update the context when contents change
		Context.m_DropdownContext.m_ShouldHide = false;
	}

	// Update/track the cursor
	if(Context.UpdateCursor())
		Context.m_DropdownContext.m_ShouldHide = false;

	// Calculate x position of the dropdown and the floating part
	float x = ToolBar.x + Context.CurrentArgPos() - pLineInput->GetScrollOffset();
	x = clamp(x, ToolBar.x + PartMargin, ToolBar.x + ToolBar.w);

	if(pLineInput->IsActive())
	{
		// If line input is active, let's display a floating part for either the current argument name
		// or for the error, if any. The error is only displayed when the cursor is at the end of the input.
		const bool IsAtEnd = pLineInput->GetCursorOffset() >= (m_MapSettingsCommandContext.CommentOffset() != -1 ? m_MapSettingsCommandContext.CommentOffset() : pLineInput->GetLength());

		if(Context.CurrentArgName() && (!Context.HasError() || !IsAtEnd)) // Render argument name
			RenderFloatingPart(&ToolBar, x, Context.CurrentArgName());
		else if(Context.HasError() && IsAtEnd) // Render error
			RenderFloatingPart(&ToolBar, ToolBar.x + PartMargin, Context.Error());
	}

	// If we have possible matches for the current argument, let's display an editbox suggestions dropdown
	const auto &vPossibleCommands = Context.PossibleMatches();
	int Selected = DoEditBoxDropdown<SPossibleValueMatch>(&Context.m_DropdownContext, pLineInput, &ToolBar, x - PartMargin, DropdownMaxHeight, Context.CurrentArg() >= 0, vPossibleCommands, MapSettingsDropdownRenderCallback);

	// If the dropdown just became visible, update the context
	// This is needed when input loses focus and then we click a command in the map settings list
	if(Context.m_DropdownContext.m_DidBecomeVisible)
	{
		Context.Update();
		Context.UpdateCursor(true);
	}

	if(!vPossibleCommands.empty())
	{
		// Check if the completion index has changed
		if(Selected != pContext->m_CurrentCompletionIndex)
		{
			// If so, we should autocomplete the selected option
			if(Selected != -1)
			{
				const char *pStr = vPossibleCommands[Selected].m_pValue;
				int Len = pContext->m_CurrentCompletionIndex == -1 ? str_length(Context.CurrentArgValue()) : (pContext->m_CurrentCompletionIndex < (int)vPossibleCommands.size() ? str_length(vPossibleCommands[pContext->m_CurrentCompletionIndex].m_pValue) : 0);
				size_t Start = Context.CurrentArgOffset();
				size_t End = Start + Len;
				pLineInput->SetRange(pStr, Start, End);
			}

			pContext->m_CurrentCompletionIndex = Selected;
		}
	}
	else
	{
		Context.m_DropdownContext.m_ListBox.SetActive(false);
	}
}

template<typename T>
int CEditor::DoEditBoxDropdown(SEditBoxDropdownContext *pDropdown, CLineInput *pLineInput, const CUIRect *pEditBoxRect, int x, float MaxHeight, bool AutoWidth, const std::vector<T> &vData, const FDropdownRenderCallback<T> &pfnMatchCallback)
{
	// Do an edit box with a possible dropdown
	// This is a generic method which can display any data we want

	pDropdown->m_Selected = clamp(pDropdown->m_Selected, -1, (int)vData.size() - 1);

	if(Input()->KeyPress(KEY_SPACE) && Input()->ModifierIsPressed())
	{ // Handle Ctrl+Space to show available options
		pDropdown->m_ShortcutUsed = true;
		// Remove inserted space
		pLineInput->SetRange("", pLineInput->GetCursorOffset() - 1, pLineInput->GetCursorOffset());
	}

	if((!pDropdown->m_ShouldHide && !pLineInput->IsEmpty() && (pLineInput->IsActive() || pDropdown->m_MousePressedInside)) || pDropdown->m_ShortcutUsed)
	{
		if(!pDropdown->m_Visible)
		{
			pDropdown->m_DidBecomeVisible = true;
			pDropdown->m_Visible = true;
		}
		else if(pDropdown->m_DidBecomeVisible)
			pDropdown->m_DidBecomeVisible = false;

		if(!pLineInput->IsEmpty() || !pLineInput->IsActive())
			pDropdown->m_ShortcutUsed = false;

		int CurrentSelected = pDropdown->m_Selected;

		// Use tab to navigate through entries
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_TAB) && !vData.empty())
		{
			int Direction = Input()->ShiftIsPressed() ? -1 : 1;

			pDropdown->m_Selected += Direction;
			if(pDropdown->m_Selected < 0)
				pDropdown->m_Selected = (int)vData.size() - 1;
			pDropdown->m_Selected %= vData.size();
		}

		int Selected = RenderEditBoxDropdown<T>(pDropdown, *pEditBoxRect, pLineInput, x, MaxHeight, AutoWidth, vData, pfnMatchCallback);
		if(Selected != -1)
			pDropdown->m_Selected = Selected;

		if(CurrentSelected != pDropdown->m_Selected)
			pDropdown->m_ListBox.ScrollToSelected();

		return pDropdown->m_Selected;
	}
	else
	{
		pDropdown->m_ShortcutUsed = false;
		pDropdown->m_Visible = false;
		pDropdown->m_ListBox.SetActive(false);
		pDropdown->m_Selected = -1;
	}

	return -1;
}

template<typename T>
int CEditor::RenderEditBoxDropdown(SEditBoxDropdownContext *pDropdown, CUIRect View, CLineInput *pLineInput, int x, float MaxHeight, bool AutoWidth, const std::vector<T> &vData, const FDropdownRenderCallback<T> &pfnMatchCallback)
{
	// Render a dropdown tied to an edit box/line input
	auto *pListBox = &pDropdown->m_ListBox;

	pListBox->SetActive(m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen() && pLineInput->IsActive());
	pListBox->SetScrollbarWidth(15.0f);

	const int NumEntries = vData.size();

	// Setup the rect
	CUIRect CommandsDropdown = View;
	CommandsDropdown.y += View.h + 0.1f;
	CommandsDropdown.x = x;
	if(AutoWidth)
		CommandsDropdown.w = pDropdown->m_Width + pListBox->ScrollbarWidth();

	pListBox->SetActive(NumEntries > 0);
	if(NumEntries > 0)
	{
		// Draw the background
		CommandsDropdown.h = minimum(NumEntries * 15.0f + 1.0f, MaxHeight);
		CommandsDropdown.Draw(ColorRGBA(0.1f, 0.1f, 0.1f, 0.9f), IGraphics::CORNER_ALL, 3.0f);

		if(Ui()->MouseButton(0) && Ui()->MouseInside(&CommandsDropdown))
			pDropdown->m_MousePressedInside = true;

		// Do the list box
		int Selected = pDropdown->m_Selected;
		pListBox->DoStart(15.0f, NumEntries, 1, 3, Selected, &CommandsDropdown);
		CUIRect Label;

		int NewIndex = Selected;
		float LargestWidth = 0;
		for(int i = 0; i < NumEntries; i++)
		{
			const CListboxItem Item = pListBox->DoNextItem(&vData[i], Selected == i);

			Item.m_Rect.VMargin(4.0f, &Label);

			SLabelProperties Props;
			Props.m_MaxWidth = Label.w;
			Props.m_EllipsisAtEnd = true;

			// Call the callback to fill the current line string
			char aBuf[128];
			pfnMatchCallback(vData.at(i), aBuf, Props.m_vColorSplits);

			LargestWidth = maximum(LargestWidth, TextRender()->TextWidth(12.0f, aBuf) + 10.0f);
			if(!Item.m_Visible)
				continue;

			Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML, Props);

			if(Ui()->ActiveItem() == &vData[i])
			{
				// If we selected an item (by clicking on it for example), then set the active item back to the
				// line input so we don't loose focus
				NewIndex = i;
				Ui()->SetActiveItem(pLineInput);
			}
		}

		pDropdown->m_Width = LargestWidth;

		int EndIndex = pListBox->DoEnd();
		if(NewIndex == Selected)
			NewIndex = EndIndex;

		if(pDropdown->m_MousePressedInside && !Ui()->MouseButton(0))
		{
			Ui()->SetActiveItem(pLineInput);
			pDropdown->m_MousePressedInside = false;
		}

		if(NewIndex != Selected)
		{
			Ui()->SetActiveItem(pLineInput);
			return NewIndex;
		}
	}
	return -1;
}

void CEditor::RenderMapSettingsErrorDialog()
{
	auto &LoadedMapSettings = m_MapSettingsBackend.m_LoadedMapSettings;
	auto &vSettingsInvalid = LoadedMapSettings.m_vSettingsInvalid;
	auto &vSettingsValid = LoadedMapSettings.m_vSettingsValid;
	auto &SettingsDuplicate = LoadedMapSettings.m_SettingsDuplicate;

	Ui()->MapScreen();
	CUIRect Overlay = *Ui()->Screen();

	Overlay.Draw(ColorRGBA(0, 0, 0, 0.33f), IGraphics::CORNER_NONE, 0.0f);
	CUIRect Background;
	Overlay.VMargin(150.0f, &Background);
	Background.HMargin(50.0f, &Background);
	Background.Draw(ColorRGBA(0, 0, 0, 0.80f), IGraphics::CORNER_ALL, 5.0f);

	CUIRect View;
	Background.Margin(10.0f, &View);

	CUIRect Title, ButtonBar, Label;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, nullptr, &View); // some spacing
	View.HSplitBottom(18.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, nullptr); // some spacing

	// title bar
	Title.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 4.0f);
	Title.VMargin(10.0f, &Title);
	Ui()->DoLabel(&Title, "Map settings error", 12.0f, TEXTALIGN_ML);

	// Render body
	{
		static CLineInputBuffered<256> s_Input;
		static CMapSettingsBackend::CContext s_Context = m_MapSettingsBackend.NewContext(&s_Input);

		// Some text
		SLabelProperties Props;
		CUIRect Text;
		View.HSplitTop(30.0f, &Text, &View);
		Props.m_MaxWidth = Text.w;
		Ui()->DoLabel(&Text, "Below is a report of the invalid map settings found when loading the map. Please fix them before proceeding further.", 10.0f, TEXTALIGN_MC, Props);

		// Mixed list
		CUIRect List = View;
		View.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 3.0f);

		const float RowHeight = 18.0f;
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&List, &ScrollOffset, &ScrollParams);
		const float EndY = List.y + List.h;
		List.y += ScrollOffset.y;

		List.HSplitTop(20.0f, nullptr, &List);

		static int s_FixingCommandIndex = -1;

		auto &&SetInput = [&](const char *pString) {
			s_Input.Set(pString);
			s_Context.Update();
			s_Context.UpdateCursor(true);
			Ui()->SetActiveItem(&s_Input);
		};

		CUIRect FixInput;
		bool DisplayFixInput = false;
		float DropdownHeight = 110.0f;

		for(int i = 0; i < (int)m_Map.m_vSettings.size(); i++)
		{
			CUIRect Slot;

			auto pInvalidSetting = std::find_if(vSettingsInvalid.begin(), vSettingsInvalid.end(), [i](const SInvalidSetting &Setting) { return Setting.m_Index == i; });
			if(pInvalidSetting != vSettingsInvalid.end())
			{ // This setting is invalid, only display it if its not a duplicate
				if(!(pInvalidSetting->m_Type & SInvalidSetting::TYPE_DUPLICATE))
				{
					bool IsFixing = s_FixingCommandIndex == i;
					List.HSplitTop(RowHeight, &Slot, &List);

					// Draw a reddish background if setting is marked as deleted
					if(pInvalidSetting->m_Context.m_Deleted)
						Slot.Draw(ColorRGBA(0.85f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 3.0f);

					Slot.VMargin(5.0f, &Slot);
					Slot.HMargin(1.0f, &Slot);

					if(!IsFixing && !pInvalidSetting->m_Context.m_Fixed)
					{ // Display "Fix" and "delete" buttons if we're not fixing the command and the command has not been fixed
						CUIRect FixBtn, DelBtn;
						Slot.VSplitRight(30.0f, &Slot, &DelBtn);
						Slot.VSplitRight(5.0f, &Slot, nullptr);
						DelBtn.HMargin(1.0f, &DelBtn);

						Slot.VSplitRight(30.0f, &Slot, &FixBtn);
						Slot.VSplitRight(10.0f, &Slot, nullptr);
						FixBtn.HMargin(1.0f, &FixBtn);

						// Delete button
						if(DoButton_FontIcon(&pInvalidSetting->m_Context.m_Deleted, FONT_ICON_TRASH, pInvalidSetting->m_Context.m_Deleted, &DelBtn, 0, "Delete this command.", IGraphics::CORNER_ALL, 10.0f))
							pInvalidSetting->m_Context.m_Deleted = !pInvalidSetting->m_Context.m_Deleted;

						// Fix button
						if(DoButton_Editor(&pInvalidSetting->m_Context.m_Fixed, "Fix", !pInvalidSetting->m_Context.m_Deleted ? (s_FixingCommandIndex == -1 ? 0 : (IsFixing ? 1 : -1)) : -1, &FixBtn, 0, "Fix this command."))
						{
							s_FixingCommandIndex = i;
							SetInput(pInvalidSetting->m_aSetting);
						}
					}
					else if(IsFixing)
					{ // If we're fixing this command, then display "Done" and "Cancel" buttons
						// Also setup the input rect
						CUIRect OkBtn, CancelBtn;
						Slot.VSplitRight(50.0f, &Slot, &CancelBtn);
						Slot.VSplitRight(5.0f, &Slot, nullptr);
						CancelBtn.HMargin(1.0f, &CancelBtn);

						Slot.VSplitRight(30.0f, &Slot, &OkBtn);
						Slot.VSplitRight(10.0f, &Slot, nullptr);
						OkBtn.HMargin(1.0f, &OkBtn);

						// Buttons
						static int s_Cancel = 0, s_Ok = 0;
						if(DoButton_Editor(&s_Cancel, "Cancel", 0, &CancelBtn, 0, "Cancel fixing this command.") || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
						{
							s_FixingCommandIndex = -1;
							s_Input.Clear();
						}

						// "Done" button only enabled if the fixed setting is valid
						// For that we use a local CContext s_Context and use it to check
						// that the setting is valid and that it is not a duplicate
						ECollisionCheckResult Res = ECollisionCheckResult::ERROR;
						s_Context.CheckCollision(vSettingsValid, Res);
						bool Valid = s_Context.Valid() && Res == ECollisionCheckResult::ADD;

						if(DoButton_Editor(&s_Ok, "Done", Valid ? 0 : -1, &OkBtn, 0, "Confirm editing of this command.") || (s_Input.IsActive() && Valid && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
						{
							// Mark the setting is being fixed
							pInvalidSetting->m_Context.m_Fixed = true;
							str_copy(pInvalidSetting->m_aSetting, s_Input.GetString());
							// Add it to the list for future collision checks
							vSettingsValid.emplace_back(s_Input.GetString());

							// Clear the input & fixing command index
							s_FixingCommandIndex = -1;
							s_Input.Clear();
						}
					}

					Label = Slot;
					Props.m_EllipsisAtEnd = true;
					Props.m_MaxWidth = Label.w;

					if(IsFixing)
					{
						// Setup input rect, which will be used to draw the map settings input later
						Label.HMargin(1.0, &FixInput);
						DisplayFixInput = true;
						DropdownHeight = minimum(DropdownHeight, EndY - FixInput.y - 16.0f);
					}
					else
					{
						// Draw label in case we're not fixing this setting.
						// Deleted settings are shown in gray with a red line through them
						// Fixed settings are shown in green
						// Invalid settings are shown in red
						if(!pInvalidSetting->m_Context.m_Deleted)
						{
							if(pInvalidSetting->m_Context.m_Fixed)
								TextRender()->TextColor(0.0f, 1.0f, 0.0f, 1.0f);
							else
								TextRender()->TextColor(1.0f, 0.0f, 0.0f, 1.0f);
							Ui()->DoLabel(&Label, pInvalidSetting->m_aSetting, 10.0f, TEXTALIGN_ML, Props);
						}
						else
						{
							TextRender()->TextColor(0.3f, 0.3f, 0.3f, 1.0f);
							Ui()->DoLabel(&Label, pInvalidSetting->m_aSetting, 10.0f, TEXTALIGN_ML, Props);

							CUIRect Line = Label;
							Line.y = Label.y + Label.h / 2;
							Line.h = 1;
							Line.Draw(ColorRGBA(1, 0, 0, 1), IGraphics::CORNER_NONE, 0.0f);
						}
					}
					TextRender()->TextColor(TextRender()->DefaultTextColor());
				}
			}
			else
			{ // This setting is valid
				// Check for duplicates
				const std::vector<int> &vDuplicates = SettingsDuplicate.at(i);
				int Chosen = -1; // This is the chosen duplicate setting. -1 means the first valid setting that was found which was not a duplicate
				for(int d = 0; d < (int)vDuplicates.size(); d++)
				{
					int DupIndex = vDuplicates[d];
					if(vSettingsInvalid[DupIndex].m_Context.m_Chosen)
					{
						Chosen = d;
						break;
					}
				}

				List.HSplitTop(RowHeight * (vDuplicates.size() + 1) + 2.0f, &Slot, &List);
				Slot.HMargin(1.0f, &Slot);

				// Draw a background to highlight group of duplicates
				if(!vDuplicates.empty())
					Slot.Draw(ColorRGBA(1, 1, 1, 0.15f), IGraphics::CORNER_ALL, 3.0f);

				Slot.VMargin(5.0f, &Slot);
				Slot.HSplitTop(RowHeight, &Label, &Slot);
				Label.HMargin(1.0f, &Label);

				// Draw a "choose" button next to the label in case we have duplicates for this line
				if(!vDuplicates.empty())
				{
					CUIRect ChooseBtn;
					Label.VSplitRight(50.0f, &Label, &ChooseBtn);
					Label.VSplitRight(5.0f, &Label, nullptr);
					ChooseBtn.HMargin(1.0f, &ChooseBtn);
					if(DoButton_Editor(&vDuplicates, "Choose", Chosen == -1, &ChooseBtn, 0, "Choose this command."))
					{
						if(Chosen != -1)
							vSettingsInvalid[vDuplicates[Chosen]].m_Context.m_Chosen = false;
						Chosen = -1; // Choosing this means that we do not choose any of the duplicates
					}
				}

				// Draw the label
				Props.m_MaxWidth = Label.w;
				Ui()->DoLabel(&Label, m_Map.m_vSettings[i].m_aCommand, 10.0f, TEXTALIGN_ML, Props);

				// Draw the list of duplicates, with a "Choose" button for each duplicate
				// In case a duplicate is also invalid, then we draw a "Fix" button which behaves like the fix button above
				// Duplicate settings name are shown in light blue, or in purple if they are also invalid
				Slot.VSplitLeft(10.0f, nullptr, &Slot);
				for(int DuplicateIndex = 0; DuplicateIndex < (int)vDuplicates.size(); DuplicateIndex++)
				{
					auto &Duplicate = vSettingsInvalid.at(vDuplicates[DuplicateIndex]);
					bool IsFixing = s_FixingCommandIndex == Duplicate.m_Index;
					bool IsInvalid = Duplicate.m_Type & SInvalidSetting::TYPE_INVALID;

					ColorRGBA Color(0.329f, 0.714f, 0.859f, 1.0f);
					CUIRect SubSlot;
					Slot.HSplitTop(RowHeight, &SubSlot, &Slot);
					SubSlot.HMargin(1.0f, &SubSlot);

					if(!IsFixing)
					{
						// If not fixing, then display "Choose" and maybe "Fix" buttons.

						CUIRect ChooseBtn;
						SubSlot.VSplitRight(50.0f, &SubSlot, &ChooseBtn);
						SubSlot.VSplitRight(5.0f, &SubSlot, nullptr);
						ChooseBtn.HMargin(1.0f, &ChooseBtn);
						if(DoButton_Editor(&Duplicate.m_Context.m_Chosen, "Choose", IsInvalid && !Duplicate.m_Context.m_Fixed ? -1 : Duplicate.m_Context.m_Chosen, &ChooseBtn, 0, "Override with this command."))
						{
							Duplicate.m_Context.m_Chosen = !Duplicate.m_Context.m_Chosen;
							if(Chosen != -1 && Chosen != DuplicateIndex)
								vSettingsInvalid[vDuplicates[Chosen]].m_Context.m_Chosen = false;
							Chosen = DuplicateIndex;
						}

						if(IsInvalid)
						{
							if(!Duplicate.m_Context.m_Fixed)
							{
								Color = ColorRGBA(1, 0, 1, 1);
								CUIRect FixBtn;
								SubSlot.VSplitRight(30.0f, &SubSlot, &FixBtn);
								SubSlot.VSplitRight(10.0f, &SubSlot, nullptr);
								FixBtn.HMargin(1.0f, &FixBtn);
								if(DoButton_Editor(&Duplicate.m_Context.m_Fixed, "Fix", s_FixingCommandIndex == -1 ? 0 : (IsFixing ? 1 : -1), &FixBtn, 0, "Fix this command (needed before it can be chosen)."))
								{
									s_FixingCommandIndex = Duplicate.m_Index;
									SetInput(Duplicate.m_aSetting);
								}
							}
							else
							{
								Color = ColorRGBA(0.329f, 0.714f, 0.859f, 1.0f);
							}
						}
					}
					else
					{
						// If we're fixing, display "Done" and "Cancel" buttons
						CUIRect OkBtn, CancelBtn;
						SubSlot.VSplitRight(50.0f, &SubSlot, &CancelBtn);
						SubSlot.VSplitRight(5.0f, &SubSlot, nullptr);
						CancelBtn.HMargin(1.0f, &CancelBtn);

						SubSlot.VSplitRight(30.0f, &SubSlot, &OkBtn);
						SubSlot.VSplitRight(10.0f, &SubSlot, nullptr);
						OkBtn.HMargin(1.0f, &OkBtn);

						static int s_Cancel = 0, s_Ok = 0;
						if(DoButton_Editor(&s_Cancel, "Cancel", 0, &CancelBtn, 0, "Cancel fixing this command.") || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
						{
							s_FixingCommandIndex = -1;
							s_Input.Clear();
						}

						// Use the local CContext s_Context to validate the input
						// We also need to make sure the fixed setting matches the initial duplicate setting
						// For example:
						//   sv_deepfly 0
						//      sv_deepfly 5  <- This is invalid and duplicate. We can only fix it by writing "sv_deepfly 0" or "sv_deepfly 1".
						//                       If we write any other setting, like "sv_hit 1", it won't work as it does not match "sv_deepfly".
						// To do that, we use the context and we check for collision with the current map setting
						ECollisionCheckResult Res = ECollisionCheckResult::ERROR;
						s_Context.CheckCollision({m_Map.m_vSettings[i]}, Res);
						bool Valid = s_Context.Valid() && Res == ECollisionCheckResult::REPLACE;

						if(DoButton_Editor(&s_Ok, "Done", Valid ? 0 : -1, &OkBtn, 0, "Confirm editing of this command.") || (s_Input.IsActive() && Valid && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
						{
							if(Valid) // Just to make sure
							{
								// Mark the setting as fixed
								Duplicate.m_Context.m_Fixed = true;
								str_copy(Duplicate.m_aSetting, s_Input.GetString());

								s_FixingCommandIndex = -1;
								s_Input.Clear();
							}
						}
					}

					Label = SubSlot;
					Props.m_MaxWidth = Label.w;

					if(IsFixing)
					{
						// Setup input rect in case we are fixing the setting
						Label.HMargin(1.0, &FixInput);
						DisplayFixInput = true;
						DropdownHeight = minimum(DropdownHeight, EndY - FixInput.y - 16.0f);
					}
					else
					{
						// Otherwise, render the setting label
						TextRender()->TextColor(Color);
						Ui()->DoLabel(&Label, Duplicate.m_aSetting, 10.0f, TEXTALIGN_ML, Props);
						TextRender()->TextColor(TextRender()->DefaultTextColor());
					}
				}
			}

			// Finally, add the slot to the scroll region
			s_ScrollRegion.AddRect(Slot);
		}

		// Add some padding to the bottom so the dropdown can actually display some values in case we
		// fix an invalid setting at the bottom of the list
		CUIRect PaddingBottom;
		List.HSplitTop(30.0f, &PaddingBottom, &List);
		s_ScrollRegion.AddRect(PaddingBottom);

		// Display the map settings edit box after having rendered all the lines, so the dropdown shows in
		// front of everything, but is still being clipped by the scroll region.
		if(DisplayFixInput)
			DoMapSettingsEditBox(&s_Context, &FixInput, 10.0f, maximum(DropdownHeight, 30.0f));

		s_ScrollRegion.End();
	}

	// Confirm button
	static int s_ConfirmButton = 0, s_CancelButton = 0, s_FixAllButton = 0;
	CUIRect ConfimButton, CancelButton, FixAllUnknownButton;
	ButtonBar.VSplitLeft(110.0f, &CancelButton, &ButtonBar);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &ConfimButton);
	ButtonBar.VSplitRight(5.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(150.0f, &ButtonBar, &FixAllUnknownButton);

	bool CanConfirm = true;
	bool CanFixAllUnknown = false;
	for(auto &InvalidSetting : vSettingsInvalid)
	{
		if(!InvalidSetting.m_Context.m_Fixed && !InvalidSetting.m_Context.m_Deleted && !(InvalidSetting.m_Type & SInvalidSetting::TYPE_DUPLICATE))
		{
			CanConfirm = false;
			if(InvalidSetting.m_Unknown)
				CanFixAllUnknown = true;
			break;
		}
	}

	auto &&Execute = [&]() {
		// Execute will modify the actual map settings according to the fixes that were just made within the dialog.

		// Fix fixed settings, erase deleted settings
		for(auto &FixedSetting : vSettingsInvalid)
		{
			if(FixedSetting.m_Context.m_Fixed)
			{
				str_copy(m_Map.m_vSettings[FixedSetting.m_Index].m_aCommand, FixedSetting.m_aSetting);
			}
		}

		// Choose chosen settings
		// => Erase settings that don't match
		// => Erase settings that were not chosen
		std::vector<CEditorMapSetting> vSettingsToErase;
		for(auto &Setting : vSettingsInvalid)
		{
			if(Setting.m_Type & SInvalidSetting::TYPE_DUPLICATE)
			{
				if(!Setting.m_Context.m_Chosen)
					vSettingsToErase.emplace_back(Setting.m_aSetting);
				else
					vSettingsToErase.emplace_back(m_Map.m_vSettings[Setting.m_CollidingIndex].m_aCommand);
			}
		}

		// Erase deleted settings
		for(auto &DeletedSetting : vSettingsInvalid)
		{
			if(DeletedSetting.m_Context.m_Deleted)
			{
				m_Map.m_vSettings.erase(
					std::remove_if(m_Map.m_vSettings.begin(), m_Map.m_vSettings.end(), [&](const CEditorMapSetting &MapSetting) {
						return str_comp_nocase(MapSetting.m_aCommand, DeletedSetting.m_aSetting) == 0;
					}),
					m_Map.m_vSettings.end());
			}
		}

		// Erase settings to erase
		for(auto &Setting : vSettingsToErase)
		{
			m_Map.m_vSettings.erase(
				std::remove_if(m_Map.m_vSettings.begin(), m_Map.m_vSettings.end(), [&](const CEditorMapSetting &MapSetting) {
					return str_comp_nocase(MapSetting.m_aCommand, Setting.m_aCommand) == 0;
				}),
				m_Map.m_vSettings.end());
		}

		m_Map.OnModify();
	};

	auto &&FixAllUnknown = [&] {
		// Mark unknown settings as fixed
		for(auto &InvalidSetting : vSettingsInvalid)
			if(!InvalidSetting.m_Context.m_Fixed && !InvalidSetting.m_Context.m_Deleted && !(InvalidSetting.m_Type & SInvalidSetting::TYPE_DUPLICATE) && InvalidSetting.m_Unknown)
				InvalidSetting.m_Context.m_Fixed = true;
	};

	// Fix all unknown settings
	if(DoButton_Editor(&s_FixAllButton, "Allow all unknown settings", CanFixAllUnknown ? 0 : -1, &FixAllUnknownButton, 0, nullptr))
	{
		FixAllUnknown();
	}

	// Confirm - execute the fixes
	if(DoButton_Editor(&s_ConfirmButton, "Confirm", CanConfirm ? 0 : -1, &ConfimButton, 0, nullptr) || (CanConfirm && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		Execute();
		m_Dialog = DIALOG_NONE;
	}

	// Cancel - we load a new empty map
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &CancelButton, 0, nullptr) || (Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
	{
		Reset();
		m_aFileName[0] = 0;
		m_Dialog = DIALOG_NONE;
	}
}

void CEditor::MapSettingsDropdownRenderCallback(const SPossibleValueMatch &Match, char (&aOutput)[128], std::vector<STextColorSplit> &vColorSplits)
{
	// Check the match argument index.
	// If it's -1, we're displaying the list of available map settings names
	// If its >= 0, we're displaying the list of possible values matches for that argument
	if(Match.m_ArgIndex == -1)
	{
		IMapSetting *pInfo = (IMapSetting *)Match.m_pData;
		vColorSplits = {
			{str_length(pInfo->m_pName) + 1, -1, ColorRGBA(0.6f, 0.6f, 0.6f, 1)}, // Darker arguments
		};

		if(pInfo->m_Type == IMapSetting::SETTING_INT)
		{
			str_format(aOutput, sizeof(aOutput), "%s i[value]", pInfo->m_pName);
		}
		else if(pInfo->m_Type == IMapSetting::SETTING_COMMAND)
		{
			SMapSettingCommand *pCommand = (SMapSettingCommand *)pInfo;
			str_format(aOutput, sizeof(aOutput), "%s %s", pCommand->m_pName, pCommand->m_pArgs);
		}
	}
	else
	{
		str_copy(aOutput, Match.m_pValue);
	}
}

// ----------------------------------------

CMapSettingsBackend::CContext *CMapSettingsBackend::ms_pActiveContext = nullptr;

void CMapSettingsBackend::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);

	// Register values loader
	InitValueLoaders();

	// Load settings/commands
	LoadAllMapSettings();

	CValuesBuilder Builder(&m_PossibleValuesPerCommand);

	// Load and parse static map settings so we can use them here
	for(auto &pSetting : m_vpMapSettings)
	{
		// We want to parse the arguments of each map setting so we can autocomplete them later
		// But that depends on the type of the setting.
		// If we have a INT setting, then we know we can only ever have 1 argument which is a integer value
		// If we have a COMMAND setting, then we need to parse its arguments
		if(pSetting->m_Type == IMapSetting::SETTING_INT)
			LoadSettingInt(std::static_pointer_cast<SMapSettingInt>(pSetting));
		else if(pSetting->m_Type == IMapSetting::SETTING_COMMAND)
			LoadSettingCommand(std::static_pointer_cast<SMapSettingCommand>(pSetting));

		LoadPossibleValues(Builder(pSetting->m_pName), pSetting);
	}

	// Init constraints
	LoadConstraints();
}

void CMapSettingsBackend::LoadAllMapSettings()
{
	// Gather all config variables having the flag CFGFLAG_GAME
	Editor()->ConfigManager()->PossibleConfigVariables("", CFGFLAG_GAME, PossibleConfigVariableCallback, this);

	// Load list of commands
	LoadCommand("tune", "s[tuning] f[value]", "Tune variable to value or show current value");
	LoadCommand("tune_zone", "i[zone] s[tuning] f[value]", "Tune in zone a variable to value");
	LoadCommand("tune_zone_enter", "i[zone] r[message]", "Which message to display on zone enter; use 0 for normal area");
	LoadCommand("tune_zone_leave", "i[zone] r[message]", "Which message to display on zone leave; use 0 for normal area");
	LoadCommand("mapbug", "s[mapbug]", "Enable map compatibility mode using the specified bug (example: grenade-doubleexplosion@ddnet.tw)");
	LoadCommand("switch_open", "i[switch]", "Whether a switch is deactivated by default (otherwise activated)");
}

void CMapSettingsBackend::LoadCommand(const char *pName, const char *pArgs, const char *pHelp)
{
	m_vpMapSettings.emplace_back(std::make_shared<SMapSettingCommand>(pName, pHelp, pArgs));
}

void CMapSettingsBackend::LoadSettingInt(const std::shared_ptr<SMapSettingInt> &pSetting)
{
	// We load an int argument here
	m_ParsedCommandArgs[pSetting].emplace_back();
	auto &Arg = m_ParsedCommandArgs[pSetting].back();
	str_copy(Arg.m_aName, "value");
	Arg.m_Type = 'i';
}

void CMapSettingsBackend::LoadSettingCommand(const std::shared_ptr<SMapSettingCommand> &pSetting)
{
	// This method parses a setting into its arguments (name and type) so we can later
	// use them to validate the current input as well as display the current argument value
	// over the line input.

	m_ParsedCommandArgs[pSetting].clear();
	const char *pIterator = pSetting->m_pArgs;

	char Type;

	while(*pIterator)
	{
		if(*pIterator == '?') // Skip optional values as a map setting should not have optional values
			pIterator++;

		Type = *pIterator;
		pIterator++;
		while(*pIterator && *pIterator != '[')
			pIterator++;
		pIterator++; // skip '['

		const char *pNameStart = pIterator;

		while(*pIterator && *pIterator != ']')
			pIterator++;

		size_t Len = pIterator - pNameStart;
		pIterator++; // Skip ']'

		dbg_assert(Len + 1 < sizeof(SParsedMapSettingArg::m_aName), "Length of server setting name exceeds limit.");

		// Append parsed arg
		m_ParsedCommandArgs[pSetting].emplace_back();
		auto &Arg = m_ParsedCommandArgs[pSetting].back();
		str_copy(Arg.m_aName, pNameStart, Len + 1);
		Arg.m_Type = Type;

		pIterator = str_skip_whitespaces_const(pIterator);
	}
}

void CMapSettingsBackend::LoadPossibleValues(const CSettingValuesBuilder &Builder, const std::shared_ptr<IMapSetting> &pSetting)
{
	// Call the value loader for that setting
	auto Iter = m_LoaderFunctions.find(pSetting->m_pName);
	if(Iter == m_LoaderFunctions.end())
		return;

	(*Iter->second)(Builder);
}

void CMapSettingsBackend::RegisterLoader(const char *pSettingName, const FLoaderFunction &pfnLoader)
{
	// Registers a value loader function for a specific setting name
	m_LoaderFunctions[pSettingName] = pfnLoader;
}

void CMapSettingsBackend::LoadConstraints()
{
	// Make an instance of constraint builder
	CCommandArgumentConstraintBuilder Command(&m_ArgConstraintsPerCommand);

	// Define constraints like this
	// This is still a bit sad as we have to do it manually here.
	Command("tune", 2).Unique(0);
	Command("tune_zone", 3).Multiple(0).Unique(1);
	Command("tune_zone_enter", 2).Unique(0);
	Command("tune_zone_leave", 2).Unique(0);
	Command("switch_open", 1).Unique(0);
	Command("mapbug", 1).Unique(0);
}

void CMapSettingsBackend::PossibleConfigVariableCallback(const SConfigVariable *pVariable, void *pUserData)
{
	CMapSettingsBackend *pBackend = (CMapSettingsBackend *)pUserData;

	if(pVariable->m_Type == SConfigVariable::VAR_INT)
	{
		SIntConfigVariable *pIntVariable = (SIntConfigVariable *)pVariable;
		pBackend->m_vpMapSettings.emplace_back(std::make_shared<SMapSettingInt>(
			pIntVariable->m_pScriptName,
			pIntVariable->m_pHelp,
			pIntVariable->m_Default,
			pIntVariable->m_Min,
			pIntVariable->m_Max));
	}
}

void CMapSettingsBackend::CContext::Reset()
{
	m_LastCursorOffset = 0;
	m_CursorArgIndex = -1;
	m_pCurrentSetting = nullptr;
	m_vCurrentArgs.clear();
	m_aCommand[0] = '\0';
	m_DropdownContext.m_Selected = -1;
	m_CurrentCompletionIndex = -1;
	m_DropdownContext.m_ShortcutUsed = false;
	m_DropdownContext.m_MousePressedInside = false;
	m_DropdownContext.m_Visible = false;
	m_DropdownContext.m_ShouldHide = false;
	m_CommentOffset = -1;

	ClearError();
}

void CMapSettingsBackend::CContext::Update()
{
	UpdateFromString(InputString());
}

void CMapSettingsBackend::CContext::UpdateFromString(const char *pStr)
{
	// This is the main method that does all the argument parsing and validating.
	// It fills pretty much all the context values, the arguments, their position,
	// if they are valid or not, etc.

	m_pCurrentSetting = nullptr;
	m_vCurrentArgs.clear();
	m_CommentOffset = -1;

	const char *pIterator = pStr;

	// Check for comment
	const char *pEnd = pStr;
	int InString = 0;

	while(*pEnd)
	{
		if(*pEnd == '"')
			InString ^= 1;
		else if(*pEnd == '\\') // Escape sequences
		{
			if(pEnd[1] == '"')
				pEnd++;
		}
		else if(!InString)
		{
			if(*pEnd == '#') // Found comment
			{
				m_CommentOffset = pEnd - pStr;
				break;
			}
		}

		pEnd++;
	}

	if(m_CommentOffset == 0)
		return;

	// End command at start of comment, if any
	char aInputString[256];
	str_copy(aInputString, pStr, m_CommentOffset != -1 ? m_CommentOffset + 1 : sizeof(aInputString));
	pIterator = aInputString;

	// Get the command/setting
	m_aCommand[0] = '\0';
	while(pIterator && *pIterator != ' ' && *pIterator != '\0')
		pIterator++;

	str_copy(m_aCommand, aInputString, (pIterator - aInputString) + 1);

	// Get the command if it is a recognized one
	for(auto &pSetting : m_pBackend->m_vpMapSettings)
	{
		if(str_comp_nocase(m_aCommand, pSetting->m_pName) == 0)
		{
			m_pCurrentSetting = pSetting;
			break;
		}
	}

	// Parse args
	ParseArgs(aInputString, pIterator);
}

void CMapSettingsBackend::CContext::ParseArgs(const char *pLineInputStr, const char *pStr)
{
	// This method parses the arguments of the current command, starting at pStr

	ClearError();

	const char *pIterator = pStr;

	if(!pStr || *pStr == '\0')
		return; // No arguments

	// NextArg is used to get the contents of the current argument and go to the next argument position
	// It outputs the length of the argument in pLength and returns a boolean indicating if the parsing
	// of that argument is valid or not (only the case when using strings with quotes ("))
	auto &&NextArg = [&](const char *pArg, int *pLength) {
		if(*pIterator == '"')
		{
			pIterator++;
			bool Valid = true;
			bool IsEscape = false;

			while(true)
			{
				if(pIterator[0] == '"' && !IsEscape)
					break;
				else if(pIterator[0] == 0)
				{
					Valid = false;
					break;
				}

				if(pIterator[0] == '\\' && !IsEscape)
					IsEscape = true;
				else if(IsEscape)
					IsEscape = false;

				pIterator++;
			}
			const char *pEnd = ++pIterator;
			pIterator = str_skip_to_whitespace_const(pIterator);

			// Make sure there are no other characters at the end, otherwise the string is invalid.
			// E.g. "abcd"ef is invalid
			Valid = Valid && pIterator == pEnd;
			*pLength = pEnd - pArg;

			return Valid;
		}
		else
		{
			pIterator = str_skip_to_whitespace_const(pIterator);
			*pLength = pIterator - pArg;
			return true;
		}
	};

	// Simple validation of string. Checks that it does not contain unescaped " in the middle of it.
	auto &&ValidateStr = [](const char *pString) -> bool {
		const char *pIt = pString;
		bool IsEscape = false;
		while(*pIt)
		{
			if(pIt[0] == '"' && !IsEscape)
				return false;

			if(pIt[0] == '\\' && !IsEscape)
				IsEscape = true;
			else if(IsEscape)
				IsEscape = false;

			pIt++;
		}
		return true;
	};

	const int CommandArgCount = m_pCurrentSetting != nullptr ? m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting).size() : 0;
	int ArgIndex = 0;
	SCommandParseError::EErrorType Error = SCommandParseError::ERROR_NONE;

	// Also keep track of the visual X position of each argument within the input
	float PosX = 0;
	const float WW = m_pBackend->TextRender()->TextWidth(m_FontSize, " ");
	PosX += m_pBackend->TextRender()->TextWidth(m_FontSize, m_aCommand);

	// Parsing beings
	while(*pIterator)
	{
		Error = SCommandParseError::ERROR_NONE;
		pIterator++; // Skip whitespace
		PosX += WW; // Add whitespace width

		// Insert argument here
		char Char = *pIterator;
		const char *pArgStart = pIterator;
		int Length;
		bool Valid = NextArg(pArgStart, &Length); // Get contents and go to next argument position
		size_t Offset = pArgStart - pLineInputStr; // Compute offset from the start of the input

		// Add new argument, copy the argument contents
		m_vCurrentArgs.emplace_back();
		auto &NewArg = m_vCurrentArgs.back();
		// Fill argument value, with a maximum length of 256
		str_copy(NewArg.m_aValue, pArgStart, minimum((int)sizeof(SCurrentSettingArg::m_aValue), Length + 1));

		// Validate argument from the parsed argument of the current setting.
		// If current setting is not valid, then there are no arguments which results in an error.

		char Type = 'u'; // u = unknown
		if(ArgIndex < CommandArgCount)
		{
			SParsedMapSettingArg &Arg = m_pBackend->m_ParsedCommandArgs[m_pCurrentSetting].at(ArgIndex);
			if(Arg.m_Type == 'r')
			{
				// Rest of string, should add all the string if there was no quotes
				// Otherwise, only get the contents in the quotes, and consider content after that as other arguments
				if(Char != '"')
				{
					while(*pIterator)
						pIterator++;
					Length = pIterator - pArgStart;
					str_copy(NewArg.m_aValue, pArgStart, Length + 1);
				}

				if(!Valid)
					Error = SCommandParseError::ERROR_INVALID_VALUE;
			}
			else if(Arg.m_Type == 'i')
			{
				// Validate int
				if(!str_toint(NewArg.m_aValue, nullptr))
					Error = SCommandParseError::ERROR_INVALID_VALUE;
			}
			else if(Arg.m_Type == 'f')
			{
				// Validate float
				if(!str_tofloat(NewArg.m_aValue, nullptr))
					Error = SCommandParseError::ERROR_INVALID_VALUE;
			}
			else if(Arg.m_Type == 's')
			{
				// Validate string
				if(!Valid || (Char != '"' && !ValidateStr(NewArg.m_aValue)))
					Error = SCommandParseError::ERROR_INVALID_VALUE;
			}

			// Extended argument validation:
			//   for int settings it checks that the value is in range
			//   for command settings, it checks that the value is one of the possible values if there are any
			EValidationResult Result = ValidateArg(ArgIndex, NewArg.m_aValue);
			if(Length && !Error && Result != EValidationResult::VALID)
			{
				if(Result == EValidationResult::ERROR)
					Error = SCommandParseError::ERROR_INVALID_VALUE; // Invalid argument value (invalid int, invalid float)
				else if(Result == EValidationResult::UNKNOWN)
					Error = SCommandParseError::ERROR_UNKNOWN_VALUE; // Unknown argument value
				else if(Result == EValidationResult::INCOMPLETE)
					Error = SCommandParseError::ERROR_INCOMPLETE; // Incomplete argument in case of possible values
				else if(Result == EValidationResult::OUT_OF_RANGE)
					Error = SCommandParseError::ERROR_OUT_OF_RANGE; // Out of range argument value in case of int settings
				else
					Error = SCommandParseError::ERROR_UNKNOWN; // Unknown error
			}

			Type = Arg.m_Type;
		}
		else
		{
			// Error: too many arguments if no comment after
			if(m_CommentOffset == -1)
				Error = SCommandParseError::ERROR_TOO_MANY_ARGS;
			else
			{ // Otherwise, check if there are any arguments left between this argument and the comment
				const char *pSubIt = pArgStart;
				pSubIt = str_skip_whitespaces_const(pSubIt);
				if(*pSubIt != '\0')
				{ // If there aren't only spaces between the last argument and the comment, then this is an error
					Error = SCommandParseError::ERROR_TOO_MANY_ARGS;
				}
				else // If there are, then just exit the loop to avoid getting an error
				{
					m_vCurrentArgs.pop_back();
					break;
				}
			}
		}

		// Fill argument informations
		NewArg.m_X = PosX;
		NewArg.m_Start = Offset;
		NewArg.m_End = Offset + Length;
		NewArg.m_Error = Error != SCommandParseError::ERROR_NONE || Length == 0 || m_Error.m_Type != SCommandParseError::ERROR_NONE;
		NewArg.m_ExpectedType = Type;

		// Check error and fill the error field with different messages
		if(Error == SCommandParseError::ERROR_INVALID_VALUE || Error == SCommandParseError::ERROR_UNKNOWN_VALUE || Error == SCommandParseError::ERROR_OUT_OF_RANGE || Error == SCommandParseError::ERROR_INCOMPLETE)
		{
			// Only keep first error
			if(!m_Error.m_aMessage[0])
			{
				int ErrorArgIndex = (int)m_vCurrentArgs.size() - 1;
				SCurrentSettingArg &ErrorArg = m_vCurrentArgs.back();
				SParsedMapSettingArg &SettingArg = m_pBackend->m_ParsedCommandArgs[m_pCurrentSetting].at(ArgIndex);
				char aFormattedValue[256];
				FormatDisplayValue(ErrorArg.m_aValue, aFormattedValue);

				if(Error == SCommandParseError::ERROR_INVALID_VALUE || Error == SCommandParseError::ERROR_UNKNOWN_VALUE || Error == SCommandParseError::ERROR_INCOMPLETE)
				{
					static const std::map<int, const char *> s_Names = {
						{SCommandParseError::ERROR_INVALID_VALUE, "Invalid"},
						{SCommandParseError::ERROR_UNKNOWN_VALUE, "Unknown"},
						{SCommandParseError::ERROR_INCOMPLETE, "Incomplete"},
					};
					str_format(m_Error.m_aMessage, sizeof(m_Error.m_aMessage), "%s argument value: %s at position %d for argument '%s'", s_Names.at(Error), aFormattedValue, (int)ErrorArg.m_Start, SettingArg.m_aName);
				}
				else
				{
					std::shared_ptr<SMapSettingInt> pSettingInt = std::static_pointer_cast<SMapSettingInt>(m_pCurrentSetting);
					str_format(m_Error.m_aMessage, sizeof(m_Error.m_aMessage), "Invalid argument value: %s at position %d for argument '%s': out of range [%d, %d]", aFormattedValue, (int)ErrorArg.m_Start, SettingArg.m_aName, pSettingInt->m_Min, pSettingInt->m_Max);
				}
				m_Error.m_ArgIndex = ErrorArgIndex;
				m_Error.m_Type = Error;
			}
		}
		else if(Error == SCommandParseError::ERROR_TOO_MANY_ARGS)
		{
			// Only keep first error
			if(!m_Error.m_aMessage[0])
			{
				if(m_pCurrentSetting != nullptr)
				{
					str_copy(m_Error.m_aMessage, "Too many arguments");
					m_Error.m_ArgIndex = ArgIndex;
					break;
				}
				else
				{
					char aFormattedValue[256];
					FormatDisplayValue(m_aCommand, aFormattedValue);
					str_format(m_Error.m_aMessage, sizeof(m_Error.m_aMessage), "Unknown server setting: %s", aFormattedValue);
					m_Error.m_ArgIndex = -1;
					break;
				}
				m_Error.m_Type = Error;
			}
		}

		PosX += m_pBackend->TextRender()->TextWidth(m_FontSize, pArgStart, Length); // Advance argument position
		ArgIndex++;
	}
}

void CMapSettingsBackend::CContext::ClearError()
{
	m_Error.m_aMessage[0] = '\0';
	m_Error.m_Type = SCommandParseError::ERROR_NONE;
}

bool CMapSettingsBackend::CContext::UpdateCursor(bool Force)
{
	// This method updates the cursor offset in this class from
	// the cursor offset of the line input.
	// It also updates the argument index where the cursor is at
	// and the possible values matches if the argument index changes.
	// Returns true in case the cursor changed position

	if(!m_pLineInput)
		return false;

	size_t Offset = m_pLineInput->GetCursorOffset();
	if(Offset == m_LastCursorOffset && !Force)
		return false;

	m_LastCursorOffset = Offset;
	int NewArg = m_CursorArgIndex;

	// Update current argument under cursor
	if(m_CommentOffset != -1 && Offset >= (size_t)m_CommentOffset)
	{
		NewArg = (int)m_vCurrentArgs.size();
	}
	else
	{
		bool FoundArg = false;
		for(int i = (int)m_vCurrentArgs.size() - 1; i >= 0; i--)
		{
			if(Offset >= m_vCurrentArgs[i].m_Start)
			{
				NewArg = i;
				FoundArg = true;
				break;
			}
		}

		if(!FoundArg)
			NewArg = -1;
	}

	bool ShouldUpdate = NewArg != m_CursorArgIndex;
	m_CursorArgIndex = NewArg;

	// Do not show error if current argument is incomplete, as we are editing it
	if(m_pLineInput != nullptr)
	{
		if(Offset == m_pLineInput->GetLength() && m_Error.m_aMessage[0] && m_Error.m_ArgIndex == m_CursorArgIndex && m_Error.m_Type == SCommandParseError::ERROR_INCOMPLETE)
			ClearError();
	}

	if(m_DropdownContext.m_Selected == -1 || ShouldUpdate || Force)
	{
		// Update possible commands from cursor
		UpdatePossibleMatches();
	}

	return true;
}

EValidationResult CMapSettingsBackend::CContext::ValidateArg(int Index, const char *pArg)
{
	if(!m_pCurrentSetting)
		return EValidationResult::ERROR;

	// Check if this argument is valid against current argument
	if(m_pCurrentSetting->m_Type == IMapSetting::SETTING_INT)
	{
		std::shared_ptr<SMapSettingInt> pSetting = std::static_pointer_cast<SMapSettingInt>(m_pCurrentSetting);
		if(Index > 0)
			return EValidationResult::ERROR;

		int Value;
		if(!str_toint(pArg, &Value)) // Try parse the integer
			return EValidationResult::ERROR;

		return Value >= pSetting->m_Min && Value <= pSetting->m_Max ? EValidationResult::VALID : EValidationResult::OUT_OF_RANGE;
	}
	else if(m_pCurrentSetting->m_Type == IMapSetting::SETTING_COMMAND)
	{
		auto &vArgs = m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting);
		if(Index < (int)vArgs.size())
		{
			auto It = m_pBackend->m_PossibleValuesPerCommand.find(m_pCurrentSetting->m_pName);
			if(It != m_pBackend->m_PossibleValuesPerCommand.end())
			{
				auto ValuesIt = It->second.find(Index);
				if(ValuesIt != It->second.end())
				{
					// This means that we have possible values for this argument for this setting
					// In order to validate such arg, we have to check if it maches any of the possible values
					const bool EqualsAny = std::any_of(ValuesIt->second.begin(), ValuesIt->second.end(), [pArg](auto *pValue) { return str_comp_nocase(pArg, pValue) == 0; });

					// If equals, then argument is valid
					if(EqualsAny)
						return EValidationResult::VALID;

					// Here we check if argument is incomplete
					const bool StartsAny = std::any_of(ValuesIt->second.begin(), ValuesIt->second.end(), [pArg](auto *pValue) { return str_startswith_nocase(pValue, pArg) != nullptr; });
					if(StartsAny)
						return EValidationResult::INCOMPLETE;

					return EValidationResult::UNKNOWN;
				}
			}
		}

		// If we get here, it means there are no posssible values for that specific argument.
		// The validation for specific types such as int and floats were done earlier so if we get here
		// we know the argument is valid.
		// String and "rest of string" types are valid by default.
		return EValidationResult::VALID;
	}

	return EValidationResult::ERROR;
}

void CMapSettingsBackend::CContext::UpdatePossibleMatches()
{
	// This method updates the possible values matches based on the cursor position within the current argument in the line input.
	// For example ("|" is the cursor):
	// - Typing "sv_deep|" will show "sv_deepfly" as a possible match in the dropdown
	//   Moving the cursor: "sv_|deep" will show all possible commands starting with "sv_"
	// - Typing "tune ground_frict|" will show "ground_friction" as possible match
	//   Moving the cursor: "tune ground_|frict" will show all possible values starting with "ground_" for that argument (argument 0 of "tune" setting)

	m_vPossibleMatches.clear();
	m_DropdownContext.m_Selected = -1;

	if(m_CommentOffset == 0 || (m_aCommand[0] == '\0' && !m_DropdownContext.m_ShortcutUsed))
		return;

	// First case: argument index under cursor is -1 => we're on the command/setting name
	if(m_CursorArgIndex == -1)
	{
		// Use a substring from the start of the input to the cursor offset
		char aSubString[128];
		str_copy(aSubString, m_aCommand, minimum(m_LastCursorOffset + 1, sizeof(aSubString)));

		// Iterate through available map settings and find those which the beginning matches with the command/setting name we are writing
		for(auto &pSetting : m_pBackend->m_vpMapSettings)
		{
			if(str_startswith_nocase(pSetting->m_pName, aSubString))
			{
				m_vPossibleMatches.emplace_back(SPossibleValueMatch{
					pSetting->m_pName,
					m_CursorArgIndex,
					pSetting.get(),
				});
			}
		}

		// If there are no matches, then the command is unknown
		if(m_vPossibleMatches.empty())
		{
			// Fill the error if we do not allow unknown commands
			char aFormattedValue[256];
			FormatDisplayValue(m_aCommand, aFormattedValue);
			str_format(m_Error.m_aMessage, sizeof(m_Error.m_aMessage), "Unknown server setting: %s", aFormattedValue);
			m_Error.m_ArgIndex = -1;
		}
	}
	else
	{
		// Second case: we are on an argument
		if(!m_pCurrentSetting) // If we are on an argument of an unknown setting, we can't handle it => no possible values, ever.
			return;

		if(m_pCurrentSetting->m_Type == IMapSetting::SETTING_INT)
		{
			// No possible values for int settings.
			// Maybe we can add "0" and "1" as possible values for settings that are binary.
		}
		else
		{
			// Get the parsed arguments for the current setting
			auto &vArgs = m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting);
			// Make sure we are not out of bounds
			if(m_CursorArgIndex < (int)vArgs.size() && m_CursorArgIndex < (int)m_vCurrentArgs.size())
			{
				// Check if there are possible values for this command
				auto It = m_pBackend->m_PossibleValuesPerCommand.find(m_pCurrentSetting->m_pName);
				if(It != m_pBackend->m_PossibleValuesPerCommand.end())
				{
					// If that's the case, then check if there are possible values for the current argument index the cursor is on
					auto ValuesIt = It->second.find(m_CursorArgIndex);
					if(ValuesIt != It->second.end())
					{
						// If that's the case, then do the same as previously, we check for each value if they match
						// with the current argument value

						auto &CurrentArg = m_vCurrentArgs.at(m_CursorArgIndex);
						int SubstringLength = minimum(m_LastCursorOffset, CurrentArg.m_End) - CurrentArg.m_Start;

						// Substring based on the cursor position inside that argument
						char aSubString[256];
						str_copy(aSubString, CurrentArg.m_aValue, SubstringLength + 1);

						for(auto &pValue : ValuesIt->second)
						{
							if(str_startswith_nocase(pValue, aSubString))
							{
								m_vPossibleMatches.emplace_back(SPossibleValueMatch{
									pValue,
									m_CursorArgIndex,
									nullptr,
								});
							}
						}
					}
				}
			}
		}
	}
}

bool CMapSettingsBackend::CContext::OnInput(const IInput::CEvent &Event)
{
	if(!m_pLineInput)
		return false;

	if(!m_pLineInput->IsActive())
		return false;

	if(Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_TEXT) && !m_pBackend->Input()->ModifierIsPressed() && !m_pBackend->Input()->AltIsPressed())
	{
		// How to make this better?
		// This checks when we press any key that is not handled by the dropdown
		// When that's the case, it means we confirm the completion if we have a valid completion index
		if(Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT && Event.m_Key != KEY_UP && Event.m_Key != KEY_DOWN && !(Event.m_Key >= KEY_MOUSE_1 && Event.m_Key <= KEY_MOUSE_WHEEL_RIGHT))
		{
			if(m_CurrentCompletionIndex != -1)
			{
				m_CurrentCompletionIndex = -1;
				m_DropdownContext.m_Selected = -1;
				Update();
				UpdateCursor(true);
			}
		}
	}

	return false;
}

const char *CMapSettingsBackend::CContext::InputString() const
{
	if(!m_pLineInput)
		return nullptr;
	return m_pBackend->Input()->HasComposition() ? m_CompositionStringBuffer.c_str() : m_pLineInput->GetString();
}

const ColorRGBA CMapSettingsBackend::CContext::ms_ArgumentStringColor = ColorRGBA(84 / 255.0f, 1.0f, 1.0f, 1.0f);
const ColorRGBA CMapSettingsBackend::CContext::ms_ArgumentNumberColor = ColorRGBA(0.1f, 0.9f, 0.05f, 1.0f);
const ColorRGBA CMapSettingsBackend::CContext::ms_ArgumentUnknownColor = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f);
const ColorRGBA CMapSettingsBackend::CContext::ms_CommentColor = ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f);
const ColorRGBA CMapSettingsBackend::CContext::ms_ErrorColor = ColorRGBA(240 / 255.0f, 70 / 255.0f, 70 / 255.0f, 1.0f);

void CMapSettingsBackend::CContext::ColorArguments(std::vector<STextColorSplit> &vColorSplits) const
{
	// Get argument color based on its type
	auto &&GetArgumentColor = [](char Type) -> ColorRGBA {
		if(Type == 'u')
			return ms_ArgumentUnknownColor;
		else if(Type == 's' || Type == 'r')
			return ms_ArgumentStringColor;
		else if(Type == 'i' || Type == 'f')
			return ms_ArgumentNumberColor;
		return ms_ErrorColor; // Invalid arg type
	};

	// Iterate through all the current arguments and color them
	for(int i = 0; i < ArgCount(); i++)
	{
		const auto &Argument = Arg(i);
		// Color is based on the error flag and the type of the argument
		auto Color = Argument.m_Error ? ms_ErrorColor : GetArgumentColor(Argument.m_ExpectedType);
		vColorSplits.emplace_back(Argument.m_Start, Argument.m_End - Argument.m_Start, Color);
	}

	if(m_pLineInput && !m_pLineInput->IsEmpty())
	{
		if(!CommandIsValid() && m_CommentOffset != 0)
		{
			// If command is invalid, override color splits with red, but not comment
			int ErrorLength = m_CommentOffset == -1 ? -1 : m_CommentOffset;
			vColorSplits = {{0, ErrorLength, ms_ErrorColor}};
		}
		else if(HasError())
		{
			// If there is an error, then color the wrong part of the input, excluding comment
			int ErrorLength = m_CommentOffset == -1 ? -1 : m_CommentOffset - ErrorOffset();
			vColorSplits.emplace_back(ErrorOffset(), ErrorLength, ms_ErrorColor);
		}
		if(m_CommentOffset != -1)
		{ // Color comment if there is one
			vColorSplits.emplace_back(m_CommentOffset, -1, ms_CommentColor);
		}
	}

	std::sort(vColorSplits.begin(), vColorSplits.end(), [](const STextColorSplit &a, const STextColorSplit &b) {
		return a.m_CharIndex < b.m_CharIndex;
	});
}

int CMapSettingsBackend::CContext::CheckCollision(ECollisionCheckResult &Result) const
{
	return CheckCollision(m_pBackend->Editor()->m_Map.m_vSettings, Result);
}

int CMapSettingsBackend::CContext::CheckCollision(const std::vector<CEditorMapSetting> &vSettings, ECollisionCheckResult &Result) const
{
	return CheckCollision(InputString(), vSettings, Result);
}

int CMapSettingsBackend::CContext::CheckCollision(const char *pInputString, const std::vector<CEditorMapSetting> &vSettings, ECollisionCheckResult &Result) const
{
	// Checks for a collision with the current map settings.
	// A collision is when a setting with the same arguments already exists and that it can't be added multiple times.
	// For this, we use argument constraints that we define in CMapSettingsCommandObject::LoadConstraints().
	// For example, the "tune" command can be added multiple times, but only if the actual tune argument is different, thus
	//   the tune argument must be defined as UNIQUE.
	// This method CheckCollision(ECollisionCheckResult&) returns an integer which is the index of the colliding line. If no
	//   colliding line was found, then it returns -1.

	const int InputLength = str_length(pInputString);

	if(m_CommentOffset == 0 || InputLength == 0)
	{ // Ignore comments
		Result = ECollisionCheckResult::ADD;
		return -1;
	}

	struct SArgument
	{
		char m_aValue[128];
		SArgument(const char *pStr)
		{
			str_copy(m_aValue, pStr);
		}
	};

	struct SLineArgs
	{
		int m_Index;
		std::vector<SArgument> m_vArgs;
	};

	// For now we split each map setting corresponding to the setting we want to add by spaces
	auto &&SplitSetting = [](const char *pStr) {
		std::vector<SArgument> vaArgs;
		const char *pIt = pStr;
		char aBuffer[128];
		while((pIt = str_next_token(pIt, " ", aBuffer, sizeof(aBuffer))))
			vaArgs.emplace_back(aBuffer);
		return vaArgs;
	};

	// Define the result of the check
	Result = ECollisionCheckResult::ERROR;

	// First case: the command is not a valid (recognized) command.
	if(!CommandIsValid())
	{
		// If we don't allow unknown commands, then we know there is no collision
		// and the check results in an error.
		if(!m_AllowUnknownCommands)
			return -1;

		// If we get here, it means we allow unknown commands.
		// For them, we need to check if a similar exact command exists or not in the settings list.
		// If it does, then we found a collision, and the result is REPLACE.
		for(int i = 0; i < (int)vSettings.size(); i++)
		{
			if(str_comp_nocase(vSettings[i].m_aCommand, pInputString) == 0)
			{
				Result = ECollisionCheckResult::REPLACE;
				return i;
			}
		}

		// If nothing was found, then we must ensure that the command, although unknown, is somewhat valid
		// by checking if the command contains a space and that there is at least one non-empty argument.
		const char *pSpace = str_find(pInputString, " ");
		if(!pSpace || !*(pSpace + 1))
			Result = ECollisionCheckResult::ERROR;
		else
			Result = ECollisionCheckResult::ADD;

		return -1; // No collision
	}

	// Second case: the command is valid.
	// In this case, we know we have a valid setting name, which means we can use everything we have in this class which are
	// related to valid map settings, such as parsed command arguments, etc.

	const std::shared_ptr<IMapSetting> &pSetting = Setting();
	if(pSetting->m_Type == IMapSetting::SETTING_INT)
	{
		// For integer settings, the check is quite simple as we know
		// we can only ever have 1 argument.

		// The integer setting cannot be added multiple times, which means if a collision was found, then the only result we
		// can have is REPLACE.
		// In this case, the collision is found only by checking the command name for every setting in the current map settings.
		char aBuffer[256];
		auto It = std::find_if(vSettings.begin(), vSettings.end(), [&](const CEditorMapSetting &Setting) {
			const char *pLineSettingValue = Setting.m_aCommand; // Get the map setting command
			pLineSettingValue = str_next_token(pLineSettingValue, " ", aBuffer, sizeof(aBuffer)); // Get the first token before the first space
			return str_comp_nocase(aBuffer, pSetting->m_pName) == 0; // Check if that equals our current command
		});

		if(It == vSettings.end())
		{
			// If nothing was found, then there is no collision and we can add that command to the list
			Result = ECollisionCheckResult::ADD;
			return -1;
		}
		else
		{
			// Otherwise, we can only replace it
			Result = ECollisionCheckResult::REPLACE;
			return It - vSettings.begin(); // This is the index of the colliding line
		}
	}
	else if(pSetting->m_Type == IMapSetting::SETTING_COMMAND)
	{
		// For "command" settings, this is a bit more complex as we have to use argument constraints.
		// The general idea is to split every map setting in their arguments separated by spaces.
		// Then, for each argument, we check if it collides with any of the map settings. When that's the case,
		//   we need to check the constraint of the argument. If set to UNIQUE, then that's a collision and we can only
		//   replace the command in the list.
		// If set to anything else, we consider that it is not a collision and we move to the next argument.
		// This system is simple and somewhat flexible as we only need to declare the constraints, the rest should be
		//   handled automatically.

		std::shared_ptr<SMapSettingCommand> pSettingCommand = std::static_pointer_cast<SMapSettingCommand>(pSetting);
		// Get matching lines for that command
		std::vector<SLineArgs> vLineArgs;
		for(int i = 0; i < (int)vSettings.size(); i++)
		{
			const auto &Setting = vSettings.at(i);

			// Split this setting into its arguments
			std::vector<SArgument> vArgs = SplitSetting(Setting.m_aCommand);
			// Only keep settings that match with the current input setting name
			if(!vArgs.empty() && str_comp_nocase(vArgs[0].m_aValue, pSettingCommand->m_pName) == 0)
			{
				// When that's the case, we save them
				vArgs.erase(vArgs.begin());
				vLineArgs.push_back(SLineArgs{
					i,
					vArgs,
				});
			}
		}

		// Here is the simple algorithm to check for collisions according to argument constraints
		bool Error = false;
		int CollidingLineIndex = -1;
		for(int ArgIndex = 0; ArgIndex < ArgCount(); ArgIndex++)
		{
			bool Collide = false;
			const char *pValue = Arg(ArgIndex).m_aValue;
			for(auto &Line : vLineArgs)
			{
				// Check first colliding line
				if(str_comp_nocase(pValue, Line.m_vArgs[ArgIndex].m_aValue) == 0)
				{
					Collide = true;
					CollidingLineIndex = Line.m_Index;
					Error = m_pBackend->ArgConstraint(pSetting->m_pName, ArgIndex) == CMapSettingsBackend::EArgConstraint::UNIQUE;
				}
				if(Error)
					break;
			}

			// If we did not collide with any of the lines for that argument, we're good to go
			// (or if we had an error)
			if(!Collide || Error)
				break;

			// Otherwise, remove non-colliding args from the list
			vLineArgs.erase(
				std::remove_if(vLineArgs.begin(), vLineArgs.end(), [&](const SLineArgs &Line) {
					return str_comp_nocase(pValue, Line.m_vArgs[ArgIndex].m_aValue) != 0;
				}),
				vLineArgs.end());
		}

		// The result is either REPLACE when we found a collision, or ADD
		Result = Error ? ECollisionCheckResult::REPLACE : ECollisionCheckResult::ADD;
		return CollidingLineIndex;
	}

	return -1;
}

bool CMapSettingsBackend::CContext::Valid() const
{
	// Check if the entire setting is valid or not

	// We don't need to check whether a command is valid if we allow unknown commands
	if(m_AllowUnknownCommands)
		return true;

	if(m_CommentOffset == 0 || m_aCommand[0] == '\0')
		return true; // A "comment" setting is considered valid.

	// Check if command is valid
	if(m_pCurrentSetting)
	{
		// Check if all arguments are valid
		const bool ArgumentsValid = std::all_of(m_vCurrentArgs.begin(), m_vCurrentArgs.end(), [](const SCurrentSettingArg &Arg) {
			return !Arg.m_Error;
		});

		if(!ArgumentsValid)
			return false;

		// Check that we have the same number of arguments
		return m_vCurrentArgs.size() == m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting).size();
	}
	else
	{
		return false;
	}
}

void CMapSettingsBackend::CContext::GetCommandHelpText(char *pStr, int Length) const
{
	if(!m_pCurrentSetting)
		return;

	str_copy(pStr, m_pCurrentSetting->m_pHelp, Length);
}

void CMapSettingsBackend::CContext::UpdateCompositionString()
{
	if(!m_pLineInput)
		return;

	const bool HasComposition = m_pBackend->Input()->HasComposition();

	if(HasComposition)
	{
		const size_t CursorOffset = m_pLineInput->GetCursorOffset();
		const size_t DisplayCursorOffset = m_pLineInput->OffsetFromActualToDisplay(CursorOffset);
		const std::string DisplayStr = std::string(m_pLineInput->GetString());
		std::string CompositionBuffer = DisplayStr.substr(0, DisplayCursorOffset) + m_pBackend->Input()->GetComposition() + DisplayStr.substr(DisplayCursorOffset);
		if(CompositionBuffer != m_CompositionStringBuffer)
		{
			m_CompositionStringBuffer = CompositionBuffer;
			Update();
			UpdateCursor();
		}
	}
}

template<int N>
void CMapSettingsBackend::CContext::FormatDisplayValue(const char *pValue, char (&aOut)[N])
{
	const int MaxLength = 32;
	if(str_length(pValue) > MaxLength)
	{
		str_copy(aOut, pValue, MaxLength);
		str_append(aOut, "...");
	}
	else
	{
		str_copy(aOut, pValue);
	}
}

bool CMapSettingsBackend::OnInput(const IInput::CEvent &Event)
{
	if(ms_pActiveContext)
		return ms_pActiveContext->OnInput(Event);

	return false;
}

void CMapSettingsBackend::OnUpdate()
{
	if(ms_pActiveContext && ms_pActiveContext->m_pLineInput && ms_pActiveContext->m_pLineInput->IsActive())
		ms_pActiveContext->UpdateCompositionString();
}

void CMapSettingsBackend::OnMapLoad()
{
	// Load & validate all map settings
	m_LoadedMapSettings.Reset();

	auto &vLoadedMapSettings = Editor()->m_Map.m_vSettings;

	// Keep a vector of valid map settings, to check collision against: m_vValidLoadedMapSettings

	// Create a local context with no lineinput, only used to parse the commands
	CContext LocalContext = NewContext(nullptr);

	// Iterate through map settings
	// Two steps:
	// 1. Save valid and invalid settings
	// 2. Check for duplicates

	std::vector<std::tuple<int, bool, CEditorMapSetting>> vSettingsInvalid;

	for(int i = 0; i < (int)vLoadedMapSettings.size(); i++)
	{
		CEditorMapSetting &Setting = vLoadedMapSettings.at(i);
		// Parse the setting using the context
		LocalContext.UpdateFromString(Setting.m_aCommand);

		bool Valid = LocalContext.Valid();
		ECollisionCheckResult Result = ECollisionCheckResult::ERROR;
		LocalContext.CheckCollision(Setting.m_aCommand, m_LoadedMapSettings.m_vSettingsValid, Result);

		if(Valid && Result == ECollisionCheckResult::ADD)
			m_LoadedMapSettings.m_vSettingsValid.emplace_back(Setting);
		else
			vSettingsInvalid.emplace_back(i, Valid, Setting);

		LocalContext.Reset();

		// Empty duplicates for this line, might be filled later
		m_LoadedMapSettings.m_SettingsDuplicate.insert({i, {}});
	}

	for(const auto &[Index, Valid, Setting] : vSettingsInvalid)
	{
		LocalContext.UpdateFromString(Setting.m_aCommand);

		ECollisionCheckResult Result = ECollisionCheckResult::ERROR;
		int CollidingLineIndex = LocalContext.CheckCollision(Setting.m_aCommand, m_LoadedMapSettings.m_vSettingsValid, Result);
		int RealCollidingLineIndex = CollidingLineIndex;

		if(CollidingLineIndex != -1)
			RealCollidingLineIndex = std::find_if(vLoadedMapSettings.begin(), vLoadedMapSettings.end(), [&](const CEditorMapSetting &MapSetting) {
				return str_comp_nocase(MapSetting.m_aCommand, m_LoadedMapSettings.m_vSettingsValid.at(CollidingLineIndex).m_aCommand) == 0;
			}) - vLoadedMapSettings.begin();

		int Type = 0;
		if(!Valid)
			Type |= SInvalidSetting::TYPE_INVALID;
		if(Result == ECollisionCheckResult::REPLACE)
			Type |= SInvalidSetting::TYPE_DUPLICATE;

		m_LoadedMapSettings.m_vSettingsInvalid.emplace_back(Index, Setting.m_aCommand, Type, RealCollidingLineIndex, !LocalContext.CommandIsValid());
		if(Type & SInvalidSetting::TYPE_DUPLICATE)
			m_LoadedMapSettings.m_SettingsDuplicate[RealCollidingLineIndex].emplace_back(m_LoadedMapSettings.m_vSettingsInvalid.size() - 1);

		LocalContext.Reset();
	}

	if(!m_LoadedMapSettings.m_vSettingsInvalid.empty())
		Editor()->m_Dialog = DIALOG_MAPSETTINGS_ERROR;
}

// ------ loaders

void CMapSettingsBackend::InitValueLoaders()
{
	// Load the different possible values for some specific settings
	RegisterLoader("tune", SValueLoader::LoadTuneValues);
	RegisterLoader("tune_zone", SValueLoader::LoadTuneZoneValues);
	RegisterLoader("mapbug", SValueLoader::LoadMapBugs);
}

void SValueLoader::LoadTuneValues(const CSettingValuesBuilder &TuneBuilder)
{
	// Add available tuning names to argument 0 of setting "tune"
	LoadArgumentTuneValues(TuneBuilder.Argument(0));
}

void SValueLoader::LoadTuneZoneValues(const CSettingValuesBuilder &TuneZoneBuilder)
{
	// Add available tuning names to argument 1 of setting "tune_zone"
	LoadArgumentTuneValues(TuneZoneBuilder.Argument(1));
}

void SValueLoader::LoadMapBugs(const CSettingValuesBuilder &BugBuilder)
{
	// Get argument 0 of setting "mapbug"
	auto ArgBuilder = BugBuilder.Argument(0);
	// Add available map bugs options
	ArgBuilder.Add("grenade-doubleexplosion@ddnet.tw");
}

void SValueLoader::LoadArgumentTuneValues(CArgumentValuesListBuilder &&ArgBuilder)
{
	// Iterate through available tunings add their name to the list
	for(int i = 0; i < CTuningParams::Num(); i++)
	{
		ArgBuilder.Add(CTuningParams::Name(i));
	}
}
