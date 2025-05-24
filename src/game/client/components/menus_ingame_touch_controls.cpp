#include "menus.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>

#include <cstring>
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

static const char *BEHAVIORS[] = {Localizable("Bind"), Localizable("Bind Toggle"), Localizable("Predefined")};
static const char *PREDEFINEDS[] = {Localizable("Extra Menu"), Localizable("Joystick Hook"), Localizable("Joystick Fire"), Localizable("Joystick Aim"), Localizable("Joystick Action"), Localizable("Use Action"), Localizable("Swap Action"), Localizable("Spectate"), Localizable("Emoticon"), Localizable("Ingame Menu")};
static const char *LABELTYPES[] = {Localizable("Plain"), Localizable("Localized"), Localizable("Icon")};
static const char *VISIBILITIES[] = {Localizable("Ingame"), Localizable("Zoom Allowed"), Localizable("Vote Active"), Localizable("Dummy Allowed"), Localizable("Dummy Connected"), Localizable("Rcon Authed"), Localizable("Demo Player"), Localizable("Extra Menu 1"), Localizable("Extra Menu 2"), Localizable("Extra Menu 3"), Localizable("Extra Menu 4"), Localizable("Extra Menu 5")};
static const char *SHAPES[] = {Localizable("Rectangle"), Localizable("Circle")};
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
	// Used to decide if need to update the Samplebutton.
	bool Changed = false;
	CUIRect Left, A, B, C, EditBox, Block;
	MainView.h = 2 * MAINMARGIN + 9 * ROWSIZE + 8 * ROWGAP;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);

	if(m_BindTogglePreviewExtension && m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE && m_EditElement == EElementType::BEHAVIOR)
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

	if(DoButton_MenuTab(m_aEditElementIds.data(), Localize("Layout"), m_EditElement == EElementType::LAYOUT, &C, IGraphics::CORNER_TL, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::LAYOUT;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[1], Localize("Visibility"), m_EditElement == EElementType::VISIBILITY, &A, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::VISIBILITY;
	}
	if(DoButton_MenuTab(&m_aEditElementIds[2], Localize("Behavior"), m_EditElement == EElementType::BEHAVIOR, &B, IGraphics::CORNER_TR, nullptr, nullptr, nullptr, nullptr, 5.0f))
	{
		m_EditElement = EElementType::BEHAVIOR;
	}

	// Edit blocks.
	Block.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	switch(m_EditElement)
	{
	case EElementType::LAYOUT: Changed = RenderLayoutSettingBlock(Block) || Changed; break;
	case EElementType::VISIBILITY: Changed = RenderVisibilitySettingBlock(Block) || Changed; break;
	case EElementType::BEHAVIOR: Changed = RenderBehaviorSettingBlock(Block) || Changed; break;
	default: dbg_assert(false, "Unknown m_EditElement = %d.", m_EditElement);
	}

	// Leave some free space for the bind toggle preview.
	if(m_BindTogglePreviewExtension && m_EditBehaviorType == (int)EBehaviorType::BIND_TOGGLE && m_EditElement == EElementType::BEHAVIOR)
	{
		if(Changed)
		{
			UpdateSampleButton();
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
	if(DoButton_Menu(&s_ConfirmButton, Localize("Save changes"), UnsavedChanges() ? 0 : 1, &A))
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
	EditBox.VSplitLeft(ButtonWidth, &A, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	if(UnsavedChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&A, Localize("Unsaved changes"), 14.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	static CButtonContainer s_CancelButton;
	if(DoButton_Menu(&s_CancelButton, Localize("Discard changes"), UnsavedChanges() ? 0 : 1, &B))
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
	if(DoButton_Menu(&s_AddNewButton, Localize("New button"), Checked ? 1 : 0, &A))
	{
		CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
		if(Checked)
		{
			PopupMessage(Localize("Already Created New Button"), Localize("A new button is already created, please save or delete it before creating a new one"), Localize("OK"));
		}
		else if(FreeRect.m_X == -1)
		{
			PopupMessage(Localize("No Space"), Localize("No enough space for another button."), Localize("OK"));
		}
		else if(UnsavedChanges())
		{
			PopupConfirm(Localize("Unsaved Changes"), Localize("Save all changes before creating another button?"), Localize("Save"), Localize("Cancel"), &CMenus::PopupConfirm_NewButton);
		}
		else
		{
			PopupCancel_NewButton();
		}
	}

	static CButtonContainer s_RemoveButton;
	if(DoButton_Menu(&s_RemoveButton, Localize("Delete"), 0, &B))
	{
		PopupConfirm(Localize("Delete Button"), Localize("Are you sure to delete this button? This can't be undone."), Localize("Delete"), Localize("Cancel"), &CMenus::PopupConfirm_DeleteButton);
	}

	// Create a new button with current cached settings. New button will be automatically moved to nearest empty space.
	Left.HSplitTop(ROWGAP, nullptr, &Left);
	Left.HSplitTop(ROWSIZE, &EditBox, &Left);
	EditBox.VSplitLeft(ButtonWidth2, &A, &EditBox);
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	static CButtonContainer s_CopyPasteButton;
	if(DoButton_Menu(&s_CopyPasteButton, Localize("Duplicate"), UnsavedChanges() || Checked ? 1 : 0, &A))
	{
		if(Checked)
		{
			PopupMessage(Localize("Already Created New Button"), Localize("A new button is already created, please save or delete it before creating a new one"), Localize("OK"));
		}
		else if(UnsavedChanges())
		{
			PopupMessage(Localize("Unsaved Changes"), Localize("Save changes before duplicate a button."), Localize("OK"));
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
					PopupMessage(Localize("No Space"), Localize("No enough space for another button."), Localize("OK"));
				}
				else
				{
					PopupMessage(Localize("Not Enough Space"), Localize("Space is not enough for another button with this size. The button has been resized."), Localize("OK"));
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
	if(DoButton_Menu(&s_DeselectButton, Localize("Deselect"), 0, &B))
	{
		m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_pNewSelectedButton = nullptr;
		if(UnsavedChanges())
		{
			PopupConfirm(Localize("Unsaved Changes"), Localize("You'll lose unsaved changes after deselecting."), Localize("Deselect"), Localize("Cancel"), &CMenus::PopupCancel_DeselectButton);
		}
		else
		{
			PopupCancel_DeselectButton();
		}
	}

	// This ensures m_pSampleButton being updated always.
	if(Changed)
	{
		UpdateSampleButton();
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
	auto DoRedLabel = [&](const char *pLabel, CUIRect &LabelBlock, const int &Size) {
		if(pLabel == nullptr)
			return;
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&LabelBlock, pLabel, Size, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	// There're strings without ":" below, so don't localize these with ":" together.
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s%s", Localize("X"), ":");
	if(X < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel(aBuf, PosX, FONTSIZE);
	else
		Ui()->DoLabel(&PosX, aBuf, FONTSIZE, TEXTALIGN_ML);

	memset(aBuf, 0, sizeof(aBuf));
	str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Y"), ":");
	if(Y < 0 || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
		DoRedLabel(aBuf, PosY, FONTSIZE);
	else
		Ui()->DoLabel(&PosY, aBuf, FONTSIZE, TEXTALIGN_ML);

	memset(aBuf, 0, sizeof(aBuf));
	str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Width"), ":");
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel(aBuf, PosW, FONTSIZE);
	else
		Ui()->DoLabel(&PosW, aBuf, FONTSIZE, TEXTALIGN_ML);

	memset(aBuf, 0, sizeof(aBuf));
	str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Height"), ":");
	if(H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
		DoRedLabel(aBuf, PosH, FONTSIZE);
	else
		Ui()->DoLabel(&PosH, aBuf, FONTSIZE, TEXTALIGN_ML);

	// Drop down menu for shapes
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&A, &B);
	memset(aBuf, 0, sizeof(aBuf));
	str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Shape"), ":");
	Ui()->DoLabel(&A, aBuf, FONTSIZE, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonShapeDropDownState;
	static CScrollRegion s_ButtonShapeDropDownScrollRegion;
	s_ButtonShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonShapeDropDownScrollRegion;
	const CTouchControls::EButtonShape NewButtonShape = (CTouchControls::EButtonShape)Ui()->DoDropDown(&B, (int)m_CachedShape, SHAPES, (int)CTouchControls::EButtonShape::NUM_SHAPES, s_ButtonShapeDropDownState);
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
	Block.HSplitTop(ROWSIZE, &EditBox, &Block);
	Block.HSplitTop(ROWGAP, nullptr, &Block);
	EditBox.VSplitMid(&A, &B);
	Ui()->DoLabel(&A, Localize("Behavior type:"), FONTSIZE, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonBehaviorDropDownState;
	static CScrollRegion s_ButtonBehaviorDropDownScrollRegion;
	s_ButtonBehaviorDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonBehaviorDropDownScrollRegion;
	const int NewButtonBehavior = Ui()->DoDropDown(&B, m_EditBehaviorType, BEHAVIORS, std::size(BEHAVIORS), s_ButtonBehaviorDropDownState);
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
		EditBox.VSplitMid(&A, &B);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Command"), ":");
			Ui()->DoLabel(&A, aBuf, FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(m_vBehaviorElements[0].m_InputCommand.get(), &B, 10.0f))
			{
				m_vBehaviorElements[0].UpdateCommand();
				SetUnsavedChanges(true);
				Changed = true;
			}
		}
		else if(m_EditBehaviorType == (int)EBehaviorType::PREDEFINED)
		{
			Ui()->DoLabel(&A, Localize("Type:"), FONTSIZE, TEXTALIGN_ML);
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
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Label"), ":");
			Ui()->DoLabel(&A, aBuf, FONTSIZE, TEXTALIGN_ML);
			if(Ui()->DoClearableEditBox(m_vBehaviorElements[0].m_InputLabel.get(), &B, 10.0f))
			{
				m_vBehaviorElements[0].UpdateLabel();
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
				if(m_CachedExtraMenuNumber > 0)
				{
					// Menu Number also counts from 1, but written as 0.
					m_CachedExtraMenuNumber--;
					SetUnsavedChanges(true);
					Changed = true;
				}
			}

			B.VSplitRight(ROWSIZE, &A, &B);
			Ui()->DoLabel(&A, std::to_string(m_CachedExtraMenuNumber + 1).c_str(), FONTSIZE, TEXTALIGN_MC);
			static CButtonContainer s_ExtraMenuIncreaseButton;
			if(DoButton_FontIcon(&s_ExtraMenuIncreaseButton, "+", 0, &B, BUTTONFLAG_LEFT))
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
		EditBox.VSplitMid(&A, &B);
		if(m_EditBehaviorType == (int)EBehaviorType::BIND)
		{
			Ui()->DoLabel(&A, Localize("Label type:"), FONTSIZE, TEXTALIGN_ML);
			int NewButtonLabelType = (int)m_vBehaviorElements[0].m_CachedCommands.m_LabelType;
			B.VSplitLeft(B.w / 3.0f, &A, &B);
			B.VSplitMid(&B, &C);
			if(DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[0], LABELTYPES[0], NewButtonLabelType == 0 ? 1 : 0, &A, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
				NewButtonLabelType = 0;
			if(DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[1], LABELTYPES[1], NewButtonLabelType == 1 ? 1 : 0, &B, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				NewButtonLabelType = 1;
			if(DoButton_Menu(&m_vBehaviorElements[0].m_aLabelTypeRadios[2], LABELTYPES[2], NewButtonLabelType == 2 ? 1 : 0, &C, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
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
		if(m_BindTogglePreviewExtension)
			Block.HSplitBottom(MAINMARGIN, &Block, nullptr);
		Block.HSplitBottom(ROWSIZE, &Block, &EditBox);
		Block.HSplitBottom(SUBMARGIN, &Block, nullptr);
		static CButtonContainer s_ExtendButton;
		if(DoButton_Menu(&s_ExtendButton, m_BindTogglePreviewExtension ? Localize("Fold list") : Localize("Unfold list"), 0, &EditBox))
		{
			m_BindTogglePreviewExtension = !m_BindTogglePreviewExtension;
		}
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
				EditBox.VSplitMid(&EditBox, &C);
				C.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &C);
				EditBox.VSplitLeft(ROWSIZE, &B, &EditBox);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &A);
				Ui()->DoLabel(&A, Localize("Add command"), FONTSIZE, TEXTALIGN_ML);
				if(DoButton_FontIcon(&m_vBehaviorElements[CommandIndex].m_BindToggleAddButtons, "+", 0, &B, BUTTONFLAG_LEFT))
				{
					m_vBehaviorElements.emplace(m_vBehaviorElements.begin() + CommandIndex, GameClient());
					m_vBehaviorElements[CommandIndex].UpdateInputs();
					Changed = true;
					SetUnsavedChanges(true);
				}
				C.VSplitLeft(ROWSIZE, &B, &C);
				C.VSplitLeft(SUBMARGIN, nullptr, &A);
				Ui()->DoLabel(&A, Localize("Delete command"), FONTSIZE, TEXTALIGN_ML);
				if(DoButton_FontIcon(&m_vBehaviorElements[CommandIndex].m_BindToggleDeleteButtons, "\xEF\x81\xA3", 0, &B, BUTTONFLAG_LEFT))
				{
					if(m_vBehaviorElements.size() > 2)
					{
						m_vBehaviorElements.erase(m_vBehaviorElements.begin() + CommandIndex);
					}
					else
					{
						m_vBehaviorElements[CommandIndex].Reset();
					}
					SetUnsavedChanges(true);
					Changed = true;
				}
			}
			if(CommandIndex >= m_vBehaviorElements.size())
				continue;
			Block.HSplitTop(ROWGAP, nullptr, &Block);
			Block.HSplitTop(ROWSIZE, &EditBox, &Block);
			if(s_BindToggleScrollRegion.AddRect(EditBox))
			{
				EditBox.VSplitMid(&A, &B);
				B.VSplitLeft(ScrollParam.m_ScrollbarWidth / 2.0f, nullptr, &B);
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Command"), ":");
				Ui()->DoLabel(&A, aBuf, FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(m_vBehaviorElements[CommandIndex].m_InputCommand.get(), &B, 10.0f))
				{
					m_vBehaviorElements[CommandIndex].UpdateCommand();
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
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "%s%s", Localize("Label"), ":");
				Ui()->DoLabel(&A, aBuf, FONTSIZE, TEXTALIGN_ML);
				if(Ui()->DoClearableEditBox(m_vBehaviorElements[CommandIndex].m_InputLabel.get(), &B, 10.0f))
				{
					m_vBehaviorElements[CommandIndex].UpdateLabel();
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
				Ui()->DoLabel(&A, Localize("Label type:"), FONTSIZE, TEXTALIGN_ML);
				int NewButtonLabelType = (int)m_vBehaviorElements[CommandIndex].m_CachedCommands.m_LabelType;
				B.VSplitLeft(B.w / 3.0f, &A, &B);
				B.VSplitMid(&B, &C);
				if(DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[0], LABELTYPES[0], NewButtonLabelType == 0 ? 1 : 0, &A, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
					NewButtonLabelType = 0;
				if(DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[1], LABELTYPES[1], NewButtonLabelType == 1 ? 1 : 0, &B, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
					NewButtonLabelType = 1;
				if(DoButton_Menu(&m_vBehaviorElements[CommandIndex].m_aLabelTypeRadios[0], LABELTYPES[2], NewButtonLabelType == 2 ? 1 : 0, &C, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
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
			EditBox.VSplitLeft(ROWSIZE, &B, &EditBox);
			EditBox.VSplitLeft(SUBMARGIN, nullptr, &A);
			Ui()->DoLabel(&A, Localize("Add command"), FONTSIZE, TEXTALIGN_ML);
			static CButtonContainer s_FinalAddButton;
			if(DoButton_FontIcon(&s_FinalAddButton, "+", 0, &B, BUTTONFLAG_LEFT))
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
	const std::vector<const char *> &vLabels = {Localize("Included"), Localize("Excluded"), Localize("Ignored")};
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		Block.HSplitTop((ROWGAP + ROWSIZE), &EditBox, &Block);
		if(s_VisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(ROWGAP, nullptr, &EditBox);
			EditBox.VMargin(MAINMARGIN, &EditBox);
			if(DoLine_RadioMenu(EditBox, VISIBILITIES[Current],
				   s_vVisibilitySelector[Current], vLabels, {(int)EVisibilityType::INCLUDE, (int)EVisibilityType::EXCLUDE, (int)EVisibilityType::IGNORE}, m_aCachedVisibilities[Current]))
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
	Ui()->DoLabel(&C, Localize("Long press on a touch button to select it."), 15.0f, TEXTALIGN_MC);
	MainView.HSplitTop(MAINMARGIN, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	EditBox.VSplitLeft((EditBox.w - SUBMARGIN) / 2.0f, &A, &EditBox);
	static CButtonContainer s_NewButton;
	if(DoButton_Menu(&s_NewButton, Localize("New button"), 0, &A))
		PopupCancel_NewButton();
	EditBox.VSplitLeft(SUBMARGIN, nullptr, &B);
	static CButtonContainer s_SelecteButton;
	if(DoButton_Menu(&s_SelecteButton, Localize("Select button by touch"), 0, &B))
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
		{{&LabelRect, Localize("Label")},
			{&X, Localize("X")},
			{&Y, Localize("Y")},
			{&W, Localize("Width")},
			{&H, Localize("Height")}}};
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
	Ui()->DoLabel(&CommandRect, Localize("Command"), FONTSIZE, TEXTALIGN_ML);

	if(m_NeedUpdatePreview)
	{
		m_NeedUpdatePreview = false;
		m_vpVisibleButtons = GameClient()->m_TouchControls.VisibleButtons();
		m_vpInvisibleButtons = GameClient()->m_TouchControls.InvisibleButtons();
		m_NeedSort = true;
		m_NeedFilter = true;
	}

	auto vVisibleButtons = m_vpVisibleButtons;
	auto vInvisibleButtons = m_vpInvisibleButtons;

	if(m_NeedFilter || m_NeedSort)
	{
		if(m_NeedFilter)
		{
			m_NeedFilter = false;
			std::string FilterString = m_FilterInput.GetString();
			// The two will get the filter done. Both label and command will be considered.
			{
				const auto DeleteIt = std::remove_if(vVisibleButtons.begin(), vVisibleButtons.end(), [&](CTouchControls::CTouchButton *pButton) {
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
				vVisibleButtons.erase(DeleteIt, vVisibleButtons.end());
			}
			{
				const auto DeleteIt = std::remove_if(vInvisibleButtons.begin(), vInvisibleButtons.end(), [&](CTouchControls::CTouchButton *pButton) {
					std::string Label = pButton->m_pBehavior->GetLabel().m_pLabel;
					if(std::search(Label.begin(), Label.end(), FilterString.begin(), FilterString.end(), [](char Char1, char Char2) {
						   if(Char1 >= 'A' && Char1 <= 'Z')
							   Char1 += 'a' - 'A';
						   if(Char2 >= 'A' && Char2 <= 'Z')
							   Char2 += 'a' - 'A';
						   return Char1 == Char2;
					   }) != Label.end())
						return false;
					std::string Command = DetermineTouchButtonCommandLabel(pButton);
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
		m_vpSortedButtons.clear();
		m_vpSortedButtons.reserve(vVisibleButtons.size() + vInvisibleButtons.size());
		std::for_each(vVisibleButtons.begin(), vVisibleButtons.end(), [&](auto *pButton) {
			m_vpSortedButtons.emplace_back(pButton);
		});
		std::for_each(vInvisibleButtons.begin(), vInvisibleButtons.end(), [&](auto *pButton) {
			m_vpSortedButtons.emplace_back(pButton);
		});
	}

	if(!m_vpSortedButtons.empty())
	{
		s_PreviewListBox.SetActive(true);
		s_PreviewListBox.DoStart(ROWSIZE, m_vpSortedButtons.size(), 1, 4, -1, &MainView, true, IGraphics::CORNER_B);
		for(unsigned ButtonIndex = 0; ButtonIndex < m_vpSortedButtons.size(); ButtonIndex++)
		{
			CTouchControls::CTouchButton *pButton = m_vpSortedButtons[ButtonIndex];
			const CListboxItem ListItem = s_PreviewListBox.DoNextItem(&m_vpSortedButtons[ButtonIndex], m_SelectedPreviewButtonIndex == ButtonIndex);
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
				std::string Label = pButton->m_pBehavior->GetLabel().m_pLabel;
				const auto LabelType = pButton->m_pBehavior->GetLabel().m_Type;
				if(LabelType == CTouchControls::CButtonLabel::EType::PLAIN)
				{
					SLabelProperties Props;
					Props.m_MaxWidth = A.w;
					Props.m_EllipsisAtEnd = true;
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML, Props);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::LOCALIZED)
				{
					Label = Localize(Label.c_str());
					SLabelProperties Props;
					Props.m_MaxWidth = A.w;
					Props.m_EllipsisAtEnd = true;
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML, Props);
				}
				else if(LabelType == CTouchControls::CButtonLabel::EType::ICON)
				{
					SLabelProperties Props;
					Props.m_MaxWidth = A.w;
					Props.m_EllipsisAtEnd = true;
					TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
					Ui()->DoLabel(&A, Label.c_str(), FONTSIZE, TEXTALIGN_ML, Props);
					TextRender()->SetRenderFlags(0);
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(CommandRect.w, &A, &EditBox);
				std::string Command = DetermineTouchButtonCommandLabel(pButton);
				SLabelProperties Props;
				Props.m_MaxWidth = A.w;
				Props.m_EllipsisAtEnd = true;
				Ui()->DoLabel(&A, Command.c_str(), FONTSIZE, TEXTALIGN_ML, Props);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(X.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(pButton->m_UnitRect.m_X).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(Y.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(pButton->m_UnitRect.m_Y).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(W.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(pButton->m_UnitRect.m_W).c_str(), FONTSIZE, TEXTALIGN_ML);
				EditBox.VSplitLeft(SUBMARGIN, nullptr, &EditBox);
				EditBox.VSplitLeft(H.w, &A, &EditBox);
				Ui()->DoLabel(&A, std::to_string(pButton->m_UnitRect.m_H).c_str(), FONTSIZE, TEXTALIGN_ML);
			}
		}

		m_SelectedPreviewButtonIndex = s_PreviewListBox.DoEnd();
		if(s_PreviewListBox.WasItemActivated())
		{
			GameClient()->m_TouchControls.SetSelectedButton(m_vpSortedButtons[m_SelectedPreviewButtonIndex]);
			CacheAllSettingsFromTarget(m_vpSortedButtons[m_SelectedPreviewButtonIndex]);
			SetUnsavedChanges(false);
			UpdateSampleButton();
		}
	}
	else
	{
		// Copied from menus_browser.cpp
		MainView.HMargin((MainView.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &A);
		A.HSplitTop(16.0f, &A, &B);
		B.HSplitTop(8.0f, nullptr, &B);
		B.VMargin((B.w - 200.0f) / 2.0f, &B);
		Ui()->DoLabel(&A, Localize("No buttons match your filter criteria"), 16.0f, TEXTALIGN_MC);
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
	if(DoButton_MenuTab(&s_FileTab, Localize("File"), m_CurrentMenu == EMenuType::MENU_FILE, &A, IGraphics::CORNER_TL))
		m_CurrentMenu = EMenuType::MENU_FILE;
	SelectingTab.VSplitLeft(SelectingTab.w / 3.0f, &A, &SelectingTab);
	static CButtonContainer s_ButtonTab;
	if(DoButton_MenuTab(&s_ButtonTab, Localize("Buttons"), m_CurrentMenu == EMenuType::MENU_BUTTONS, &A, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_BUTTONS;
	SelectingTab.VSplitLeft(SelectingTab.w / 2.0f, &A, &SelectingTab);
	static CButtonContainer s_SettingsMenuTab;
	if(DoButton_MenuTab(&s_SettingsMenuTab, Localize("Settings"), m_CurrentMenu == EMenuType::MENU_SETTINGS, &A, IGraphics::CORNER_NONE))
		m_CurrentMenu = EMenuType::MENU_SETTINGS;
	SelectingTab.VSplitLeft(SelectingTab.w / 1.0f, &A, &SelectingTab);
	static CButtonContainer s_PreviewTab;
	if(DoButton_MenuTab(&s_PreviewTab, Localize("Preview"), m_CurrentMenu == EMenuType::MENU_PREVIEW, &A, IGraphics::CORNER_TR))
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
	ColorHSLA ColorTest = DoLine_ColorPicker(&s_ActiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, Localize("Active Color"), &m_ColorActive, CTouchControls::DefaultBackgroundColorActive(), false, nullptr, true);
	GameClient()->m_TouchControls.SetBackgroundColorActive(color_cast<ColorRGBA>(ColorHSLA(m_ColorActive, true)));
	if(color_cast<ColorRGBA>(ColorTest) != GameClient()->m_TouchControls.BackgroundColorActive())
		GameClient()->m_TouchControls.SetEditingChanges(true);

	MainView.HSplitTop(ROWGAP, nullptr, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	static CButtonContainer s_InactiveColorPicker;
	ColorTest = DoLine_ColorPicker(&s_InactiveColorPicker, ROWSIZE, 15.0f, 5.0f, &EditBox, Localize("Inactive Color"), &m_ColorInactive, CTouchControls::DefaultBackgroundColorInactive(), false, nullptr, true);
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
	MainView.h = 3 * MAINMARGIN + 4 * ROWSIZE + 2 * ROWGAP + 250.0f;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.VMargin(MAINMARGIN, &MainView);
	MainView.HMargin(MAINMARGIN, &MainView);
	MainView.HSplitTop(ROWSIZE, &EditBox, &MainView);
	Ui()->DoLabel(&EditBox, Localize("Preview button visibility while the editor is active."), FONTSIZE, TEXTALIGN_MC);
	MainView.HSplitBottom(ROWSIZE, &MainView, &EditBox);
	MainView.HSplitBottom(ROWGAP, &MainView, nullptr);
	EditBox.VSplitLeft(MAINMARGIN, nullptr, &EditBox);
	static CButtonContainer s_PreviewAllCheckBox;
	bool Preview = GameClient()->m_TouchControls.PreviewAllButtons();
	if(DoButton_CheckBox(&s_PreviewAllCheckBox, Localize("Show all buttons"), Preview ? 1 : 0, &EditBox))
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
			if(DoButton_CheckBox(&m_aVisibilityIds[Current], VISIBILITIES[Current], aVirtualVisibilities[Current] == 1, &EditBox))
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

void CMenus::ChangeSelectedButtonWhileHavingUnsavedChanges()
{
	// Both old and new button pointer can be nullptr.
	// Saving settings to the old selected button(nullptr = create), then switch to new selected button(new = haven't created).
	PopupConfirm(Localize("Unsaved Changes"), "Save all changes before switching selected button?", Localize("Save"), Localize("Discard"), &CMenus::PopupConfirm_ChangeSelectedButton, POPUP_NONE, &CMenus::PopupCancel_ChangeSelectedButton);
}

void CMenus::PopupConfirm_ChangeSelectedButton()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_pOldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		SetUnsavedChanges(false);
		PopupCancel_ChangeSelectedButton();
	}
}

// Also a safe function for changing selected button.
void CMenus::PopupCancel_ChangeSelectedButton()
{
	GameClient()->m_TouchControls.SetSelectedButton(m_pNewSelectedButton);
	CacheAllSettingsFromTarget(m_pNewSelectedButton);
	SetUnsavedChanges(false);
	if(m_pNewSelectedButton != nullptr)
	{
		UpdateSampleButton();
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
	PopupMessage(Localize("No Space"), Localize("No space left for the button. Make sure you didn't choose wrong visibilities, or edit its size."), Localize("OK"));
}

void CMenus::SelectedButtonNotVisible()
{
	// Cancel shouldn't do anything but open ingame menu, the menu is already opened now.
	m_CloseMenu = false;
	PopupConfirm(Localize("Selected button not visible"), Localize("The selected button is not visible, do you want to deselect it or edit it's visibility?"), Localize("Deselect"), Localize("Edit"), &CMenus::PopupConfirm_SelectedNotVisible);
}

void CMenus::PopupConfirm_SelectedNotVisible()
{
	if(UnsavedChanges())
	{
		// The m_pSelectedButton can't nullptr, because this function is triggered when selected button not visible.
		m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_pNewSelectedButton = nullptr;
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
		SaveCachedSettingsToTarget(m_pOldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		PopupCancel_NewButton();
	}
}

void CMenus::PopupCancel_NewButton()
{
	// New button doesn't create a real button, instead it reset the Samplebutton to cache every setting. When saving a the Samplebutton then a real button will be created.
	CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM}, true);
	ResetButtonPointers();
	ResetCachedSettings();
	SetPosInputs(FreeRect);
	UpdateSampleButton();
	SetUnsavedChanges(true);
}

void CMenus::PopupConfirm_SaveSettings()
{
	SetUnsavedChanges(false);
	SaveCachedSettingsToTarget(m_pOldSelectedButton);
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
		SaveCachedSettingsToTarget(m_pOldSelectedButton);
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
	std::vector<std::string> Errors;
	char aBuf[512] = "";
	int X = m_InputX.GetInteger();
	int Y = m_InputY.GetInteger();
	int W = m_InputW.GetInteger();
	int H = m_InputH.GetInteger();
	// Illegal size settings.
	if(W < CTouchControls::BUTTON_SIZE_MINIMUM || W > CTouchControls::BUTTON_SIZE_MAXIMUM || H < CTouchControls::BUTTON_SIZE_MINIMUM || H > CTouchControls::BUTTON_SIZE_MAXIMUM)
	{
		Errors.emplace_back(Localize("Width and Height are required to be within the range of [50,000, 500,000]."));
		Errors.emplace_back("\n");
	}
	if(X < 0 || Y < 0 || X + W > CTouchControls::BUTTON_SIZE_SCALE || Y + H > CTouchControls::BUTTON_SIZE_SCALE)
	{
		Errors.emplace_back(Localize("Out of bound position value."));
		Errors.emplace_back("\n");
	}
	if(GameClient()->m_TouchControls.IfOverlapping({X, Y, W, H}))
	{
		Errors.emplace_back(Localize("The selected button is overlapping with other buttons."));
		Errors.emplace_back("\n");
	}
	if(!Errors.empty())
	{
		memset(aBuf, 0, sizeof(aBuf));
		for(const std::string &Error : Errors)
		{
			char aErrorBuf[1024];
			str_format(aErrorBuf, sizeof(aErrorBuf), "%s%s", aBuf, Error.c_str());
			str_copy(aBuf, aErrorBuf, sizeof(aErrorBuf));
		}
		PopupMessage(Localize("Illegal settings"), aBuf, Localize("OK"));
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
void CMenus::CacheAllSettingsFromTarget(CTouchControls::CTouchButton *pTargetButton)
{
	// This reset will as well give m_vCachedCommands one default member.
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
		if((int)Visibility.m_Type >= (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES)
			dbg_assert(false, "Target button has out of bound visibility type: %d", (int)Visibility.m_Type);
		m_aCachedVisibilities[(int)Visibility.m_Type] = Visibility.m_Parity ? (int)EVisibilityType::INCLUDE : (int)EVisibilityType::EXCLUDE;
	}

	// These are behavior values.
	if(pTargetButton->m_pBehavior != nullptr)
	{
		std::string BehaviorType = pTargetButton->m_pBehavior->GetBehaviorType();
		if(BehaviorType == "bind")
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND;
			auto *pCastedBehavior = static_cast<CTouchControls::CBindTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
			// m_LabelType must not be null. Default value is PLAIN.
			m_vBehaviorElements[0].m_CachedCommands = {pCastedBehavior->GetLabel().m_pLabel, pCastedBehavior->GetLabel().m_Type, pCastedBehavior->GetCommand().c_str()};
			m_vBehaviorElements[0].UpdateInputs();
		}
		else if(BehaviorType == "bind-toggle")
		{
			m_EditBehaviorType = (int)EBehaviorType::BIND_TOGGLE;
			auto *pCastedBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
			auto TargetCommands = pCastedBehavior->GetCommand();
			// Can't use resize here :(
			while(m_vBehaviorElements.size() > minimum<size_t>(TargetCommands.size(), 2))
				m_vBehaviorElements.pop_back();
			while(m_vBehaviorElements.size() > minimum<size_t>(TargetCommands.size(), 2))
				m_vBehaviorElements.emplace_back(GameClient());
			for(unsigned CommandIndex = 0; CommandIndex < TargetCommands.size(); CommandIndex++)
			{
				m_vBehaviorElements[CommandIndex].m_CachedCommands = TargetCommands[CommandIndex];
				m_vBehaviorElements[CommandIndex].UpdateInputs();
			}
		}
		else if(BehaviorType == "predefined")
		{
			m_EditBehaviorType = (int)EBehaviorType::PREDEFINED;
			const char *PredefinedType = pTargetButton->m_pBehavior->GetPredefinedType();
			if(PredefinedType == nullptr)
				m_PredefinedBehaviorType = (int)EPredefinedType::EXTRA_MENU;
			else
				m_PredefinedBehaviorType = CalculatePredefinedType(PredefinedType);

			if(m_PredefinedBehaviorType == (int)EPredefinedType::NUM_PREDEFINEDS)
				dbg_assert(false, "Detected out of bound m_PredefinedBehaviorType. PredefinedType = %s", PredefinedType);

			if(m_PredefinedBehaviorType == (int)EPredefinedType::EXTRA_MENU)
			{
				auto *pCastedBehavior = static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pTargetButton->m_pBehavior.get());
				m_CachedExtraMenuNumber = pCastedBehavior->GetNumber();
			}
		}
		else // Empty
			dbg_assert(false, "Detected out of bound value in m_EditBehaviorType");
	}
}

// Will override everything in the button. If nullptr is passed, a new button will be created.
void CMenus::SaveCachedSettingsToTarget(CTouchControls::CTouchButton *pTargetButton)
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
		GameClient()->m_TouchControls.SetSelectedButton(pTargetButton);
	}

	pTargetButton->m_UnitRect.m_W = clamp(m_InputW.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	pTargetButton->m_UnitRect.m_H = clamp(m_InputH.GetInteger(), CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MAXIMUM);
	pTargetButton->m_UnitRect.m_X = clamp(m_InputX.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - pTargetButton->m_UnitRect.m_W);
	pTargetButton->m_UnitRect.m_Y = clamp(m_InputY.GetInteger(), 0, CTouchControls::BUTTON_SIZE_SCALE - pTargetButton->m_UnitRect.m_H);
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
void CMenus::SetPosInputs(CTouchControls::CUnitRect MyRect)
{
	m_InputX.SetInteger(MyRect.m_X);
	m_InputY.SetInteger(MyRect.m_Y);
	m_InputW.SetInteger(MyRect.m_W);
	m_InputH.SetInteger(MyRect.m_H);
}

// Used to make sure the input box is numbers only, also clamp the value.
void CMenus::InputPosFunction(CLineInputNumber *pInput)
{
	int InputValue = pInput->GetInteger();
	// Deal with the "-1" FindPositionXY give.
	InputValue = clamp(InputValue, 0, CTouchControls::BUTTON_SIZE_SCALE);
	pInput->SetInteger(InputValue);
	SetUnsavedChanges(true);
}

// Update m_pSampleButton in CTouchControls. The Samplebutton is used for showing on screen.
void CMenus::UpdateSampleButton()
{
	GameClient()->m_TouchControls.RemakeSampleButton();
	SaveCachedSettingsToTarget(GameClient()->m_TouchControls.SampleButton());
	GameClient()->m_TouchControls.SetShownRect(GameClient()->m_TouchControls.SampleButton()->m_UnitRect);
}

// Not inline so there's no more includes in menus.h. A shortcut to the function in CTouchControls.
void CMenus::ResetButtonPointers()
{
	GameClient()->m_TouchControls.ResetButtonPointers();
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
			case(int)CTouchControls::EIssueType::CACHE_SETTINGS: CacheAllSettingsFromTarget(Issues[Current].m_pTargetButton); break;
			case(int)CTouchControls::EIssueType::SAVE_SETTINGS: SaveCachedSettingsToTarget(Issues[Current].m_pTargetButton); break;
			case(int)CTouchControls::EIssueType::CACHE_POSITION: SetPosInputs(Issues[Current].m_pTargetButton->m_UnitRect); break;
			default: dbg_assert(false, "Unknown Issue type: %d", (int)Current);
			}
		}
	}
}

// Turn predefined behavior strings like "joystick-hook" into integers according to the enum.
int CMenus::CalculatePredefinedType(const char *pType)
{
	int IntegerType;
	for(IntegerType = (int)EPredefinedType::EXTRA_MENU;
		str_comp(pType, GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[IntegerType].m_pId) != 0 && IntegerType < (int)EPredefinedType::NUM_PREDEFINEDS;
		IntegerType++)
		;
	return IntegerType;
}

std::string CMenus::DetermineTouchButtonCommandLabel(CTouchControls::CTouchButton *pButton)
{
	std::string BehaviorType = pButton->m_pBehavior->GetBehaviorType();
	if(BehaviorType == CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		return static_cast<CTouchControls::CBindTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetCommand();
	}
	else if(BehaviorType == CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		const auto *pBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pButton->m_pBehavior.get());
		return pBehavior->GetCommand()[pBehavior->GetActiveCommandIndex()].m_Command;
	}
	else if(BehaviorType == CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE)
	{
		std::string PredefinedType = pButton->m_pBehavior->GetPredefinedType();
		std::string Command = PREDEFINEDS[CalculatePredefinedType(PredefinedType.c_str())];
		if(PredefinedType == CTouchControls::CExtraMenuTouchButtonBehavior::BEHAVIOR_ID)
		{
			Command += " " + std::to_string(static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetNumber());
		}
		return Command;
	}
	else
	{
		dbg_assert(false, "Detected unknown behavior type in CMenus::GetCommand()");
	}
}

// Used for making json chars like \n or \uf3ce visible.
std::string CMenus::CBehaviorElements::ParseLabel(const char *pLabel)
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

CMenus::CBehaviorElements::CBehaviorElements(CGameClient *pGameClient) noexcept :
	m_pGameClient(pGameClient)
{
	m_InputCommand = std::make_unique<CLineInputBuffered<1024>>();
	m_InputLabel = std::make_unique<CLineInputBuffered<1024>>();
	Reset();
}

CMenus::CBehaviorElements::CBehaviorElements(CBehaviorElements &&Other) noexcept :
	m_InputCommand(std::move(Other.m_InputCommand)), m_InputLabel(std::move(Other.m_InputLabel)), m_CachedCommands(std::move(Other.m_CachedCommands)), m_pGameClient(Other.m_pGameClient)
{
}

CMenus::CBehaviorElements::~CBehaviorElements()
{
	if(m_InputCommand != nullptr)
		m_InputCommand->Deactivate();
	if(m_InputLabel != nullptr)
		m_InputLabel->Deactivate();
	if(m_pGameClient != nullptr)
		m_pGameClient->Ui()->SetActiveItem(nullptr);
}

CMenus::CBehaviorElements &CMenus::CBehaviorElements::operator=(CBehaviorElements &&Other) noexcept
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

void CMenus::CBehaviorElements::UpdateInputs()
{
	m_InputLabel->Set(ParseLabel(m_CachedCommands.m_Label.c_str()).c_str());
	m_InputCommand->Set(m_CachedCommands.m_Command.c_str());
}

void CMenus::CBehaviorElements::Reset()
{
	m_CachedCommands = {};
	m_InputCommand->Clear();
	m_InputLabel->Clear();
}
/*
	Note: FindPositionXY is used for finding a position of the current moving rect not overlapping with other visible rects.
		  It's a bit slow, time = o(n^2 * logn), maybe need optimization in the future.

	General Logic: key elements: unique_ptr<CTouchButton>m_pSampleButton, optional<CUnitRect>m_ShownRect, CachedSettings(A group of elements stored in menus.h)
								   CTouchButton *m_pSelectedButton, m_vTouchButton, touch_controls.json
				   touch_controls.json stores all buttons that are already saved to the system, when you enter the game,
				   The buttons in touch_conrtols.json will be parsed into m_vTouchButton.
				   m_vTouchButton stores currently real ingame buttons, when you quit the editor, only buttons in m_vTouchButton will exist.
				   m_pSelectedButton is a pointer that points to an exact button in m_vTouchButton or nullptr, not anything else.
				   Its data shouldn't be changed anytime except when player wants to save the cached controls.
				   Upon changing the member it's pointing to, will check if there's unsaved changes, and popup confirm if saving data before changing.
				   If changes are made through sliding screen, and only for a small distance(<10000 unit), will not consider it as a change. Any changes made in editor will be considered as a change.
				   m_pSampleButton stores current settings, when it is nullptr, usually no button selected, no settings that cached(Cached settings won't be deleted. They still exist but meaningless), will update when changes are made in editor as well.
				   m_ShownRect for rendering the m_pSampleButton.
				   SelectedButton won't be rendered, instead it will render m_ShownRect(Using m_pSampleButton's behavior). While sliding on screen directly,
				   SampleButton will be overlapping with other buttons, m_ShownRect will get a position that not overlapping, as well closest to SampleButton.
				   So m_ShownRect could pass through blocks.
				   m_ShownRect is also used for saving the UnitRect data, so don't use m_pSampleButton's unitrect, it might be dangerous.
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
