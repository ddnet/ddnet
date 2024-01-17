/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/editor.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>

#include <engine/client/updater.h>

#include <game/client/components/binds.h>
#include <game/client/components/console.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

#include "menus.h"

using namespace FontIcons;
using namespace std::chrono_literals;

ColorRGBA CMenus::ms_GuiColor;
ColorRGBA CMenus::ms_ColorTabbarInactiveOutgame;
ColorRGBA CMenus::ms_ColorTabbarActiveOutgame;
ColorRGBA CMenus::ms_ColorTabbarHoverOutgame;
ColorRGBA CMenus::ms_ColorTabbarInactive;
ColorRGBA CMenus::ms_ColorTabbarActive = ColorRGBA(0, 0, 0, 0.5f);
ColorRGBA CMenus::ms_ColorTabbarHover;
ColorRGBA CMenus::ms_ColorTabbarInactiveIngame;
ColorRGBA CMenus::ms_ColorTabbarActiveIngame;
ColorRGBA CMenus::ms_ColorTabbarHoverIngame;

float CMenus::ms_ButtonHeight = 25.0f;
float CMenus::ms_ListheaderHeight = 17.0f;

CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_INTERNET;
	m_MenuPage = 0;
	m_GamePage = PAGE_GAME;
	m_JoinTutorial = false;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
	m_NeedSendinfo = false;
	m_NeedSendDummyinfo = false;
	m_MenuActive = true;
	m_ShowStart = true;

	str_copy(m_aCurrentDemoFolder, "demos");
	m_DemolistStorageType = IStorage::TYPE_ALL;

	m_DemoPlayerState = DEMOPLAYER_NONE;
	m_Dummy = false;

	m_ServerProcess.m_Process = INVALID_PROCESS;

	for(SUIAnimator &animator : m_aAnimatorsSettingsTab)
	{
		animator.m_YOffset = -2.5f;
		animator.m_HOffset = 5.0f;
		animator.m_WOffset = 5.0f;
		animator.m_RepositionLabel = true;
	}

	for(SUIAnimator &animator : m_aAnimatorsBigPage)
	{
		animator.m_YOffset = -5.0f;
		animator.m_HOffset = 5.0f;
	}

	for(SUIAnimator &animator : m_aAnimatorsSmallPage)
	{
		animator.m_YOffset = -2.5f;
		animator.m_HOffset = 2.5f;
	}

	m_PasswordInput.SetBuffer(g_Config.m_Password, sizeof(g_Config.m_Password));
	m_PasswordInput.SetHidden(true);
}

int CMenus::DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIBUTTONS].m_Id);
	Graphics()->QuadsBegin();
	if(!Active)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(Checked ? SPRITE_GUIBUTTON_ON : SPRITE_GUIBUTTON_OFF);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	if(UI()->HotItem() == pID && Active)
	{
		RenderTools()->SelectSprite(SPRITE_GUIBUTTON_HOVER);
		QuadItem = IGraphics::CQuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();

	return Active ? UI()->DoButtonLogic(pID, Checked, pRect) : 0;
}

int CMenus::DoButton_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color)
{
	CUIRect Text = *pRect;

	if(Checked)
		Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	Color.a *= UI()->ButtonColorMul(pButtonContainer);

	pRect->Draw(Color, Corners, Rounding);

	if(pImageName)
	{
		CUIRect Image;
		pRect->VSplitRight(pRect->h * 4.0f, &Text, &Image); // always correct ratio for image

		// render image
		const CMenuImage *pImage = FindMenuImage(pImageName);
		if(pImage)
		{
			Graphics()->TextureSet(UI()->HotItem() == pButtonContainer ? pImage->m_OrgTexture : pImage->m_GreyTexture);
			Graphics()->WrapClamp();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(Image.x, Image.y, Image.w, Image.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
	}

	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	UI()->DoLabel(&Text, pText, Text.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);

	return UI()->DoButtonLogic(pButtonContainer, Checked, pRect);
}

int CMenus::DoButton_MenuTab(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, SUIAnimator *pAnimator, const ColorRGBA *pDefaultColor, const ColorRGBA *pActiveColor, const ColorRGBA *pHoverColor, float EdgeRounding)
{
	const bool MouseInside = UI()->HotItem() == pButtonContainer;
	CUIRect Rect = *pRect;

	if(pAnimator != NULL)
	{
		auto Time = time_get_nanoseconds();

		if(pAnimator->m_Time + 100ms < Time)
		{
			pAnimator->m_Value = pAnimator->m_Active ? 1 : 0;
			pAnimator->m_Time = Time;
		}

		pAnimator->m_Active = Checked || MouseInside;

		if(pAnimator->m_Active)
			pAnimator->m_Value = clamp<float>(pAnimator->m_Value + (Time - pAnimator->m_Time).count() / (double)std::chrono::nanoseconds(100ms).count(), 0, 1);
		else
			pAnimator->m_Value = clamp<float>(pAnimator->m_Value - (Time - pAnimator->m_Time).count() / (double)std::chrono::nanoseconds(100ms).count(), 0, 1);

		Rect.w += pAnimator->m_Value * pAnimator->m_WOffset;
		Rect.h += pAnimator->m_Value * pAnimator->m_HOffset;
		Rect.x += pAnimator->m_Value * pAnimator->m_XOffset;
		Rect.y += pAnimator->m_Value * pAnimator->m_YOffset;

		pAnimator->m_Time = Time;
	}

	if(Checked)
	{
		ColorRGBA ColorMenuTab = ms_ColorTabbarActive;
		if(pActiveColor)
			ColorMenuTab = *pActiveColor;

		Rect.Draw(ColorMenuTab, Corners, EdgeRounding);
	}
	else
	{
		if(MouseInside)
		{
			ColorRGBA HoverColorMenuTab = ms_ColorTabbarHover;
			if(pHoverColor)
				HoverColorMenuTab = *pHoverColor;

			Rect.Draw(HoverColorMenuTab, Corners, EdgeRounding);
		}
		else
		{
			ColorRGBA ColorMenuTab = ms_ColorTabbarInactive;
			if(pDefaultColor)
				ColorMenuTab = *pDefaultColor;

			Rect.Draw(ColorMenuTab, Corners, EdgeRounding);
		}
	}

	if(pAnimator != NULL)
	{
		if(pAnimator->m_RepositionLabel)
		{
			Rect.x += Rect.w - pRect->w + Rect.x - pRect->x;
			Rect.y += Rect.h - pRect->h + Rect.y - pRect->y;
		}

		if(!pAnimator->m_ScaleLabel)
		{
			Rect.w = pRect->w;
			Rect.h = pRect->h;
		}
	}

	CUIRect Temp;
	Rect.HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);

	return UI()->DoButtonLogic(pButtonContainer, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	if(Checked == 2)
		pRect->Draw(ColorRGBA(1, 0.98f, 0.5f, 0.55f), IGraphics::CORNER_T, 5.0f);
	else if(Checked)
		pRect->Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_T, 5.0f);

	CUIRect Temp;
	pRect->VSplitLeft(5.0f, nullptr, &Temp);
	UI()->DoLabel(&Temp, pText, pRect->h * CUI::ms_FontmodHeight, TEXTALIGN_ML);
	return UI()->DoButtonLogic(pID, Checked, pRect);
}

int CMenus::DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect)
{
	CUIRect Box, Label;
	pRect->VSplitLeft(pRect->h, &Box, &Label);
	Label.VSplitLeft(5.0f, nullptr, &Label);

	Box.Margin(2.0f, &Box);
	Box.Draw(ColorRGBA(1, 1, 1, 0.25f * UI()->ButtonColorMul(pID)), IGraphics::CORNER_ALL, 3.0f);

	const bool Checkable = *pBoxText == 'X';
	if(Checkable)
	{
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		UI()->DoLabel(&Box, FONT_ICON_XMARK, Box.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
	else
		UI()->DoLabel(&Box, pBoxText, Box.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);

	TextRender()->SetRenderFlags(0);
	UI()->DoLabel(&Label, pText, Box.h * CUI::ms_FontmodHeight, TEXTALIGN_ML);

	return UI()->DoButtonLogic(pID, 0, pRect);
}

void CMenus::DoLaserPreview(const CUIRect *pRect, const ColorHSLA LaserOutlineColor, const ColorHSLA LaserInnerColor, const int LaserType)
{
	ColorRGBA LaserRGB;
	CUIRect Section = *pRect;
	vec2 From = vec2(Section.x + 50.0f, Section.y + Section.h / 2.0f);
	vec2 Pos = vec2(Section.x + Section.w - 10.0f, Section.y + Section.h / 2.0f);

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	LaserRGB = color_cast<ColorRGBA, ColorHSLA>(LaserOutlineColor);
	ColorRGBA OuterColor(LaserRGB.r, LaserRGB.g, LaserRGB.b, 1.0f);
	Graphics()->SetColor(LaserRGB.r, LaserRGB.g, LaserRGB.b, 1.0f);
	vec2 Out = vec2(0.0f, -1.0f) * (3.15f);

	IGraphics::CFreeformItem Freeform(From.x - Out.x, From.y - Out.y, From.x + Out.x, From.y + Out.y, Pos.x - Out.x, Pos.y - Out.y, Pos.x + Out.x, Pos.y + Out.y);
	Graphics()->QuadsDrawFreeform(&Freeform, 1);

	LaserRGB = color_cast<ColorRGBA, ColorHSLA>(LaserInnerColor);
	ColorRGBA InnerColor(LaserRGB.r, LaserRGB.g, LaserRGB.b, 1.0f);
	Out = vec2(0.0f, -1.0f) * (2.25f);
	Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);

	Freeform = IGraphics::CFreeformItem(From.x - Out.x, From.y - Out.y, From.x + Out.x, From.y + Out.y, Pos.x - Out.x, Pos.y - Out.y, Pos.x + Out.x, Pos.y + Out.y);
	Graphics()->QuadsDrawFreeform(&Freeform, 1);
	Graphics()->QuadsEnd();

	Graphics()->BlendNormal();
	int SpriteIndex = time_get() % 3;
	Graphics()->TextureSet(GameClient()->m_ParticlesSkin.m_aSpriteParticleSplat[SpriteIndex]);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(time_get());
	Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
	IGraphics::CQuadItem QuadItem(Pos.x, Pos.y, 24, 24);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
	QuadItem = IGraphics::CQuadItem(Pos.x, Pos.y, 20, 20);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();

	switch(LaserType)
	{
	case LASERTYPE_RIFLE:
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponLaser);
		RenderTools()->SelectSprite(SPRITE_WEAPON_LASER_BODY);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);
		Graphics()->QuadsEnd();
		break;
	case LASERTYPE_SHOTGUN:
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponShotgun);
		RenderTools()->SelectSprite(SPRITE_WEAPON_SHOTGUN_BODY);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);
		Graphics()->QuadsEnd();
		break;
	default:
		Graphics()->QuadsBegin();
		Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
		QuadItem = IGraphics::CQuadItem(From.x, From.y, 24, 24);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
		QuadItem = IGraphics::CQuadItem(From.x, From.y, 20, 20);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
}

