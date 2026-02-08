/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_settings_controls.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/client/components/binds.h>
#include <game/client/components/key_binder.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <functional>
#include <string>
#include <vector>

inline constexpr float HEADER_FONT_SIZE = 16.0f;
inline constexpr float FONT_SIZE = 13.0f;
inline constexpr float MARGIN = 10.0f;
inline constexpr float BUTTON_HEIGHT = 20.0f;
inline constexpr float BUTTON_SPACING = 2.0f;
inline constexpr float BIND_OPTION_SPACING = 4.0f;

bool CBindSlotUiElement::operator<(const CBindSlotUiElement &Other) const
{
	if(m_Bind == EMPTY_BIND_SLOT)
	{
		return false;
	}
	if(Other.m_Bind == EMPTY_BIND_SLOT)
	{
		return true;
	}
	return m_Bind.m_ModifierMask < Other.m_Bind.m_ModifierMask ||
	       m_Bind.m_Key < Other.m_Bind.m_Key;
}

std::vector<CBindSlotUiElement>::iterator CBindOption::GetBindSlotElement(const CBindSlot &BindSlot)
{
	return std::find_if(m_vCurrentBinds.begin(), m_vCurrentBinds.end(), [&](const CBindSlotUiElement &BindSlotUiElement) {
		return BindSlotUiElement.m_Bind == BindSlot;
	});
}

bool CBindOption::MatchesSearch(const char *pSearch) const
{
	return (m_Group != EBindOptionGroup::CUSTOM && str_utf8_find_nocase(Localize(m_pLabel), pSearch) != nullptr) ||
	       str_utf8_find_nocase(m_Command.c_str(), pSearch) != nullptr;
}

void CMenusSettingsControls::OnInterfacesInit(CGameClient *pClient)
{
	CComponentInterfaces::OnInterfacesInit(pClient);

	m_vBindOptions = {
		{EBindOptionGroup::MOVEMENT, Localizable("Move left"), "+left"},
		{EBindOptionGroup::MOVEMENT, Localizable("Move right"), "+right"},
		{EBindOptionGroup::MOVEMENT, Localizable("Jump"), "+jump"},
		{EBindOptionGroup::MOVEMENT, Localizable("Fire"), "+fire"},
		{EBindOptionGroup::MOVEMENT, Localizable("Hook"), "+hook"},
		{EBindOptionGroup::MOVEMENT, Localizable("Hook collisions"), "+showhookcoll"},
		{EBindOptionGroup::MOVEMENT, Localizable("Pause"), "say /pause"},
		{EBindOptionGroup::MOVEMENT, Localizable("Kill"), "kill"},
		{EBindOptionGroup::MOVEMENT, Localizable("Zoom in"), "zoom+"},
		{EBindOptionGroup::MOVEMENT, Localizable("Zoom out"), "zoom-"},
		{EBindOptionGroup::MOVEMENT, Localizable("Default zoom"), "zoom"},
		{EBindOptionGroup::MOVEMENT, Localizable("Show others"), "say /showothers"},
		{EBindOptionGroup::MOVEMENT, Localizable("Show all"), "say /showall"},
		{EBindOptionGroup::MOVEMENT, Localizable("Toggle dyncam"), "toggle cl_dyncam 0 1"},
		{EBindOptionGroup::MOVEMENT, Localizable("Toggle ghost"), "toggle cl_race_show_ghost 0 1"},
		{EBindOptionGroup::WEAPON, Localizable("Hammer"), "+weapon1"},
		{EBindOptionGroup::WEAPON, Localizable("Pistol"), "+weapon2"},
		{EBindOptionGroup::WEAPON, Localizable("Shotgun"), "+weapon3"},
		{EBindOptionGroup::WEAPON, Localizable("Grenade"), "+weapon4"},
		{EBindOptionGroup::WEAPON, Localizable("Laser"), "+weapon5"},
		{EBindOptionGroup::WEAPON, Localizable("Next weapon"), "+nextweapon"},
		{EBindOptionGroup::WEAPON, Localizable("Prev. weapon"), "+prevweapon"},
		{EBindOptionGroup::VOTING, Localizable("Vote yes"), "vote yes"},
		{EBindOptionGroup::VOTING, Localizable("Vote no"), "vote no"},
		{EBindOptionGroup::CHAT, Localizable("Chat"), "+show_chat; chat all"},
		{EBindOptionGroup::CHAT, Localizable("Team chat"), "+show_chat; chat team"},
		{EBindOptionGroup::CHAT, Localizable("Converse"), "+show_chat; chat all /c "},
		{EBindOptionGroup::CHAT, Localizable("Chat command"), "+show_chat; chat all /"},
		{EBindOptionGroup::CHAT, Localizable("Show chat"), "+show_chat"},
		{EBindOptionGroup::DUMMY, Localizable("Toggle dummy"), "toggle cl_dummy 0 1"},
		{EBindOptionGroup::DUMMY, Localizable("Dummy copy"), "toggle cl_dummy_copy_moves 0 1"},
		{EBindOptionGroup::DUMMY, Localizable("Hammerfly dummy"), "toggle cl_dummy_hammer 0 1"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Emoticon"), "+emote"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Spectator mode"), "+spectate"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Spectate next"), "spectate_next"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Spectate previous"), "spectate_previous"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Console"), "toggle_local_console"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Remote console"), "toggle_remote_console"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Screenshot"), "screenshot"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Scoreboard"), "+scoreboard"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Scoreboard cursor"), "toggle_scoreboard_cursor"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Statboard"), "+statboard"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Lock team"), "say /lock"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Show entities"), "toggle cl_overlay_entities 0 100"},
		{EBindOptionGroup::MISCELLANEOUS, Localizable("Show HUD"), "toggle cl_showhud 0 1"},
	};
	m_NumPredefinedBindOptions = m_vBindOptions.size();

	std::fill(std::begin(m_aBindGroupExpanded), std::end(m_aBindGroupExpanded), true);
	m_aBindGroupExpanded[(int)EBindOptionGroup::CUSTOM] = false;

	m_JoystickDropDownState.m_SelectionPopupContext.m_pScrollRegion = &m_JoystickDropDownScrollRegion;
}

