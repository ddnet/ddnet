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
#include <game/generated/client_data.h>
#include <game/localization.h>

#include "countryflags.h"
#include "menus.h"

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

SColorPicker CMenus::ms_ColorPicker;
bool CMenus::ms_ValueSelectorTextMode;

float CMenus::ms_ButtonHeight = 25.0f;
float CMenus::ms_ListheaderHeight = 17.0f;

IInput::CEvent CMenus::m_aInputEvents[MAX_INPUTEVENTS];
int CMenus::m_NumInputEvents;

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

	m_NumInputEvents = 0;

	str_copy(m_aCurrentDemoFolder, "demos");
	m_aCallvoteReason[0] = 0;

	m_FriendlistSelectedIndex = -1;
	m_DoubleClickIndex = -1;

	m_DemoPlayerState = DEMOPLAYER_NONE;
	m_Dummy = false;

	m_ServerProcess.Process = 0;
	m_ServerProcess.Initialized = false;

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

int CMenus::DoButton_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName, int Corners, float r, float FontFactor, vec4 ColorHot, vec4 Color, int AlignVertically, bool CheckForActiveColorPicker)
{
	CUIRect Text = *pRect;

	bool MouseInsideColorPicker = false;

	if(CheckForActiveColorPicker)
	{
		if(ms_ColorPicker.m_Active)
		{
			CUIRect PickerRect;
			PickerRect.x = ms_ColorPicker.m_X;
			PickerRect.y = ms_ColorPicker.m_Y;
			PickerRect.w = ms_ColorPicker.ms_Width;
			PickerRect.h = ms_ColorPicker.ms_Height;

			MouseInsideColorPicker = UI()->MouseInside(&PickerRect);
		}
	}

	if(!MouseInsideColorPicker)
		Color.a *= UI()->ButtonColorMul(pButtonContainer);
	pRect->Draw(Color, Corners, r);

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
	SLabelProperties Props;
	Props.m_AlignVertically = AlignVertically;
	UI()->DoLabel(&Text, pText, Text.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);

	if(MouseInsideColorPicker)
		return 0;

	return UI()->DoButtonLogic(pButtonContainer, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	pRect->Draw(ColorRGBA(1, 1, 1, 0.5f * UI()->ButtonColorMul(pID)), IGraphics::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER);
}

int CMenus::DoButton_MenuTab(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, SUIAnimator *pAnimator, const ColorRGBA *pDefaultColor, const ColorRGBA *pActiveColor, const ColorRGBA *pHoverColor, float EdgeRounding, int AlignVertically)
{
	const bool MouseInside = UI()->MouseInside(pRect);
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
	SLabelProperties Props;
	Props.m_AlignVertically = AlignVertically;
	UI()->DoLabel(&Temp, pText, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);

	return UI()->DoButtonLogic(pButtonContainer, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	if(Checked == 2)
		pRect->Draw(ColorRGBA(1, 0.98f, 0.5f, 0.55f), IGraphics::CORNER_T, 5.0f);
	else if(Checked)
		pRect->Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_T, 5.0f);
	CUIRect t;
	pRect->VSplitLeft(5.0f, 0, &t);
	UI()->DoLabel(&t, pText, pRect->h * CUI::ms_FontmodHeight, TEXTALIGN_LEFT);
	return UI()->DoButtonLogic(pID, Checked, pRect);
}

int CMenus::DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect)
{
	CUIRect c = *pRect;
	CUIRect t = *pRect;
	c.w = c.h;
	t.x += c.w;
	t.w -= c.w;
	t.VSplitLeft(5.0f, 0, &t);

	c.Margin(2.0f, &c);
	c.Draw(ColorRGBA(1, 1, 1, 0.25f * UI()->ButtonColorMul(pID)), IGraphics::CORNER_ALL, 3.0f);

	const bool Checkable = *pBoxText == 'X';
	SLabelProperties Props;
	Props.m_AlignVertically = 0;
	if(Checkable)
	{
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		UI()->DoLabel(&c, "\xEF\x80\x8D", c.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
		TextRender()->SetCurFont(NULL);
	}
	else
		UI()->DoLabel(&c, pBoxText, c.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
	TextRender()->SetRenderFlags(0);
	UI()->DoLabel(&t, pText, c.h * CUI::ms_FontmodHeight, TEXTALIGN_LEFT);

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

ColorHSLA CMenus::DoLine_ColorPicker(CButtonContainer *pResetID, const float LineSize, const float WantedPickerPosition, const float LabelSize, const float BottomMargin, CUIRect *pMainRect, const char *pText, unsigned int *pColorValue, const ColorRGBA DefaultColor, bool CheckBoxSpacing, bool UseCheckBox, int *pCheckBoxValue)
{
	CUIRect Section, Button, Label;

	pMainRect->HSplitTop(LineSize, &Section, pMainRect);
	pMainRect->HSplitTop(BottomMargin, 0x0, pMainRect);

	float SectionWidth = Section.w;

	if(CheckBoxSpacing || UseCheckBox)
		Section.VSplitLeft(20.0f, &Button, &Section);

	if(UseCheckBox)
	{
		CUIRect CheckBox;
		Button.Margin(2.0f, &CheckBox);

		if(DoButton_CheckBox(pCheckBoxValue, "", *pCheckBoxValue, &CheckBox))
			*pCheckBoxValue ^= 1;
	}

	Section.VSplitLeft(5.0f, 0x0, &Section);
	float LabelWidth = TextRender()->TextWidth(0, 14.0f, pText, -1, -1.0f);
	Section.VSplitLeft(LabelWidth, &Label, &Section);

	UI()->DoLabel(&Label, pText, LabelSize, TEXTALIGN_LEFT);

	float Cut = WantedPickerPosition - (SectionWidth - Section.w);
	if(Cut < 5)
		Cut = 5.0f;

	Section.VSplitLeft(Cut, 0x0, &Section);
	Section.VSplitLeft(LineSize, &Button, &Section);

	ColorHSLA PickedColor = RenderHSLColorPicker(&Button, pColorValue, false);

	Section.VSplitLeft(7.5f, 0x0, &Section);
	Section.VSplitLeft(55.0f, &Button, &Section);
	Button.HSplitTop(2.0f, 0x0, &Button);
	Button.HSplitBottom(2.0f, &Button, 0x0);

	if(DoButton_Menu(pResetID, Localize("Reset"), 0, &Button, 0, IGraphics::CORNER_ALL, 8.0f, 0, vec4(1, 1, 1, 0.5f), vec4(1, 1, 1, 0.25f), 1, true))
	{
		ColorHSLA HSL = color_cast<ColorHSLA>(DefaultColor);
		*pColorValue = HSL.Pack(false);
	}

	return PickedColor;
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
	str_format(aBuf, sizeof(aBuf), "%d", Checked);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, bool UseScroll, int Current, int Min, int Max, int Step, float Scale, bool IsHex, float Round, ColorRGBA *pColor)
{
	// logic
	static float s_Value;
	static char s_aNumStr[64];
	static void *s_pLastTextpID = pID;
	const bool Inside = UI()->MouseInside(pRect);

	if(Inside)
		UI()->SetHotItem(pID);

	if(UI()->MouseButtonReleased(1) && UI()->HotItem() == pID)
	{
		s_pLastTextpID = pID;
		ms_ValueSelectorTextMode = true;
		if(IsHex)
			str_format(s_aNumStr, sizeof(s_aNumStr), "%06X", Current);
		else
			str_format(s_aNumStr, sizeof(s_aNumStr), "%d", Current);
	}

	if(UI()->CheckActiveItem(pID))
	{
		if(!UI()->MouseButton(0))
		{
			//m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			ms_ValueSelectorTextMode = false;
		}
	}

	if(ms_ValueSelectorTextMode && s_pLastTextpID == pID)
	{
		static float s_NumberBoxID = 0;
		UI()->DoEditBox(&s_NumberBoxID, pRect, s_aNumStr, sizeof(s_aNumStr), 10.0f, &s_NumberBoxID, false, IGraphics::CORNER_ALL);

		UI()->SetActiveItem(&s_NumberBoxID);

		if(Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER) ||
			((UI()->MouseButtonClicked(1) || UI()->MouseButtonClicked(0)) && !Inside))
		{
			if(IsHex)
				Current = clamp(str_toint_base(s_aNumStr, 16), Min, Max);
			else
				Current = clamp(str_toint(s_aNumStr), Min, Max);
			//m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			ms_ValueSelectorTextMode = false;
		}

		if(Input()->KeyIsPressed(KEY_ESCAPE))
		{
			//m_LockMouse = false;
			UI()->SetActiveItem(nullptr);
			ms_ValueSelectorTextMode = false;
		}
	}
	else
	{
		if(UI()->CheckActiveItem(pID))
		{
			if(UseScroll)
			{
				if(UI()->MouseButton(0))
				{
					float delta = UI()->MouseDeltaX();

					if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
						s_Value += delta * 0.05f;
					else
						s_Value += delta;

					if(absolute(s_Value) > Scale)
					{
						int Count = (int)(s_Value / Scale);
						s_Value = fmod(s_Value, Scale);
						Current += Step * Count;
						Current = clamp(Current, Min, Max);

						// Constrain to discrete steps
						if(Count > 0)
							Current = Current / Step * Step;
						else
							Current = round_ceil(Current / (float)Step) * Step;
					}
				}
			}
		}
		else if(UI()->HotItem() == pID)
		{
			if(UI()->MouseButtonClicked(0))
			{
				//m_LockMouse = true;
				s_Value = 0;
				UI()->SetActiveItem(pID);
			}
		}

		// render
		char aBuf[128];
		if(pLabel[0] != '\0')
		{
			if(IsHex)
				str_format(aBuf, sizeof(aBuf), "%s #%06X", pLabel, Current);
			else
				str_format(aBuf, sizeof(aBuf), "%s %d", pLabel, Current);
		}
		else
		{
			if(IsHex)
				str_format(aBuf, sizeof(aBuf), "#%06X", Current);
			else
				str_format(aBuf, sizeof(aBuf), "%d", Current);
		}
		pRect->Draw(*pColor, IGraphics::CORNER_ALL, Round);
		UI()->DoLabel(pRect, aBuf, 10, TEXTALIGN_CENTER);
	}

	return Current;
}

int CMenus::DoKeyReader(void *pID, const CUIRect *pRect, int Key, int ModifierCombination, int *pNewModifierCombination)
{
	// process
	static void *pGrabbedID = 0;
	static bool MouseReleased = true;
	static int s_ButtonUsed = 0;
	const bool Inside = UI()->MouseHovered(pRect);
	int NewKey = Key;
	*pNewModifierCombination = ModifierCombination;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && pGrabbedID == pID)
		MouseReleased = true;

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
			MouseReleased = false;
			pGrabbedID = pID;
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
		if(MouseReleased)
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

	// draw
	if(UI()->CheckActiveItem(pID) && s_ButtonUsed == 0)
		DoButton_KeySelect(pID, "???", 0, pRect);
	else
	{
		if(Key)
		{
			char aBuf[64];
			if(*pNewModifierCombination)
				str_format(aBuf, sizeof(aBuf), "%s%s", CBinds::GetKeyBindModifiersName(*pNewModifierCombination), Input()->KeyName(Key));
			else
				str_format(aBuf, sizeof(aBuf), "%s", Input()->KeyName(Key));

			DoButton_KeySelect(pID, aBuf, 0, pRect);
		}
		else
			DoButton_KeySelect(pID, "", 0, pRect);
	}
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

		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
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
		ColorRGBA *pHomeButtonColor = NULL;
		ColorRGBA *pHomeButtonColorHover = NULL;

		const char *pHomeScreenButtonLabel = "\xEF\x80\x95";
		if(GotNewsOrUpdate)
		{
			pHomeScreenButtonLabel = "\xEF\x87\xAA";
			pHomeButtonColor = &HomeButtonColorAlert;
			pHomeButtonColorHover = &HomeButtonColorAlertHover;
		}

		if(DoButton_MenuTab(&s_StartButton, pHomeScreenButtonLabel, false, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_HOME], pHomeButtonColor, pHomeButtonColor, pHomeButtonColorHover, 10.0f, 0))
		{
			m_ShowStart = true;
			m_DoubleClickIndex = -1;
		}

		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);

		Box.VSplitLeft(10.0f, 0, &Box);

		// offline menus
		if(m_ActivePage == PAGE_NEWS)
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static CButtonContainer s_NewsButton;
			if(DoButton_MenuTab(&s_NewsButton, Localize("News"), m_ActivePage == PAGE_NEWS, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_NEWS]))
			{
				NewPage = PAGE_NEWS;
				m_DoubleClickIndex = -1;
			}
		}
		else if(m_ActivePage == PAGE_DEMOS)
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static CButtonContainer s_DemosButton;
			if(DoButton_MenuTab(&s_DemosButton, Localize("Demos"), m_ActivePage == PAGE_DEMOS, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_DEMOS]))
			{
				DemolistPopulate();
				NewPage = PAGE_DEMOS;
				m_DoubleClickIndex = -1;
			}
		}
		else
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static CButtonContainer s_InternetButton;
			if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), m_ActivePage == PAGE_INTERNET, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_INTERNET]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_INTERNET)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				NewPage = PAGE_INTERNET;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static CButtonContainer s_LanButton;
			if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), m_ActivePage == PAGE_LAN, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_LAN]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_LAN)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
				NewPage = PAGE_LAN;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static CButtonContainer s_FavoritesButton;
			if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), m_ActivePage == PAGE_FAVORITES, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_FAVORITES]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_FAVORITES)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
				NewPage = PAGE_FAVORITES;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(90.0f, &Button, &Box);
			static CButtonContainer s_DDNetButton;
			if(DoButton_MenuTab(&s_DDNetButton, "DDNet", m_ActivePage == PAGE_DDNET, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_DDNET]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_DDNET)
				{
					Client()->RequestDDNetInfo();
					ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
				}
				NewPage = PAGE_DDNET;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(90.0f, &Button, &Box);
			static CButtonContainer s_KoGButton;
			if(DoButton_MenuTab(&s_KoGButton, "KoG", m_ActivePage == PAGE_KOG, &Button, IGraphics::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_KOG]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_KOG)
				{
					Client()->RequestDDNetInfo();
					ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
				}
				NewPage = PAGE_KOG;
				m_DoubleClickIndex = -1;
			}
		}
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
		Box.VSplitLeft(4.0f, 0, &Box);
		static CButtonContainer s_CallVoteButton;
		if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage == PAGE_CALLVOTE, &Button, IGraphics::CORNER_TR))
		{
			NewPage = PAGE_CALLVOTE;
			m_ControlPageOpening = true;
		}
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_QuitButton;
	ColorRGBA QuitColor(1, 0, 0, 0.5f);
	if(DoButton_MenuTab(&s_QuitButton, "\xEF\x80\x91", 0, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_QUIT], NULL, NULL, &QuitColor, 10.0f, 0))
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

	Box.VSplitRight(10.0f, &Box, &Button);
	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_SettingsButton;

	if(DoButton_MenuTab(&s_SettingsButton, "\xEF\x80\x93", m_ActivePage == PAGE_SETTINGS, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_SETTINGS], NULL, NULL, NULL, 10.0f, 0))
		NewPage = PAGE_SETTINGS;

	Box.VSplitRight(10.0f, &Box, &Button);
	Box.VSplitRight(33.0f, &Box, &Button);
	static CButtonContainer s_EditorButton;
	if(DoButton_MenuTab(&s_EditorButton, "\xEF\x81\x84", 0, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_EDITOR], NULL, NULL, NULL, 10.0f, 0))
	{
		g_Config.m_ClEditor = 1;
	}

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		Box.VSplitRight(10.0f, &Box, &Button);
		Box.VSplitRight(33.0f, &Box, &Button);
		static CButtonContainer s_DemoButton;

		if(DoButton_MenuTab(&s_DemoButton, "\xEE\x84\xB1", m_ActivePage == PAGE_DEMOS, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_DEMOBUTTON], NULL, NULL, NULL, 10.0f, 0))
			NewPage = PAGE_DEMOS;

		Box.VSplitRight(10.0f, &Box, &Button);
		Box.VSplitRight(33.0f, &Box, &Button);
		static CButtonContainer s_ServerButton;

		if(DoButton_MenuTab(&s_ServerButton, "\xEF\x95\xBD", m_ActivePage == g_Config.m_UiPage, &Button, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_SERVER], NULL, NULL, NULL, 10.0f, 0))
			NewPage = g_Config.m_UiPage;
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetCurFont(NULL);

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

	static std::chrono::nanoseconds LastLoadRender{0};
	auto CurLoadRenderCount = m_LoadCurrent;
	m_LoadCurrent += IncreaseCounter;
	float Percent = CurLoadRenderCount / (float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get_nanoseconds() - LastLoadRender < std::chrono::nanoseconds(1s) / 60l)
		return;

	LastLoadRender = time_get_nanoseconds();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	CUIRect Screen = *UI()->Screen();
	// some margin around the screen
	Screen.Margin(10.0f, &Screen);
	UI()->MapScreen();

	if(!RenderMenuBackgroundMap || !m_pBackground->Render())
	{
		RenderBackground();
	}

	CUIRect Box = Screen;
	Box.Margin(150.0f, &Box);

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Box.Draw(ColorRGBA{0, 0, 0, 0.50f}, IGraphics::CORNER_ALL, 15.0f);

	CUIRect Part;

	Box.HSplitTop(20.f, &Part, &Box);
	Box.HSplitTop(24.f, &Part, &Box);
	Part.VMargin(20.f, &Part);
	SLabelProperties Props;
	Props.m_MaxWidth = (int)Part.w;
	UI()->DoLabel(&Part, pCaption, 24.f, TEXTALIGN_CENTER);
	Box.HSplitTop(20.f, &Part, &Box);
	Box.HSplitTop(24.f, &Part, &Box);
	Part.VMargin(20.f, &Part);

	Props.m_MaxWidth = (int)Part.w;
	UI()->DoLabel(&Part, pContent, 20.0f, TEXTALIGN_CENTER);

	if(RenderLoadingBar)
		Graphics()->DrawRect(Box.x + 40, Box.y + Box.h - 75, (Box.w - 80) * Percent, 25, ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f), IGraphics::CORNER_ALL, 5.0f);

	Client()->UpdateAndSwap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	g_Config.m_UiUnreadNews = false;

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(15.0f, 0, &MainView);
	MainView.VSplitLeft(15.0f, 0, &MainView);

	CUIRect Label;

	const char *pStr = Client()->m_aNews;
	char aLine[256];
	while((pStr = str_next_token(pStr, "\n", aLine, sizeof(aLine))))
	{
		const int Len = str_length(aLine);
		if(Len > 0 && aLine[0] == '|' && aLine[Len - 1] == '|')
		{
			MainView.HSplitTop(30.0f, &Label, &MainView);
			aLine[Len - 1] = '\0';
			UI()->DoLabel(&Label, aLine + 1, 20.0f, TEXTALIGN_LEFT);
		}
		else
		{
			MainView.HSplitTop(20.0f, &Label, &MainView);
			UI()->DoLabel(&Label, aLine, 15.f, TEXTALIGN_LEFT);
		}
	}
}

