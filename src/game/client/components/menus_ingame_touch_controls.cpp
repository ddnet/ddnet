#include <base/color.h>
#include <base/system.h>

#include <game/client/components/touch_controls.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "menus.h"

static const char *BEHAVIORS[] = {"Bind", "Bind Toggle", "Predefined"};
static const char *PREDEFINEDS[] = {"Extra Menu", "Joystick Hook", "Joystick Fire", "Joystick Aim", "Joystick Action", "Use Action", "Swap Action", "Spectate", "Emoticon", "Ingame Menu"};
static const char *LABELTYPES[] = {"Plain", "Localized", "Icon"};
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
	// These values can't be null. The constructor has been updated. Default:{0,0,50000,50000}, shape = rect.
	m_InputX.Set(std::to_string(TargetButton->m_UnitRect.m_X).c_str());
	m_InputY.Set(std::to_string(TargetButton->m_UnitRect.m_Y).c_str());
	m_InputW.Set(std::to_string(TargetButton->m_UnitRect.m_W).c_str());
	m_InputH.Set(std::to_string(TargetButton->m_UnitRect.m_H).c_str());
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
			m_EditBehaviorType = 0;
			auto *CastedBehavior = static_cast<CTouchControls::CBindTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
			// Take care m_LabelType must not be null as for now. When adding a new button give it a default value or cry.
			m_vCachedCommands[0].m_Label = CastedBehavior->GetLabel().m_pLabel;
			m_vCachedCommands[0].m_LabelType = CastedBehavior->GetLabel().m_Type;
			m_vCachedCommands[0].m_Command = CastedBehavior->GetCommand();
			m_InputCommand.Set(CastedBehavior->GetCommand().c_str());
			m_InputLabel.Set(CastedBehavior->GetLabel().m_pLabel);
		}
		else if(BehaviorType == "bind-toggle")
		{
			m_EditBehaviorType = 1;
			auto *CastedBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
			m_vCachedCommands = CastedBehavior->GetCommand();
			m_EditCommandNumber = 0;
			if(!m_vCachedCommands.empty())
			{
				m_InputCommand.Set(m_vCachedCommands[0].m_Command.c_str());
				m_InputLabel.Set(m_vCachedCommands[0].m_Label.c_str());
			}
		}
		else if(BehaviorType == "predefined")
		{
			m_EditBehaviorType = 2;
			const char *PredefinedType = TargetButton->m_pBehavior->GetPredefinedType();
			if(PredefinedType == nullptr)
				m_PredefinedBehaviorType = 0;
			else
				for(m_PredefinedBehaviorType = 0; m_PredefinedBehaviorType < 10 && str_comp(PredefinedType, GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[m_PredefinedBehaviorType].m_pId) != 0; m_PredefinedBehaviorType++)
					/*Update m_PredefinedBehaviorType, nothing else should be done here*/;

			if(m_PredefinedBehaviorType == 10)
				dbg_assert(false, "Detected out of bound m_PredefinedBehaviorType. PredefinedType = %s", PredefinedType);

			if(m_PredefinedBehaviorType == 0)
			{
				auto *CastedBehavior = static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(TargetButton->m_pBehavior.get());
				GameClient()->m_TouchControls.m_CachedNumber = CastedBehavior->GetNumber();
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
	m_EditBehaviorType = 0;
	m_PredefinedBehaviorType = 0;
	GameClient()->m_TouchControls.m_CachedNumber = 0;
	m_EditCommandNumber = 0;
	m_vCachedCommands.clear();
	m_vCachedCommands.reserve(5);
	m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
	m_aCachedVisibilities.fill(2); // 2 means don't have the visibility, true:1,false:0
	// These things can't be empty. std::stoi can't cast empty string.
	m_InputX.Set("0");
	m_InputY.Set("0");
	m_InputW.Set("50000");
	m_InputH.Set("50000");
	m_InputLabel.Set("");
	m_InputCommand.Set("");
	m_CachedShape = CTouchControls::EButtonShape::RECT;
}

void CMenus::InputPosFunction(CLineInputBuffered<7> *Input)
{
	std::string InputValue = Input->GetString();
	// Deal with the "-1" FindPositionXY give.
	auto Remove = std::remove_if(InputValue.begin(), InputValue.end(), [](char Obj) {
		return Obj < '0' || Obj > '9';
	});
	InputValue.erase(Remove, InputValue.end());
	auto LeadingZero = std::find_if(InputValue.begin(), InputValue.end(), [](char Value) {
		return Value != '0';
	});
	InputValue.erase(InputValue.begin(), LeadingZero);
	if(InputValue == "")
		InputValue = "0";
	Input->Set(InputValue.c_str());
	m_UnsavedChanges = true;
}

void CMenus::RenderTouchButtonEditor(CUIRect MainView)
{
	// Used to decide if need to update the tmpbutton.
	bool Changed = false;
	CUIRect Left, Right, A, B, EditBox, VisRec;
	MainView.VSplitLeft(MainView.w / 4.0f, &Left, &Right);
	Left.Margin(5.0f, &Left);
	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	EditBox.VSplitLeft(25.0f, &A, &EditBox);
	Ui()->DoLabel(&A, "X:", 16.0f, TEXTALIGN_ML);
	if(Ui()->DoClearableEditBox(&m_InputX, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputX);
		Changed = true;
	}

	// Auto check if the input value contains char that is not digit. If so delete it.
	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	EditBox.VSplitLeft(25.0f, &A, &EditBox);
	Ui()->DoLabel(&A, "Y:", 16.0f, TEXTALIGN_ML);
	if(Ui()->DoClearableEditBox(&m_InputY, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputY);
		Changed = true;
	}

	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	EditBox.VSplitLeft(25.0f, &A, &EditBox);
	Ui()->DoLabel(&A, "W:", 16.0f, TEXTALIGN_ML);
	if(Ui()->DoClearableEditBox(&m_InputW, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputW);
		Changed = true;
	}

	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	EditBox.VSplitLeft(25.0f, &A, &EditBox);
	Ui()->DoLabel(&A, "H:", 16.0f, TEXTALIGN_ML);
	if(Ui()->DoClearableEditBox(&m_InputH, &EditBox, 12.0f))
	{
		InputPosFunction(&m_InputH);
		Changed = true;
	}

	// Drop down menu for shapes
	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	Ui()->DoLabel(&A, "Shape:", 16.0f, TEXTALIGN_ML);
	static CUi::SDropDownState s_ButtonShapeDropDownState;
	static CScrollRegion s_ButtonShapeDropDownScrollRegion;
	s_ButtonShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonShapeDropDownScrollRegion;
	const char *Shapes[] = {"Rect", "Circle"};
	const CTouchControls::EButtonShape NewButtonShape = (CTouchControls::EButtonShape)Ui()->DoDropDown(&B, (int)m_CachedShape, Shapes, std::size(Shapes), s_ButtonShapeDropDownState);
	if(NewButtonShape != m_CachedShape)
	{
		m_CachedShape = NewButtonShape;
		m_UnsavedChanges = true;
		Changed = true;
	}

	// Right for behaviors, left(center) for visibility. They share 0.75 width of mainview, each 0.375.
	Right.VSplitMid(&VisRec, &Right);
	Right.Margin(5.0f, &Right);
	Right.HSplitTop(25.0f, &EditBox, &Right);
	Right.HSplitTop(5.0f, nullptr, &Right);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
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
		if(m_EditBehaviorType == 0)
		{
			m_InputLabel.Set(m_vCachedCommands[0].m_Label.c_str());
			m_InputCommand.Set(m_vCachedCommands[0].m_Command.c_str());
		}
		if(m_EditBehaviorType == 1)
		{
			if(m_vCachedCommands.size() <= (size_t)(m_EditCommandNumber))
				m_EditCommandNumber = 0;
			while(m_vCachedCommands.size() < 2)
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
			m_InputLabel.Set(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
		}
		m_UnsavedChanges = true;
		Changed = true;
	}

	Right.HSplitTop(25.0f, &EditBox, &Right);
	Right.HSplitTop(5.0f, nullptr, &Right);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == 0)
	{
		Ui()->DoLabel(&A, "Command:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputCommand, &B, 10.0f))
		{
			m_vCachedCommands[0].m_Command = m_InputCommand.GetString();
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == 1)
	{
		Ui()->DoLabel(&A, "Number:", 16.0f, TEXTALIGN_ML);
		// Decrease Button, increase button and delete button share 1/2 width of B, the rest is for number. 1/6, 1/2, 1/6, 1/6.
		B.VSplitLeft(B.w / 6, &A, &B);
		static CButtonContainer s_DecreaseButton;
		if(DoButton_Menu(&s_DecreaseButton, "-", 0, &A))
		{
			if(m_EditCommandNumber > 0)
			{
				m_EditCommandNumber--;
			}
			if(m_vCachedCommands.size() <= static_cast<size_t>(m_EditCommandNumber))
			{
				dbg_assert(false, "commands.size < number at do decrease button");
			}
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
			m_InputLabel.Set(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
		}
		B.VSplitLeft(B.w * 0.6f, &A, &B);
		// m_EditCommandNumber counts from 0. But shown from 1.
		Ui()->DoLabel(&A, std::to_string(m_EditCommandNumber + 1).c_str(), 16.0f, TEXTALIGN_MC);
		B.VSplitLeft(B.w / 2.0f, &A, &B);
		static CButtonContainer s_IncreaseButton;
		if(DoButton_Menu(&s_IncreaseButton, "+", 0, &A))
		{
			m_EditCommandNumber++;
			if((int)m_vCachedCommands.size() < m_EditCommandNumber + 1)
			{
				m_vCachedCommands.emplace_back("", CTouchControls::CButtonLabel::EType::PLAIN, "");
				m_UnsavedChanges = true;
				Changed = true;
			}
			if(m_vCachedCommands.size() <= static_cast<size_t>(m_EditCommandNumber))
				dbg_assert(false, "commands.size < number at do increase button");
			m_InputCommand.Set(m_vCachedCommands[m_EditCommandNumber].m_Command.c_str());
			m_InputLabel.Set(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
		}
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, FontIcons::FONT_ICON_TRASH, 0, &B))
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
			m_InputLabel.Set(m_vCachedCommands[m_EditCommandNumber].m_Label.c_str());
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == 2)
	{
		Ui()->DoLabel(&A, "Type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonPredefinedDropDownState;
		static CScrollRegion s_ButtonPredefinedDropDownScrollRegion;
		s_ButtonPredefinedDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonPredefinedDropDownScrollRegion;
		const int NewPredefined = Ui()->DoDropDown(&B, m_PredefinedBehaviorType, PREDEFINEDS, std::size(PREDEFINEDS), s_ButtonPredefinedDropDownState);
		if(NewPredefined != m_PredefinedBehaviorType)
		{
			m_PredefinedBehaviorType = NewPredefined;
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	Right.HSplitTop(25.0f, &EditBox, &Right);
	Right.HSplitTop(5.0f, nullptr, &Right);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == 0)
	{
		Ui()->DoLabel(&A, "Label:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputLabel, &B, 10.0f))
		{
			m_vCachedCommands[0].m_Label = m_InputLabel.GetString();
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == 1)
	{
		Ui()->DoLabel(&A, "Command:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputCommand, &B, 10.0f))
		{
			m_vCachedCommands[m_EditCommandNumber].m_Command = m_InputCommand.GetString();
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == 2 && m_PredefinedBehaviorType == 0) // Extra menu type, needs to input number.
	{
		// Increase & Decrease button share 1/2 width, the rest is for label.
		EditBox.VSplitLeft(EditBox.w / 4, &A, &B);
		SMenuButtonProperties Props;
		Props.m_UseIconFont = true;
		static CButtonContainer s_ExtraMenuDecreaseButton;
		if(DoButton_Menu(&s_ExtraMenuDecreaseButton, "-", 0, &A))
		{
			if(GameClient()->m_TouchControls.m_CachedNumber > 0)
			{
				// Menu Number also counts from 1, but written as 0.
				GameClient()->m_TouchControls.m_CachedNumber--;
				m_UnsavedChanges = true;
				Changed = true;
			}
		}

		B.VSplitLeft(B.w * 2 / 3.0f, &A, &B);
		Ui()->DoLabel(&A, std::to_string(GameClient()->m_TouchControls.m_CachedNumber + 1).c_str(), 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ExtraMenuIncreaseButton;
		if(DoButton_Menu(&s_ExtraMenuIncreaseButton, "+", 0, &B))
		{
			if(GameClient()->m_TouchControls.m_CachedNumber < 4)
			{
				GameClient()->m_TouchControls.m_CachedNumber++;
				m_UnsavedChanges = true;
				Changed = true;
			}
		}
	}
	Right.HSplitTop(25.0f, &EditBox, &Right);
	Right.HSplitTop(5.0f, nullptr, &Right);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == 0)
	{
		Ui()->DoLabel(&A, "Label type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonLabelTypeDropDownState;
		static CScrollRegion s_ButtonLabelTypeDropDownScrollRegion;
		s_ButtonLabelTypeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonLabelTypeDropDownScrollRegion;
		const CTouchControls::CButtonLabel::EType NewButtonLabelType = (CTouchControls::CButtonLabel::EType)Ui()->DoDropDown(&B, (int)m_vCachedCommands[0].m_LabelType, LABELTYPES, std::size(LABELTYPES), s_ButtonLabelTypeDropDownState);
		if(NewButtonLabelType != m_vCachedCommands[0].m_LabelType)
		{
			m_vCachedCommands[0].m_LabelType = NewButtonLabelType;
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	else if(m_EditBehaviorType == 1)
	{
		Ui()->DoLabel(&A, "Label:", 16.0f, TEXTALIGN_ML);
		if(Ui()->DoClearableEditBox(&m_InputLabel, &B, 10.0f))
		{
			m_vCachedCommands[m_EditCommandNumber].m_Label = m_InputLabel.GetString();
			m_UnsavedChanges = true;
			Changed = true;
		}
	}
	Right.HSplitTop(25.0f, &EditBox, &Right);
	Right.HSplitTop(5.0f, nullptr, &Right);
	EditBox.VSplitLeft(EditBox.w / 2.0f, &A, &B);
	A.VMargin(5.0f, &A);
	B.VMargin(5.0f, &B);
	if(m_EditBehaviorType == 1)
	{
		Ui()->DoLabel(&A, "Label type:", 16.0f, TEXTALIGN_ML);
		static CUi::SDropDownState s_ButtonLabelTypeDropDownState;
		static CScrollRegion s_ButtonLabelTypeDropDownScrollRegion;
		s_ButtonLabelTypeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ButtonLabelTypeDropDownScrollRegion;
		const CTouchControls::CButtonLabel::EType NewButtonLabelType = (CTouchControls::CButtonLabel::EType)Ui()->DoDropDown(&B, (int)m_vCachedCommands[m_EditCommandNumber].m_LabelType, LABELTYPES, std::size(LABELTYPES), s_ButtonLabelTypeDropDownState);
		if(NewButtonLabelType != m_vCachedCommands[m_EditCommandNumber].m_LabelType)
		{
			m_vCachedCommands[m_EditCommandNumber].m_LabelType = NewButtonLabelType;
			m_UnsavedChanges = true;
			Changed = true;
		}
	}

	// Visibilities time. This is button's visibility, not virtual.
	VisRec.h = 150.0f;
	VisRec.Margin(5.0f, &VisRec);
	static CScrollRegion s_VisibilityScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	VisRec.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	s_VisibilityScrollRegion.Begin(&VisRec, &ScrollOffset);
	VisRec.y += ScrollOffset.y;
	const std::array<const char *, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> VisibilityStrings = {"Ingame", "Zoom Allowed", "Vote Active", "Dummy Allowed", "Dummy Connected", "Rcon Authed",
		"Demo Player", "Extra Menu 1", "Extra Menu 2", "Extra Menu 3", "Extra Menu 4", "Extra Menu 5"};
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		VisRec.HSplitTop(30.0f, &EditBox, &VisRec);
		if(s_VisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(5.0f, nullptr, &EditBox);
			if(Ui()->DoButtonLogic(&m_aButtonVisibilityIds[Current], 0, &EditBox, BUTTONFLAG_LEFT))
			{
				m_aCachedVisibilities[Current] += 2;
				m_aCachedVisibilities[Current] %= 3;
				m_UnsavedChanges = true;
				Changed = true;
			}
			TextRender()->TextColor(LABELCOLORS[m_aCachedVisibilities[Current] == 2 ? 0 : 1]);
			char aBuf[20];
			str_format(aBuf, sizeof(aBuf), "%s%s", m_aCachedVisibilities[Current] == 0 ? "-" : "+", VisibilityStrings[Current]);
			Ui()->DoLabel(&EditBox, aBuf, 16.0f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
	s_VisibilityScrollRegion.End();

	// Combine left and right together.
	Left.w = MainView.w;
	Left.w -= 10.0f;
	Left.HSplitTop(25.0f, &EditBox, &Left);
	Left.HSplitTop(5.0f, nullptr, &Left);
	// Confirm && Cancel button share 1/2 width, and they will be shaped into square, placed at the middle of their space.
	EditBox.VSplitLeft(EditBox.w / 4.0f, &A, &EditBox);
	A.VMargin((A.w - 100.0f) / 2.0f, &A);
	static CButtonContainer s_ConfirmButton;
	if(DoButton_Menu(&s_ConfirmButton, "Save", 0, &A))
	{
		m_OldSelectedButton = GameClient()->m_TouchControls.m_pSelectedButton;
		if(CheckCachedSettings())
		{
			SaveCachedSettingsToTarget(m_OldSelectedButton);
			GameClient()->m_TouchControls.SetEditingChanges(true);
			m_UnsavedChanges = false;
		}
	}

	EditBox.VSplitLeft(EditBox.w * 2.0f / 3.0f, &A, &B);
	if(m_UnsavedChanges)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&A, Localize("Unsaved changes"), 10.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	B.VMargin((B.w - 100.0f) / 2.0f, &B);
	static CButtonContainer s_CancelButton;
	if(DoButton_Menu(&s_CancelButton, "Cancel", 0, &B))
	{
		// Since the settings are cancelled, reset the cached settings to m_pSelectedButton though selected button didn't change.
		CacheAllSettingsFromTarget(GameClient()->m_TouchControls.m_pSelectedButton);
		if(GameClient()->m_TouchControls.m_pSelectedButton == nullptr)
		{
			GameClient()->m_TouchControls.m_pTmpButton = nullptr;
			GameClient()->m_TouchControls.m_ShownRect = std::nullopt;
		}
		m_UnsavedChanges = false;
		Changed = false;
	}

	Left.HSplitTop(25.0f, &EditBox, &Left);
	EditBox.VSplitLeft(EditBox.w / 4.0f, &A, &B);
	A.VMargin(10.0f, &A);
	static CButtonContainer s_AddNewButton;
	bool Checked = GameClient()->m_TouchControls.m_pSelectedButton == nullptr;
	if(DoButton_Menu(&s_AddNewButton, "New Button", Checked ? 1 : 0, &A))
	{
		CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, 50000, 50000}, true);
		if(Checked)
		{
			PopupMessage("Already Created New Button", "A new button is already created, please save or delete it before creating a new one", "OK");
		}
		else if(FreeRect.m_X == -1)
		{
			PopupMessage("No Space", "No enough space for another button.", "OK");
		}
		else if(m_UnsavedChanges)
		{
			PopupConfirm("Unsaved Changes", "Save all changes before creating another button?", "Save", "Discard", &CMenus::PopupConfirm_NewButton, POPUP_NONE, &CMenus::PopupCancel_NewButton);
		}
		else
		{
			PopupCancel_NewButton();
		}
	}

	B.VSplitLeft(B.w / 3.0f, &A, &B);
	A.VMargin(10.0f, &A);
	static CButtonContainer s_RemoveButton;
	if(DoButton_Menu(&s_RemoveButton, "Delete Button", 0, &A))
	{
		GameClient()->m_TouchControls.DeleteButton();
	}

	// Create a new button with current cached settings. Auto moving to nearest empty space.
	B.VSplitLeft(B.w / 2.0f, &A, &B);
	A.VMargin(10.0f, &A);
	static CButtonContainer s_CopyPasteButton;
	if(DoButton_Menu(&s_CopyPasteButton, "C&P Button", m_UnsavedChanges || Checked ? 1 : 0, &A))
	{
		if(Checked)
		{
			PopupMessage("Already Created New Button", "A new button is already created, please save or delete it before creating a new one", "OK");
		}
		else if(m_UnsavedChanges)
		{
			PopupMessage("Unsaved Changes", "Save changes before copy paste a button.", "OK");
		}
		else
		{
			CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition(GameClient()->m_TouchControls.m_ShownRect.value(), true);
			if(FreeRect.m_X == -1)
			{
				FreeRect.m_W = 50000;
				FreeRect.m_H = 50000;
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
				GameClient()->m_TouchControls.m_pSelectedButton = nullptr;
				m_InputX.Set(std::to_string(FreeRect.m_X).c_str());
				m_InputY.Set(std::to_string(FreeRect.m_Y).c_str());
				m_InputW.Set(std::to_string(FreeRect.m_W).c_str());
				m_InputH.Set(std::to_string(FreeRect.m_H).c_str());
				SaveCachedSettingsToTarget(GameClient()->m_TouchControls.m_pTmpButton.get());
				GameClient()->m_TouchControls.m_ShownRect = GameClient()->m_TouchControls.m_pTmpButton->m_UnitRect;
				Changed = true;
				m_UnsavedChanges = true;
			}
		}
	}

	// Deselect a button.
	B.VMargin(10.0f, &B);
	static CButtonContainer s_DeselectButton;
	if(DoButton_Menu(&s_DeselectButton, "De-Select", 0, &B))
	{
		m_OldSelectedButton = GameClient()->m_TouchControls.m_pSelectedButton;
		m_NewSelectedButton = nullptr;
		if(m_UnsavedChanges)
		{
			PopupConfirm("Unsaved Changes", "Do you want to save all changes before de-select?", "Save", "Discard", &CMenus::PopupConfirm_DeselectButton, POPUP_NONE, &CMenus::PopupCancel_DeselectButton);
		}
	}

	if(Changed == true)
	{
		SaveCachedSettingsToTarget(GameClient()->m_TouchControls.m_pTmpButton.get());
		GameClient()->m_TouchControls.m_ShownRect = GameClient()->m_TouchControls.m_pTmpButton->m_UnitRect;
	}
}

void CMenus::RenderVirtualVisibilityEditor(CUIRect MainView)
{
	CUIRect EditBox;
	const std::array<const ColorRGBA, 2> LabelColor = {ColorRGBA(0.3f, 0.3f, 0.3f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)};
	CUIRect Label;
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitTop(25.0f, &Label, &MainView);
	MainView.HMargin(5.0f, &MainView);
	Ui()->DoLabel(&Label, Localize("Edit Visibilities"), 20.0f, TEXTALIGN_MC);
	const std::array<const char *, (size_t)CTouchControls::EButtonVisibility::NUM_VISIBILITIES> VisibilityStrings = {"Ingame", "Zoom Allowed", "Vote Active", "Dummy Allowed", "Dummy Connected", "Rcon Authed",
		"Demo Player", "Extra Menu 1", "Extra Menu 2", "Extra Menu 3", "Extra Menu 4", "Extra Menu 5"};
	static CScrollRegion s_VirtualVisibilityScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	MainView.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	s_VirtualVisibilityScrollRegion.Begin(&MainView, &ScrollOffset);
	MainView.y += ScrollOffset.y;
	for(unsigned Current = 0; Current < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Current)
	{
		MainView.HSplitTop(30.0f, &EditBox, &MainView);
		if(s_VirtualVisibilityScrollRegion.AddRect(EditBox))
		{
			EditBox.HSplitTop(5.0f, nullptr, &EditBox);
			if(Ui()->DoButtonLogic(&m_aVisibilityIds[Current], 0, &EditBox, BUTTONFLAG_LEFT))
			{
				GameClient()->m_TouchControls.m_aVirtualVisibilities[Current] = !GameClient()->m_TouchControls.m_aVirtualVisibilities[Current];
			}
			TextRender()->TextColor(LabelColor[GameClient()->m_TouchControls.m_aVirtualVisibilities[Current] ? 1 : 0]);
			Ui()->DoLabel(&EditBox, VisibilityStrings[Current], 16.0f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
	s_VirtualVisibilityScrollRegion.End();
}

void CMenus::ChangeSelectedButtonWhileHavingUnsavedChanges(CTouchControls::CTouchButton *OldSelectedButton, CTouchControls::CTouchButton *NewSelectedButton)
{
	// Both can be nullptr.
	// Saving settings to the old selected button, then switch to new selected button.
	m_OldSelectedButton = OldSelectedButton;
	m_NewSelectedButton = NewSelectedButton;
	m_CloseMenu = false;
	if(!GameClient()->m_Menus.IsActive())
	{
		GameClient()->m_Menus.SetActive(true);
		m_CloseMenu = true;
	}
	PopupConfirm("Unsaved changes", "Save all changes before switching selected button?", "Save", "Discard", &CMenus::PopupConfirm_ChangeSelectedButton, POPUP_NONE, &CMenus::PopupCancel_ChangeSelectedButton);
}

void CMenus::SaveCachedSettingsToTarget(CTouchControls::CTouchButton *TargetButton)
{
	// Save the cached config to the target button. If no button to save, create a new one, then select it.
	if(TargetButton == nullptr)
	{
		TargetButton = GameClient()->m_TouchControls.NewButton();
		GameClient()->m_TouchControls.m_pSelectedButton = TargetButton;
	}

	TargetButton->m_UnitRect.m_W = clamp(std::stoi(m_InputW.GetString()), 50000, 500000);
	TargetButton->m_UnitRect.m_H = clamp(std::stoi(m_InputH.GetString()), 50000, 500000);
	TargetButton->m_UnitRect.m_X = clamp(std::stoi(m_InputX.GetString()), 0, 1000000 - TargetButton->m_UnitRect.m_W);
	TargetButton->m_UnitRect.m_Y = clamp(std::stoi(m_InputY.GetString()), 0, 1000000 - TargetButton->m_UnitRect.m_H);
	TargetButton->m_vVisibilities.clear();
	for(unsigned Iterator = (unsigned)CTouchControls::EButtonVisibility::INGAME; Iterator < (unsigned)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; ++Iterator)
	{
		if(m_aCachedVisibilities[Iterator] != 2)
			TargetButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Iterator, static_cast<bool>(m_aCachedVisibilities[Iterator]));
	}
	TargetButton->m_Shape = m_CachedShape;
	TargetButton->UpdateScreenFromUnitRect();
	if(m_EditBehaviorType == 0)
	{
		TargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindTouchButtonBehavior>(m_vCachedCommands[0].m_Label.c_str(), m_vCachedCommands[0].m_LabelType, m_vCachedCommands[0].m_Command.c_str());
	}
	else if(m_EditBehaviorType == 1)
	{
		std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vMovingBehavior = m_vCachedCommands;
		TargetButton->m_pBehavior = std::make_unique<CTouchControls::CBindToggleTouchButtonBehavior>(std::move(vMovingBehavior));
	}
	else if(m_EditBehaviorType == 2)
	{
		if(m_PredefinedBehaviorType != 0)
			TargetButton->m_pBehavior = GameClient()->m_TouchControls.m_BehaviorFactoriesEditor[m_PredefinedBehaviorType].m_Factory();
		else
			TargetButton->m_pBehavior = std::make_unique<CTouchControls::CExtraMenuTouchButtonBehavior>(GameClient()->m_TouchControls.m_CachedNumber);
	}
	TargetButton->UpdatePointers();
}

void CMenus::PopupConfirm_ChangeSelectedButton()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_OldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		m_UnsavedChanges = false;
		PopupCancel_ChangeSelectedButton();
	}
}

void CMenus::PopupCancel_ChangeSelectedButton()
{
	GameClient()->m_TouchControls.m_pSelectedButton = m_NewSelectedButton;
	if(m_NewSelectedButton != nullptr)
	{
		CacheAllSettingsFromTarget(m_NewSelectedButton);
		m_UnsavedChanges = false;
		GameClient()->m_TouchControls.m_pTmpButton = std::make_unique<CTouchControls::CTouchButton>(&GameClient()->m_TouchControls);
		SaveCachedSettingsToTarget(GameClient()->m_TouchControls.m_pTmpButton.get());
		GameClient()->m_TouchControls.m_ShownRect = GameClient()->m_TouchControls.m_pTmpButton->m_UnitRect;
	}
	else
	{
		CacheAllSettingsFromTarget(nullptr);
		GameClient()->m_TouchControls.m_pTmpButton = nullptr;
		GameClient()->m_TouchControls.m_ShownRect = std::nullopt;
		m_UnsavedChanges = false;
	}
	if(m_CloseMenu)
		GameClient()->m_Menus.SetActive(false);
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
	CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, 50000, 50000}, true);
	GameClient()->m_TouchControls.m_pSelectedButton = nullptr;
	GameClient()->m_TouchControls.m_pTmpButton = std::make_unique<CTouchControls::CTouchButton>(&GameClient()->m_TouchControls);
	ResetCachedSettings();
	m_InputX.Set(std::to_string(FreeRect.m_X).c_str());
	m_InputY.Set(std::to_string(FreeRect.m_Y).c_str());
	m_InputW.Set(std::to_string(FreeRect.m_W).c_str());
	m_InputH.Set(std::to_string(FreeRect.m_H).c_str());
	SaveCachedSettingsToTarget(GameClient()->m_TouchControls.m_pTmpButton.get());
	GameClient()->m_TouchControls.m_ShownRect = GameClient()->m_TouchControls.m_pTmpButton->m_UnitRect;
	m_UnsavedChanges = true;
}

bool CMenus::CheckCachedSettings()
{
	bool FatalError = false;
	char ErrorBuf[256] = "";
	int X = std::stoi(m_InputX.GetString());
	int Y = std::stoi(m_InputY.GetString());
	int W = std::stoi(m_InputW.GetString());
	int H = std::stoi(m_InputH.GetString());
	// Illegal size settings.
	if(W < 50000 || W > 500000 || H < 50000 || H > 500000)
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%sWidth and Height are required to be within the range of [50,000, 500,000].\n", ErrorBuf);
		FatalError = true;
	}
	if(X < 0 || Y < 0 || X + W > 1000000 || Y + H > 1000000)
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%sOut of bound position value.\n", ErrorBuf);
		FatalError = true;
	}
	if(GameClient()->m_TouchControls.IfOverlapping({X, Y, W, H}))
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%sThe selected button is overlapping with other buttons.\n", ErrorBuf);
		FatalError = true;
	}
	// Bind Toggle has less than two commands. This is illegal.
	if(m_EditBehaviorType == 1 && m_vCachedCommands.size() < 2)
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%sCommands in Bind Toggle has less than two command. Add more commands or use Bind behavior.\n", ErrorBuf);
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
				str_format(ErrorBuf, sizeof(ErrorBuf), "%sVisibility \"Extra Menu %d", ErrorBuf, CurrentNumber + 1);
				FirstCheck = true;
			}
			else
			{
				str_format(ErrorBuf, sizeof(ErrorBuf), "%s, %d", ErrorBuf, CurrentNumber + 1);
			}
		}
	}
	if(FirstCheck)
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%s\" has no corresponding button to get activated or deactivated.\n", ErrorBuf);
	}
	// Demo Player is used with other visibilities except Extra Menu. This is meaningless.
	if(m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DEMO_PLAYER)] == 1 && (m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::RCON_AUTHED)] != 2 || m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::INGAME)] != 2 || m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::VOTE_ACTIVE)] != 2 || m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DUMMY_CONNECTED)] != 2 || m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::DUMMY_ALLOWED)] != 2 || m_aCachedVisibilities[(int)(CTouchControls::EButtonVisibility::ZOOM_ALLOWED)] != 2))
	{
		str_format(ErrorBuf, sizeof(ErrorBuf), "%sWhen visibility \"Demo Player\" is on, visibilities except Extra Menu is meaningless.\n", ErrorBuf);
	}
	if(str_comp(ErrorBuf, "") != 0)
	{
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
	PopupConfirm("Selected button not visible", "The selected button is not visible, do you want to de-select it or edit it's visibility?", "De-select", "Edit", &CMenus::PopupConfirm_SelectedNotVisible);
}