ColorHSLA CMenus::DoLine_ColorPicker(CButtonContainer *pResetID, const float LineSize, const float LabelSize, const float BottomMargin, CUIRect *pMainRect, const char *pText, unsigned int *pColorValue, const ColorRGBA DefaultColor, bool CheckBoxSpacing, int *pCheckBoxValue, bool Alpha)
{
	CUIRect Section, ColorPickerButton, ResetButton, Label;

	pMainRect->HSplitTop(LineSize, &Section, pMainRect);
	pMainRect->HSplitTop(BottomMargin, nullptr, pMainRect);

	if(CheckBoxSpacing || pCheckBoxValue != nullptr)
	{
		CUIRect CheckBox;
		Section.VSplitLeft(Section.h, &CheckBox, &Section);
		if(pCheckBoxValue != nullptr)
		{
			CheckBox.Margin(2.0f, &CheckBox);
			if(DoButton_CheckBox(pCheckBoxValue, "", *pCheckBoxValue, &CheckBox))
				*pCheckBoxValue ^= 1;
		}
		Section.VSplitLeft(5.0f, nullptr, &Section);
	}

	Section.VSplitMid(&Label, &Section, 4.0f);
	Section.VSplitRight(60.0f, &Section, &ResetButton);
	Section.VSplitRight(8.0f, &Section, nullptr);
	Section.VSplitRight(Section.h, &Section, &ColorPickerButton);

	UI()->DoLabel(&Label, pText, LabelSize, TEXTALIGN_ML);

	ColorHSLA PickedColor = DoButton_ColorPicker(&ColorPickerButton, pColorValue, Alpha);

	ResetButton.HMargin(2.0f, &ResetButton);
	if(DoButton_Menu(pResetID, Localize("Reset"), 0, &ResetButton, nullptr, IGraphics::CORNER_ALL, 4.0f, 0.1f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)))
	{
		*pColorValue = color_cast<ColorHSLA>(DefaultColor).Pack(Alpha);
	}

	return PickedColor;
}

ColorHSLA CMenus::DoButton_ColorPicker(const CUIRect *pRect, unsigned int *pHslaColor, bool Alpha)
{
	ColorHSLA HslaColor = ColorHSLA(*pHslaColor, Alpha);

	ColorRGBA Outline = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);
	Outline.a *= UI()->ButtonColorMul(pHslaColor);

	CUIRect Rect;
	pRect->Margin(3.0f, &Rect);

	pRect->Draw(Outline, IGraphics::CORNER_ALL, 4.0f);
	Rect.Draw(color_cast<ColorRGBA>(HslaColor), IGraphics::CORNER_ALL, 4.0f);

	static CUI::SColorPickerPopupContext s_ColorPickerPopupContext;
	if(UI()->DoButtonLogic(pHslaColor, 0, pRect))
	{
		s_ColorPickerPopupContext.m_pHslaColor = pHslaColor;
		s_ColorPickerPopupContext.m_HslaColor = HslaColor;
		s_ColorPickerPopupContext.m_HsvaColor = color_cast<ColorHSVA>(HslaColor);
		s_ColorPickerPopupContext.m_RgbaColor = color_cast<ColorRGBA>(s_ColorPickerPopupContext.m_HsvaColor);
		s_ColorPickerPopupContext.m_Alpha = Alpha;
		UI()->ShowPopupColorPicker(UI()->MouseX(), UI()->MouseY(), &s_ColorPickerPopupContext);
	}
	else if(UI()->IsPopupOpen(&s_ColorPickerPopupContext) && s_ColorPickerPopupContext.m_pHslaColor == pHslaColor)
	{
		HslaColor = color_cast<ColorHSLA>(s_ColorPickerPopupContext.m_HsvaColor);
	}

	return HslaColor;
}

int CMenus::DoButton_CheckBoxAutoVMarginAndSet(const void *pID, const char *pText, int *pValue, CUIRect *pRect, float VMargin)
{
	CUIRect CheckBoxRect;
	pRect->HSplitTop(VMargin, &CheckBoxRect, pRect);

	int Logic = DoButton_CheckBox_Common(pID, pText, *pValue ? "X" : "", &CheckBoxRect);

	if(Logic)
		*pValue ^= 1;

	return Logic;
}

int CMenus::DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButton_CheckBox_Common(pID, pText, Checked ? "X" : "", pRect);
}

