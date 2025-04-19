#include "menus.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <engine/external/json-parser/json.h>
#include <engine/shared/jsonwriter.h>

#include <game/client/components/touch_controls.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_rect.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

static const char *BEHAVIORS[] = {"Bind", "Bind Toggle", "Predefined"};
static const char *PREDEFINEDS[] = {"Extra Menu", "Joystick Hook", "Joystick Fire", "Joystick Aim", "Joystick Action", "Use Action", "Swap Action", "Spectate", "Emoticon", "Ingame Menu"};
static const char *LABELTYPES[] = {"Plain", "Localized", "Icon"};
// This is for the gray names in visibility editor.
static const ColorRGBA LABELCOLORS[2] = {ColorRGBA(0.3f, 0.3f, 0.3f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)};

// This is called when the Touch button editor is rendered, the below one. Also when selectedbutton changes. Used for updating all cached settings.
void CMenus::CacheAllSettingsFromTarget(CTouchControls::CTouchButton *TargetButton)
{
	// This reset will as well give m_vCachedCommands one default member.
	ResetCachedSettings();
	if(TargetButton == nullptr)
	{
		return; // Nothing to cache.
	}
	// These values can't be null. The constructor has been updated. Default:{0,0,CTouchControls::BUTTON_SIZE_MINIMUM,CTouchControls::BUTTON_SIZE_MINIMUM}, shape = rect.
	SetPosInputs(TargetButton->m_UnitRect);
	m_CachedShape = TargetButton->m_Shape;
	for(const auto &Visibility : TargetButton->m_vVisibilities)
	{
		if((int)Visibility.m_Type >= (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES)
			dbg_assert(false, "Target button has out of bound visibility type value");
		m_aCachedVisibilities[(int)Visibility.m_Type] = Visibility.m_Parity ? 1 : 0;
	}

	// These are behavior values.
	if(TargetButton->m_pBehavior != nullptr)
	{
		std::string BehaviorType = TargetButton->m_pBehavior->GetBehaviorType();
		if(BehaviorType == "bind")
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND;
			auto *CastedBehavior = static_cast<CTouchControls::CBindTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
			// Take care m_LabelType must not be null as for now. When adding a new button give it a default value or cry.
			m_vCachedCommands[0].m_Label = CastedBehavior->GetLabel().m_pLabel;
			m_vCachedCommands[0].m_LabelType = CastedBehavior->GetLabel().m_Type;
			m_vCachedCommands[0].m_Command = CastedBehavior->GetCommand();
			m_InputCommand.Set(CastedBehavior->GetCommand().c_str());
			ParseLabel(CastedBehavior->GetLabel().m_pLabel);
			m_InputLabel.Set(m_ParsedString.c_str());
		}
		else if(BehaviorType == "bind-toggle")
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND_TOGGLE;
			auto *CastedBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
			m_vCachedCommands = CastedBehavior->GetCommand();
			m_EditCommandNumber = 0;
			if(!m_vCachedCommands.empty())
			{
				m_InputCommand.Set(m_vCachedCommands[0].m_Command.c_str());
				ParseLabel(m_vCachedCommands[0].m_Label.c_str());
				m_InputLabel.Set(m_ParsedString.c_str());
			}
		}
		else if(BehaviorType == "predefined")
		{
			m_EditBehaviorType = (int)EBehaviorType::PREDEFINED;
			const char *PredefinedType = TargetButton->m_pBehavior->GetPredefinedType();
			if(PredefinedType == nullptr)
				m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
			else
				for(m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU; m_PredefinedBehaviorType < (int)EPredefinedType::NUM_PREDEFINEDS && str_comp(PredefinedType, GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[m_PredefinedBehaviorType].m_pId) != 0; m_PredefinedBehaviorType++)
					/*Update m_PredefinedBehaviorType, nothing else should be done here*/;

			if(m_PredefinedBehaviorType == (int)EPredefinedType::NUM_PREDEFINEDS)
				dbg_assert(false, "Detected out of bound m_PredefinedBehaviorType. PredefinedType = %s", PredefinedType);

			if(m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU)
			{
				auto *CastedBehavior = static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
				m_CachedNumber = CastedBehavior->GetNumber();
			}
		}
		else // Empty
			dbg_assert(false, "Detected out of bound value in m_EditBehaviorType");
	}
	if(m_vCachedCommands.size() < 2)
		m_vCachedCommands.resize(2);
}

void CMenus::ResetCachedSettings()
{
	// Reset all cached values.
	m_EditBehaviorType = (int)EBehaviorType::BIND;
	m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
	m_CachedNumber = 0;
	m_EditCommandNumber = 0;
	m_vCachedCommands.clear();
	m_vCachedCommands.reserve(5);
	m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
	m_aCachedVisibilities.fill(2); // 2 means don't have the visibility, true:1,false:0
	m_aCachedVisibilities[(int)CTouchControls::EButtonVisibility::DEMO_PLAYER] = 0;
	// These things can't be empty. std::stoi can't cast empty string.
	SetPosInputs({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM});
	m_InputLabel.Set("");
	m_InputCommand.Set("");
	m_CachedShape = CTouchControls::EButtonShape::RECT;
}

void CMenus::InputPosFunction(CLineInputNumber *Input)
{
	int InputValue = Input->GetInteger();
	// Deal with the "-1" FindPositionXY give.
	InputValue = clamp(InputValue, 0, CTouchControls::BUTTON_SIZE_SCALE);
	Input->SetInteger(InputValue);
	SetUnsavedChanges(true);
}

