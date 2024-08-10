/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "menus.h"
#include "skins7.h"

#include <vector>

void CMenus::RenderSettingsTee7(CUIRect MainView)
{
	static bool s_CustomSkinMenu = false;
	// static int s_PlayerCountry = 0;
	// static char s_aPlayerName[64] = {0};
	// static char s_aPlayerClan[64] = {0};

	// if(m_pClient->m_IdentityState < 0)
	// {
	// 	s_PlayerCountry = Config()->m_PlayerCountry;
	// 	str_copy(s_aPlayerName, Config()->m_PlayerName, sizeof(s_aPlayerName));
	// 	str_copy(s_aPlayerClan, Config()->m_PlayerClan, sizeof(s_aPlayerClan));
	// 	m_pClient->m_IdentityState = 0;
	// }

	CUIRect Label, TopView, BottomView, Left, Right;

	// cut view
	MainView.HSplitBottom(40.0f, &MainView, &BottomView);
	BottomView.HSplitTop(20.f, 0, &BottomView);

	CUIRect QuickSearch, Buttons;
	CUIRect ButtonLeft, ButtonMiddle, ButtonRight;

	BottomView.VSplitMid(&QuickSearch, &Buttons);

	const float ButtonSize = Buttons.w / 3;
	Buttons.VSplitLeft(ButtonSize, &ButtonLeft, &Buttons);
	Buttons.VSplitLeft(ButtonSize, &ButtonMiddle, &Buttons);
	Buttons.VSplitLeft(ButtonSize, &ButtonRight, &Buttons);

	// HotCuiRects(MainView, BottomView, QuickSearch, ButtonLeft, ButtonMiddle, ButtonRight);

	// render skin preview background
	const float SpacingH = 2.0f;
	const float SpacingW = 3.0f;
	const float ButtonHeight = 20.0f;
	const float SkinHeight = 50.0f;
	const float BackgroundHeight = (ButtonHeight + SpacingH) + SkinHeight * 2;
	const vec2 MousePosition = vec2(Ui()->MouseX(), Ui()->MouseY());

	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(BackgroundHeight, &TopView, &MainView);
	TopView.VSplitMid(&Left, &Right, 3.0f);
	Left.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);
	Right.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

	Left.HSplitTop(ButtonHeight, &Label, &Left);
	Ui()->DoLabel(&Label, Localize("Tee"), ButtonHeight * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);

	// Preview
	{
		CUIRect Top, Bottom, TeeLeft, TeeRight;

		Left.HSplitTop(SpacingH, 0, &Left);
		Left.HSplitTop(SkinHeight * 2, &Top, &Left);

		// split the menu in 2 parts
		Top.HSplitMid(&Top, &Bottom, SpacingH);

		// handle left

		// validate skin parts for solo mode
		CTeeRenderInfo OwnSkinInfo;
		OwnSkinInfo.m_Size = 50.0f;

		char aSkinParts[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_ARRAY_SIZE];
		char *apSkinPartsPtr[protocol7::NUM_SKINPARTS];
		int aUCCVars[protocol7::NUM_SKINPARTS];
		int aColorVars[protocol7::NUM_SKINPARTS];
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			str_copy(aSkinParts[Part], CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], protocol7::MAX_SKIN_ARRAY_SIZE);
			apSkinPartsPtr[Part] = aSkinParts[Part];
			aUCCVars[Part] = *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part];
			aColorVars[Part] = *CSkins7::ms_apColorVariables[(int)m_Dummy][Part];
		}

		m_pClient->m_Skins7.ValidateSkinParts(apSkinPartsPtr, aUCCVars, aColorVars, 0);

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			int SkinPart = m_pClient->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false);
			const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.GetSkinPart(Part, SkinPart);
			if(aUCCVars[Part])
			{
				OwnSkinInfo.m_Sixup.m_aTextures[Part] = pSkinPart->m_ColorTexture;
				OwnSkinInfo.m_Sixup.m_aColors[Part] = m_pClient->m_Skins7.GetColor(aColorVars[Part], Part == protocol7::SKINPART_MARKING);
			}
			else
			{
				OwnSkinInfo.m_Sixup.m_aTextures[Part] = pSkinPart->m_OrgTexture;
				OwnSkinInfo.m_Sixup.m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

		// draw preview
		Top.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

		Top.VSplitLeft(Top.w / 3.0f + SpacingW / 2.0f, &Label, &Top);
		Label.y += 17.0f;
		Ui()->DoLabel(&Label, Localize("Normal:"), ButtonHeight * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_CENTER);

		Top.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

		{
			// interactive tee: tee looking towards cursor, and it is happy when you touch it
			vec2 TeePosition = vec2(Top.x + Top.w / 2.0f, Top.y + Top.h / 2.0f + 6.0f);
			vec2 DeltaPosition = MousePosition - TeePosition;
			float Distance = length(DeltaPosition);
			vec2 TeeDirection = Distance < 20.0f ? normalize(vec2(DeltaPosition.x, maximum(DeltaPosition.y, 0.5f))) : normalize(DeltaPosition);
			int TeeEmote = Distance < 20.0f ? EMOTE_HAPPY : EMOTE_NORMAL;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, TeeEmote, TeeDirection, TeePosition);
			if(Distance < 20.0f && Ui()->MouseButtonClicked(0))
				m_pClient->m_Sounds.Play(CSounds::CHN_GUI, SOUND_PLAYER_SPAWN, 0);
		}

		// handle right (team skins)

		// validate skin parts for team game mode
		CTeeRenderInfo TeamSkinInfo = OwnSkinInfo;

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			str_copy(aSkinParts[Part], CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], protocol7::MAX_SKIN_ARRAY_SIZE);
			apSkinPartsPtr[Part] = aSkinParts[Part];
			aUCCVars[Part] = *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part];
			aColorVars[Part] = *CSkins7::ms_apColorVariables[(int)m_Dummy][Part];
		}

		m_pClient->m_Skins7.ValidateSkinParts(apSkinPartsPtr, aUCCVars, aColorVars, GAMEFLAG_TEAMS);

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			int SkinPart = m_pClient->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false);
			const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.GetSkinPart(Part, SkinPart);
			if(aUCCVars[Part])
			{
				TeamSkinInfo.m_Sixup.m_aTextures[Part] = pSkinPart->m_ColorTexture;
				TeamSkinInfo.m_Sixup.m_aColors[Part] = m_pClient->m_Skins7.GetColor(aColorVars[Part], Part == protocol7::SKINPART_MARKING);
			}
			else
			{
				TeamSkinInfo.m_Sixup.m_aTextures[Part] = pSkinPart->m_OrgTexture;
				TeamSkinInfo.m_Sixup.m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

		// draw preview
		Bottom.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

		Bottom.VSplitLeft(Bottom.w / 3.0f + SpacingW / 2.0f, &Label, &Bottom);
		Label.y += 17.0f;
		Ui()->DoLabel(&Label, Localize("Team:"), ButtonHeight * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_CENTER);

		Bottom.VSplitMid(&TeeLeft, &TeeRight, SpacingW);

		TeeLeft.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			ColorRGBA TeamColor = m_pClient->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_RED, Part);
			TeamSkinInfo.m_Sixup.m_aColors[Part] = TeamColor;
		}
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, 0, vec2(1, 0), vec2(TeeLeft.x + TeeLeft.w / 2.0f, TeeLeft.y + TeeLeft.h / 2.0f + 6.0f));

		TeeRight.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			ColorRGBA TeamColor = m_pClient->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_BLUE, Part);
			TeamSkinInfo.m_Sixup.m_aColors[Part] = TeamColor;
		}
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, 0, vec2(-1, 0), vec2(TeeRight.x + TeeRight.w / 2.0f, TeeRight.y + TeeRight.h / 2.0f + 6.0f));
	}

	Right.HSplitTop(ButtonHeight, &Label, &Right);
	Ui()->DoLabel(&Label, Localize("Settings"), ButtonHeight * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);

	// Settings
	{
		CUIRect Top, Bottom, Dummy, DummyLabel;
		Right.HSplitTop(SpacingH, 0, &Right);
		Right.HSplitMid(&Top, &Bottom, SpacingH);

		Right.HSplitTop(20.0f, &Dummy, &Right);
		Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

		if(DoButton_CheckBox(&m_Dummy, Localize("Dummy settings"), m_Dummy, &DummyLabel))
		{
			m_Dummy ^= 1;
		}
		GameClient()->m_Tooltips.DoToolTip(&m_Dummy, &DummyLabel, Localize("Toggle to edit your dummy settings"));
	}

	MainView.HSplitTop(10.0f, 0, &MainView);

	if(s_CustomSkinMenu)
		RenderSettingsTeeCustom7(MainView);
	else
		RenderSettingsTeeBasic7(MainView);

	// bottom buttons
	if(s_CustomSkinMenu)
	{
		static CButtonContainer s_RandomizeSkinButton;
		if(DoButton_Menu(&s_RandomizeSkinButton, Localize("Randomize"), 0, &ButtonMiddle))
		{
			m_pClient->m_Skins7.RandomizeSkin(m_Dummy);
			Config()->m_ClPlayer7Skin[0] = 0;
			SetNeedSendInfo();
		}
	}
	else if(m_pSelectedSkin && (m_pSelectedSkin->m_Flags & CSkins7::SKINFLAG_STANDARD) == 0)
	{
		static CButtonContainer s_CustomSkinDeleteButton;
		if(DoButton_Menu(&s_CustomSkinDeleteButton, Localize("Delete"), 0, &ButtonMiddle))
		{
			// char aBuf[128];
			// str_format(aBuf, sizeof(aBuf), Localize("Are you sure that you want to delete the skin '%s'?"), m_pSelectedSkin->m_aName);
			// PopupConfirm(Localize("Delete skin"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDeleteSkin);
		}
	}

	static CButtonContainer s_CustomSwitchButton;
	if(DoButton_Menu(&s_CustomSwitchButton, s_CustomSkinMenu ? Localize("Basic") : Localize("Custom"), 0, &ButtonRight))
	{
		s_CustomSkinMenu = !s_CustomSkinMenu;
		if(s_CustomSkinMenu && m_pSelectedSkin)
		{
			if(m_pSelectedSkin->m_Flags & CSkins7::SKINFLAG_STANDARD)
			{
				m_SkinNameInput.Set("copy_");
				m_SkinNameInput.Append(m_pSelectedSkin->m_aName);
			}
			else
				m_SkinNameInput.Set(m_pSelectedSkin->m_aName);
		}
	}

	// Quick search
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&QuickSearch, FontIcons::FONT_ICON_MAGNIFYING_GLASS, 14.0f, TEXTALIGN_ML);
		float SearchWidth = TextRender()->TextWidth(14.0f, FontIcons::FONT_ICON_MAGNIFYING_GLASS, -1, -1.0f);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		QuickSearch.VSplitLeft(SearchWidth + 5.0f, nullptr, &QuickSearch);
		static CLineInput s_SkinFilterInput(g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString));
		if(Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			Ui()->SetActiveItem(&s_SkinFilterInput);
			s_SkinFilterInput.SelectAll();
		}
		s_SkinFilterInput.SetEmptyText(Localize("Search"));
		if(Ui()->DoClearableEditBox(&s_SkinFilterInput, &QuickSearch, 14.0f))
			m_SkinListNeedsUpdate = true;
	}
}