int CMenus::DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	char aBuf[16];
	str_from_int(Checked, aBuf);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoKeyReader(const void *pID, const CUIRect *pRect, int Key, int ModifierCombination, int *pNewModifierCombination)
{
	// process
	static const void *s_pGrabbedID = nullptr;
	static bool s_MouseReleased = true;
	static int s_ButtonUsed = 0;
	const bool Inside = UI()->MouseHovered(pRect);
	int NewKey = Key;
	*pNewModifierCombination = ModifierCombination;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && s_pGrabbedID == pID)
		s_MouseReleased = true;

	if(UI()->CheckActiveItem(pID))
	{
		if(m_Binder.m_GotKey)
		{
			// abort with escape key
			if(m_Binder.m_Key.m_Key != KEY_ESCAPE)
			{
				NewKey = m_Binder.m_Key.m_Key;
				*pNewModifierCombination = m_Binder.m_ModifierCombination;
			}
			m_Binder.m_GotKey = false;
			UI()->SetActiveItem(nullptr);
			s_MouseReleased = false;
			s_pGrabbedID = pID;
		}

		if(s_ButtonUsed == 1 && !UI()->MouseButton(1))
		{
			if(Inside)
				NewKey = 0;
			UI()->SetActiveItem(nullptr);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(s_MouseReleased)
		{
			if(UI()->MouseButton(0))
			{
				m_Binder.m_TakeKey = true;
				m_Binder.m_GotKey = false;
				UI()->SetActiveItem(pID);
				s_ButtonUsed = 0;
			}

			if(UI()->MouseButton(1))
			{
				UI()->SetActiveItem(pID);
				s_ButtonUsed = 1;
			}
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	char aBuf[64];
	if(UI()->CheckActiveItem(pID) && s_ButtonUsed == 0)
		str_copy(aBuf, Localize("Press a key…"));
	else if(NewKey == 0)
		aBuf[0] = '\0';
	else
		str_format(aBuf, sizeof(aBuf), "%s%s", CBinds::GetKeyBindModifiersName(*pNewModifierCombination), Input()->KeyName(NewKey));

	const ColorRGBA Color = UI()->CheckActiveItem(pID) && m_Binder.m_TakeKey ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.4f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * UI()->ButtonColorMul(pID));
	pRect->Draw(Color, IGraphics::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	UI()->DoLabel(&Temp, aBuf, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);

	return NewKey;
}

int CMenus::RenderMenubar(CUIRect r)
{
	CUIRect Box = r;
	CUIRect Button;

	m_ActivePage = m_MenuPage;
	int NewPage = -1;

	if(Client()->State() != IClient::STATE_OFFLINE)
		m_ActivePage = m_GamePage;

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		Box.VSplitLeft(33.0f, &Button, &Box);
		static CButtonContainer s_StartButton;

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		bool GotNewsOrUpdate = false;

#if defined(CONF_AUTOUPDATE)
		int State = Updater()->GetCurrentState();
		bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");
		if(State == IUpdater::CLEAN && NeedUpdate)
		{
			GotNewsOrUpdate = true;
		}
#endif

		GotNewsOrUpdate |= (bool)g_Config.m_UiUnreadNews;

		ColorRGBA HomeButtonColorAlert(0, 1, 0, 0.25f);
		ColorRGBA HomeButtonColorAlertHover(0, 1, 0, 0.5f);
		ColorRGBA *pHomeButtonColor = nullptr;
		ColorRGBA *pHomeButtonColorHover = nullptr;

		const char *pHomeScreenButtonLabel = FONT_ICON_HOUSE;
		if(GotNewsOrUpdate)
		{
			pHomeScreenButtonLabel = FONT_ICON_NEWSPAPER;
			pHomeButtonColor = &HomeButtonColorAlert;
			pHomeButtonColorHover = &HomeButtonColorAlertHover;
		}

		if(DoButton_MenuTab(&s_StartButton, pHomeScreenButtonLabel, false, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_HOME], pHomeButtonColor, pHomeButtonColor, pHomeButtonColorHover, 10.0f))
		{
			m_ShowStart = true;
		}

		Box.VSplitLeft(10.0f, nullptr, &Box);
		Box.VSplitLeft(75.0f, &Button, &Box);
		static CButtonContainer s_InternetButton;
		if(DoButton_MenuTab(&s_InternetButton, FONT_ICON_EARTH_AMERICAS, m_ActivePage == PAGE_INTERNET, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_INTERNET]))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_INTERNET)
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_FAVORITES)
					Client()->RequestDDNetInfo();
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			}
			NewPage = PAGE_INTERNET;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_InternetButton, &Button, Localize("Internet"));

		Box.VSplitLeft(75.0f, &Button, &Box);
		static CButtonContainer s_LanButton;
		if(DoButton_MenuTab(&s_LanButton, FONT_ICON_NETWORK_WIRED, m_ActivePage == PAGE_LAN, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_LAN]))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_LAN)
				ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
			NewPage = PAGE_LAN;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_LanButton, &Button, Localize("LAN"));

		Box.VSplitLeft(75.0f, &Button, &Box);
		static CButtonContainer s_FavoritesButton;
		if(DoButton_MenuTab(&s_FavoritesButton, FONT_ICON_STAR, m_ActivePage == PAGE_FAVORITES, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_FAVORITES]))
		{
			if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_FAVORITES)
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_INTERNET)
					Client()->RequestDDNetInfo();
				ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
			}
			NewPage = PAGE_FAVORITES;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_FavoritesButton, &Button, Localize("Favorites"));

		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
	else
	{
		// online menus
		Box.VSplitLeft(90.0f, &Button, &Box);
		static CButtonContainer s_GameButton;
		if(DoButton_MenuTab(&s_GameButton, Localize("Game"), m_ActivePage == PAGE_GAME, &Button, IGraphics::CORNER_TL))
			NewPage = PAGE_GAME;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static CButtonContainer s_PlayersButton;
		if(DoButton_MenuTab(&s_PlayersButton, Localize("Players"), m_ActivePage == PAGE_PLAYERS, &Button, 0))
			NewPage = PAGE_PLAYERS;

		Box.VSplitLeft(130.0f, &Button, &Box);
		static CButtonContainer s_ServerInfoButton;
		if(DoButton_MenuTab(&s_ServerInfoButton, Localize("Server info"), m_ActivePage == PAGE_SERVER_INFO, &Button, 0))
			NewPage = PAGE_SERVER_INFO;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static CButtonContainer s_NetworkButton;
		if(DoButton_MenuTab(&s_NetworkButton, Localize("Browser"), m_ActivePage == PAGE_NETWORK, &Button, 0))
			NewPage = PAGE_NETWORK;

		{
			static CButtonContainer s_GhostButton;
			if(GameClient()->m_GameInfo.m_Race)
			{
				Box.VSplitLeft(90.0f, &Button, &Box);
				if(DoButton_MenuTab(&s_GhostButton, Localize("Ghost"), m_ActivePage == PAGE_GHOST, &Button, 0))
					NewPage = PAGE_GHOST;
			}
		}

		Box.VSplitLeft(100.0f, &Button, &Box);
		Box.VSplitLeft(4.0f, nullptr, &Box);
		static CButtonContainer s_CallVoteButton;
		if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage == PAGE_CALLVOTE, &Button, IGraphics::CORNER_TR))
		{
			NewPage = PAGE_CALLVOTE;
			m_ControlPageOpening = true;
		}
	}

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_QuitButton;
	ColorRGBA QuitColor(1, 0, 0, 0.5f);
	if(DoButton_MenuTab(&s_QuitButton, FONT_ICON_POWER_OFF, 0, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_QUIT], nullptr, nullptr, &QuitColor, 10.0f))
	{
		if(m_pClient->Editor()->HasUnsavedData() || (Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
		{
			m_Popup = POPUP_QUIT;
		}
		else
		{
			Client()->Quit();
		}
	}

	Box.VSplitRight(10.0f, &Box, nullptr);
	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_SettingsButton;
	if(DoButton_MenuTab(&s_SettingsButton, FONT_ICON_GEAR, m_ActivePage == PAGE_SETTINGS, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_SETTINGS], nullptr, nullptr, nullptr, 10.0f))
		NewPage = PAGE_SETTINGS;

	Box.VSplitRight(10.0f, &Box, nullptr);
	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_EditorButton;
	if(DoButton_MenuTab(&s_EditorButton, FONT_ICON_PEN_TO_SQUARE, 0, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_EDITOR], nullptr, nullptr, nullptr, 10.0f))
	{
		g_Config.m_ClEditor = 1;
	}

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		Box.VSplitRight(10.0f, &Box, nullptr);
		Box.VSplitRight(33.0f, &Box, &Button);
		static CButtonContainer s_DemoButton;
		if(DoButton_MenuTab(&s_DemoButton, FONT_ICON_CLAPPERBOARD, m_ActivePage == PAGE_DEMOS, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_DEMOBUTTON], nullptr, nullptr, nullptr, 10.0f))
			NewPage = PAGE_DEMOS;
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	if(NewPage != -1)
	{
		if(Client()->State() == IClient::STATE_OFFLINE)
			SetMenuPage(NewPage);
		else
			m_GamePage = NewPage;
	}

	return 0;
}

void CMenus::RenderLoading(const char *pCaption, const char *pContent, int IncreaseCounter, bool RenderLoadingBar, bool RenderMenuBackgroundMap)
{
	// TODO: not supported right now due to separate render thread

	static std::chrono::nanoseconds s_LastLoadRender{0};
	const int CurLoadRenderCount = m_LoadCurrent;
	m_LoadCurrent += IncreaseCounter;
	const float Percent = CurLoadRenderCount / (float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get_nanoseconds() - s_LastLoadRender < std::chrono::nanoseconds(1s) / 60l)
		return;

	s_LastLoadRender = time_get_nanoseconds();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	UI()->MapScreen();

	if(!RenderMenuBackgroundMap || !m_pBackground->Render())
	{
		RenderBackground();
	}

	CUIRect Box = *UI()->Screen();
	Box.Margin(160.0f, &Box);

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Box.Draw(ColorRGBA{0, 0, 0, 0.50f}, IGraphics::CORNER_ALL, 15.0f);

	CUIRect Part;
	Box.HSplitTop(20.f, nullptr, &Box);
	Box.HSplitTop(24.f, &Part, &Box);
	Part.VMargin(20.f, &Part);
	UI()->DoLabel(&Part, pCaption, 24.f, TEXTALIGN_MC);

	Box.HSplitTop(20.f, nullptr, &Box);
	Box.HSplitTop(24.f, &Part, &Box);
	Part.VMargin(20.f, &Part);
	UI()->DoLabel(&Part, pContent, 20.0f, TEXTALIGN_MC);

	if(RenderLoadingBar)
		Graphics()->DrawRect(Box.x + 40, Box.y + Box.h - 75, (Box.w - 80) * Percent, 25, ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f), IGraphics::CORNER_ALL, 5.0f);

	Client()->UpdateAndSwap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	g_Config.m_UiUnreadNews = false;

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.VSplitLeft(15.0f, nullptr, &MainView);

	CUIRect Label;

	const char *pStr = Client()->News();
	char aLine[256];
	while((pStr = str_next_token(pStr, "\n", aLine, sizeof(aLine))))
	{
		const int Len = str_length(aLine);
		if(Len > 0 && aLine[0] == '|' && aLine[Len - 1] == '|')
		{
			MainView.HSplitTop(30.0f, &Label, &MainView);
			aLine[Len - 1] = '\0';
			UI()->DoLabel(&Label, aLine + 1, 20.0f, TEXTALIGN_ML);
		}
		else
		{
			MainView.HSplitTop(20.0f, &Label, &MainView);
			UI()->DoLabel(&Label, aLine, 15.f, TEXTALIGN_ML);
		}
	}
}