void CMenus::RenderTouchButtonEditor(CUIRect MainView)
{
	if(!GameClient()->m_TouchControls.IsButtonEditing())
	{
		RenderTouchButtonEditorWhileNothingSelected(MainView);
		return;
	}
	MainView.h = 11 * (LINEGAP + 25.0f);
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B | IGraphics::CORNER_TR, 10.0f);
	// Used to decide if need to update the tmpbutton.
	bool Changed = false;
	CUIRect Left, A, B, C, EditBox, Block;
	MainView.HSplitTop(6 * (LINEGAP + 25.0f) + 6 * LINEGAP, &Block, &Left);

	// Choosing which to edit.
	Block.HSplitTop(4 * LINEGAP, &EditBox, &Block);
	EditBox.HMargin((EditBox.h - 25.0f) / 2.0f, &EditBox);
	EditBox.VMargin(20.0f, &EditBox);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &C, &EditBox);
	EditBox.VSplitMid(&A, &B);

	if(DoButton_MenuTab(m_aEditElementIds.data(), "Position", m_EditElement == 0, &C, IGraphics::CORNER_L))
	{
		m_EditElement = 0;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[1], "Visibility", m_EditElement == 1, &A, IGraphics::CORNER_NONE))
	{
		m_EditElement = 1;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[2], "Behavior", m_EditElement == 2, &B, IGraphics::CORNER_R))
	{
		m_EditElement = 2;
	}

	// Edit blocks.
	switch(m_EditElement)
	{
	case 0: Changed = RenderPosSettingBlock(Block) || Changed; break;
	case 1: Changed = RenderVisibilitySettingBlock(Block) || Changed; break;
	case 2: Changed = RenderBehaviorSettingBlock(Block) || Changed; break;
	default: dbg_assert(false, "Unknown m_EditElement = %d.", m_EditElement);
	}

	// Save & Cancel & Hint.
	Left.VMargin(20.0f, &Left);
	Left.HSplitTop(25.0f, &EditBox, &Left);
	const float ButtonWidth = (EditBox.w - 10) / 3.0f;
	EditBox.VSplitLeft(ButtonWidth, &A, &EditBox);
	EditBox.VSplitLeft(5.0f, nullptr, &EditBox);
	static CButtonContainer s_ConfirmButton;
	// After touching this button, the button is then added into the button vector. Or it is still virtual.
	const char *ConfirmText = GameClient()->m_TouchControls.NoRealButtonSelected() ? "Finish Creating" : "Save";
	if(DoButton_Menu(&s_ConfirmButton, ConfirmText, UnsavedChanges() ? 0 : 1, &A))
	{
		if(UnsavedChanges())
		{
			m_OldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
			if(CheckCachedSettings())
			{
				SaveCachedSettingsToTarget(m_OldSelectedButton);
				GameClient()->m_TouchControls.SetEditingChanges(true);
				SetUnsavedChanges(false);
			}
		}
	}
	EditBox.VSplitLeft(ButtonWidth, &A, &EditBox);
	EditBox.VSplitLeft(5.0f, nullptr, &B);
	if(UnsavedChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&A, "Unsaved Changes", 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	static CButtonContainer s_CancelButton;
	if(DoButton_Menu(&s_CancelButton, "Reset Changes", UnsavedChanges() ? 0 : 1, &B))
	{
		// Since the settings are cancelled, reset the cached settings to m_pSelectedButton though selected button didn't change.
		// Cancel will reset changes to default if the button is still virtual.
		if(UnsavedChanges())
		{
			CacheAllSettingsFromTarget(GameClient()->m_TouchControls.SelectedButton());
			Changed = true;
			if(!GameClient()->m_TouchControls.NoRealButtonSelected())
			{
				SetUnsavedChanges(false);
			}
		}
		// Cancel does nothing if nothing is unsaved.
	}

	// Functional Buttons.
	Left.HSplitTop(LINEGAP, nullptr, &Left);
	Left.HSplitTop(25.0f, &EditBox, &Left);
	EditBox.VSplitMid(&A, &B);
	A.VSplitRight(5.0f, &A, nullptr);
	static CButtonContainer s_AddNewButton;
	// If a new button has created, Checked==0.
	bool Checked = GameClient()->m_TouchControls.NoRealButtonSelected();
	if(DoButton_Menu(&s_AddNewButton, "New Button", Checked ? 1 : 0, &A))
	{
		CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
		if(Checked)
		{
			PopupMessage("Already Created New Button", "A new button is already created, please save or delete it before creating a new one", "OK");
		}
		else if(FreeRect.m_X == -1)
		{
			PopupMessage("No Space", "No enough space for another button.", "OK");
		}
		else if(UnsavedChanges())
		{
			PopupConfirm("Unsaved Changes", "Save all changes before creating another button?", "Save", "Discard", &CMenus::PopupConfirm_NewButton, POPUP_NONE, &CMenus::PopupCancel_NewButton);
		}
		else
		{
			PopupCancel_NewButton();
		}
	}

	B.VSplitLeft(5.0f, nullptr, &B);
	static CButtonContainer s_RemoveButton;
	if(DoButton_Menu(&s_RemoveButton, "Delete Button", 0, &B))
	{
		PopupConfirm("Delete Button", "Are you sure to delete this button? This can't be undone.", "Delete", "Cancel", &CMenus::PopupConfirm_DeleteButton);
	}

	// Create a new button with current cached settings. Auto moving to nearest empty space.
	Left.HSplitTop(LINEGAP, nullptr, &Left);
	Left.HSplitTop(25.0f, &EditBox, &Left);
	EditBox.VSplitMid(&A, &B);
	A.VSplitRight(5.0f, &A, nullptr);
	static CButtonContainer s_CopyPasteButton;
	if(DoButton_Menu(&s_CopyPasteButton, "Duplicate Button", UnsavedChanges() || Checked ? 1 : 0, &A))
	{
		if(Checked)
		{
			PopupMessage("Already Created New Button", "A new button is already created, please save or delete it before creating a new one", "OK");
		}
		else if(UnsavedChanges())
		{
			PopupMessage("Unsaved Changes", "Save changes before duplicate a button.", "OK");
		}
		else
		{
			CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition(GameClient()->m_TouchControls.ShownRect().value(), true);
			if(FreeRect.m_X == -1)
			{
				FreeRect.m_W = CTouchControls::BUTTON_SIZE_MINIMUM;
				FreeRect.m_H = CTouchControls::BUTTON_SIZE_MINIMUM;
				FreeRect = GameClient()->m_TouchControls.UpdatePosition(FreeRect, true);
				if(FreeRect.m_X == -1)
				{
					PopupMessage("No Space", "No enough space for another button.", "OK");
				}
				else
				{
					PopupMessage("Not Enough Space", "Space is not enough for another button with this size. The button has been resized.", "OK");
				}
			}
			if(FreeRect.m_X != -1) // FreeRect might change. Don't use else here.
			{
				ResetButtonPointers();
				SetPosInputs(FreeRect);
				Changed = true;
				SetUnsavedChanges(true);
			}
		}
	}

	// Deselect a button.
	B.VSplitLeft(5.0f, nullptr, &B);
	static CButtonContainer s_DeselectButton;
	if(DoButton_Menu(&s_DeselectButton, "Deselect", 0, &B))
	{
		m_OldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_NewSelectedButton = nullptr;
		if(UnsavedChanges())
		{
			PopupConfirm("Unsaved Changes", "You'll lose unsaved changes after deselecting.", "Deselect", "Cancel", &CMenus::PopupCancel_DeselectButton);
		}
		else
		{
			PopupCancel_DeselectButton();
		}
	}

	// This ensures m_pTmpButton being updated always.
	if(Changed == true)
	{
		UpdateTmpButton();
	}
}

