/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <vector>

#include <base/tl/array.h>

#include <math.h>

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>
#include <game/version.h>

#include <engine/client/updater.h>

#include <game/client/components/binds.h>
#include <game/client/components/console.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/generated/client_data.h>
#include <game/localization.h>
#include <mastersrv/mastersrv.h>

#include "controls.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

#include <limits>

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
float CMenus::ms_FontmodHeight = 0.8f;

IInput::CEvent CMenus::m_aInputEvents[MAX_INPUTEVENTS];
int CMenus::m_NumInputEvents;

CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_INTERNET;
	m_MenuPage = 0;
	m_GamePage = PAGE_GAME;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
	m_NeedSendinfo = false;
	m_NeedSendDummyinfo = false;
	m_MenuActive = true;
	m_ShowStart = true;
	m_UseMouseButtons = true;
	m_MouseSlow = false;

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;

	m_LastInput = time_get();

	str_copy(m_aCurrentDemoFolder, "demos", sizeof(m_aCurrentDemoFolder));
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

float CMenus::ButtonColorMul(const void *pID)
{
	if(UI()->ActiveItem() == pID)
		return ButtonColorMulActive();
	else if(UI()->HotItem() == pID)
		return ButtonColorMulHot();
	return ButtonColorMulDefault();
}

int CMenus::DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect)
{
	int x = pRect->x;
	int y = pRect->y;
	int w = pRect->w;
	int h = pRect->h;

	// Square and center
	if(w > h)
	{
		x += (w - h) / 2;
		w = h;
	}
	else if(h > w)
	{
		y += (h - w) / 2;
		h = w;
	}

	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);

	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SpriteId);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return 0;
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
		IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();

	return Active ? UI()->DoButtonLogic(pID, "", Checked, pRect) : 0;
}

int CMenus::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName, int Corners, float r, float FontFactor, vec4 ColorHot, vec4 Color, int AlignVertically, bool CheckForActiveColorPicker)
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
		Color.a *= ButtonColorMul(pID);
	RenderTools()->DrawUIRect(pRect, Color, Corners, r);

	if(pImageName)
	{
		CUIRect Image;
		pRect->VSplitRight(pRect->h * 4.0f, &Text, &Image); // always correct ratio for image

		// render image
		const CMenuImage *pImage = FindMenuImage(pImageName);
		if(pImage)
		{
			Graphics()->TextureSet(UI()->HotItem() == pID ? pImage->m_OrgTexture : pImage->m_GreyTexture);
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
	UI()->DoLabel(&Text, pText, Text.h * ms_FontmodHeight, 0, -1, AlignVertically);

	if(MouseInsideColorPicker)
		return 0;

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	RenderTools()->DrawUIRect(pRect, ColorRGBA(1, 1, 1, 0.5f * ButtonColorMul(pID)), CUI::CORNER_ALL, 5.0f);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h * ms_FontmodHeight, 0);
}

int CMenus::DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners, SUIAnimator *pAnimator, const ColorRGBA *pDefaultColor, const ColorRGBA *pActiveColor, const ColorRGBA *pHoverColor, float EdgeRounding, int AlignVertically)
{
	bool MouseInside = UI()->MouseInside(pRect);
	CUIRect Rect = *pRect;

	if(pAnimator != NULL)
	{
		int64 Time = time_get_microseconds();

		if(pAnimator->m_Time + (int64)100000 < Time)
		{
			pAnimator->m_Value = pAnimator->m_Active ? 1 : 0;
			pAnimator->m_Time = Time;
		}

		pAnimator->m_Active = Checked || MouseInside;

		if(pAnimator->m_Active)
			pAnimator->m_Value = clamp<float>(pAnimator->m_Value + (Time - pAnimator->m_Time) / 100000.f, 0, 1);
		else
			pAnimator->m_Value = clamp<float>(pAnimator->m_Value - (Time - pAnimator->m_Time) / 100000.f, 0, 1);

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

		RenderTools()->DrawUIRect(&Rect, ColorMenuTab, Corners, EdgeRounding);
	}
	else
	{
		if(UI()->MouseInside(pRect))
		{
			ColorRGBA HoverColorMenuTab = ms_ColorTabbarHover;
			if(pHoverColor)
				HoverColorMenuTab = *pHoverColor;

			RenderTools()->DrawUIRect(&Rect, HoverColorMenuTab, Corners, EdgeRounding);
		}
		else
		{
			ColorRGBA ColorMenuTab = ms_ColorTabbarInactive;
			if(pDefaultColor)
				ColorMenuTab = *pDefaultColor;

			RenderTools()->DrawUIRect(&Rect, ColorMenuTab, Corners, EdgeRounding);
		}
	}

	CUIRect Temp;

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

	Rect.HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h * ms_FontmodHeight, 0, -1, AlignVertically);

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	if(Checked == 2)
		RenderTools()->DrawUIRect(pRect, ColorRGBA(1, 0.98f, 0.5f, 0.55f), CUI::CORNER_T, 5.0f);
	else if(Checked)
		RenderTools()->DrawUIRect(pRect, ColorRGBA(1, 1, 1, 0.5f), CUI::CORNER_T, 5.0f);
	CUIRect t;
	pRect->VSplitLeft(5.0f, 0, &t);
	UI()->DoLabel(&t, pText, pRect->h * ms_FontmodHeight, -1);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
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
	RenderTools()->DrawUIRect(&c, ColorRGBA(1, 1, 1, 0.25f * ButtonColorMul(pID)), CUI::CORNER_ALL, 3.0f);

	bool CheckAble = *pBoxText == 'X';
	if(CheckAble)
	{
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		UI()->DoLabel(&c, "\xEE\x97\x8D", c.h * ms_FontmodHeight, 0, -1, 0);
		TextRender()->SetCurFont(NULL);
	}
	else
		UI()->DoLabel(&c, pBoxText, c.h * ms_FontmodHeight, 0, -1, 0);
	TextRender()->SetRenderFlags(0);
	UI()->DoLabel(&t, pText, c.h * ms_FontmodHeight, -1);

	return UI()->DoButtonLogic(pID, pText, 0, pRect);
}

void CMenus::DoLaserPreview(const CUIRect *pRect, const ColorHSLA LaserOutlineColor, const ColorHSLA LaserInnerColor)
{
	ColorRGBA LaserRGB;
	CUIRect Section = *pRect;
	vec2 From = vec2(Section.x, Section.y + Section.h / 2.0f);
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
	Graphics()->TextureSet(GameClient()->m_ParticlesSkin.m_SpriteParticleSplat[SpriteIndex]);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(time_get());
	Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
	IGraphics::CQuadItem QuadItem(Pos.x, Pos.y, 24, 24);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
	QuadItem = IGraphics::CQuadItem(Pos.x, Pos.y, 20, 20);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();

	Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponLaser);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_WEAPON_LASER_BODY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->DrawSprite(Section.x, Section.y + Section.h / 2.0f, 60.0f);
	Graphics()->QuadsEnd();
}

ColorHSLA CMenus::DoLine_ColorPicker(int *pResetID, const float LineSize, const float WantedPickerPosition, const float LabelSize, const float BottomMargin, CUIRect *pMainRect, const char *pText, unsigned int *pColorValue, const ColorRGBA DefaultColor, bool CheckBoxSpacing, bool UseCheckBox, int *pCheckBoxValue)
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

	UI()->DoLabelScaled(&Label, pText, LabelSize, -1);

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

	if(DoButton_Menu(pResetID, Localize("Reset"), 0, &Button, 0, CUI::CORNER_ALL, 8.0f, 0, vec4(1, 1, 1, 0.5f), vec4(1, 1, 1, 0.25f), 1, true))
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

