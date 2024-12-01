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

using namespace FontIcons;

void CMenus::RenderSettingsTee7(CUIRect MainView)
{
	CUIRect SkinPreview, NormalSkinPreview, RedTeamSkinPreview, BlueTeamSkinPreview, Buttons, QuickSearch, DirectoryButton, RefreshButton, SaveDeleteButton, TabBars, TabBar, LeftTab, RightTab;
	MainView.HSplitBottom(20.0f, &MainView, &Buttons);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	Buttons.VSplitRight(25.0f, &Buttons, &RefreshButton);
	Buttons.VSplitRight(10.0f, &Buttons, nullptr);
	Buttons.VSplitRight(140.0f, &Buttons, &DirectoryButton);
	Buttons.VSplitLeft(220.0f, &QuickSearch, &Buttons);
	Buttons.VSplitLeft(10.0f, nullptr, &Buttons);
	Buttons.VSplitLeft(120.0f, &SaveDeleteButton, &Buttons);
	MainView.HSplitTop(50.0f, &TabBars, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);
	TabBars.VSplitMid(&TabBars, &SkinPreview, 20.0f);

	TabBars.HSplitTop(20.0f, &TabBar, &TabBars);
	TabBar.VSplitMid(&LeftTab, &RightTab);
	TabBars.HSplitTop(10.0f, nullptr, &TabBars);

	SkinPreview.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	SkinPreview.VMargin(10.0f, &SkinPreview);
	SkinPreview.VSplitRight(50.0f, &SkinPreview, &BlueTeamSkinPreview);
	SkinPreview.VSplitRight(10.0f, &SkinPreview, nullptr);
	SkinPreview.VSplitRight(50.0f, &SkinPreview, &RedTeamSkinPreview);
	SkinPreview.VSplitRight(10.0f, &SkinPreview, nullptr);
	SkinPreview.VSplitRight(50.0f, &SkinPreview, &NormalSkinPreview);
	SkinPreview.VSplitRight(10.0f, &SkinPreview, nullptr);

	static CButtonContainer s_PlayerTabButton;
	if(DoButton_MenuTab(&s_PlayerTabButton, Localize("Player"), !m_Dummy, &LeftTab, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = false;
	}

	static CButtonContainer s_DummyTabButton;
	if(DoButton_MenuTab(&s_DummyTabButton, Localize("Dummy"), m_Dummy, &RightTab, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = true;
	}

	TabBars.HSplitTop(20.0f, &TabBar, &TabBars);
	TabBar.VSplitMid(&LeftTab, &RightTab);

	static CButtonContainer s_BasicTabButton;
	if(DoButton_MenuTab(&s_BasicTabButton, Localize("Basic"), !m_CustomSkinMenu, &LeftTab, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_CustomSkinMenu = false;
	}

	static CButtonContainer s_CustomTabButton;
	if(DoButton_MenuTab(&s_CustomTabButton, Localize("Custom"), m_CustomSkinMenu, &RightTab, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_CustomSkinMenu = true;
		if(m_CustomSkinMenu && m_pSelectedSkin)
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

	// validate skin parts for solo mode
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

	CTeeRenderInfo OwnSkinInfo;
	OwnSkinInfo.m_Size = 50.0f;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false);
		if(aUCCVars[Part])
		{
			OwnSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = pSkinPart->m_ColorTexture;
			OwnSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = m_pClient->m_Skins7.GetColor(aColorVars[Part], Part == protocol7::SKINPART_MARKING);
		}
		else
		{
			OwnSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = pSkinPart->m_OrgTexture;
			OwnSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	char aBuf[128 + IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Your skin"));
	Ui()->DoLabel(&SkinPreview, aBuf, 14.0f, TEXTALIGN_ML);

	{
		// interactive tee: tee looking towards cursor, and it is happy when you touch it
		const vec2 TeePosition = NormalSkinPreview.Center() + vec2(0.0f, 6.0f);
		const vec2 DeltaPosition = Ui()->MousePos() - TeePosition;
		const float Distance = length(DeltaPosition);
		const float InteractionDistance = 20.0f;
		const vec2 TeeDirection = Distance < InteractionDistance ? normalize(vec2(DeltaPosition.x, maximum(DeltaPosition.y, 0.5f))) : normalize(DeltaPosition);
		const int TeeEmote = Distance < InteractionDistance ? EMOTE_HAPPY : EMOTE_NORMAL;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, TeeEmote, TeeDirection, TeePosition);
		static char s_InteractiveTeeButtonId;
		if(Distance < InteractionDistance && Ui()->DoButtonLogic(&s_InteractiveTeeButtonId, 0, &NormalSkinPreview))
		{
			m_pClient->m_Sounds.Play(CSounds::CHN_GUI, SOUND_PLAYER_SPAWN, 1.0f);
		}
	}

	// validate skin parts for team game mode
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		str_copy(aSkinParts[Part], CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], protocol7::MAX_SKIN_ARRAY_SIZE);
		apSkinPartsPtr[Part] = aSkinParts[Part];
		aUCCVars[Part] = *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part];
		aColorVars[Part] = *CSkins7::ms_apColorVariables[(int)m_Dummy][Part];
	}
	m_pClient->m_Skins7.ValidateSkinParts(apSkinPartsPtr, aUCCVars, aColorVars, GAMEFLAG_TEAMS);

	CTeeRenderInfo TeamSkinInfo;
	TeamSkinInfo.m_Size = OwnSkinInfo.m_Size;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false);
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = aUCCVars[Part] ? pSkinPart->m_ColorTexture : pSkinPart->m_OrgTexture;
	}

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = m_pClient->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_RED, Part);
	}
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, 0, vec2(1, 0), RedTeamSkinPreview.Center() + vec2(0.0f, 6.0f));

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = m_pClient->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_BLUE, Part);
	}
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, 0, vec2(-1, 0), BlueTeamSkinPreview.Center() + vec2(0.0f, 6.0f));

	if(m_CustomSkinMenu)
		RenderSettingsTeeCustom7(MainView);
	else
		RenderSkinSelection7(MainView);

	if(m_CustomSkinMenu)
	{
		static CButtonContainer s_CustomSkinSaveButton;
		if(DoButton_Menu(&s_CustomSkinSaveButton, Localize("Save"), 0, &SaveDeleteButton))
		{
			m_Popup = POPUP_SAVE_SKIN;
			m_SkinNameInput.SelectAll();
			Ui()->SetActiveItem(&m_SkinNameInput);
		}
	}
	else if(m_pSelectedSkin && (m_pSelectedSkin->m_Flags & CSkins7::SKINFLAG_STANDARD) == 0)
	{
		static CButtonContainer s_CustomSkinDeleteButton;
		if(DoButton_Menu(&s_CustomSkinDeleteButton, Localize("Delete"), 0, &SaveDeleteButton) || Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE))
		{
			str_format(aBuf, sizeof(aBuf), Localize("Are you sure that you want to delete '%s'?"), m_pSelectedSkin->m_aName);
			PopupConfirm(Localize("Delete skin"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDeleteSkin7);
		}
	}

	static CLineInput s_SkinFilterInput(g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString));
	if(Ui()->DoEditBox_Search(&s_SkinFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && m_pClient->m_GameConsole.IsClosed()))
	{
		m_SkinList7LastRefreshTime = std::nullopt;
		m_SkinPartsList7LastRefreshTime = std::nullopt;
	}

	static CButtonContainer s_DirectoryButton;
	if(DoButton_Menu(&s_DirectoryButton, Localize("Skins directory"), 0, &DirectoryButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins7", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("skins7", IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_DirectoryButton, &DirectoryButton, Localize("Open the directory to add custom skins"));

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static CButtonContainer s_SkinRefreshButton;
	if(DoButton_Menu(&s_SkinRefreshButton, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &RefreshButton) ||
		(!Ui()->IsPopupOpen() && m_pClient->m_GameConsole.IsClosed() && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R)))))
	{
		// reset render flags for possible loading screen
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		// TODO: m_pClient->RefreshSkins();
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::PopupConfirmDeleteSkin7()
{
	dbg_assert(m_pSelectedSkin, "no skin selected for deletion");

	if(!m_pClient->m_Skins7.RemoveSkin(m_pSelectedSkin))
	{
		PopupMessage(Localize("Error"), Localize("Unable to delete skin"), Localize("Ok"));
		return;
	}
	m_pSelectedSkin = nullptr;
}

void CMenus::RenderSettingsTeeCustom7(CUIRect MainView)
{
	CUIRect ButtonBar, SkinPartSelection, CustomColors;

	MainView.HSplitTop(20.0f, &ButtonBar, &MainView);
	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_B, 5.0f);
	MainView.VSplitMid(&SkinPartSelection, &CustomColors, 10.0f);
	CustomColors.Margin(5.0f, &CustomColors);
	CUIRect CustomColorsButton, RandomSkinButton;
	CustomColors.HSplitTop(20.0f, &CustomColorsButton, &CustomColors);
	CustomColorsButton.VSplitRight(30.0f, &CustomColorsButton, &RandomSkinButton);
	CustomColorsButton.VSplitRight(20.0f, &CustomColorsButton, nullptr);

	const float ButtonWidth = ButtonBar.w / protocol7::NUM_SKINPARTS;

	static CButtonContainer s_aSkinPartButtons[protocol7::NUM_SKINPARTS];
	for(int i = 0; i < protocol7::NUM_SKINPARTS; i++)
	{
		CUIRect Button;
		ButtonBar.VSplitLeft(ButtonWidth, &Button, &ButtonBar);
		const int Corners = i == 0 ? IGraphics::CORNER_TL : (i == (protocol7::NUM_SKINPARTS - 1) ? IGraphics::CORNER_TR : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aSkinPartButtons[i], Localize(CSkins7::ms_apSkinPartNamesLocalized[i], "skins"), m_TeePartSelected == i, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			m_TeePartSelected = i;
		}
	}

	RenderSkinPartSelection7(SkinPartSelection);

	int *pUseCustomColor = CSkins7::ms_apUCCVariables[(int)m_Dummy][m_TeePartSelected];
	if(DoButton_CheckBox(pUseCustomColor, Localize("Custom colors"), *pUseCustomColor, &CustomColorsButton))
	{
		*pUseCustomColor = !*pUseCustomColor;
		SetNeedSendInfo();
	}

	if(*pUseCustomColor)
	{
		CUIRect CustomColorScrollbars;
		CustomColors.HSplitTop(5.0f, nullptr, &CustomColors);
		CustomColors.HSplitTop(95.0f, &CustomColorScrollbars, &CustomColors);

		if(RenderHslaScrollbars(&CustomColorScrollbars, CSkins7::ms_apColorVariables[(int)m_Dummy][m_TeePartSelected], m_TeePartSelected == protocol7::SKINPART_MARKING, ColorHSLA::DARKEST_LGT7))
		{
			SetNeedSendInfo();
		}
	}

	// Random skin button
	static CButtonContainer s_RandomSkinButton;
	static const char *s_apDice[] = {FONT_ICON_DICE_ONE, FONT_ICON_DICE_TWO, FONT_ICON_DICE_THREE, FONT_ICON_DICE_FOUR, FONT_ICON_DICE_FIVE, FONT_ICON_DICE_SIX};
	static int s_CurrentDie = rand() % std::size(s_apDice);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(DoButton_Menu(&s_RandomSkinButton, s_apDice[s_CurrentDie], 0, &RandomSkinButton, nullptr, IGraphics::CORNER_ALL, 5.0f, -0.2f))
	{
		m_pClient->m_Skins7.RandomizeSkin(m_Dummy);
		SetNeedSendInfo();
		s_CurrentDie = rand() % std::size(s_apDice);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	GameClient()->m_Tooltips.DoToolTip(&s_RandomSkinButton, &RandomSkinButton, Localize("Create a random skin"));
}

void CMenus::RenderSkinSelection7(CUIRect MainView)
{
	static float s_LastSelectionTime = -10.0f;
	static std::vector<const CSkins7::CSkin *> s_vpSkinList;
	static CListBox s_ListBox;

	if(!m_SkinList7LastRefreshTime.has_value() || m_SkinList7LastRefreshTime.value() != m_SkinList7LastRefreshTime)
	{
		s_vpSkinList.clear();
		for(const CSkins7::CSkin &Skin : GameClient()->m_Skins7.GetSkins())
		{
			if((Skin.m_Flags & CSkins7::SKINFLAG_SPECIAL) != 0)
				continue;
			if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(Skin.m_aName, g_Config.m_ClSkinFilterString))
				continue;

			s_vpSkinList.emplace_back(&Skin);
		}
	}

	m_pSelectedSkin = nullptr;
	int s_OldSelected = -1;
	s_ListBox.DoStart(50.0f, s_vpSkinList.size(), 4, 1, s_OldSelected, &MainView);

	for(int i = 0; i < (int)s_vpSkinList.size(); ++i)
	{
		const CSkins7::CSkin *pSkin = s_vpSkinList[i];
		if(pSkin == nullptr)
			continue;
		if(!str_comp(pSkin->m_aName, CSkins7::ms_apSkinNameVariables[m_Dummy]))
		{
			m_pSelectedSkin = pSkin;
			s_OldSelected = i;
		}

		const CListboxItem Item = s_ListBox.DoNextItem(&s_vpSkinList[i], s_OldSelected == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TeePreview, Label;
		Item.m_Rect.VSplitLeft(60.0f, &TeePreview, &Label);

		CTeeRenderInfo Info;
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			if(pSkin->m_aUseCustomColors[Part])
			{
				Info.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = pSkin->m_apParts[Part]->m_ColorTexture;
				Info.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = m_pClient->m_Skins7.GetColor(pSkin->m_aPartColors[Part], Part == protocol7::SKINPART_MARKING);
			}
			else
			{
				Info.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = pSkin->m_apParts[Part]->m_OrgTexture;
				Info.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}
		Info.m_Size = 50.0f;

		{
			// interactive tee: tee is happy to be selected
			int TeeEmote = (Item.m_Selected && s_LastSelectionTime + 0.75f > Client()->GlobalTime()) ? EMOTE_HAPPY : EMOTE_NORMAL;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, TeeEmote, vec2(1.0f, 0.0f), TeePreview.Center() + vec2(0.0f, 6.0f));
		}

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w - 5.0f;
		Ui()->DoLabel(&Label, pSkin->m_aName, 12.0f, TEXTALIGN_ML, Props);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != -1 && NewSelected != s_OldSelected)
	{
		s_LastSelectionTime = Client()->GlobalTime();
		m_pSelectedSkin = s_vpSkinList[NewSelected];
		str_copy(CSkins7::ms_apSkinNameVariables[m_Dummy], m_pSelectedSkin->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
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
	static std::vector<const CSkins7::CSkinPart *> s_paList[protocol7::NUM_SKINPARTS];
	static CListBox s_ListBox;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		if(!m_SkinList7LastRefreshTime.has_value() || m_SkinList7LastRefreshTime.value() != GameClient()->m_Skins7.LastRefreshTime())
		{
			s_paList[Part].clear();
			for(const CSkins7::CSkinPart &SkinPart : GameClient()->m_Skins7.GetSkinParts(Part))
			{
				if((SkinPart.m_Flags & CSkins7::SKINFLAG_SPECIAL) != 0)
					continue;

				if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(SkinPart.m_aName, g_Config.m_ClSkinFilterString))
					continue;

				s_paList[Part].emplace_back(&SkinPart);
			}
		}
	}

	static int s_OldSelected = -1;
	s_ListBox.DoBegin(&MainView);
	s_ListBox.DoStart(72.0f, s_paList[m_TeePartSelected].size(), 4, 1, s_OldSelected, nullptr, false, IGraphics::CORNER_NONE, true);

	for(int i = 0; i < (int)s_paList[m_TeePartSelected].size(); ++i)
	{
		const CSkins7::CSkinPart *pPart = s_paList[m_TeePartSelected][i];
		if(pPart == nullptr)
			continue;
		if(!str_comp(pPart->m_aName, CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected]))
			s_OldSelected = i;

		CListboxItem Item = s_ListBox.DoNextItem(&s_paList[m_TeePartSelected][i], s_OldSelected == i);
		if(!Item.m_Visible)
			continue;

		CUIRect Label;
		Item.m_Rect.Margin(5.0f, &Item.m_Rect);
		Item.m_Rect.HSplitBottom(12.0f, &Item.m_Rect, &Label);

		CTeeRenderInfo Info;
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			const CSkins7::CSkinPart *pSkinPart = m_pClient->m_Skins7.FindSkinPart(Part, CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], false);
			if(*CSkins7::ms_apUCCVariables[(int)m_Dummy][Part])
			{
				Info.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = m_TeePartSelected == Part ? pPart->m_ColorTexture : pSkinPart->m_ColorTexture;
				Info.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = m_pClient->m_Skins7.GetColor(*CSkins7::ms_apColorVariables[(int)m_Dummy][Part], Part == protocol7::SKINPART_MARKING);
			}
			else
			{
				Info.m_aSixup[g_Config.m_ClDummy].m_aTextures[Part] = m_TeePartSelected == Part ? pPart->m_OrgTexture : pSkinPart->m_OrgTexture;
				Info.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}
		Info.m_Size = 50.0f;

		const vec2 TeePos = Item.m_Rect.Center() + vec2(0.0f, 6.0f);
		if(m_TeePartSelected == protocol7::SKINPART_HANDS)
		{
			// RenderTools()->RenderTeeHand(&Info, TeePos, vec2(1.0f, 0.0f), -pi*0.5f, vec2(18, 0));
		}
		int TeePartEmote = EMOTE_NORMAL;
		if(m_TeePartSelected == protocol7::SKINPART_EYES)
		{
			TeePartEmote = (int)(Client()->GlobalTime() * 0.5f) % NUM_EMOTES;
		}
		RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, TeePartEmote, vec2(1.0f, 0.0f), TeePos);

		Ui()->DoLabel(&Label, pPart->m_aName, 12.0f, TEXTALIGN_MC);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != -1 && NewSelected != s_OldSelected)
	{
		str_copy(CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected], s_paList[m_TeePartSelected][NewSelected]->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
		CSkins7::ms_apSkinNameVariables[m_Dummy][0] = '\0';
		SetNeedSendInfo();
	}
	s_OldSelected = NewSelected;
}
