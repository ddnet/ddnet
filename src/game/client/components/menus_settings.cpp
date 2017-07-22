/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SDL.h" // SDL_VIDEO_DRIVER_X11

#include <base/tl/string.h>

#include <base/math.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/components/sounds.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/localization.h>

#include "binds.h"
#include "camera.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

CMenusKeyBinder CMenus::m_Binder;

CMenusKeyBinder::CMenusKeyBinder()
{
	m_TakeKey = false;
	m_GotKey = false;
}

bool CMenusKeyBinder::OnInput(IInput::CEvent Event)
{
	if(m_TakeKey)
	{
		if(Event.m_Flags&IInput::FLAG_PRESS)
		{
			m_Key = Event;
			m_GotKey = true;
			m_TakeKey = false;
		}
		return true;
	}

	return false;
}

void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
	char aBuf[128];
	CUIRect Label, Button, Left, Right, Game, Client, AutoReconnect;
	MainView.HSplitTop(180.0f, &Game, &Client);
	Client.HSplitTop(160.0f, &Client, &AutoReconnect);

	// game
	{
		// headline
		Game.HSplitTop(30.0f, &Label, &Game);
		UI()->DoLabelScaled(&Label, Localize("Game"), 20.0f, -1);
		Game.Margin(5.0f, &Game);
		Game.VSplitMid(&Left, &Right);
		Left.VSplitRight(5.0f, &Left, 0);
		Right.VMargin(5.0f, &Right);

		// dynamic camera
		Left.HSplitTop(20.0f, &Button, &Left);
		static int s_DynamicCameraButton = 0;
		if(DoButton_CheckBox(&s_DynamicCameraButton, Localize("Dynamic Camera"), g_Config.m_ClMouseDeadzone != 0, &Button))
		{
			if(g_Config.m_ClMouseDeadzone)
			{
				g_Config.m_ClMouseFollowfactor = 0;
				g_Config.m_ClMouseMaxDistance = 400;
				g_Config.m_ClMouseDeadzone = 0;
			}
			else
			{
				g_Config.m_ClMouseFollowfactor = 60;
				g_Config.m_ClMouseMaxDistance = 1000;
				g_Config.m_ClMouseDeadzone = 300;
			}
		}

		// weapon pickup
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeapons, Localize("Switch weapon on pickup"), g_Config.m_ClAutoswitchWeapons, &Button))
			g_Config.m_ClAutoswitchWeapons ^= 1;

		// weapon out of ammo autoswitch
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeaponsOutOfAmmo, Localize("Switch weapon when out of ammo"), g_Config.m_ClAutoswitchWeaponsOutOfAmmo, &Button))
			g_Config.m_ClAutoswitchWeaponsOutOfAmmo ^= 1;

		// weapon reset on death
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClResetWantedWeaponOnDeath, Localize("Reset wanted weapon on death"), g_Config.m_ClResetWantedWeaponOnDeath, &Button))
			g_Config.m_ClResetWantedWeaponOnDeath ^= 1;

		// chat messages
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClShowChatFriends, Localize("Show only chat messages from friends"), g_Config.m_ClShowChatFriends, &Button))
			g_Config.m_ClShowChatFriends ^= 1;

		// name plates
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClNameplates, Localize("Show name plates"), g_Config.m_ClNameplates, &Button))
			g_Config.m_ClNameplates ^= 1;

		if(g_Config.m_ClNameplates)
		{
			Right.HSplitTop(2.5f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Name plates size"), g_Config.m_ClNameplatesSize);
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClNameplatesSize = (int)(DoScrollbarH(&g_Config.m_ClNameplatesSize, &Button, g_Config.m_ClNameplatesSize/100.0f)*100.0f+0.1f);

			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClNameplatesTeamcolors, Localize("Use team colors for name plates"), g_Config.m_ClNameplatesTeamcolors, &Button))
				g_Config.m_ClNameplatesTeamcolors ^= 1;

			Right.HSplitTop(5.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClNameplatesClan, Localize("Show clan above name plates"), g_Config.m_ClNameplatesClan, &Button))
				g_Config.m_ClNameplatesClan ^= 1;
			}

			if(g_Config.m_ClNameplatesClan)
			{
				Right.HSplitTop(2.5f, 0, &Right);
				Right.HSplitTop(20.0f, &Label, &Right);
				Right.HSplitTop(20.0f, &Button, &Right);
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Clan plates size"), g_Config.m_ClNameplatesClanSize);
				UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
				Button.HMargin(2.0f, &Button);
				g_Config.m_ClNameplatesClanSize = (int)(DoScrollbarH(&g_Config.m_ClNameplatesClanSize, &Button, g_Config.m_ClNameplatesClanSize/100.0f)*100.0f+0.1f);
			}
	}

	// client
	{
		// headline
		Client.HSplitTop(30.0f, &Label, &Client);
		UI()->DoLabelScaled(&Label, Localize("Client"), 20.0f, -1);
		Client.Margin(5.0f, &Client);
		Client.VSplitMid(&Left, &Right);
		Left.VSplitRight(5.0f, &Left, 0);
		Right.VMargin(5.0f, &Right);

		// auto demo settings
		{
			Left.HSplitTop(20.0f, &Button, &Left);
			if(DoButton_CheckBox(&g_Config.m_ClAutoDemoRecord, Localize("Automatically record demos"), g_Config.m_ClAutoDemoRecord, &Button))
				g_Config.m_ClAutoDemoRecord ^= 1;

			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoScreenshot, Localize("Automatically take game over screenshot"), g_Config.m_ClAutoScreenshot, &Button))
				g_Config.m_ClAutoScreenshot ^= 1;

			Left.HSplitTop(10.0f, 0, &Left);
			Left.HSplitTop(20.0f, &Label, &Left);
			Button.VSplitRight(20.0f, &Button, 0);
			char aBuf[64];
			if(g_Config.m_ClAutoDemoMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max demos"), g_Config.m_ClAutoDemoMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max demos"), Localize("no limit"));
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Left.HSplitTop(20.0f, &Button, 0);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoDemoMax = static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoDemoMax, &Button, g_Config.m_ClAutoDemoMax/1000.0f)*1000.0f+0.1f);

			Right.HSplitTop(10.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Button.VSplitRight(20.0f, &Button, 0);
			if(g_Config.m_ClAutoScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), g_Config.m_ClAutoScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), Localize("no limit"));
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, 0);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoScreenshotMax = static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoScreenshotMax, &Button, g_Config.m_ClAutoScreenshotMax/1000.0f)*1000.0f+0.1f);
		}

		Left.HSplitTop(20.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Label, &Left);
		Button.VSplitRight(20.0f, &Button, 0);
		char aBuf[64];
		if(g_Config.m_ClRefreshRate)
			str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), g_Config.m_ClRefreshRate);
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Refresh Rate"), Localize("no limit"));
		UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
		Left.HSplitTop(20.0f, &Button, 0);
		Button.HMargin(2.0f, &Button);
		g_Config.m_ClRefreshRate= static_cast<int>(DoScrollbarH(&g_Config.m_ClRefreshRate, &Button, g_Config.m_ClRefreshRate/10000.0f)*10000.0f+0.1f);

#if defined(CONF_FAMILY_WINDOWS)
		Left.HSplitTop(20.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowConsole, Localize("Show console window"), g_Config.m_ClShowConsole, &Button))
			g_Config.m_ClShowConsole ^= 1;