void CMenus::OnInit()
{
	if(g_Config.m_ClShowWelcome)
	{
		m_Popup = POPUP_LANGUAGE;
		str_copy(g_Config.m_BrFilterExcludeCommunities, "*ddnet");
	}
	if(g_Config.m_ClSkipStartMenu)
		m_ShowStart = false;

	m_RefreshButton.Init(UI(), -1);
	m_ConnectButton.Init(UI(), -1);

	Console()->Chain("add_favorite", ConchainFavoritesUpdate, this);
	Console()->Chain("remove_favorite", ConchainFavoritesUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("br_filter_exclude_communities", ConchainCommunitiesUpdate, this);

	Console()->Chain("snd_enable", ConchainUpdateMusicState, this);
	Console()->Chain("snd_enable_music", ConchainUpdateMusicState, this);

	Console()->Chain("cl_assets_entities", ConchainAssetsEntities, this);
	Console()->Chain("cl_asset_game", ConchainAssetGame, this);
	Console()->Chain("cl_asset_emoticons", ConchainAssetEmoticons, this);
	Console()->Chain("cl_asset_particles", ConchainAssetParticles, this);
	Console()->Chain("cl_asset_hud", ConchainAssetHud, this);
	Console()->Chain("cl_asset_extras", ConchainAssetExtras, this);

	m_TextureBlob = Graphics()->LoadTexture("blob.png", IStorage::TYPE_ALL);

	// setup load amount
	const int NumMenuImages = 5;
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages + NumMenuImages + GameClient()->ComponentCount();
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;

	m_IsInit = true;

	// load menu images
	m_vMenuImages.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "menuimages", MenuImageScan, this);

	// load community icons
	m_vCommunityIcons.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "communityicons", CommunityIconScan, this);
}

void CMenus::OnConsoleInit()
{
	ConfigManager()->RegisterCallback(CMenus::ConfigSaveCallback, this);
	Console()->Register("add_favorite_skin", "s[skin_name]", CFGFLAG_CLIENT, Con_AddFavoriteSkin, this, "Add a skin as a favorite");
	Console()->Register("remove_favorite_skin", "s[skin_name]", CFGFLAG_CLIENT, Con_RemFavoriteSkin, this, "Remove a skin from the favorites");
}

void CMenus::ConchainUpdateMusicState(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	auto *pSelf = (CMenus *)pUserData;
	if(pResult->NumArguments())
		pSelf->UpdateMusicState();
}

void CMenus::UpdateMusicState()
{
	const bool ShouldPlay = Client()->State() == IClient::STATE_OFFLINE && g_Config.m_SndEnable && g_Config.m_SndMusic;
	if(ShouldPlay && !m_pClient->m_Sounds.IsPlaying(SOUND_MENU))
		m_pClient->m_Sounds.Enqueue(CSounds::CHN_MUSIC, SOUND_MENU);
	else if(!ShouldPlay && m_pClient->m_Sounds.IsPlaying(SOUND_MENU))
		m_pClient->m_Sounds.Stop(SOUND_MENU);
}

void CMenus::PopupMessage(const char *pTitle, const char *pMessage, const char *pButtonLabel, int NextPopup, FPopupButtonCallback pfnButtonCallback)
{
	// reset active item
	UI()->SetActiveItem(nullptr);

	str_copy(m_aPopupTitle, pTitle);
	str_copy(m_aPopupMessage, pMessage);
	str_copy(m_aPopupButtons[BUTTON_CONFIRM].m_aLabel, pButtonLabel);
	m_aPopupButtons[BUTTON_CONFIRM].m_NextPopup = NextPopup;
	m_aPopupButtons[BUTTON_CONFIRM].m_pfnCallback = pfnButtonCallback;
	m_Popup = POPUP_MESSAGE;
}

void CMenus::PopupConfirm(const char *pTitle, const char *pMessage, const char *pConfirmButtonLabel, const char *pCancelButtonLabel,
	FPopupButtonCallback pfnConfirmButtonCallback, int ConfirmNextPopup, FPopupButtonCallback pfnCancelButtonCallback, int CancelNextPopup)
{
	// reset active item
	UI()->SetActiveItem(nullptr);

	str_copy(m_aPopupTitle, pTitle);
	str_copy(m_aPopupMessage, pMessage);
	str_copy(m_aPopupButtons[BUTTON_CONFIRM].m_aLabel, pConfirmButtonLabel);
	m_aPopupButtons[BUTTON_CONFIRM].m_NextPopup = ConfirmNextPopup;
	m_aPopupButtons[BUTTON_CONFIRM].m_pfnCallback = pfnConfirmButtonCallback;
	str_copy(m_aPopupButtons[BUTTON_CANCEL].m_aLabel, pCancelButtonLabel);
	m_aPopupButtons[BUTTON_CANCEL].m_NextPopup = CancelNextPopup;
	m_aPopupButtons[BUTTON_CANCEL].m_pfnCallback = pfnCancelButtonCallback;
	m_Popup = POPUP_CONFIRM;
}

void CMenus::PopupWarning(const char *pTopic, const char *pBody, const char *pButton, std::chrono::nanoseconds Duration)
{
	// no multiline support for console
	std::string BodyStr = pBody;
	while(BodyStr.find('\n') != std::string::npos)
		BodyStr.replace(BodyStr.find('\n'), 1, " ");
	dbg_msg(pTopic, "%s", BodyStr.c_str());

	// reset active item
	UI()->SetActiveItem(nullptr);

	str_copy(m_aMessageTopic, pTopic);
	str_copy(m_aMessageBody, pBody);
	str_copy(m_aMessageButton, pButton);
	m_Popup = POPUP_WARNING;
	SetActive(true);

	m_PopupWarningDuration = Duration;
	m_PopupWarningLastTime = time_get_nanoseconds();
}

bool CMenus::CanDisplayWarning() const
{
	return m_Popup == POPUP_NONE;
}

int CMenus::Render()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && m_Popup == POPUP_NONE)
		return 0;

	CUIRect Screen = *UI()->Screen();
	UI()->MapScreen();
	UI()->ResetMouseSlow();

	static int s_Frame = 0;
	if(s_Frame == 0)
	{
		SetMenuPage(g_Config.m_UiPage);
		s_Frame++;
	}
	else if(s_Frame == 1)
	{
		UpdateMusicState();
		RefreshBrowserTab(g_Config.m_UiPage);
		s_Frame++;
	}

	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveIngame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveIngame;
		ms_ColorTabbarHover = ms_ColorTabbarHoverIngame;
	}
	else
	{
		if(!m_pBackground->Render())
		{
			RenderBackground();
		}
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveOutgame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveOutgame;
		ms_ColorTabbarHover = ms_ColorTabbarHoverOutgame;
	}

	CUIRect TabBar;
	CUIRect MainView;

	// some margin around the screen
	Screen.Margin(10.0f, &Screen);

	static bool s_SoundCheck = false;
	if(!s_SoundCheck && m_Popup == POPUP_NONE)
	{
		if(Client()->SoundInitFailed())
			PopupMessage(Localize("Sound error"), Localize("The audio device couldn't be initialised."), Localize("Ok"));
		s_SoundCheck = true;
	}

	if(m_Popup == POPUP_NONE)
	{
		if(m_JoinTutorial && !Client()->InfoTaskRunning() && !ServerBrowser()->IsGettingServerlist())
		{
			m_JoinTutorial = false;
			const char *pAddr = ServerBrowser()->GetTutorialServer();
			if(pAddr)
				Client()->Connect(pAddr);
		}
		if(m_ShowStart && Client()->State() == IClient::STATE_OFFLINE)
		{
			m_pBackground->ChangePosition(CMenuBackground::POS_START);
			RenderStartMenu(Screen);
		}
		else
		{
			Screen.HSplitTop(24.0f, &TabBar, &MainView);

			// render news
			if(m_MenuPage < PAGE_NEWS || m_MenuPage > PAGE_SETTINGS || (Client()->State() == IClient::STATE_OFFLINE && m_MenuPage >= PAGE_GAME && m_MenuPage <= PAGE_CALLVOTE))
			{
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				SetMenuPage(PAGE_INTERNET);
			}

			// render current page
			if(Client()->State() != IClient::STATE_OFFLINE)
			{
				if(m_GamePage == PAGE_GAME)
				{
					RenderGame(MainView);
					RenderIngameHint();
				}
				else if(m_GamePage == PAGE_PLAYERS)
				{
					RenderPlayers(MainView);
				}
				else if(m_GamePage == PAGE_SERVER_INFO)
				{
					RenderServerInfo(MainView);
				}
				else if(m_GamePage == PAGE_NETWORK)
				{
					RenderInGameNetwork(MainView);
				}
				else if(m_GamePage == PAGE_GHOST)
				{
					RenderGhost(MainView);
				}
				else if(m_GamePage == PAGE_CALLVOTE)
				{
					RenderServerControl(MainView);
				}
				else if(m_GamePage == PAGE_SETTINGS)
				{
					RenderSettings(MainView);
				}
			}
			else if(m_MenuPage == PAGE_NEWS)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_NEWS);
				RenderNews(MainView);
			}
			else if(m_MenuPage == PAGE_INTERNET)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_INTERNET);
				RenderServerbrowser(MainView);
			}
			else if(m_MenuPage == PAGE_LAN)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_LAN);
				RenderServerbrowser(MainView);
			}
			else if(m_MenuPage == PAGE_DEMOS)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_DEMOS);
				RenderDemoBrowser(MainView);
			}
			else if(m_MenuPage == PAGE_FAVORITES)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_FAVORITES);
				RenderServerbrowser(MainView);
			}
			else if(m_MenuPage == PAGE_SETTINGS)
				RenderSettings(MainView);

			// do tab bar
			RenderMenubar(TabBar);
		}
	}
	else
	{
		char aBuf[1536];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		bool TopAlign = false;
		bool UseIpLabel = false;

		ColorRGBA BgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);
		if(m_Popup == POPUP_MESSAGE || m_Popup == POPUP_CONFIRM)
		{
			pTitle = m_aPopupTitle;
			pExtraText = m_aPopupMessage;
			TopAlign = true;
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			pTitle = Localize("Connecting to");
			UseIpLabel = true;
			pButtonText = Localize("Abort");
			if(Client()->State() == IClient::STATE_CONNECTING && time_get() - Client()->StateStartTime() > time_freq())
			{
				int Connectivity = Client()->UdpConnectivity(Client()->ConnectNetTypes());
				switch(Connectivity)
				{
				case IClient::CONNECTIVITY_UNKNOWN:
					break;
				case IClient::CONNECTIVITY_CHECKING:
					pExtraText = Localize("Trying to determine UDP connectivity...");
					break;
				case IClient::CONNECTIVITY_UNREACHABLE:
					pExtraText = Localize("UDP seems to be filtered.");
					break;
				case IClient::CONNECTIVITY_DIFFERING_UDP_TCP_IP_ADDRESSES:
					pExtraText = Localize("UDP and TCP IP addresses seem to be different. Try disabling VPN, proxy or network accelerators.");
					break;
				case IClient::CONNECTIVITY_REACHABLE:
					pExtraText = Localize("No answer from server yet.");
					break;
				}
			}
			else if(Client()->MapDownloadTotalsize() > 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Downloading map"), Client()->MapDownloadName());
				pTitle = aBuf;
				UseIpLabel = false;
			}
			else if(Client()->State() == IClient::STATE_LOADING)
			{
				UseIpLabel = false;
				if(Client()->LoadingStateDetail() == IClient::LOADING_STATE_DETAIL_INITIAL)
				{
					pTitle = Localize("Connected");
					pExtraText = Localize("Getting game info");
				}
				else if(Client()->LoadingStateDetail() == IClient::LOADING_STATE_DETAIL_LOADING_MAP)
				{
					pTitle = Localize("Connected");
					pExtraText = Localize("Loading map file from storage");
				}
				else if(Client()->LoadingStateDetail() == IClient::LOADING_STATE_DETAIL_SENDING_READY)
				{
					pTitle = Localize("Connected");
					pExtraText = Localize("Requesting to join the game");
				}
				else if(Client()->LoadingStateDetail() == IClient::LOADING_STATE_DETAIL_GETTING_READY)
				{
					pTitle = Localize("Connected");
					pExtraText = Localize("Sending initial client info");
				}
			}
		}
		else if(m_Popup == POPUP_DISCONNECTED)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Client()->ErrorString();
			pButtonText = Localize("Ok");
			if(Client()->ReconnectTime() > 0)
			{
				str_format(aBuf, sizeof(aBuf), Localize("Reconnect in %d sec"), (int)((Client()->ReconnectTime() - time_get()) / time_freq()));
				pTitle = Client()->ErrorString();
				pExtraText = aBuf;
				pButtonText = Localize("Abort");
			}
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			dbg_assert(m_DemolistSelectedIndex >= 0, "m_DemolistSelectedIndex invalid for POPUP_RENAME_DEMO");
			pTitle = m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? Localize("Rename folder") : Localize("Rename demo");
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			pTitle = Localize("Render demo");
		}
		else if(m_Popup == POPUP_RENDER_DONE)
		{
			pTitle = Localize("Render complete");
		}