void CMenus::RenderTouchButtonEditorWhileNothingSelected(CUIRect MainView)
{
	CUIRect A, B, C;
	MainView.h = 200.0f;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B | IGraphics::CORNER_TR, 10.0f);
	MainView.HSplitTop(MainView.h / 3.0f, &A, &MainView);
	MainView.HSplitMid(&C, &B);
	Ui()->DoLabel(&A, "No Button Selected.", 20.0f, TEXTALIGN_MC);
	Ui()->DoLabel(&C, "Long press to selected a button.", 15.0f, TEXTALIGN_MC);
	B.HMargin((B.h - 30.0f) / 2.0f, &B);
	B.VSplitMid(&A, &B);
	A.VMargin(15.0f, &A);
	static CButtonContainer s_NewButton;
	if(DoButton_Menu(&s_NewButton, "New Button", 0, &A))
		PopupCancel_NewButton();
	B.VMargin(15.0f, &B);
	static CButtonContainer s_SelecteButton;
	if(DoButton_Menu(&s_SelecteButton, "Select a Button", 0, &B))
		SetActive(false);
}

void CMenus::RenderVirtualVisibilityEditor(CUIRect MainView)
{
	CUIRect EditBox;
	MainView.HMargin(10.0f, &MainView);
	MainView.HSplitBottom(20.0f, &MainView, &EditBox);
	EditBox.VMargin(10.0f, &EditBox);
	Ui()->DoLabel(&EditBox, "This visibility only works in editor.", 15.0f, TEXTALIGN_MC);
	MainView.VMargin(10.0f, &MainView);
	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 10.0f);
	MainView.VMargin(10.0f, &MainView);
	MainView.HMargin(5.0f, &MainView);
	static CScrollRegion s_VirtualVisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VirtualVisibilityScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParam);
	MainView.y += ScrollOffset.y;
	std::array<bool, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> aVirtualVisibilities = GameClient()->m_TouchControls.VirtualVisibilities();
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		MainView.HSplitTop(30.0f, &EditBox, &MainView);
		if(s_VirtualVisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(5.0f, nullptr, &EditBox);
			if(DoButton_CheckBox(&m_aVisibilityIds[Current], GameClient()->m_TouchControls.VisibilityStrings()[Current], aVirtualVisibilities[Current] == 1, &EditBox))
				GameClient()->m_TouchControls.ReverseVirtualVisibilities(Current);
		}
	}
	s_VirtualVisibilityScrollRegion.End();
}

void CMenus::ChangeSelectedButtonWhileHavingUnsavedChanges()
{
	// Both old and new button pointer can be nullptr.
	// Saving settings to the old selected button(nullptr = create), then switch to new selected button(new = haven't created).
	PopupConfirm("Unsaved changes", "Save all changes before switching selected button?", "Save", "Discard", &CMenus::PopupConfirm_ChangeSelectedButton, POPUP_NONE, &CMenus::PopupCancel_ChangeSelectedButton);
}

void CMenus::SaveCachedSettingsToTarget(CTouchControls::CTouchButton *TargetButton)
{
	// Save the cached config to the target button. If no button to save, create a new one, then select it.
	if(TargetButton == nullptr)
	{
		TargetButton = GameClient()->m_TouchControls.NewButton();
		// Keep the new button's visibility equal to the last selected one.
		for(unsigned Iterator = (unsigned)CTouchControls::EButtonVisibility::INGAME; Iterator < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Iterator)
		{
			if(GameClient()->m_Menus.m_aCachedVisibilities[Iterator] != 2)
				TargetButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Iterator, static_cast<bool>(GameClient()->m_Menus.m_aCachedVisibilities[Iterator]));
		}
		GameClient()->m_TouchControls.SetSelectedButton(TargetButton);
	}

	TargetButton->m_UnitRect.m_W = clamp(m_InputW.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	TargetButton->m_UnitRect.m_H = clamp(m_InputH.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	TargetButton->m_UnitRect.m_X = clamp(m_InputX.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - TargetButton->m_UnitRect.m_W);
	TargetButton->m_UnitRect.m_Y = clamp(m_InputY.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - TargetButton->m_UnitRect.m_H);
	TargetButton->m_vVisibilities.clear();
	for(unsigned Iterator = (unsigned)CTouchControls::EButtonVisibility::INGAME; Iterator < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Iterator)
	{
		if(m_aCachedVisibilities[Iterator] != 2)
			TargetButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Iterator, static_cast<bool>(m_aCachedVisibilities[Iterator]));
	}
	TargetButton->m_Shape = m_CachedShape;
	TargetButton->UpdateScreenFromUnitRect();
	if(m_EditBehaviorType == (int)EBehaviorType::BIND)
	{
		TargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindTouchButtonBehavior>(m_vCachedCommands[0].m_Label.c_str(), m_vCachedCommands[0].m_LabelType, m_vCachedCommands[0].m_Command.c_str());
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vMovingBehavior = m_vCachedCommands;
		TargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindToggleTouchButtonBehavior>(std::move(vMovingBehavior));
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
	{
		if(m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU)
			TargetButton->m_pBehavior = std::make_unique<CTouchControls::CExtraMenuTouchButtonBehavior>(CTouchControls::CExtraMenuTouchButtonBehavior(m_CachedNumber));
		else
			TargetButton->m_pBehavior = GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[m_PredefinedBehaviorType].m_Factory();
	}
	else
	{
		dbg_assert(false, "Unknown m_EditBehaviorType = %d", m_EditBehaviorType);
	}
	TargetButton->UpdatePointers();
}

void CMenus::PopupConfirm_ChangeSelectedButton()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_OldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		SetUnsavedChanges(false);
		PopupCancel_ChangeSelectedButton();
	}
}

void CMenus::PopupCancel_ChangeSelectedButton()
{
	GameClient()->m_TouchControls.SetSelectedButton(m_NewSelectedButton);
	if(m_NewSelectedButton != nullptr)
	{
		CacheAllSettingsFromTarget(m_NewSelectedButton);
		SetUnsavedChanges(false);
		UpdateTmpButton();
	}
	else
	{
		// New selected button is nullptr, meaning changing selected button to air.
		CacheAllSettingsFromTarget(nullptr);
		ResetButtonPointers();
		SetUnsavedChanges(false);
	}
	if(m_CloseMenu)
		SetActive(false);
}

void CMenus::PopupConfirm_NewButton()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_OldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		PopupCancel_NewButton();
	}
}

void CMenus::PopupCancel_NewButton()
{
	// New button doesn't create a real button, instead it reset the tmpbutton to cache every setting. When saving a the tmpbutton then a real button will be created.
	CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
	ResetButtonPointers();
	ResetCachedSettings();
	SetPosInputs(FreeRect);
	UpdateTmpButton();
	SetUnsavedChanges(true);
}