int CMenus::DoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, bool UseScroll, int Current, int Min, int Max, int Step, float Scale, bool IsHex, float Round, ColorRGBA *Color)
{
	// logic
	static float s_Value;
	static char s_NumStr[64];
	static void *s_LastTextpID = pID;
	int Inside = UI()->MouseInside(pRect);

	if(Inside)
		UI()->SetHotItem(pID);

	if(UI()->MouseButtonReleased(1) && UI()->HotItem() == pID)
	{
		s_LastTextpID = pID;
		ms_ValueSelectorTextMode = true;
		if(IsHex)
			str_format(s_NumStr, sizeof(s_NumStr), "%06X", Current);
		else
			str_format(s_NumStr, sizeof(s_NumStr), "%d", Current);
	}

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			//m_LockMouse = false;
			UI()->SetActiveItem(0);
			ms_ValueSelectorTextMode = false;
		}
	}

	if(ms_ValueSelectorTextMode && s_LastTextpID == pID)
	{
		static float s_NumberBoxID = 0;
		DoEditBox(&s_NumberBoxID, pRect, s_NumStr, sizeof(s_NumStr), 10.0f, &s_NumberBoxID, false, CUI::CORNER_ALL);

		UI()->SetActiveItem(&s_NumberBoxID);

		if(Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER) ||
			((UI()->MouseButtonClicked(1) || UI()->MouseButtonClicked(0)) && !Inside))
		{
			if(IsHex)
				Current = clamp(str_toint_base(s_NumStr, 16), Min, Max);
			else
				Current = clamp(str_toint(s_NumStr), Min, Max);
			//m_LockMouse = false;
			UI()->SetActiveItem(0);
			ms_ValueSelectorTextMode = false;
		}

		if(Input()->KeyIsPressed(KEY_ESCAPE))
		{
			//m_LockMouse = false;
			UI()->SetActiveItem(0);
			ms_ValueSelectorTextMode = false;
		}
	}
	else
	{
		if(UI()->ActiveItem() == pID)
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
		RenderTools()->DrawUIRect(pRect, *Color, CUI::CORNER_ALL, Round);
		UI()->DoLabel(pRect, aBuf, 10, 0, -1);
	}

	return Current;
}

int CMenus::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners, const char *pEmptyText)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;
	static int s_AtIndex = 0;
	static bool s_DoScroll = false;
	static float s_ScrollStart = 0.0f;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		int Len = str_length(pStr);
		if(Len == 0)
			s_AtIndex = 0;

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_V))
		{
			const char *Text = Input()->GetClipboardText();
			if(Text)
			{
				int Offset = str_length(pStr);
				int CharsLeft = StrSize - Offset - 1;
				char *pCur = pStr + Offset;
				str_utf8_copy(pCur, Text, CharsLeft);
				for(int i = 0; i < CharsLeft; i++)
				{
					if(pCur[i] == 0)
						break;
					else if(pCur[i] == '\r')
						pCur[i] = ' ';
					else if(pCur[i] == '\n')
						pCur[i] = ' ';
				}
				s_AtIndex = str_length(pStr);
				ReturnValue = true;
			}
		}

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_C))
		{
			Input()->SetClipboardText(pStr);
		}

		/* TODO: Doesn't work, SetClipboardText doesn't retain the string quickly enough?
		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_X))
		{
			Input()->SetClipboardText(pStr);
			pStr[0] = '\0';
			s_AtIndex = 0;
			ReturnValue = true;
		}
		*/

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_U))
		{
			pStr[0] = '\0';
			s_AtIndex = 0;
			ReturnValue = true;
		}

		if(Inside && UI()->MouseButton(0))
		{
			s_DoScroll = true;
			s_ScrollStart = UI()->MouseX();
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for(int i = 1; i <= Len; i++)
			{
				if(TextRender()->TextWidth(0, FontSize, pStr, i, std::numeric_limits<float>::max()) - *Offset > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if(i == Len)
					s_AtIndex = Len;
			}
		}
		else if(!UI()->MouseButton(0))
			s_DoScroll = false;
		else if(s_DoScroll)
		{
			// do scrolling
			if(UI()->MouseX() < pRect->x && s_ScrollStart - UI()->MouseX() > 10.0f)
			{
				s_AtIndex = maximum(0, s_AtIndex - 1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
			else if(UI()->MouseX() > pRect->x + pRect->w && UI()->MouseX() - s_ScrollStart > 10.0f)
			{
				s_AtIndex = minimum(Len, s_AtIndex + 1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
		}

		for(int i = 0; i < m_NumInputEvents; i++)
		{
			Len = str_length(pStr);
			int NumChars = Len;
			ReturnValue |= CLineInput::Manipulate(m_aInputEvents[i], pStr, StrSize, StrSize, &Len, &s_AtIndex, &NumChars);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			s_AtIndex = minimum(s_AtIndex, str_length(pStr));
			s_DoScroll = false;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if(UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(pID);
	}

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, ColorRGBA(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.VMargin(2.0f, &Textbox);
	Textbox.HMargin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pDisplayStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars) - 1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	char aDispEditingText[128 + IInput::INPUT_TEXT_SIZE + 2] = {0};
	int DispCursorPos = s_AtIndex;
	if(UI()->LastActiveItem() == pID && Input()->GetIMEEditingTextLength() > -1)
	{
		int EditingTextCursor = Input()->GetEditingCursor();
		str_copy(aDispEditingText, pDisplayStr, sizeof(aDispEditingText));
		char aEditingText[IInput::INPUT_TEXT_SIZE + 2];
		if(Hidden)
		{
			// Do not show editing text in password field
			str_copy(aEditingText, "[*]", sizeof(aEditingText));
			EditingTextCursor = 1;
		}
		else
		{
			str_format(aEditingText, sizeof(aEditingText), "[%s]", Input()->GetIMEEditingText());
		}
		int NewTextLen = str_length(aEditingText);
		int CharsLeft = (int)sizeof(aDispEditingText) - str_length(aDispEditingText) - 1;
		int FillCharLen = minimum(NewTextLen, CharsLeft);
		for(int i = str_length(aDispEditingText) - 1; i >= s_AtIndex; i--)
			aDispEditingText[i + FillCharLen] = aDispEditingText[i];
		for(int i = 0; i < FillCharLen; i++)
			aDispEditingText[s_AtIndex + i] = aEditingText[i];
		DispCursorPos = s_AtIndex + EditingTextCursor + 1;
		pDisplayStr = aDispEditingText;
		UpdateOffset = true;
	}

	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = pEmptyText;
		TextRender()->TextColor(1, 1, 1, 0.75f);
	}

	DispCursorPos = minimum(DispCursorPos, str_length(pDisplayStr));

	// check if the text has to be moved
	if(UI()->LastActiveItem() == pID && !JustGotActive && (UpdateOffset || m_NumInputEvents))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		if(w - *Offset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1, std::numeric_limits<float>::max());
			do
			{
				*Offset += minimum(wt - *Offset - Textbox.w, Textbox.w / 3);
			} while(w - *Offset > Textbox.w + 0.0001f);
		}
		else if(w - *Offset < 0.0f)
		{
			// move to the right
			do
			{
				*Offset = maximum(0.0f, *Offset - Textbox.w / 3);
			} while(w - *Offset < -0.0001f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *Offset;

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);

	TextRender()->TextColor(1, 1, 1, 1);

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float OnePixelWidth = ((ScreenX1 - ScreenX0) / Graphics()->ScreenWidth());

	// render the cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		Textbox.x += w;

		if((2 * time_get() / time_freq()) % 2)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 0, 0.3f);
			IGraphics::CQuadItem CursorTBack(Textbox.x - (OnePixelWidth * 2.0f) / 2.0f, Textbox.y, OnePixelWidth * 2 * 2.0f, Textbox.h);
			Graphics()->QuadsDrawTL(&CursorTBack, 1);
			Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem CursorT(Textbox.x, Textbox.y + OnePixelWidth * 1.5f, OnePixelWidth * 2.0f, Textbox.h - OnePixelWidth * 1.5f * 2);
			Graphics()->QuadsDrawTL(&CursorT, 1);
			Graphics()->QuadsEnd();
		}

		Input()->SetEditingPosition(Textbox.x, Textbox.y + FontSize);
	}

	UI()->ClipDisable();

	return ReturnValue;
}

int CMenus::DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners, const char *pEmptyText)
{
	bool ReturnValue = false;
	CUIRect EditBox;
	CUIRect ClearButton;
	pRect->VSplitRight(15.0f, &EditBox, &ClearButton);
	if(DoEditBox(pID, &EditBox, pStr, StrSize, FontSize, Offset, Hidden, Corners & ~CUI::CORNER_R, pEmptyText))
	{
		ReturnValue = true;
	}

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
	RenderTools()->DrawUIRect(&ClearButton, ColorRGBA(1, 1, 1, 0.33f * ButtonColorMul(pClearID)), Corners & ~CUI::CORNER_L, 3.0f);
	UI()->DoLabel(&ClearButton, "×", ClearButton.h * ms_FontmodHeight, 0, -1, 0);
	TextRender()->SetRenderFlags(0);
	if(UI()->DoButtonLogic(pClearID, "×", 0, &ClearButton))
	{
		pStr[0] = 0;
		UI()->SetActiveItem(pID);
		ReturnValue = true;
	}
	return ReturnValue;
}