#endif
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_RESTART)
		{
			pTitle = Localize("Restart");
			pExtraText = Localize("Are you sure that you want to restart?");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to DDNet");
			str_format(aBuf, sizeof(aBuf), "%s\n\n%s\n\n%s\n\n%s",
				Localize("DDraceNetwork is a cooperative online game where the goal is for you and your group of tees to reach the finish line of the map. As a newcomer you should start on Novice servers, which host the easiest maps. Consider the ping to choose a server close to you."),
				Localize("Use k key to kill (restart), q to pause and watch other players. See settings for other key binds."),
				Localize("It's recommended that you check the settings to adjust them to your liking before joining a server."),
				Localize("Please enter your nickname below."));
			pExtraText = aBuf;
			pButtonText = Localize("Ok");
			TopAlign = true;
		}
		else if(m_Popup == POPUP_POINTS)
		{
			pTitle = Localize("Existing Player");
			if(Client()->Points() > 50)
			{
				str_format(aBuf, sizeof(aBuf), Localize("Your nickname '%s' is already used (%d points). Do you still want to use it?"), Client()->PlayerName(), Client()->Points());
				pExtraText = aBuf;
				TopAlign = true;
			}
			else if(Client()->Points() >= 0)
			{
				m_Popup = POPUP_NONE;
			}
			else
			{
				pExtraText = Localize("Checking for existing player with your name");
			}
		}
		else if(m_Popup == POPUP_WARNING)
		{
			BgColor = ColorRGBA(0.5f, 0.0f, 0.0f, 0.7f);
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
			TopAlign = true;
		}

		CUIRect Box, Part;
		Box = Screen;
		if(m_Popup != POPUP_FIRST_LAUNCH)
			Box.Margin(150.0f, &Box);

		// render the box
		Box.Draw(BgColor, IGraphics::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f, &Part, &Box);
		Box.HSplitTop(24.f, &Part, &Box);
		Part.VMargin(20.f, &Part);
		SLabelProperties Props;
		Props.m_MaxWidth = (int)Part.w;

		if(TextRender()->TextWidth(24.f, pTitle, -1, -1.0f) > Part.w)
			UI()->DoLabel(&Part, pTitle, 24.f, TEXTALIGN_ML, Props);
		else
			UI()->DoLabel(&Part, pTitle, 24.f, TEXTALIGN_MC);

		Box.HSplitTop(20.f, &Part, &Box);
		Box.HSplitTop(24.f, &Part, &Box);
		Part.VMargin(20.f, &Part);

		float FontSize = m_Popup == POPUP_FIRST_LAUNCH ? 16.0f : 20.f;

		if(UseIpLabel)
		{
			SLabelProperties IpLabelProps;
			IpLabelProps.m_MaxWidth = Part.w;
			IpLabelProps.m_EllipsisAtEnd = true;
			UI()->DoLabel(&Part, Client()->ConnectAddressString(), FontSize, TEXTALIGN_MC, IpLabelProps);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitTop(24.f, &Part, &Box);
		}

		Props.m_MaxWidth = (int)Part.w;
		if(TopAlign)
			UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_TL, Props);
		else if(TextRender()->TextWidth(FontSize, pExtraText, -1, -1.0f) > Part.w)
			UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_ML, Props);
		else
			UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_MC);

		if(m_Popup == POPUP_MESSAGE || m_Popup == POPUP_CONFIRM)
		{
			CUIRect ButtonBar;
			Box.HSplitBottom(20.0f, &Box, nullptr);
			Box.HSplitBottom(24.0f, &Box, &ButtonBar);
			ButtonBar.VMargin(100.0f, &ButtonBar);

			if(m_Popup == POPUP_MESSAGE)
			{
				static CButtonContainer s_ButtonConfirm;
				if(DoButton_Menu(&s_ButtonConfirm, m_aPopupButtons[BUTTON_CONFIRM].m_aLabel, 0, &ButtonBar) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
				{
					m_Popup = m_aPopupButtons[BUTTON_CONFIRM].m_NextPopup;
					(this->*m_aPopupButtons[BUTTON_CONFIRM].m_pfnCallback)();
				}
			}
			else if(m_Popup == POPUP_CONFIRM)
			{
				CUIRect CancelButton, ConfirmButton;
				ButtonBar.VSplitMid(&CancelButton, &ConfirmButton, 40.0f);

				static CButtonContainer s_ButtonCancel;
				if(DoButton_Menu(&s_ButtonCancel, m_aPopupButtons[BUTTON_CANCEL].m_aLabel, 0, &CancelButton) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				{
					m_Popup = m_aPopupButtons[BUTTON_CANCEL].m_NextPopup;
					(this->*m_aPopupButtons[BUTTON_CANCEL].m_pfnCallback)();
				}

				static CButtonContainer s_ButtonConfirm;
				if(DoButton_Menu(&s_ButtonConfirm, m_aPopupButtons[BUTTON_CONFIRM].m_aLabel, 0, &ConfirmButton) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
				{
					m_Popup = m_aPopupButtons[BUTTON_CONFIRM].m_NextPopup;
					(this->*m_aPopupButtons[BUTTON_CONFIRM].m_pfnCallback)();
				}
			}
		}
		else if(m_Popup == POPUP_QUIT || m_Popup == POPUP_RESTART)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.VMargin(20.f, &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				str_format(aBuf, sizeof(aBuf), "%s\n\n%s", Localize("There's an unsaved map in the editor, you might want to save it."), Localize("Continue anyway?"));
				Props.m_MaxWidth = Part.w - 20.0f;
				UI()->DoLabel(&Box, aBuf, 20.f, TEXTALIGN_ML, Props);
			}

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_NONE;

			static CButtonContainer s_ButtonTryAgain;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				if(m_Popup == POPUP_RESTART)
				{
					m_Popup = POPUP_NONE;
					Client()->Restart();
				}
				else
				{
					m_Popup = POPUP_NONE;
					Client()->Quit();
				}
			}
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			CUIRect Label, TextBox, TryAgain, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &TryAgain);

			TryAgain.VMargin(20.0f, &TryAgain);
			Abort.VMargin(20.0f, &Abort);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_NONE;

			static CButtonContainer s_ButtonTryAgain;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Try again"), 0, &TryAgain) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				Client()->Connect(g_Config.m_UiServerAddress, g_Config.m_Password);
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, TEXTALIGN_ML);
			UI()->DoClearableEditBox(&m_PasswordInput, &TextBox, 12.0f);
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			Box = Screen;
			Box.Margin(150.0f, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				Client()->Disconnect();
				m_Popup = POPUP_NONE;
				RefreshBrowserTab(g_Config.m_UiPage);
			}

			if(Client()->MapDownloadTotalsize() > 0)
			{
				int64_t Now = time_get();
				if(Now - m_DownloadLastCheckTime >= time_freq())
				{
					if(m_DownloadLastCheckSize > Client()->MapDownloadAmount())
					{
						// map downloaded restarted
						m_DownloadLastCheckSize = 0;
					}

					// update download speed
					float Diff = (Client()->MapDownloadAmount() - m_DownloadLastCheckSize) / ((int)((Now - m_DownloadLastCheckTime) / time_freq()));
					float StartDiff = m_DownloadLastCheckSize - 0.0f;
					if(StartDiff + Diff > 0.0f)
						m_DownloadSpeed = (Diff / (StartDiff + Diff)) * (Diff / 1.0f) + (StartDiff / (Diff + StartDiff)) * m_DownloadSpeed;
					else
						m_DownloadSpeed = 0.0f;
					m_DownloadLastCheckTime = Now;
					m_DownloadLastCheckSize = Client()->MapDownloadAmount();
				}

				Box.HSplitTop(64.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), "%d/%d KiB (%.1f KiB/s)", Client()->MapDownloadAmount() / 1024, Client()->MapDownloadTotalsize() / 1024, m_DownloadSpeed / 1024.0f);
				UI()->DoLabel(&Part, aBuf, 20.f, TEXTALIGN_MC);

				// time left
				int TimeLeft = maximum(1, m_DownloadSpeed > 0.0f ? static_cast<int>((Client()->MapDownloadTotalsize() - Client()->MapDownloadAmount()) / m_DownloadSpeed) : 1);
				if(TimeLeft >= 60)
				{
					TimeLeft /= 60;
					str_format(aBuf, sizeof(aBuf), TimeLeft == 1 ? Localize("%i minute left") : Localize("%i minutes left"), TimeLeft);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), TimeLeft == 1 ? Localize("%i second left") : Localize("%i seconds left"), TimeLeft);
				}
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				UI()->DoLabel(&Part, aBuf, 20.f, TEXTALIGN_MC);

				// progress bar
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				Part.VMargin(40.0f, &Part);
				Part.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
				Part.w = maximum(10.0f, (Part.w * Client()->MapDownloadAmount()) / Client()->MapDownloadTotalsize());
				Part.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), IGraphics::CORNER_ALL, 5.0f);
			}
		}
		else if(m_Popup == POPUP_LANGUAGE)
		{
			CUIRect Button;
			Screen.Margin(150.0f, &Box);
			Box.HSplitTop(20.0f, nullptr, &Box);
			Box.HSplitBottom(20.0f, &Box, nullptr);
			Box.HSplitBottom(24.0f, &Box, &Button);
			Box.HSplitBottom(20.0f, &Box, nullptr);
			Box.VMargin(20.0f, &Box);
			const bool Activated = RenderLanguageSelection(Box);
			Button.VMargin(120.0f, &Button);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Button) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || Activated)
				m_Popup = POPUP_FIRST_LAUNCH;
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_NONE;

			static CButtonContainer s_ButtonOk;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_Popup = POPUP_NONE;
				// rename demo
				char aBufOld[IO_MAX_PATH_LENGTH];
				str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
				char aBufNew[IO_MAX_PATH_LENGTH];
				str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_DemoRenameInput.GetString());
				if(!m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir && !str_endswith(aBufNew, ".demo"))
					str_append(aBufNew, ".demo");

				if(Storage()->FileExists(aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
				{
					PopupMessage(Localize("Error"), Localize("A demo with this name already exists"), Localize("Ok"), POPUP_RENAME_DEMO);
				}
				else if(Storage()->FolderExists(aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
				{
					PopupMessage(Localize("Error"), Localize("A folder with this name already exists"), Localize("Ok"), POPUP_RENAME_DEMO);
				}
				else if(Storage()->RenameFile(aBufOld, aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
				{
					str_copy(m_aCurrentDemoSelectionName, m_DemoRenameInput.GetString());
					if(!m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir)
						fs_split_file_extension(m_DemoRenameInput.GetString(), m_aCurrentDemoSelectionName, sizeof(m_aCurrentDemoSelectionName));
					DemolistPopulate();
					DemolistOnUpdate(false);
				}
				else
				{
					PopupMessage(Localize("Error"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? Localize("Unable to rename the folder") : Localize("Unable to rename the demo"), Localize("Ok"), POPUP_RENAME_DEMO);
				}
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, TEXTALIGN_ML);
			UI()->DoEditBox(&m_DemoRenameInput, &TextBox, 12.0f);
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			CUIRect Row, Ok, Abort;
			Box.VMargin(60.0f, &Box);
			Box.HMargin(20.0f, &Box);
			Box.HSplitBottom(24.0f, &Box, &Row);
			Box.HSplitBottom(40.0f, &Box, nullptr);
			Row.VMargin(40.0f, &Row);
			Row.VSplitMid(&Abort, &Ok, 40.0f);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				m_DemoRenderInput.Clear();
				m_Popup = POPUP_NONE;
			}

			static CButtonContainer s_ButtonOk;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_Popup = POPUP_NONE;
				// render video
				char aVideoPath[IO_MAX_PATH_LENGTH];
				str_format(aVideoPath, sizeof(aVideoPath), "videos/%s", m_DemoRenderInput.GetString());
				if(!str_endswith(aVideoPath, ".mp4"))
					str_append(aVideoPath, ".mp4");
				if(Storage()->FolderExists(aVideoPath, IStorage::TYPE_SAVE))
				{
					PopupMessage(Localize("Error"), Localize("A folder with this name already exists"), Localize("Ok"), POPUP_RENDER_DEMO);
				}
				else if(Storage()->FileExists(aVideoPath, IStorage::TYPE_SAVE))
				{
					char aMessage[128 + IO_MAX_PATH_LENGTH];
					str_format(aMessage, sizeof(aMessage), Localize("File '%s' already exists, do you want to overwrite it?"), m_DemoRenderInput.GetString());
					PopupConfirm(Localize("Replace video"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDemoReplaceVideo, POPUP_NONE, &CMenus::DefaultButtonCallback, POPUP_RENDER_DEMO);
				}
				else
				{
					PopupConfirmDemoReplaceVideo();
				}
			}

			CUIRect ShowChatCheckbox, UseSoundsCheckbox;
			Box.HSplitBottom(20.0f, &Box, &Row);
			Box.HSplitBottom(10.0f, &Box, nullptr);
			Row.VSplitMid(&ShowChatCheckbox, &UseSoundsCheckbox, 20.0f);

			if(DoButton_CheckBox(&g_Config.m_ClVideoShowChat, Localize("Show chat"), g_Config.m_ClVideoShowChat, &ShowChatCheckbox))
				g_Config.m_ClVideoShowChat ^= 1;

			if(DoButton_CheckBox(&g_Config.m_ClVideoSndEnable, Localize("Use sounds"), g_Config.m_ClVideoSndEnable, &UseSoundsCheckbox))
				g_Config.m_ClVideoSndEnable ^= 1;

			CUIRect ShowHudButton;
			Box.HSplitBottom(20.0f, &Box, &Row);
			Row.VSplitMid(&Row, &ShowHudButton, 20.0f);

			if(DoButton_CheckBox(&g_Config.m_ClVideoShowhud, Localize("Show ingame HUD"), g_Config.m_ClVideoShowhud, &ShowHudButton))
				g_Config.m_ClVideoShowhud ^= 1;

			// slowdown
			CUIRect SlowDownButton;
			Row.VSplitLeft(20.0f, &SlowDownButton, &Row);
			Row.VSplitLeft(5.0f, nullptr, &Row);
			static CButtonContainer s_SlowDownButton;
			if(DoButton_FontIcon(&s_SlowDownButton, FONT_ICON_BACKWARD, 0, &SlowDownButton, IGraphics::CORNER_ALL))
				m_Speed = clamp(m_Speed - 1, 0, (int)(g_DemoSpeeds - 1));

			// paused
			CUIRect PausedButton;
			Row.VSplitLeft(20.0f, &PausedButton, &Row);
			Row.VSplitLeft(5.0f, nullptr, &Row);
			static CButtonContainer s_PausedButton;
			if(DoButton_FontIcon(&s_PausedButton, FONT_ICON_PAUSE, 0, &PausedButton, IGraphics::CORNER_ALL))
				m_StartPaused ^= 1;

			// fastforward
			CUIRect FastForwardButton;
			Row.VSplitLeft(20.0f, &FastForwardButton, &Row);
			Row.VSplitLeft(8.0f, nullptr, &Row);
			static CButtonContainer s_FastForwardButton;
			if(DoButton_FontIcon(&s_FastForwardButton, FONT_ICON_FORWARD, 0, &FastForwardButton, IGraphics::CORNER_ALL))
				m_Speed = clamp(m_Speed + 1, 0, (int)(g_DemoSpeeds - 1));

			// speed meter
			char aBuffer[128];
			const char *pPaused = m_StartPaused ? Localize("(paused)") : "";
			str_format(aBuffer, sizeof(aBuffer), "%s: ×%g %s", Localize("Speed"), g_aSpeeds[m_Speed], pPaused);
			UI()->DoLabel(&Row, aBuffer, 12.8f, TEXTALIGN_ML);

			Box.HSplitBottom(16.0f, &Box, nullptr);
			Box.HSplitBottom(24.0f, &Box, &Row);

			CUIRect Label, TextBox;
			Row.VSplitLeft(110.0f, &Label, &TextBox);
			TextBox.VSplitLeft(10.0f, nullptr, &TextBox);
			UI()->DoLabel(&Label, Localize("Video name:"), 12.8f, TEXTALIGN_ML);
			UI()->DoEditBox(&m_DemoRenderInput, &TextBox, 12.8f);
		}
		else if(m_Popup == POPUP_RENDER_DONE)
		{
			CUIRect Ok, OpenFolder;

			char aFilePath[IO_MAX_PATH_LENGTH];
			char aSaveFolder[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "videos", aSaveFolder, sizeof(aSaveFolder));
			str_format(aFilePath, sizeof(aFilePath), "%s/%s.mp4", aSaveFolder, m_DemoRenderInput.GetString());

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&OpenFolder, &Ok);

			Ok.VMargin(20.0f, &Ok);
			OpenFolder.VMargin(20.0f, &OpenFolder);

			static CButtonContainer s_ButtonOpenFolder;
			if(DoButton_Menu(&s_ButtonOpenFolder, Localize("Videos directory"), 0, &OpenFolder))
			{
				if(!open_file(aSaveFolder))
				{
					dbg_msg("menus", "couldn't open file '%s'", aSaveFolder);
				}
			}

			static CButtonContainer s_ButtonOk;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_Popup = POPUP_NONE;
				m_DemoRenderInput.Clear();
			}

			Box.HSplitBottom(160.f, &Box, &Part);
			Part.VMargin(20.0f, &Part);

			str_format(aBuf, sizeof(aBuf), Localize("Video was saved to '%s'"), aFilePath);

			SLabelProperties MessageProps;
			MessageProps.m_MaxWidth = (int)Part.w;
			UI()->DoLabel(&Part, aBuf, 18.0f, TEXTALIGN_TL, MessageProps);
		}
#endif
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			CUIRect Label, TextBox, Skip, Join;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&Skip, &Join);
			Skip.VMargin(20.0f, &Skip);
			Join.VMargin(20.0f, &Join);

			static CButtonContainer s_JoinTutorialButton;
			if(DoButton_Menu(&s_JoinTutorialButton, Localize("Join Tutorial Server"), 0, &Join) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_JoinTutorial = true;
				Client()->RequestDDNetInfo();
				m_Popup = g_Config.m_BrIndicateFinished ? POPUP_POINTS : POPUP_NONE;
			}

			static CButtonContainer s_SkipTutorialButton;
			if(DoButton_Menu(&s_SkipTutorialButton, Localize("Skip Tutorial"), 0, &Skip) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				m_JoinTutorial = false;
				Client()->RequestDDNetInfo();
				m_Popup = g_Config.m_BrIndicateFinished ? POPUP_POINTS : POPUP_NONE;
			}

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(30.0f, 0, &Part);
			str_format(aBuf, sizeof(aBuf), "%s\n(%s)",
				Localize("Show DDNet map finishes in server browser"),
				Localize("transmits your player name to info.ddnet.org"));

			if(DoButton_CheckBox(&g_Config.m_BrIndicateFinished, aBuf, g_Config.m_BrIndicateFinished, &Part))
				g_Config.m_BrIndicateFinished ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Nickname"), 16.0f, TEXTALIGN_ML);
			static CLineInput s_PlayerNameInput(g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName));
			s_PlayerNameInput.SetEmptyText(Client()->PlayerName());
			UI()->DoEditBox(&s_PlayerNameInput, &TextBox, 12.0f);
		}
		else if(m_Popup == POPUP_POINTS)
		{
			CUIRect Yes, No;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static CButtonContainer s_ButtonNo;
			if(DoButton_Menu(&s_ButtonNo, Localize("No"), 0, &No) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_FIRST_LAUNCH;

			static CButtonContainer s_ButtonYes;
			if(DoButton_Menu(&s_ButtonYes, Localize("Yes"), 0, &Yes) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
				m_Popup = POPUP_NONE;
		}
		else if(m_Popup == POPUP_WARNING)
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || (m_PopupWarningDuration > 0s && time_get_nanoseconds() - m_PopupWarningLastTime >= m_PopupWarningDuration))
			{
				m_Popup = POPUP_NONE;
				SetActive(false);
			}
		}
		else
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				if(m_Popup == POPUP_DISCONNECTED && Client()->ReconnectTime() > 0)
					Client()->SetReconnectTime(0);
				m_Popup = POPUP_NONE;
			}
		}

		if(m_Popup == POPUP_NONE)
			UI()->SetActiveItem(nullptr);
	}

	UI()->RenderPopupMenus();

	// Handle this escape hotkey after popup menus
	if(!m_ShowStart && Client()->State() == IClient::STATE_OFFLINE && UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
	{
		m_ShowStart = true;
	}

	return 0;
}