#endif

		// auto statboard screenshot
		{
			Right.HSplitTop(20.0f, 0, &Right); //
			Right.HSplitTop(20.0f, 0, &Right); // Make some distance so it looks more natural
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoStatboardScreenshot,
						Localize("Automatically take statboard screenshot"),
						g_Config.m_ClAutoStatboardScreenshot, &Button))
			{
				g_Config.m_ClAutoStatboardScreenshot ^= 1;
			}

			Right.HSplitTop(10.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Button.VSplitRight(20.0f, &Button, 0);
			if(g_Config.m_ClAutoStatboardScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), g_Config.m_ClAutoStatboardScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), Localize("no limit"));
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, 0);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoStatboardScreenshotMax =
				static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoStatboardScreenshotMax,
							&Button,
							g_Config.m_ClAutoStatboardScreenshotMax/1000.0f)*1000.0f+0.1f);
		}

		// auto statboard csv
		{
			Right.HSplitTop(20.0f, 0, &Right); //
			Right.HSplitTop(20.0f, 0, &Right); // Make some distance so it looks more natural
			Right.HSplitTop(20.0f, &Button, &Right);
			if (DoButton_CheckBox(&g_Config.m_ClAutoCSV,
				Localize("Automatically create statboard csv"),
				g_Config.m_ClAutoCSV, &Button))
			{
				g_Config.m_ClAutoCSV ^= 1;
			}

			Right.HSplitTop(10.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Button.VSplitRight(20.0f, &Button, 0);
			if (g_Config.m_ClAutoCSVMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max CSVs"), g_Config.m_ClAutoCSVMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max CSVs"), Localize("no limit"));
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, 0);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoCSVMax =
				static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoCSVMax,
					&Button,
					g_Config.m_ClAutoCSVMax / 1000.0f)*1000.0f + 0.1f);
		}
	}
}

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	CUIRect Button, Label, Dummy;
	MainView.HSplitTop(10.0f, 0, &MainView);

	char *Name = g_Config.m_PlayerName;
	char *Clan = g_Config.m_PlayerClan;
	int *Country = &g_Config.m_PlayerCountry;

	if(m_Dummy)
	{
		Name = g_Config.m_ClDummyName;
		Clan = g_Config.m_ClDummyClan;
		Country = &g_Config.m_ClDummyCountry;
	}

	// player name
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(200.0f, &Button, &Dummy);
	Button.VSplitLeft(150.0f, &Button, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0, -1);
	static float s_OffsetName = 0.0f;
	if(DoEditBox(Name, &Button, Name, sizeof(g_Config.m_PlayerName), 14.0f, &s_OffsetName))
	{
		if(m_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}

	if(DoButton_CheckBox(&g_Config.m_ClShowKillMessages, Localize("Dummy settings"), m_Dummy, &Dummy))
	{
		m_Dummy ^= 1;
	}

	// player clan
	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, 0);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0, -1);
	static float s_OffsetClan = 0.0f;
	if(DoEditBox(Clan, &Button, Clan, sizeof(g_Config.m_PlayerClan), 14.0f, &s_OffsetClan))
	{
		if(m_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}

	// country flag selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	static float s_ScrollValue = 0.0f;
	int OldSelected = -1;
	UiDoListboxStart(&s_ScrollValue, &MainView, 50.0f, Localize("Country"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

	for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
	{
		const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
		if(pEntry->m_CountryCode == *Country)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i);
		if(Item.m_Visible)
		{
			CUIRect Label;
			Item.m_Rect.Margin(5.0f, &Item.m_Rect);
			Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
			float OldWidth = Item.m_Rect.w;
			Item.m_Rect.w = Item.m_Rect.h*2;
			Item.m_Rect.x += (OldWidth-Item.m_Rect.w)/ 2.0f;
			vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
			m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
			if(pEntry->m_Texture != -1)
				UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		*Country = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;
		if(m_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}
}

void CMenus::RenderSettingsTee(CUIRect MainView)
{
	CUIRect Button, Label, Button2, Dummy, DummyLabel, SkinList, QuickSearch, QuickSearchClearButton;

	bool CheckSettings = false;
	static int s_ClVanillaSkinsOnly = g_Config.m_ClVanillaSkinsOnly;

	static bool s_InitSkinlist = true;
	MainView.HSplitTop(10.0f, 0, &MainView);

	char *Skin = g_Config.m_ClPlayerSkin;
	int *UseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
	int *ColorBody = &g_Config.m_ClPlayerColorBody;
	int *ColorFeet = &g_Config.m_ClPlayerColorFeet;

	if(m_Dummy)
	{
		Skin = g_Config.m_ClDummySkin;
		UseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		ColorBody = &g_Config.m_ClDummyColorBody;
		ColorFeet = &g_Config.m_ClDummyColorFeet;
	}

	// skin info
	const CSkins::CSkin *pOwnSkin = m_pClient->m_pSkins->Get(m_pClient->m_pSkins->Find(Skin));
	CTeeRenderInfo OwnSkinInfo;
	if(*UseCustomColor)
	{
		OwnSkinInfo.m_Texture = pOwnSkin->m_ColorTexture;
		OwnSkinInfo.m_ColorBody = m_pClient->m_pSkins->GetColorV4(*ColorBody);
		OwnSkinInfo.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(*ColorFeet);
	}
	else
	{
		OwnSkinInfo.m_Texture = pOwnSkin->m_OrgTexture;
		OwnSkinInfo.m_ColorBody = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		OwnSkinInfo.m_ColorFeet = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	OwnSkinInfo.m_Size = 50.0f*UI()->Scale();

	MainView.HSplitTop(20.0f, &Label, &MainView);
	Label.VSplitLeft(280.0f, &Label, &Dummy);
	Label.VSplitLeft(230.0f, &Label, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Your skin"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&g_Config.m_ClShowKillMessages, Localize("Dummy settings"), m_Dummy, &DummyLabel))
	{
		m_Dummy ^= 1;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&g_Config.m_ClVanillaSkinsOnly, Localize("Vanilla Skins only"), g_Config.m_ClVanillaSkinsOnly, &DummyLabel))
	{
		g_Config.m_ClVanillaSkinsOnly ^= 1;
		CheckSettings = true;
	}

	if(CheckSettings)
	{
		if(s_ClVanillaSkinsOnly == g_Config.m_ClVanillaSkinsOnly)
			m_NeedRestartSkins = false;
		else
			m_NeedRestartSkins = true;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(230.0f, &Label, 0);
	RenderTools()->DrawUIRect(&Label, vec4(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 10.0f);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1, 0), vec2(Label.x+30.0f, Label.y+28.0f));
	Label.HSplitTop(15.0f, 0, &Label);
	Label.VSplitLeft(70.0f, 0, &Label);
	UI()->DoLabelScaled(&Label, Skin, 14.0f, -1, 150.0f);

	// custom colour selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitMid(&Button, &Button2);
	if(DoButton_CheckBox(&ColorBody, Localize("Custom colors"), *UseCustomColor, &Button))
	{
		*UseCustomColor = *UseCustomColor?0:1;
		if(m_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}

	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(82.5f, &Label, &MainView);
	if(*UseCustomColor)
	{
		CUIRect aRects[2];
		Label.VSplitMid(&aRects[0], &aRects[1]);
		aRects[0].VSplitRight(10.0f, &aRects[0], 0);
		aRects[1].VSplitLeft(10.0f, 0, &aRects[1]);

		int *paColors[2];
		paColors[0] = ColorBody;
		paColors[1] = ColorFeet;

		const char *paParts[] = {
			Localize("Body"),
			Localize("Feet")};
		const char *paLabels[] = {
			Localize("Hue"),
			Localize("Sat."),
			Localize("Lht.")};
		static int s_aColorSlider[2][3] = {{0}};

		for(int i = 0; i < 2; i++)
		{
			aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
			UI()->DoLabelScaled(&Label, paParts[i], 14.0f, -1);
			aRects[i].VSplitLeft(20.0f, 0, &aRects[i]);
			aRects[i].HSplitTop(2.5f, 0, &aRects[i]);

			int PrevColor = *paColors[i];
			int Color = 0;
			for(int s = 0; s < 3; s++)
			{
				aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
				Label.VSplitLeft(100.0f, &Label, &Button);
				Button.HMargin(2.0f, &Button);

				float k = ((PrevColor>>((2-s)*8))&0xff) / 255.0f;
				k = DoScrollbarH(&s_aColorSlider[i][s], &Button, k);
				Color <<= 8;
				Color += clamp((int)(k*255), 0, 255);
				UI()->DoLabelScaled(&Label, paLabels[s], 14.0f, -1);
			}

			if(PrevColor != Color)
			{
				if(m_Dummy)
					m_NeedSendDummyinfo = true;
				else
					m_NeedSendinfo = true;
			}

			*paColors[i] = Color;
		}
	}

	// skin selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(230.0f, &SkinList, &MainView);
	static sorted_array<const CSkins::CSkin *> s_paSkinList;
	static float s_ScrollValue = 0.0f;
	if(s_InitSkinlist)
	{
		s_paSkinList.clear();
		for(int i = 0; i < m_pClient->m_pSkins->Num(); ++i)
		{
			const CSkins::CSkin *s = m_pClient->m_pSkins->Get(i);

			// filter quick search
			if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_find_nocase(s->m_aName, g_Config.m_ClSkinFilterString))
				continue;

			// no special skins
			if((s->m_aName[0] == 'x' && s->m_aName[1] == '_'))
				continue;

			s_paSkinList.add(s);
		}
		s_InitSkinlist = false;
	}

	int OldSelected = -1;
	UiDoListboxStart(&s_InitSkinlist, &SkinList, 50.0f, Localize("Skins"), "", s_paSkinList.size(), 4, OldSelected, s_ScrollValue);
	for(int i = 0; i < s_paSkinList.size(); ++i)
	{
		const CSkins::CSkin *s = s_paSkinList[i];
		if(s == 0)
			continue;

		if(str_comp(s->m_aName, Skin) == 0)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
		char aBuf[128];
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			if(*UseCustomColor)
			{
				Info.m_Texture = s->m_ColorTexture;
				Info.m_ColorBody = m_pClient->m_pSkins->GetColorV4(*ColorBody);
				Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(*ColorFeet);
			}
			else
			{
				Info.m_Texture = s->m_OrgTexture;
				Info.m_ColorBody = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				Info.m_ColorFeet = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			}

			Info.m_Size = UI()->Scale()*50.0f;
			Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top
			RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, 0, vec2(1.0f, 0.0f), vec2(Item.m_Rect.x+30, Item.m_Rect.y+Item.m_Rect.h/2));


			Item.m_Rect.VSplitLeft(60.0f, 0, &Item.m_Rect);
			Item.m_Rect.HSplitTop(10.0f, 0, &Item.m_Rect);
			str_format(aBuf, sizeof(aBuf), "%s", s->m_aName);
			RenderTools()->UI()->DoLabelScaled(&Item.m_Rect, aBuf, 12.0f, -1,Item.m_Rect.w);
			if(g_Config.m_Debug)
			{
				vec3 BloodColor = *UseCustomColor ? m_pClient->m_pSkins->GetColorV3(*ColorBody) : s->m_BloodColor;
				Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BloodColor.r, BloodColor.g, BloodColor.b, 1.0f);
				IGraphics::CQuadItem QuadItem(Item.m_Rect.x, Item.m_Rect.y, 12.0f, 12.0f);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		mem_copy(Skin, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_ClPlayerSkin));
		if(m_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}

	// render quick search
	{
		MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
		QuickSearch.VSplitLeft(240.0f, &QuickSearch, 0);
		QuickSearch.HSplitTop(5.0f, 0, &QuickSearch);
		const char *pSearchLabel = "⚲";
		UI()->DoLabelScaled(&QuickSearch, pSearchLabel, 14.0f, -1);
		float wSearch = TextRender()->TextWidth(0, 14.0f, pSearchLabel, -1);
		QuickSearch.VSplitLeft(wSearch, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);
		QuickSearch.VSplitLeft(QuickSearch.w-15.0f, &QuickSearch, &QuickSearchClearButton);
		static float Offset = 0.0f;
		if(DoEditBox(&g_Config.m_ClSkinFilterString, &QuickSearch, g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString), 14.0f, &Offset, false, CUI::CORNER_L, Localize("Search")))
			s_InitSkinlist = true;

		// clear button
		{
			static int s_ClearButton = 0;
			RenderTools()->DrawUIRect(&QuickSearchClearButton, vec4(1,1,1,0.33f)*ButtonColorMul(&s_ClearButton), CUI::CORNER_R, 3.0f);
			UI()->DoLabel(&QuickSearchClearButton, "×", QuickSearchClearButton.h*ms_FontmodHeight, 0);
			if(UI()->DoButtonLogic(&s_ClearButton, "×", 0, &QuickSearchClearButton))
			{
				g_Config.m_ClSkinFilterString[0] = 0;
				UI()->SetActiveItem(&g_Config.m_ClSkinFilterString);
				s_InitSkinlist = true;
			}
		}
	}
}


typedef void (*pfnAssignFuncCallback)(CConfiguration *pConfig, int Value);

typedef struct
{
	CLocConstString m_Name;
	const char *m_pCommand;
	int m_KeyId;
} CKeyInfo;

static CKeyInfo gs_aKeys[] =
{
	{ "Move left", "+left", 0}, // Localize - these strings are localized within CLocConstString
	{ "Move right", "+right", 0 },
	{ "Jump", "+jump", 0 },
	{ "Fire", "+fire", 0 },
	{ "Hook", "+hook", 0 },
	{ "Hook collisions", "+showhookcoll", 0 },
	{ "Pause", "say /pause", 0 },
	{ "Kill", "kill", 0 },
	{ "Zoom in", "zoom+", 0 },
	{ "Zoom out", "zoom-", 0 },
	{ "Default zoom", "zoom", 0 },
	{ "Show others", "say /showothers", 0 },
	{ "Show all", "say /showall", 0 },
	{ "Toggle dyncam", "toggle cl_dyncam 0 1", 0 },
	{ "Toggle dummy", "toggle cl_dummy 0 1", 0 },
	{ "Dummy copy", "toggle cl_dummy_copy_moves 0 1", 0 },
	{ "Hammerfly dummy", "toggle cl_dummy_hammer 0 1", 0 },

	{ "Hammer", "+weapon1", 0 },
	{ "Pistol", "+weapon2", 0 },
	{ "Shotgun", "+weapon3", 0 },
	{ "Grenade", "+weapon4", 0 },
	{ "Rifle", "+weapon5", 0 },
	{ "Next weapon", "+nextweapon", 0 },
	{ "Prev. weapon", "+prevweapon", 0 },

	{ "Vote yes", "vote yes", 0 },
	{ "Vote no", "vote no", 0 },

	{ "Chat", "+show_chat; chat all", 0 },
	{ "Team chat", "+show_chat; chat team", 0 },
	{ "Converse", "+show_chat; chat all /c ", 0 },
	{ "Show chat", "+show_chat", 0 },

	{ "Emoticon", "+emote", 0 },
	{ "Spectator mode", "+spectate", 0 },
	{ "Spectate next", "spectate_next", 0 },
	{ "Spectate previous", "spectate_previous", 0 },
	{ "Console", "toggle_local_console", 0 },
	{ "Remote console", "toggle_remote_console", 0 },
	{ "Screenshot", "screenshot", 0 },
	{ "Scoreboard", "+scoreboard", 0 },
	{ "Statboard", "+statboard", 0 },
	{ "Lock team", "say /lock", 0 },
	{ "Show entities", "toggle cl_overlay_entities 0 100", 0 },
	{ "Show HUD", "toggle cl_showhud 0 1", 0 },
};

/*	This is for scripts/update_localization.py to work, don't remove!
	Localize("Move left");Localize("Move right");Localize("Jump");Localize("Fire");Localize("Hook");Localize("Hammer");
	Localize("Pistol");Localize("Shotgun");Localize("Grenade");Localize("Rifle");Localize("Next weapon");Localize("Prev. weapon");
	Localize("Vote yes");Localize("Vote no");Localize("Chat");Localize("Team chat");Localize("Show chat");Localize("Emoticon");
	Localize("Spectator mode");Localize("Spectate next");Localize("Spectate previous");Localize("Console");Localize("Remote console");Localize("Screenshot");Localize("Scoreboard");Localize("Respawn");
*/

const int g_KeyCount = sizeof(gs_aKeys) / sizeof(CKeyInfo);

void CMenus::UiDoGetButtons(int Start, int Stop, CUIRect View, CUIRect ScopeView)
{
	for(int i = Start; i < Stop; i++)
	{
		CKeyInfo &Key = gs_aKeys[i];
		CUIRect Button, Label;
		View.HSplitTop(20.0f, &Button, &View);
		Button.VSplitLeft(135.0f, &Label, &Button);

		if(Button.y < ScopeView.y || Button.y + Button.h > ScopeView.y + ScopeView.h)
			continue;

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", (const char *)Key.m_Name);

		UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
		int OldId = Key.m_KeyId;
		int NewId = DoKeyReader((void *)&gs_aKeys[i].m_Name, &Button, OldId);
		if(NewId != OldId)
		{
			if(OldId != 0 || NewId == 0)
				m_pClient->m_pBinds->Bind(OldId, "");
			if(NewId != 0)
				m_pClient->m_pBinds->Bind(NewId, gs_aKeys[i].m_pCommand);
		}
		View.HSplitTop(2.0f, 0, &View);
	}
}

void CMenus::RenderSettingsControls(CUIRect MainView)
{
	char aBuf[128];

	// this is kinda slow, but whatever
	for(int i = 0; i < g_KeyCount; i++)
		gs_aKeys[i].m_KeyId = 0;

	for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
	{
		const char *pBind = m_pClient->m_pBinds->Get(KeyId);
		if(!pBind[0])
			continue;

		for(int i = 0; i < g_KeyCount; i++)
			if(str_comp(pBind, gs_aKeys[i].m_pCommand) == 0)
			{
				gs_aKeys[i].m_KeyId = KeyId;
				break;
			}
	}

	// controls in a scrollable listbox
	static int s_ControlsList = 0;
	static int s_SelectedControl = -1;
	static float s_ScrollValue = 0;
	int OldSelected = s_SelectedControl;
	UiDoListboxStart(&s_ControlsList , &MainView, 475.0f, Localize("Controls"), "", 1, 1, s_SelectedControl, s_ScrollValue);

	CUIRect MovementSettings, WeaponSettings, VotingSettings, ChatSettings, MiscSettings, ResetButton;
	CListboxItem Item = UiDoListboxNextItem(&OldSelected, false, false);
	Item.m_Rect.HSplitTop(10.0f, 0, &Item.m_Rect);
	Item.m_Rect.VSplitMid(&MovementSettings, &VotingSettings);

	// movement settings
	{
		MovementSettings.VMargin(5.0f, &MovementSettings);
		MovementSettings.HSplitTop(450.0f, &MovementSettings, &WeaponSettings);
		RenderTools()->DrawUIRect(&MovementSettings, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		MovementSettings.VMargin(10.0f, &MovementSettings);

		TextRender()->Text(0, MovementSettings.x, MovementSettings.y, 14.0f*UI()->Scale(), Localize("Movement"), -1);

		MovementSettings.HSplitTop(14.0f+5.0f+10.0f, 0, &MovementSettings);

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Mouse sens."), g_Config.m_InpMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f*UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&g_Config.m_InpMousesens, &Button, (min(g_Config.m_InpMousesens, 500)-5)/500.0f)*500.0f)+5;
			if(g_Config.m_InpMousesens < 500 || NewValue < 500)
				g_Config.m_InpMousesens = min(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("UI mouse s."), g_Config.m_UiMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f*UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&g_Config.m_UiMousesens, &Button, (min(g_Config.m_UiMousesens, 500)-5)/500.0f)*500.0f)+5;
			if(g_Config.m_UiMousesens < 500 || NewValue < 500)
				g_Config.m_UiMousesens = min(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		UiDoGetButtons(0, 17, MovementSettings, MainView);

	}

	// weapon settings
	{
		WeaponSettings.HSplitTop(10.0f, 0, &WeaponSettings);
		WeaponSettings.HSplitTop(190.0f, &WeaponSettings, &ResetButton);
		RenderTools()->DrawUIRect(&WeaponSettings, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		WeaponSettings.VMargin(10.0f, &WeaponSettings);

		TextRender()->Text(0, WeaponSettings.x, WeaponSettings.y, 14.0f*UI()->Scale(), Localize("Weapon"), -1);

		WeaponSettings.HSplitTop(14.0f+5.0f+10.0f, 0, &WeaponSettings);
		UiDoGetButtons(17, 24, WeaponSettings, MainView);
	}

	// defaults
	{
		ResetButton.HSplitTop(10.0f, 0, &ResetButton);
		ResetButton.HSplitTop(40.0f, &ResetButton, 0);
		RenderTools()->DrawUIRect(&ResetButton, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		ResetButton.HMargin(10.0f, &ResetButton);
		ResetButton.VMargin(30.0f, &ResetButton);
		ResetButton.HSplitTop(20.0f, &ResetButton, 0);
		static int s_DefaultButton = 0;
		if(DoButton_Menu((void*)&s_DefaultButton, Localize("Reset to defaults"), 0, &ResetButton))
			m_pClient->m_pBinds->SetDefaults();
	}

	// voting settings
	{
		VotingSettings.VMargin(5.0f, &VotingSettings);
		VotingSettings.HSplitTop(80.0f, &VotingSettings, &ChatSettings);
		RenderTools()->DrawUIRect(&VotingSettings, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		VotingSettings.VMargin(10.0f, &VotingSettings);

		TextRender()->Text(0, VotingSettings.x, VotingSettings.y, 14.0f*UI()->Scale(), Localize("Voting"), -1);

		VotingSettings.HSplitTop(14.0f+5.0f+10.0f, 0, &VotingSettings);
		UiDoGetButtons(24, 26, VotingSettings, MainView);
	}

	// chat settings
	{
		ChatSettings.HSplitTop(10.0f, 0, &ChatSettings);
		ChatSettings.HSplitTop(125.0f, &ChatSettings, &MiscSettings);
		RenderTools()->DrawUIRect(&ChatSettings, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		ChatSettings.VMargin(10.0f, &ChatSettings);

		TextRender()->Text(0, ChatSettings.x, ChatSettings.y, 14.0f*UI()->Scale(), Localize("Chat"), -1);

		ChatSettings.HSplitTop(14.0f+5.0f+10.0f, 0, &ChatSettings);
		UiDoGetButtons(26, 30, ChatSettings, MainView);
	}

	// misc settings
	{
		MiscSettings.HSplitTop(10.0f, 0, &MiscSettings);
		MiscSettings.HSplitTop(300.0f, &MiscSettings, 0);
		RenderTools()->DrawUIRect(&MiscSettings, vec4(1,1,1,0.25f), CUI::CORNER_ALL, 10.0f);
		MiscSettings.VMargin(10.0f, &MiscSettings);

		TextRender()->Text(0, MiscSettings.x, MiscSettings.y, 14.0f*UI()->Scale(), Localize("Miscellaneous"), -1);

		MiscSettings.HSplitTop(14.0f+5.0f+10.0f, 0, &MiscSettings);
		UiDoGetButtons(30, 42, MiscSettings, MainView);
	}

	UiDoListboxEnd(&s_ScrollValue, 0);
}

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button, Label;
	char aBuf[128];
	bool CheckSettings = false;

	static const int MAX_RESOLUTIONS = 256;
	static CVideoMode s_aModes[MAX_RESOLUTIONS];
	static int s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	static int s_GfxScreenWidth = g_Config.m_GfxScreenWidth;
	static int s_GfxScreenHeight = g_Config.m_GfxScreenHeight;
	static int s_GfxColorDepth = g_Config.m_GfxColorDepth;
	static int s_GfxVsync = g_Config.m_GfxVsync;
	static int s_GfxFsaaSamples = g_Config.m_GfxFsaaSamples;
	static int s_GfxTextureQuality = g_Config.m_GfxTextureQuality;
	static int s_GfxTextureCompression = g_Config.m_GfxTextureCompression;

	CUIRect ModeList;
	MainView.VSplitLeft(300.0f, &MainView, &ModeList);

	// draw allmodes switch
	ModeList.HSplitTop(20, &Button, &ModeList);
	if(DoButton_CheckBox(&g_Config.m_GfxDisplayAllModes, Localize("Show only supported"), g_Config.m_GfxDisplayAllModes^1, &Button))
	{
		g_Config.m_GfxDisplayAllModes ^= 1;
		s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	}

	// display mode list
	static float s_ScrollValue = 0;
	int OldSelected = -1;
	int G = gcd(s_GfxScreenWidth, s_GfxScreenHeight);
	str_format(aBuf, sizeof(aBuf), "%s: %dx%d %d bit (%d:%d)", Localize("Current"), s_GfxScreenWidth, s_GfxScreenHeight, s_GfxColorDepth, s_GfxScreenWidth/G, s_GfxScreenHeight/G);
	UiDoListboxStart(&s_NumNodes , &ModeList, 24.0f, Localize("Display Modes"), aBuf, s_NumNodes, 1, OldSelected, s_ScrollValue);

	for(int i = 0; i < s_NumNodes; ++i)
	{
		const int Depth = s_aModes[i].m_Red+s_aModes[i].m_Green+s_aModes[i].m_Blue > 16 ? 24 : 16;
		if(g_Config.m_GfxColorDepth == Depth &&
			g_Config.m_GfxScreenWidth == s_aModes[i].m_Width &&
			g_Config.m_GfxScreenHeight == s_aModes[i].m_Height)
		{
			OldSelected = i;
		}

		CListboxItem Item = UiDoListboxNextItem(&s_aModes[i], OldSelected == i);
		if(Item.m_Visible)
		{
			int G = gcd(s_aModes[i].m_Width, s_aModes[i].m_Height);
			str_format(aBuf, sizeof(aBuf), " %dx%d %d bit (%d:%d)", s_aModes[i].m_Width, s_aModes[i].m_Height, Depth, s_aModes[i].m_Width/G, s_aModes[i].m_Height/G);
			UI()->DoLabelScaled(&Item.m_Rect, aBuf, 16.0f, -1);
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		const int Depth = s_aModes[NewSelected].m_Red+s_aModes[NewSelected].m_Green+s_aModes[NewSelected].m_Blue > 16 ? 24 : 16;
		g_Config.m_GfxColorDepth = Depth;
		g_Config.m_GfxScreenWidth = s_aModes[NewSelected].m_Width;
		g_Config.m_GfxScreenHeight = s_aModes[NewSelected].m_Height;
#if defined(SDL_VIDEO_DRIVER_X11)
		Graphics()->Resize(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);
#else
		CheckSettings = true;
#endif
	}

	// switches
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxBorderless, Localize("Borderless window"), g_Config.m_GfxBorderless, &Button))
	{
		Client()->ToggleWindowBordered();
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxFullscreen, Localize("Fullscreen"), g_Config.m_GfxFullscreen, &Button))
	{
		Client()->ToggleFullscreen();
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxVsync, Localize("V-Sync"), g_Config.m_GfxVsync, &Button))
	{
		Client()->ToggleWindowVSync();
	}

	if(Graphics()->GetNumScreens() > 1)
	{
		int NumScreens = Graphics()->GetNumScreens();
		MainView.HSplitTop(20.0f, &Button, &MainView);
		int Screen_MouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxScreen, Localize("Screen"), g_Config.m_GfxScreen, &Button);
		if(Screen_MouseButton == 1) //inc
		{
			Client()->SwitchWindowScreen((g_Config.m_GfxScreen+1)%NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		}
		else if(Screen_MouseButton == 2) //dec
		{
			Client()->SwitchWindowScreen((g_Config.m_GfxScreen-1+NumScreens)%NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	int GfxFsaaSamples_MouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxFsaaSamples, Localize("FSAA samples"), g_Config.m_GfxFsaaSamples, &Button);
	if(GfxFsaaSamples_MouseButton == 1) //inc
	{
		g_Config.m_GfxFsaaSamples = (g_Config.m_GfxFsaaSamples+1)%17;
		CheckSettings = true;
	}
	else if(GfxFsaaSamples_MouseButton == 2) //dec
	{
		g_Config.m_GfxFsaaSamples = (g_Config.m_GfxFsaaSamples-1 +17)%17;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxTextureQuality, Localize("Quality Textures"), g_Config.m_GfxTextureQuality, &Button))
	{
		g_Config.m_GfxTextureQuality ^= 1;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxTextureCompression, Localize("Texture Compression"), g_Config.m_GfxTextureCompression, &Button))
	{
		g_Config.m_GfxTextureCompression ^= 1;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighDetail, Localize("High Detail"), g_Config.m_GfxHighDetail, &Button))
		g_Config.m_GfxHighDetail ^= 1;

	// check if the new settings require a restart
	if(CheckSettings)
	{
		if(s_GfxScreenWidth == g_Config.m_GfxScreenWidth &&
			s_GfxScreenHeight == g_Config.m_GfxScreenHeight &&
			s_GfxColorDepth == g_Config.m_GfxColorDepth &&
			s_GfxVsync == g_Config.m_GfxVsync &&
			s_GfxFsaaSamples == g_Config.m_GfxFsaaSamples &&
			s_GfxTextureQuality == g_Config.m_GfxTextureQuality &&
			s_GfxTextureCompression == g_Config.m_GfxTextureCompression)
			m_NeedRestartGraphics = false;
		else
			m_NeedRestartGraphics = true;
	}

	MainView.HSplitTop(20.0f, &Label, &MainView);
	Label.VSplitLeft(130.0f, &Label, &Button);
	str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), g_Config.m_GfxRefreshRate);
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	Button.HMargin(2.0f, &Button);
	g_Config.m_GfxRefreshRate = static_cast<int>(DoScrollbarH(&g_Config.m_GfxRefreshRate, &Button, g_Config.m_GfxRefreshRate/1000.0f)*1000.0f+0.1f);

	CUIRect Text;
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Text, &MainView);
	//text.VSplitLeft(15.0f, 0, &text);
	UI()->DoLabelScaled(&Text, Localize("UI Color"), 14.0f, -1);

	const char *paLabels[] = {
		Localize("Hue"),
		Localize("Sat."),
		Localize("Lht."),
		Localize("Alpha")};
	int *pColorSlider[4] = {&g_Config.m_UiColorHue, &g_Config.m_UiColorSat, &g_Config.m_UiColorLht, &g_Config.m_UiColorAlpha};
	for(int s = 0; s < 4; s++)
	{
		CUIRect Text;
		MainView.HSplitTop(19.0f, &Button, &MainView);
		Button.VMargin(15.0f, &Button);
		Button.VSplitLeft(100.0f, &Text, &Button);
		//Button.VSplitRight(5.0f, &Button, 0);
		Button.HSplitTop(4.0f, 0, &Button);

		float k = (*pColorSlider[s]) / 255.0f;
		k = DoScrollbarH(pColorSlider[s], &Button, k);
		*pColorSlider[s] = (int)(k*255.0f);
		UI()->DoLabelScaled(&Text, paLabels[s], 15.0f, -1);
	}
}

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	CUIRect Button;
	MainView.VSplitMid(&MainView, 0);
	static int s_SndEnable = g_Config.m_SndEnable;
	static int s_SndRate = g_Config.m_SndRate;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndEnable, Localize("Use sounds"), g_Config.m_SndEnable, &Button))
	{
		g_Config.m_SndEnable ^= 1;
		if(g_Config.m_SndEnable)
		{
			if(g_Config.m_SndMusic && Client()->State() == IClient::STATE_OFFLINE)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		}
		else
			m_pClient->m_pSounds->Stop(SOUND_MENU);
		m_NeedRestartSound = g_Config.m_SndEnable && (!s_SndEnable || s_SndRate != g_Config.m_SndRate);
	}

	if(!g_Config.m_SndEnable)
		return;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndMusic, Localize("Play background music"), g_Config.m_SndMusic, &Button))
	{
		g_Config.m_SndMusic ^= 1;
		if(Client()->State() == IClient::STATE_OFFLINE)
		{
			if(g_Config.m_SndMusic)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
			else
				m_pClient->m_pSounds->Stop(SOUND_MENU);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndNonactiveMute, Localize("Mute when not active"), g_Config.m_SndNonactiveMute, &Button))
		g_Config.m_SndNonactiveMute ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndGame, Localize("Enable game sounds"), g_Config.m_SndGame, &Button))
		g_Config.m_SndGame ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndGun, Localize("Enable gun sound"), g_Config.m_SndGun, &Button))
		g_Config.m_SndGun ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndLongPain, Localize("Enable long pain sound (used when shooting in freeze)"), g_Config.m_SndLongPain, &Button))
		g_Config.m_SndLongPain ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndServerMessage, Localize("Enable server message sound"), g_Config.m_SndServerMessage, &Button))
		g_Config.m_SndServerMessage ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndChat, Localize("Enable regular chat sound"), g_Config.m_SndChat, &Button))
		g_Config.m_SndChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndTeamChat, Localize("Enable team chat sound"), g_Config.m_SndTeamChat, &Button))
		g_Config.m_SndTeamChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndHighlight, Localize("Enable highlighted chat sound"), g_Config.m_SndHighlight, &Button))
		g_Config.m_SndHighlight ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClThreadsoundloading, Localize("Threaded sound loading"), g_Config.m_ClThreadsoundloading, &Button))
		g_Config.m_ClThreadsoundloading ^= 1;

	// sample rate box
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", g_Config.m_SndRate);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		UI()->DoLabelScaled(&Button, Localize("Sample rate"), 14.0f, -1);
		Button.VSplitLeft(190.0f, 0, &Button);
		static float Offset = 0.0f;
		DoEditBox(&g_Config.m_SndRate, &Button, aBuf, sizeof(aBuf), 14.0f, &Offset);
		g_Config.m_SndRate = max(1, str_toint(aBuf));
		m_NeedRestartSound = !s_SndEnable || s_SndRate != g_Config.m_SndRate;
	}

	// volume slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sound volume"), 14.0f, -1);
		g_Config.m_SndVolume = (int)(DoScrollbarH(&g_Config.m_SndVolume, &Button, g_Config.m_SndVolume/100.0f)*100.0f);
	}

	// volume slider map sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Map sound volume"), 14.0f, -1);
		g_Config.m_SndMapSoundVolume = (int)(DoScrollbarH(&g_Config.m_SndMapSoundVolume, &Button, g_Config.m_SndMapSoundVolume/100.0f)*100.0f);
	}
}

