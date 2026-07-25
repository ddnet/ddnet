/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

void CMenus::RenderLanguageSettings(CUIRect MainView)
{
	const float CreditsFontSize = 14.0f;
	const float CreditsMargin = 10.0f;

	CUIRect List, CreditsScroll;
	MainView.HSplitBottom(4.0f * CreditsFontSize + 2.0f * CreditsMargin, &List, &CreditsScroll);
	List.HSplitBottom(5.0f, &List, nullptr);

	RenderLanguageSelection(List);

	CreditsScroll.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);

	static CScrollRegion s_CreditsScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = CreditsFontSize;
	s_CreditsScrollRegion.Begin(&CreditsScroll, &ScrollParams);

	CTextCursor Cursor;
	Cursor.m_FontSize = CreditsFontSize;
	Cursor.m_LineWidth = CreditsScroll.w - 2.0f * CreditsMargin;

	const unsigned OldRenderFlags = TextRender()->GetRenderFlags();
	TextRender()->SetRenderFlags(OldRenderFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
	STextContainerIndex CreditsTextContainer;
	TextRender()->CreateTextContainer(CreditsTextContainer, &Cursor, Localize("English translation by the DDNet Team", "Translation credits: Add your own name here when you update translations"));
	TextRender()->SetRenderFlags(OldRenderFlags);
	if(CreditsTextContainer.Valid())
	{
		CUIRect CreditsLabel;
		CreditsScroll.HSplitTop(TextRender()->GetBoundingBoxTextContainer(CreditsTextContainer).m_H + 2.0f * CreditsMargin, &CreditsLabel, &CreditsScroll);
		s_CreditsScrollRegion.AddRect(CreditsLabel);
		CreditsLabel.Margin(CreditsMargin, &CreditsLabel);
		TextRender()->RenderTextContainer(CreditsTextContainer, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), CreditsLabel.x, CreditsLabel.y);
		TextRender()->DeleteTextContainer(CreditsTextContainer);
	}

	s_CreditsScrollRegion.End();
}

bool CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_SelectedLanguage = -2; // -2 = unloaded, -1 = unset
	static CListBox s_ListBox;

	if(s_SelectedLanguage == -2)
	{
		s_SelectedLanguage = -1;
		for(size_t i = 0; i < g_Localization.Languages().size(); i++)
		{
			if(str_comp(g_Localization.Languages()[i].m_Filename.c_str(), g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				s_ListBox.ScrollToSelected();
				break;
			}
		}
	}

	const int OldSelected = s_SelectedLanguage;

	s_ListBox.DoStart(24.0f, g_Localization.Languages().size(), 1, 3, s_SelectedLanguage, &MainView);

	for(const auto &Language : g_Localization.Languages())
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&Language.m_Name, s_SelectedLanguage != -1 && !str_comp(g_Localization.Languages()[s_SelectedLanguage].m_Name.c_str(), Language.m_Name.c_str()));
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &FlagRect, &Label);
		FlagRect.VMargin(6.0f, &FlagRect);
		FlagRect.HMargin(3.0f, &FlagRect);
		GameClient()->m_CountryFlags.Render(Language.m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		Ui()->DoLabel(&Label, Language.m_Name.c_str(), 16.0f, TEXTALIGN_ML);
	}

	s_SelectedLanguage = s_ListBox.DoEnd();

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(g_Config.m_ClLanguagefile, g_Localization.Languages()[s_SelectedLanguage].m_Filename.c_str());
		GameClient()->OnLanguageChange();
	}

	return s_ListBox.WasItemActivated();
}