#if defined(CONF_VIDEORECORDER)
void CMenus::PopupConfirmDemoReplaceVideo()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoSelectionName);
	char aVideoName[IO_MAX_PATH_LENGTH];
	str_copy(aVideoName, m_DemoRenderInput.GetString());
	if(!str_endswith(aVideoName, ".mp4"))
		str_append(aVideoName, ".mp4");
	const char *pError = Client()->DemoPlayer_Render(aBuf, m_DemolistStorageType, aVideoName, m_Speed, m_StartPaused);
	m_Speed = 4;
	m_StartPaused = false;
	m_LastPauseChange = -1.0f;
	m_LastSpeedChange = -1.0f;
	if(pError)
	{
		m_DemoRenderInput.Clear();
		PopupMessage(Localize("Error loading demo"), pError, Localize("Ok"));
	}
}
#endif

void CMenus::RenderThemeSelection(CUIRect MainView)
{
	const std::vector<CTheme> &vThemes = m_pBackground->GetThemes();

	int SelectedTheme = -1;
	for(int i = 0; i < (int)vThemes.size(); i++)
	{
		if(str_comp(vThemes[i].m_Name.c_str(), g_Config.m_ClMenuMap) == 0)
		{
			SelectedTheme = i;
			break;
		}
	}
	const int OldSelected = SelectedTheme;

	static CListBox s_ListBox;
	s_ListBox.DoHeader(&MainView, Localize("Theme"), 20.0f);
	s_ListBox.DoStart(20.0f, vThemes.size(), 1, 3, SelectedTheme);

	for(int i = 0; i < (int)vThemes.size(); i++)
	{
		const CTheme &Theme = vThemes[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&Theme.m_Name, i == SelectedTheme);

		if(!Item.m_Visible)
			continue;

		CUIRect Icon, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Icon, &Label);

		// draw icon if it exists
		if(Theme.m_IconTexture.IsValid())
		{
			Icon.VMargin(6.0f, &Icon);
			Icon.HMargin(3.0f, &Icon);
			Graphics()->TextureSet(Theme.m_IconTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(Icon.x, Icon.y, Icon.w, Icon.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		char aName[128];
		if(Theme.m_Name.empty())
			str_copy(aName, "(none)");
		else if(str_comp(Theme.m_Name.c_str(), "auto") == 0)
			str_copy(aName, "(seasons)");
		else if(str_comp(Theme.m_Name.c_str(), "rand") == 0)
			str_copy(aName, "(random)");
		else if(Theme.m_HasDay && Theme.m_HasNight)
			str_copy(aName, Theme.m_Name.c_str());
		else if(Theme.m_HasDay && !Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (day)", Theme.m_Name.c_str());
		else if(!Theme.m_HasDay && Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (night)", Theme.m_Name.c_str());
		else // generic
			str_copy(aName, Theme.m_Name.c_str());

		UI()->DoLabel(&Label, aName, 16.0f * CUI::ms_FontmodHeight, TEXTALIGN_ML);
	}

	SelectedTheme = s_ListBox.DoEnd();

	if(OldSelected != SelectedTheme)
	{
		const CTheme &Theme = vThemes[SelectedTheme];
		str_copy(g_Config.m_ClMenuMap, Theme.m_Name.c_str());
		m_pBackground->LoadMenuBackground(Theme.m_HasDay, Theme.m_HasNight);
	}
}

void CMenus::SetActive(bool Active)
{
	if(Active != m_MenuActive)
	{
		UI()->SetHotItem(nullptr);
		UI()->SetActiveItem(nullptr);
	}
	m_MenuActive = Active;
	if(!m_MenuActive)
	{
		if(m_NeedSendinfo)
		{
			m_pClient->SendInfo(false);
			m_NeedSendinfo = false;
		}

		if(m_NeedSendDummyinfo)
		{
			m_pClient->SendDummyInfo(false);
			m_NeedSendDummyinfo = false;
		}

		if(Client()->State() == IClient::STATE_ONLINE)
		{
			m_pClient->OnRelease();
		}
	}
	else if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->OnRelease();
	}
}

void CMenus::OnReset()
{
}

void CMenus::OnShutdown()
{
	KillServer();
}

bool CMenus::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_MenuActive)
		return false;

	UI()->ConvertMouseMove(&x, &y, CursorType);
	UI()->OnCursorMove(x, y);

	return true;
}

bool CMenus::OnInput(const IInput::CEvent &Event)
{
	// Escape key is always handled to activate/deactivate menu
	if((Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE) || IsActive())
	{
		UI()->OnInput(Event);
		return true;
	}
	return false;
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(nullptr);

	if(OldState == IClient::STATE_ONLINE || OldState == IClient::STATE_OFFLINE)
		TextRender()->DeleteTextContainer(m_MotdTextContainerIndex);

	if(NewState == IClient::STATE_OFFLINE)
	{
		if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_QUITTING)
			UpdateMusicState();
		m_Popup = POPUP_NONE;
		if(Client()->ErrorString() && Client()->ErrorString()[0] != 0)
		{
			if(str_find(Client()->ErrorString(), "password"))
			{
				m_Popup = POPUP_PASSWORD;
				m_PasswordInput.SelectAll();
				UI()->SetActiveItem(&m_PasswordInput);
			}
			else
				m_Popup = POPUP_DISCONNECTED;
		}
	}
	else if(NewState == IClient::STATE_LOADING)
	{
		m_Popup = POPUP_CONNECTING;
		m_DownloadLastCheckTime = time_get();
		m_DownloadLastCheckSize = 0;
		m_DownloadSpeed = 0.0f;
	}
	else if(NewState == IClient::STATE_CONNECTING)
	{
		m_Popup = POPUP_CONNECTING;
	}
	else if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_Popup != POPUP_WARNING)
		{
			m_Popup = POPUP_NONE;
			SetActive(false);
		}
	}
}