class CLanguage
{
public:
	CLanguage() {}
	CLanguage(const char *n, const char *f, int Code) : m_Name(n), m_FileName(f), m_CountryCode(Code) {}

	string m_Name;
	string m_FileName;
	int m_CountryCode;

	bool operator<(const CLanguage &Other) { return m_Name < Other.m_Name; }
};

void LoadLanguageIndexfile(IStorage *pStorage, IConsole *pConsole, sorted_array<CLanguage> *pLanguages)
{
	IOHANDLE File = pStorage->OpenFile("languages/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "couldn't open index file");
		return;
	}

	char aOrigin[128];
	char aReplacement[128];
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aOrigin, pLine, sizeof(aOrigin));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aReplacement, pLine+3, sizeof(aReplacement));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		char aFileName[128];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aOrigin);
		pLanguages->add(CLanguage(aReplacement, aFileName, str_toint(pLine+3)));
	}
	io_close(File);
}

void CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_LanguageList = 0;
	static int s_SelectedLanguage = 0;
	static sorted_array<CLanguage> s_Languages;
	static float s_ScrollValue = 0;

	if(s_Languages.size() == 0)
	{
		s_Languages.add(CLanguage("English", "", 826));
		LoadLanguageIndexfile(Storage(), Console(), &s_Languages);
		for(int i = 0; i < s_Languages.size(); i++)
			if(str_comp(s_Languages[i].m_FileName, g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				break;
			}
	}

	int OldSelected = s_SelectedLanguage;

#if defined(__ANDROID__)
	UiDoListboxStart(&s_LanguageList , &MainView, 50.0f, Localize("Language"), "", s_Languages.size(), 1, s_SelectedLanguage, s_ScrollValue);
#else
	UiDoListboxStart(&s_LanguageList , &MainView, 24.0f, Localize("Language"), "", s_Languages.size(), 1, s_SelectedLanguage, s_ScrollValue);
#endif

	for(sorted_array<CLanguage>::range r = s_Languages.all(); !r.empty(); r.pop_front())
	{
		CListboxItem Item = UiDoListboxNextItem(&r.front());

		if(Item.m_Visible)
		{
			CUIRect Rect;
			Item.m_Rect.VSplitLeft(Item.m_Rect.h*2.0f, &Rect, &Item.m_Rect);
			Rect.VMargin(6.0f, &Rect);
			Rect.HMargin(3.0f, &Rect);
			vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
			m_pClient->m_pCountryFlags->Render(r.front().m_CountryCode, &Color, Rect.x, Rect.y, Rect.w, Rect.h);
			Item.m_Rect.HSplitTop(2.0f, 0, &Item.m_Rect);
			UI()->DoLabelScaled(&Item.m_Rect, r.front().m_Name, 16.0f, -1);
		}
	}

	s_SelectedLanguage = UiDoListboxEnd(&s_ScrollValue, 0);

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(g_Config.m_ClLanguagefile, s_Languages[s_SelectedLanguage].m_FileName, sizeof(g_Config.m_ClLanguagefile));
		g_Localization.Load(s_Languages[s_SelectedLanguage].m_FileName, Storage(), Console());

		// Load Font
		static CFont *pDefaultFont = 0;
		char aFilename[512];
		const char *pFontFile = "fonts/DejaVuSansCJKName.ttf";
		if (str_find(g_Config.m_ClLanguagefile, "chinese") != NULL || str_find(g_Config.m_ClLanguagefile, "japanese") != NULL ||
			str_find(g_Config.m_ClLanguagefile, "korean") != NULL)
			pFontFile = "fonts/DejavuWenQuanYiMicroHei.ttf";
		IOHANDLE File = Storage()->OpenFile(pFontFile, IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
		if(File)
		{
			io_close(File);
			pDefaultFont = TextRender()->LoadFont(aFilename);
			TextRender()->SetDefaultFont(pDefaultFont);
		}
		if(!pDefaultFont)
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load font. filename='%s'", pFontFile);
	}
}