void CMenus::OnInit()
{
	if(g_Config.m_ClShowWelcome)
		m_Popup = POPUP_LANGUAGE;
	if(g_Config.m_ClSkipStartMenu)
		m_ShowStart = false;

	UI()->InitInputs(m_aInputEvents, &m_NumInputEvents);

	m_RefreshButton.Init(UI(), -1);
	m_ConnectButton.Init(UI(), -1);

	Console()->Chain("add_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("remove_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);

	Console()->Chain("snd_enable", ConchainUpdateMusicState, this);
	Console()->Chain("snd_enable_music", ConchainUpdateMusicState, this);

	Console()->Chain("cl_assets_entities", ConchainAssetsEntities, this);
	Console()->Chain("cl_asset_game", ConchainAssetGame, this);
	Console()->Chain("cl_asset_emoticons", ConchainAssetEmoticons, this);
	Console()->Chain("cl_asset_particles", ConchainAssetParticles, this);
	Console()->Chain("cl_asset_hud", ConchainAssetHud, this);
	Console()->Chain("cl_asset_extras", ConchainAssetExtras, this);

	m_TextureBlob = Graphics()->LoadTexture("blob.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

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
}

void CMenus::OnConsoleInit()
{
	auto *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager != nullptr)
		pConfigManager->RegisterCallback(CMenus::ConfigSaveCallback, this);
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

void CMenus::PopupMessage(const char *pTopic, const char *pBody, const char *pButton)
{
	// reset active item
	UI()->SetActiveItem(nullptr);

	str_copy(m_aMessageTopic, pTopic);
	str_copy(m_aMessageBody, pBody);
	str_copy(m_aMessageButton, pButton);
	m_Popup = POPUP_MESSAGE;
}

void CMenus::PopupWarning(const char *pTopic, const char *pBody, const char *pButton, std::chrono::nanoseconds Duration)
{
	dbg_msg(pTopic, "%s", pBody);

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

bool CMenus::CanDisplayWarning()
{
	return m_Popup == POPUP_NONE;
}

void CMenus::RenderColorPicker()
{
	if(UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
	{
		ms_ColorPicker.m_Active = false;
		ms_ValueSelectorTextMode = false;
		UI()->SetActiveItem(nullptr);
	}

	if(!ms_ColorPicker.m_Active)
		return;

	// First check if we should disable color picker
	CUIRect PickerRect;
	PickerRect.x = ms_ColorPicker.m_X;
	PickerRect.y = ms_ColorPicker.m_Y;
	PickerRect.w = ms_ColorPicker.ms_Width;
	PickerRect.h = ms_ColorPicker.ms_Height;

	if(UI()->MouseButtonClicked(0) && !UI()->MouseInside(&PickerRect) && !UI()->MouseInside(&ms_ColorPicker.m_AttachedRect))
	{
		ms_ColorPicker.m_Active = false;
		ms_ValueSelectorTextMode = false;
		UI()->SetActiveItem(nullptr);
		return;
	}

	// Prevent activation of UI elements outside of active color picker
	if(UI()->MouseInside(&PickerRect))
		UI()->SetHotItem(&ms_ColorPicker);

	// Render
	ColorRGBA BackgroundColor(0.1f, 0.1f, 0.1f, 1.0f);
	PickerRect.Draw(BackgroundColor, 0, 0);

	CUIRect ColorsArea, HueArea, ValuesHitbox, BottomArea, HueRect, SatRect, ValueRect, HexRect, AlphaRect;
	PickerRect.Margin(3, &ColorsArea);

	ColorsArea.HSplitBottom(ms_ColorPicker.ms_Height - 140.0f, &ColorsArea, &ValuesHitbox);
	ColorsArea.VSplitRight(20, &ColorsArea, &HueArea);

	BottomArea = ValuesHitbox;
	BottomArea.HSplitTop(3, 0x0, &BottomArea);
	HueArea.VSplitLeft(3, 0x0, &HueArea);

	BottomArea.HSplitTop(20, &HueRect, &BottomArea);
	BottomArea.HSplitTop(3, 0x0, &BottomArea);

	constexpr float ValuePadding = 5.0f;
	const float HsvValueWidth = (HueRect.w - ValuePadding * 2) / 3.0f;
	const float HexValueWidth = HsvValueWidth * 2 + ValuePadding;

	HueRect.VSplitLeft(HsvValueWidth, &HueRect, &SatRect);
	SatRect.VSplitLeft(ValuePadding, 0x0, &SatRect);
	SatRect.VSplitLeft(HsvValueWidth, &SatRect, &ValueRect);
	ValueRect.VSplitLeft(ValuePadding, 0x0, &ValueRect);

	BottomArea.HSplitTop(20, &HexRect, &BottomArea);
	HexRect.VSplitLeft(HexValueWidth, &HexRect, &AlphaRect);
	AlphaRect.VSplitLeft(ValuePadding, 0x0, &AlphaRect);

	if(UI()->MouseButtonReleased(1) && !UI()->MouseInside(&ValuesHitbox))
	{
		ms_ColorPicker.m_Active = false;
		ms_ValueSelectorTextMode = false;
		UI()->SetActiveItem(nullptr);
		return;
	}

	ColorRGBA BlackColor(0, 0, 0, 0.5f);

	HueArea.Draw(BlackColor, 0, 0);
	HueArea.Margin(1, &HueArea);

	ColorsArea.Draw(BlackColor, 0, 0);
	ColorsArea.Margin(1, &ColorsArea);

	ColorHSVA PickerColorHSV(ms_ColorPicker.m_HSVColor);
	unsigned H = (unsigned)(PickerColorHSV.x * 255.0f);
	unsigned S = (unsigned)(PickerColorHSV.y * 255.0f);
	unsigned V = (unsigned)(PickerColorHSV.z * 255.0f);

	// Color Area
	vec4 TL = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 0.0f, 1.0f));
	vec4 TR = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f));
	vec4 BL = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 0.0f, 1.0f));
	vec4 BR = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f));

	ColorsArea.Draw4(TL, TR, BL, BR, IGraphics::CORNER_NONE, 0.0f);

	TL = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	TR = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	BL = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	BR = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	ColorsArea.Draw4(TL, TR, BL, BR, IGraphics::CORNER_NONE, 0.0f);

	// Hue Area
	static const float s_aColorIndices[7][3] = {
		{1.0f, 0.0f, 0.0f}, // red
		{1.0f, 0.0f, 1.0f}, // magenta
		{0.0f, 0.0f, 1.0f}, // blue
		{0.0f, 1.0f, 1.0f}, // cyan
		{0.0f, 1.0f, 0.0f}, // green
		{1.0f, 1.0f, 0.0f}, // yellow
		{1.0f, 0.0f, 0.0f} // red
	};

	float HuePickerOffset = HueArea.h / 6.0f;
	CUIRect HuePartialArea = HueArea;
	HuePartialArea.h = HuePickerOffset;

	for(int j = 0; j < 6; j++)
	{
		TL = vec4(s_aColorIndices[j][0], s_aColorIndices[j][1], s_aColorIndices[j][2], 1.0f);
		BL = vec4(s_aColorIndices[j + 1][0], s_aColorIndices[j + 1][1], s_aColorIndices[j + 1][2], 1.0f);

		HuePartialArea.y = HueArea.y + HuePickerOffset * j;
		HuePartialArea.Draw4(TL, TL, BL, BL, IGraphics::CORNER_NONE, 0.0f);
	}

	// Editboxes Area
	ColorRGBA EditboxBackground(0, 0, 0, 0.4f);

	static int s_aValueSelectorIds[4];

	H = DoValueSelector(&s_aValueSelectorIds[0], &HueRect, "H:", true, H, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);
	S = DoValueSelector(&s_aValueSelectorIds[1], &SatRect, "S:", true, S, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);
	V = DoValueSelector(&s_aValueSelectorIds[2], &ValueRect, "V:", true, V, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);

	PickerColorHSV = ColorHSVA(H / 255.0f, S / 255.0f, V / 255.0f);

	unsigned int Hex = color_cast<ColorRGBA>(PickerColorHSV).Pack(false);
	unsigned int NewHex = DoValueSelector(&s_aValueSelectorIds[3], &HexRect, "HEX:", false, Hex, 0, 0xFFFFFF, 1, 1, true, 5.0f, &EditboxBackground);

	if(Hex != NewHex)
		PickerColorHSV = color_cast<ColorHSVA>(ColorRGBA(NewHex));

	// TODO : ALPHA SUPPORT
	UI()->DoLabel(&AlphaRect, "A: 255", 10, TEXTALIGN_CENTER);
	AlphaRect.Draw(ColorRGBA(0, 0, 0, 0.65f), IGraphics::CORNER_ALL, 5.0f);

	// Logic
	float PickerX, PickerY;

	static int s_ColorPickerId = 0;
	static int s_HuePickerId = 0;

	if(UI()->DoPickerLogic(&s_ColorPickerId, &ColorsArea, &PickerX, &PickerY))
	{
		PickerColorHSV.y = PickerX / ColorsArea.w;
		PickerColorHSV.z = 1.0f - PickerY / ColorsArea.h;
	}

	if(UI()->DoPickerLogic(&s_HuePickerId, &HueArea, &PickerX, &PickerY))
		PickerColorHSV.x = 1.0f - PickerY / HueArea.h;

	// Marker Color Area
	float MarkerX = ColorsArea.x + ColorsArea.w * PickerColorHSV.y;
	float MarkerY = ColorsArea.y + ColorsArea.h * (1.0f - PickerColorHSV.z);

	const float MarkerOutlineInd = PickerColorHSV.z > 0.5f ? 0.0f : 1.0f;
	ColorRGBA MarkerOutline(MarkerOutlineInd, MarkerOutlineInd, MarkerOutlineInd, 1.0f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(MarkerOutline);
	Graphics()->DrawCircle(MarkerX, MarkerY, 4.5f, 32);
	Graphics()->SetColor(color_cast<ColorRGBA>(PickerColorHSV));
	Graphics()->DrawCircle(MarkerX, MarkerY, 3.5f, 32);
	Graphics()->QuadsEnd();

	// Marker Hue Area
	CUIRect HueMarker;
	HueArea.Margin(-2.5f, &HueMarker);
	HueMarker.h = 6.5f;
	HueMarker.y = (HueArea.y + HueArea.h * (1.0f - PickerColorHSV.x)) - HueMarker.h / 2.0f;

	ColorRGBA HueMarkerColor = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 1, 1, 1));
	const float HueMarkerOutlineColor = PickerColorHSV.x > 0.75f ? 1.0f : 0.0f;
	ColorRGBA HueMarkerOutline(HueMarkerOutlineColor, HueMarkerOutlineColor, HueMarkerOutlineColor, 1);

	HueMarker.Draw(HueMarkerOutline, IGraphics::CORNER_ALL, 1.2f);
	HueMarker.Margin(1.2f, &HueMarker);
	HueMarker.Draw(HueMarkerColor, IGraphics::CORNER_ALL, 1.2f);

	ms_ColorPicker.m_HSVColor = PickerColorHSV.Pack(false);
	*ms_ColorPicker.m_pColor = color_cast<ColorHSLA>(PickerColorHSV).Pack(false);
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
		m_MenuPage = g_Config.m_UiPage;
		s_Frame++;
	}
	else if(s_Frame == 1)
	{
		UpdateMusicState();
		s_Frame++;
		m_DoubleClickIndex = -1;

		RefreshBrowserTab(g_Config.m_UiPage);
		if(g_Config.m_UiPage == PAGE_INTERNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		else if(g_Config.m_UiPage == PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
		else if(g_Config.m_UiPage == PAGE_DDNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
		else if(g_Config.m_UiPage == PAGE_KOG)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
	}

	if(Client()->State() >= IClient::STATE_ONLINE)
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
			m_Popup = POPUP_SOUNDERROR;
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

			if(Client()->State() == IClient::STATE_OFFLINE && UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				m_ShowStart = true;
			}

			// render news
			if(m_MenuPage < PAGE_NEWS || m_MenuPage > PAGE_SETTINGS || (Client()->State() == IClient::STATE_OFFLINE && m_MenuPage >= PAGE_GAME && m_MenuPage <= PAGE_CALLVOTE))
			{
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				SetMenuPage(PAGE_INTERNET);
				m_DoubleClickIndex = -1;
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
				RenderDemoList(MainView);
			}
			else if(m_MenuPage == PAGE_FAVORITES)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_FAVORITES);
				RenderServerbrowser(MainView);
			}
			else if(m_MenuPage == PAGE_DDNET)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_CUSTOM0);
				RenderServerbrowser(MainView);
			}
			else if(m_MenuPage == PAGE_KOG)
			{
				m_pBackground->ChangePosition(CMenuBackground::POS_BROWSER_CUSTOM0 + 1);
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
		// make sure that other windows doesn't do anything funnay!
		//UI()->SetHotItem(0);
		//UI()->SetActiveItem(nullptr);
		char aBuf[1536];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		int ExtraAlign = 0;
		bool UseIpLabel = false;

		ColorRGBA BgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);
		if(m_Popup == POPUP_MESSAGE)
		{
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
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
			if(Client()->m_ReconnectTime > 0)
			{
				str_format(aBuf, sizeof(aBuf), Localize("Reconnect in %d sec"), (int)((Client()->m_ReconnectTime - time_get()) / time_freq()));
				pTitle = Client()->ErrorString();
				pExtraText = aBuf;
				pButtonText = Localize("Abort");
			}
			ExtraAlign = 0;
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			pTitle = Localize("Delete demo");
			pExtraText = Localize("Are you sure that you want to delete the demo?");
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			pTitle = Localize("Rename demo");
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			pTitle = Localize("Render demo");
		}
		else if(m_Popup == POPUP_REPLACE_VIDEO)
		{
			pTitle = Localize("Replace video");
			pExtraText = Localize("File already exists, do you want to overwrite it?");
		}