float CMenus::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h - Handle.h) * Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			m_MouseSlow = true;

		float Min = pRect->y;
		float Max = pRect->h - Handle.h;
		float Cur = UI()->MouseY() - OffsetY;
		ReturnValue = (Cur - Min) / Max;
		if(ReturnValue < 0.0f)
			ReturnValue = 0.0f;
		if(ReturnValue > 1.0f)
			ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetY = UI()->MouseY() - Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	RenderTools()->DrawUIRect(pRect, ColorRGBA(0, 0, 0, 0.25f), CUI::CORNER_ALL, 2.5f);

	CUIRect Slider = Handle;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 2.5f);
	Slider.Margin(2, &Slider);

	float ColorSlider = 0;

	if(UI()->ActiveItem() == pID)
		ColorSlider = 1.0f;
	else if(UI()->HotItem() == pID)
		ColorSlider = 0.9f;
	else
		ColorSlider = 0.75f;

	RenderTools()->DrawUIRect(&Slider, ColorRGBA(ColorSlider, ColorSlider, ColorSlider, 0.75f), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}

float CMenus::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetX;
	pRect->VSplitLeft(33, &Handle, 0);

	Handle.x += (pRect->w - Handle.w) * Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			m_MouseSlow = true;

		float Min = pRect->x;
		float Max = pRect->w - Handle.w;
		float Cur = UI()->MouseX() - OffsetX;
		ReturnValue = (Cur - Min) / Max;
		if(ReturnValue < 0.0f)
			ReturnValue = 0.0f;
		if(ReturnValue > 1.0f)
			ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetX = UI()->MouseX() - Handle.x;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	CUIRect Rail;
	pRect->HMargin(5.0f, &Rail);
	RenderTools()->DrawUIRect(&Rail, ColorRGBA(1, 1, 1, 0.25f), 0, 0.0f);

	CUIRect Slider = Handle;
	Slider.h = Rail.y - Slider.y;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_T, 2.5f);
	Slider.y = Rail.y + Rail.h;
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_B, 2.5f);

	Slider = Handle;
	Slider.Margin(5.0f, &Slider);
	RenderTools()->DrawUIRect(&Slider, ColorRGBA(1, 1, 1, 0.25f * ButtonColorMul(pID)), CUI::CORNER_ALL, 2.5f);

	return ReturnValue;
}

int CMenus::DoKeyReader(void *pID, const CUIRect *pRect, int Key, int Modifier, int *NewModifier)
{
	// process
	static void *pGrabbedID = 0;
	static bool MouseReleased = true;
	static int ButtonUsed = 0;
	int Inside = UI()->MouseInside(pRect);
	int NewKey = Key;
	*NewModifier = Modifier;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && pGrabbedID == pID)
		MouseReleased = true;

	if(UI()->ActiveItem() == pID)
	{
		if(m_Binder.m_GotKey)
		{
			// abort with escape key
			if(m_Binder.m_Key.m_Key != KEY_ESCAPE)
			{
				NewKey = m_Binder.m_Key.m_Key;
				*NewModifier = m_Binder.m_Modifier;
			}
			m_Binder.m_GotKey = false;
			UI()->SetActiveItem(0);
			MouseReleased = false;
			pGrabbedID = pID;
		}

		if(ButtonUsed == 1 && !UI()->MouseButton(1))
		{
			if(Inside)
				NewKey = 0;
			UI()->SetActiveItem(0);
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
				ButtonUsed = 0;
			}

			if(UI()->MouseButton(1))
			{
				UI()->SetActiveItem(pID);
				ButtonUsed = 1;
			}
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// draw
	if(UI()->ActiveItem() == pID && ButtonUsed == 0)
		DoButton_KeySelect(pID, "???", 0, pRect);
	else
	{
		if(Key)
		{
			char aBuf[64];
			if(*NewModifier)
				str_format(aBuf, sizeof(aBuf), "%s+%s", CBinds::GetModifierName(*NewModifier), Input()->KeyName(Key));
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
		static int s_StartButton = 0;

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

		const char *pHomeScreenButtonLabel = "\xEE\xA2\x8A";
		if(GotNewsOrUpdate)
		{
			pHomeScreenButtonLabel = "\xEE\x80\xB1";
			pHomeButtonColor = &HomeButtonColorAlert;
			pHomeButtonColorHover = &HomeButtonColorAlertHover;
		}

		if(DoButton_MenuTab(&s_StartButton, pHomeScreenButtonLabel, false, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_HOME], pHomeButtonColor, pHomeButtonColor, pHomeButtonColorHover, 10.0f, 0))
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
			static int s_NewsButton = 0;
			if(DoButton_MenuTab(&s_NewsButton, Localize("News"), m_ActivePage == PAGE_NEWS, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_NEWS]))
			{
				NewPage = PAGE_NEWS;
				m_DoubleClickIndex = -1;
			}
		}
		else if(m_ActivePage == PAGE_DEMOS)
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_DemosButton = 0;
			if(DoButton_MenuTab(&s_DemosButton, Localize("Demos"), m_ActivePage == PAGE_DEMOS, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_DEMOS]))
			{
				DemolistPopulate();
				NewPage = PAGE_DEMOS;
				m_DoubleClickIndex = -1;
			}
		}
		else
		{
			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_InternetButton = 0;
			if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), m_ActivePage == PAGE_INTERNET, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_INTERNET]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_INTERNET)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				NewPage = PAGE_INTERNET;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_LanButton = 0;
			if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), m_ActivePage == PAGE_LAN, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_LAN]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_LAN)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
				NewPage = PAGE_LAN;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(100.0f, &Button, &Box);
			static int s_FavoritesButton = 0;
			if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), m_ActivePage == PAGE_FAVORITES, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_FAVORITES]))
			{
				if(ServerBrowser()->GetCurrentType() != IServerBrowser::TYPE_FAVORITES)
					ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
				NewPage = PAGE_FAVORITES;
				m_DoubleClickIndex = -1;
			}

			Box.VSplitLeft(90.0f, &Button, &Box);
			static int s_DDNetButton = 0;
			if(DoButton_MenuTab(&s_DDNetButton, "DDNet", m_ActivePage == PAGE_DDNET, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_DDNET]))
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
			static int s_KoGButton = 0;
			if(DoButton_MenuTab(&s_KoGButton, "KoG", m_ActivePage == PAGE_KOG, &Button, CUI::CORNER_T, &m_aAnimatorsBigPage[BIG_TAB_KOG]))
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
		static int s_GameButton = 0;
		if(DoButton_MenuTab(&s_GameButton, Localize("Game"), m_ActivePage == PAGE_GAME, &Button, CUI::CORNER_TL))
			NewPage = PAGE_GAME;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static int s_PlayersButton = 0;
		if(DoButton_MenuTab(&s_PlayersButton, Localize("Players"), m_ActivePage == PAGE_PLAYERS, &Button, 0))
			NewPage = PAGE_PLAYERS;

		Box.VSplitLeft(130.0f, &Button, &Box);
		static int s_ServerInfoButton = 0;
		if(DoButton_MenuTab(&s_ServerInfoButton, Localize("Server info"), m_ActivePage == PAGE_SERVER_INFO, &Button, 0))
			NewPage = PAGE_SERVER_INFO;

		Box.VSplitLeft(90.0f, &Button, &Box);
		static int s_NetworkButton = 0;
		if(DoButton_MenuTab(&s_NetworkButton, Localize("Browser"), m_ActivePage == PAGE_NETWORK, &Button, 0))
			NewPage = PAGE_NETWORK;

		{
			static int s_GhostButton = 0;
			if(GameClient()->m_GameInfo.m_Race)
			{
				Box.VSplitLeft(90.0f, &Button, &Box);
				if(DoButton_MenuTab(&s_GhostButton, Localize("Ghost"), m_ActivePage == PAGE_GHOST, &Button, 0))
					NewPage = PAGE_GHOST;
			}
		}

		Box.VSplitLeft(100.0f, &Button, &Box);
		Box.VSplitLeft(4.0f, 0, &Box);
		static int s_CallVoteButton = 0;
		if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage == PAGE_CALLVOTE, &Button, CUI::CORNER_TR))
			NewPage = PAGE_CALLVOTE;
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	Box.VSplitRight(33.0f, &Box, &Button);
	static int s_QuitButton = 0;
	ColorRGBA QuitColor(1, 0, 0, 0.5f);
	if(DoButton_MenuTab(&s_QuitButton, "\xEE\x97\x8D", 0, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_QUIT], NULL, NULL, &QuitColor, 10.0f, 0))
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
	static int s_SettingsButton = 0;

	if(DoButton_MenuTab(&s_SettingsButton, "\xEE\xA2\xB8", m_ActivePage == PAGE_SETTINGS, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_SETTINGS], NULL, NULL, NULL, 10.0f, 0))
		NewPage = PAGE_SETTINGS;

	Box.VSplitRight(10.0f, &Box, &Button);
	Box.VSplitRight(33.0f, &Box, &Button);
	static int s_EditorButton = 0;
	if(DoButton_MenuTab(&s_EditorButton, "\xEE\x8F\x89", 0, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_EDITOR], NULL, NULL, NULL, 10.0f, 0))
	{
		g_Config.m_ClEditor = 1;
	}

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		Box.VSplitRight(10.0f, &Box, &Button);
		Box.VSplitRight(33.0f, &Box, &Button);
		static int s_DemoButton = 0;

		if(DoButton_MenuTab(&s_DemoButton, "\xEE\x80\xAC", m_ActivePage == PAGE_DEMOS, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_DEMOBUTTON], NULL, NULL, NULL, 10.0f, 0))
			NewPage = PAGE_DEMOS;

		Box.VSplitRight(10.0f, &Box, &Button);
		Box.VSplitRight(33.0f, &Box, &Button);
		static int s_ServerButton = 0;

		if(DoButton_MenuTab(&s_ServerButton, "\xEE\xA0\x8B", m_ActivePage == g_Config.m_UiPage, &Button, CUI::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_SERVER], NULL, NULL, NULL, 10.0f, 0))
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