void CMenus::RenderSettings(CUIRect MainView)
{
	static int s_SettingsPage = 0;

	// render background
	CUIRect Temp, TabBar, RestartWarning;
	MainView.VSplitRight(120.0f, &MainView, &TabBar);
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B|CUI::CORNER_TL, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitBottom(15.0f, &MainView, &RestartWarning);
	TabBar.HSplitTop(50.0f, &Temp, &TabBar);
	RenderTools()->DrawUIRect(&Temp, ms_ColorTabbarActive, CUI::CORNER_R, 10.0f);

	MainView.HSplitTop(10.0f, 0, &MainView);

	CUIRect Button;

	const char *aTabs[] = {
		Localize("Language"),
		Localize("General"),
		Localize("Player"),
		("Tee"),
		Localize("HUD"),
		Localize("Controls"),
		Localize("Graphics"),
		Localize("Sound"),
		Localize("DDNet")
	};

	int NumTabs = (int)(sizeof(aTabs)/sizeof(*aTabs));

	for(int i = 0; i < NumTabs; i++)
	{
		TabBar.HSplitTop(10, &Button, &TabBar);
		TabBar.HSplitTop(26, &Button, &TabBar);
		if(DoButton_MenuTab(aTabs[i], aTabs[i], s_SettingsPage == i, &Button, CUI::CORNER_R))
			s_SettingsPage = i;
	}

	MainView.Margin(10.0f, &MainView);

	if(s_SettingsPage == 0)
		RenderLanguageSelection(MainView);
	else if(s_SettingsPage == 1)
		RenderSettingsGeneral(MainView);
	else if(s_SettingsPage == 2)
		RenderSettingsPlayer(MainView);
	else if(s_SettingsPage == 3)
		RenderSettingsTee(MainView);
	else if(s_SettingsPage == 4)
		RenderSettingsHUD(MainView);
	else if(s_SettingsPage == 5)
		RenderSettingsControls(MainView);
	else if(s_SettingsPage == 6)
		RenderSettingsGraphics(MainView);
	else if(s_SettingsPage == 7)
		RenderSettingsSound(MainView);
	else if(s_SettingsPage == 8)
		RenderSettingsDDNet(MainView);

	if(m_NeedRestartUpdate)
	{
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		UI()->DoLabelScaled(&RestartWarning, Localize("DDNet Client needs to be restarted to complete update!"), 14.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(m_NeedRestartSkins || m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartDDNet)
		UI()->DoLabelScaled(&RestartWarning, Localize("You must restart the game for all settings to take effect."), 14.0f, -1);
}

void CMenus::RenderSettingsHUD(CUIRect MainView)
{
	static int pIDP1 = 0, pIDP2 = 0;
	static int Page = 1;
	CUIRect Left, Right, HUD, Messages, Button, Label, Weapon, Laser, Page1Tab, Page2Tab, Enable, Heart;

	MainView.HSplitTop(150.0f, &HUD, &MainView);

	HUD.HSplitTop(30.0f, &Label, &HUD);
	float tw = TextRender()->TextWidth(0, 20.0f, Localize("HUD"), -1);
	Label.VSplitLeft(tw + 10.0f, &Label, &Page1Tab);
	Page1Tab.VSplitLeft(60.0f, &Page1Tab, 0);
	Page1Tab.VSplitLeft(30.0f, &Page1Tab, &Page2Tab);

	UI()->DoLabelScaled(&Label, Localize("HUD"), 20.0f, -1);
	if(DoButton_MenuTab((void *)&pIDP1, "1", Page == 1, &Page1Tab, 5))
		Page = 1;
	if(DoButton_MenuTab((void *)&pIDP2, "2", Page == 2, &Page2Tab, 10))
		Page = 2;

	HUD.Margin(5.0f, &HUD);
	HUD.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	if(Page == 1)
	{
		// show hud1
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowhud, Localize("Show ingame HUD"), g_Config.m_ClShowhud, &Button))
			g_Config.m_ClShowhud ^= 1;


		Left.HSplitTop(20.0f, &Button, &Left);
		if (DoButton_CheckBox(&g_Config.m_ClDDRaceScoreBoard, Localize("Use DDRace Scoreboard"), g_Config.m_ClDDRaceScoreBoard, &Button))
		{
			g_Config.m_ClDDRaceScoreBoard ^= 1;
		}

		Left.HSplitTop(20.0f, &Button, &Left);
		if (DoButton_CheckBox(&g_Config.m_ClShowIDs, Localize("Show client IDs in Scoreboard"), g_Config.m_ClShowIDs, &Button))
		{
			g_Config.m_ClShowIDs ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if (DoButton_CheckBox(&g_Config.m_ClShowhudScore, Localize("Show score"), g_Config.m_ClShowhudScore, &Button))
		{
			g_Config.m_ClShowhudScore ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if (DoButton_CheckBox(&g_Config.m_ClShowhudHealthAmmo, Localize("Show health + ammo"), g_Config.m_ClShowhudHealthAmmo, &Button))
		{
			g_Config.m_ClShowhudHealthAmmo ^= 1;
		}

		Left.HSplitTop(20.0f, &Button, &Left);
		if (DoButton_CheckBox(&g_Config.m_ClShowChat, Localize("Show chat"), g_Config.m_ClShowChat, &Button))
		{
			g_Config.m_ClShowChat ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if (DoButton_CheckBox(&g_Config.m_ClChatTeamColors, Localize("Show names in chat in team colors"), g_Config.m_ClChatTeamColors, &Button))
		{
			g_Config.m_ClChatTeamColors ^= 1;
		}

		Left.HSplitTop(20.0f, &Button, &Left);
		if (DoButton_CheckBox(&g_Config.m_ClShowKillMessages, Localize("Show kill messages"), g_Config.m_ClShowKillMessages, &Button))
		{
			g_Config.m_ClShowKillMessages ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if (DoButton_CheckBox(&g_Config.m_ClShowVotesAfterVoting, Localize("Show votes window after voting"), g_Config.m_ClShowVotesAfterVoting, &Button))
		{
			g_Config.m_ClShowVotesAfterVoting ^= 1;
		}
		MainView.HSplitTop(170.0f, &Messages, &MainView);
		Messages.HSplitTop(30.0f, &Label, &Messages);
		Label.VSplitMid(&Label, &Button);
		UI()->DoLabelScaled(&Label, Localize("Messages"), 20.0f, -1);
		Messages.Margin(5.0f, &Messages);
		Messages.VSplitMid(&Left, &Right);
		Left.VSplitRight(5.0f, &Left, 0);
		Right.VMargin(5.0f, &Right);
		{
			char aBuf[64];
			Left.HSplitTop(20.0f, &Label, &Left);
			Label.VSplitRight(50.0f, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("System message"), 16.0f, -1);
			{
				static int s_DefaultButton = 0;
				if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
				{
					vec3 HSL = RgbToHsl(vec3(1.0f, 1.0f, 0.5f)); // default values
					g_Config.m_ClMessageSystemHue = HSL.h;
					g_Config.m_ClMessageSystemSat = HSL.s;
					g_Config.m_ClMessageSystemLht = HSL.l;
				}
			}
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Hue"), 14.0f, -1);
			g_Config.m_ClMessageSystemHue = (int)(DoScrollbarH(&g_Config.m_ClMessageSystemHue, &Button, g_Config.m_ClMessageSystemHue / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Sat."), 14.0f, -1);
			g_Config.m_ClMessageSystemSat = (int)(DoScrollbarH(&g_Config.m_ClMessageSystemSat, &Button, g_Config.m_ClMessageSystemSat / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Lht."), 14.0f, -1);
			g_Config.m_ClMessageSystemLht = (int)(DoScrollbarH(&g_Config.m_ClMessageSystemLht, &Button, g_Config.m_ClMessageSystemLht / 255.0f)*255.0f);

			Left.HSplitTop(10.0f, &Label, &Left);

			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageSystemHue / 255.0f, g_Config.m_ClMessageSystemSat / 255.0f, g_Config.m_ClMessageSystemLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, 1.0f);


			char name[16];
			str_copy(name, g_Config.m_PlayerName, sizeof(name));
			str_format(aBuf, sizeof(aBuf), "*** '%s' entered and joined the spectators", name);
			while (TextRender()->TextWidth(0, 12.0f, aBuf, -1) > Label.w)
			{
				name[str_length(name) - 1] = 0;
				str_format(aBuf, sizeof(aBuf), "*** '%s' entered and joined the spectators", name);
			}
			UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Left.HSplitTop(20.0f, 0, &Left);
		}
		{
			char aBuf[64];
			Right.HSplitTop(20.0f, &Label, &Right);
			Label.VSplitRight(50.0f, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Highlighted message"), 16.0f, -1);
			{
				static int s_DefaultButton = 0;
				if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
				{
					vec3 HSL = RgbToHsl(vec3(1.0f, 0.5f, 0.5f)); // default values
					g_Config.m_ClMessageHighlightHue = HSL.h;
					g_Config.m_ClMessageHighlightSat = HSL.s;
					g_Config.m_ClMessageHighlightLht = HSL.l;
				}
			}
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Hue"), 14.0f, -1);
			g_Config.m_ClMessageHighlightHue = (int)(DoScrollbarH(&g_Config.m_ClMessageHighlightHue, &Button, g_Config.m_ClMessageHighlightHue / 255.0f)*255.0f);

			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Sat."), 14.0f, -1);
			g_Config.m_ClMessageHighlightSat = (int)(DoScrollbarH(&g_Config.m_ClMessageHighlightSat, &Button, g_Config.m_ClMessageHighlightSat / 255.0f)*255.0f);

			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Lht."), 14.0f, -1);
			g_Config.m_ClMessageHighlightLht = (int)(DoScrollbarH(&g_Config.m_ClMessageHighlightLht, &Button, g_Config.m_ClMessageHighlightLht / 255.0f)*255.0f);

			Right.HSplitTop(10.0f, &Label, &Right);

			TextRender()->TextColor(0.75f, 0.5f, 0.75f, 1.0f);
			float tw = TextRender()->TextWidth(0, 12.0f, Localize("Spectator"), -1);
			Label.VSplitLeft(tw, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Spectator"), 12.0f, -1);

			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageHighlightHue / 255.0f, g_Config.m_ClMessageHighlightSat / 255.0f, g_Config.m_ClMessageHighlightLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, 1.0f);
			char name[16];
			str_copy(name, g_Config.m_PlayerName, sizeof(name));
			str_format(aBuf, sizeof(aBuf), ": %s: %s", name, Localize ("Look out!"));
			while (TextRender()->TextWidth(0, 12.0f, aBuf, -1) > Button.w)
			{
				name[str_length(name) - 1] = 0;
				str_format(aBuf, sizeof(aBuf), ": %s: %s", name, Localize("Look out!"));
			}
			UI()->DoLabelScaled(&Button, aBuf, 12.0f, -1);

			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Right.HSplitTop(20.0f, 0, &Right);
		}
		{
			char aBuf[64];
			Left.HSplitTop(20.0f, &Label, &Left);
			Label.VSplitRight(50.0f, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Team message"), 16.0f, -1);
			{
				static int s_DefaultButton = 0;
				if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
				{
					vec3 HSL = RgbToHsl(vec3(0.65f, 1.0f, 0.65f)); // default values
					g_Config.m_ClMessageTeamHue = HSL.h;
					g_Config.m_ClMessageTeamSat = HSL.s;
					g_Config.m_ClMessageTeamLht = HSL.l;
				}
			}
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Hue"), 14.0f, -1);
			g_Config.m_ClMessageTeamHue = (int)(DoScrollbarH(&g_Config.m_ClMessageTeamHue, &Button, g_Config.m_ClMessageTeamHue / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Sat."), 14.0f, -1);
			g_Config.m_ClMessageTeamSat = (int)(DoScrollbarH(&g_Config.m_ClMessageTeamSat, &Button, g_Config.m_ClMessageTeamSat / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Lht."), 14.0f, -1);
			g_Config.m_ClMessageTeamLht = (int)(DoScrollbarH(&g_Config.m_ClMessageTeamLht, &Button, g_Config.m_ClMessageTeamLht / 255.0f)*255.0f);

			Left.HSplitTop(10.0f, &Label, &Left);

			vec3 rgbn = CalculateNameColor(vec3(g_Config.m_ClMessageTeamHue / 255.0f, g_Config.m_ClMessageTeamSat / 255.0f, g_Config.m_ClMessageTeamLht / 255.0f));
			TextRender()->TextColor(rgbn.r, rgbn.g, rgbn.b, 1.0f);
			float tw = TextRender()->TextWidth(0, 12.0f, Localize("Player"), -1);
			Label.VSplitLeft(tw, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Player"), 12.0f, -1);

			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageTeamHue / 255.0f, g_Config.m_ClMessageTeamSat / 255.0f, g_Config.m_ClMessageTeamLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, 1.0f);
			str_format(aBuf, sizeof(aBuf), ": %s!",  Localize("We will win"));
			UI()->DoLabelScaled(&Button, aBuf, 12.0f, -1);

			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Left.HSplitTop(20.0f, 0, &Left);
		}
		{
			char aBuf[64];
			Right.HSplitTop(20.0f, &Label, &Right);
			Label.VSplitRight(50.0f, &Label, &Button);
			float twh = TextRender()->TextWidth(0, 16.0f, "Friend message", -1) ;
			Label.VSplitLeft(twh + 5.0f, &Label, &Enable);
			Enable.VSplitLeft(20.0f, &Enable, 0);
			UI()->DoLabelScaled(&Label, "Friend message", 16.0f, -1);
			{
				static int s_DefaultButton = 0;
				if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
				{
					g_Config.m_ClMessageFriendHue = 0;
					g_Config.m_ClMessageFriendSat = 255;
					g_Config.m_ClMessageFriendLht = 145;
				}
			}

			if(DoButton_CheckBox(&g_Config.m_ClMessageFriend, "", g_Config.m_ClMessageFriend, &Enable))
			{
				g_Config.m_ClMessageFriend ^= 1;
			}

			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Hue"), 14.0f, -1);
			g_Config.m_ClMessageFriendHue = (int)(DoScrollbarH(&g_Config.m_ClMessageFriendHue, &Button, g_Config.m_ClMessageFriendHue / 255.0f)*255.0f);

			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Sat."), 14.0f, -1);
			g_Config.m_ClMessageFriendSat = (int)(DoScrollbarH(&g_Config.m_ClMessageFriendSat, &Button, g_Config.m_ClMessageFriendSat / 255.0f)*255.0f);

			Right.HSplitTop(20.0f, &Button, &Right);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Lht."), 14.0f, -1);
			g_Config.m_ClMessageFriendLht = (int)(DoScrollbarH(&g_Config.m_ClMessageFriendLht, &Button, g_Config.m_ClMessageFriendLht / 255.0f)*255.0f);

			Right.HSplitTop(10.0f, &Label, &Right);

			vec3 rgbf = HslToRgb(vec3(g_Config.m_ClMessageFriendHue / 255.0f, g_Config.m_ClMessageFriendSat / 255.0f, g_Config.m_ClMessageFriendLht / 255.0f));
			TextRender()->TextColor(rgbf.r, rgbf.g, rgbf.b, 1.0f);
			float hw = TextRender()->TextWidth(0, 12.0f, "♥ ", -1);
			Label.VSplitLeft(hw, &Heart, &Label);
			UI()->DoLabelScaled(&Heart, "♥ ", 12.0f, -1);

			TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
			float tw = TextRender()->TextWidth(0, 12.0f, Localize("Friend"), -1);
			Label.VSplitLeft(tw, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Friend"), 12.0f, -1);

			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageHue / 255.0f, g_Config.m_ClMessageSat / 255.0f, g_Config.m_ClMessageLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, 1.0f);
			str_format(aBuf, sizeof(aBuf), ": %s", Localize("Hi o/"));
			UI()->DoLabelScaled(&Button, aBuf, 12.0f, -1);

			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Right.HSplitTop(20.0f, 0, &Right);
		}
		{
			char aBuf[64];
			Left.HSplitTop(20.0f, &Label, &Left);
			Label.VSplitRight(50.0f, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Normal message"), 16.0f, -1);
			{
				static int s_DefaultButton = 0;
				if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
				{
					vec3 HSL = RgbToHsl(vec3(1.0f, 1.0f, 1.0f)); // default values
					g_Config.m_ClMessageHue = HSL.h;
					g_Config.m_ClMessageSat = HSL.s;
					g_Config.m_ClMessageLht = HSL.l;
				}
			}
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Hue"), 14.0f, -1);
			g_Config.m_ClMessageHue = (int)(DoScrollbarH(&g_Config.m_ClMessageHue, &Button, g_Config.m_ClMessageHue / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Sat."), 14.0f, -1);
			g_Config.m_ClMessageSat = (int)(DoScrollbarH(&g_Config.m_ClMessageSat, &Button, g_Config.m_ClMessageSat / 255.0f)*255.0f);
			Left.HSplitTop(20.0f, &Button, &Left);
			Button.VSplitLeft(15.0f, 0, &Button);
			Button.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);
			UI()->DoLabelScaled(&Label, Localize("Lht."), 14.0f, -1);
			g_Config.m_ClMessageLht = (int)(DoScrollbarH(&g_Config.m_ClMessageLht, &Button, g_Config.m_ClMessageLht / 255.0f)*255.0f);

			Left.HSplitTop(10.0f, &Label, &Left);

			TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
			float tw = TextRender()->TextWidth(0, 12.0f, Localize("Player"), -1);
			Label.VSplitLeft(tw, &Label, &Button);
			UI()->DoLabelScaled(&Label, Localize("Player"), 12.0f, -1);

			vec3 rgb = HslToRgb(vec3(g_Config.m_ClMessageHue / 255.0f, g_Config.m_ClMessageSat / 255.0f, g_Config.m_ClMessageLht / 255.0f));
			TextRender()->TextColor(rgb.r, rgb.g, rgb.b, 1.0f);
			str_format(aBuf, sizeof(aBuf), ": %s :D", Localize("Hello and welcome"));
			UI()->DoLabelScaled(&Button, aBuf, 12.0f, -1);

			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	else if(Page == 2)
	{
		Left.HSplitTop(220.0f, &Laser, &Left);
		//RenderTools()->DrawUIRect(&Laser, vec4(1.0f, 1.0f, 1.0f, 0.1f), CUI::CORNER_ALL, 5.0f);
		//Laser.Margin(10.0f, &Laser);
		Laser.HSplitTop(30.0f, &Label, &Laser);
		Label.VSplitLeft(TextRender()->TextWidth(0, 20.0f, Localize("Laser"), -1) + 5.0f, &Label, &Weapon);
		UI()->DoLabelScaled(&Label, Localize("Laser"), 20.0f, -1);

		Laser.HSplitTop(20.0f, &Label, &Laser);
		Label.VSplitLeft(5.0f, 0, &Label);
		Label.VSplitRight(50.0f, &Label, &Button);
		UI()->DoLabelScaled(&Label, Localize("Inner color"), 16.0f, -1);
		{
			static int s_DefaultButton = 0;
			if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
			{
				vec3 HSL = RgbToHsl(vec3(0.5f, 0.5f, 1.0f)); // default values
				g_Config.m_ClLaserInnerHue = HSL.h;
				g_Config.m_ClLaserInnerSat = HSL.s;
				g_Config.m_ClLaserInnerLht = HSL.l;
			}
		}

		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(20.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Hue"), 12.0f, -1);
		g_Config.m_ClLaserInnerHue = (int)(DoScrollbarH(&g_Config.m_ClLaserInnerHue, &Button, g_Config.m_ClLaserInnerHue / 255.0f)*255.0f);
		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(20.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sat."), 12.0f, -1);
		g_Config.m_ClLaserInnerSat = (int)(DoScrollbarH(&g_Config.m_ClLaserInnerSat, &Button, g_Config.m_ClLaserInnerSat / 255.0f)*255.0f);
		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(20.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Lht."), 12.0f, -1);
		g_Config.m_ClLaserInnerLht = (int)(DoScrollbarH(&g_Config.m_ClLaserInnerLht, &Button, g_Config.m_ClLaserInnerLht / 255.0f)*255.0f);

		Laser.HSplitTop(10.0f, 0, &Laser);

		Laser.HSplitTop(20.0f, &Label, &Laser);
		Label.VSplitLeft(5.0f, 0, &Label);
		Label.VSplitRight(50.0f, &Label, &Button);
		UI()->DoLabelScaled(&Label, Localize("Outline color"), 16.0f, -1);
		{
			static int s_DefaultButton = 0;
			if (DoButton_Menu(&s_DefaultButton, Localize("Reset"), 0, &Button))
			{
				vec3 HSL = RgbToHsl(vec3(0.075f, 0.075f, 0.25f)); // default values
				g_Config.m_ClLaserOutlineHue = HSL.h;
				g_Config.m_ClLaserOutlineSat = HSL.s;
				g_Config.m_ClLaserOutlineLht = HSL.l;
			}
		}

		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(15.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Hue"), 12.0f, -1);
		g_Config.m_ClLaserOutlineHue = (int)(DoScrollbarH(&g_Config.m_ClLaserOutlineHue, &Button, g_Config.m_ClLaserOutlineHue / 255.0f)*255.0f);
		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(15.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sat."), 12.0f, -1);
		g_Config.m_ClLaserOutlineSat = (int)(DoScrollbarH(&g_Config.m_ClLaserOutlineSat, &Button, g_Config.m_ClLaserOutlineSat / 255.0f)*255.0f);
		Laser.HSplitTop(20.0f, &Button, &Laser);
		Button.VSplitLeft(15.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Lht."), 12.0f, -1);
		g_Config.m_ClLaserOutlineLht = (int)(DoScrollbarH(&g_Config.m_ClLaserOutlineLht, &Button, g_Config.m_ClLaserOutlineLht / 255.0f)*255.0f);


		//Laser.HSplitTop(8.0f, &Weapon, &Laser);
		Weapon.VSplitLeft(30.0f, 0, &Weapon);

		vec3 RGB;
		vec2 From = vec2(Weapon.x, Weapon.y + Weapon.h / 2.0f);
		vec2 Pos = vec2(Weapon.x + Weapon.w - 10.0f, Weapon.y + Weapon.h / 2.0f);

		vec2 Out, Border;

		Graphics()->BlendNormal();
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();

		// do outline
		RGB = HslToRgb(vec3(g_Config.m_ClLaserOutlineHue / 255.0f, g_Config.m_ClLaserOutlineSat / 255.0f, g_Config.m_ClLaserOutlineLht / 255.0f));
		vec4 OuterColor(RGB.r, RGB.g, RGB.b, 1.0f);
		Graphics()->SetColor(RGB.r, RGB.g, RGB.b, 1.0f); // outline
		Out = vec2(0.0f, -1.0f) * (3.15f);

		IGraphics::CFreeformItem Freeform(
			From.x - Out.x, From.y - Out.y,
			From.x + Out.x, From.y + Out.y,
			Pos.x - Out.x, Pos.y - Out.y,
			Pos.x + Out.x, Pos.y + Out.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		// do inner
		RGB = HslToRgb(vec3(g_Config.m_ClLaserInnerHue / 255.0f, g_Config.m_ClLaserInnerSat / 255.0f, g_Config.m_ClLaserInnerLht / 255.0f));
		vec4 InnerColor(RGB.r, RGB.g, RGB.b, 1.0f);
		Out = vec2(0.0f, -1.0f) * (2.25f);
		Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f); // center

		Freeform = IGraphics::CFreeformItem(
			From.x - Out.x, From.y - Out.y,
			From.x + Out.x, From.y + Out.y,
			Pos.x - Out.x, Pos.y - Out.y,
			Pos.x + Out.x, Pos.y + Out.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		Graphics()->QuadsEnd();

		// render head
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PARTICLES].m_Id);
			Graphics()->QuadsBegin();

			int Sprites[] = { SPRITE_PART_SPLAT01, SPRITE_PART_SPLAT02, SPRITE_PART_SPLAT03 };
			RenderTools()->SelectSprite(Sprites[time_get() % 3]);
			Graphics()->QuadsSetRotation(time_get());
			Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
			IGraphics::CQuadItem QuadItem(Pos.x, Pos.y, 24, 24);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
			QuadItem = IGraphics::CQuadItem(Pos.x, Pos.y, 20, 20);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
		// draw laser weapon
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();

		RenderTools()->SelectSprite(SPRITE_WEAPON_RIFLE_BODY);
		RenderTools()->DrawSprite(Weapon.x, Weapon.y + Weapon.h / 2.0f, 60.0f);

		Graphics()->QuadsEnd();
	}
	/*
	Left.VSplitLeft(20.0f, 0, &Left);
	Left.HSplitTop(20.0f, &Label, &Left);
	Button.VSplitRight(20.0f, &Button, 0);
	char aBuf[64];
	if (g_Config.m_ClReconnectTimeout == 1)
	{
		str_format(aBuf, sizeof(aBuf), "%s %i %s", Localize("Wait before try for"), g_Config.m_ClReconnectTimeout, Localize("second"));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s %i %s", Localize("Wait before try for"), g_Config.m_ClReconnectTimeout, Localize("seconds"));
	}
	UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
	Left.HSplitTop(20.0f, &Button, 0);
	Button.HMargin(2.0f, &Button);
	g_Config.m_ClReconnectTimeout = static_cast<int>(DoScrollbarH(&g_Config.m_ClReconnectTimeout, &Button, g_Config.m_ClReconnectTimeout / 120.0f) * 120.0f);
	if (g_Config.m_ClReconnectTimeout < 5)
		g_Config.m_ClReconnectTimeout = 5;*/
}

void CMenus::RenderSettingsDDNet(CUIRect MainView)
{
	CUIRect Button, Left, Right, LeftLeft, Demo, Gameplay, Miscellaneous, Label, Background;

	bool CheckSettings = false;
	static int s_InpMouseOld = g_Config.m_InpMouseOld;

	MainView.HSplitTop(100.0f, &Demo , &MainView);

	Demo.HSplitTop(30.0f, &Label, &Demo);
	UI()->DoLabelScaled(&Label, Localize("Demo"), 20.0f, -1);
	Demo.Margin(5.0f, &Demo);
	Demo.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClAutoRaceRecord, Localize("Save the best demo of each race"), g_Config.m_ClAutoRaceRecord, &Button))
	{
		g_Config.m_ClAutoRaceRecord ^= 1;
	}

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClRaceGhost, Localize("Ghost"), g_Config.m_ClRaceGhost, &Button))
	{
		g_Config.m_ClRaceGhost ^= 1;
	}

	if(g_Config.m_ClRaceGhost)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClRaceShowGhost, Localize("Show ghost"), g_Config.m_ClRaceShowGhost, &Button))
		{
			g_Config.m_ClRaceShowGhost ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClRaceSaveGhost, Localize("Save ghost"), g_Config.m_ClRaceSaveGhost, &Button))
		{
			g_Config.m_ClRaceSaveGhost ^= 1;
		}
	}

	MainView.HSplitTop(290.0f, &Gameplay , &MainView);

	Gameplay.HSplitTop(30.0f, &Label, &Gameplay);
	UI()->DoLabelScaled(&Label, Localize("Gameplay"), 20.0f, -1);
	Gameplay.Margin(5.0f, &Gameplay);
	Gameplay.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitLeft(120.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Overlay entities"), 14.0f, -1);
		g_Config.m_ClOverlayEntities = (int)(DoScrollbarH(&g_Config.m_ClOverlayEntities, &Button, g_Config.m_ClOverlayEntities/100.0f)*100.0f);
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitMid(&LeftLeft, &Button);

		Button.VSplitLeft(50.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Alpha"), 14.0f, -1);
		g_Config.m_ClShowOthersAlpha = (int)(DoScrollbarH(&g_Config.m_ClShowOthersAlpha, &Button, g_Config.m_ClShowOthersAlpha /100.0f)*100.0f);

		if(DoButton_CheckBox(&g_Config.m_ClShowOthers, Localize("Show others"), g_Config.m_ClShowOthers, &LeftLeft))
		{
			g_Config.m_ClShowOthers ^= 1;
		}
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowQuads, Localize("Show quads"), g_Config.m_ClShowQuads, &Button))
	{
		g_Config.m_ClShowQuads ^= 1;
	}

	Right.HSplitTop(20.0f, &Label, &Right);
	Label.VSplitLeft(130.0f, &Label, &Button);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Default zoom"), g_Config.m_ClDefaultZoom);
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	//Right.HSplitTop(20.0f, &Button, 0);
	Button.HMargin(2.0f, &Button);
	g_Config.m_ClDefaultZoom= static_cast<int>(DoScrollbarH(&g_Config.m_ClDefaultZoom, &Button, g_Config.m_ClDefaultZoom/20.0f)*20.0f+0.1f);

	Right.HSplitTop(20.0f, &Label, &Right);
	Label.VSplitLeft(130.0f, &Label, &Button);
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("AntiPing limit"), g_Config.m_ClAntiPingLimit);
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	//Right.HSplitTop(20.0f, &Button, 0);
	Button.HMargin(2.0f, &Button);
	g_Config.m_ClAntiPingLimit= static_cast<int>(DoScrollbarH(&g_Config.m_ClAntiPingLimit, &Button, g_Config.m_ClAntiPingLimit/200.0f)*200.0f+0.1f);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClAntiPing, Localize("AntiPing"), g_Config.m_ClAntiPing, &Button))
	{
		g_Config.m_ClAntiPing ^= 1;
	}

	if(g_Config.m_ClAntiPing)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingPlayers, Localize("AntiPing: predict other players"), g_Config.m_ClAntiPingPlayers, &Button))
		{
			g_Config.m_ClAntiPingPlayers ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingWeapons, Localize("AntiPing: predict weapons"), g_Config.m_ClAntiPingWeapons, &Button))
		{
			g_Config.m_ClAntiPingWeapons ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingGrenade, Localize("AntiPing: predict grenade paths"), g_Config.m_ClAntiPingGrenade, &Button))
		{
			g_Config.m_ClAntiPingGrenade ^= 1;
		}
	}
	else
	{
		Right.HSplitTop(60.0f, 0, &Right);
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowHookCollOther, Localize("Show other players' hook collision lines"), g_Config.m_ClShowHookCollOther, &Button))
	{
		g_Config.m_ClShowHookCollOther ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowDirection, Localize("Show other players' key presses"), g_Config.m_ClShowDirection, &Button))
	{
		g_Config.m_ClShowDirection ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_InpMouseOld, Localize("Old mouse mode"), g_Config.m_InpMouseOld, &Button))
	{
		g_Config.m_InpMouseOld ^= 1;
		CheckSettings = true;
	}

	if(CheckSettings)
	{
		if(s_InpMouseOld == g_Config.m_InpMouseOld)
			m_NeedRestartDDNet = false;
		else
			m_NeedRestartDDNet = true;
	}

	CUIRect aRects[2];
	Left.HSplitTop(5.0f, &Button, &Left);
	Right.HSplitTop(5.0f, &Button, &Right);
	aRects[0] = Left;
	aRects[1] = Right;
	aRects[0].VSplitRight(10.0f, &aRects[0], 0);
	aRects[1].VSplitLeft(10.0f, 0, &aRects[1]);

	int *pColorSlider[2][3] = {{&g_Config.m_ClBackgroundHue, &g_Config.m_ClBackgroundSat, &g_Config.m_ClBackgroundLht}, {&g_Config.m_ClBackgroundEntitiesHue, &g_Config.m_ClBackgroundEntitiesSat, &g_Config.m_ClBackgroundEntitiesLht}};

	const char *paParts[] = {
		Localize("Background (regular)"),
		Localize("Background (entities)")};
	const char *paLabels[] = {
		Localize("Hue"),
		Localize("Sat."),
		Localize("Lht.")};

	for(int i = 0; i < 2; i++)
	{
		aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
		UI()->DoLabelScaled(&Label, paParts[i], 14.0f, -1);
		aRects[i].VSplitLeft(20.0f, 0, &aRects[i]);
		aRects[i].HSplitTop(2.5f, 0, &aRects[i]);

		for(int s = 0; s < 3; s++)
		{
			aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
			Label.VSplitLeft(100.0f, &Label, &Button);
			Button.HMargin(2.0f, &Button);

			float k = (*pColorSlider[i][s]) / 255.0f;
			k = DoScrollbarH(pColorSlider[i][s], &Button, k);
			*pColorSlider[i][s] = (int)(k*255.0f);
			UI()->DoLabelScaled(&Label, paLabels[s], 15.0f, -1);
		}
	}

	{
		static float s_Map = 0.0f;
		aRects[1].HSplitTop(25.0f, &Background, &aRects[1]);
		Background.HSplitTop(20.0f, &Background, 0);
		Background.VSplitLeft(100.0f, &Label, &Left);
		UI()->DoLabelScaled(&Label, Localize("Map"), 14.0f, -1);
		DoEditBox(g_Config.m_ClBackgroundEntities, &Left, g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities), 14.0f, &s_Map);

		aRects[1].HSplitTop(20.0f, &Button, 0);
		if(DoButton_CheckBox(&g_Config.m_ClBackgroundShowTilesLayers, Localize("Show tiles layers from BG map"), g_Config.m_ClBackgroundShowTilesLayers, &Button))
		{
			g_Config.m_ClBackgroundShowTilesLayers ^= 1;
		}
	}

	MainView.HSplitTop(30.0f, &Label, &Miscellaneous);
	UI()->DoLabelScaled(&Label, Localize("Miscellaneous"), 20.0f, -1);
	Miscellaneous.VMargin(5.0f, &Miscellaneous);
	Miscellaneous.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClHttpMapDownload, Localize("Try fast HTTP map download first"), g_Config.m_ClHttpMapDownload, &Button))
	{
		g_Config.m_ClHttpMapDownload ^= 1;
	}

	//Updater
#if defined(CONF_FAMILY_WINDOWS) || (defined(CONF_PLATFORM_LINUX) && !defined(__ANDROID__))
	{
		Left.HSplitTop(20.0f, &Label, &Left);
		bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");
		char aBuf[256];
		int State = Updater()->GetCurrentState();

		//Update Button
		if(NeedUpdate && State <= IUpdater::CLEAN)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is available:"), Client()->LatestVersion());
			Label.VSplitLeft(TextRender()->TextWidth(0, 14.0f, aBuf, -1) + 10.0f, &Label, &Button);
			Button.VSplitLeft(100.0f, &Button, 0);
			static int s_ButtonUpdate = 0;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Update now"), 0, &Button))
				Updater()->InitiateUpdate();
		}
		else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
			str_format(aBuf, sizeof(aBuf), Localize("Updating..."));
		else if(State == IUpdater::NEED_RESTART)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet Client updated!"));
			m_NeedRestartUpdate = true;
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), Localize("No updates available"));
			Label.VSplitLeft(TextRender()->TextWidth(0, 14.0f, aBuf, -1) + 10.0f, &Label, &Button);
			Button.VSplitLeft(100.0f, &Button, 0);
			static int s_ButtonUpdate = 0;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Check now"), 0, &Button))
			{
				Client()->CheckVersionUpdate();
			}
		}
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
#endif

	{
		static int s_ButtonTimeout = 0;
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_Menu(&s_ButtonTimeout, Localize("New random timeout code"), 0, &Button))
		{
			Client()->GenerateTimeoutSeed();
		}
	}
}
