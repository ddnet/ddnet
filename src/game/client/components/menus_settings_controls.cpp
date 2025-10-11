/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_settings_controls.h"

#include <base/math.h>
#include <base/system.h>

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

#include <string>
#include <vector>

class CKeyInfo
{
public:
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
};

static CKeyInfo gs_aKeys[] = {
	{Localizable("Move left"), "+left", 0, 0},
	{Localizable("Move right"), "+right", 0, 0},
	{Localizable("Jump"), "+jump", 0, 0},
	{Localizable("Fire"), "+fire", 0, 0},
	{Localizable("Hook"), "+hook", 0, 0},
	{Localizable("Hook collisions"), "+showhookcoll", 0, 0},
	{Localizable("Pause"), "say /pause", 0, 0},
	{Localizable("Kill"), "kill", 0, 0},
	{Localizable("Zoom in"), "zoom+", 0, 0},
	{Localizable("Zoom out"), "zoom-", 0, 0},
	{Localizable("Default zoom"), "zoom", 0, 0},
	{Localizable("Show others"), "say /showothers", 0, 0},
	{Localizable("Show all"), "say /showall", 0, 0},
	{Localizable("Toggle dyncam"), "toggle cl_dyncam 0 1", 0, 0},
	{Localizable("Toggle ghost"), "toggle cl_race_show_ghost 0 1", 0, 0},

	{Localizable("Hammer"), "+weapon1", 0, 0},
	{Localizable("Pistol"), "+weapon2", 0, 0},
	{Localizable("Shotgun"), "+weapon3", 0, 0},
	{Localizable("Grenade"), "+weapon4", 0, 0},
	{Localizable("Laser"), "+weapon5", 0, 0},
	{Localizable("Next weapon"), "+nextweapon", 0, 0},
	{Localizable("Prev. weapon"), "+prevweapon", 0, 0},

	{Localizable("Vote yes"), "vote yes", 0, 0},
	{Localizable("Vote no"), "vote no", 0, 0},

	{Localizable("Chat"), "+show_chat; chat all", 0, 0},
	{Localizable("Team chat"), "+show_chat; chat team", 0, 0},
	{Localizable("Converse"), "+show_chat; chat all /c ", 0, 0},
	{Localizable("Chat command"), "+show_chat; chat all /", 0, 0},
	{Localizable("Show chat"), "+show_chat", 0, 0},

	{Localizable("Toggle dummy"), "toggle cl_dummy 0 1", 0, 0},
	{Localizable("Dummy copy"), "toggle cl_dummy_copy_moves 0 1", 0, 0},
	{Localizable("Hammerfly dummy"), "toggle cl_dummy_hammer 0 1", 0, 0},

	{Localizable("Emoticon"), "+emote", 0, 0},
	{Localizable("Spectator mode"), "+spectate", 0, 0},
	{Localizable("Spectate next"), "spectate_next", 0, 0},
	{Localizable("Spectate previous"), "spectate_previous", 0, 0},
	{Localizable("Console"), "toggle_local_console", 0, 0},
	{Localizable("Remote console"), "toggle_remote_console", 0, 0},
	{Localizable("Screenshot"), "screenshot", 0, 0},
	{Localizable("Scoreboard"), "+scoreboard", 0, 0},
	{Localizable("Statboard"), "+statboard", 0, 0},
	{Localizable("Lock team"), "say /lock", 0, 0},
	{Localizable("Show entities"), "toggle cl_overlay_entities 0 100", 0, 0},
	{Localizable("Show HUD"), "toggle cl_showhud 0 1", 0, 0},
};