void CMenusSettingsControls::Render(CUIRect MainView)
{
	UpdateBindOptions();

	CUIRect QuickSearch, SearchMatches, ResetToDefault;
	MainView.HSplitBottom(BUTTON_HEIGHT, &MainView, &QuickSearch);
	QuickSearch.VSplitRight(200.0f, &QuickSearch, &ResetToDefault);
	QuickSearch.VSplitRight(MARGIN, &QuickSearch, nullptr);
	QuickSearch.VSplitRight(150.0f, &QuickSearch, &SearchMatches);
	QuickSearch.VSplitRight(MARGIN, &QuickSearch, nullptr);
	MainView.HSplitBottom(MARGIN, &MainView, nullptr);

	// Quick search
	if(Ui()->DoEditBox_Search(&m_FilterInput, &QuickSearch, FONT_SIZE, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && !GameClient()->m_KeyBinder.IsActive()))
	{
		m_CurrentSearchMatch = 0;
		UpdateSearchMatches();
		m_SearchMatchReveal = true;
	}
	else if(!m_vSearchMatches.empty() && (Ui()->ConsumeHotkey(CUi::EHotkey::HOTKEY_ENTER) || Ui()->ConsumeHotkey(CUi::EHotkey::HOTKEY_TAB)))
	{
		UpdateSearchMatches();
		m_CurrentSearchMatch += Input()->ShiftIsPressed() ? -1 : 1;
		if(m_CurrentSearchMatch >= (int)m_vSearchMatches.size())
		{
			m_CurrentSearchMatch = 0;
		}
		if(m_CurrentSearchMatch < 0)
		{
			m_CurrentSearchMatch = m_vSearchMatches.size() - 1;
		}
		m_SearchMatchReveal = true;
	}

	if(!m_FilterInput.IsEmpty())
	{
		if(!m_vSearchMatches.empty())
		{
			char aSearchMatchLabel[64];
			str_format(aSearchMatchLabel, sizeof(aSearchMatchLabel), Localize("Match %d of %d"), m_CurrentSearchMatch + 1, (int)m_vSearchMatches.size());
			Ui()->DoLabel(&SearchMatches, aSearchMatchLabel, FONT_SIZE, TEXTALIGN_MC);
		}
		else
		{
			Ui()->DoLabel(&SearchMatches, Localize("No results"), FONT_SIZE, TEXTALIGN_MC);
		}
	}

	// Reset to default button
	if(GameClient()->m_Menus.DoButton_Menu(&m_ResetToDefaultButton, Localize("Reset to defaults"), 0, &ResetToDefault))
	{
		GameClient()->m_Menus.PopupConfirm(Localize("Reset controls"), Localize("Are you sure that you want to reset the controls to their defaults?"),
			Localize("Reset"), Localize("Cancel"), &CMenus::ResetSettingsControls);
	}

	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 6.0f * BUTTON_HEIGHT;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	m_SettingsScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.y += ScrollOffset.y;

	CUIRect LeftColumn, RightColumn;
	MainView.VSplitMid(&LeftColumn, &RightColumn, MARGIN);

	// Left column
	RenderSettingsBlock(MeasureSettingsMouseHeight(), &LeftColumn,
		Localize("Mouse"), nullptr, nullptr, std::bind_front(&CMenusSettingsControls::RenderSettingsMouse, this));
	RenderSettingsBlock(MeasureSettingsJoystickHeight(), &LeftColumn,
		Localize("Controller"), nullptr, nullptr, std::bind_front(&CMenusSettingsControls::RenderSettingsJoystick, this));
	RenderSettingsBindsBlock(EBindOptionGroup::MOVEMENT, &LeftColumn, Localize("Movement"));
	RenderSettingsBindsBlock(EBindOptionGroup::WEAPON, &LeftColumn, Localize("Weapon"));

	// Right column
	RenderSettingsBindsBlock(EBindOptionGroup::VOTING, &RightColumn, Localize("Voting"));
	RenderSettingsBindsBlock(EBindOptionGroup::CHAT, &RightColumn, Localize("Chat"));
	RenderSettingsBindsBlock(EBindOptionGroup::DUMMY, &RightColumn, Localize("Dummy"));
	RenderSettingsBindsBlock(EBindOptionGroup::MISCELLANEOUS, &RightColumn, Localize("Miscellaneous"));
	if(std::any_of(m_vBindOptions.begin(), m_vBindOptions.end(), [](const CBindOption &Option) { return Option.m_Group == EBindOptionGroup::CUSTOM; }))
	{
		RenderSettingsBindsBlock(EBindOptionGroup::CUSTOM, &RightColumn, Localize("Custom"));
	}

	m_SettingsScrollRegion.End();
}

