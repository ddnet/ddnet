#include "menus_ingame_touch_controls.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/localization.h>
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

static const constexpr float MAINMARGIN = 10.0f;
static const constexpr float SUBMARGIN = 5.0f;
static const constexpr float ROWSIZE = 25.0f;
static const constexpr float ROWGAP = 5.0f;
static const constexpr float FONTSIZE = 15.0f;

void CMenusIngameTouchControls::RenderTouchButtonEditor(CUIRect MainView)
{
	if(!GameClient()->m_TouchControls.IsButtonEditing())
	{
		RenderTouchButtonEditorWhileNothingSelected(MainView);
		return;
	}
	// Used to decide if need to update the Samplebutton.
	bool Changed = false;
	CUIRect Functional, LeftButton, MiddleButton, RightButton, EditBox, Block;
	MainView.h = 600.0f - 40.0f - MainView.y;
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);

	MainView.HSplitBottom(2 * ROWSIZE + 2 * ROWGAP + MAINMARGIN, &Block, &Functional);
	Functional.HSplitTop(ROWGAP, nullptr, &Functional);
	Functional.HSplitBottom(MAINMARGIN, &Functional, nullptr);

	// Choosing which to edit.
	EditBox.VSplitLeft(EditBox.w / 3.0f, &RightButton, &EditBox);
	EditBox.VSplitMid(&LeftButton, &MiddleButton);

	if(GameClient()->m_Menus.DoButton_MenuTab(m_aEditElementIds.data(), Localize("Layout"), m_EditElement == EElementType::LAYOUT, &RightButton, IGraphics::CORNER_TL, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::LAYOUT;
	}
	if(GameClient()->m_Menus.DoButton_MenuTab(&m_aEditElementIds[1], Localize("Visibility"), m_EditElement == EElementType::VISIBILITY, &LeftButton, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::VISIBILITY;
	}
	if(GameClient()->m_Menus.DoButton_MenuTab(&m_aEditElementIds[2], Localize("Behavior"), m_EditElement == EElementType::BEHAVIOR, &MiddleButton, IGraphics::CORNER_TR, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::BEHAVIOR;
	}

	// Edit blocks.
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	Block.VMargin(SUBMARGIN, &Block);
	switch(m_EditElement)
	{
	case EElementType::LAYOUT: Changed = RenderLayoutSettingBlock(Block) || Changed; break;
	case EElementType::VISIBILITY: Changed = RenderVisibilitySettingBlock(Block) || Changed; break;
	case EElementType::BEHAVIOR: Changed = RenderBehaviorSettingBlock(Block) || Changed; break;
	default: dbg_assert(false, "Unknown m_EditElement = %d.", m_EditElement);
	}

	// Save & Cancel & Hint.
	Functional.HSplitTop(ROWSIZE, &EditBox, &Functional);
	const float ButtonWidth = (EditBox.w - SUBMARGIN * 2.0f) / 3.0f;
	EditBox.VSplitLeft(ButtonWidth, &LeftButton, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	static CButtonContainer s_ConfirmButton;
	// After touching this button, the button is then added into the button vector. Or it is still virtual.
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConfirmButton, Localize("Save changes"), UnsavedChanges() ? 0 : 1, &LeftButton))
	{
		if(UnsavedChanges())
		{
			m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
			if(CheckCachedSettings())
			{
				SaveCachedSettingsToTarget(m_pOldSelectedButton);
				GameClient()->m_TouchControls.SetEditingChanges(true);
				SetUnsavedChanges(false);
			}
		}
	}
	EditBox.VSplitLeft(ButtonWidth, &LeftButton, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &MiddleButton);
	if(UnsavedChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&LeftButton, Localize("Unsaved changes"), 14.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	static CButtonContainer s_CancelButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_CancelButton, Localize("Discard changes"), UnsavedChanges() ? 0 : 1, &MiddleButton))
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
	Functional.HSplitTop(ROWGAP, nullptr, &Functional);
	Functional.HSplitTop(ROWSIZE, &EditBox, &Functional);
	EditBox.VSplitLeft(ButtonWidth, &LeftButton, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	static CButtonContainer s_RemoveButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_RemoveButton, Localize("Delete"), 0, &LeftButton))
	{
		GameClient()->m_Menus.PopupConfirm(Localize("Delete button"), Localize("Are you sure that you want to delete this button?"), Localize("Delete"), Localize("Cancel"), &CMenus::PopupConfirmDeleteButton);
	}

	EditBox.VSplitLeft(ButtonWidth, &LeftButton, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &MiddleButton);
	// Create a new button with current cached settings. New button will be automatically moved to nearest empty space.
	static CButtonContainer s_CopyPasteButton;
	bool Checked = GameClient()->m_TouchControls.NoRealButtonSelected();
	if(GameClient()->m_Menus.DoButton_Menu(&s_CopyPasteButton, Localize("Duplicate"), UnsavedChanges() || Checked ? 1 : 0, &LeftButton))
	{
		if(Checked)
		{
			GameClient()->m_Menus.PopupMessage(Localize("New button already created"), Localize("A new button has already been created, please save or delete it before creating another one."), Localize("Ok"));
		}
		else if(UnsavedChanges())
		{
			GameClient()->m_Menus.PopupMessage(Localize("Unsaved changes"), Localize("Please save your changes before duplicating a button."), Localize("Ok"));
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
					GameClient()->m_Menus.PopupMessage(Localize("No space for button"), Localize("There is not enough space available to place another button."), Localize("Ok"));
				}
				else
				{
					GameClient()->m_Menus.PopupMessage(Localize("No space for button"), Localize("There is not enough space available to place another button with this size. The button has been resized."), Localize("Ok"));
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
	if(GameClient()->m_Menus.DoButton_Menu(&s_DeselectButton, Localize("Deselect"), 0, &MiddleButton))
	{
		m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_pNewSelectedButton = nullptr;
		if(UnsavedChanges())
		{
			GameClient()->m_Menus.PopupConfirm(Localize("Unsaved changes"), Localize("You'll lose unsaved changes after deselecting."), Localize("Deselect"), Localize("Cancel"), &CMenus::PopupCancelDeselectButton);
		}
		else
		{
			GameClient()->m_Menus.PopupCancelDeselectButton();
		}
	}

	// This ensures m_pSampleButton being updated always.
	if(Changed)
	{
		UpdateSampleButton();
	}
}

bool CMenusIngameTouchControls::RenderLayoutSettingBlock(CUIRect Block)
{
	bool Changed = false;
	CUIRect EditBox, LeftButton, RightButton, PosX, PosY, PosW, PosH;
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
	auto DoRedLabel = [&](const char *pLabel, CUIRect &LabelBlock, const int &Size) {
		if(pLabel == nullptr)
			return;
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&LabelBlock, pLabel, Size, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	// There're strings without ":" below, so don't localize these with ":" together.
	char aBuf[64];
	if(X < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("X:", PosX, FONTSIZE);
	else
		Ui()->DoLabel(&PosX, "X:", FONTSIZE, TEXTALIGN_ML);

	if(Y < 0 || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel("Y:", PosY, FONTSIZE);
	else
		Ui()->DoLabel(&PosY, "Y:", FONTSIZE, TEXTALIGN_ML);

	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Width"));
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel(aBuf, PosW, FONTSIZE);
	else
		Ui()->DoLabel(&PosW, aBuf, FONTSIZE, TEXTALIGN_ML);

	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Height"));
	if(H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel(aBuf, PosH, FONTSIZE);
	else
		Ui()->DoLabel(&PosH, aBuf, FONTSIZE, TEXTALIGN_ML);

	// Drop down menu for shapes
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&LeftButton, &RightButton);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Shape"));
	const char *apShapes[] = {Localize("Rectangle", "Touch button shape"), Localize("Circle", "Touch button shape")};
	Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonShapeDropDownState;
	static CScrollRegion s_ButtonShapeDropDownScrollRegion;
	s_ButtonShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonShapeDropDownScrollRegion;
	const CTouchControls::EButtonShape NewButtonShape = (CTouchControls::EButtonShape)Ui()->DoDropDown(&RightButton, (int)m_CachedShape, apShapes, (int)CTouchControls::EButtonShape::NUM_SHAPES, s_ButtonShapeDropDownState);
	if(NewButtonShape != m_CachedShape)
	{
		m_CachedShape = NewButtonShape;
		SetUnsavedChanges(true);
		Changed = true;
	}
	return Changed;
}

bool CMenusIngameTouchControls::RenderBehaviorSettingBlock(CUIRect Block)
{
	const char *apLabelTypes[] = {Localize("Plain", "Touch button label type"), Localize("Localized", "Touch button label type"), Localize("Icon", "Touch button label type")};
	bool Changed = false;
	CUIRect EditBox, LeftButton, MiddleButton, RightButton;
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&LeftButton, &MiddleButton);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Behavior type"));
	Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonBehaviorDropDownState;
	static CScrollRegion s_ButtonBehaviorDropDownScrollRegion;
	s_ButtonBehaviorDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonBehaviorDropDownScrollRegion;
	const char *apBehaviors[] = {Localize("Bind", "Touch button behavior"), Localize("Bind Toggle", "Touch button behavior"), Localize("Predefined", "Touch button behavior")};
	const int NewButtonBehavior = Ui()->DoDropDown(&MiddleButton, m_EditBehaviorType, apBehaviors, std::size(apBehaviors), s_ButtonBehaviorDropDownState);
	if(NewButtonBehavior != m_EditBehaviorType)
	{
		m_EditBehaviorType = NewButtonBehavior;
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			m_vBehaviorElements[0].UpdateInputs();
		}
		SetUnsavedChanges(true);
		Changed = true;
	}
	if(m_EditBehaviorType != (int)EBehaviorType::BIND_TOGGLE)
	{
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&LeftButton, &MiddleButton);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			str_format(aBuf, sizeof(aBuf), "%s:", Localize("Command"));
			Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(m_vBehaviorElements[0].m_InputCommand.get(), &MiddleButton, 10.0f))
			{
				m_vBehaviorElements[0].UpdateCommand();
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
		{
			str_format(aBuf, sizeof(aBuf), "%s:", Localize("Type"));
			Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
			static CUi::SDropDownState s_ButtonPredefinedDropDownState;
			static CScrollRegion s_ButtonPredefinedDropDownScrollRegion;
			const char *apPredefineds[] = {Localize("Extra Menu", "Predefined touch button behaviors"), Localize("Joystick Hook", "Predefined touch button behaviors"), Localize("Joystick Fire", "Predefined touch button behaviors"), Localize("Joystick Aim", "Predefined touch button behaviors"), Localize("Joystick Action"), Localize("Use Action"), Localize("Swap Action", "Predefined touch button behaviors"), Localize("Spectate", "Predefined touch button behaviors"), Localize("Emoticon", "Predefined touch button behaviors"), Localize("Ingame Menu", "Predefined touch button behaviors")};
			s_ButtonPredefinedDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonPredefinedDropDownScrollRegion;
			const int NewPredefined = Ui()->DoDropDown(&MiddleButton, m_PredefinedBehaviorType, apPredefineds, std::size(apPredefineds), s_ButtonPredefinedDropDownState);
			if(NewPredefined != m_PredefinedBehaviorType)
			{
				m_PredefinedBehaviorType = NewPredefined;
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&LeftButton, &MiddleButton);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			str_format(aBuf, sizeof(aBuf), "%s:", Localize("Label"));
			Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(m_vBehaviorElements[0].m_InputLabel.get(), &MiddleButton, 10.0f))
			{
				m_vBehaviorElements[0].UpdateLabel();
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED && m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU) // Extra menu type, needs to input number.
		{
			// Increase & Decrease button share 1/2 width, the rest is for label.
			EditBox.VSplitLeft(ROWSIZE, &LeftButton, &MiddleButton);
			static CButtonContainer s_ExtraMenuDecreaseButton;
			if(Ui()->DoButton_FontIcon(&s_ExtraMenuDecreaseButton, "-", 0, &LeftButton, BUTTONFLAG_LEFT))
			{
				if(m_CachedExtraMenuNumber > 0)
				{
					// Menu Number also counts from 1, but written as 0.
					m_CachedExtraMenuNumber--;
					SetUnsavedChanges(true);
					Changed = true;
				}
			}

			MiddleButton.VSplitRight(ROWSIZE, &LeftButton, &MiddleButton);
			Ui()->DoLabel(&LeftButton, std::to_string(m_CachedExtraMenuNumber + 1).c_str(), FONTSIZE, TEXTALIGN_MC);
			static CButtonContainer s_ExtraMenuIncreaseButton;
			if(Ui()->DoButton_FontIcon(&s_ExtraMenuIncreaseButton, "+", 0, &MiddleButton, BUTTONFLAG_LEFT))
			{
				if(m_CachedExtraMenuNumber < 4)
				{
					m_CachedExtraMenuNumber++;
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
		}
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		EditBox.VSplitMid(&LeftButton, &MiddleButton);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			str_format(aBuf, sizeof(aBuf), "%s:", Localize("Label type"));
			Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
			int NewButtonLabelType = (int)m_vBehaviorElements[0].m_CachedCommands.m_LabelType;
			MiddleButton.VSplitLeft(MiddleButton.w / 3.0f, &LeftButton, &MiddleButton);
			MiddleButton.VSplitMid(&MiddleButton, &RightButton);
			if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[0], apLabelTypes[0], NewButtonLabelType == 0 ? 1 : 0, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
				NewButtonLabelType = 0;
			if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[1], apLabelTypes[1], NewButtonLabelType == 1 ? 1 : 0, &MiddleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				NewButtonLabelType = 1;
			if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[2], apLabelTypes[2], NewButtonLabelType == 2 ? 1 : 0, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
				NewButtonLabelType = 2;
			if(NewButtonLabelType != (int)m_vBehaviorElements[0].m_CachedCommands.m_LabelType)
			{
				Changed = true;
				SetUnsavedChanges(true);
				m_vBehaviorElements[0].m_CachedCommands.m_LabelType = (CTouchControls::CButtonLabel::EType)NewButtonLabelType;
			}
		}
	}
	else
	{
		static CScrollRegion s_BindToggleScrollRegion;
		CScrollRegionParams ScrollParam;
		ScrollParam.m_ScrollUnit = 90.0f;
		vec2 ScrollOffset(0.0f, 0.0f);
		s_BindToggleScrollRegion.Begin(&Block, &ScrollOffset, &ScrollParam);
		Block.y += ScrollOffset.y;
		for(unsigned CommandIndex = 0; CommandIndex < m_vBehaviorElements.size(); CommandIndex++)
		{
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_T, 5.0f);
				EditBox.VSplitMid(&EditBox, &RightButton);
				RightButton.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &RightButton);
				EditBox.VSplitLeft(ROWSIZE, &MiddleButton, &EditBox);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &LeftButton);
				Ui()->DoLabel(&LeftButton, Localize("Add command"), FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoButton_FontIcon(&m_vBehaviorElements[CommandIndex].m_BindToggleAddButtons, "+", 0, &MiddleButton, BUTTONFLAG_LEFT))
				{
					m_vBehaviorElements.emplace(m_vBehaviorElements.begin() + CommandIndex, GameClient());
					m_vBehaviorElements[CommandIndex].UpdateInputs();
					Changed = true;
					SetUnsavedChanges(true);
				}
				RightButton.VSplitLeft(ROWSIZE, &MiddleButton, &RightButton);
				RightButton.VSplitLeft(SUBMARGIN, nullptr, &LeftButton);
				Ui()->DoLabel(&LeftButton, Localize("Delete command"), FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoButton_FontIcon(&m_vBehaviorElements[CommandIndex].m_BindToggleDeleteButtons, FontIcons::FONT_ICON_TRASH, 0, &MiddleButton, BUTTONFLAG_LEFT))
				{
					if(m_vBehaviorElements.size() > 2)
					{
						m_vBehaviorElements.erase(m_vBehaviorElements.begin() + CommandIndex);
						continue;
					}
					else
					{
						m_vBehaviorElements[CommandIndex].Reset();
					}
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			Block.HSplitTop(ROWGAP, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
			}
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
				EditBox.VSplitMid(&LeftButton, &MiddleButton);
				MiddleButton.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &MiddleButton);
				str_format(aBuf, sizeof(aBuf), "%s:", Localize("Command"));
				Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(m_vBehaviorElements[CommandIndex].m_InputCommand.get(), &MiddleButton, 10.0f))
				{
					m_vBehaviorElements[CommandIndex].UpdateCommand();
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			Block.HSplitTop(ROWGAP, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
			}
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
				EditBox.VSplitMid(&LeftButton, &MiddleButton);
				MiddleButton.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &MiddleButton);
				str_format(aBuf, sizeof(aBuf), "%s:", Localize("Label"));
				Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(m_vBehaviorElements[CommandIndex].m_InputLabel.get(), &MiddleButton, 10.0f))
				{
					m_vBehaviorElements[CommandIndex].UpdateLabel();
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			Block.HSplitTop(ROWGAP, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);
			}
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
				EditBox.VSplitMid(&LeftButton, &MiddleButton);
				MiddleButton.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &MiddleButton);
				str_format(aBuf, sizeof(aBuf), "%s:", Localize("Label type"));
				Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
				int NewButtonLabelType = (int)m_vBehaviorElements[CommandIndex].m_CachedCommands.m_LabelType;
				MiddleButton.VSplitLeft(MiddleButton.w / 3.0f, &LeftButton, &MiddleButton);
				MiddleButton.VSplitMid(&MiddleButton, &RightButton);
				if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[0], apLabelTypes[0], NewButtonLabelType == 0 ? 1 : 0, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
					NewButtonLabelType = 0;
				if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[1], apLabelTypes[1], NewButtonLabelType == 1 ? 1 : 0, &MiddleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
					NewButtonLabelType = 1;
				if(GameClient()->m_Menus.DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[0], apLabelTypes[2], NewButtonLabelType == 2 ? 1 : 0, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
					NewButtonLabelType = 2;
				if(NewButtonLabelType != (int)m_vBehaviorElements[CommandIndex].m_CachedCommands.m_LabelType)
				{
					Changed = true;
					SetUnsavedChanges(true);
					m_vBehaviorElements[CommandIndex].m_CachedCommands.m_LabelType = (CTouchControls::CButtonLabel::EType)NewButtonLabelType;
				}
			}
			Block.HSplitTop(ROWGAP, nullptr, &Block);
		}
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		if(s_BindToggleScrollRegion.AddRect(EditBox))
		{
			EditBox.VSplitLeft(ROWSIZE, &MiddleButton, &EditBox);
			EditBox.VSplitLeft(SUBMARGIN, nullptr, &LeftButton);
			Ui()->DoLabel(&LeftButton, Localize("Add command"), FONTSIZE, TEXTALIGN_ML);
			static CButtonContainer s_FinalAddButton;
			if(Ui()->DoButton_FontIcon(&s_FinalAddButton, "+", 0, &MiddleButton, BUTTONFLAG_LEFT))
			{
				m_vBehaviorElements.emplace_back(GameClient());
				Changed = true;
				SetUnsavedChanges(true);
			}
		}
		s_BindToggleScrollRegion.End();
	}
	return Changed;
}