bool CMenus::CheckCachedSettings()
{
	bool FatalError = false;
	std::vector<std::string> Errors;
	char ErrorBuf[256] = "";
	int X = m_InputX.GetInteger();
	int Y = m_InputY.GetInteger();
	int W = m_InputW.GetInteger();
	int H = m_InputH.GetInteger();
	// Illegal size settings.
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM || H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
	{
		Errors.emplace_back("Width and Height are required to be within the range of [50,000, 500,000].\n");
		FatalError = true;
	}
	if(X < 0 || Y < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
	{
		Errors.emplace_back("Out of bound position value.\n");
		FatalError = true;
	}
	if(GameClient()->m_TouchControls.IfOverlapping({X, Y, W, H}))
	{
		Errors.emplace_back("The selected button is overlapping with other buttons.\n");
		FatalError = true;
	}
	// Bind Toggle has less than two commands. This is illegal.
	if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE && m_vCachedCommands.size() < 2)
	{
		Errors.emplace_back("Commands in Bind Toggle has less than two command. Add more commands or use Bind behavior.\n");
		FatalError = true;
	}
	// Button has extra menu visibilities that doesn't have buttons to toggle them. This is meaningless.
	const auto ExistingMenus = GameClient()->m_TouchControls.FindExistingExtraMenus();
	bool FirstCheck = false;
	for(int CurrentNumber = 0; CurrentNumber < CTouchControls::MAXNUMBER; CurrentNumber++)
	{
		int CachedSetting = m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::EXTRA_MENU_1) + CurrentNumber];
		if(CachedSetting != 2 && ExistingMenus[CurrentNumber] == false)
		{
			if(FirstCheck == false)
			{
				str_format(ErrorBuf, sizeof(ErrorBuf), "Visibility \"Extra Menu %d", CurrentNumber + 1);
				Errors.emplace_back(ErrorBuf);
				FirstCheck = true;
			}
			else
			{
				str_format(ErrorBuf, sizeof(ErrorBuf), ", %d", CurrentNumber + 1);
				Errors.emplace_back(ErrorBuf);
			}
		}
	}
	if(FirstCheck)
	{
		Errors.emplace_back("\" has no corresponding button to get activated or deactivated.\n");
	}
	// Demo Player is used with other visibilities except Extra Menu. This is meaningless.
	if(m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DEMO_PLAYER)] == 1 &&
		(m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::RCON_AUTHED)] != 2 ||
			m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::INGAME)] != 2 ||
			m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::VOTE_ACTIVE)] != 2 ||
			m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DUMMY_CONNECTED)] != 2 ||
			m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DUMMY_ALLOWED)] != 2 ||
			m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::ZOOM_ALLOWED)] != 2))
	{
		Errors.emplace_back("When visibility \"Demo Player\" is on, visibilities except Extra Menu is meaningless.\n");
	}
	if(m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DEMO_PLAYER)] == 2)
	{
		Errors.emplace_back("Visibility \"Demo Player\" is missing. The button will be visible both in and out of demo player.");
	}
	if(!Errors.empty())
	{
		for(const std::string &Error : Errors)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s%s", ErrorBuf, Error.c_str());
			str_copy(ErrorBuf, aBuf, 256);
		}
		if(FatalError)
		{
			PopupMessage("Illegal settings", ErrorBuf, "OK");
		}
		else
		{
			PopupConfirm("Redundant settings", ErrorBuf, "Continue Saving", "Cancel", &CMenus::PopupConfirm_SaveSettings);
		}
		return false;
	}
	else
	{
		return true;
	}
}

void CMenus::SelectedButtonNotVisible()
{
	// Cancel shouldn't do anything but open ingame menu, the menu is already opened now.
	m_CloseMenu = false;
	PopupConfirm("Selected button not visible", "The selected button is not visible, do you want to de-select it or edit it's visibility?", "De-select", "Edit", &CMenus::PopupConfirm_SelectedNotVisible);
}

void CMenus::PopupConfirm_SelectedNotVisible()
{
	if(UnsavedChanges())
	{
		// The m_pSelectedButton can't nullptr, because this function is triggered when selected button not visible.
		m_OldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_NewSelectedButton = nullptr;
		m_CloseMenu = true;
		ChangeSelectedButtonWhileHavingUnsavedChanges();
	}
	else
	{
		ResetButtonPointers();
		SetActive(false);
	}
}

void CMenus::PopupCancel_DeselectButton()
{
	ResetButtonPointers();
	SetUnsavedChanges(false);
	ResetCachedSettings();
}

void CMenus::NoSpaceForOverlappingButton()
{
	PopupMessage("No Space", "No space left for the button. Make sure you didn't choose wrong visibilities, or edit its size.", "OK");
}

void CMenus::RenderTinyButtonTab(CUIRect MainView)
{
	MainView.Margin(10.0f, &MainView);
	MainView.VMargin((MainView.w - 150.0f) / 2.0f, &MainView);
	static CButtonContainer s_TabNewButton;
	if(DoButton_Menu(&s_TabNewButton, "New Button", 0, &MainView))
	{
		CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
		if(FreeRect.m_X == -1)
		{
			PopupMessage("No Space", "No enough space for another button. Check if you choosed a wrong visibility.", "OK");
		}
		else
		{
			PopupCancel_NewButton();
		}
	}
}

void CMenus::ResetButtonPointers()
{
	// So there's no more includes in menus.h.
	GameClient()->m_TouchControls.ResetButtonPointers();
}

void CMenus::SetPosInputs(CTouchControls::CUnitRect MyRect)
{
	m_InputX.SetInteger(MyRect.m_X);
	m_InputY.SetInteger(MyRect.m_Y);
	m_InputW.SetInteger(MyRect.m_W);
	m_InputH.SetInteger(MyRect.m_H);
}

void CMenus::PopupConfirm_TurnOffEditor()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_OldSelectedButton);
		PopupCancel_TurnOffEditor();
	}
}

void CMenus::PopupCancel_TurnOffEditor()
{
	GameClient()->m_TouchControls.SetEditingActive(!GameClient()->m_TouchControls.IsEditingActive());
	ResetButtonPointers();
}