#endif
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			pTitle = Localize("Remove friend");
			pExtraText = Localize("Are you sure that you want to remove the player from your friends list?");
		}
		else if(m_Popup == POPUP_SOUNDERROR)
		{
			pTitle = Localize("Sound error");
			pExtraText = Localize("The audio device couldn't be initialised.");
			pButtonText = Localize("Ok");
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
		}
		else if(m_Popup == POPUP_DISCONNECT)
		{
			pTitle = Localize("Disconnect");
			pExtraText = Localize("Are you sure that you want to disconnect?");
		}
		else if(m_Popup == POPUP_DISCONNECT_DUMMY)
		{
			pTitle = Localize("Disconnect Dummy");
			pExtraText = Localize("Are you sure that you want to disconnect your dummy?");
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
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_POINTS)
		{
			pTitle = Localize("Existing Player");
			if(Client()->m_Points > 50)
			{
				str_format(aBuf, sizeof(aBuf), Localize("Your nickname '%s' is already used (%d points). Do you still want to use it?"), Client()->PlayerName(), Client()->m_Points);
				pExtraText = aBuf;
				ExtraAlign = -1;
			}
			else if(Client()->m_Points >= 0)
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
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SWITCH_SERVER)
		{
			pTitle = Localize("Disconnect");
			pExtraText = Localize("Are you sure that you want to disconnect and switch to a different server?");
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

		if(TextRender()->TextWidth(0, 24.f, pTitle, -1, -1.0f) > Part.w)
			UI()->DoLabel(&Part, pTitle, 24.f, TEXTALIGN_LEFT, Props);
		else
			UI()->DoLabel(&Part, pTitle, 24.f, TEXTALIGN_CENTER);

		Box.HSplitTop(20.f, &Part, &Box);
		Box.HSplitTop(24.f, &Part, &Box);
		Part.VMargin(20.f, &Part);

		float FontSize = m_Popup == POPUP_FIRST_LAUNCH ? 16.0f : 20.f;

		if(UseIpLabel)
		{
			UI()->DoLabel(&Part, Client()->ConnectAddressString(), FontSize, TEXTALIGN_CENTER);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitTop(24.f, &Part, &Box);
		}

		Props.m_MaxWidth = (int)Part.w;
		if(ExtraAlign == -1)
			UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_LEFT, Props);
		else
		{
			if(TextRender()->TextWidth(0, FontSize, pExtraText, -1, -1.0f) > Part.w)
				UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_LEFT, Props);
			else
				UI()->DoLabel(&Part, pExtraText, FontSize, TEXTALIGN_CENTER);
		}

		if(m_Popup == POPUP_QUIT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.VMargin(20.f, &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				str_format(aBuf, sizeof(aBuf), "%s\n%s", Localize("There's an unsaved map in the editor, you might want to save it before you quit the game."), Localize("Quit anyway?"));
				Props.m_MaxWidth = Part.w - 20.0f;
				UI()->DoLabel(&Box, aBuf, 20.f, TEXTALIGN_LEFT, Props);
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
				m_Popup = POPUP_NONE;
				Client()->Quit();
			}
		}
		else if(m_Popup == POPUP_DISCONNECT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

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
				Client()->Disconnect();
		}
		else if(m_Popup == POPUP_DISCONNECT_DUMMY)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

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
				Client()->DummyDisconnect(0);
				m_Popup = POPUP_NONE;
				SetActive(false);
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
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, TEXTALIGN_LEFT);
			static float s_Offset = 0.0f;
			UI()->DoEditBox(&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &s_Offset, true);
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
				UI()->DoLabel(&Part, aBuf, 20.f, TEXTALIGN_CENTER);

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
				UI()->DoLabel(&Part, aBuf, 20.f, TEXTALIGN_CENTER);

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
			Box = Screen;
			Box.Margin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);
			RenderLanguageSelection(Box);
			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
				m_Popup = POPUP_FIRST_LAUNCH;
		}
		else if(m_Popup == POPUP_COUNTRY)
		{
			Box = Screen;
			Box.Margin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);

			static int CurSelection = -2;
			if(CurSelection == -2)
				CurSelection = g_Config.m_BrFilterCountryIndex;
			static float s_ScrollValue = 0.0f;
			int OldSelected = -1;
			UiDoListboxStart(&s_ScrollValue, &Box, 50.0f, Localize("Country / Region"), "", m_pClient->m_CountryFlags.Num(), 6, OldSelected, s_ScrollValue);

			for(size_t i = 0; i < m_pClient->m_CountryFlags.Num(); ++i)
			{
				const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_CountryFlags.GetByIndex(i);
				if(pEntry->m_CountryCode == CurSelection)
					OldSelected = i;

				CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected >= 0 && (size_t)OldSelected == i);
				if(Item.m_Visible)
				{
					CUIRect Label;
					Item.m_Rect.Margin(5.0f, &Item.m_Rect);
					Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
					float OldWidth = Item.m_Rect.w;
					Item.m_Rect.w = Item.m_Rect.h * 2;
					Item.m_Rect.x += (OldWidth - Item.m_Rect.w) / 2.0f;
					ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_CountryFlags.Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
					UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, TEXTALIGN_CENTER);
				}
			}

			const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
			if(OldSelected != NewSelected)
				CurSelection = m_pClient->m_CountryFlags.GetByIndex(NewSelected)->m_CountryCode;

			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				g_Config.m_BrFilterCountryIndex = CurSelection;
				Client()->ServerBrowserUpdate();
				m_Popup = POPUP_NONE;
			}

			if(UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				CurSelection = g_Config.m_BrFilterCountryIndex;
				m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
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
				m_Popup = POPUP_NONE;
				// delete demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
					if(Storage()->RemoveFile(aBuf, m_vDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to delete the demo"), Localize("Ok"));
				}
			}
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
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length - 5] != '.' || str_comp_nocase(m_aCurrentDemoFile + Length - 4, "demo"))
						str_format(aBufNew, sizeof(aBufNew), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					if(Storage()->RenameFile(aBufOld, aBufNew, m_vDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to rename the demo"), Localize("Ok"));
				}
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, TEXTALIGN_LEFT);
			static float s_Offset = 0.0f;
			UI()->DoEditBox(&s_Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &s_Offset);
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort, IncSpeed, DecSpeed, Button, Buttons;

			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif
			Part.VMargin(80.0f, &Part);

			Part.HSplitBottom(20.0f, &Part, &Buttons);
			Buttons.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_NONE;

			static CButtonContainer s_ButtonOk;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_Popup = POPUP_NONE;
				// name video
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 3 || m_aCurrentDemoFile[Length - 4] != '.' || str_comp_nocase(m_aCurrentDemoFile + Length - 3, "mp4"))
						str_format(aBufNew, sizeof(aBufNew), "%s.mp4", m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s", m_aCurrentDemoFile);
					char aWholePath[1024];
					// store new video filename to origin buffer
					str_copy(m_aCurrentDemoFile, aBufNew);
					if(Storage()->FindFile(m_aCurrentDemoFile, "videos", IStorage::TYPE_ALL, aWholePath, sizeof(aWholePath)))
					{
						PopupMessage(Localize("Error"), Localize("Destination file already exist"), Localize("Ok"));
						m_Popup = POPUP_REPLACE_VIDEO;
					}
					else
					{
						const char *pError = Client()->DemoPlayer_Render(aBufOld, m_vDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
						m_Speed = 4;
						//Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "demo_render_path", aWholePath);
						if(pError)
							PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
					}
				}
			}
			Box.HSplitBottom(30.f, &Box, 0);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(10.f, &Box, 0);

			float ButtonSize = 20.0f;
			Part.VSplitLeft(113.0f, 0, &Part);
			Part.VSplitLeft(200.0f, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowChat, Localize("Show chat"), g_Config.m_ClVideoShowChat, &Button))
				g_Config.m_ClVideoShowChat ^= 1;
			Part.VSplitLeft(32.0f, 0, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoSndEnable, Localize("Use sounds"), g_Config.m_ClVideoSndEnable, &Part))
				g_Config.m_ClVideoSndEnable ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
			Part.VSplitLeft(60.0f, 0, &Part);
			Part.VSplitLeft(60.0f, 0, &Label);
			Part.VSplitMid(&IncSpeed, &DecSpeed);

			IncSpeed.VMargin(20.0f, &IncSpeed);
			DecSpeed.VMargin(20.0f, &DecSpeed);

			Part.VSplitLeft(20.0f, &Button, &Part);
			bool IncDemoSpeed = false, DecDemoSpeed = false;
			// slowdown
			Part.VSplitLeft(5.0f, 0, &Part);
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			static CButtonContainer s_SlowDownButton;
			if(DoButton_FontIcon(&s_SlowDownButton, "\xEF\x81\x8A", 0, &Button, IGraphics::CORNER_ALL))
				DecDemoSpeed = true;

			// fastforward
			Part.VSplitLeft(5.0f, 0, &Part);
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			static CButtonContainer s_FastForwardButton;
			if(DoButton_FontIcon(&s_FastForwardButton, "\xEF\x81\x8E", 0, &Button, IGraphics::CORNER_ALL))
				IncDemoSpeed = true;

			// speed meter
			Part.VSplitLeft(8.0f, 0, &Part);
			char aBuffer[64];
			str_format(aBuffer, sizeof(aBuffer), "%s: ×%g", Localize("Speed"), g_aSpeeds[m_Speed]);
			UI()->DoLabel(&Part, aBuffer, 12.8f, TEXTALIGN_LEFT);

			if(IncDemoSpeed)
				m_Speed = clamp(m_Speed + 1, 0, (int)(g_DemoSpeeds - 1));
			else if(DecDemoSpeed)
				m_Speed = clamp(m_Speed - 1, 0, (int)(g_DemoSpeeds - 1));

			Part.VSplitLeft(207.0f, 0, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowhud, Localize("Show ingame HUD"), g_Config.m_ClVideoShowhud, &Part))
				g_Config.m_ClVideoShowhud ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Video name:"), 18.0f, TEXTALIGN_LEFT);
			static float s_Offset = 0.0f;
			UI()->DoEditBox(&s_Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &s_Offset);
		}
		else if(m_Popup == POPUP_REPLACE_VIDEO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
				m_Popup = POPUP_RENDER_DEMO;

			static CButtonContainer s_ButtonTryAgain;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				m_Popup = POPUP_NONE;
				// render video
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Render(aBuf, m_vDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
				m_Speed = 4;
				if(pError)
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
			}
		}