void CMenus::RenderSettingsTeeBasic7(CUIRect MainView)
{
	RenderSkinSelection7(MainView); // yes thats all here ^^
}

void CMenus::RenderSettingsTeeCustom7(CUIRect MainView)
{
	CUIRect Label, Patterns, Button, Left, Right, Picker, Palette;

	// render skin preview background
	float SpacingH = 2.0f;
	float SpacingW = 3.0f;
	float ButtonHeight = 20.0f;

	MainView.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

	MainView.HSplitTop(ButtonHeight, &Label, &MainView);
	Ui()->DoLabel(&Label, Localize("Customize"), ButtonHeight * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);

	// skin part selection
	MainView.HSplitTop(SpacingH, 0, &MainView);
	MainView.HSplitTop(ButtonHeight, &Patterns, &MainView);
	Patterns.Draw(vec4(0.0f, 0.0f, 0.0f, 0.25f), 15, 5.0F);

	float ButtonWidth = (Patterns.w / 6.0f) - (SpacingW * 5.0) / 6.0f;

	static CButtonContainer s_aPatternButtons[protocol7::NUM_SKINPARTS];
	for(int i = 0; i < protocol7::NUM_SKINPARTS; i++)
	{
		Patterns.VSplitLeft(ButtonWidth, &Button, &Patterns);
		if(DoButton_MenuTab(&s_aPatternButtons[i], Localize(CSkins7::ms_apSkinPartNames[i], "skins"), m_TeePartSelected == i, &Button, IGraphics::CORNER_ALL))
		{
			m_TeePartSelected = i;
		}
		Patterns.VSplitLeft(SpacingW, 0, &Patterns);
	}

	MainView.HSplitTop(SpacingH, 0, &MainView);
	MainView.VSplitMid(&Left, &Right, SpacingW);

	// part selection
	RenderSkinPartSelection7(Left);

	// use custom color checkbox
	Right.HSplitTop(ButtonHeight, &Button, &Right);
	Right.HSplitBottom(45.0f, &Picker, &Palette);
	static CButtonContainer s_ColorPicker;
	DoLine_ColorPicker(
		&s_ColorPicker,
		25.0f, // LineSize
		13.0f, // LabelSize
		5.0f, // BottomMargin
		&Right,
		Localize("Custom colors"),
		(unsigned int *)CSkins7::ms_apColorVariables[(int)m_Dummy][m_TeePartSelected],
		ColorRGBA(1.0f, 1.0f, 0.5f), // DefaultColor
		true, // CheckBoxSpacing
		CSkins7::ms_apUCCVariables[(int)m_Dummy][m_TeePartSelected], // CheckBoxValue
		m_TeePartSelected == protocol7::SKINPART_MARKING); // use alpha
	static int s_OldColor = *CSkins7::ms_apColorVariables[(int)m_Dummy][m_TeePartSelected];
	int NewColor = *CSkins7::ms_apColorVariables[(int)m_Dummy][m_TeePartSelected];
	if(s_OldColor != NewColor)
	{
		s_OldColor = NewColor;
		SetNeedSendInfo();
	}
}