void CMenus::RenderLoading()
{
	// TODO: not supported right now due to separate render thread

	static int64 LastLoadRender = 0;
	float Percent = m_LoadCurrent++ / (float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get() - LastLoadRender < time_freq() / 60)
		return;

	LastLoadRender = time_get();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	if(!m_pBackground->Render())
	{
		RenderBackground();
	}

	float w = 700;
	float h = 200;
	float x = Screen.w / 2 - w / 2;
	float y = Screen.h / 2 - h / 2;

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.50f);
	RenderTools()->DrawRoundRect(x, y, w, h, 40.0f);
	Graphics()->QuadsEnd();

	const char *pCaption = Localize("Loading DDNet Client");

	CUIRect r;
	r.x = x;
	r.y = y + 20;
	r.w = w;
	r.h = h - 130;
	UI()->DoLabel(&r, pCaption, 48.0f, 0, -1);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 0.75f);
	RenderTools()->DrawRoundRect(x + 40, y + h - 75, (w - 80) * Percent, 25, 5.0f);
	Graphics()->QuadsEnd();

	Graphics()->Swap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	g_Config.m_UiUnreadNews = false;

	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B, 10.0f);

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
			UI()->DoLabelScaled(&Label, aLine + 1, 20.0f, -1);
		}
		else
		{
			MainView.HSplitTop(20.0f, &Label, &MainView);
			UI()->DoLabelScaled(&Label, aLine, 15.f, -1, -1);
		}
	}
}

void CMenus::OnInit()
{
	if(g_Config.m_ClShowWelcome)
		m_Popup = POPUP_LANGUAGE;
	if(g_Config.m_ClSkipStartMenu)
		m_ShowStart = false;

	m_RefreshButton.Init(UI());
	m_ConnectButton.Init(UI());

	Console()->Chain("add_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("remove_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);

	m_TextureBlob = Graphics()->LoadTexture("blob.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	// setup load amount
	const int NumMenuImages = 5;
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages + NumMenuImages;
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;

	// load menu images
	m_lMenuImages.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "menuimages", MenuImageScan, this);
}

void CMenus::PopupMessage(const char *pTopic, const char *pBody, const char *pButton)
{
	// reset active item
	UI()->SetActiveItem(0);

	str_copy(m_aMessageTopic, pTopic, sizeof(m_aMessageTopic));
	str_copy(m_aMessageBody, pBody, sizeof(m_aMessageBody));
	str_copy(m_aMessageButton, pButton, sizeof(m_aMessageButton));
	m_Popup = POPUP_MESSAGE;
}

void CMenus::PopupWarning(const char *pTopic, const char *pBody, const char *pButton, int64 Duration)
{
	dbg_msg(pTopic, "%s", pBody);

	// reset active item
	UI()->SetActiveItem(0);

	str_copy(m_aMessageTopic, pTopic, sizeof(m_aMessageTopic));
	str_copy(m_aMessageBody, pBody, sizeof(m_aMessageBody));
	str_copy(m_aMessageButton, pButton, sizeof(m_aMessageButton));
	m_Popup = POPUP_WARNING;
	SetActive(true);

	m_PopupWarningDuration = Duration;
	m_PopupWarningLastTime = time_get_microseconds();
}

bool CMenus::CanDisplayWarning()
{
	return m_Popup == POPUP_NONE;
}