void CMenus::PopupConfirm_SelectedNotVisible()
{
	if(m_UnsavedChanges)
	{
		// The m_pSelectedButton can't nullptr, because this function is triggered when selected button not visible.
		ChangeSelectedButtonWhileHavingUnsavedChanges(GameClient()->m_TouchControls.m_pSelectedButton, nullptr);
	}
	else
	{
		GameClient()->m_TouchControls.m_pTmpButton = nullptr;
		GameClient()->m_TouchControls.m_pSelectedButton = nullptr;
		SetActive(false);
	}
}

void CMenus::PopupConfirm_DeselectButton()
{
	if(CheckCachedSettings())
	{
		SaveCachedSettingsToTarget(m_OldSelectedButton);
		GameClient()->m_TouchControls.SetEditingChanges(true);
		PopupCancel_DeselectButton();
	}
}

void CMenus::PopupCancel_DeselectButton()
{
	GameClient()->m_TouchControls.m_pSelectedButton = nullptr;
	GameClient()->m_TouchControls.m_pTmpButton = nullptr;
	GameClient()->m_TouchControls.m_ShownRect = std::nullopt;
	m_UnsavedChanges = false;
	ResetCachedSettings();
}

void CMenus::NoSpaceForOverlappingButton()
{
	SetActive(true);
	PopupMessage("No Space", "No space left for the button. Make sure you didn't choose wrong visibilities, or edit its size.", "OK");
}

void CMenus::RenderTinyButtonTab(CUIRect MainView)
{
	MainView.Margin(10.0f, &MainView);
	MainView.VMargin((MainView.w - 150.0f) / 2.0f, &MainView);
	static CButtonContainer s_TabNewButton;
	if(DoButton_Menu(&s_TabNewButton, "New Button", 0, &MainView))
	{
		CTouchControls::CUnitRect FreeRect = GameClient()->m_TouchControls.UpdatePosition({0, 0, 50000, 50000}, true);
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