#endif
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
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
				m_Popup = POPUP_NONE;
				// remove friend
				if(m_FriendlistSelectedIndex >= 0)
				{
					m_pClient->Friends()->RemoveFriend(m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName,
						m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
				}
			}
		}
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
			UI()->DoLabel(&Label, Localize("Nickname"), 16.0f, TEXTALIGN_LEFT);
			static float s_Offset = 0.0f;
			SUIExEditBoxProperties EditProps;
			EditProps.m_pEmptyText = Client()->PlayerName();
			UI()->DoEditBox(&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &s_Offset, false, IGraphics::CORNER_ALL, EditProps);
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
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || (time_get_nanoseconds() - m_PopupWarningLastTime >= m_PopupWarningDuration))
			{
				m_Popup = POPUP_NONE;
				SetActive(false);
			}
		}
		else if(m_Popup == POPUP_SWITCH_SERVER)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static CButtonContainer s_ButtonAbort;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			{
				m_Popup = POPUP_NONE;
				m_aNextServer[0] = '\0';
			}

			static CButtonContainer s_ButtonTryAgain;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
				Client()->Connect(m_aNextServer);
		}
		else
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static CButtonContainer s_Button;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				if(m_Popup == POPUP_DISCONNECTED && Client()->m_ReconnectTime > 0)
					Client()->m_ReconnectTime = 0;
				m_Popup = POPUP_NONE;
			}
		}

		if(m_Popup == POPUP_NONE)
			UI()->SetActiveItem(nullptr);
	}
	return 0;
}