void CMenus::OnWindowResize()
{
	TextRender()->DeleteTextContainer(m_MotdTextContainerIndex);
}

void CMenus::OnRender()
{
	UI()->StartCheck();

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		SetActive(true);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		UI()->MapScreen();
		RenderDemoPlayer(*UI()->Screen());
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == CGameClient::SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		PopupMessage(Localize("Disconnected"), Localize("The server is running a non-standard tuning on a pure game type."), Localize("Ok"));
	}

	if(!IsActive())
	{
		if(UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			SetActive(true);
		UI()->FinishCheck();
		UI()->ClearHotkeys();
		return;
	}

	UpdateColors();

	UI()->Update();

	Render();
	RenderTools()->RenderCursor(UI()->MousePos(), 24.0f);

	// render debug information
	if(g_Config.m_Debug)
		UI()->DebugRender();

	if(UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
		SetActive(false);

	UI()->FinishCheck();
	UI()->ClearHotkeys();
}

void CMenus::UpdateColors()
{
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	ms_ColorTabbarInactiveOutgame = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
	ms_ColorTabbarActiveOutgame = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);
	ms_ColorTabbarHoverOutgame = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);

	const float ColorIngameScaleI = 0.5f;
	const float ColorIngameAcaleA = 0.2f;

	ms_ColorTabbarInactiveIngame = ColorRGBA(
		ms_GuiColor.r * ColorIngameScaleI,
		ms_GuiColor.g * ColorIngameScaleI,
		ms_GuiColor.b * ColorIngameScaleI,
		ms_GuiColor.a * 0.8f);

	ms_ColorTabbarActiveIngame = ColorRGBA(
		ms_GuiColor.r * ColorIngameAcaleA,
		ms_GuiColor.g * ColorIngameAcaleA,
		ms_GuiColor.b * ColorIngameAcaleA,
		ms_GuiColor.a);

	ms_ColorTabbarHoverIngame = ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f);
}