void CMenus::RenderSkinSelection7(CUIRect MainView)
{
	static float s_LastSelectionTime = -10.0f;
	static std::vector<const CSkins7::CSkin *> s_vpSkinList;
	static CListBox s_ListBox;
	static int s_SkinCount = 0;
	if(m_SkinListNeedsUpdate || m_pClient->m_Skins7.Num() != s_SkinCount)
	{
		s_vpSkinList.clear();
		s_SkinCount = m_pClient->m_Skins7.Num();
		for(int i = 0; i < s_SkinCount; ++i)
		{
			const CSkins7::CSkin *s = m_pClient->m_Skins7.Get(i);

			if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(s->m_aName, g_Config.m_ClSkinFilterString))
				continue;

			// no special skins
			if((s->m_Flags & CSkins7::SKINFLAG_SPECIAL) == 0)
			{
				s_vpSkinList.emplace_back(s);
			}
		}
		m_SkinListNeedsUpdate = false;
	}

	m_pSelectedSkin = 0;
	int s_OldSelected = -1;
	s_ListBox.DoStart(60.0f, s_vpSkinList.size(), 10, 1, s_OldSelected, &MainView);

	for(int i = 0; i < (int)s_vpSkinList.size(); ++i)
	{
		const CSkins7::CSkin *s = s_vpSkinList[i];
		if(s == 0)
			continue;
		if(!str_comp(s->m_aName, Config()->m_ClPlayer7Skin))
		{
			m_pSelectedSkin = s;
			s_OldSelected = i;
		}

		CListboxItem Item = s_ListBox.DoNextItem(&s_vpSkinList[i], s_OldSelected == i);
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
			{
				if(s->m_aUseCustomColors[Part])
				{
					Info.m_Sixup.m_aTextures[Part] = s->m_apParts[Part]->m_ColorTexture;
					Info.m_Sixup.m_aColors[Part] = m_pClient->m_Skins7.GetColor(s->m_aPartColors[Part], Part == protocol7::SKINPART_MARKING);
				}
				else
				{
					Info.m_Sixup.m_aTextures[Part] = s->m_apParts[Part]->m_OrgTexture;
					Info.m_Sixup.m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
				}
			}

			Info.m_Size = 50.0f;
			Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

			{
				// interactive tee: tee is happy to be selected
				int TeeEmote = (Item.m_Selected && s_LastSelectionTime + 0.75f > Client()->LocalTime()) ? EMOTE_HAPPY : EMOTE_NORMAL;
				RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, TeeEmote, vec2(1.0f, 0.0f), vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}

			CUIRect Label;
			Item.m_Rect.Margin(5.0f, &Item.m_Rect);
			Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);

			Ui()->DoLabel(&Label, s->m_aName, 10.0f, TEXTALIGN_MC);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != -1 && NewSelected != s_OldSelected)
	{
		s_LastSelectionTime = Client()->LocalTime();
		m_pSelectedSkin = s_vpSkinList[NewSelected];
		str_copy(Config()->m_ClPlayer7Skin, m_pSelectedSkin->m_aName);
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			str_copy(CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], m_pSelectedSkin->m_apParts[Part]->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
			*CSkins7::ms_apUCCVariables[(int)m_Dummy][Part] = m_pSelectedSkin->m_aUseCustomColors[Part];
			*CSkins7::ms_apColorVariables[(int)m_Dummy][Part] = m_pSelectedSkin->m_aPartColors[Part];
		}
		SetNeedSendInfo();
	}
}