void CMenus::RenderThemeSelection(CUIRect MainView, bool Header)
{
	std::vector<CTheme> &vThemesRef = m_pBackground->GetThemes();

	int SelectedTheme = -1;
	for(int i = 0; i < (int)vThemesRef.size(); i++)
	{
		if(str_comp(vThemesRef[i].m_Name.c_str(), g_Config.m_ClMenuMap) == 0)
		{
			SelectedTheme = i;
			break;
		}
	}

	static int s_ListBox = 0;
	static float s_ScrollValue = 0.0f;
	UiDoListboxStart(&s_ListBox, &MainView, 26.0f, Localize("Theme"), "", vThemesRef.size(), 1, -1, s_ScrollValue);

	for(int i = 0; i < (int)vThemesRef.size(); i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&vThemesRef[i].m_Name, i == SelectedTheme);

		CTheme &Theme = vThemesRef[i];

		if(!Item.m_Visible)
			continue;

		CUIRect Icon;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Icon, &Item.m_Rect);

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
			str_format(aName, sizeof(aName), "%s", Theme.m_Name.c_str());
		else if(Theme.m_HasDay && !Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (day)", Theme.m_Name.c_str());
		else if(!Theme.m_HasDay && Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (night)", Theme.m_Name.c_str());
		else // generic
			str_format(aName, sizeof(aName), "%s", Theme.m_Name.c_str());

		UI()->DoLabel(&Item.m_Rect, aName, 16 * CUI::ms_FontmodHeight, TEXTALIGN_LEFT);
	}

	bool ItemActive = false;
	int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0, &ItemActive);

	if(ItemActive && NewSelected != SelectedTheme)
	{
		str_format(g_Config.m_ClMenuMap, sizeof(g_Config.m_ClMenuMap), "%s", vThemesRef[NewSelected].m_Name.c_str());
		m_pBackground->LoadMenuBackground(vThemesRef[NewSelected].m_HasDay, vThemesRef[NewSelected].m_HasNight);
	}
}

