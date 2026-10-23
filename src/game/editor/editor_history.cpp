#include "editor_history.h"

#include "editor.h"

#include <engine/font_icons.h>
#include <engine/shared/config.h>

void CEditor::RenderEditorHistory(CUIRect View)
{
	auto &Facade = Map()->m_DocumentHistory;
	const auto *pHistory = Facade.History();
	if(!pHistory)
		return;
	auto &State = Map()->m_EditorHistoryUiState;
	auto &ListBox = State.m_ListBox;
	ListBox.SetActive(m_Dialog == DIALOG_NONE && !Ui()->IsPopupOpen());

	CUIRect ToolBar, Button, Label, List, DragBar;
	View.HSplitTop(22.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::TOP, &m_aExtraEditorSplits[EXTRAEDITOR_HISTORY]);
	View.HSplitTop(20.0f, &ToolBar, &View);
	View.HSplitTop(2.0f, nullptr, &List);
	ToolBar.HMargin(2.0f, &ToolBar);

	const char *apCategories[] = {"All", "Map", "Envelope", "Settings"};
	for(int Index = 0; Index < 4; ++Index)
	{
		ToolBar.VSplitLeft(65.0f, &Button, &ToolBar);
		if(DoButton_Ex(&State.m_aCategoryButtonIds[Index], apCategories[Index], State.m_Category == Index - 1, &Button, BUTTONFLAG_LEFT, "Filter the displayed entries. Traversal includes every intervening edit.", Index == 0 ? IGraphics::CORNER_L : Index == 3 ? IGraphics::CORNER_R :
																																	     IGraphics::CORNER_NONE))
			State.m_Category = Index - 1;
	}
	ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
	if(DoButton_FontIcon(&State.m_DeleteButtonId, FontIcon::TRASH, pHistory->Revisions().size() > 1 ? 0 : -1, &Button, BUTTONFLAG_LEFT, "Clear retained history, keeping the current map and saved state.", IGraphics::CORNER_ALL, 9.0f))
		Facade.Clear();
	ToolBar.VMargin(8.0f, &Label);
	char aInfo[192];
	const bool TargetExceeded = pHistory->Oversized() || pHistory->Revisions().size() - 1 > pHistory->Limits().m_Entries;
	const bool RetainedRedo = pHistory->Cursor() + 1 < pHistory->Revisions().size();
	str_format(aInfo, sizeof(aInfo), "%" PRIzu " edits, %.1f MiB retained%s", pHistory->Revisions().size() - 1, pHistory->RetainedBytes() / (1024.0 * 1024.0), TargetExceeded ? (RetainedRedo ? " — target exceeded while preserving redo" : " — current undo pair exceeds target") : "");
	SLabelProperties InfoProps;
	InfoProps.m_MaxWidth = Label.w;
	InfoProps.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Label, aInfo, 10.0f, TEXTALIGN_ML, InfoProps);

	CUIRect LimitsRow, LimitBox;
	List.HSplitTop(18.0f, &LimitsRow, &List);
	LimitsRow.VSplitLeft(100.0f, &LimitBox, &LimitsRow);
	g_Config.m_ClEditorMaxHistory = UiDoValueSelector(&State.m_EntryLimitId, &LimitBox, "Edits:", g_Config.m_ClEditorMaxHistory, 1, 500, 1, 1.0f, "Maximum retained edits per map. Applies to the unified timeline.").m_Value;
	LimitsRow.VSplitLeft(5.0f, nullptr, &LimitsRow);
	LimitsRow.VSplitLeft(120.0f, &LimitBox, &LimitsRow);
	g_Config.m_EdHistoryMemory = UiDoValueSelector(&State.m_MemoryLimitId, &LimitBox, "MiB:", g_Config.m_EdHistoryMemory, 1, 16384, 1, 1.0f, "Retention target. Keep the current map and its preceding state even when the pair exceeds this target. Existing redo is preserved.").m_Value;
	Facade.ApplyPreferences();
	List.HSplitTop(16.0f, &Label, &List);
	const auto Usage = Facade.MemoryUsage();
	char aMemory[256];
	if(!Usage.m_Sampled)
		str_copy(aMemory, "Memory details updating...");
	else
		str_format(aMemory, sizeof(aMemory), "Last sample%s: live %.1f MiB, draft extra %.1f MiB, CPU caches %.1f MiB (shared counts overlap)", Usage.m_Current ? "" : " (updating)",
			Usage.m_LiveBytes / 1048576.0, Usage.m_DraftExtraBytes / 1048576.0, Usage.m_CacheBytes / 1048576.0);
	InfoProps.m_MaxWidth = Label.w;
	Ui()->DoLabel(&Label, aMemory, 10.0f, TEXTALIGN_ML, InfoProps);

	std::vector<std::size_t> vVisible;
	int Selected = -1;
	for(std::size_t Index = pHistory->Revisions().size(); Index-- > 0;)
	{
		const auto &Revision = pHistory->Revisions()[Index];
		if(Index != 0 && State.m_Category >= 0 && static_cast<int>(Revision.m_Category) != State.m_Category)
			continue;
		if(Index == pHistory->Cursor())
			Selected = static_cast<int>(vVisible.size());
		vVisible.push_back(Index);
	}
	ListBox.DoStart(15.0f, static_cast<int>(vVisible.size()), 1, 3, Selected, &List);
	for(std::size_t VisibleIndex = 0; VisibleIndex < vVisible.size(); ++VisibleIndex)
	{
		const auto Index = vVisible[VisibleIndex];
		const auto &Revision = pHistory->Revisions()[Index];
		const CListboxItem Item = ListBox.DoNextItem(&Revision, Selected == static_cast<int>(VisibleIndex));
		if(!Item.m_Visible)
			continue;
		Item.m_Rect.VMargin(5.0f, &Label);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		if(Index > pHistory->Cursor())
			TextRender()->TextColor({0.5f, 0.5f, 0.5f});
		Ui()->DoLabel(&Label, Index == 0 ? "Initial retained state" : Revision.m_Label.c_str(), 10.0f, TEXTALIGN_ML, Props);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelected = ListBox.DoEnd();
	if(NewSelected >= 0 && NewSelected != Selected && NewSelected < static_cast<int>(vVisible.size()))
		Facade.GoTo(vVisible[NewSelected]);
}