void CMenus::RenderColorPicker()
{
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
		UI()->SetActiveItem(0);
		return;
	}

	// Render
	ColorRGBA BackgroundColor(0.1f, 0.1f, 0.1f, 1.0f);
	RenderTools()->DrawUIRect(&PickerRect, BackgroundColor, 0, 0);

	CUIRect ColorsArea, HueArea, ValuesHitbox, BottomArea, HSVHRect, HSVSRect, HSVVRect, HEXRect, ALPHARect;
	PickerRect.Margin(3, &ColorsArea);

	ColorsArea.HSplitBottom(ms_ColorPicker.ms_Height - 140.0f, &ColorsArea, &ValuesHitbox);
	ColorsArea.VSplitRight(20, &ColorsArea, &HueArea);

	BottomArea = ValuesHitbox;
	BottomArea.HSplitTop(3, 0x0, &BottomArea);
	HueArea.VSplitLeft(3, 0x0, &HueArea);

	BottomArea.HSplitTop(20, &HSVHRect, &BottomArea);
	BottomArea.HSplitTop(3, 0x0, &BottomArea);

	constexpr float ValuePadding = 5.0f;
	const float HSVValueWidth = (HSVHRect.w - ValuePadding * 2) / 3.0f;
	const float HEXValueWidth = HSVValueWidth * 2 + ValuePadding;

	HSVHRect.VSplitLeft(HSVValueWidth, &HSVHRect, &HSVSRect);
	HSVSRect.VSplitLeft(ValuePadding, 0x0, &HSVSRect);
	HSVSRect.VSplitLeft(HSVValueWidth, &HSVSRect, &HSVVRect);
	HSVVRect.VSplitLeft(ValuePadding, 0x0, &HSVVRect);

	BottomArea.HSplitTop(20, &HEXRect, &BottomArea);
	HEXRect.VSplitLeft(HEXValueWidth, &HEXRect, &ALPHARect);
	ALPHARect.VSplitLeft(ValuePadding, 0x0, &ALPHARect);

	if(UI()->MouseButtonReleased(1) && !UI()->MouseInside(&ValuesHitbox))
	{
		ms_ColorPicker.m_Active = false;
		ms_ValueSelectorTextMode = false;
		UI()->SetActiveItem(0);
		return;
	}

	ColorRGBA BlackColor(0, 0, 0, 0.5f);

	RenderTools()->DrawUIRect(&HueArea, BlackColor, 0, 0);
	HueArea.Margin(1, &HueArea);

	RenderTools()->DrawUIRect(&ColorsArea, BlackColor, 0, 0);
	ColorsArea.Margin(1, &ColorsArea);

	unsigned int H = ms_ColorPicker.m_HSVColor / (1 << 16);
	unsigned int S = (ms_ColorPicker.m_HSVColor - (H << 16)) / (1 << 8);
	unsigned int V = ms_ColorPicker.m_HSVColor % (1 << 8);

	ColorHSVA PickerColorHSV(ms_ColorPicker.m_HSVColor);

	// Color Area
	ColorRGBA rgb;
	rgb = color_cast<ColorRGBA, ColorHSVA>(ColorHSVA(PickerColorHSV.x, 0.0f, 1.0f));
	vec4 TL(rgb.r, rgb.g, rgb.b, 1.0f);
	rgb = color_cast<ColorRGBA, ColorHSVA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f));
	vec4 TR(rgb.r, rgb.g, rgb.b, 1.0f);
	rgb = color_cast<ColorRGBA, ColorHSVA>(ColorHSVA(PickerColorHSV.x, 0.0f, 1.0f));
	vec4 BL(rgb.r, rgb.g, rgb.b, 1.0f);
	rgb = color_cast<ColorRGBA, ColorHSVA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f));
	vec4 BR(rgb.r, rgb.g, rgb.b, 1.0f);

	RenderTools()->DrawUIRect4NoRounding(&ColorsArea, TL, TR, BL, BR);

	TL = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	TR = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	BL = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	BR = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	RenderTools()->DrawUIRect4NoRounding(&ColorsArea, TL, TR, BL, BR);

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
		RenderTools()->DrawUIRect4NoRounding(&HuePartialArea, TL, TL, BL, BL);
	}

	//Editboxes Area
	ColorRGBA EditboxBackground(0, 0, 0, 0.4f);

	static int RGBRID = 0;
	static int RGBGID = 0;
	static int RGBBID = 0;

	H = DoValueSelector(&RGBRID, &HSVHRect, "H:", true, H, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);
	S = DoValueSelector(&RGBGID, &HSVSRect, "S:", true, S, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);
	V = DoValueSelector(&RGBBID, &HSVVRect, "V:", true, V, 0, 255, 1, 1, false, 5.0f, &EditboxBackground);

	PickerColorHSV = ColorHSVA((H << 16) + (S << 8) + V);

	unsigned int HEX = color_cast<ColorRGBA, ColorHSVA>(PickerColorHSV).Pack(false);
	static int HEXID = 0;

	unsigned int NEWHEX = DoValueSelector(&HEXID, &HEXRect, "HEX:", false, HEX, 0, 0xFFFFFF, 1, 1, true, 5.0f, &EditboxBackground);

	if(HEX != NEWHEX)
		PickerColorHSV = color_cast<ColorHSVA, ColorRGBA>(NEWHEX);

	// TODO : ALPHA SUPPORT
	//static int ALPHAID = 0;
	UI()->DoLabel(&ALPHARect, "A: 255", 10, 0, -1);
	RenderTools()->DrawUIRect(&ALPHARect, ColorRGBA(0, 0, 0, 0.65f), CUI::CORNER_ALL, 5.0f);

	// Logic
	float PickerX, PickerY;

	static int ColorPickerID = 0;
	static int HuePickerID = 0;

	if(UI()->DoPickerLogic(&ColorPickerID, &ColorsArea, &PickerX, &PickerY))
	{
		PickerColorHSV.y = PickerX / ColorsArea.w;
		PickerColorHSV.z = 1.0f - PickerY / ColorsArea.h;
	}

	if(UI()->DoPickerLogic(&HuePickerID, &HueArea, &PickerX, &PickerY))
		PickerColorHSV.x = 1.0f - PickerY / HueArea.h;

	// Marker Color Area
	float MarkerX = ColorsArea.x + ColorsArea.w * PickerColorHSV.y;
	float MarkerY = ColorsArea.y + ColorsArea.h * (1.0f - PickerColorHSV.z);

	int MarkerOutlineInd = PickerColorHSV.z > 0.5f ? 0.0f : 1.0f;
	ColorRGBA MarkerOutline(MarkerOutlineInd, MarkerOutlineInd, MarkerOutlineInd, 1.0f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(MarkerOutline);
	RenderTools()->DrawCircle(MarkerX, MarkerY, 4.5f, 32);
	Graphics()->SetColor(color_cast<ColorRGBA, ColorHSVA>(PickerColorHSV));
	RenderTools()->DrawCircle(MarkerX, MarkerY, 3.5f, 32);
	Graphics()->QuadsEnd();

	// Marker Hue Area
	CUIRect HueMarker;
	HueArea.Margin(-2.5f, &HueMarker);
	HueMarker.h = 6.5f;
	HueMarker.y = (HueArea.y + HueArea.h * (1.0f - PickerColorHSV.x)) - HueMarker.h / 2.0f;

	ColorRGBA HueMarkerColor = color_cast<ColorRGBA, ColorHSVA>(ColorHSVA(PickerColorHSV.x, 1, 1, 1));
	const float HMOColor = PickerColorHSV.x > 0.75f ? 1.0f : 0.0f;
	ColorRGBA HueMarkerOutline(HMOColor, HMOColor, HMOColor, 1);

	RenderTools()->DrawUIRect(&HueMarker, HueMarkerOutline, CUI::CORNER_ALL, 1.2f);
	HueMarker.Margin(1.2f, &HueMarker);
	RenderTools()->DrawUIRect(&HueMarker, HueMarkerColor, CUI::CORNER_ALL, 1.2f);

	ms_ColorPicker.m_HSVColor = PickerColorHSV.Pack(false);
	*ms_ColorPicker.m_pColor = color_cast<ColorHSLA, ColorHSVA>(PickerColorHSV).Pack(false);
}