bool CMenus::RenderPosSettingBlock(CUIRect Block)
{
	bool Changed = false;
	CUIRect EditBox, A, B, PosX, PosY, PosW, PosH;
	Block.VMargin((Block.w - 270.0f) / 2.0f, &Block);
	Block.Margin(LINEGAP, &Block);
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 5.0f);
	Block.HMargin((Block.h - 5 * (LINEGAP + 25.0f)) / 2.0f, &Block);
	Block.VMargin(LINEGAP, &Block);
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(25.0f, &PosX, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputX, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputX);
		Changed = true;
	}

	// Auto check if the input value contains char that is not digit. If so delete it.
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(25.0f, &PosY, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputY, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputY);
		Changed = true;
	}

	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(25.0f, &PosW, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputW, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputW);
		Changed = true;
	}

	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(25.0f, &PosH, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputH, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputH);
		Changed = true;
	}
	int X = m_InputX.GetInteger();
	int Y = m_InputY.GetInteger();
	int W = m_InputW.GetInteger();
	int H = m_InputH.GetInteger();
	if(X < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("X:", PosX, 16.0f);
	else
		Ui()->DoLabel(&PosX, "X:", 16.0f, TEXTALIGN_ML);
	if(Y < 0 || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("Y:", PosY, 16.0f);
	else
		Ui()->DoLabel(&PosY, "Y:", 16.0f, TEXTALIGN_ML);
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel("W:", PosW, 16.0f);
	else
		Ui()->DoLabel(&PosW, "W:", 16.0f, TEXTALIGN_ML);
	if(H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel("H:", PosH, 16.0f);
	else
		Ui()->DoLabel(&PosH, "H:", 16.0f, TEXTALIGN_ML);

	// Drop down menu for shapes
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	Ui()->DoLabel(&A, "Shape:", 16.0f, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonShapeDropDownState;
	static CScrollRegion s_ButtonShapeDropDownScrollRegion;
	s_ButtonShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonShapeDropDownScrollRegion;
	std::array<const char *, (int)CTouchControls::EButtonShape::NUM_SHAPES> aShapes = GameClient()->m_TouchControls.Shapes();
	const char *Shapes[(int)CTouchControls::EButtonShape::NUM_SHAPES];
	for(int Shape = 0; Shape < (int)CTouchControls::EButtonShape::NUM_SHAPES; Shape++)
	{
		Shapes[Shape] = aShapes[Shape];
	}
	B.VSplitRight(150.0f, nullptr, &B);
	const CTouchControls::EButtonShape NewButtonShape = (CTouchControls::EButtonShape)Ui()->DoDropDown(&B, (int)m_CachedShape, Shapes, (int)CTouchControls::EButtonShape::NUM_SHAPES, s_ButtonShapeDropDownState);
	if(NewButtonShape != m_CachedShape)
	{
		m_CachedShape = NewButtonShape;
		SetUnsavedChanges(true);
		Changed = true;
	}
	return Changed;
}

bool CMenus::RenderBehaviorSettingBlock(CUIRect Block)
{
	bool Changed = false;
	CUIRect EditBox, A, B, C;
	Block.VMargin((Block.w - 400.0f) / 2.0f, &Block);
	Block.Margin(LINEGAP, &Block);
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 5.0f);
	Block.HMargin((Block.h - 5 * (LINEGAP + 25.0f)) / 2.0f, &Block);
	Block.VMargin(LINEGAP, &Block);
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	Ui()->DoLabel(&A, "Behavior Type:", 16.0f, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonBehaviorDropDownState;
	static CScrollRegion s_ButtonBehaviorDropDownScrollRegion;
	s_ButtonBehaviorDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonBehaviorDropDownScrollRegion;
	const int NewButtonBehavior = Ui()->DoDropDown(&B, m_EditBehaviorType, BEHAVIORS, std::size(BEHAVIORS), s_ButtonBehaviorDropDownState);
	if(NewButtonBehavior != m_EditBehaviorType)
	{
		m_EditBehaviorType = NewButtonBehavior;
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			ParseLabel(m_vCachedCommands[0].m_Label.c_str());
			m_InputLabel.Set(m_ParsedString.c_str());
			m_InputCommand.Set(m_vCachedCommands[0].m_Command.c_str());
		}
		if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
		{
			if(m_vCachedCommands.size() <= (size_t)(m_EditCommandNumber))
				m_EditCommandNumber = 0;
			while(m_vCachedCommands.size() < 2)
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
			ParseLabel(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_InputLabel.Set(m_ParsedString.c_str());
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
		}
		SetUnsavedChanges(true);
		Changed = true;
	}

	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == (int)EBehaviorType::BIND)
	{
		Ui()->DoLabel(&A, "Command:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputCommand, &B, 10.0f))
		{
			m_vCachedCommands[0].m_Command = m_InputCommand.GetString();
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		EditBox.VSplitLeft(25.0f, &A, &EditBox);
		EditBox.VSplitRight(25.0f, &B, &C);
		B.VMargin(5.0f, &B);
		static CUi::SDropDownState s_BehaviorToggleDropDownState;
		static CScrollRegion s_BehaviorToggleDropDownScrollRegion;
		s_BehaviorToggleDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_BehaviorToggleDropDownScrollRegion;
		static std::vector<std::string> s_vCachedStrings;
		s_vCachedStrings.clear();
		std::vector<const char *> vToggleStrings;
		vToggleStrings.reserve(m_vCachedCommands.size());
		for(unsigned CurrentCommand = 0; CurrentCommand < m_vCachedCommands.size(); CurrentCommand++)
		{
			char Buf[32];
			if(m_vCachedCommands[CurrentCommand].m_Label.empty())
			{
				str_format(Buf, sizeof(Buf), "%d", CurrentCommand + 1);
				s_vCachedStrings.emplace_back(Buf);
				vToggleStrings.emplace_back(s_vCachedStrings.back().data());
				continue;
			}
			// The 12 there isn't really changable, or it will render some characters that are not showable. 12 suits all characters.
			if(m_vCachedCommands[CurrentCommand].m_Label.size() <= 12)
				str_format(Buf, sizeof(Buf), "%d: %s", CurrentCommand + 1, m_vCachedCommands[CurrentCommand].m_Label.c_str());
			else
				str_format(Buf, sizeof(Buf), "%d: %s...", CurrentCommand + 1, m_vCachedCommands[CurrentCommand].m_Label.substr(0, 12).c_str());
			s_vCachedStrings.emplace_back(Buf);
			vToggleStrings.emplace_back(s_vCachedStrings.back().data());
		}
		const int NewCommandNumber = Ui()->DoDropDown(&B, m_EditCommandNumber, vToggleStrings.data(), m_vCachedCommands.size(), s_BehaviorToggleDropDownState);
		if(NewCommandNumber != m_EditCommandNumber)
		{
			m_EditCommandNumber = NewCommandNumber;
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
			ParseLabel(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_InputLabel.Set(m_ParsedString.c_str());
		}

		static CButtonContainer s_IncreaseButton;
		if(DoButton_FontIcon(&s_IncreaseButton, "+", 0, &A, BUTTONFLAG_LEFT))
		{
			m_EditCommandNumber++;
			m_vCachedCommands.emplace(m_vCachedCommands.begin() + m_EditCommandNumber, "", CTouchControls::CButtonLabel::EType::PLAIN, "");
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
			ParseLabel(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_InputLabel.Set(m_ParsedString.c_str());
		}
		static CButtonContainer s_DeleteButton;
		if(DoButton_FontIcon(&s_DeleteButton, FontIcons::FONT_ICON_TRASH, 0, &C, BUTTONFLAG_LEFT))
		{
			const auto DeleteIt = m_vCachedCommands.begin() + m_EditCommandNumber;
			m_vCachedCommands.erase(DeleteIt);
			if(m_EditCommandNumber + 1 > (int)m_vCachedCommands.size())
			{
				m_EditCommandNumber--;
				if(m_EditCommandNumber < 0)
					dbg_assert(false, "Detected m_EditCommandNumber < 0.");
			}
			while(m_vCachedCommands.size() < 2)
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
			ParseLabel(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_InputLabel.Set(m_ParsedString.c_str());
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
	{
		Ui()->DoLabel(&A, "Type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonPredefinedDropDownState;
		static CScrollRegion s_ButtonPredefinedDropDownScrollRegion;
		s_ButtonPredefinedDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonPredefinedDropDownScrollRegion;
		const int NewPredefined = Ui()->DoDropDown(&B, m_PredefinedBehaviorType, PREDEFINEDS, std::size(PREDEFINEDS), s_ButtonPredefinedDropDownState);
		if(NewPredefined != m_PredefinedBehaviorType)
		{
			m_PredefinedBehaviorType = NewPredefined;
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == (int)EBehaviorType::BIND)
	{
		Ui()->DoLabel(&A, "Label:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputLabel, &B, 10.0f))
		{
			ParseLabel(m_InputLabel.GetString());
			m_vCachedCommands[0].m_Label = m_ParsedString;
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		Ui()->DoLabel(&A, "Command:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputCommand, &B, 10.0f))
		{
			m_vCachedCommands[m_EditCommandNumber].m_Command = m_InputCommand.GetString();
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED && m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU) // Extra menu type, needs to input number.
	{
		// Increase & Decrease button share 1/2 width, the rest is for label.
		EditBox.VSplitLeft(EditBox.w / 4, &A, &B);
		static CButtonContainer s_ExtraMenuDecreaseButton;
		if(DoButton_FontIcon(&s_ExtraMenuDecreaseButton, "-", 0, &A, BUTTONFLAG_LEFT))
		{
			if(m_CachedNumber > 0)
			{
				// Menu Number also counts from 1, but written as 0.
				m_CachedNumber--;
				SetUnsavedChanges(true);
				Changed = true;
			}
		}

		B.VSplitLeft(B.w * 2 / 3.0f, &A, &B);
		Ui()->DoLabel(&A, std::to_string(m_CachedNumber + 1).c_str(), 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ExtraMenuIncreaseButton;
		if(DoButton_FontIcon(&s_ExtraMenuIncreaseButton, "+", 0, &B, BUTTONFLAG_LEFT))
		{
			if(m_CachedNumber < 4)
			{
				m_CachedNumber++;
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
	}
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == (int)EBehaviorType::BIND)
	{
		Ui()->DoLabel(&A, "Label type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonLabelTypeDropDownState;
		static CScrollRegion s_ButtonLabelTypeDropDownScrollRegion;
		s_ButtonLabelTypeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonLabelTypeDropDownScrollRegion;
		const CTouchControls::CButtonLabel::EType NewButtonLabelType = (CTouchControls::CButtonLabel::EType)Ui()->DoDropDown(&B, (int)m_vCachedCommands[0].m_LabelType, LABELTYPES, std::size(LABELTYPES), s_ButtonLabelTypeDropDownState);
		if(NewButtonLabelType != m_vCachedCommands[0].m_LabelType)
		{
			m_vCachedCommands[0].m_LabelType = NewButtonLabelType;
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		Ui()->DoLabel(&A, "Label:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputLabel, &B, 10.0f))
		{
			ParseLabel(m_InputLabel.GetString());
			m_vCachedCommands[m_EditCommandNumber].m_Label = m_ParsedString;
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	Block.HSplitTop(25.0f, &EditBox, &Block);
	Block.HSplitTop(LINEGAP, nullptr, &Block);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		Ui()->DoLabel(&A, "Label type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonLabelTypeDropDownState;
		static CScrollRegion s_ButtonLabelTypeDropDownScrollRegion;
		s_ButtonLabelTypeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonLabelTypeDropDownScrollRegion;
		const CTouchControls::CButtonLabel::EType NewButtonLabelType = (CTouchControls::CButtonLabel::EType)Ui()->DoDropDown(&B, (int)m_vCachedCommands[m_EditCommandNumber].m_LabelType, LABELTYPES, std::size(LABELTYPES), s_ButtonLabelTypeDropDownState);
		if(NewButtonLabelType != m_vCachedCommands[m_EditCommandNumber].m_LabelType)
		{
			m_vCachedCommands[m_EditCommandNumber].m_LabelType = NewButtonLabelType;
			SetUnsavedChanges(true);
			Changed = true;
		}
	}
	return Changed;
}

bool CMenus::RenderVisibilitySettingBlock(CUIRect Block)
{
	// Visibilities time. This is button's visibility, not virtual.
	bool Changed = false;
	CUIRect EditBox;
	Block.VMargin((Block.w - 400.0f) / 2.0f, &Block);
	Block.Margin(LINEGAP, &Block);
	Block.HMargin((Block.h - 5 * (LINEGAP + 25.0f)) / 2.0f, &Block);
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 10.0f);
	Block.VMargin(LINEGAP, &Block);

	static CScrollRegion s_VisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VisibilityScrollRegion.Begin(&Block, &ScrollOffset, &ScrollParam);
	Block.y += ScrollOffset.y;

	static std::vector<CButtonContainer> s_vVisibilitySelector[(int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES];
	std::for_each_n(s_vVisibilitySelector, (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES, [](auto &Element) {
		Element.resize(3);
	});
	const std::vector<const char *> &vLabels = {"Include", "Exclude", "Ignored"};
	const std::array<const char *, (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> VisibilityStrings = GameClient()->m_TouchControls.VisibilityStrings();
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		Block.HSplitTop(30.0f, &EditBox, &Block);
		if(s_VisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(5.0f, nullptr, &EditBox);
			if(DoLine_RadioMenu(EditBox, VisibilityStrings[Current],
				   s_vVisibilitySelector[Current], vLabels, {1, 0, 2}, m_aCachedVisibilities[Current]))
			{
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
	}
	s_VisibilityScrollRegion.End();
	return Changed;
}

void CMenus::DoRedLabel(const char *pLabel, CUIRect Block, const int &Size)
{
	if(pLabel == nullptr)
		return;
	TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
	Ui()->DoLabel(&Block, pLabel, Size, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CMenus::PopupConfirm_SaveSettings()
{
	SetUnsavedChanges(false);
	SaveCachedSettingsToTarget(m_OldSelectedButton);
}

void CMenus::PopupConfirm_DeleteButton()
{
	GameClient()->m_TouchControls.DeleteButton();
	CacheAllSettingsFromTarget(nullptr);
}

void CMenus::DoPopupType(CTouchControls::CPopupParam PopupParam)
{
	m_OldSelectedButton = PopupParam.m_OldSelectedButton;
	m_NewSelectedButton = PopupParam.m_NewSelectedButton;
	m_CloseMenu = !PopupParam.m_KeepMenuOpen;
	switch(PopupParam.m_PopupType)
	{
	case CTouchControls::EPopupType::BUTTON_CHANGED: ChangeSelectedButtonWhileHavingUnsavedChanges(); break;
	case CTouchControls::EPopupType::NO_SPACE: NoSpaceForOverlappingButton(); break;
	case CTouchControls::EPopupType::BUTTON_INVISIBLE: SelectedButtonNotVisible(); break;
	// The NUM_POPUPS will not call the function.
	default: dbg_assert(false, "Unknown popup type.");
	}
}

bool CMenus::UnsavedChanges()
{
	return GameClient()->m_TouchControls.UnsavedChanges();
}

void CMenus::SetUnsavedChanges(bool UnsavedChanges)
{
	GameClient()->m_TouchControls.SetUnsavedChanges(UnsavedChanges);
}

void CMenus::UpdateTmpButton()
{
	GameClient()->m_TouchControls.RemakeTmpButton();
	SaveCachedSettingsToTarget(GameClient()->m_TouchControls.TmpButton());
	GameClient()->m_TouchControls.SetShownRect(GameClient()->m_TouchControls.TmpButton()->m_UnitRect);
}

void CMenus::RenderSelectingTab(CUIRect SelectingTab)
{
	CUIRect A;
	SelectingTab.VSplitLeft(150.0f, &A, &SelectingTab);
	static CButtonContainer s_FileTab;
	if(DoButton_MenuTab(&s_FileTab, "File", m_CurrentMenu == EMenuType::MENU_FILE, &A, IGraphics::CORNER_TL))
		m_CurrentMenu = EMenuType::MENU_FILE;
	SelectingTab.VSplitLeft(150.0f, &A, &SelectingTab);
	static CButtonContainer s_ButtonTab;
	if(DoButton_MenuTab(&s_ButtonTab, "Buttons", m_CurrentMenu == EMenuType::MENU_BUTTONS, &A, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_BUTTONS;

	SelectingTab.VSplitLeft(150.0f, &A, &SelectingTab);
	static CButtonContainer s_SettingsMenuTab;
	if(DoButton_MenuTab(&s_SettingsMenuTab, "Settings", m_CurrentMenu == EMenuType::MENU_SETTINGS, &A, IGraphics::CORNER_TR))
		m_CurrentMenu = EMenuType::MENU_SETTINGS;
}

void CMenus::ResolveIssues()
{
	if(GameClient()->m_TouchControls.IsIssueNotFinished())
	{
		std::array<CTouchControls::CIssueParam, (unsigned)CTouchControls::EIssueType::NUM_ISSUES> Issues = GameClient()->m_TouchControls.Issues();
		for(unsigned Current = 0; Current < (unsigned)CTouchControls::EIssueType::NUM_ISSUES; Current++)
		{
			if(Issues[Current].m_Finished == true)
				continue;
			switch(Current)
			{
			case(int)CTouchControls::EIssueType::CACHE_SETTINGS: CacheAllSettingsFromTarget(Issues[Current].m_TargetButton); break;
			case(int)CTouchControls::EIssueType::SAVE_SETTINGS: SaveCachedSettingsToTarget(Issues[Current].m_TargetButton); break;
			default: dbg_assert(false, "Unknown Issue.");
			}
		}
	}
}

void CMenus::RenderButtonSettings(CUIRect MainView)
{
	CUIRect EditBox, A;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B | IGraphics::CORNER_TR, 10.0f);
	MainView.HSplitTop(25.0f, nullptr, &MainView);
	MainView.HSplitTop(25.0f, &EditBox, &MainView);
	EditBox.VMargin(15.0f, &EditBox);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &EditBox);
	static CButtonContainer s_PreviewVisibilityTab;
	if(DoButton_MenuTab(&s_PreviewVisibilityTab, "Preview Visibility", m_CurrentSetting == ESettingType::PREVIEW_VISIBILITY, &A, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 5.0f))
		m_CurrentSetting = ESettingType::PREVIEW_VISIBILITY;
	EditBox.VSplitMid(&A, &EditBox);
	static CButtonContainer s_ColorTab;
	if(DoButton_MenuTab(&s_ColorTab, "Button Color", m_CurrentSetting == ESettingType::COLOR, &A, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 5.0f))
		m_CurrentSetting = ESettingType::COLOR;
	static CButtonContainer s_DirectTouchTab;
	if(DoButton_MenuTab(&s_DirectTouchTab, "Direct Input", m_CurrentSetting == ESettingType::DIRECT_INPUT, &EditBox, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 5.0f))
		m_CurrentSetting = ESettingType::DIRECT_INPUT;

	switch(m_CurrentSetting)
	{
	case ESettingType::PREVIEW_VISIBILITY: RenderVirtualVisibilityEditor(MainView); break;
	case ESettingType::COLOR: RenderColorSettings(MainView); break;
	case ESettingType::DIRECT_INPUT: RenderDirectTouchEditor(MainView); break;
	default: dbg_assert(false, "Unknown Setting Detected in button editor.");
	}
}

void CMenus::RenderColorSettings(CUIRect MainView)
{
	CUIRect EditBox;
	MainView.Margin(20.0f, &MainView);

	MainView.HSplitTop(20.0f, nullptr, &MainView);
	MainView.HSplitTop(40.0f, &EditBox, &MainView);
	static CButtonContainer s_ActiveColorPicker;
	ColorHSLA ColorTest = DoLine_ColorPicker(&s_ActiveColorPicker, 25.0f, 15.0f, 5.0f, &EditBox, "Active Color", &m_ColorActive, GameClient()->m_TouchControls.DefaultBackgroundColorActive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorActive(color_cast<ColorRGBA>(ColorHSLA(m_ColorActive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorActive())
		GameClient()->m_TouchControls.SetEditingChanges(true);

	MainView.HSplitTop(20.0f, nullptr, &MainView);
	MainView.HSplitTop(40.0f, &EditBox, &MainView);
	static CButtonContainer s_InactiveColorPicker;
	ColorTest = DoLine_ColorPicker(&s_InactiveColorPicker, 25.0f, 15.0f, 5.0f, &EditBox, "Inactive Color", &m_ColorInactive, GameClient()->m_TouchControls.DefaultBackgroundColorInactive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorInactive(color_cast<ColorRGBA>(ColorHSLA(m_ColorInactive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorInactive())
		GameClient()->m_TouchControls.SetEditingChanges(true);
}

void CMenus::RenderDirectTouchEditor(CUIRect MainView)
{
	CUIRect Row, Label, Button;
	MainView.Margin(20.0f, &MainView);
	const float Gap = 20.0f;
	MainView.HSplitTop(Gap, nullptr, &MainView);
	MainView.HSplitTop(40.0f, &Row, &MainView);
	MainView.HSplitTop(Gap, nullptr, &MainView);
	Row.HMargin((Row.h - 20.0f) / 2.0f, &Row);
	Row.VSplitLeft(300.0f, &Label, &Row);
	Ui()->DoLabel(&Label, Localize("Direct touch input while ingame"), 16.0f, TEXTALIGN_ML);

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(150.0f, &Button, &Row);
	const char *apIngameTouchModes[(int)CTouchControls::EDirectTouchIngameMode::NUM_STATES] = {Localize("Disabled", "Direct touch input"), Localize("Active action", "Direct touch input"), Localize("Aim", "Direct touch input"), Localize("Fire", "Direct touch input"), Localize("Hook", "Direct touch input")};
	const CTouchControls::EDirectTouchIngameMode OldDirectTouchIngame = GameClient()->m_TouchControls.DirectTouchIngame();
	static CUi::SDropDownState s_DirectTouchIngameDropDownState;
	static CScrollRegion s_DirectTouchIngameDropDownScrollRegion;
	s_DirectTouchIngameDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DirectTouchIngameDropDownScrollRegion;
	const CTouchControls::EDirectTouchIngameMode NewDirectTouchIngame = (CTouchControls::EDirectTouchIngameMode)Ui()->DoDropDown(&Button, (int)OldDirectTouchIngame, apIngameTouchModes, std::size(apIngameTouchModes), s_DirectTouchIngameDropDownState);
	if(OldDirectTouchIngame != NewDirectTouchIngame)
	{
		GameClient()->m_TouchControls.SetDirectTouchIngame(NewDirectTouchIngame);
	}

	MainView.HSplitTop(40.0f, &Row, &MainView);
	Row.HMargin((Row.h - 20.0f) / 2.0f, &Row);
	Row.VSplitLeft(300.0f, &Label, &Row);
	Ui()->DoLabel(&Label, Localize("Direct touch input while spectating"), 16.0f, TEXTALIGN_ML);

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(150.0f, &Button, &Row);
	const char *apSpectateTouchModes[(int)CTouchControls::EDirectTouchSpectateMode::NUM_STATES] = {Localize("Disabled", "Direct touch input"), Localize("Aim", "Direct touch input")};
	const CTouchControls::EDirectTouchSpectateMode OldDirectTouchSpectate = GameClient()->m_TouchControls.DirectTouchSpectate();
	static CUi::SDropDownState s_DirectTouchSpectateDropDownState;
	static CScrollRegion s_DirectTouchSpectateDropDownScrollRegion;
	s_DirectTouchSpectateDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DirectTouchSpectateDropDownScrollRegion;
	const CTouchControls::EDirectTouchSpectateMode NewDirectTouchSpectate = (CTouchControls::EDirectTouchSpectateMode)Ui()->DoDropDown(&Button, (int)OldDirectTouchSpectate, apSpectateTouchModes, std::size(apSpectateTouchModes), s_DirectTouchSpectateDropDownState);
	if(OldDirectTouchSpectate != NewDirectTouchSpectate)
	{
		GameClient()->m_TouchControls.SetDirectTouchSpectate(NewDirectTouchSpectate);
	}
}

void CMenus::ParseLabel(const char *pLabel)
{
	json_settings JsonSettings{};
	char aError[256];
	char Buf[1048];
	str_format(Buf, sizeof(Buf), "{\"Label\":\"%s\"}", pLabel);
	json_value *pJsonLabel = json_parse_ex(&JsonSettings, Buf, str_length(Buf), aError);
	if(pJsonLabel == nullptr)
	{
		m_ParsedString = pLabel;
		return;
	}
	const json_value &Label = (*pJsonLabel)["Label"];
	m_ParsedString = Label.u.string.ptr;
	json_value_free(pJsonLabel);
}
/*
	Note: FindPositionXY is used for finding a position of the current moving rect not overlapping with other visible rects.
		  It's a bit slow, time = o(n^2 * logn), maybe need optimization in the future.

	General Logic: key elements: unique_ptr<CTouchButton>m_pTmpButton, optional<CUnitRect>m_ShownRect, CachedSettings(A group of elements stored in menus.h)
								   CTouchButton *m_pSelectedButton, m_vTouchButton, touch_controls.json
				   touch_controls.json stores all buttons that are already saved to the system, when you enter the game,
				   The buttons in touch_conrtols.json will be parsed into m_vTouchButton.
				   m_vTouchButton stores currently real ingame buttons, when you quit the editor, only buttons in m_vTouchButton will exist.
				   m_pSelectedButton is a pointer that points to an exact button in m_vTouchButton or nullptr, not anything else.
				   Its data shouldn't be changed anytime except when player wants to save the cached controls.
				   Upon changing the member it's pointing to, will check if there's unsaved changes, and popup confirm if saving data before changing.
				   If changes are made through sliding screen, and only for a small distance(<10000 unit), will not consider it as a change. Any changes made in editor will be considered as a change.
				   m_pTmpButton stores current settings, when it is nullptr, usually no button selected, no settings that cached(Cached settings won't be deleted. They still exist but meaningless), will update when changes are made in editor as well.
				   m_ShownRect for rendering the m_pTmpButton.
				   SelectedButton won't be rendered, instead it will render m_ShownRect(Using m_pTmpButton's behavior). While sliding on screen directly,
				   TmpButton will be overlapping with other buttons, m_ShownRect will get a position that not overlapping, as well closest to TmpButton.
				   So m_ShownRect could pass through blocks.
				   m_ShownRect is also used for saving the UnitRect data, so don't use m_pTmpButton's unitrect, it might be dangerous.
				   At any moment if there's no space for a button, FindPosition will return {-1, -1, -1, -1}, and then trigger the function NoSpaceForOverlappingButton().
				   This function will open editor automatically.

	Updates: Deleted the pointer that points to the joystick, instead made a counter that will be 0 if no joystick pressed.
			 TouchButton.Render() now has two default arguments. First one Selected == true to force Render() using activated button color, Selected == false using deactivated color. Second one force Render() to render the button using the given rect.
			 The default touch control has overlapping buttons.
		{
			"x": 100000,
			"y": 666667, (OVERLAPPING ONE UNIT)
			"w": 200000,
			"h": 166667,
			"shape": "rect",
			"visibilities": [
				"ingame"
			],
			"behavior": {
				"type": "bind",
				"label": "Jump",
				"label-type": "localized",
				"command": "+jump"
			}
		}, Overlapping the +left and +right buttons. So its "y" is changed to 666666.
*/
