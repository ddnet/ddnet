/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/console.h>
#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <optional>
#include <vector>

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	CUIRect TabBar, PlayerTab, DummyTab, ChangeInfo;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	TabBar.VSplitMid(&TabBar, &ChangeInfo, 20.f);
	TabBar.VSplitMid(&PlayerTab, &DummyTab);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	static CButtonContainer s_PlayerTabButton;
	if(DoButton_MenuTab(&s_PlayerTabButton, Localize("Player"), !m_Dummy, &PlayerTab, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = false;
	}

	static CButtonContainer s_DummyTabButton;
	if(DoButton_MenuTab(&s_DummyTabButton, Localize("Dummy"), m_Dummy, &DummyTab, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = true;
	}

	if(Client()->State() == IClient::STATE_ONLINE &&
		GameClient()->m_aNextChangeInfo[m_Dummy] > Client()->GameTick(m_Dummy))
	{
		char aChangeInfo[128], aTimeLeft[32];
		str_format(aTimeLeft, sizeof(aTimeLeft), Localize("%ds left"), (GameClient()->m_aNextChangeInfo[m_Dummy] - Client()->GameTick(m_Dummy) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
		str_format(aChangeInfo, sizeof(aChangeInfo), "%s: %s", Localize("Player info change cooldown"), aTimeLeft);
		Ui()->DoLabel(&ChangeInfo, aChangeInfo, 10.f, TEXTALIGN_ML);
	}

	static CLineInput s_NameInput;
	static CLineInput s_ClanInput;

	int *pCountry;
	if(!m_Dummy)
	{
		pCountry = &g_Config.m_PlayerCountry;
		s_NameInput.SetBuffer(g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName));
		s_NameInput.SetEmptyText(Client()->PlayerName());
		s_ClanInput.SetBuffer(g_Config.m_PlayerClan, sizeof(g_Config.m_PlayerClan));
	}
	else
	{
		pCountry = &g_Config.m_ClDummyCountry;
		s_NameInput.SetBuffer(g_Config.m_ClDummyName, sizeof(g_Config.m_ClDummyName));
		s_NameInput.SetEmptyText(Client()->DummyName());
		s_ClanInput.SetBuffer(g_Config.m_ClDummyClan, sizeof(g_Config.m_ClDummyClan));
	}

	CUIRect InfoRow, InfoLeft, InfoRight;
	MainView.HSplitTop(50.0f, &InfoRow, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	// name/clan field area
	InfoRow.VSplitLeft(300.0f, &InfoLeft, &InfoRight);
	InfoLeft.VSplitRight(20.0f, &InfoLeft, nullptr);

	CUIRect Button, Label;
	char aBuf[128];

	// player name
	InfoLeft.HSplitTop(20.0f, &Button, &InfoLeft);
	InfoLeft.HSplitTop(5.0f, nullptr, &InfoLeft);
	Button.VSplitLeft(60.0f, &Label, &Button);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_NameInput, &Button, 14.0f))
	{
		SetNeedSendInfo();
	}

	// player clan
	InfoLeft.HSplitTop(20.0f, &Button, &InfoLeft);
	Button.VSplitLeft(60.0f, &Label, &Button);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_ClanInput, &Button, 14.0f))
	{
		SetNeedSendInfo();
	}

	// country flag button (click opens a popup)
	InfoRight.VSplitLeft(60.0f, &Label, &InfoRight);
	Ui()->DoLabel(&Label, Localize("Flag:"), 14.0f, TEXTALIGN_ML);

	// Flag button on the left; debug skin-loading stats fill the rest.
	CUIRect FlagButton, DebugLabel;
	InfoRight.VSplitLeft(120.0f, &FlagButton, &DebugLabel);
	FlagButton.HMargin((FlagButton.h - 30.0f) / 2.0f, &FlagButton);

	DebugLabel.VSplitLeft(20.0f, nullptr, &DebugLabel);
	RenderSkinLoadingStatsDebug(DebugLabel);

	FlagButton.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	if(Ui()->MouseHovered(&FlagButton))
	{
		FlagButton.Draw(ColorRGBA(0.5f, 0.5f, 0.5f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	}

	const CCountryFlags::CCountryFlag *pCurrentFlag = nullptr;
	for(size_t i = 0; i < GameClient()->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag &Entry = GameClient()->m_CountryFlags.GetByIndex(i);
		if(Entry.m_CountryCode == *pCountry)
		{
			pCurrentFlag = &Entry;
			break;
		}
	}

	CUIRect FlagImage, FlagCode;
	// make sure 2:1 flag fits
	FlagButton.VSplitLeft(52.0f, &FlagImage, &FlagCode);
	FlagImage.Margin(4.0f, &FlagImage);
	if(pCurrentFlag != nullptr && (pCurrentFlag->m_Texture.IsValid() || pCurrentFlag->m_CountryCode == CountryCode::DEFAULT))
	{
		GameClient()->m_CountryFlags.Render(pCurrentFlag->m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagImage.x, FlagImage.y, FlagImage.w, FlagImage.h);
	}
	if(pCurrentFlag != nullptr)
	{
		FlagCode.VMargin(5.0f, &FlagCode);
		Ui()->DoLabel(&FlagCode, pCurrentFlag->m_aCountryCodeString, 14.0f, TEXTALIGN_ML);
	}

	static CButtonContainer s_FlagButton;
	if(Ui()->DoButtonLogic(&s_FlagButton, 0, &FlagButton, BUTTONFLAG_LEFT))
	{
		m_PopupCountrySelection = *pCountry;
		m_Popup = POPUP_COUNTRY;
	}

	// Tee/skin settings
	if(Client()->IsSixup())
		RenderSettingsTee7(MainView);
	else
		RenderSettingsTee(MainView);
}

// Country flag popup: searchable listbox of every flag.
void CMenus::RenderPopupCountry(CUIRect MainView)
{
	static CLineInputBuffered<25> s_FlagFilterInput;

	class CCountryFlagEntry
	{
	public:
		const CCountryFlags::CCountryFlag *m_pFlag;
		std::optional<std::pair<int, int>> m_NameMatch;
	};

	std::vector<CCountryFlagEntry> vFilteredFlags;
	for(size_t i = 0; i < GameClient()->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag &Entry = GameClient()->m_CountryFlags.GetByIndex(i);
		if(!s_FlagFilterInput.IsEmpty())
		{
			const char *pNameMatchEnd;
			const char *pNameMatchStart = str_utf8_find_nocase(Entry.m_aCountryCodeString, s_FlagFilterInput.GetString(), &pNameMatchEnd);
			if(pNameMatchStart != nullptr)
			{
				vFilteredFlags.emplace_back(&Entry, std::make_pair<int, int>(pNameMatchStart - Entry.m_aCountryCodeString, pNameMatchEnd - pNameMatchStart));
			}
		}
		else
		{
			vFilteredFlags.emplace_back(&Entry, std::nullopt);
		}
	}

	// Footer: search field on the left, OK button on the right.
	CUIRect Footer, QuickSearch, OkButton;
	MainView.HSplitBottom(20.0f, &MainView, &Footer);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);

	OkButton = Footer;
	OkButton.VSplitRight(100.0f, &QuickSearch, &OkButton);
	QuickSearch.VSplitRight(10.0f, &QuickSearch, nullptr);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, nullptr);

	int OldSelected = -1;
	static CListBox s_ListBox;
	s_ListBox.DoStart(48.0f, vFilteredFlags.size(), 10, 3, OldSelected, &MainView);

	for(size_t i = 0; i < vFilteredFlags.size(); i++)
	{
		const CCountryFlagEntry &Entry = vFilteredFlags[i];

		if(Entry.m_pFlag->m_CountryCode == m_PopupCountrySelection)
			OldSelected = i;

		const CListboxItem Item = s_ListBox.DoNextItem(&Entry.m_pFlag->m_CountryCode, OldSelected >= 0 && (size_t)OldSelected == i);
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.Margin(5.0f, &FlagRect);
		FlagRect.HSplitBottom(12.0f, &FlagRect, &Label);
		Label.HSplitTop(2.0f, nullptr, &Label);
		const float OldWidth = FlagRect.w;
		FlagRect.w = FlagRect.h * 2;
		FlagRect.x += (OldWidth - FlagRect.w) / 2.0f;
		GameClient()->m_CountryFlags.Render(Entry.m_pFlag->m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		if(Entry.m_pFlag->m_Texture.IsValid() || Entry.m_pFlag->m_CountryCode == CountryCode::DEFAULT)
		{
			SLabelProperties Props;
			Props.m_MaxWidth = Label.w - 5.0f;
			if(Entry.m_NameMatch.has_value())
			{
				const auto [MatchStart, MatchLength] = Entry.m_NameMatch.value();
				Props.m_vColorSplits.emplace_back(MatchStart, MatchLength, ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f));
			}
			Ui()->DoLabel(&Label, Entry.m_pFlag->m_aCountryCodeString, 10.0f, TEXTALIGN_MC, Props);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected && NewSelected >= 0)
	{
		// Stage the selection; the OK button below applies it.
		m_PopupCountrySelection = vFilteredFlags[NewSelected].m_pFlag->m_CountryCode;
	}

	if(GameClient()->m_CountryFlags.Num() > 0 && vFilteredFlags.empty())
	{
		CUIRect FilterLabel, ResetButton;
		MainView.HMargin((MainView.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &FilterLabel);
		FilterLabel.HSplitTop(16.0f, &FilterLabel, &ResetButton);
		ResetButton.HSplitTop(8.0f, nullptr, &ResetButton);
		ResetButton.VMargin((ResetButton.w - 200.0f) / 2.0f, &ResetButton);
		Ui()->DoLabel(&FilterLabel, Localize("No country flags match your filter criteria"), 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ResetButton;
		if(DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &ResetButton))
		{
			s_FlagFilterInput.Clear();
		}
	}

	Ui()->DoEditBox_Search(&s_FlagFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	static CButtonContainer s_ButtonOk;
	if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &OkButton) ||
		Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER))
	{
		m_Popup = POPUP_NONE;
		PopupConfirmPlayerCountry();
	}
}

void CMenus::PopupConfirmPlayerCountry()
{
	if(m_PopupCountrySelection != -2)
	{
		if(!m_Dummy)
			g_Config.m_PlayerCountry = m_PopupCountrySelection;
		else
			g_Config.m_ClDummyCountry = m_PopupCountrySelection;
		SetNeedSendInfo();
	}
	m_PopupCountrySelection = -2;
}