void CMenusSettingsControls::UpdateBindOptions()
{
	for(CBindOption &Option : m_vBindOptions)
	{
		for(CBindSlotUiElement &BindSlot : Option.m_vCurrentBinds)
		{
			if(BindSlot.m_Bind != EMPTY_BIND_SLOT)
			{
				BindSlot.m_ToBeDeleted = true;
			}
		}
	}

	for(int Mod = KeyModifier::NONE; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = KEY_FIRST; KeyId < KEY_LAST; KeyId++)
		{
			const CBindSlot BindSlot = CBindSlot(KeyId, Mod);
			const char *pBind = GameClient()->m_Binds.Get(BindSlot);
			if(!pBind[0])
			{
				continue;
			}

			auto ExistingOption = std::find_if(m_vBindOptions.begin(), m_vBindOptions.end(), [pBind](const CBindOption &Option) {
				return str_comp(pBind, Option.m_Command.c_str()) == 0;
			});
			if(ExistingOption == m_vBindOptions.end())
			{
				// Bind option not found for command, add custom bind option.
				CBindOption NewOption = {EBindOptionGroup::CUSTOM, nullptr, pBind};
				ExistingOption = m_vBindOptions.insert(
					std::upper_bound(m_vBindOptions.begin() + m_NumPredefinedBindOptions, m_vBindOptions.end(), NewOption, [&](const CBindOption &Option1, const CBindOption &Option2) {
						return str_utf8_comp_nocase(Option1.m_Command.c_str(), Option2.m_Command.c_str()) < 0;
					}),
					NewOption);

				// Update search matches due to new option being added.
				if(!m_FilterInput.IsEmpty())
				{
					const int OptionIndex = ExistingOption - m_vBindOptions.begin();
					for(int &SearchMatch : m_vSearchMatches)
					{
						if(OptionIndex <= SearchMatch)
						{
							++SearchMatch;
						}
					}
					if(ExistingOption->MatchesSearch(m_FilterInput.GetString()))
					{
						const int MatchIndex = m_vSearchMatches.insert(std::upper_bound(m_vSearchMatches.begin(), m_vSearchMatches.end(), OptionIndex), OptionIndex) - m_vSearchMatches.begin();
						if(MatchIndex <= m_CurrentSearchMatch)
						{
							++m_CurrentSearchMatch;
						}
					}
				}
			}
			auto ExistingBindSlot = ExistingOption->GetBindSlotElement(BindSlot);
			if(ExistingBindSlot == ExistingOption->m_vCurrentBinds.end())
			{
				// Remove empty bind slot if one is present because it will be replaced with a bind slot for the new bind.
				auto ExistingEmptyBindSlot = ExistingOption->GetBindSlotElement(EMPTY_BIND_SLOT);
				if(ExistingEmptyBindSlot != ExistingOption->m_vCurrentBinds.end())
				{
					ExistingOption->m_vCurrentBinds.erase(ExistingEmptyBindSlot);
				}

				CBindSlotUiElement BindSlotUiElement = {BindSlot};
				ExistingOption->m_vCurrentBinds.insert(
					std::upper_bound(ExistingOption->m_vCurrentBinds.begin(), ExistingOption->m_vCurrentBinds.end(), BindSlotUiElement),
					BindSlotUiElement);
			}
			else
			{
				ExistingBindSlot->m_ToBeDeleted = false;
			}
		}
	}

	// Remove bind slots that are not bound anymore,
	// mark unused custom bind options for removal.
	for(CBindOption &Option : m_vBindOptions)
	{
		Option.m_vCurrentBinds.erase(std::remove_if(Option.m_vCurrentBinds.begin(), Option.m_vCurrentBinds.end(),
						     [&](const CBindSlotUiElement &BindSlotUiElement) { return BindSlotUiElement.m_ToBeDeleted; }),
			Option.m_vCurrentBinds.end());

		Option.m_ToBeDeleted = Option.m_vCurrentBinds.empty() && Option.m_Group == EBindOptionGroup::CUSTOM;
		if(Option.m_ToBeDeleted)
		{
			continue;
		}

		if(Option.m_vCurrentBinds.empty() ||
			(Option.m_AddNewBind && Option.GetBindSlotElement(EMPTY_BIND_SLOT) == Option.m_vCurrentBinds.end()))
		{
			Option.m_vCurrentBinds.emplace_back(EMPTY_BIND_SLOT);
		}
	}

	// Update search matches when removing bind options.
	for(const CBindOption &Option : m_vBindOptions)
	{
		if(!Option.m_ToBeDeleted)
		{
			continue;
		}
		const int OptionIndex = &Option - m_vBindOptions.data();
		auto ExactSearchMatch = std::find(m_vSearchMatches.begin(), m_vSearchMatches.end(), OptionIndex);
		if(ExactSearchMatch != m_vSearchMatches.end())
		{
			m_vSearchMatches.erase(ExactSearchMatch);
			if((int)(ExactSearchMatch - m_vSearchMatches.begin()) < m_CurrentSearchMatch)
			{
				--m_CurrentSearchMatch;
			}
		}
		for(int &SearchMatch : m_vSearchMatches)
		{
			if(OptionIndex < SearchMatch)
			{
				--SearchMatch;
			}
		}
	}
	if(m_vSearchMatches.empty())
	{
		m_CurrentSearchMatch = 0;
	}
	else if(m_CurrentSearchMatch >= (int)m_vSearchMatches.size())
	{
		m_CurrentSearchMatch = m_vSearchMatches.size() - 1;
	}

	// Remove unused bind options.
	m_vBindOptions.erase(std::remove_if(m_vBindOptions.begin() + m_NumPredefinedBindOptions, m_vBindOptions.end(),
				     [&](const CBindOption &Option) { return Option.m_ToBeDeleted; }),
		m_vBindOptions.end());
}