void CMenus::RenderSkinPartSelection7(CUIRect MainView)
{
	static bool s_InitSkinPartList = true;
	static std::vector<const CSkins7::CSkinPart *> s_paList[protocol7::NUM_SKINPARTS];
	static CListBox s_ListBox;
	if(s_InitSkinPartList)
	{
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			s_paList[Part].clear();
			for(int i = 0; i < m_pClient->m_Skins7.NumSkinPart(Part); ++i)
			{
				const CSkins7::CSkinPart *s = m_pClient->m_Skins7.GetSkinPart(Part, i);
				// no special skins
				if((s->m_Flags & CSkins7::SKINFLAG_SPECIAL) == 0)
				{
					s_paList[Part].emplace_back(s);
				}
			}
		}
		s_InitSkinPartList = false;
	}

	static int s_OldSelected = -1;
	s_ListBox.DoBegin(&MainView);
	s_ListBox.DoStart(60.0f, s_paList[m_TeePartSelected].size(), 5, 1, s_OldSelected);

	for(int i = 0; i < (int)s_paList[m_TeePartSelected].size(); ++i)
	{
		const CSkins7::CSkinPart *s = s_paList[m_TeePartSelected][i];
		if(s == 0)
			continue;
		if(!str_comp(s->m_aName, CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected]))
			s_OldSelected = i;

		CListboxItem Item = s_ListBox.DoNextItem(&s_paList[m_TeePartSelected][i], s_OldSelected == i);
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			for(int j = 0; j < protocol7::NUM_SKINPARTS; j++)
			{
				int SkinPart = m_pClient->m_Skins7.FindSkinPart(j, CSkins7::ms_apSkinVariables[(int)m_Dummy][j], false);
				const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.GetSkinPart(j, SkinPart);
				if(*CSkins7::ms_apUCCVariables[(int)m_Dummy][j])
				{
					if(m_TeePartSelected == j)
						Info.m_Sixup.m_aTextures[j] = s->m_ColorTexture;
					else
						Info.m_Sixup.m_aTextures[j] = pSkinPart->m_ColorTexture;
					Info.m_Sixup.m_aColors[j] = m_pClient->m_Skins7.GetColor(*CSkins7::ms_apColorVariables[(int)m_Dummy][j], j == protocol7::SKINPART_MARKING);
				}
				else
				{
					if(m_TeePartSelected == j)
						Info.m_Sixup.m_aTextures[j] = s->m_OrgTexture;
					else
						Info.m_Sixup.m_aTextures[j] = pSkinPart->m_OrgTexture;
					Info.m_Sixup.m_aColors[j] = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				}
			}
			Info.m_Size = 50.0f;
			Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top
			const vec2 TeePos(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2);

			if(m_TeePartSelected == protocol7::SKINPART_HANDS)
			{
				// RenderTools()->RenderTeeHand(&Info, TeePos, vec2(1.0f, 0.0f), -pi*0.5f, vec2(18, 0));
			}
			int TeePartEmote = EMOTE_NORMAL;
			if(m_TeePartSelected == protocol7::SKINPART_EYES)
			{
				float LocalTime = Client()->LocalTime();
				TeePartEmote = (int)(LocalTime * 0.5f) % NUM_EMOTES;
			}
			RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, TeePartEmote, vec2(1.0f, 0.0f), TeePos);

			CUIRect Label;
			Item.m_Rect.Margin(5.0f, &Item.m_Rect);
			Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);

			Ui()->DoLabel(&Label, s->m_aName, 10.0f, TEXTALIGN_MC);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != -1 && NewSelected != s_OldSelected)
	{
		const CSkins7::CSkinPart *s = s_paList[m_TeePartSelected][NewSelected];
		str_copy(CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected], s->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
		Config()->m_ClPlayer7Skin[0] = 0;
		SetNeedSendInfo();
	}
	s_OldSelected = NewSelected;
}