void CMenusSettingsControls::Render(CUIRect MainView)
{
	// this is kinda slow, but whatever
	for(auto &Key : gs_aKeys)
		Key.m_KeyId = Key.m_ModifierCombination = 0;

	for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			for(auto &Key : gs_aKeys)
				if(str_comp(pBind, Key.m_pCommand) == 0)
				{
					Key.m_KeyId = KeyId;
					Key.m_ModifierCombination = Mod;
					break;
				}
		}
	}

	// scrollable controls
	static float s_JoystickSettingsHeight = 0.0f; // we calculate this later and don't render until enough space is available
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 120.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.y += ScrollOffset.y;

	const float FontSize = 14.0f;
	const float Margin = 10.0f;
	const float HeaderHeight = FontSize + 5.0f + Margin;

	CUIRect MouseSettings, MovementSettings, WeaponSettings, VotingSettings, ChatSettings, DummySettings, MiscSettings, JoystickSettings, ResetButton, Button;
	MainView.VSplitMid(&MouseSettings, &VotingSettings);

	// mouse settings
	{
		MouseSettings.VMargin(5.0f, &MouseSettings);
		MouseSettings.HSplitTop(80.0f, &MouseSettings, &JoystickSettings);
		if(s_ScrollRegion.AddRect(MouseSettings))
		{
			MouseSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MouseSettings.VMargin(10.0f, &MouseSettings);

			MouseSettings.HSplitTop(HeaderHeight, &Button, &MouseSettings);
			Ui()->DoLabel(&Button, Localize("Mouse"), FontSize, TEXTALIGN_ML);

			MouseSettings.HSplitTop(20.0f, &Button, &MouseSettings);
			Ui()->DoScrollbarOption(&g_Config.m_InpMousesens, &g_Config.m_InpMousesens, &Button, Localize("Ingame mouse sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

			MouseSettings.HSplitTop(2.0f, nullptr, &MouseSettings);

			MouseSettings.HSplitTop(20.0f, &Button, &MouseSettings);
			Ui()->DoScrollbarOption(&g_Config.m_UiMousesens, &g_Config.m_UiMousesens, &Button, Localize("UI mouse sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE);
		}
	}

	// joystick settings
	{
		JoystickSettings.HSplitTop(Margin, nullptr, &JoystickSettings);
		JoystickSettings.HSplitTop(s_JoystickSettingsHeight + HeaderHeight + Margin, &JoystickSettings, &MovementSettings);
		if(s_ScrollRegion.AddRect(JoystickSettings))
		{
			JoystickSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			JoystickSettings.VMargin(Margin, &JoystickSettings);

			JoystickSettings.HSplitTop(HeaderHeight, &Button, &JoystickSettings);
			Ui()->DoLabel(&Button, Localize("Controller"), FontSize, TEXTALIGN_ML);

			s_JoystickSettingsHeight = RenderSettingsJoystick(JoystickSettings);
		}
	}

	// movement settings
	{
		MovementSettings.HSplitTop(Margin, nullptr, &MovementSettings);
		MovementSettings.HSplitTop(365.0f, &MovementSettings, &WeaponSettings);
		if(s_ScrollRegion.AddRect(MovementSettings))
		{
			MovementSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MovementSettings.VMargin(Margin, &MovementSettings);

			MovementSettings.HSplitTop(HeaderHeight, &Button, &MovementSettings);
			Ui()->DoLabel(&Button, Localize("Movement"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(0, 15, MovementSettings);
		}
	}

	// weapon settings
	{
		WeaponSettings.HSplitTop(Margin, nullptr, &WeaponSettings);
		WeaponSettings.HSplitTop(190.0f, &WeaponSettings, &ResetButton);
		if(s_ScrollRegion.AddRect(WeaponSettings))
		{
			WeaponSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			WeaponSettings.VMargin(Margin, &WeaponSettings);

			WeaponSettings.HSplitTop(HeaderHeight, &Button, &WeaponSettings);
			Ui()->DoLabel(&Button, Localize("Weapon"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(15, 22, WeaponSettings);
		}
	}

	// defaults
	{
		ResetButton.HSplitTop(Margin, nullptr, &ResetButton);
		ResetButton.HSplitTop(40.0f, &ResetButton, nullptr);
		if(s_ScrollRegion.AddRect(ResetButton))
		{
			ResetButton.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			ResetButton.Margin(10.0f, &ResetButton);
			static CButtonContainer s_DefaultButton;
			if(GameClient()->m_Menus.DoButton_Menu(&s_DefaultButton, Localize("Reset to defaults"), 0, &ResetButton))
			{
				GameClient()->m_Menus.PopupConfirm(Localize("Reset controls"), Localize("Are you sure that you want to reset the controls to their defaults?"),
					Localize("Reset"), Localize("Cancel"), &CMenus::ResetSettingsControls);
			}
		}
	}

	// voting settings
	{
		VotingSettings.VMargin(5.0f, &VotingSettings);
		VotingSettings.HSplitTop(80.0f, &VotingSettings, &ChatSettings);
		if(s_ScrollRegion.AddRect(VotingSettings))
		{
			VotingSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			VotingSettings.VMargin(Margin, &VotingSettings);

			VotingSettings.HSplitTop(HeaderHeight, &Button, &VotingSettings);
			Ui()->DoLabel(&Button, Localize("Voting"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(22, 24, VotingSettings);
		}
	}

	// chat settings
	{
		ChatSettings.HSplitTop(Margin, nullptr, &ChatSettings);
		ChatSettings.HSplitTop(145.0f, &ChatSettings, &DummySettings);
		if(s_ScrollRegion.AddRect(ChatSettings))
		{
			ChatSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			ChatSettings.VMargin(Margin, &ChatSettings);

			ChatSettings.HSplitTop(HeaderHeight, &Button, &ChatSettings);
			Ui()->DoLabel(&Button, Localize("Chat"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(24, 29, ChatSettings);
		}
	}

	// dummy settings
	{
		DummySettings.HSplitTop(Margin, nullptr, &DummySettings);
		DummySettings.HSplitTop(100.0f, &DummySettings, &MiscSettings);
		if(s_ScrollRegion.AddRect(DummySettings))
		{
			DummySettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			DummySettings.VMargin(Margin, &DummySettings);

			DummySettings.HSplitTop(HeaderHeight, &Button, &DummySettings);
			Ui()->DoLabel(&Button, Localize("Dummy"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(29, 32, DummySettings);
		}
	}

	// misc settings
	{
		MiscSettings.HSplitTop(Margin, nullptr, &MiscSettings);
		MiscSettings.HSplitTop(300.0f, &MiscSettings, nullptr);
		if(s_ScrollRegion.AddRect(MiscSettings))
		{
			MiscSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MiscSettings.VMargin(Margin, &MiscSettings);

			MiscSettings.HSplitTop(HeaderHeight, &Button, &MiscSettings);
			Ui()->DoLabel(&Button, Localize("Miscellaneous"), FontSize, TEXTALIGN_ML);

			RenderSettingsBinds(32, 44, MiscSettings);
		}
	}

	s_ScrollRegion.End();
}

void CMenusSettingsControls::RenderSettingsBinds(int Start, int Stop, CUIRect View)
{
	for(int i = Start; i < Stop; i++)
	{
		const CKeyInfo &Key = gs_aKeys[i];

		CUIRect Button, Label;
		View.HSplitTop(20.0f, &Button, &View);
		Button.VSplitLeft(135.0f, &Label, &Button);

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize(Key.m_pName));

		Ui()->DoLabel(&Label, aBuf, 13.0f, TEXTALIGN_ML);
		int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
		int NewId = GameClient()->m_KeyBinder.DoKeyReader(&Key.m_KeyId, &Button, OldId, OldModifierCombination, &NewModifierCombination);
		if(NewId != OldId || NewModifierCombination != OldModifierCombination)
		{
			if(OldId != 0 || NewId == 0)
				GameClient()->m_Binds.Bind(OldId, "", false, OldModifierCombination);
			if(NewId != 0)
				GameClient()->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
		}

		View.HSplitTop(2.0f, nullptr, &View);
	}
}

float CMenusSettingsControls::RenderSettingsJoystick(CUIRect View)
{
	bool JoystickEnabled = g_Config.m_InpControllerEnable;
	int NumJoysticks = Input()->NumJoysticks();
	int NumOptions = 1; // expandable header
	if(JoystickEnabled)
	{
		NumOptions++; // message or joystick name/selection
		if(NumJoysticks > 0)
		{
			NumOptions += 3; // mode, ui sens, tolerance
			if(!g_Config.m_InpControllerAbsolute)
				NumOptions++; // ingame sens
			NumOptions += Input()->GetActiveJoystick()->GetNumAxes() + 1; // axis selection + header
		}
	}
	const float ButtonHeight = 20.0f;
	const float Spacing = 2.0f;
	const float BackgroundHeight = NumOptions * (ButtonHeight + Spacing) + (NumOptions == 1 ? 0.0f : Spacing);
	if(View.h < BackgroundHeight)
		return BackgroundHeight;

	View.HSplitTop(BackgroundHeight, &View, nullptr);

	CUIRect Button;
	View.HSplitTop(Spacing, nullptr, &View);
	View.HSplitTop(ButtonHeight, &Button, &View);
	if(GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_InpControllerEnable, Localize("Enable controller"), g_Config.m_InpControllerEnable, &Button))
	{
		g_Config.m_InpControllerEnable ^= 1;
	}
	if(JoystickEnabled)
	{
		if(NumJoysticks > 0)
		{
			// show joystick device selection if more than one available or just the joystick name if there is only one
			{
				CUIRect JoystickDropDown;
				View.HSplitTop(Spacing, nullptr, &View);
				View.HSplitTop(ButtonHeight, &JoystickDropDown, &View);
				if(NumJoysticks > 1)
				{
					static std::vector<std::string> s_vJoystickNames;
					static std::vector<const char *> s_vpJoystickNames;
					s_vJoystickNames.resize(NumJoysticks);
					s_vpJoystickNames.resize(NumJoysticks);

					for(int i = 0; i < NumJoysticks; ++i)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "%s %d: %s", Localize("Controller"), i, Input()->GetJoystick(i)->GetName());
						s_vJoystickNames[i] = aBuf;
						s_vpJoystickNames[i] = s_vJoystickNames[i].c_str();
					}

					static CUi::SDropDownState s_JoystickDropDownState;
					static CScrollRegion s_JoystickDropDownScrollRegion;
					s_JoystickDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_JoystickDropDownScrollRegion;
					const int CurrentJoystick = Input()->GetActiveJoystick()->GetIndex();
					const int NewJoystick = Ui()->DoDropDown(&JoystickDropDown, CurrentJoystick, s_vpJoystickNames.data(), s_vpJoystickNames.size(), s_JoystickDropDownState);
					if(NewJoystick != CurrentJoystick)
					{
						Input()->SetActiveJoystick(NewJoystick);
					}
				}
				else
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "%s 0: %s", Localize("Controller"), Input()->GetJoystick(0)->GetName());
					Ui()->DoLabel(&JoystickDropDown, aBuf, 13.0f, TEXTALIGN_ML);
				}
			}

			GameClient()->m_Menus.DoLine_RadioMenu(View, Localize("Ingame controller mode"),
				m_vButtonContainersJoystickAbsolute,
				{Localize("Relative", "Ingame controller mode"), Localize("Absolute", "Ingame controller mode")},
				{0, 1},
				g_Config.m_InpControllerAbsolute);

			if(!g_Config.m_InpControllerAbsolute)
			{
				View.HSplitTop(Spacing, nullptr, &View);
				View.HSplitTop(ButtonHeight, &Button, &View);
				Ui()->DoScrollbarOption(&g_Config.m_InpControllerSens, &g_Config.m_InpControllerSens, &Button, Localize("Ingame controller sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);
			}

			View.HSplitTop(Spacing, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoScrollbarOption(&g_Config.m_UiControllerSens, &g_Config.m_UiControllerSens, &Button, Localize("UI controller sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

			View.HSplitTop(Spacing, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoScrollbarOption(&g_Config.m_InpControllerTolerance, &g_Config.m_InpControllerTolerance, &Button, Localize("Controller jitter tolerance"), 0, 50);

			View.HSplitTop(Spacing, nullptr, &View);
			View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.125f), IGraphics::CORNER_ALL, 5.0f);
			RenderJoystickAxisPicker(View);
		}
		else
		{
			View.HSplitTop(View.h - ButtonHeight, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoLabel(&Button, Localize("No controller found. Plug in a controller."), 13.0f, TEXTALIGN_ML);
		}
	}

	return BackgroundHeight;
}

void CMenusSettingsControls::RenderJoystickAxisPicker(CUIRect View)
{
	const float FontSize = 13.0f;
	const float RowHeight = 20.0f;
	const float SpacingH = 2.0f;
	const float AxisWidth = 0.2f * View.w;
	const float StatusWidth = 0.4f * View.w;
	const float AimBindWidth = 90.0f;
	const float SpacingV = (View.w - AxisWidth - StatusWidth - AimBindWidth) / 2.0f;

	CUIRect Row, Axis, Status, AimBind;
	View.HSplitTop(SpacingH, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(AxisWidth, &Axis, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(StatusWidth, &Status, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

	Ui()->DoLabel(&Axis, Localize("Axis"), FontSize, TEXTALIGN_MC);
	Ui()->DoLabel(&Status, Localize("Status"), FontSize, TEXTALIGN_MC);
	Ui()->DoLabel(&AimBind, Localize("Aim bind"), FontSize, TEXTALIGN_MC);

	IInput::IJoystick *pJoystick = Input()->GetActiveJoystick();
	static int s_aActive[NUM_JOYSTICK_AXES][2];
	for(int i = 0; i < minimum<int>(pJoystick->GetNumAxes(), NUM_JOYSTICK_AXES); i++)
	{
		const bool Active = g_Config.m_InpControllerX == i || g_Config.m_InpControllerY == i;

		View.HSplitTop(SpacingH, nullptr, &View);
		View.HSplitTop(RowHeight, &Row, &View);
		Row.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.125f), IGraphics::CORNER_ALL, 5.0f);
		Row.VSplitLeft(AxisWidth, &Axis, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(StatusWidth, &Status, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

		// Axis label
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d", i + 1);
		if(Active)
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		else
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		Ui()->DoLabel(&Axis, aBuf, FontSize, TEXTALIGN_MC);

		// Axis status
		Status.HMargin(7.0f, &Status);
		RenderJoystickBar(&Status, (pJoystick->GetAxisValue(i) + 1.0f) / 2.0f, g_Config.m_InpControllerTolerance / 50.0f, Active);

		// Bind to X/Y
		CUIRect AimBindX, AimBindY;
		AimBind.VSplitMid(&AimBindX, &AimBindY);
		if(GameClient()->m_Menus.DoButton_CheckBox(&s_aActive[i][0], "X", g_Config.m_InpControllerX == i, &AimBindX))
		{
			if(g_Config.m_InpControllerY == i)
				g_Config.m_InpControllerY = g_Config.m_InpControllerX;
			g_Config.m_InpControllerX = i;
		}
		if(GameClient()->m_Menus.DoButton_CheckBox(&s_aActive[i][1], "Y", g_Config.m_InpControllerY == i, &AimBindY))
		{
			if(g_Config.m_InpControllerX == i)
				g_Config.m_InpControllerX = g_Config.m_InpControllerY;
			g_Config.m_InpControllerY = i;
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
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