void CMenusSettingsControls::UpdateSearchMatches()
{
	m_vSearchMatches.clear();

	if(!m_FilterInput.IsEmpty())
	{
		for(CBindOption &Option : m_vBindOptions)
		{
			if(!Option.MatchesSearch(m_FilterInput.GetString()))
			{
				continue;
			}

			m_aBindGroupExpanded[(int)Option.m_Group] = true;
			m_vSearchMatches.emplace_back(&Option - m_vBindOptions.data());
		}
	}

	if(m_vSearchMatches.empty())
	{
		m_CurrentSearchMatch = 0;
	}
	else if(m_CurrentSearchMatch >= (int)m_vSearchMatches.size())
	{
		m_CurrentSearchMatch = m_vSearchMatches.size() - 1;
	}
}

void CMenusSettingsControls::RenderSettingsBlock(float Height, CUIRect *pParentRect, const char *pTitle,
	bool *pExpanded, CButtonContainer *pExpandButton, const std::function<void(CUIRect Rect)> &RenderContentFunction)
{
	const bool WasExpanded = pExpanded == nullptr || *pExpanded;
	float FullHeight = WasExpanded ? Height : 0.0f; // Content
	FullHeight += pTitle == nullptr ? 0.0f : HEADER_FONT_SIZE + (WasExpanded ? MARGIN : 0.0f); // Title and spacing
	FullHeight += 2.0f * MARGIN; // Margin

	CUIRect SettingsBlock;
	pParentRect->HSplitTop(FullHeight, &SettingsBlock, pParentRect);
	pParentRect->HSplitTop(MARGIN, nullptr, pParentRect);
	if(m_SettingsScrollRegion.AddRect(SettingsBlock) || m_SearchMatchReveal)
	{
		SettingsBlock.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, pExpandButton == nullptr || Ui()->HotItem() != pExpandButton ? 0.25f : 0.3f), IGraphics::CORNER_ALL, 10.0f);
		SettingsBlock.Margin(MARGIN, &SettingsBlock);

		if(pTitle != nullptr)
		{
			CUIRect Label;
			SettingsBlock.HSplitTop(HEADER_FONT_SIZE, &Label, &SettingsBlock);
			if(WasExpanded)
			{
				SettingsBlock.HSplitTop(MARGIN, nullptr, &SettingsBlock);
			}

			if(pExpanded != nullptr)
			{
				CUIRect ButtonArea;
				Label.Margin(-MARGIN, &ButtonArea);
				if(Ui()->DoButtonLogic(pExpandButton, 0, &ButtonArea, BUTTONFLAG_LEFT))
				{
					*pExpanded = !*pExpanded;
				}

				CUIRect ExpandButton;
				Label.VSplitRight(20.0f, &Label, &ExpandButton);
				Label.VSplitRight(BUTTON_SPACING, &Label, nullptr);
				if(m_SettingsScrollRegion.AddRect(ExpandButton))
				{
					SLabelProperties Props;
					Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.65f * Ui()->ButtonColorMul(pExpandButton)));
					Props.m_EnableWidthCheck = false;
					TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
					Ui()->DoLabel(&ExpandButton, *pExpanded ? FontIcon::CHEVRON_UP : FontIcon::CHEVRON_DOWN, HEADER_FONT_SIZE, TEXTALIGN_MR, Props);
					TextRender()->SetRenderFlags(0);
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
			}

			if(m_SettingsScrollRegion.AddRect(Label))
			{
				Ui()->DoLabel(&Label, pTitle, HEADER_FONT_SIZE, TEXTALIGN_ML);
			}
		}

		if(WasExpanded)
		{
			RenderContentFunction(SettingsBlock);
		}
	}
}