void CMenus::SetActive(bool Active)
{
	if(Active != m_MenuActive)
	{
		ms_ColorPicker.m_Active = false;
		Input()->SetIMEState(Active);
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

	m_MousePos.x = clamp(m_MousePos.x + x, 0.f, (float)Graphics()->WindowWidth());
	m_MousePos.y = clamp(m_MousePos.y + y, 0.f, (float)Graphics()->WindowHeight());

	return true;
}

bool CMenus::OnInput(IInput::CEvent Event)
{
	// special handle esc and enter for popup purposes
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		SetActive(!IsActive());
		UI()->OnInput(Event);
		return true;
	}
	if(IsActive())
	{
		if(m_NumInputEvents < MAX_INPUTEVENTS)
			m_aInputEvents[m_NumInputEvents++] = Event;
		UI()->OnInput(Event);
		return true;
	}
	return false;
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(nullptr);

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
				UI()->SetHotItem(&g_Config.m_Password);
				UI()->SetActiveItem(&g_Config.m_Password);
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

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == m_pClient->SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		PopupMessage(Localize("Disconnected"), Localize("The server is running a non-standard tuning on a pure game type."), Localize("Ok"));
	}

	if(!IsActive())
	{
		UI()->FinishCheck();
		UI()->ClearHotkeys();
		m_NumInputEvents = 0;
		return;
	}

	// update colors
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	ms_ColorTabbarInactiveOutgame = ColorRGBA(0, 0, 0, 0.25f);
	ms_ColorTabbarActiveOutgame = ColorRGBA(0, 0, 0, 0.5f);
	ms_ColorTabbarHoverOutgame = ColorRGBA(1, 1, 1, 0.25f);

	float ColorIngameScaleI = 0.5f;
	float ColorIngameAcaleA = 0.2f;

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

	ms_ColorTabbarHoverIngame = ColorRGBA(1, 1, 1, 0.75f);

	// update the ui
	CUIRect *pScreen = UI()->Screen();
	float mx = (m_MousePos.x / (float)Graphics()->WindowWidth()) * pScreen->w;
	float my = (m_MousePos.y / (float)Graphics()->WindowHeight()) * pScreen->h;

	UI()->Update(mx, my, mx * 3.0f, my * 3.0f);

	Render();
	RenderTools()->RenderCursor(vec2(mx, my), 24.0f);

	// render debug information
	if(g_Config.m_Debug)
	{
		UI()->MapScreen();

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%p %p %p", UI()->HotItem(), UI()->ActiveItem(), UI()->LastActiveItem());
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 10, 10, 10, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	UI()->FinishCheck();
	UI()->ClearHotkeys();
	m_NumInputEvents = 0;
}