bool CMenusIngameTouchControls::RenderVisibilitySettingBlock(CUIRect Block)
{
	// Visibilities time. This is button's visibility, not virtual.
	bool Changed = false;
	CUIRect EditBox, LeftButton, MiddleButton, RightButton;

	// Block.HSplitTop(ROWGAP, nullptr, &Block);
	static CScrollRegion s_VisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VisibilityScrollRegion.Begin(&Block, &ScrollOffset, &ScrollParam);
	Block.y += ScrollOffset.y;

	static std::vector<CButtonContainer> s_avVisibilitySelector[(int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES];
	if(s_avVisibilitySelector[0].empty())
		std::for_each_n(s_avVisibilitySelector, (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES, [](auto &Element) {
			Element.resize(3);
		});
	const char *apVisibilities[] = {Localize("Ingame", "Touch button visibilities"), Localize("Zoom Allowed", "Touch button visibilities"), Localize("Vote Active", "Touch button visibilities"), Localize("Dummy Allowed", "Touch button visibilities"), Localize("Dummy Connected", "Touch button visibilities"), Localize("Rcon Authed", "Touch button visibilities"), Localize("Demo Player", "Touch button visibilities"), Localize("Extra Menu", "Touch button visibilities")};
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		Block.HSplitTop(ROWSIZE, &EditBox, &Block);
		Block.HSplitTop(ROWGAP, nullptr, &Block);
		if(s_VisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.VSplitMid(&LeftButton, &MiddleButton);
			MiddleButton.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &MiddleButton);
			if(Current < (unsigned)CTouchControls::EButtonVisibility::EXTRA_MENU_1)
				Ui()->DoLabel(&LeftButton, apVisibilities[Current], FONTSIZE, TEXTALIGN_ML);
			else
			{
				unsigned ExtraMenuNumber = Current - (unsigned)CTouchControls::EButtonVisibility::EXTRA_MENU_1 + 1;
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%s %d", apVisibilities[(int)CTouchControls::EButtonVisibility::EXTRA_MENU_1], ExtraMenuNumber);
				Ui()->DoLabel(&LeftButton, aBuf, FONTSIZE, TEXTALIGN_ML);
			}
			MiddleButton.VSplitLeft(MiddleButton.w / 3.0f, &LeftButton, &MiddleButton);
			MiddleButton.VSplitMid(&MiddleButton, &RightButton);
			if(GameClient()->m_Menus.DoButton_Menu(&s_avVisibilitySelector[Current][(int)EVisibilityType::INCLUDE], Localize("Included", "Touch button visibility preview"), m_aCachedVisibilities[Current] == (int)EVisibilityType::INCLUDE, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
			{
				m_aCachedVisibilities[Current] = (int)EVisibilityType::INCLUDE;
				SetUnsavedChanges(true);
				Changed = true;
			}
			if(GameClient()->m_Menus.DoButton_Menu(&s_avVisibilitySelector[Current][(int)EVisibilityType::EXCLUDE], Localize("Excluded", "Touch button visibility preview"), m_aCachedVisibilities[Current] == (int)EVisibilityType::EXCLUDE, &MiddleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
			{
				m_aCachedVisibilities[Current] = (int)EVisibilityType::EXCLUDE;
				SetUnsavedChanges(true);
				Changed = true;
			}
			if(GameClient()->m_Menus.DoButton_Menu(&s_avVisibilitySelector[Current][(int)EVisibilityType::IGNORE], Localize("Ignore", "Touch button visibility preview"), m_aCachedVisibilities[Current] == (int)EVisibilityType::IGNORE, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
			{
				m_aCachedVisibilities[Current] = (int)EVisibilityType::IGNORE;
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
	}
	s_VisibilityScrollRegion.End();
	return Changed;
}

void CMenusIngameTouchControls::RenderTouchButtonEditorWhileNothingSelected(CUIRect MainView)
{
	CUIRect LeftButton, MiddleButton, RightButton, EditBox, LabelRect, CommandRect, X, Y, W, H;
	MainView.h = 600.0f - 40.0f - MainView.y;
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(MAINMARGIN, &MainView);
	MainView.HSplitTop(ROWSIZE, &RightButton, &MainView);
	Ui()->DoLabel(&RightButton, Localize("Long press on a touch button to select it."), 15.0f, TEXTALIGN_MC);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	EditBox.VSplitLeft((EditBox.w - SUBMARGIN) / 2.0f, &LeftButton, &EditBox);
	static CButtonContainer s_NewButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_NewButton, Localize("New button"), 0, &LeftButton))
		NewButton();
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &MiddleButton);
	static CButtonContainer s_SelecteButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_SelecteButton, Localize("Select button by touch"), 0, &MiddleButton))
		GameClient()->m_Menus.SetActive(false);

	MainView.HSplitBottom(ROWSIZE, &MainView, &EditBox);
	MainView.HSplitBottom(ROWGAP, &MainView, nullptr);
	EditBox.VSplitLeft(ROWSIZE, &LeftButton, &EditBox);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&LeftButton, FontIcons::FONT_ICON_MAGNIFYING_GLASS, FONTSIZE, TEXTALIGN_ML);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
	EditBox.VSplitLeft(EditBox.w / 3.0f, &LeftButton, &EditBox);
	char aBufSearch[64];
	str_format(aBufSearch, sizeof(aBufSearch), "%s:", Localize("Search"));
	Ui()->DoLabel(&LeftButton, aBufSearch, FONTSIZE, TEXTALIGN_ML);

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
	EditBox.VSplitLeft(ButtonWidth, &RightButton, &EditBox);
	RightButton.VSplitLeft(PosWidth, &X, &RightButton);
	RightButton.VSplitLeft(SUBMARGIN, nullptr, &RightButton);
	RightButton.VSplitLeft(PosWidth, &Y, &RightButton);
	RightButton.VSplitLeft(SUBMARGIN, nullptr, &RightButton);
	RightButton.VSplitLeft(PosWidth, &W, &RightButton);
	RightButton.VSplitLeft(SUBMARGIN, nullptr, &RightButton);
	RightButton.VSplitLeft(PosWidth, &H, &RightButton);
	RightButton.VSplitLeft(SUBMARGIN, nullptr, &RightButton);
	const std::array<std::pair<CUIRect *, const char *>, (unsigned)ESortType::NUM_SORTS> aHeaderDatas = {
		{{&LabelRect, Localize("Label")},
			{&X, "X"},
			{&Y, "Y"},
			{&W, Localize("Width")},
			{&H, Localize("Height")}}};
	std::array<std::function<bool(CTouchControls::CTouchButton *, CTouchControls::CTouchButton *)>, (unsigned)ESortType::NUM_SORTS> aSortFunctions = {
		[](CTouchControls::CTouchButton *pLhs, CTouchControls::CTouchButton *pRhs) {
			if(pLhs->m_VisibilityCached == pRhs->m_VisibilityCached)
				return str_comp(pLhs->m_pBehavior->GetLabel().m_pLabel, pRhs->m_pBehavior->GetLabel().m_pLabel) < 0;
			return pLhs->m_VisibilityCached;
		},
		[](CTouchControls::CTouchButton *pLhs, CTouchControls::CTouchButton *pRhs) {
			if(pLhs->m_VisibilityCached == pRhs->m_VisibilityCached)
				return pLhs->m_UnitRect.m_X < pRhs->m_UnitRect.m_X;
			return pLhs->m_VisibilityCached;
		},
		[](CTouchControls::CTouchButton *pLhs, CTouchControls::CTouchButton *pRhs) {
			if(pLhs->m_VisibilityCached == pRhs->m_VisibilityCached)
				return pLhs->m_UnitRect.m_Y < pRhs->m_UnitRect.m_Y;
			return pLhs->m_VisibilityCached;
		},
		[](CTouchControls::CTouchButton *pLhs, CTouchControls::CTouchButton *pRhs) {
			if(pLhs->m_VisibilityCached == pRhs->m_VisibilityCached)
				return pLhs->m_UnitRect.m_W < pRhs->m_UnitRect.m_W;
			return pLhs->m_VisibilityCached;
		},
		[](CTouchControls::CTouchButton *pLhs, CTouchControls::CTouchButton *pRhs) {
			if(pLhs->m_VisibilityCached == pRhs->m_VisibilityCached)
				return pLhs->m_UnitRect.m_H < pRhs->m_UnitRect.m_H;
			return pLhs->m_VisibilityCached;
		}};
	for(unsigned HeaderIndex = (unsigned)ESortType::LABEL; HeaderIndex < (unsigned)ESortType::NUM_SORTS; HeaderIndex++)
	{
		if(GameClient()->m_Menus.DoButton_GridHeader(&m_aSortHeaderIds[HeaderIndex], "",
			   (unsigned)m_SortType == HeaderIndex, aHeaderDatas[HeaderIndex].first))
		{
			if(m_SortType != (ESortType)HeaderIndex)
			{
				m_NeedSort = true;
				m_SortType = (ESortType)HeaderIndex;
			}
		}
		Ui()->DoLabel(aHeaderDatas[HeaderIndex].first, aHeaderDatas[HeaderIndex].second, FONTSIZE, TEXTALIGN_ML);
	}
	// Can't sort buttons basing on command, that's meaning less and too slow.
	Ui()->DoLabel(&CommandRect, Localize("Command"), FONTSIZE, TEXTALIGN_ML);

	if(m_NeedUpdatePreview)
	{
		m_NeedUpdatePreview = false;
		m_vpButtons = GameClient()->m_TouchControls.GetButtonsEditor();
		m_NeedSort = true;
		m_NeedFilter = true;
	}

	if(m_NeedFilter || m_NeedSort)
	{
		// Sorting can be done directly.
		if(m_NeedSort)
		{
			m_NeedSort = false;
			std::sort(m_vpButtons.begin(), m_vpButtons.end(), aSortFunctions[(unsigned)m_SortType]);
		}

		m_vpMutableButtons = m_vpButtons;

		// Filtering should be done separately.
		if(m_NeedFilter && !m_FilterInput.IsEmpty())
		{
			std::string FilterString = m_FilterInput.GetString();
			// Get the filter done. Both label and command will be considered.
			{
				const auto DeleteIt = std::remove_if(m_vpMutableButtons.begin(), m_vpMutableButtons.end(), [&](CTouchControls::CTouchButton *pButton) {
					std::string Label = pButton->m_pBehavior->GetLabel().m_pLabel;
					if(std::search(Label.begin(), Label.end(), FilterString.begin(), FilterString.end(), [](char &Char1, char &Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Label.end())
						return false;
					std::string Command = DetermineTouchButtonCommandLabel(pButton);
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
				m_vpMutableButtons.erase(DeleteIt, m_vpMutableButtons.end());
			}
		}
		m_NeedFilter = false;
	}

	if(!m_vpMutableButtons.empty())
	{
		s_PreviewListBox.SetActive(true);
		s_PreviewListBox.DoStart(ROWSIZE, m_vpMutableButtons.size(), 1, 4, -1, &MainView, true, IGraphics::CORNER_B);
		for(unsigned ButtonIndex = 0; ButtonIndex < m_vpMutableButtons.size(); ButtonIndex++)
		{
			CTouchControls::CTouchButton *pButton = m_vpMutableButtons[ButtonIndex];
			const CListboxItem ListItem = s_PreviewListBox.DoNextItem(&m_vpMutableButtons[ButtonIndex], m_SelectedPreviewButtonIndex == ButtonIndex);
			if(ListItem.m_Visible)
			{
				EditBox = ListItem.m_Rect;
				EditBox.VSplitLeft(ROWSIZE, &LeftButton, &EditBox);
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
				Ui()->DoLabel(&LeftButton, m_vpMutableButtons[ButtonIndex]->m_VisibilityCached ? FontIcons::FONT_ICON_EYE : FontIcons::FONT_ICON_EYE_SLASH, FONTSIZE, TEXTALIGN_ML);
				TextRender()->SetRenderFlags(0);
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				EditBox.VSplitLeft(LabelRect.w, &LeftButton, &EditBox);
				const char *pLabel = pButton->m_pBehavior->GetLabel().m_pLabel;
				const auto LabelType = pButton->m_pBehavior->GetLabel().m_Type;
				if(LabelType == CTouchControls::CButtonLabel::EType::PLAIN)
				{
					SLabelProperties Props;
					Props.m_MaxWidth = LeftButton.w;
					Props.m_EnableWidthCheck = false;
					Props.m_EllipsisAtEnd = true;
					Ui()->DoLabel(&LeftButton, pLabel, FONTSIZE, TEXTALIGN_ML, Props);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::LOCALIZED)
				{
					pLabel = Localize(pLabel);
					SLabelProperties Props;
					Props.m_MaxWidth = LeftButton.w;
					Props.m_EnableWidthCheck = false;
					Props.m_EllipsisAtEnd = true;
					Ui()->DoLabel(&LeftButton, pLabel, FONTSIZE, TEXTALIGN_ML, Props);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::ICON)
				{
					SLabelProperties Props;
					Props.m_MaxWidth = LeftButton.w;
					Props.m_EnableWidthCheck = false;
					Props.m_EllipsisAtEnd = true;
					TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
					Ui()->DoLabel(&LeftButton, pLabel, FONTSIZE, TEXTALIGN_ML, Props);
					TextRender()->SetRenderFlags(0);
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(CommandRect.w, &LeftButton, &EditBox);
				std::string Command = DetermineTouchButtonCommandLabel(pButton);
				SLabelProperties Props;
				Props.m_MaxWidth = LeftButton.w;
				Props.m_EnableWidthCheck = false;
				Props.m_EllipsisAtEnd = true;
				Ui()->DoLabel(&LeftButton, Command.c_str(), FONTSIZE, TEXTALIGN_ML, Props);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(X.w, &LeftButton, &EditBox);
				Ui()->DoLabel(&LeftButton, std::to_string(pButton->m_UnitRect.m_X).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(Y.w, &LeftButton, &EditBox);
				Ui()->DoLabel(&LeftButton, std::to_string(pButton->m_UnitRect.m_Y).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(W.w, &LeftButton, &EditBox);
				Ui()->DoLabel(&LeftButton, std::to_string(pButton->m_UnitRect.m_W).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(H.w, &LeftButton, &EditBox);
				Ui()->DoLabel(&LeftButton, std::to_string(pButton->m_UnitRect.m_H).c_str(), FONTSIZE, TEXTALIGN_ML);
			}
		}

		m_SelectedPreviewButtonIndex = s_PreviewListBox.DoEnd();
		if(s_PreviewListBox.WasItemActivated())
		{
			GameClient()->m_TouchControls.SetSelectedButton(m_vpMutableButtons[m_SelectedPreviewButtonIndex]);
			CacheAllSettingsFromTarget(m_vpMutableButtons[m_SelectedPreviewButtonIndex]);
			SetUnsavedChanges(false);
			UpdateSampleButton();
		}
	}
	else
	{
		// Copied from menus_browser.cpp
		MainView.HMargin((MainView.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &LeftButton);
		LeftButton.HSplitTop(16.0f, &LeftButton, &MiddleButton);
		MiddleButton.HSplitTop(8.0f, nullptr, &MiddleButton);
		MiddleButton.VMargin((MiddleButton.w - 200.0f) / 2.0f, &MiddleButton);
		Ui()->DoLabel(&LeftButton, Localize("No buttons match your filter criteria"), 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ResetButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &MiddleButton))
		{
			m_FilterInput.Clear();
		}
	}
}

void CMenusIngameTouchControls::RenderSelectingTab(CUIRect SelectingTab)
{
	CUIRect LeftButton;
	SelectingTab.VSplitLeft(SelectingTab.w / 4.0f, &LeftButton, &SelectingTab);
	static CButtonContainer s_FileTab;
	if(GameClient()->m_Menus.DoButton_MenuTab(&s_FileTab, Localize("File"), m_CurrentMenu == EMenuType::MENU_FILE, &LeftButton, IGraphics::CORNER_TL))
		m_CurrentMenu = EMenuType::MENU_FILE;
	SelectingTab.VSplitLeft(SelectingTab.w / 3.0f, &LeftButton, &SelectingTab);
	static CButtonContainer s_ButtonTab;
	if(GameClient()->m_Menus.DoButton_MenuTab(&s_ButtonTab, Localize("Buttons"), m_CurrentMenu == EMenuType::MENU_BUTTONS, &LeftButton, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_BUTTONS;
	SelectingTab.VSplitLeft(SelectingTab.w / 2.0f, &LeftButton, &SelectingTab);
	static CButtonContainer s_SettingsMenuTab;
	if(GameClient()->m_Menus.DoButton_MenuTab(&s_SettingsMenuTab, Localize("Settings"), m_CurrentMenu == EMenuType::MENU_SETTINGS, &LeftButton, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_SETTINGS;
	SelectingTab.VSplitLeft(SelectingTab.w / 1.0f, &LeftButton, &SelectingTab);
	static CButtonContainer s_PreviewTab;
	if(GameClient()->m_Menus.DoButton_MenuTab(&s_PreviewTab, Localize("Preview"), m_CurrentMenu == EMenuType::MENU_PREVIEW, &LeftButton, IGraphics::CORNER_TR))
		m_CurrentMenu = EMenuType::MENU_PREVIEW;
}

void CMenusIngameTouchControls::RenderConfigSettings(CUIRect MainView)
{
	CUIRect EditBox, Row, Label, Button;
	MainView.h = 2 * MAINMARGIN + 4 * ROWSIZE + 3 * ROWGAP;
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	static CButtonContainer s_ActiveColorPicker;
	ColorHSLA ColorTest = GameClient()->m_Menus.DoLine_ColorPicker(&s_ActiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, Localize("Active color"), &m_ColorActive, CTouchControls::DefaultBackgroundColorActive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorActive(color_cast<ColorRGBA>(ColorHSLA(m_ColorActive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorActive())
		GameClient()->m_TouchControls.SetEditingChanges(true);

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	static CButtonContainer s_InactiveColorPicker;
	ColorTest = GameClient()->m_Menus.DoLine_ColorPicker(&s_InactiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, Localize("Inactive color"), &m_ColorInactive, CTouchControls::DefaultBackgroundColorInactive(), false, nullptr, true);
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

void CMenusIngameTouchControls::RenderPreviewSettings(CUIRect MainView)
{
	CUIRect EditBox;
	MainView.h = 600.0f - 40.0f - MainView.y;
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	Ui()->DoLabel(&EditBox, Localize("Preview button visibility while the editor is active."), FONTSIZE, TEXTALIGN_MC);
	MainView.HSplitBottom(ROWSIZE, &MainView, &EditBox);
	MainView.HSplitBottom(ROWGAP, &MainView, nullptr);
	EditBox.VSplitLeft(MAINMARGIN, nullptr, &EditBox);
	static CButtonContainer s_PreviewAllCheckBox;
	bool Preview = GameClient()->m_TouchControls.PreviewAllButtons();
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_PreviewAllCheckBox, Localize("Show all buttons"), Preview ? 1 : 0, &EditBox))
	{
		GameClient()->m_TouchControls.SetPreviewAllButtons(!Preview);
	}
	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HMargin(ROWGAP, &MainView);
	static CScrollRegion s_VirtualVisibilityScrollRegion;
	CScrollRegionParams ScrollParam;
	ScrollParam.m_ScrollUnit = 90.0f;
	vec2 ScrollOffset(0.0f, 0.0f);
	s_VirtualVisibilityScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParam);
	MainView.y += ScrollOffset.y;
	std::array<bool, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> aVirtualVisibilities = GameClient()->m_TouchControls.VirtualVisibilities();
	const char *apVisibilities[] = {Localize("Ingame", "Touch button visibilities"), Localize("Zoom Allowed", "Touch button visibilities"), Localize("Vote Active", "Touch button visibilities"), Localize("Dummy Allowed", "Touch button visibilities"), Localize("Dummy Connected", "Touch button visibilities"), Localize("Rcon Authed", "Touch button visibilities"), Localize("Demo Player", "Touch button visibilities"), Localize("Extra Menu", "Touch button visibilities")};
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		MainView.HSplitTop(ROWSIZE + ROWGAP, &EditBox, &MainView);
		if(s_VirtualVisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(ROWGAP, nullptr, &EditBox);
			if(Current < (unsigned)CTouchControls::EButtonVisibility::EXTRA_MENU_1)
			{
				if(GameClient()->m_Menus.DoButton_CheckBox(&m_aVisibilityIds[Current], apVisibilities[Current], aVirtualVisibilities[Current] == 1, &EditBox))
					GameClient()->m_TouchControls.ReverseVirtualVisibilities(Current);
			}
			else
			{
				unsigned ExtraMenuNumber = Current - (unsigned)CTouchControls::EButtonVisibility::EXTRA_MENU_1 + 1;
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%s %d", apVisibilities[(int)CTouchControls::EButtonVisibility::EXTRA_MENU_1], ExtraMenuNumber);
				if(GameClient()->m_Menus.DoButton_CheckBox(&m_aVisibilityIds[Current], aBuf, aVirtualVisibilities[Current] == 1, &EditBox))
					GameClient()->m_TouchControls.ReverseVirtualVisibilities(Current);
			}
		}
	}
	s_VirtualVisibilityScrollRegion.End();
}

void CMenusIngameTouchControls::RenderTouchControlsEditor(CUIRect MainView)
{
	CUIRect Label, Button, Row;
	MainView.h = 2 * MAINMARGIN + 4 * ROWSIZE + 3 * ROWGAP;
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(MAINMARGIN, &MainView);

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	Row.VSplitLeft(Row.h, nullptr, &Row);
	Row.VSplitRight(Row.h, &Row, &Button);
	Row.VMargin(5.0f, &Label);
	Ui()->DoLabel(&Label, Localize("Edit touch controls"), 20.0f, TEXTALIGN_MC);

	static CButtonContainer s_OpenHelpButton;
	if(Ui()->DoButton_FontIcon(&s_OpenHelpButton, FontIcons::FONT_ICON_QUESTION, 0, &Button, BUTTONFLAG_LEFT))
	{
		Client()->ViewLink(Localize("https://wiki.ddnet.org/wiki/Touch_controls"));
	}

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(ROWGAP, nullptr, &MainView);

	Row.VSplitLeft((Row.w - SUBMARGIN) / 2.0f, &Button, &Row);
	static CButtonContainer s_SaveConfigurationButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_SaveConfigurationButton, Localize("Save changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
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
	if(GameClient()->m_Menus.DoButton_Menu(&s_DiscardChangesButton, Localize("Discard changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
	{
		GameClient()->m_Menus.PopupConfirm(Localize("Discard changes"),
			Localize("Are you sure that you want to discard the current changes to the touch controls?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmDiscardTouchControlsChanges);
	}

	Row.VSplitLeft(SUBMARGIN, nullptr, &Button);
	static CButtonContainer s_ResetButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_ResetButton, Localize("Reset to defaults"), 0, &Button))
	{
		GameClient()->m_Menus.PopupConfirm(Localize("Reset to defaults"),
			Localize("Are you sure that you want to reset the touch controls to default?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmResetTouchControls);
	}

	MainView.HSplitTop(ROWSIZE, &Row, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);

	Row.VSplitLeft((Row.w - SUBMARGIN) / 2.0f, &Button, &Row);
	static CButtonContainer s_ClipboardImportButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_ClipboardImportButton, Localize("Import from clipboard"), 0, &Button))
	{
		GameClient()->m_Menus.PopupConfirm(Localize("Import from clipboard"),
			Localize("Are you sure that you want to import the touch controls from the clipboard? This will overwrite your current touch controls."),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmImportTouchControlsClipboard);
	}

	Row.VSplitLeft(SUBMARGIN, nullptr, &Button);
	static CButtonContainer s_ClipboardExportButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_ClipboardExportButton, Localize("Export to clipboard"), 0, &Button))
	{
		GameClient()->m_TouchControls.SaveConfigurationToClipboard();
	}
}

// Check if CTouchControls need CMenus to open any popups.
void CMenusIngameTouchControls::DoPopupType(CTouchControls::CPopupParam PopupParam)
{
	m_pOldSelectedButton = PopupParam.m_pOldSelectedButton;
	m_pNewSelectedButton = PopupParam.m_pNewSelectedButton;
	m_CloseMenu = !PopupParam.m_KeepMenuOpen;
	switch(PopupParam.m_PopupType)
	{
	case CTouchControls::EPopupType::BUTTON_CHANGED: ChangeSelectedButtonWhileHavingUnsavedChanges(); break;
	case CTouchControls::EPopupType::NO_SPACE: NoSpaceForOverlappingButton(); break;
	case CTouchControls::EPopupType::BUTTON_INVISIBLE: SelectedButtonNotVisible(); break;
	// The NUM_POPUPS will not call the function.
	default: dbg_assert(false, "Unknown popup type = %d.", (int)PopupParam.m_PopupType);
	}
}

void CMenusIngameTouchControls::ChangeSelectedButtonWhileHavingUnsavedChanges()
{
	// Both old and new button pointer can be nullptr.
	// Saving settings to the old selected button(nullptr = create), then switch to new selected button(new = haven't created).
	GameClient()->m_Menus.PopupConfirm(Localize("Unsaved changes"), Localize("Save all changes before switching selected button?"), Localize("Save"), Localize("Discard"), &CMenus::PopupConfirmChangeSelectedButton, CMenus::POPUP_NONE, &CMenus::PopupCancelChangeSelectedButton);
}

void CMenusIngameTouchControls::NoSpaceForOverlappingButton()
{
	GameClient()->m_Menus.PopupMessage(Localize("No space for button"), Localize("There is not enough space available for the button. Check its visibilities and size."), Localize("Ok"));
}

void CMenusIngameTouchControls::SelectedButtonNotVisible()
{
	// Cancel shouldn't do anything but open ingame menu, the menu is already opened now.
	m_CloseMenu = false;
	GameClient()->m_Menus.PopupConfirm(Localize("Selected button not visible"), Localize("The selected button is not visible. Do you want to deselect it or edit its visibility?"), Localize("Deselect"), Localize("Edit"), &CMenus::PopupConfirmSelectedNotVisible);
}

bool CMenusIngameTouchControls::UnsavedChanges()
{
	return GameClient()->m_TouchControls.UnsavedChanges();
}

void CMenusIngameTouchControls::SetUnsavedChanges(bool UnsavedChanges)
{
	GameClient()->m_TouchControls.SetUnsavedChanges(UnsavedChanges);
}

// Check if cached settings are legal.
bool CMenusIngameTouchControls::CheckCachedSettings()
{
	std::vector<const char *> vpErrors;
	char aBuf[256];
	int X = m_InputX.GetInteger();
	int Y = m_InputY.GetInteger();
	int W = m_InputW.GetInteger();
	int H = m_InputH.GetInteger();
	// Illegal size settings.
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM || H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Width and height are required to be within the range from %d to %d."), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
		vpErrors.emplace_back(aBuf);
	}
	if(X < 0 || Y < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
	{
		vpErrors.emplace_back(Localize("Button position is outside of the screen."));
	}
	if(GameClient()->m_TouchControls.IsRectOverlapping({X, Y, W, H}))
	{
		vpErrors.emplace_back(Localize("The selected button is overlapping with other buttons."));
	}
	if(!vpErrors.empty())
	{
		char aErrorMessage[1024] = "";
		for(const char *pError : vpErrors)
		{
			if(aErrorMessage[0] != '\0')
				str_append(aErrorMessage, "\n");
			str_append(aErrorMessage, pError);
		}
		GameClient()->m_Menus.PopupMessage(Localize("Wrong button settings"), aErrorMessage, Localize("Ok"));
		return false;
	}
	else
	{
		return true;
	}
}

// All default settings are here.
void CMenusIngameTouchControls::ResetCachedSettings()
{
	// Reset all cached values.
	m_EditBehaviorType = (int)EBehaviorType::BIND;
	m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
	m_CachedExtraMenuNumber = 0;
	m_vBehaviorElements.clear();
	m_vBehaviorElements.emplace_back(GameClient());
	m_vBehaviorElements.emplace_back(GameClient());
	m_aCachedVisibilities.fill((int)EVisibilityType::IGNORE); // 2 means don't have the visibility, true:1,false:0
	m_aCachedVisibilities[(int)CTouchControls::EButtonVisibility::DEMO_PLAYER] = (int)EVisibilityType::EXCLUDE;
	// These things can't be empty. std::stoi can't cast empty string.
	SetPosInputs({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM});
	m_CachedShape = CTouchControls::EButtonShape::RECT;
}

// This is called when the Touch button editor is rendered as well when selectedbutton changes. Used for updating all cached settings.
void CMenusIngameTouchControls::CacheAllSettingsFromTarget(CTouchControls::CTouchButton *pTargetButton)
{
	ResetCachedSettings();
	if(pTargetButton == nullptr)
	{
		return; // Nothing to cache.
	}
	// These values can't be null. The constructor has been updated. Default:{0,0,CTouchControls::BUTTON_SIZE_MINIMUM,CTouchControls::BUTTON_SIZE_MINIMUM}, shape = rect.
	SetPosInputs(pTargetButton->m_UnitRect);
	m_CachedShape = pTargetButton->m_Shape;
	for(const auto &Visibility : pTargetButton->m_vVisibilities)
	{
		dbg_assert((int)Visibility.m_Type < (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES, "Target button has out of bound visibility type: %d", (int)Visibility.m_Type);
		m_aCachedVisibilities[(int)Visibility.m_Type] = Visibility.m_Parity ? (int)EVisibilityType::INCLUDE : (int)EVisibilityType::EXCLUDE;
	}

	// These are behavior values.
	if(pTargetButton->m_pBehavior != nullptr)
	{
		const char *pBehaviorType = pTargetButton->m_pBehavior->GetBehaviorType();
		if(str_comp(pBehaviorType, CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND;
			auto *pTargetBehavior = static_cast<CTouchControls::CBindTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
			// m_LabelType must not be null. Default value is PLAIN.
			m_vBehaviorElements[0].m_CachedCommands = {pTargetBehavior->GetLabel().m_pLabel, pTargetBehavior->GetLabel().m_Type, pTargetBehavior->GetCommand()};
			m_vBehaviorElements[0].UpdateInputs();
		}
		else if(str_comp(pBehaviorType, CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND_TOGGLE;
			auto *pTargetBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
			auto TargetCommands = pTargetBehavior->GetCommand();
			// Can't use resize here :(
			while(m_vBehaviorElements.size() > maximum<size_t>(TargetCommands.size(), 2))
				m_vBehaviorElements.pop_back();
			while(m_vBehaviorElements.size() < maximum<size_t>(TargetCommands.size(), 2))
				m_vBehaviorElements.emplace_back(GameClient());
			for(unsigned CommandIndex = 0; CommandIndex < TargetCommands.size(); CommandIndex++)
			{
				m_vBehaviorElements[CommandIndex].m_CachedCommands = TargetCommands[CommandIndex];
				m_vBehaviorElements[CommandIndex].UpdateInputs();
			}
		}
		else if(str_comp(pBehaviorType, CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			m_EditBehaviorType = (int)EBehaviorType::PREDEFINED;
			auto *pTargetBehavior = static_cast<CTouchControls::CPredefinedTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
			const char *pPredefinedType = pTargetBehavior->GetPredefinedType();
			if(pPredefinedType == nullptr)
				m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
			else
				m_PredefinedBehaviorType = CalculatePredefinedType(pPredefinedType);
			dbg_assert(m_PredefinedBehaviorType != (int)EPredefinedType::NUM_PREDEFINEDS, "Detected out of bound m_PredefinedBehaviorType. pPredefinedType = %s", pPredefinedType);

			if(m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU)
			{
				auto *pExtraMenuBehavior = static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
				m_CachedExtraMenuNumber = pExtraMenuBehavior->GetNumber();
			}
		}
		else // Empty
			dbg_assert(false, "Detected out of bound value in m_EditBehaviorType");
	}
}

// Will override everything in the button. If nullptr is passed, a new button will be created.
void CMenusIngameTouchControls::SaveCachedSettingsToTarget(CTouchControls::CTouchButton *pTargetButton)
{
	// Save the cached config to the target button. If no button to save, create a new one, then select it.
	if(pTargetButton == nullptr)
	{
		pTargetButton = GameClient()->m_TouchControls.NewButton();
		// Keep the new button's visibility equal to the last selected one.
		for(unsigned Iterator = (unsigned)CTouchControls::EButtonVisibility::INGAME; Iterator < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Iterator)
		{
			if(m_aCachedVisibilities[Iterator] != (int)EVisibilityType::IGNORE)
				pTargetButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Iterator, m_aCachedVisibilities[Iterator] == (int)EVisibilityType::INCLUDE);
		}
	}

	pTargetButton->m_UnitRect.m_W = std::clamp(m_InputW.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	pTargetButton->m_UnitRect.m_H = std::clamp(m_InputH.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	pTargetButton->m_UnitRect.m_X = std::clamp(m_InputX.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - pTargetButton->m_UnitRect.m_W);
	pTargetButton->m_UnitRect.m_Y = std::clamp(m_InputY.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - pTargetButton->m_UnitRect.m_H);
	pTargetButton->m_vVisibilities.clear();
	for(unsigned Iterator = (unsigned)CTouchControls::EButtonVisibility::INGAME; Iterator < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Iterator)
	{
		if(m_aCachedVisibilities[Iterator] != (int)EVisibilityType::IGNORE)
			pTargetButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Iterator, m_aCachedVisibilities[Iterator] == (int)EVisibilityType::INCLUDE);
	}
	pTargetButton->m_Shape = m_CachedShape;
	pTargetButton->UpdateScreenFromUnitRect();

	// Make a new behavior class instead of modify the original one.
	if(m_EditBehaviorType == (int)EBehaviorType::BIND)
	{
		pTargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindTouchButtonBehavior>(
			m_vBehaviorElements[0].m_CachedCommands.m_Label.c_str(),
			m_vBehaviorElements[0].m_CachedCommands.m_LabelType,
			m_vBehaviorElements[0].m_CachedCommands.m_Command.c_str());
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE)
	{
		std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vMovingBehavior;
		vMovingBehavior.reserve(m_vBehaviorElements.size());
		for(const auto &Element : m_vBehaviorElements)
			vMovingBehavior.emplace_back(Element.m_CachedCommands);
		pTargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindToggleTouchButtonBehavior>(std::move(vMovingBehavior));
	}
	else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
	{
		if(m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU)
			pTargetButton->m_pBehavior = std::make_unique<CTouchControls::CExtraMenuTouchButtonBehavior>(CTouchControls::CExtraMenuTouchButtonBehavior(m_CachedExtraMenuNumber));
		else
			pTargetButton->m_pBehavior = GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[m_PredefinedBehaviorType].m_Factory();
	}
	else
	{
		dbg_assert(false, "Unknown m_EditBehaviorType = %d", m_EditBehaviorType);
	}
	pTargetButton->UpdatePointers();
}

// Used for setting the value of four position input box to the unitrect.
void CMenusIngameTouchControls::SetPosInputs(CTouchControls::CUnitRect MyRect)
{
	m_InputX.SetInteger(MyRect.m_X);
	m_InputY.SetInteger(MyRect.m_Y);
	m_InputW.SetInteger(MyRect.m_W);
	m_InputH.SetInteger(MyRect.m_H);
}

// Used to make sure the input box is numbers only, also clamp the value.
void CMenusIngameTouchControls::InputPosFunction(CLineInputNumber *pInput)
{
	int InputValue = pInput->GetInteger();
	// Deal with the "-1" FindPositionXY give.
	InputValue = std::clamp(InputValue, 0, CTouchControls::BUTTON_SIZE_SCALE);
	pInput->SetInteger(InputValue);
	SetUnsavedChanges(true);
}

// Update m_pSampleButton in CTouchControls. The Samplebutton is used for showing on screen.
void CMenusIngameTouchControls::UpdateSampleButton()
{
	GameClient()->m_TouchControls.RemakeSampleButton();
	SaveCachedSettingsToTarget(GameClient()->m_TouchControls.SampleButton());
	GameClient()->m_TouchControls.SetShownRect(GameClient()->m_TouchControls.SampleButton()->m_UnitRect);
}

// Not inline so there's no more includes in menus.h. A shortcut to the function in CTouchControls.
void CMenusIngameTouchControls::ResetButtonPointers()
{
	GameClient()->m_TouchControls.ResetButtonPointers();
}

// New button doesn't create a real button, instead it reset the Samplebutton to cache every setting. When saving a the Samplebutton then a real button will be created.
void CMenusIngameTouchControls::NewButton()
{
	CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
	ResetButtonPointers();
	ResetCachedSettings();
	SetPosInputs(FreeRect);
	UpdateSampleButton();
	SetUnsavedChanges(true);
}

// Used for updating cached settings or something else only when opening the editor, to reduce lag. Issues come from CTouchControls.
void CMenusIngameTouchControls::ResolveIssues()
{
	if(GameClient()->m_TouchControls.AnyIssueNotResolved())
	{
		std::array<CTouchControls::CIssueParam, (unsigned)CTouchControls::EIssueType::NUM_ISSUES> aIssues = GameClient()->m_TouchControls.Issues();
		for(unsigned Current = 0; Current < (unsigned)CTouchControls::EIssueType::NUM_ISSUES; Current++)
		{
			if(aIssues[Current].m_Resolved == true)
				continue;
			switch(Current)
			{
			case(int)CTouchControls::EIssueType::CACHE_SETTINGS: CacheAllSettingsFromTarget(aIssues[Current].m_pTargetButton); break;
			case(int)CTouchControls::EIssueType::SAVE_SETTINGS: SaveCachedSettingsToTarget(aIssues[Current].m_pTargetButton); break;
			case(int)CTouchControls::EIssueType::CACHE_POSITION: SetPosInputs(aIssues[Current].m_pTargetButton->m_UnitRect); break;
			default: dbg_assert(false, "Unknown Issue type: %d", (int)Current);
			}
		}
	}
}

// Turn predefined behavior strings like "joystick-hook" into integers according to the enum.
int CMenusIngameTouchControls::CalculatePredefinedType(const char *pType)
{
	int IntegerType;
	for(IntegerType = (int)EPredefinedType::EXTRA_MENU;
		str_comp(pType, GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[IntegerType].m_pId) != 0 && IntegerType < (int)EPredefinedType::NUM_PREDEFINEDS;
		IntegerType++)
		;
	return IntegerType;
}

std::string CMenusIngameTouchControls::DetermineTouchButtonCommandLabel(CTouchControls::CTouchButton *pButton)
{
	const char *pBehaviorType = pButton->m_pBehavior->GetBehaviorType();
	if(str_comp(pBehaviorType, CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		return static_cast<CTouchControls::CBindTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetCommand();
	}
	else if(str_comp(pBehaviorType, CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		const auto *pBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pButton->m_pBehavior.get());
		return pBehavior->GetCommand()[pBehavior->GetActiveCommandIndex()].m_Command;
	}
	else if(str_comp(pBehaviorType, CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		auto *pTargetBehavior = static_cast<CTouchControls::CPredefinedTouchButtonBehavior *>(pButton->m_pBehavior.get());
		const char *pPredefinedType = pTargetBehavior->GetPredefinedType();
		const char *apPredefineds[] = {Localize("Extra Menu", "Predefined touch button behaviors"), Localize("Joystick Hook", "Predefined touch button behaviors"), Localize("Joystick Fire", "Predefined touch button behaviors"), Localize("Joystick Aim", "Predefined touch button behaviors"), Localize("Joystick Action"), Localize("Use Action"), Localize("Swap Action", "Predefined touch button behaviors"), Localize("Spectate", "Predefined touch button behaviors"), Localize("Emoticon", "Predefined touch button behaviors"), Localize("Ingame Menu", "Predefined touch button behaviors")};
		std::string Command = apPredefineds[CalculatePredefinedType(pPredefinedType)];
		if(pPredefinedType == CTouchControls::CExtraMenuTouchButtonBehavior::BEHAVIOR_ID)
		{
			Command += " " + std::to_string(static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetNumber());
		}
		return Command;
	}
	else
	{
		dbg_assert(false, "Detected unknown behavior type in CMenusIngameTouchControls::GetCommand()");
	}
}

// Used for making json chars like \n or \uf3ce visible.
std::string CMenusIngameTouchControls::CBehaviorElements::ParseLabel(const char *pLabel)
{
	std::string ParsedString;
	json_settings JsonSettings{};
	char aError[256];
	char Buf[1048];
	str_format(Buf, sizeof(Buf), "{\"Label\":\"%s\"}", pLabel);
	json_value *pJsonLabel = json_parse_ex(&JsonSettings, Buf, str_length(Buf), aError);
	if(pJsonLabel == nullptr)
	{
		ParsedString = pLabel;
		return ParsedString;
	}
	const json_value &Label = (*pJsonLabel)["Label"];
	ParsedString = Label.u.string.ptr;
	json_value_free(pJsonLabel);
	return ParsedString;
}

CMenusIngameTouchControls::CBehaviorElements::CBehaviorElements(CGameClient *pGameClient) noexcept :
	m_pGameClient(pGameClient)
{
	m_InputCommand = std::make_unique<CLineInputBuffered<1024>>();
	m_InputLabel = std::make_unique<CLineInputBuffered<1024>>();
	Reset();
}

CMenusIngameTouchControls::CBehaviorElements::CBehaviorElements(CBehaviorElements &&Other) noexcept :
	m_InputCommand(std::move(Other.m_InputCommand)), m_InputLabel(std::move(Other.m_InputLabel)), m_CachedCommands(std::move(Other.m_CachedCommands)), m_pGameClient(Other.m_pGameClient)
{
}

CMenusIngameTouchControls::CBehaviorElements::~CBehaviorElements()
{
	if(m_InputCommand != nullptr)
		m_InputCommand->Deactivate();
	if(m_InputLabel != nullptr)
		m_InputLabel->Deactivate();
	if(m_pGameClient != nullptr)
		m_pGameClient->Ui()->SetActiveItem(nullptr);
}

CMenusIngameTouchControls::CBehaviorElements &CMenusIngameTouchControls::CBehaviorElements::operator=(CBehaviorElements &&Other) noexcept
{
	if(this == &Other)
	{
		return *this;
	}
	m_InputCommand = std::move(Other.m_InputCommand);
	m_InputLabel = std::move(Other.m_InputLabel);
	m_CachedCommands = std::move(Other.m_CachedCommands);
	m_pGameClient = Other.m_pGameClient;
	Other.m_pGameClient = nullptr;
	return *this;
}

void CMenusIngameTouchControls::CBehaviorElements::UpdateInputs()
{
	m_InputLabel->Set(ParseLabel(m_CachedCommands.m_Label.c_str()).c_str());
	m_InputCommand->Set(m_CachedCommands.m_Command.c_str());
}

void CMenusIngameTouchControls::CBehaviorElements::Reset()
{
	m_CachedCommands = {};
	m_InputCommand->Clear();
	m_InputLabel->Clear();
}