void CMenusSettingsControls::RenderSettingsBindsBlock(EBindOptionGroup Group, CUIRect *pParentRect, const char *pTitle)
{
	RenderSettingsBlock(MeasureSettingsBindsHeight(Group), pParentRect, pTitle,
		&m_aBindGroupExpanded[(int)Group], &m_aBindGroupExpandButtons[(int)Group],
		[&](CUIRect Rect) { RenderSettingsBinds(Group, Rect); });
}

float CMenusSettingsControls::MeasureSettingsBindsHeight(EBindOptionGroup Group) const
{
	float Height = 0.0f;
	for(const CBindOption &BindOption : m_vBindOptions)
	{
		if(BindOption.m_Group != Group)
		{
			continue;
		}
		if(Height > 0.0f)
		{
			Height += BIND_OPTION_SPACING;
		}
		Height += BUTTON_HEIGHT * BindOption.m_vCurrentBinds.size() + BUTTON_SPACING * (BindOption.m_vCurrentBinds.size() - 1) + BIND_OPTION_SPACING;
	}
	return Height;
}

void CMenusSettingsControls::RenderSettingsBinds(EBindOptionGroup Group, CUIRect View)
{
	for(CBindOption &BindOption : m_vBindOptions)
	{
		if(BindOption.m_Group != Group)
		{
			continue;
		}

		CUIRect KeyReaders;
		View.HSplitTop(BUTTON_HEIGHT * BindOption.m_vCurrentBinds.size() + BUTTON_SPACING * (BindOption.m_vCurrentBinds.size() - 1) + 4.0f, &KeyReaders, &View);
		View.HSplitTop(BIND_OPTION_SPACING, nullptr, &View);
		if(!m_SettingsScrollRegion.AddRect(KeyReaders) && !m_SearchMatchReveal)
		{
			continue;
		}
		KeyReaders.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f), IGraphics::CORNER_ALL, 5.0f);
		KeyReaders.Margin(2.0f, &KeyReaders);

		CUIRect Label, AddButton;
		KeyReaders.VSplitLeft(KeyReaders.w / 3.0f, &Label, &KeyReaders);
		KeyReaders.VSplitLeft(5.0f, nullptr, &KeyReaders);
		KeyReaders.VSplitLeft(BUTTON_HEIGHT, &AddButton, &KeyReaders);
		AddButton.HSplitTop(BUTTON_HEIGHT, &AddButton, nullptr);
		KeyReaders.VSplitLeft(2.0f, nullptr, &KeyReaders);
		Label.HSplitTop(BUTTON_HEIGHT, &Label, nullptr);

		const auto SearchMatch = std::find(m_vSearchMatches.begin(), m_vSearchMatches.end(), &BindOption - m_vBindOptions.data());
		const bool SearchMatchSelected = SearchMatch != m_vSearchMatches.end() && m_CurrentSearchMatch == (int)(SearchMatch - m_vSearchMatches.begin());
		if(SearchMatchSelected && m_SearchMatchReveal)
		{
			m_SearchMatchReveal = false;
			// Scroll to reveal search match
			CUIRect ScrollTarget;
			Label.HMargin(-MARGIN, &ScrollTarget);
			m_SettingsScrollRegion.AddRect(ScrollTarget, true);
		}
		SLabelProperties LabelProps = {.m_MaxWidth = Label.w, .m_EllipsisAtEnd = BindOption.m_Group == EBindOptionGroup::CUSTOM, .m_MinimumFontSize = 9.0f};
		if(SearchMatchSelected)
		{
			LabelProps.SetColor(ColorRGBA(0.1f, 0.1f, 1.0f, 1.0f));
		}
		else if(SearchMatch != m_vSearchMatches.end())
		{
			LabelProps.SetColor(ColorRGBA(0.4f, 0.4f, 0.9f, 1.0f));
		}
		const CLabelResult LabelResult = Ui()->DoLabel(&Label, BindOption.m_Group == EBindOptionGroup::CUSTOM ? BindOption.m_Command.c_str() : Localize(BindOption.m_pLabel),
			FONT_SIZE, TEXTALIGN_ML, LabelProps);
		if(BindOption.m_Group != EBindOptionGroup::CUSTOM || LabelResult.m_Truncated)
		{
			Ui()->DoButtonLogic(&BindOption.m_TooltipButtonId, 0, &Label, BUTTONFLAG_NONE);
			GameClient()->m_Tooltips.DoToolTip(&BindOption.m_TooltipButtonId, &Label, BindOption.m_Command.c_str());
		}

		for(CBindSlotUiElement &CurrentBind : BindOption.m_vCurrentBinds)
		{
			CUIRect KeyReader;
			KeyReaders.HSplitTop(BUTTON_HEIGHT, &KeyReader, &KeyReaders);
			KeyReaders.HSplitTop(BUTTON_SPACING, nullptr, &KeyReaders);
			const bool ActivateKeyReader = BindOption.m_AddNewBindActivate && CurrentBind.m_Bind == EMPTY_BIND_SLOT;
			const CKeyBinder::CKeyReaderResult KeyReaderResult = GameClient()->m_KeyBinder.DoKeyReader(
				&CurrentBind.m_KeyReaderButton, &CurrentBind.m_KeyResetButton,
				&KeyReader, CurrentBind.m_Bind, ActivateKeyReader);
			if(ActivateKeyReader)
			{
				BindOption.m_AddNewBindActivate = false;
				// Scroll to reveal activated key reader
				CUIRect ScrollTarget;
				KeyReader.HMargin(-MARGIN, &ScrollTarget);
				m_SettingsScrollRegion.AddRect(ScrollTarget, true);
			}
			if(KeyReaderResult.m_Aborted)
			{
				BindOption.m_AddNewBind = false;
				if(CurrentBind.m_Bind == EMPTY_BIND_SLOT && (&CurrentBind - BindOption.m_vCurrentBinds.data()) > 0)
				{
					CurrentBind.m_ToBeDeleted = true;
				}
			}
			else if(KeyReaderResult.m_Bind != CurrentBind.m_Bind)
			{
				BindOption.m_AddNewBind = false;
				if(CurrentBind.m_Bind.m_Key != KEY_UNKNOWN || KeyReaderResult.m_Bind.m_Key == KEY_UNKNOWN)
				{
					GameClient()->m_Binds.Bind(CurrentBind.m_Bind.m_Key, "", false, CurrentBind.m_Bind.m_ModifierMask);
				}
				if(KeyReaderResult.m_Bind.m_Key != KEY_UNKNOWN)
				{
					GameClient()->m_Binds.Bind(KeyReaderResult.m_Bind.m_Key, BindOption.m_Command.c_str(), false, KeyReaderResult.m_Bind.m_ModifierMask);
				}
			}
		}

		if(Ui()->DoButton_FontIcon(&BindOption.m_AddBindButtonContainer, FontIcon::PLUS, BindOption.m_AddNewBind ? 1 : 0, &AddButton, BUTTONFLAG_LEFT))
		{
			BindOption.m_AddNewBind = true;
			BindOption.m_AddNewBindActivate = true;
		}
	}
}