int CMenus::Render()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && m_Popup == POPUP_NONE)
		return 0;

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	m_MouseSlow = false;

	static int s_Frame = 0;
	if(s_Frame == 0)
	{
		m_MenuPage = g_Config.m_UiPage;
		s_Frame++;
	}
	else if(s_Frame == 1)
	{
		m_pClient->m_pSounds->Enqueue(CSounds::CHN_MUSIC, SOUND_MENU);
		s_Frame++;
		m_DoubleClickIndex = -1;

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
		if(m_ShowStart && Client()->State() == IClient::STATE_OFFLINE)
		{
			m_pBackground->ChangePosition(CMenuBackground::POS_START);
			RenderStartMenu(Screen);
		}
		else
		{
			Screen.HSplitTop(24.0f, &TabBar, &MainView);

			if(Client()->State() == IClient::STATE_OFFLINE && m_EscapePressed)
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
					RenderGame(MainView);
				else if(m_GamePage == PAGE_PLAYERS)
					RenderPlayers(MainView);
				else if(m_GamePage == PAGE_SERVER_INFO)
					RenderServerInfo(MainView);
				else if(m_GamePage == PAGE_NETWORK)
					RenderInGameNetwork(MainView);
				else if(m_GamePage == PAGE_GHOST)
					RenderGhost(MainView);
				else if(m_GamePage == PAGE_CALLVOTE)
					RenderServerControl(MainView);
				else if(m_GamePage == PAGE_SETTINGS)
					RenderSettings(MainView);
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
		//UI()->SetActiveItem(0);
		char aBuf[1536];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		int ExtraAlign = 0;

		ColorRGBA BgColor = ColorRGBA{0.0f, 0.0f, 0.0f, 0.5f};
		if(m_Popup == POPUP_MESSAGE)
		{
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			pTitle = Localize("Connecting to");
			pExtraText = g_Config.m_UiServerAddress; // TODO: query the client about the address
			pButtonText = Localize("Abort");
			if(Client()->MapDownloadTotalsize() > 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Downloading map"), Client()->MapDownloadName());
				pTitle = aBuf;
				pExtraText = "";
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
		else if(m_Popup == POPUP_PURE)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Localize("The server is running a non-standard tuning on a pure game type.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			pTitle = Localize("Delete demo");
			pExtraText = Localize("Are you sure that you want to delete the demo?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			pTitle = Localize("Rename demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			pTitle = Localize("Render demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_REPLACE_VIDEO)
		{
			pTitle = Localize("Replace video");
			pExtraText = Localize("File already exists, do you want to overwrite it?");
			ExtraAlign = -1;
		}
#endif
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			pTitle = Localize("Remove friend");
			pExtraText = Localize("Are you sure that you want to remove the player from your friends list?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SOUNDERROR)
		{
			pTitle = Localize("Sound error");
			pExtraText = Localize("The audio device couldn't be initialised.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pExtraText = "";
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DISCONNECT)
		{
			pTitle = Localize("Disconnect");
			pExtraText = Localize("Are you sure that you want to disconnect?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DISCONNECT_DUMMY)
		{
			pTitle = Localize("Disconnect Dummy");
			pExtraText = Localize("Are you sure that you want to disconnect your dummy?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to DDNet");
			str_format(aBuf, sizeof(aBuf), "%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s",
				Localize("DDraceNetwork is a cooperative online game where the goal is for you and your group of tees to reach the finish line of the map. As a newcomer you should start on Novice servers, which host the easiest maps. Consider the ping to choose a server close to you."),
				Localize("The maps contain freeze, which temporarily make a tee unable to move. You have to work together to get through these parts."),
				Localize("The mouse wheel changes weapons. Hammer (left mouse) can be used to hit other tees and wake them up from being frozen."),
				Localize("Hook (right mouse) can be used to swing through the map and to hook other tees to you."),
				Localize("Most importantly communication is key: There is no tutorial so you'll have to chat (t key) with other players to learn the basics and tricks of the game."),
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
			}
			else if(Client()->m_Points >= 0)
			{
				m_Popup = POPUP_NONE;
			}
			else
			{
				pExtraText = Localize("Checking for existing player with your name");
			}
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_WARNING)
		{
			BgColor = ColorRGBA{0.5f, 0.0f, 0.0f, 0.7f};
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
			ExtraAlign = -1;
		}

		CUIRect Box, Part;
		Box = Screen;
		if(m_Popup != POPUP_FIRST_LAUNCH)
		{
			Box.VMargin(150.0f / UI()->Scale(), &Box);
			Box.HMargin(150.0f / UI()->Scale(), &Box);
		}

		// render the box
		RenderTools()->DrawUIRect(&Box, BgColor, CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f / UI()->Scale(), &Part);
		if(TextRender()->TextWidth(0, 24.f, pTitle, -1, -1.0f) > Part.w)
			UI()->DoLabelScaled(&Part, pTitle, 24.f, -1, (int)Part.w);
		else
			UI()->DoLabelScaled(&Part, pTitle, 24.f, 0);
		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f / UI()->Scale(), &Part);

		float FontSize = m_Popup == POPUP_FIRST_LAUNCH ? 16.0f : 20.f;

		if(ExtraAlign == -1)
			UI()->DoLabelScaled(&Part, pExtraText, FontSize, -1, (int)Part.w);
		else
		{
			if(TextRender()->TextWidth(0, FontSize, pExtraText, -1, -1.0f) > Part.w)
				UI()->DoLabelScaled(&Part, pExtraText, FontSize, -1, (int)Part.w);
			else
				UI()->DoLabelScaled(&Part, pExtraText, FontSize, 0, -1);
		}

		if(m_Popup == POPUP_QUIT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.VMargin(20.f / UI()->Scale(), &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s\n%s", Localize("There's an unsaved map in the editor, you might want to save it before you quit the game."), Localize("Quit anyway?"));
				UI()->DoLabelScaled(&Box, aBuf, 20.f, -1, Part.w - 20.0f);
			}

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Try again"), 0, &TryAgain) || m_EnterPressed)
			{
				Client()->Connect(g_Config.m_UiServerAddress, g_Config.m_Password);
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &Offset, true);
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || (m_EnterPressed && m_Popup != POPUP_CONNECTING))
			{
				Client()->Disconnect();
				m_Popup = POPUP_NONE;
			}

			if(Client()->MapDownloadTotalsize() > 0)
			{
				int64 Now = time_get();
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
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

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
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// progress bar
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				Part.VMargin(40.0f, &Part);
				RenderTools()->DrawUIRect(&Part, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 5.0f);
				Part.w = maximum(10.0f, (Part.w * Client()->MapDownloadAmount()) / Client()->MapDownloadTotalsize());
				RenderTools()->DrawUIRect(&Part, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), CUI::CORNER_ALL, 5.0f);
			}
		}
		else if(m_Popup == POPUP_LANGUAGE)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);
			RenderLanguageSelection(Box);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = POPUP_FIRST_LAUNCH;
		}
		else if(m_Popup == POPUP_COUNTRY)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);

			static int ActSelection = -2;
			if(ActSelection == -2)
				ActSelection = g_Config.m_BrFilterCountryIndex;
			static float s_ScrollValue = 0.0f;
			int OldSelected = -1;
			UiDoListboxStart(&s_ScrollValue, &Box, 50.0f, Localize("Country / Region"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

			for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
			{
				const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
				if(pEntry->m_CountryCode == ActSelection)
					OldSelected = i;

				CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i);
				if(Item.m_Visible)
				{
					CUIRect Label;
					Item.m_Rect.Margin(5.0f, &Item.m_Rect);
					Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
					float OldWidth = Item.m_Rect.w;
					Item.m_Rect.w = Item.m_Rect.h * 2;
					Item.m_Rect.x += (OldWidth - Item.m_Rect.w) / 2.0f;
					ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
					UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
				}
			}

			const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
			if(OldSelected != NewSelected)
				ActSelection = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;

			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EnterPressed)
			{
				g_Config.m_BrFilterCountryIndex = ActSelection;
				Client()->ServerBrowserUpdate();
				m_Popup = POPUP_NONE;
			}

			if(m_EscapePressed)
			{
				ActSelection = g_Config.m_BrFilterCountryIndex;
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// delete demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					if(Storage()->RemoveFile(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// rename demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length - 5] != '.' || str_comp_nocase(m_aCurrentDemoFile + Length - 4, "demo"))
						str_format(aBufNew, sizeof(aBufNew), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					if(Storage()->RenameFile(aBufOld, aBufNew, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
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
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
		}
#if defined(CONF_VIDEORECORDER)
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort, IncSpeed, DecSpeed, Button;

			Box.HSplitBottom(20.f, &Box, &Part);
#if defined(__ANDROID__)
			Box.HSplitBottom(60.f, &Box, &Part);
#else
			Box.HSplitBottom(24.f, &Box, &Part);
#endif
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// name video
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 3 || m_aCurrentDemoFile[Length - 4] != '.' || str_comp_nocase(m_aCurrentDemoFile + Length - 3, "mp4"))
						str_format(aBufNew, sizeof(aBufNew), "%s.mp4", m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s", m_aCurrentDemoFile);
					char aWholePath[1024];
					// store new video filename to origin buffer
					str_copy(m_aCurrentDemoFile, aBufNew, sizeof(m_aCurrentDemoFile));
					if(Storage()->FindFile(m_aCurrentDemoFile, "videos", IStorage::TYPE_ALL, aWholePath, sizeof(aWholePath)))
					{
						PopupMessage(Localize("Error"), Localize("Destination file already exist"), Localize("Ok"));
						m_Popup = POPUP_REPLACE_VIDEO;
					}
					else
					{
						const char *pError = Client()->DemoPlayer_Render(aBufOld, m_lDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
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
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowChat, Localize("Show chat"), g_Config.m_ClVideoShowChat, &Button))
				g_Config.m_ClVideoShowChat ^= 1;
			Part.VSplitLeft(112.0f, 0, &Part);
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoSndEnable, Localize("Use sounds"), g_Config.m_ClVideoSndEnable, &Button))
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
			static int s_SlowDownButton = 0;
			if(DoButton_Sprite(&s_SlowDownButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_SLOWER, 0, &Button, CUI::CORNER_ALL))
				DecDemoSpeed = true;

			// fastforward
			Part.VSplitLeft(5.0f, 0, &Part);
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			static int s_FastForwardButton = 0;
			if(DoButton_Sprite(&s_FastForwardButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_FASTER, 0, &Button, CUI::CORNER_ALL))
				IncDemoSpeed = true;

			// speed meter
			Part.VSplitLeft(8.0f, 0, &Part);
			char aBuffer[64];
			str_format(aBuffer, sizeof(aBuffer), "%s: ×%g", Localize("Speed"), g_aSpeeds[m_Speed]);
			UI()->DoLabel(&Part, aBuffer, 12.8f, -1);

			if(IncDemoSpeed)
				m_Speed = clamp(m_Speed + 1, 0, (int)(sizeof(g_aSpeeds) / sizeof(g_aSpeeds[0]) - 1));
			else if(DecDemoSpeed)
				m_Speed = clamp(m_Speed - 1, 0, (int)(sizeof(g_aSpeeds) / sizeof(g_aSpeeds[0]) - 1));

			Part.VSplitLeft(107.0f, 0, &Part);
			Part.VSplitLeft(ButtonSize, &Button, &Part);
			if(DoButton_CheckBox(&g_Config.m_ClVideoShowhud, Localize("Show ingame HUD"), g_Config.m_ClVideoShowhud, &Button))
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
			UI()->DoLabel(&Label, Localize("Video name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_RENDER_DEMO;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// render video
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Render(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType, m_aCurrentDemoFile, m_Speed);
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

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// remove friend
				if(m_FriendlistSelectedIndex >= 0)
				{
					m_pClient->Friends()->RemoveFriend(m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName,
						m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
				}
			}
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			CUIRect Label, TextBox;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			static int s_EnterButton = 0;
			if(DoButton_Menu(&s_EnterButton, Localize("Enter"), 0, &Part) || m_EnterPressed)
			{
				Client()->RequestDDNetInfo();
				if(g_Config.m_BrIndicateFinished)
					m_Popup = POPUP_POINTS;
				else
				{
					m_Popup = POPUP_NONE;
				}
			}

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(30.0f, 0, &Part);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s\n(%s)",
				Localize("Show DDNet map finishes in server browser"),
				Localize("transmits your player name to info2.ddnet.tw"));

			if(DoButton_CheckBox(&g_Config.m_BrIndicateFinished, aBuf, g_Config.m_BrIndicateFinished, &Part))
				g_Config.m_BrIndicateFinished ^= 1;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Nickname"), 16.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &Offset, false, CUI::CORNER_ALL, Client()->PlayerName());
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

			static int s_ButtonNo = 0;
			if(DoButton_Menu(&s_ButtonNo, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_FIRST_LAUNCH;

			static int s_ButtonYes = 0;
			if(DoButton_Menu(&s_ButtonYes, Localize("Yes"), 0, &Yes) || m_EnterPressed)
				m_Popup = POPUP_NONE;
		}
		else if(m_Popup == POPUP_WARNING)
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed || (time_get_microseconds() - m_PopupWarningLastTime >= m_PopupWarningDuration))
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

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
			{
				if(m_Popup == POPUP_DISCONNECTED && Client()->m_ReconnectTime > 0)
					Client()->m_ReconnectTime = 0;
				m_Popup = POPUP_NONE;
			}
		}

		if(m_Popup == POPUP_NONE)
			UI()->SetActiveItem(0);
	}
	return 0;
}

void CMenus::RenderThemeSelection(CUIRect MainView, bool Header)
{
	std::vector<CTheme> &ThemesRef = m_pBackground->GetThemes();

	int SelectedTheme = -1;
	for(int i = 0; i < (int)ThemesRef.size(); i++)
	{
		if(str_comp(ThemesRef[i].m_Name, g_Config.m_ClMenuMap) == 0)
		{
			SelectedTheme = i;
			break;
		}
	}

	static int s_ListBox = 0;
	static float s_ScrollValue = 0.0f;
	UiDoListboxStart(&s_ListBox, &MainView, 26.0f, Localize("Theme"), "", ThemesRef.size(), 1, -1, s_ScrollValue);

	for(int i = 0; i < (int)ThemesRef.size(); i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&ThemesRef[i].m_Name, i == SelectedTheme);

		CTheme &Theme = ThemesRef[i];

		if(!Item.m_Visible)
			continue;

		CUIRect Icon;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Icon, &Item.m_Rect);

		// draw icon if it exists
		if(Theme.m_IconTexture != -1)
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
		if(!Theme.m_Name[0])
			str_copy(aName, "(none)", sizeof(aName));
		else if(str_comp(Theme.m_Name, "auto") == 0)
			str_copy(aName, "(seasons)", sizeof(aName));
		else if(str_comp(Theme.m_Name, "rand") == 0)
			str_copy(aName, "(random)", sizeof(aName));
		else if(Theme.m_HasDay && Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s", Theme.m_Name.cstr());
		else if(Theme.m_HasDay && !Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (day)", Theme.m_Name.cstr());
		else if(!Theme.m_HasDay && Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (night)", Theme.m_Name.cstr());
		else // generic
			str_format(aName, sizeof(aName), "%s", Theme.m_Name.cstr());

		UI()->DoLabel(&Item.m_Rect, aName, 16 * ms_FontmodHeight, -1);
	}

	bool ItemActive = false;
	int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0, &ItemActive);

	if(ItemActive && NewSelected != SelectedTheme)
	{
		str_format(g_Config.m_ClMenuMap, sizeof(g_Config.m_ClMenuMap), "%s", ThemesRef[NewSelected].m_Name.cstr());
		m_pBackground->LoadMenuBackground(ThemesRef[NewSelected].m_HasDay, ThemesRef[NewSelected].m_HasNight);
	}
}

void CMenus::SetActive(bool Active)
{
	if(Active != m_MenuActive)
	{
		ms_ColorPicker.m_Active = false;
		Input()->SetIMEState(Active);
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

bool CMenus::OnMouseMove(float x, float y)
{
	m_LastInput = time_get();

	if(!m_MenuActive)
		return false;

	UI()->ConvertMouseMove(&x, &y);
	if(m_MouseSlow)
	{
		m_MousePos.x += x * 0.05f;
		m_MousePos.y += y * 0.05f;
	}
	else
	{
		m_MousePos.x += x;
		m_MousePos.y += y;
	}
	m_MousePos.x = clamp(m_MousePos.x, 0.f, (float)Graphics()->ScreenWidth());
	m_MousePos.y = clamp(m_MousePos.y, 0.f, (float)Graphics()->ScreenHeight());

	return true;
}

bool CMenus::OnInput(IInput::CEvent e)
{
	m_LastInput = time_get();

	// special handle esc and enter for popup purposes
	if(e.m_Flags & IInput::FLAG_PRESS)
	{
		if(e.m_Key == KEY_ESCAPE)
		{
			m_EscapePressed = true;
			if(m_Popup == POPUP_NONE)
				SetActive(!IsActive());
			return true;
		}
	}

	if(IsActive())
	{
		if(e.m_Flags & IInput::FLAG_PRESS)
		{
			// special for popups
			if(e.m_Key == KEY_RETURN || e.m_Key == KEY_KP_ENTER)
				m_EnterPressed = true;
			else if(e.m_Key == KEY_DELETE)
				m_DeletePressed = true;
		}

		if(m_NumInputEvents < MAX_INPUTEVENTS)
			m_aInputEvents[m_NumInputEvents++] = e;
		return true;
	}
	return false;
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(0);

	if(NewState == IClient::STATE_OFFLINE)
	{
		if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_QUITTING)
			m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
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
		m_Popup = POPUP_CONNECTING;
	else if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_Popup != POPUP_WARNING)
		{
			m_Popup = POPUP_NONE;
			SetActive(false);
		}
	}
}

extern "C" void font_debug_render();

void CMenus::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		SetActive(true);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
		RenderDemoPlayer(Screen);
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == m_pClient->SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		m_Popup = POPUP_PURE;
	}

	if(!IsActive())
	{
		m_EscapePressed = false;
		m_EnterPressed = false;
		m_DeletePressed = false;
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
	float mx = (m_MousePos.x / (float)Graphics()->ScreenWidth()) * pScreen->w;
	float my = (m_MousePos.y / (float)Graphics()->ScreenHeight()) * pScreen->h;

	int Buttons = 0;
	if(m_UseMouseButtons)
	{
		if(Input()->KeyIsPressed(KEY_MOUSE_1))
			Buttons |= 1;
		if(Input()->KeyIsPressed(KEY_MOUSE_2))
			Buttons |= 2;
		if(Input()->KeyIsPressed(KEY_MOUSE_3))
			Buttons |= 4;
	}

	UI()->Update(mx, my, mx * 3.0f, my * 3.0f, Buttons);

	// render
	Render();

	// render cursor
	Graphics()->WrapClamp();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(mx, my, 24, 24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();

	// render debug information
	if(g_Config.m_Debug)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%p %p %p", UI()->HotItem(), UI()->ActiveItem(), UI()->LastActiveItem());
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 10, 10, 10, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
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
			IGraphics::CQuadItem QuadItem((x - OffsetTime) * Size * 2 + (y & 1) * Size, (y + OffsetTime) * Size, Size, Size);
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
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	}
}

bool CMenus::CheckHotKey(int Key) const
{
	return m_Popup == POPUP_NONE &&
	       !Input()->KeyIsPressed(KEY_LSHIFT) && !Input()->KeyIsPressed(KEY_RSHIFT) && !Input()->KeyIsPressed(KEY_LCTRL) && !Input()->KeyIsPressed(KEY_RCTRL) && !Input()->KeyIsPressed(KEY_LALT) && // no modifier
	       Input()->KeyIsPressed(Key) && m_pClient->m_pGameConsole->IsClosed();
}

int CMenus::DoButton_CheckBox_DontCare(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	switch(Checked)
	{
	case 0:
		return DoButton_CheckBox_Common(pID, pText, "", pRect);
	case 1:
		return DoButton_CheckBox_Common(pID, pText, "X", pRect);
	case 2:
		return DoButton_CheckBox_Common(pID, pText, "O", pRect);
	default:
		return DoButton_CheckBox_Common(pID, pText, "", pRect);
	}
}

void CMenus::RenderUpdating(const char *pCaption, int current, int total)
{
	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	static int64 LastLoadRender = 0;
	if(time_get() - LastLoadRender < time_freq() / 60)
		return;
	LastLoadRender = time_get();

	// need up date this here to get correct
	ms_GuiColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true));

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w / 2 - w / 2;
	float y = Screen.h / 2 - h / 2;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.50f);
	RenderTools()->DrawRoundRect(0, y, Screen.w, h, 0.0f);
	Graphics()->QuadsEnd();

	CUIRect r;
	r.x = x;
	r.y = y + 20;
	r.w = w;
	r.h = h;
	UI()->DoLabel(&r, Localize(pCaption), 32.0f, 0, -1);

	if(total > 0)
	{
		float Percent = current / (float)total;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.15f, 0.15f, 0.15f, 0.75f);
		RenderTools()->DrawRoundRect(x + 40, y + h - 75, w - 80, 30, 5.0f);
		Graphics()->SetColor(1, 1, 1, 0.75f);
		RenderTools()->DrawRoundRect(x + 45, y + h - 70, (w - 85) * Percent, 20, 5.0f);
		Graphics()->QuadsEnd();
	}

	Graphics()->Swap();
}

