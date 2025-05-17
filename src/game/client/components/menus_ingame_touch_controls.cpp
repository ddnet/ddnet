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
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_rect.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <string>

static const char *BEHAVIORS[] = {"Bind", "Bind Toggle", "Predefined"};
static const char *PREDEFINEDS[] = {"Extra Menu", "Joystick Hook", "Joystick Fire", "Joystick Aim", "Joystick Action", "Use Action", "Swap Action", "Spectate", "Emoticon", "Ingame Menu"};
static const char *LABELTYPES[] = {"Plain", "Localized", "Icon"};
static const constexpr float MAINMARGIN = 10.0f;
static const constexpr float SUBMARGIN = 5.0f;
static const constexpr float ROWSIZE = 25.0f;
static const constexpr float ROWGAP = 5.0f;
static const constexpr float FONTSIZE = 15.0f;

void CMenus::RenderTouchButtonEditor(CUIRect MainView)
{
	if(!GameClient()->m_TouchControls.IsButtonEditing())
	{
		RenderTouchButtonEditorWhileNothingSelected(MainView);
		return;
	}
	// Used to decide if need to update the tmpbutton.
	bool Changed = false;
	CUIRect Left, A, B, C, EditBox, Block;
	MainView.h = 2 * MAINMARGIN + 9 * ROWSIZE + 8 * ROWGAP;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);

	if(m_BindTogglePreviewExtension && m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE && m_EditElement == 2)
	{
		Block = MainView;
	}
	else
	{
		MainView.HSplitTop(5 * ROWSIZE + 5 * ROWGAP, &Block, &Left);
		Left.HSplitTop(ROWGAP, nullptr, &Left);
		Left.HSplitBottom(MAINMARGIN, &Left, nullptr);
	}

	// Choosing which to edit.
	EditBox.VSplitLeft(EditBox.w / 3.0f, &C, &EditBox);
	EditBox.VSplitMid(&A, &B);

	if(DoButton_MenuTab(m_aEditElementIds.data(), "Layout", m_EditElement == 0, &C, IGraphics::CORNER_TL, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = 0;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[1], "Visibility", m_EditElement == 1, &A, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = 1;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[2], "Behavior", m_EditElement == 2, &B, IGraphics::CORNER_TR, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = 2;
	}

	// Edit blocks.
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	switch(m_EditElement)
	{
	case 0: Changed = RenderLayoutSettingBlock(Block) || Changed; break;
	case 1: Changed = RenderVisibilitySettingBlock(Block) || Changed; break;
	case 2: Changed = RenderBehaviorSettingBlock(Block) || Changed; break;
	default: dbg_assert(false, "Unknown m_EditElement = %d.", m_EditElement);
	}

	// Leave some free space for the bind toggle preview.
	if(m_BindTogglePreviewExtension && m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE && m_EditElement == 2)
	{
		if(Changed)
		{
			UpdateTmpButton();
		}
		return;
	}

	// Save & Cancel & Hint.
	Left.HSplitTop(ROWSIZE, &EditBox, &Left);
	const float ButtonWidth = (EditBox.w - SUBMARGIN * 2.0f) / 3.0f;
	EditBox.VSplitLeft(ButtonWidth, &A, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	static CButtonContainer s_ConfirmButton;
	// After touching this button, the button is then added into the button vector. Or it is still virtual.
	const char *ConfirmText = "Save changes";
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
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	if(UnsavedChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&A, "Unsaved changes", 14.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	static CButtonContainer s_CancelButton;
	if(DoButton_Menu(&s_CancelButton, "Discard changes", UnsavedChanges() ? 0 : 1, &B))
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
	Left.HSplitTop(ROWGAP, nullptr, &Left);
	Left.HSplitTop(ROWSIZE, &EditBox, &Left);
	const float ButtonWidth2 = (EditBox.w - SUBMARGIN) / 2.0f;
	EditBox.VSplitLeft(ButtonWidth2, &A, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	static CButtonContainer s_AddNewButton;
	// If a new button has created, Checked==0.
	bool Checked = GameClient()->m_TouchControls.NoRealButtonSelected();
	if(DoButton_Menu(&s_AddNewButton, "New button", Checked ? 1 : 0, &A))
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
			PopupConfirm("Unsaved Changes", "Save all changes before creating another button?", "Save", "Cancel", &CMenus::PopupConfirm_NewButton);
		}
		else
		{
			PopupCancel_NewButton();
		}
	}

	static CButtonContainer s_RemoveButton;
	if(DoButton_Menu(&s_RemoveButton, "Delete", 0, &B))
	{
		PopupConfirm("Delete Button", "Are you sure to delete this button? This can't be undone.", "Delete", "Cancel", &CMenus::PopupConfirm_DeleteButton);
	}

	// Create a new button with current cached settings. New button will be automatically moved to nearest empty space.
	Left.HSplitTop(ROWGAP, nullptr, &Left);
	Left.HSplitTop(ROWSIZE, &EditBox, &Left);
	EditBox.VSplitLeft(ButtonWidth2, &A, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	static CButtonContainer s_CopyPasteButton;
	if(DoButton_Menu(&s_CopyPasteButton, "Duplicate", UnsavedChanges() || Checked ? 1 : 0, &A))
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
	if(Changed)
	{
		UpdateTmpButton();
	}
}

bool CMenus::RenderLayoutSettingBlock(CUIRect Block)
{
	bool Changed = false;
	CUIRect EditBox, A, B, PosX, PosY, PosW, PosH;
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&PosX, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputX, &EditBox, FONTSIZE))
	{
		InputPosFunction(&m_InputX);
		Changed = true;
	}

	// Auto check if the input value contains char that is not digit. If so delete it.
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&PosY, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputY, &EditBox, FONTSIZE))
	{
		InputPosFunction(&m_InputY);
		Changed = true;
	}

	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&PosW, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputW, &EditBox, FONTSIZE))
	{
		InputPosFunction(&m_InputW);
		Changed = true;
	}

	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&PosH, &EditBox);
	if(Ui()->DoClearableEditBox(&m_InputH, &EditBox, FONTSIZE))
	{
		InputPosFunction(&m_InputH);
		Changed = true;
	}
	int X = m_InputX.GetInteger();
	int Y = m_InputY.GetInteger();
	int W = m_InputW.GetInteger();
	int H = m_InputH.GetInteger();
	if(X < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("X:", PosX, FONTSIZE);
	else
		Ui()->DoLabel(&PosX, "X:", FONTSIZE, TEXTALIGN_ML);
	if(Y < 0 || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("Y:", PosY, FONTSIZE);
	else
		Ui()->DoLabel(&PosY, "Y:", FONTSIZE, TEXTALIGN_ML);
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel("Width:", PosW, FONTSIZE);
	else
		Ui()->DoLabel(&PosW, "Width:", FONTSIZE, TEXTALIGN_ML);
	if(H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel("Height:", PosH, FONTSIZE);
	else
		Ui()->DoLabel(&PosH, "Height:", FONTSIZE, TEXTALIGN_ML);

	// Drop down menu for shapes
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&A, &B);
	Ui()->DoLabel(&A, "Shape:", FONTSIZE, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonShapeDropDownState;
	static CScrollRegion s_ButtonShapeDropDownScrollRegion;
	s_ButtonShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonShapeDropDownScrollRegion;
	std::array<const char *, (int)CTouchControls::EButtonShape::NUM_SHAPES> aShapes = GameClient()->m_TouchControls.Shapes();
	const char *Shapes[(int)CTouchControls::EButtonShape::NUM_SHAPES];
	for(int Shape = 0; Shape < (int)CTouchControls::EButtonShape::NUM_SHAPES; Shape++)
	{
		Shapes[Shape] = aShapes[Shape];
	}
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
	if(m_vBindToggleAddButtons.size() < (unsigned)maximum((int)(m_vCachedCommands.size() + 1), 4))
	{
		m_vBindToggleAddButtons.resize(m_vCachedCommands.size() + 1);
		m_vBindToggleDeleteButtons.resize(m_vCachedCommands.size() + 1);
		m_vLabelTypeRadios.resize(m_vCachedCommands.size() + 1);
	}
	bool Changed = false;
	CUIRect EditBox, A, B, C;
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&A, &B);
	Ui()->DoLabel(&A, "Behavior type:", FONTSIZE, TEXTALIGN_ML);
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
			m_vInputLabels[0]->Set(m_ParsedString.c_str());
			m_vInputCommands[0]->Set(m_vCachedCommands[0].m_Command.c_str());
		}
		if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
		{
			if(m_vCachedCommands.size() <= (size_t)(m_EditCommandNumber))
				m_EditCommandNumber = 0;
			while(m_vCachedCommands.size() < 2)
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
			ParseLabel(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_vInputLabels[0]->Set(m_ParsedString.c_str());
			m_vInputCommands[0]->Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
		}
		SetUnsavedChanges(true);
		Changed = true;
	}

	if(m_EditBehaviorType != (int)EBehaviorType::BIND_TOGGLE)
	{
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&A, &B);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			Ui()->DoLabel(&A, "Command:", FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(&*(m_vInputCommands[0]), &B, 10.0f))
			{
				m_vCachedCommands[0].m_Command = m_vInputCommands[0]->GetString();
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
		{
			Ui()->DoLabel(&A, "Type:", FONTSIZE, TEXTALIGN_ML);
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
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&A, &B);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			Ui()->DoLabel(&A, "Label:", FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(&*(m_vInputLabels[0]), &B, 10.0f))
			{
				ParseLabel(m_vInputLabels[0]->GetString());
				m_vCachedCommands[0].m_Label = m_ParsedString;
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED && m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU) // Extra menu type, needs to input number.
		{
			// Increase & Decrease button share 1/2 width, the rest is for label.
			EditBox.VSplitLeft(ROWSIZE, &A, &B);
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

			B.VSplitRight(ROWSIZE, &A, &B);
			Ui()->DoLabel(&A, std::to_string(m_CachedNumber + 1).c_str(), FONTSIZE, TEXTALIGN_MC);
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
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&A, &B);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			Ui()->DoLabel(&A, "Label type:", FONTSIZE, TEXTALIGN_ML);
			int NewButtonLabelType = (int)m_vCachedCommands[0].m_LabelType;
			B.VSplitLeft(B.w / 3.0f, &A, &B);
			B.VSplitMid(&B, &C);
			if(DoButton_Menu(m_vLabelTypeRadios[0].data(), LABELTYPES[0], NewButtonLabelType == 0 ? 1 : 0, &A, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
				NewButtonLabelType = 0;
			if(DoButton_Menu(&m_vLabelTypeRadios[0][1], LABELTYPES[1], NewButtonLabelType == 1 ? 1 : 0, &B, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				NewButtonLabelType = 1;
			if(DoButton_Menu(&m_vLabelTypeRadios[0][2], LABELTYPES[2], NewButtonLabelType == 2 ? 1 : 0, &C, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
				NewButtonLabelType = 2;
			if(NewButtonLabelType != (int)m_vCachedCommands[0].m_LabelType)
			{
				Changed = true;
				SetUnsavedChanges(true);
				m_vCachedCommands[0].m_LabelType = (CTouchControls::CButtonLabel::EType)NewButtonLabelType;
			}
		}
	}
	else
	{
		if(m_BindTogglePreviewExtension)
			Block.HSplitBottom(MAINMARGIN, &Block, nullptr);
		Block.HSplitBottom(ROWSIZE, &Block, &EditBox);
		Block.HSplitBottom(SUBMARGIN, &Block, nullptr);
		static CButtonContainer s_ExtendButton;
		if(DoButton_Menu(&s_ExtendButton, m_BindTogglePreviewExtension ? "Fold list" : "Unfold list", 0, &EditBox))
		{
			m_BindTogglePreviewExtension = !m_BindTogglePreviewExtension;
		}
		static CScrollRegion s_BindToggleScrollRegion;
		CScrollRegionParams ScrollParam;
		ScrollParam.m_ScrollUnit = 90.0f;
		vec2 ScrollOffset(0.0f, 0.0f);
		s_BindToggleScrollRegion.Begin(&Block, &ScrollOffset, &ScrollParam);
		Block.y += ScrollOffset.y;
		for(unsigned CommandIndex = 0; CommandIndex < m_vCachedCommands.size(); CommandIndex++)
		{
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.VSplitMid(&EditBox, &C);
				C.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &C);
				EditBox.VSplitLeft(ROWSIZE, &B, &EditBox);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &A);
				Ui()->DoLabel(&A, "Add command", FONTSIZE, TEXTALIGN_ML);
				if(DoButton_FontIcon(&m_vBindToggleAddButtons[CommandIndex], "+", 0, &B, BUTTONFLAG_LEFT))
				{
					m_vCachedCommands.emplace(m_vCachedCommands.begin() + CommandIndex, "", CTouchControls::CButtonLabel::EType::PLAIN, "");
					m_vInputCommands.emplace(m_vInputCommands.begin() + CommandIndex);
					m_vInputLabels.emplace(m_vInputLabels.begin() + CommandIndex);
					InitLineInputs();
					m_vInputCommands[CommandIndex]->Set("");
					m_vInputLabels[CommandIndex]->Set("");
					Changed = true;
					SetUnsavedChanges(true);
				}
				C.VSplitLeft(ROWSIZE, &B, &C);
				C.VSplitLeft(SUBMARGIN, nullptr, &A);
				Ui()->DoLabel(&A, "Delete command", FONTSIZE, TEXTALIGN_ML);
				if(DoButton_FontIcon(&m_vBindToggleDeleteButtons[CommandIndex], "\xEF\x81\xA3", 0, &B, BUTTONFLAG_LEFT))
				{
					if(m_vCachedCommands.size() > 2)
					{
						m_vCachedCommands.erase(m_vCachedCommands.begin() + CommandIndex);
						m_vInputCommands.erase(m_vInputCommands.begin() + CommandIndex);
						m_vInputLabels.erase(m_vInputLabels.begin() + CommandIndex);
					}
					else
					{
						m_vCachedCommands[CommandIndex].m_Command = "";
						m_vCachedCommands[CommandIndex].m_Label = "";
						m_vCachedCommands[CommandIndex].m_LabelType = CTouchControls::CButtonLabel::EType::PLAIN;
						m_vInputCommands[CommandIndex]->Set("");
						m_vInputLabels[CommandIndex]->Set("");
					}
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			if(CommandIndex >= m_vCachedCommands.size())
				continue;
			Block.HSplitTop(ROWGAP, nullptr, &Block);
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.VSplitMid(&A, &B);
				B.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &B);
				Ui()->DoLabel(&A, "Command:", FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(&*m_vInputCommands[CommandIndex], &B, 10.0f))
				{
					m_vCachedCommands[CommandIndex].m_Command = m_vInputCommands[CommandIndex]->GetString();
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			Block.HSplitTop(ROWGAP, nullptr, &Block);
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.VSplitMid(&A, &B);
				B.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &B);
				Ui()->DoLabel(&A, "Label:", FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(&*m_vInputLabels[CommandIndex], &B, 10.0f))
				{
					ParseLabel(m_vInputLabels[CommandIndex]->GetString());
					m_vCachedCommands[CommandIndex].m_Label = m_ParsedString;
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			Block.HSplitTop(ROWGAP, nullptr, &Block);
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.VSplitMid(&A, &B);
				B.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &B);
				Ui()->DoLabel(&A, "Label type:", FONTSIZE, TEXTALIGN_ML);
				int NewButtonLabelType = (int)m_vCachedCommands[CommandIndex].m_LabelType;
				B.VSplitLeft(B.w / 3.0f, &A, &B);
				B.VSplitMid(&B, &C);
				if(DoButton_Menu(m_vLabelTypeRadios[CommandIndex].data(), LABELTYPES[0], NewButtonLabelType == 0 ? 1 : 0, &A, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
					NewButtonLabelType = 0;
				if(DoButton_Menu(&m_vLabelTypeRadios[CommandIndex][1], LABELTYPES[1], NewButtonLabelType == 1 ? 1 : 0, &B, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
					NewButtonLabelType = 1;
				if(DoButton_Menu(&m_vLabelTypeRadios[CommandIndex][2], LABELTYPES[2], NewButtonLabelType == 2 ? 1 : 0, &C, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
					NewButtonLabelType = 2;
				if(NewButtonLabelType != (int)m_vCachedCommands[CommandIndex].m_LabelType)
				{
					Changed = true;
					SetUnsavedChanges(true);
					m_vCachedCommands[CommandIndex].m_LabelType = (CTouchControls::CButtonLabel::EType)NewButtonLabelType;
				}
			}
			Block.HSplitTop(ROWGAP, nullptr, &Block);
		}
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		if(s_BindToggleScrollRegion.AddRect(EditBox))
		{
			EditBox.VSplitLeft(ROWSIZE, &B, &EditBox);
			EditBox.VSplitLeft(SUBMARGIN, nullptr, &A);
			Ui()->DoLabel(&A, "Add command", FONTSIZE, TEXTALIGN_ML);
			if(DoButton_FontIcon(&m_vBindToggleAddButtons[m_vCachedCommands.size()], "+", 0, &B, BUTTONFLAG_LEFT))
			{
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
				m_vInputCommands.emplace_back();
				m_vInputLabels.emplace_back();
				InitLineInputs();
				Changed = true;
				SetUnsavedChanges(true);
			}
		}
		s_BindToggleScrollRegion.End();
	}
	return Changed;
}

bool CMenus::RenderVisibilitySettingBlock(CUIRect Block)
{
	// Visibilities time. This is button's visibility, not virtual.
	bool Changed = false;
	CUIRect EditBox;

	static CScrollRegion s_VisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VisibilityScrollRegion.Begin(&Block, &ScrollOffset, &ScrollParam);
	Block.y += ScrollOffset.y;

	static std::vector<CButtonContainer> s_vVisibilitySelector[(int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES];
	if(s_vVisibilitySelector[0].empty())
		std::for_each_n(s_vVisibilitySelector, (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES, [](auto &Element) {
			Element.resize(3);
		});
	const std::vector<const char *> &vLabels = {"Included", "Excluded", "Ignored"};
	const std::array<const char *, (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> VisibilityStrings = GameClient()->m_TouchControls.VisibilityStrings();
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		Block.HSplitTop((ROWGAP + ROWSIZE), &EditBox, &Block);
		if(s_VisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(ROWGAP, nullptr, &EditBox);
			EditBox.VMargin(MAINMARGIN, &EditBox);
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

void CMenus::RenderTouchButtonEditorWhileNothingSelected(CUIRect MainView)
{
	CUIRect A, B, C, EditBox, LabelRect, CommandRect, X, Y, W, H;
	MainView.h = 3 * MAINMARGIN + 4 * ROWSIZE + 2 * ROWGAP + 250.0f;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(MAINMARGIN, &MainView);
	MainView.HSplitTop(ROWSIZE, &C, &MainView);
	Ui()->DoLabel(&C, "Long press on a touch button to select it.", 15.0f, TEXTALIGN_MC);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	EditBox.VSplitLeft((EditBox.w - SUBMARGIN) / 2.0f, &A, &EditBox);
	static CButtonContainer s_NewButton;
	if(DoButton_Menu(&s_NewButton, "New button", 0, &A))
		PopupCancel_NewButton();
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	static CButtonContainer s_SelecteButton;
	if(DoButton_Menu(&s_SelecteButton, "Select button by touch", 0, &B))
		SetActive(false);

	MainView.HSplitBottom(ROWSIZE, &MainView, &EditBox);
	MainView.HSplitBottom(ROWGAP, &MainView, nullptr);
	EditBox.VSplitLeft(ROWSIZE, &A, &EditBox);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&A, FontIcons::FONT_ICON_MAGNIFYING_GLASS, FONTSIZE, TEXTALIGN_ML);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &A, &EditBox);
	char aBufSearch[64];
	str_format(aBufSearch, sizeof(aBufSearch), "%s:", Localize("Search"));
	Ui()->DoLabel(&A, aBufSearch, FONTSIZE, TEXTALIGN_ML);

	if(Ui()->DoClearableEditBox(&m_FilterInput, &EditBox, FONTSIZE))
		m_NeedFilter = true;

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	// Makes the header labels looks better.
	MainView.HSplitTop(FONTSIZE / CUi::ms_FontmodHeight, &EditBox, &MainView);
	static CListBox s_PreviewListBox;
	EditBox.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	EditBox.VSplitRight(s_PreviewListBox.ScrollbarWidthMax(), &EditBox, nullptr);
	EditBox.VSplitLeft(ROWSIZE, nullptr, &EditBox);

	float PosWidth = EditBox.w * 4.0f / 9.0f; // Total pos width.
	const float ButtonWidth = (EditBox.w - PosWidth - SUBMARGIN) / 2.0f;
	PosWidth = (PosWidth - SUBMARGIN * 4.0f) / 4.0f; // Divided pos width.
	EditBox.VSplitLeft(ButtonWidth, &LabelRect, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	EditBox.VSplitLeft(ButtonWidth, &CommandRect, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	EditBox.VSplitLeft(ButtonWidth, &C, &EditBox);
	C.VSplitLeft(PosWidth, &X, &C);
	C.VSplitLeft(SUBMARGIN, nullptr, &C);
	C.VSplitLeft(PosWidth, &Y, &C);
	C.VSplitLeft(SUBMARGIN, nullptr, &C);
	C.VSplitLeft(PosWidth, &W, &C);
	C.VSplitLeft(SUBMARGIN, nullptr, &C);
	C.VSplitLeft(PosWidth, &H, &C);
	C.VSplitLeft(SUBMARGIN, nullptr, &C);
	const std::array<std::pair<CUIRect *, std::string>, (unsigned)ESortType::NUM_SORTS> HeaderDatas = {
		{{&LabelRect, "Label"},
			{&X, "X"},
			{&Y, "Y"},
			{&W, "Width"},
			{&H, "Height"}}};
	for(unsigned HeaderIndex = (unsigned)ESortType::LABEL; HeaderIndex < (unsigned)ESortType::NUM_SORTS; HeaderIndex++)
	{
		if(DoButton_GridHeader(&m_aSortHeaderIds[HeaderIndex], "",
			   (unsigned)m_SortType == HeaderIndex, HeaderDatas[HeaderIndex].first))
		{
			if(m_SortType != (ESortType)HeaderIndex)
			{
				m_NeedSort = true;
				m_SortType = (ESortType)HeaderIndex;
			}
		}
		Ui()->DoLabel(HeaderDatas[HeaderIndex].first, HeaderDatas[HeaderIndex].second.c_str(), FONTSIZE, TEXTALIGN_ML);
	}
	// Can't sort buttons basing on command, that's meaning less and too slow.
	Ui()->DoLabel(&CommandRect, "Command", FONTSIZE, TEXTALIGN_ML);

	if(m_NeedUpdatePreview)
	{
		m_NeedUpdatePreview = false;
		m_vVisibleButtons = GameClient()->m_TouchControls.VisibleButtons();
		m_vInvisibleButtons = GameClient()->m_TouchControls.InvisibleButtons();
		m_NeedSort = true;
		m_NeedFilter = true;
	}

	auto vVisibleButtons = m_vVisibleButtons;
	auto vInvisibleButtons = m_vInvisibleButtons;

	if(m_NeedFilter || m_NeedSort)
	{
		if(m_NeedFilter)
		{
			std::string FilterString = m_FilterInput.GetString();
			// The two will get the filter done. Both label and command will be considered.
			{
				const auto DeleteIt = std::remove_if(vVisibleButtons.begin(), vVisibleButtons.end(), [&](CTouchControls::CTouchButton *Button) {
					std::string Label = Button->m_pBehavior->GetLabel().m_pLabel;
					if(std::search(Label.begin(), Label.end(), FilterString.begin(), FilterString.end(), [](char &Char1, char &Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Label.end())
						return false;
					std::string Command = GetCommand(Button);
					if(std::search(Command.begin(), Command.end(), FilterString.begin(), FilterString.end(), [](char &Char1, char &Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Command.end())
						return false;
					return true;
				});
				vVisibleButtons.erase(DeleteIt, vVisibleButtons.end());
			}
			{
				const auto DeleteIt = std::remove_if(vInvisibleButtons.begin(), vInvisibleButtons.end(), [&](CTouchControls::CTouchButton *Button) {
					std::string Label = Button->m_pBehavior->GetLabel().m_pLabel;
					if(std::search(Label.begin(), Label.end(), FilterString.begin(), FilterString.end(), [](char Char1, char Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Label.end())
						return false;
					std::string Command = GetCommand(Button);
					if(std::search(Command.begin(), Command.end(), FilterString.begin(), FilterString.end(), [](char Char1, char Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Command.end())
						return false;
					return true;
				});
				vInvisibleButtons.erase(DeleteIt, vInvisibleButtons.end());
			}
		}
		if(m_NeedSort)
		{
			m_NeedSort = false;
			std::sort(vVisibleButtons.begin(), vVisibleButtons.end(), m_SortFunctions[(unsigned)m_SortType]);
			std::sort(vInvisibleButtons.begin(), vInvisibleButtons.end(), m_SortFunctions[(unsigned)m_SortType]);
		}
		m_vSortedButtons.clear();
		m_vSortedButtons.reserve(vVisibleButtons.size() + vInvisibleButtons.size());
		std::for_each(vVisibleButtons.begin(), vVisibleButtons.end(), [&](auto *Button) {
			m_vSortedButtons.emplace_back(Button);
		});
		std::for_each(vInvisibleButtons.begin(), vInvisibleButtons.end(), [&](auto *Button) {
			m_vSortedButtons.emplace_back(Button);
		});
	}

	if(m_vSortedButtons.size() != 0)
	{
		s_PreviewListBox.SetActive(true);
		s_PreviewListBox.DoStart(ROWSIZE, m_vSortedButtons.size(), 1, 4, -1, &MainView, true, IGraphics::CORNER_B);
		for(unsigned ButtonIndex = 0; ButtonIndex < m_vSortedButtons.size(); ButtonIndex++)
		{
			CTouchControls::CTouchButton *Button = m_vSortedButtons[ButtonIndex];
			const CListboxItem ListItem = s_PreviewListBox.DoNextItem(&m_vSortedButtons[ButtonIndex], m_SelectedPreviewButton == ButtonIndex);
			if(ListItem.m_Visible)
			{
				EditBox = ListItem.m_Rect;
				EditBox.VSplitLeft(ROWSIZE, &A, &EditBox);
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
				Ui()->DoLabel(&A, ButtonIndex >= vVisibleButtons.size() ? FontIcons::FONT_ICON_EYE_SLASH : FontIcons::FONT_ICON_EYE, FONTSIZE, TEXTALIGN_ML);
				TextRender()->SetRenderFlags(0);
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				EditBox.VSplitLeft(LabelRect.w, &A, &EditBox);
				std::string Label = Button->m_pBehavior->GetLabel().m_pLabel;
				const auto LabelType = Button->m_pBehavior->GetLabel().m_Type;
				if(LabelType == CTouchControls::CButtonLabel::EType::PLAIN)
				{
					LimitStringLength(Label, 24);
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::LOCALIZED)
				{
					Label = Localize(Label.c_str());
					LimitStringLength(Label, 24);
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::ICON)
				{
					LimitStringLength(Label, 12);
					TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML);
					TextRender()->SetRenderFlags(0);
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(CommandRect.w, &A, &EditBox);
				std::string Command = GetCommand(Button);
				LimitStringLength(Command, 24);
				Ui()->DoLabel(&A, Command.c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(X.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(Button->m_UnitRect.m_X).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(Y.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(Button->m_UnitRect.m_Y).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(W.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(Button->m_UnitRect.m_W).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(H.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(Button->m_UnitRect.m_H).c_str(), FONTSIZE, TEXTALIGN_ML);
			}
		}

		m_SelectedPreviewButton = s_PreviewListBox.DoEnd();
		if(s_PreviewListBox.WasItemActivated())
		{
			GameClient()->m_TouchControls.SetSelectedButton(m_vSortedButtons[m_SelectedPreviewButton]);
			CacheAllSettingsFromTarget(m_vSortedButtons[m_SelectedPreviewButton]);
			SetUnsavedChanges(false);
			UpdateTmpButton();
		}
	}
	else
	{
		// Copied from menus_browser.cpp
		MainView.HMargin((MainView.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &A);
		A.HSplitTop(16.0f, &A, &B);
		B.HSplitTop(8.0f, nullptr, &B);
		B.VMargin((B.w - 200.0f) / 2.0f, &B);
		Ui()->DoLabel(&A, "No buttons match your filter criteria", 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ResetButton;
		if(DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &B))
		{
			m_FilterInput.Clear();
		}
	}
}

void CMenus::RenderSelectingTab(CUIRect SelectingTab)
{
	CUIRect A;
	SelectingTab.VSplitLeft(SelectingTab.w / 4.0f, &A, &SelectingTab);
	static CButtonContainer s_FileTab;
	if(DoButton_MenuTab(&s_FileTab, "File", m_CurrentMenu == EMenuType::MENU_FILE, &A, IGraphics::CORNER_TL))
		m_CurrentMenu = EMenuType::MENU_FILE;
	SelectingTab.VSplitLeft(SelectingTab.w / 3.0f, &A, &SelectingTab);
	static CButtonContainer s_ButtonTab;
	if(DoButton_MenuTab(&s_ButtonTab, "Buttons", m_CurrentMenu == EMenuType::MENU_BUTTONS, &A, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_BUTTONS;
	SelectingTab.VSplitLeft(SelectingTab.w / 2.0f, &A, &SelectingTab);
	static CButtonContainer s_SettingsMenuTab;
	if(DoButton_MenuTab(&s_SettingsMenuTab, "Settings", m_CurrentMenu == EMenuType::MENU_SETTINGS, &A, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_SETTINGS;
	SelectingTab.VSplitLeft(SelectingTab.w / 1.0f, &A, &SelectingTab);
	static CButtonContainer s_PreviewTab;
	if(DoButton_MenuTab(&s_PreviewTab, "Preview", m_CurrentMenu == EMenuType::MENU_PREVIEW, &A, IGraphics::CORNER_TR))
		m_CurrentMenu = EMenuType::MENU_PREVIEW;
}

void CMenus::RenderConfigSettings(CUIRect MainView)
{
	CUIRect EditBox, Row, Label, Button;
	MainView.h = 2 * MAINMARGIN + 4 * ROWSIZE + 3 * ROWGAP;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	static CButtonContainer s_ActiveColorPicker;
	ColorHSLA ColorTest = DoLine_ColorPicker(&s_ActiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, "Active Color", &m_ColorActive, GameClient()->m_TouchControls.DefaultBackgroundColorActive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorActive(color_cast<ColorRGBA>(ColorHSLA(m_ColorActive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorActive())
		GameClient()->m_TouchControls.SetEditingChanges(true);

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	static CButtonContainer s_InactiveColorPicker;
	ColorTest = DoLine_ColorPicker(&s_InactiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, "Inactive Color", &m_ColorInactive, GameClient()->m_TouchControls.DefaultBackgroundColorInactive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorInactive(color_cast<ColorRGBA>(ColorHSLA(m_ColorInactive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorInactive())
		GameClient()->m_TouchControls.SetEditingChanges(true);

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	Row.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, Localize("Direct touch input while ingame"), FONTSIZE, TEXTALIGN_ML);

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

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	Row.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, Localize("Direct touch input while spectating"), FONTSIZE, TEXTALIGN_ML);

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

void CMenus::RenderPreviewSettings(CUIRect MainView)
{
	CUIRect EditBox;
	MainView.h += 100.0f;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	Ui()->DoLabel(&EditBox, "Preview button visibility while the editor is active.", FONTSIZE, TEXTALIGN_MC);
	MainView.HSplitBottom(ROWSIZE, &MainView, &EditBox);
	MainView.HSplitBottom(ROWGAP, &MainView, nullptr);
	EditBox.VSplitLeft(MAINMARGIN, nullptr, &EditBox);
	static CButtonContainer s_PreviewAllCheckBox;
	bool Preview = GameClient()->m_TouchControls.PreviewAllButtons();
	if(DoButton_CheckBox(&s_PreviewAllCheckBox, "Show all buttons", Preview ? 1 : 0, &EditBox))
	{
		GameClient()->m_TouchControls.SetPreviewAllButtons(!Preview);
	}
	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HMargin(SUBMARGIN, &MainView);
	static CScrollRegion s_VirtualVisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VirtualVisibilityScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParam);
	MainView.y += ScrollOffset.y;
	std::array<bool, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> aVirtualVisibilities = GameClient()->m_TouchControls.VirtualVisibilities();
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		MainView.HSplitTop(ROWSIZE + SUBMARGIN, &EditBox, &MainView);
		if(s_VirtualVisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(SUBMARGIN, nullptr, &EditBox);
			if(DoButton_CheckBox(&m_aVisibilityIds[Current], GameClient()->m_TouchControls.VisibilityStrings()[Current], aVirtualVisibilities[Current] == 1, &EditBox))
				GameClient()->m_TouchControls.ReverseVirtualVisibilities(Current);
		}
	}
	s_VirtualVisibilityScrollRegion.End();
}

void CMenus::RenderTouchControlsEditor(CUIRect MainView)
{
	CUIRect Label, Button, Row;
	MainView.h = 2 * MAINMARGIN + 4 * ROWSIZE + 3 * ROWGAP;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(MAINMARGIN, &MainView);

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	Row.VSplitLeft(Row.h, nullptr, &Row);
	Row.VSplitRight(Row.h, &Row, &Button);
	Row.VMargin(5.0f, &Label);
	Ui()->DoLabel(&Label, Localize("Edit touch controls"), 20.0f, TEXTALIGN_MC);

	static CButtonContainer s_OpenHelpButton;
	if(DoButton_FontIcon(&s_OpenHelpButton, FontIcons::FONT_ICON_QUESTION, 0, &Button, BUTTONFLAG_LEFT))
	{
		Client()->ViewLink(Localize("https://wiki.ddnet.org/wiki/Touch_controls"));
	}

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(ROWGAP, nullptr, &MainView);

	Row.VSplitLeft((Row.w - SUBMARGIN) / 2.0f, &Button, &Row);
	static CButtonContainer s_SaveConfigurationButton;
	if(DoButton_Menu(&s_SaveConfigurationButton, Localize("Save changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
	{
		if(GameClient()->m_TouchControls.SaveConfigurationToFile())
		{
			GameClient()->m_TouchControls.SetEditingChanges(false);
		}
		else
		{
			SWarning Warning(Localize("Error saving touch controls"), Localize("Could not save touch controls to file. See local console for details."));
			Warning.m_AutoHide = false;
			Client()->AddWarning(Warning);
		}
	}

	Row.VSplitLeft(SUBMARGIN, nullptr, &Button);
	if(GameClient()->m_TouchControls.HasEditingChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&Button, Localize("Unsaved changes"), 14.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(ROWGAP, nullptr, &MainView);

	Row.VSplitLeft((Row.w - SUBMARGIN) / 2.0f, &Button, &Row);
	static CButtonContainer s_DiscardChangesButton;
	if(DoButton_Menu(&s_DiscardChangesButton, Localize("Discard changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
	{
		PopupConfirm(Localize("Discard changes"),
			Localize("Are you sure that you want to discard the current changes to the touch controls?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmDiscardTouchControlsChanges);
	}

	Row.VSplitLeft(SUBMARGIN, nullptr, &Button);
	static CButtonContainer s_ResetButton;
	if(DoButton_Menu(&s_ResetButton, Localize("Reset to defaults"), 0, &Button))
	{
		PopupConfirm(Localize("Reset to defaults"),
			Localize("Are you sure that you want to reset the touch controls to default?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmResetTouchControls);
	}

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);

	Row.VSplitLeft((Row.w - SUBMARGIN) / 2.0f, &Button, &Row);
	static CButtonContainer s_ClipboardImportButton;
	if(DoButton_Menu(&s_ClipboardImportButton, Localize("Import from clipboard"), 0, &Button))
	{
		PopupConfirm(Localize("Import from clipboard"),
			Localize("Are you sure that you want to import the touch controls from the clipboard? This will overwrite your current touch controls."),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmImportTouchControlsClipboard);
	}

	Row.VSplitLeft(SUBMARGIN, nullptr, &Button);
	static CButtonContainer s_ClipboardExportButton;
	if(DoButton_Menu(&s_ClipboardExportButton, Localize("Export to clipboard"), 0, &Button))
	{
		GameClient()->m_TouchControls.SaveConfigurationToClipboard();
	}
}

// Check if CTouchControls need CMenus to open any popups.
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

void CMenus::ChangeSelectedButtonWhileHavingUnsavedChanges()
{
	// Both old and new button pointer can be nullptr.
	// Saving settings to the old selected button(nullptr = create), then switch to new selected button(new = haven't created).
	PopupConfirm("Unsaved Changes", "Save all changes before switching selected button?", "Save", "Discard", &CMenus::PopupConfirm_ChangeSelectedButton, POPUP_NONE, &CMenus::PopupCancel_ChangeSelectedButton);
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

// Also a safe function for changing selected button.
void CMenus::PopupCancel_ChangeSelectedButton()
{
	GameClient()->m_TouchControls.SetSelectedButton(m_NewSelectedButton);
	CacheAllSettingsFromTarget(m_NewSelectedButton);
	SetUnsavedChanges(false);
	if(m_NewSelectedButton != nullptr)
	{
		UpdateTmpButton();
	}
	else
	{
		ResetButtonPointers();
	}
	if(m_CloseMenu)
		SetActive(false);
}

void CMenus::NoSpaceForOverlappingButton()
{
	PopupMessage("No Space", "No space left for the button. Make sure you didn't choose wrong visibilities, or edit its size.", "OK");
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

void CMenus::PopupConfirm_SaveSettings()
{
	SetUnsavedChanges(false);
	SaveCachedSettingsToTarget(m_OldSelectedButton);
}

void CMenus::PopupCancel_DeselectButton()
{
	ResetButtonPointers();
	SetUnsavedChanges(false);
	ResetCachedSettings();
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

void CMenus::PopupConfirm_DeleteButton()
{
	GameClient()->m_TouchControls.DeleteButton();
	ResetCachedSettings();
	GameClient()->m_TouchControls.SetEditingChanges(true);
}

bool CMenus::UnsavedChanges()
{
	return GameClient()->m_TouchControls.UnsavedChanges();
}

void CMenus::SetUnsavedChanges(bool UnsavedChanges)
{
	GameClient()->m_TouchControls.SetUnsavedChanges(UnsavedChanges);
}

// Check if cached settings are legal.
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

// All default settings are here.
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
	m_vInputCommands.resize(2);
	m_vInputLabels.resize(2);
	InitLineInputs();
	std::for_each(m_vInputLabels.begin(), m_vInputLabels.end(), [](auto &LineInput) {
		LineInput->Set("");
	});
	std::for_each(m_vInputCommands.begin(), m_vInputCommands.end(), [](auto &LineInput) {
		LineInput->Set("");
	});
	m_CachedShape = CTouchControls::EButtonShape::RECT;
}

// This is called when the Touch button editor is rendered as well when selectedbutton changes. Used for updating all cached settings.
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
			m_vInputCommands[0]->Set(CastedBehavior->GetCommand().c_str());
			ParseLabel(CastedBehavior->GetLabel().m_pLabel);
			m_vInputLabels[0]->Set(m_ParsedString.c_str());
		}
		else if(BehaviorType == "bind-toggle")
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND_TOGGLE;
			auto *CastedBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
			m_vCachedCommands = CastedBehavior->GetCommand();
			m_EditCommandNumber = 0;
			if(!m_vCachedCommands.empty())
			{
				if(m_vCachedCommands.size() != m_vInputCommands.size())
					m_vInputCommands.resize(m_vCachedCommands.size());
				if(m_vCachedCommands.size() != m_vInputLabels.size())
					m_vInputLabels.resize(m_vCachedCommands.size());
				InitLineInputs();
				for(unsigned CommandIndex = 0; CommandIndex < m_vCachedCommands.size(); CommandIndex++)
				{
					ParseLabel(m_vCachedCommands[CommandIndex].m_Label.c_str());
					m_vInputLabels[CommandIndex]->Set(m_ParsedString.c_str());
					m_vInputCommands[CommandIndex]->Set(m_vCachedCommands[CommandIndex].m_Command.c_str());
				}
			}
		}
		else if(BehaviorType == "predefined")
		{
			m_EditBehaviorType = (int)EBehaviorType::PREDEFINED;
			const char *PredefinedType = TargetButton->m_pBehavior->GetPredefinedType();
			if(PredefinedType == nullptr)
				m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
			else
				m_PredefinedBehaviorType = CalculatePredefinedType(PredefinedType);

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

// Will override everything in the button. If nullptr is passed, a new button will be created.
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

// Used for setting the value of four position input box to the unitrect.
void CMenus::SetPosInputs(CTouchControls::CUnitRect MyRect)
{
	m_InputX.SetInteger(MyRect.m_X);
	m_InputY.SetInteger(MyRect.m_Y);
	m_InputW.SetInteger(MyRect.m_W);
	m_InputH.SetInteger(MyRect.m_H);
}

// Used to make sure the input box is numbers only, also clamp the value.
void CMenus::InputPosFunction(CLineInputNumber *Input)
{
	int InputValue = Input->GetInteger();
	// Deal with the "-1" FindPositionXY give.
	InputValue = clamp(InputValue, 0, CTouchControls::BUTTON_SIZE_SCALE);
	Input->SetInteger(InputValue);
	SetUnsavedChanges(true);
}

// Update m_pTmpButton in CTouchControls. The Tmpbutton is used for showing on screen.
void CMenus::UpdateTmpButton()
{
	GameClient()->m_TouchControls.RemakeTmpButton();
	SaveCachedSettingsToTarget(GameClient()->m_TouchControls.TmpButton());
	GameClient()->m_TouchControls.SetShownRect(GameClient()->m_TouchControls.TmpButton()->m_UnitRect);
}

// Not inline so there's no more includes in menus.h. A shortcut to the function in CTouchControls.
void CMenus::ResetButtonPointers()
{
	GameClient()->m_TouchControls.ResetButtonPointers();
}

// Mainly used in Layout tab, to ensure readability.
void CMenus::DoRedLabel(const char *pLabel, CUIRect &Block, const int &Size)
{
	if(pLabel == nullptr)
		return;
	TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
	Ui()->DoLabel(&Block, pLabel, Size, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// Used for making json chars like \n or \uf3ce visible.
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

// Used for updating cached settings or something else only when opening the editor, to reduce lag. Issues come from CTouchControls.
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
			case(int)CTouchControls::EIssueType::CACHE_POSITION: SetPosInputs(Issues[Current].m_TargetButton->m_UnitRect); break;
			default: dbg_assert(false, "Unknown Issue.");
			}
		}
	}
}

// Turn behavior strings like "bind-toggle" into integers according to the enum.
int CMenus::CalculateBehaviorType(const char *Type)
{
	if(str_comp(Type, CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		return (int)EBehaviorType::BIND;
	if(str_comp(Type, CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		return (int)EBehaviorType::BIND_TOGGLE;
	if(str_comp(Type, CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		return (int)EBehaviorType::PREDEFINED;
	return (int)EBehaviorType::NUM_BEHAVIORS;
}

// Turn predefined behavior strings like "joystick-hook" into integers according to the enum.
int CMenus::CalculatePredefinedType(const char *Type)
{
	int IntegerType;
	for(IntegerType = (int)EPredefinedType::EXTRA_MENU;
		str_comp(Type, GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[IntegerType].m_pId) != 0 && IntegerType < (int)EPredefinedType::NUM_PREDEFINEDS;
		IntegerType++)
		;
	return IntegerType;
}

void CMenus::LimitStringLength(std::string &Target, unsigned MaxLength)
{
	if(Target.length() <= MaxLength)
		return;
	char Buf[256];
	str_format(Buf, sizeof(Buf), "%s...", Target.substr(0, MaxLength).c_str());
	Target = Buf;
}

void CMenus::InitLineInputs()
{
	std::for_each(m_vInputLabels.begin(), m_vInputLabels.end(), [](auto &Input) {
		if(Input == nullptr)
			Input = std::make_unique<CLineInputBuffered<1024>>();
	});
	std::for_each(m_vInputCommands.begin(), m_vInputCommands.end(), [](auto &Input) {
		if(Input == nullptr)
			Input = std::make_unique<CLineInputBuffered<1024>>();
	});
}

std::string CMenus::GetCommand(CTouchControls::CTouchButton *Button)
{
	std::string BehaviorType = Button->m_pBehavior->GetBehaviorType();
	if(BehaviorType == CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		return static_cast<CTouchControls::CBindTouchButtonBehavior *>(Button->m_pBehavior.get())->GetCommand();
	}
	else if(BehaviorType == CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		const auto *Behavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(Button->m_pBehavior.get());
		return Behavior->GetCommand()[Behavior->GetActiveCommandIndex()].m_Command;
	}
	else if(BehaviorType == CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		std::string PredefinedType = Button->m_pBehavior->GetPredefinedType();
		std::string Command = PREDEFINEDS[CalculatePredefinedType(PredefinedType.c_str())];
		if(PredefinedType == CTouchControls::CExtraMenuTouchButtonBehavior::BEHAVIOR_ID)
		{
			Command += " " + std::to_string(static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(Button->m_pBehavior.get())->GetNumber());
		}
		return Command;
	}
	else
	{
		dbg_assert(false, "Detected unknown behavior type in CMenus::GetCommand()");
	}
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