float CMenusSettingsControls::MeasureSettingsMouseHeight() const
{
	return 2.0f * BUTTON_HEIGHT + BUTTON_SPACING;
}

void CMenusSettingsControls::RenderSettingsMouse(CUIRect View)
{
	CUIRect Button;
	View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
	Ui()->DoScrollbarOption(&g_Config.m_InpMousesens, &g_Config.m_InpMousesens, &Button, Localize("Ingame mouse sens."), 1, 500,
		&CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

	View.HSplitTop(BUTTON_SPACING, nullptr, &View);

	View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
	Ui()->DoScrollbarOption(&g_Config.m_UiMousesens, &g_Config.m_UiMousesens, &Button, Localize("UI mouse sens."), 1, 500,
		&CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE);
}

float CMenusSettingsControls::MeasureSettingsJoystickHeight() const
{
	int NumOptions = 1; // expandable header
	if(g_Config.m_InpControllerEnable)
	{
		NumOptions++; // message or joystick name/selection
		if(Input()->NumJoysticks() > 0)
		{
			NumOptions += 3; // mode, ui sens, tolerance
			if(!g_Config.m_InpControllerAbsolute)
				NumOptions++; // ingame sens
			NumOptions += Input()->GetActiveJoystick()->GetNumAxes() + 1; // axis selection + header
		}
	}
	return NumOptions * (BUTTON_HEIGHT + BUTTON_SPACING) + (NumOptions == 1 ? 0.0f : BUTTON_SPACING);
}

void CMenusSettingsControls::RenderSettingsJoystick(CUIRect View)
{
	CUIRect Button;
	View.HSplitTop(BUTTON_SPACING, nullptr, &View);
	View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
	const bool WasJoystickEnabled = g_Config.m_InpControllerEnable;
	if(GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_InpControllerEnable, Localize("Enable controller"), g_Config.m_InpControllerEnable, &Button))
	{
		g_Config.m_InpControllerEnable ^= 1;
	}
	if(!WasJoystickEnabled) // Use old value because this was used to allocate the available height
	{
		return;
	}

	const int NumJoysticks = Input()->NumJoysticks();
	if(NumJoysticks > 0)
	{
		// show joystick device selection if more than one available or just the joystick name if there is only one
		{
			CUIRect JoystickDropDown;
			View.HSplitTop(BUTTON_SPACING, nullptr, &View);
			View.HSplitTop(BUTTON_HEIGHT, &JoystickDropDown, &View);
			if(NumJoysticks > 1)
			{
				std::vector<std::string> vJoystickNames;
				std::vector<const char *> vpJoystickNames;
				vJoystickNames.resize(NumJoysticks);
				vpJoystickNames.resize(NumJoysticks);

				for(int i = 0; i < NumJoysticks; ++i)
				{
					char aJoystickName[256];
					str_format(aJoystickName, sizeof(aJoystickName), "%s %d: %s", Localize("Controller"), i, Input()->GetJoystick(i)->GetName());
					vJoystickNames[i] = aJoystickName;
					vpJoystickNames[i] = vJoystickNames[i].c_str();
				}

				const int CurrentJoystick = Input()->GetActiveJoystick()->GetIndex();
				const int NewJoystick = Ui()->DoDropDown(&JoystickDropDown, CurrentJoystick, vpJoystickNames.data(), vpJoystickNames.size(), m_JoystickDropDownState);
				if(NewJoystick != CurrentJoystick)
				{
					Input()->SetActiveJoystick(NewJoystick);
				}
			}
			else
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s 0: %s", Localize("Controller"), Input()->GetJoystick(0)->GetName());
				Ui()->DoLabel(&JoystickDropDown, aBuf, FONT_SIZE, TEXTALIGN_ML);
			}
		}

		const bool WasAbsolute = g_Config.m_InpControllerAbsolute;
		GameClient()->m_Menus.DoLine_RadioMenu(View, Localize("Ingame controller mode"),
			m_vJoystickIngameModeButtonContainers,
			{Localize("Relative", "Ingame controller mode"), Localize("Absolute", "Ingame controller mode")},
			{0, 1},
			g_Config.m_InpControllerAbsolute);

		if(!WasAbsolute) // Use old value because this was used to allocate the available height
		{
			View.HSplitTop(BUTTON_SPACING, nullptr, &View);
			View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
			Ui()->DoScrollbarOption(&g_Config.m_InpControllerSens, &g_Config.m_InpControllerSens, &Button, Localize("Ingame controller sens."), 1, 500,
				&CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);
		}

		View.HSplitTop(BUTTON_SPACING, nullptr, &View);
		View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
		Ui()->DoScrollbarOption(&g_Config.m_UiControllerSens, &g_Config.m_UiControllerSens, &Button, Localize("UI controller sens."), 1, 500,
			&CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

		View.HSplitTop(BUTTON_SPACING, nullptr, &View);
		View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
		Ui()->DoScrollbarOption(&g_Config.m_InpControllerTolerance, &g_Config.m_InpControllerTolerance, &Button, Localize("Controller jitter tolerance"), 0, 50);

		View.HSplitTop(BUTTON_SPACING, nullptr, &View);
		if(m_SettingsScrollRegion.AddRect(View))
		{
			View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f), IGraphics::CORNER_ALL, 5.0f);
			RenderJoystickAxisPicker(View);
		}
	}
	else
	{
		View.HSplitTop(View.h - BUTTON_HEIGHT, nullptr, &View);
		View.HSplitTop(BUTTON_HEIGHT, &Button, &View);
		Ui()->DoLabel(&Button, Localize("No controller found. Plug in a controller."), FONT_SIZE, TEXTALIGN_ML);
	}
}