void CMenus::RenderBackground()
{
	Graphics()->BlendNormal();

	float sw = 300 * Graphics()->ScreenAspect();
	float sh = 300;
	Graphics()->MapScreen(0, 0, sw, sh);

	// render background color
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	ColorRGBA Bottom(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
	ColorRGBA Top(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
	IGraphics::CColorVertex Array[4] = {
		IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
		IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
		IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
		IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
	Graphics()->SetColorVertex(Array, 4);
	IGraphics::CQuadItem QuadItem(0, 0, sw, sh);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render the tiles
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	float Size = 15.0f;
	float OffsetTime = fmod(LocalTime() * 0.15f, 2.0f);
	for(int y = -2; y < (int)(sw / Size); y++)
		for(int x = -2; x < (int)(sh / Size); x++)
		{
			Graphics()->SetColor(0, 0, 0, 0.045f);
			QuadItem = IGraphics::CQuadItem((x - OffsetTime) * Size * 2 + (y & 1) * Size, (y + OffsetTime) * Size, Size, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
		}
	Graphics()->QuadsEnd();

	// render border fade
	Graphics()->TextureSet(m_TextureBlob);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	QuadItem = IGraphics::CQuadItem(-100, -100, sw + 200, sh + 200);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// restore screen
	UI()->MapScreen();
}

bool CMenus::CheckHotKey(int Key) const
{
	return m_Popup == POPUP_NONE &&
	       !Input()->KeyIsPressed(KEY_LSHIFT) && !Input()->KeyIsPressed(KEY_RSHIFT) && !Input()->ModifierIsPressed() && // no modifier
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
	CMenus *pSelf = (CMenus *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	char aBuf[IO_MAX_PATH_LENGTH];
	bool ImgExists = false;
	for(const auto &Img : pSelf->m_vMenuImages)
	{
		str_format(aBuf, std::size(aBuf), "%s.png", Img.m_aName);
		if(str_comp(aBuf, pName) == 0)
		{
			ImgExists = true;
			break;
		}
	}

	if(!ImgExists)
	{
		str_format(aBuf, sizeof(aBuf), "menuimages/%s", pName);
		CImageInfo Info;
		if(!pSelf->Graphics()->LoadPNG(&Info, aBuf, DirType))
		{
			str_format(aBuf, sizeof(aBuf), "failed to load menu image from %s", pName);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
			return 0;
		}

		CMenuImage MenuImage;
		MenuImage.m_OrgTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);

		unsigned char *pData = (unsigned char *)Info.m_pData;
		//int Pitch = Info.m_Width*4;

		// create colorless version
		int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

		// make the texture gray scale
		for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
		{
			int v = (pData[i * Step] + pData[i * Step + 1] + pData[i * Step + 2]) / 3;
			pData[i * Step] = v;
			pData[i * Step + 1] = v;
			pData[i * Step + 2] = v;
		}

		MenuImage.m_GreyTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
		pSelf->Graphics()->FreePNG(&Info);

		// set menu image data
		str_truncate(MenuImage.m_aName, sizeof(MenuImage.m_aName), pName, str_length(pName) - 4);
		pSelf->m_vMenuImages.push_back(MenuImage);
		pSelf->RenderLoading(Localize("Loading DDNet Client"), Localize("Loading menu images"), 1);
	}
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
	m_MenuPage = NewPage;
	if(NewPage >= PAGE_INTERNET && NewPage <= PAGE_KOG)
		g_Config.m_UiPage = NewPage;
}

void CMenus::RefreshBrowserTab(int UiPage)
{
	if(UiPage == PAGE_INTERNET)
		ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
	else if(UiPage == PAGE_LAN)
		ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
	else if(UiPage == PAGE_FAVORITES)
		ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
	else if(UiPage == PAGE_DDNET)
	{
		// start a new server list request
		Client()->RequestDDNetInfo();
		ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
	}
	else if(UiPage == PAGE_KOG)
	{
		// start a new server list request
		Client()->RequestDDNetInfo();
		ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
	}
}

bool CMenus::HandleListInputs(const CUIRect &View, float &ScrollValue, const float ScrollAmount, int *pScrollOffset, const float ElemHeight, int &SelectedIndex, const int NumElems)
{
	if(NumElems == 0)
	{
		ScrollValue = 0;
		SelectedIndex = 0;
		return false;
	}

	int NewIndex = -1;
	int Num = (int)(View.h / ElemHeight);
	int ScrollNum = maximum(NumElems - Num, 0);
	if(ScrollNum > 0)
	{
		if(pScrollOffset && *pScrollOffset >= 0)
		{
			ScrollValue = (float)(*pScrollOffset) / ScrollNum;
			*pScrollOffset = -1;
		}
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && UI()->MouseInside(&View))
			ScrollValue -= 3.0f / ScrollNum;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && UI()->MouseInside(&View))
			ScrollValue += 3.0f / ScrollNum;
	}

	ScrollValue = clamp(ScrollValue, 0.0f, 1.0f);
	SelectedIndex = clamp(SelectedIndex, 0, NumElems - 1);

	for(int i = 0; i < m_NumInputEvents; i++)
	{
		if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
		{
			if(UI()->LastActiveItem() == &g_Config.m_UiServerAddress)
				return false;
			else if(m_aInputEvents[i].m_Key == KEY_DOWN)
				NewIndex = minimum(SelectedIndex + 1, NumElems - 1);
			else if(m_aInputEvents[i].m_Key == KEY_UP)
				NewIndex = maximum(SelectedIndex - 1, 0);
			else if(m_aInputEvents[i].m_Key == KEY_PAGEUP)
				NewIndex = maximum(SelectedIndex - 25, 0);
			else if(m_aInputEvents[i].m_Key == KEY_PAGEDOWN)
				NewIndex = minimum(SelectedIndex + 25, NumElems - 1);
			else if(m_aInputEvents[i].m_Key == KEY_HOME)
				NewIndex = 0;
			else if(m_aInputEvents[i].m_Key == KEY_END)
				NewIndex = NumElems - 1;
		}
		if(NewIndex > -1 && NewIndex < NumElems)
		{
			//scroll
			float IndexY = View.y - ScrollValue * ScrollNum * ElemHeight + NewIndex * ElemHeight;
			int Scroll = View.y > IndexY ? -1 : View.y + View.h < IndexY + ElemHeight ? 1 : 0;
			if(Scroll)
			{
				if(Scroll < 0)
				{
					int NumScrolls = (View.y - IndexY + ElemHeight - 1.0f) / ElemHeight;
					ScrollValue -= (1.0f / ScrollNum) * NumScrolls;
				}
				else
				{
					int NumScrolls = (IndexY + ElemHeight - (View.y + View.h) + ElemHeight - 1.0f) / ElemHeight;
					ScrollValue += (1.0f / ScrollNum) * NumScrolls;
				}
			}

			SelectedIndex = NewIndex;
		}
	}

	return NewIndex != -1;
}