int CMenus::MenuImageScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	char aBuf[MAX_PATH_LENGTH];
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

	unsigned char *d = (unsigned char *)Info.m_pData;
	//int Pitch = Info.m_Width*4;

	// create colorless version
	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	// make the texture gray scale
	for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
	{
		int v = (d[i * Step] + d[i * Step + 1] + d[i * Step + 2]) / 3;
		d[i * Step] = v;
		d[i * Step + 1] = v;
		d[i * Step + 2] = v;
	}

	/* same grey like sinks
	int Freq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequence
	for(int y = 0; y < Info.m_Height; y++)
		for(int x = 0; x < Info.m_Width; x++)
		{
			if(d[y*Pitch+x*4+3] > 128)
				Freq[d[y*Pitch+x*4]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(Freq[OrgWeight] < Freq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255-OrgWeight;
	int InvNewWeight = 255-NewWeight;
	for(int y = 0; y < Info.m_Height; y++)
		for(int x = 0; x < Info.m_Width; x++)
		{
			int v = d[y*Pitch+x*4];
			if(v <= OrgWeight)
				v = (int)(((v/(float)OrgWeight) * NewWeight));
			else
				v = (int)(((v-OrgWeight)/(float)InvOrgWeight)*InvNewWeight + NewWeight);
			d[y*Pitch+x*4] = v;
			d[y*Pitch+x*4+1] = v;
			d[y*Pitch+x*4+2] = v;
		}
	*/

	MenuImage.m_GreyTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	pSelf->Graphics()->FreePNG(&Info);

	// set menu image data
	str_truncate(MenuImage.m_aName, sizeof(MenuImage.m_aName), pName, str_length(pName) - 4);
	pSelf->m_lMenuImages.add(MenuImage);
	pSelf->RenderLoading();

	return 0;
}

const CMenus::CMenuImage *CMenus::FindMenuImage(const char *pName)
{
	for(int i = 0; i < m_lMenuImages.size(); i++)
	{
		if(str_comp(m_lMenuImages[i].m_aName, pName) == 0)
			return &m_lMenuImages[i];
	}
	return 0;
}

void CMenus::SetMenuPage(int NewPage)
{
	m_MenuPage = NewPage;
	if(NewPage >= PAGE_INTERNET && NewPage <= PAGE_KOG)
		g_Config.m_UiPage = NewPage;
}

bool CMenus::HandleListInputs(const CUIRect &View, float &ScrollValue, const float ScrollAmount, int *pScrollOffset, const float ElemHeight, int &SelectedIndex, const int NumElems)
{
	int NewIndex = -1;
	int Num = (int)(View.h / ElemHeight) + 1;
	int ScrollNum = maximum(NumElems - Num + 1, 0);
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
	SelectedIndex = clamp(SelectedIndex, 0, NumElems);

	for(int i = 0; i < m_NumInputEvents; i++)
	{
		if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
		{
			if(m_aInputEvents[i].m_Key == KEY_DOWN)
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