void CMenusSettingsControls::RenderJoystickAxisPicker(CUIRect View)
{
	const float AxisWidth = 0.2f * View.w;
	const float StatusWidth = 0.4f * View.w;
	const float AimBindWidth = 90.0f;
	const float SpacingV = (View.w - AxisWidth - StatusWidth - AimBindWidth) / 2.0f;

	CUIRect Row, Axis, Status, AimBind;
	View.HSplitTop(BUTTON_SPACING, nullptr, &View);
	View.HSplitTop(BUTTON_HEIGHT, &Row, &View);
	Row.VSplitLeft(AxisWidth, &Axis, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(StatusWidth, &Status, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

	Ui()->DoLabel(&Axis, Localize("Axis"), FONT_SIZE, TEXTALIGN_MC);
	Ui()->DoLabel(&Status, Localize("Status"), FONT_SIZE, TEXTALIGN_MC);
	Ui()->DoLabel(&AimBind, Localize("Aim bind"), FONT_SIZE, TEXTALIGN_MC);

	IInput::IJoystick *pJoystick = Input()->GetActiveJoystick();
	for(int i = 0; i < std::min<int>(pJoystick->GetNumAxes(), NUM_JOYSTICK_AXES); i++)
	{
		View.HSplitTop(BUTTON_SPACING, nullptr, &View);
		View.HSplitTop(BUTTON_HEIGHT, &Row, &View);
		if(!m_SettingsScrollRegion.AddRect(Row))
		{
			continue;
		}
		Row.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f), IGraphics::CORNER_ALL, 5.0f);
		Row.VSplitLeft(AxisWidth, &Axis, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(StatusWidth, &Status, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

		const bool Active = g_Config.m_InpControllerX == i || g_Config.m_InpControllerY == i;

		// Axis label
		char aLabel[16];
		str_format(aLabel, sizeof(aLabel), "%d", i + 1);
		SLabelProperties LabelProps;
		if(!Active)
		{
			LabelProps.SetColor(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
		}
		Ui()->DoLabel(&Axis, aLabel, FONT_SIZE, TEXTALIGN_MC, LabelProps);

		// Axis status
		Status.HMargin(7.0f, &Status);
		RenderJoystickBar(&Status, (pJoystick->GetAxisValue(i) + 1.0f) / 2.0f, g_Config.m_InpControllerTolerance / 50.0f, Active);

		// Bind to X/Y
		CUIRect AimBindX, AimBindY;
		AimBind.VSplitMid(&AimBindX, &AimBindY);
		if(GameClient()->m_Menus.DoButton_CheckBox(&m_aaJoystickAxisCheckboxIds[i][0], "X", g_Config.m_InpControllerX == i, &AimBindX))
		{
			if(g_Config.m_InpControllerY == i)
				g_Config.m_InpControllerY = g_Config.m_InpControllerX;
			g_Config.m_InpControllerX = i;
		}
		if(GameClient()->m_Menus.DoButton_CheckBox(&m_aaJoystickAxisCheckboxIds[i][1], "Y", g_Config.m_InpControllerY == i, &AimBindY))
		{
			if(g_Config.m_InpControllerX == i)
				g_Config.m_InpControllerX = g_Config.m_InpControllerY;
			g_Config.m_InpControllerY = i;
		}
	}
}

void CMenusSettingsControls::RenderJoystickBar(const CUIRect *pRect, float Current, float Tolerance, bool Active)
{
	CUIRect Handle;
	pRect->VSplitLeft(pRect->h, &Handle, nullptr); // Slider size
	Handle.x += (pRect->w - Handle.w) * Current;

	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Active ? 0.25f : 0.125f), IGraphics::CORNER_ALL, pRect->h / 2.0f);

	CUIRect ToleranceArea = *pRect;
	ToleranceArea.w *= Tolerance;
	ToleranceArea.x += (pRect->w - ToleranceArea.w) / 2.0f;
	const ColorRGBA ToleranceColor = Active ? ColorRGBA(0.8f, 0.35f, 0.35f, 1.0f) : ColorRGBA(0.7f, 0.5f, 0.5f, 1.0f);
	ToleranceArea.Draw(ToleranceColor, IGraphics::CORNER_ALL, ToleranceArea.h / 2.0f);

	const ColorRGBA SliderColor = Active ? ColorRGBA(0.95f, 0.95f, 0.95f, 1.0f) : ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
	Handle.Draw(SliderColor, IGraphics::CORNER_ALL, Handle.h / 2.0f);
}

void CMenus::ResetSettingsControls()
{
	GameClient()->m_Binds.SetDefaults();

	g_Config.m_InpMousesens = 200;
	g_Config.m_UiMousesens = 200;

	g_Config.m_InpControllerEnable = 0;
	g_Config.m_InpControllerGUID[0] = '\0';
	g_Config.m_InpControllerAbsolute = 0;
	g_Config.m_InpControllerSens = 100;
	g_Config.m_InpControllerX = 0;
	g_Config.m_InpControllerY = 1;
	g_Config.m_InpControllerTolerance = 5;
	g_Config.m_UiControllerSens = 100;
}