void CMenus::RenderBackground()
{
	Graphics()->BlendNormal();

	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);

	// render background color
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ms_GuiColor.WithAlpha(1.0f));
	const IGraphics::CQuadItem BackgroundQuadItem = IGraphics::CQuadItem(0, 0, ScreenWidth, ScreenHeight);
	Graphics()->QuadsDrawTL(&BackgroundQuadItem, 1);
	Graphics()->QuadsEnd();

	// render the tiles
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.045f);
	const float Size = 15.0f;
	const float OffsetTime = std::fmod(LocalTime() * 0.15f, 2.0f);
	IGraphics::CQuadItem aCheckerItems[64];
	size_t NumCheckerItems = 0;
	for(int y = -2; y < (int)(ScreenWidth / Size); y++)
	{
		for(int x = -2; x < (int)(ScreenHeight / Size); x++)
		{
			aCheckerItems[NumCheckerItems] = IGraphics::CQuadItem((x - OffsetTime) * Size * 2 + (y & 1) * Size, (y + OffsetTime) * Size, Size, Size);
			NumCheckerItems++;
			if(NumCheckerItems == std::size(aCheckerItems))
			{
				Graphics()->QuadsDrawTL(aCheckerItems, NumCheckerItems);
				NumCheckerItems = 0;
			}
		}
	}
	if(NumCheckerItems != 0)
		Graphics()->QuadsDrawTL(aCheckerItems, NumCheckerItems);
	Graphics()->QuadsEnd();

	// render border fade
	Graphics()->TextureSet(m_TextureBlob);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	const IGraphics::CQuadItem BlobQuadItem = IGraphics::CQuadItem(-100, -100, ScreenWidth + 200, ScreenHeight + 200);
	Graphics()->QuadsDrawTL(&BlobQuadItem, 1);
	Graphics()->QuadsEnd();

	// restore screen
	UI()->MapScreen();
}

bool CMenus::CheckHotKey(int Key) const
{
	return m_Popup == POPUP_NONE &&
	       !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && // no modifier
	       Input()->KeyIsPressed(Key) && m_pClient->m_GameConsole.IsClosed();
}

int CMenus::DoButton_CheckBox_Tristate(const void *pID, const char *pText, TRISTATE Checked, const CUIRect *pRect)
{
	switch(Checked)
	{
	case TRISTATE::NONE:
		return DoButton_CheckBox_Common(pID, pText, "", pRect);
	case TRISTATE::SOME:
		return DoButton_CheckBox_Common(pID, pText, "O", pRect);
	case TRISTATE::ALL:
		return DoButton_CheckBox_Common(pID, pText, "X", pRect);
	default:
		dbg_assert(false, "invalid tristate");
	}
	dbg_break();
}

int CMenus::MenuImageScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	const char *pExtension = ".png";
	CMenuImage MenuImage;
	CMenus *pSelf = static_cast<CMenus *>(pUser);
	if(IsDir || !str_endswith(pName, pExtension) || str_length(pName) - str_length(pExtension) >= (int)sizeof(MenuImage.m_aName))
		return 0;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "menuimages/%s", pName);

	CImageInfo Info;
	if(!pSelf->Graphics()->LoadPNG(&Info, aPath, DirType))
	{
		char aError[IO_MAX_PATH_LENGTH + 64];
		str_format(aError, sizeof(aError), "Failed to load menu image from '%s'", aPath);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "menus", aError);
		return 0;
	}
	if(Info.m_Format != CImageInfo::FORMAT_RGBA)
	{
		pSelf->Graphics()->FreePNG(&Info);
		char aError[IO_MAX_PATH_LENGTH + 64];
		str_format(aError, sizeof(aError), "Failed to load menu image from '%s': must be an RGBA image", aPath);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "menus", aError);
		return 0;
	}

	MenuImage.m_OrgTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, 0);

	// create gray scale version
	unsigned char *pData = static_cast<unsigned char *>(Info.m_pData);
	const size_t Step = Info.PixelSize();
	for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
	{
		int v = (pData[i * Step] + pData[i * Step + 1] + pData[i * Step + 2]) / 3;
		pData[i * Step] = v;
		pData[i * Step + 1] = v;
		pData[i * Step + 2] = v;
	}
	MenuImage.m_GreyTexture = pSelf->Graphics()->LoadTextureRawMove(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, 0);

	str_truncate(MenuImage.m_aName, sizeof(MenuImage.m_aName), pName, str_length(pName) - str_length(pExtension));
	pSelf->m_vMenuImages.push_back(MenuImage);

	pSelf->RenderLoading(Localize("Loading DDNet Client"), Localize("Loading menu images"), 1);

	return 0;
}

const CMenus::CMenuImage *CMenus::FindMenuImage(const char *pName)
{
	for(auto &Image : m_vMenuImages)
		if(str_comp(Image.m_aName, pName) == 0)
			return &Image;
	return nullptr;
}

void CMenus::SetMenuPage(int NewPage)
{
	if(NewPage == PAGE_DDNET_LEGACY || NewPage == PAGE_KOG_LEGACY)
		NewPage = PAGE_INTERNET;

	m_MenuPage = NewPage;
	if(NewPage >= PAGE_INTERNET && NewPage <= PAGE_FAVORITES)
		g_Config.m_UiPage = NewPage;
}

void CMenus::RefreshBrowserTab(int UiPage)
{
	if(UiPage == PAGE_INTERNET)
	{
		Client()->RequestDDNetInfo();
		ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
	}
	else if(UiPage == PAGE_LAN)
	{
		ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
	}
	else if(UiPage == PAGE_FAVORITES)
	{
		Client()->RequestDDNetInfo();
		ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
	}
}
