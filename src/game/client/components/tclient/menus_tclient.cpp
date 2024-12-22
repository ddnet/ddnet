#include <base/log.h>
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
#include <game/client/components/tclient/bindwheel.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "../binds.h"
#include "../countryflags.h"
#include "../menus.h"
#include "../skins.h"

#include <array>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

void CMenus::OpenTClientDiscord()
{
	if(!Client()->ViewLink("https://discord.gg/fBvhH93Bt6"))
		PopupWarning(Localize("Open TClient Discord"), Localize("Failed to open the TClient Discord in your browser"), Localize("Aww"), std::chrono::nanoseconds(0));
}

enum
{
	TCLIENT_TAB_SETTINGS = 0,
	TCLIENT_TAB_BINDWHEEL = 1,
	TCLIENT_TAB_BINDCHAT = 2,
	TCLIENT_TAB_WARLIST = 3,
	TCLIENT_TAB_INFO = 4,
	NUMBER_OF_TCLIENT_TABS = 5
};

typedef struct
{
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
} CKeyInfo;

using namespace FontIcons;

static float s_Time = 0.0f;
static bool s_StartedTime = false;

const float FontSize = 14.0f;
const float EditBoxFontSize = 12.0f;
const float LineSize = 20.0f;
const float ColorPickerLineSize = 25.0f;
const float HeadlineFontSize = 20.0f;
const float StandardFontSize = 14.0f;

const float HeadlineHeight = HeadlineFontSize + 0.0f;
const float Margin = 10.0f;
const float MarginSmall = 5.0f;
const float MarginExtraSmall = 2.5f;
const float MarginBetweenSections = 30.0f;
const float MarginBetweenViews = 30.0f;

const float ColorPickerLabelSize = 13.0f;
const float ColorPickerLineSpacing = 5.0f;

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;

	int Value = *pOption;
	Min /= Scale;
	Max /= Scale;
	// Allow adjustment of slider options when ctrl is pressed (to avoid scrolling, or accidently adjusting the value)
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
	{
		// clamp the value internally for the scrollbar
		Value = clamp(Value, Min, Max);
	}

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, minimum(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
	{
		Value = *pOption;
	}

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}


bool CMenus::DoEditBoxWithLabel(CLineInput *LineInput, const CUIRect *pRect, const char *pLabel, const char *pDefault, char *pBuf, size_t BufSize)
{
	CUIRect Button, Label;
	pRect->VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML);
	LineInput->SetBuffer(pBuf, BufSize);
	LineInput->SetEmptyText(pDefault);
	return Ui()->DoEditBox(LineInput, &Button, EditBoxFontSize);
}

int CMenus::DoButtonLineSize_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, float Line_Size, bool Fake, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color)
{
	CUIRect Text = *pRect;

	if(Checked)
		Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	Color.a *= Ui()->ButtonColorMul(pButtonContainer);

	if(Fake)
		Color.a *= 0.5f;

	pRect->Draw(Color, Corners, Rounding);

	Text.HMargin(Line_Size / 2.0f, &Text);
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	Ui()->DoLabel(&Text, pText, Text.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	if(Fake)
		return 0;

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect);

}

void CMenus::RenderSettingsTClient(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	static int s_CurCustomTab = 0;

	CUIRect TabBar, Column, LeftView, RightView, Button, Label;

	MainView.HSplitTop(LineSize, &TabBar, &MainView);
	const float TabWidth = TabBar.w / NUMBER_OF_TCLIENT_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_TCLIENT_TABS] = {};
	const char *apTabNames[] = {
		Localize("Settings"),
		Localize("Bindwheel"),
		Localize("Chat Binds"),
		Localize("Warlist"),
		Localize("Info")};

	for(int Tab = 0; Tab < NUMBER_OF_TCLIENT_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == NUMBER_OF_TCLIENT_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurCustomTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurCustomTab = Tab;
	}

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	if(s_CurCustomTab == TCLIENT_TAB_SETTINGS)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
		ScrollParams.m_ScrollbarMargin = 5.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

		static std::vector<CUIRect> s_SectionBoxes;
		static vec2 s_PrevScrollOffset(0.0f, 0.0f);

		MainView.y += ScrollOffset.y;

		MainView.VSplitRight(5.0f, &MainView, nullptr); // Padding for scrollbar
		MainView.VSplitLeft(5.0f, nullptr, &MainView); // Padding for scrollbar

		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
		LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
		RightView.VSplitRight(MarginSmall, &RightView, nullptr);

		// RightView.VSplitRight(10.0f, &RightView, nullptr);
		for(CUIRect &Section : s_SectionBoxes)
		{
			float Padding = MarginBetweenViews * 0.6666f;
			Section.w += Padding;
			Section.h += Padding;
			Section.x -= Padding * 0.5f;
			Section.y -= Padding * 0.5f;
			Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
			float Shade = 0.0f;
			Section.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		}
		s_PrevScrollOffset = ScrollOffset;
		s_SectionBoxes.clear();

		// ***** LeftView ***** //
		Column = LeftView;

		// ***** Visual Miscellaneous ***** //
		Column.HSplitTop(Margin, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Visual"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeUpdateFix, Localize("Update tee skin faster after being frozen"), &g_Config.m_ClFreezeUpdateFix, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, Localize("Show ping colored circle before names"), &g_Config.m_ClPingNameCircle, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderNameplateSpec, Localize("Hide nameplates in spec"), &g_Config.m_ClRenderNameplateSpec, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowSkinName, Localize("Show skin names in nameplate"), &g_Config.m_ClShowSkinName, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeStars, Localize("Freeze Stars"), &g_Config.m_ClFreezeStars, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClColorFreeze, Localize("Color Frozen Tee Skins"), &g_Config.m_ClColorFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHammerRotatesWithCursor, Localize("Make hammer rotate with cursor"), &g_Config.m_ClHammerRotatesWithCursor, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWhiteFeet, Localize("Render all custom colored feet as white feet skin"), &g_Config.m_ClWhiteFeet, &Column, LineSize);
		CUIRect FeetBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &FeetBox, &Column);
		if(g_Config.m_ClWhiteFeet)
		{
			FeetBox.HSplitTop(MarginExtraSmall, nullptr, &FeetBox);
			FeetBox.VSplitMid(&FeetBox, nullptr);
			static CLineInput s_WhiteFeet(g_Config.m_ClWhiteFeetSkin, sizeof(g_Config.m_ClWhiteFeetSkin));
			s_WhiteFeet.SetEmptyText("x_ninja");
			Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Input ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Input"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInput, Localize("Fast Inputs (-20ms input delay)"), &g_Config.m_ClFastInput, &Column, LineSize);

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		if(g_Config.m_ClFastInput)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInputOthers, Localize("Extra tick other tees (increases other tees latency, \nmakes dragging slightly easier when using fast input)"), &g_Config.m_ClFastInputOthers, &Column, LineSize);
		else
			Column.HSplitTop(LineSize, nullptr, &Column);
		// A little extra spacing because these are multi line
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOldMouseZoom, Localize("Old Mouse Precision (fixes precision at low zoom levels, \nbreaks /tc, /telecursor while zoomed)"), &g_Config.m_ClOldMouseZoom, &Column, LineSize);
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClImproveMousePrecision, Localize("Improve mouse precision by scaling sent max distance to 1000"), &g_Config.m_ClImproveMousePrecision, &Column, LineSize);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Anti Latency Tools ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Anti Latency Tools"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, Localize("Prediction Margin"), 10, 25, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRemoveAnti, Localize("Remove prediction & antiping in freeze"), &g_Config.m_ClRemoveAnti, &Column, LineSize);
		if(g_Config.m_ClRemoveAnti)
		{
			if(g_Config.m_ClUnfreezeLagDelayTicks < g_Config.m_ClUnfreezeLagTicks)
				g_Config.m_ClUnfreezeLagDelayTicks = g_Config.m_ClUnfreezeLagTicks;
			Column.HSplitTop(LineSize, &Button, &Column);
			DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagTicks, &g_Config.m_ClUnfreezeLagTicks, &Button, Localize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
			Column.HSplitTop(LineSize, &Button, &Column);
			DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagDelayTicks, &g_Config.m_ClUnfreezeLagDelayTicks, &Button, Localize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
		}
		else
			Column.HSplitTop(LineSize * 2, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnpredOthersInFreeze, Localize("Dont predict other players if you are frozen"), &g_Config.m_ClUnpredOthersInFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPredMarginInFreeze, Localize("Adjust your prediction margin while frozen"), &g_Config.m_ClPredMarginInFreeze, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		if(g_Config.m_ClPredMarginInFreeze)
			Ui()->DoScrollbarOption(&g_Config.m_ClPredMarginInFreezeAmount, &g_Config.m_ClPredMarginInFreezeAmount, &Button, Localize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Improved Anti Ping ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("TClient Anti Ping"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingImproved, Localize("Use new Antiping algorithm"), &g_Config.m_ClAntiPingImproved, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingStableDirection, Localize("Optimistic prediction along stable direction"), &g_Config.m_ClAntiPingStableDirection, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingNevativeBuffer, Localize("Negative stability buffer (for Gores)"), &g_Config.m_ClAntiPingNevativeBuffer, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClAntiPingUncertaintyScale, &g_Config.m_ClAntiPingUncertaintyScale, &Button, Localize("Uncertainty Duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Other ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Other"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRunOnJoinConsole, Localize("Run cl_run_on_join as console command"), &g_Config.m_ClRunOnJoinConsole, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		if(g_Config.m_ClRunOnJoinConsole)
		{
			DoSliderWithScaledValue(&g_Config.m_ClRunOnJoinDelay, &g_Config.m_ClRunOnJoinDelay, &Button, Localize("Delay"), 140, 2000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
		}
		CUIRect ButtonVerify, EnableVerifySection;
		Column.HSplitTop(LineSize, &EnableVerifySection, &Column);
		EnableVerifySection.VSplitMid(&EnableVerifySection, &ButtonVerify);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoVerify, Localize("Auto Verify"), &g_Config.m_ClAutoVerify, &EnableVerifySection, LineSize);
		static CButtonContainer s_VerifyButton;
		if(DoButton_Menu(&s_VerifyButton, Localize("Manual Verify"), 0, &ButtonVerify, 0, IGraphics::CORNER_ALL))
		{
			if(!Client()->ViewLink("https://ger10.ddnet.org/"))
				dbg_msg("menus", "couldn't open link");
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Player Indicator ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Player Indicator"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, Localize("Show any enabled Indicators"), &g_Config.m_ClPlayerIndicator, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicatorFreeze, Localize("Show only freeze Players"), &g_Config.m_ClPlayerIndicatorFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTeamOnly, Localize("Only show after joining a team"), &g_Config.m_ClIndicatorTeamOnly, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTees, Localize("Render tiny tees instead of circles"), &g_Config.m_ClIndicatorTees, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorRadius, &g_Config.m_ClIndicatorRadius, &Button, Localize("Indicator size"), 1, 16);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOpacity, &g_Config.m_ClIndicatorOpacity, &Button, Localize("Indicator opacity"), 0, 100);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorVariableDistance, Localize("Change indicator offset based on distance to other tees"), &g_Config.m_ClIndicatorVariableDistance, &Column, LineSize);
		if(g_Config.m_ClIndicatorVariableDistance)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator min offset"), 16, 200);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffsetMax, &g_Config.m_ClIndicatorOffsetMax, &Button, Localize("Indicator max offset"), 16, 200);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorMaxDistance, &g_Config.m_ClIndicatorMaxDistance, &Button, Localize("Indicator max distance"), 500, 7000);
		}
		else
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator offset"), 16, 200);
			Column.HSplitTop(LineSize * 2, nullptr, &Column);
		}
		static CButtonContainer IndicatorAliveColorID, IndicatorDeadColorID, IndicatorSavedColorID;
		DoLine_ColorPicker(&IndicatorAliveColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Indicator alive color"), &g_Config.m_ClIndicatorAlive, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&IndicatorDeadColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Indicator dead color"), &g_Config.m_ClIndicatorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&IndicatorSavedColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Indicator save color"), &g_Config.m_ClIndicatorSaved, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** RightView ***** //
		LeftView = Column;
		Column = RightView;

		// ***** HUD ***** //
		Column.HSplitTop(Margin, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("HUD"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowCenterLines, Localize("Show screen center"), &g_Config.m_ClShowCenterLines, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMiniDebug, Localize("Show Position and angle (Mini debug)"), &g_Config.m_ClMiniDebug, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderCursorSpec, Localize("Show your cursor when in free spectate"), &g_Config.m_ClRenderCursorSpec, &Column, LineSize);

		Column.HSplitTop(LineSize, &Button, &Column);
		if(g_Config.m_ClRenderCursorSpec)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClRenderCursorSpecAlpha, &g_Config.m_ClRenderCursorSpecAlpha, &Button, Localize("Spectate cursor alpha"), 0, 100);
		}

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNotifyWhenLast, Localize("Show when you are the last alive"), &g_Config.m_ClNotifyWhenLast, &Column, LineSize);
		CUIRect NotificationConfig;
		Column.HSplitTop(LineSize + MarginSmall, &NotificationConfig, &Column);
		if(g_Config.m_ClNotifyWhenLast)
		{
			NotificationConfig.VSplitMid(&Button, &NotificationConfig);
			static CLineInput s_LastInput(g_Config.m_ClNotifyWhenLastText, sizeof(g_Config.m_ClNotifyWhenLastText));
			s_LastInput.SetEmptyText(Localize("Last!"));
			Button.HSplitTop(MarginSmall, nullptr, &Button);
			Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize);
			static CButtonContainer s_ClientNotifyWhenLastColor;
			DoLine_ColorPicker(&s_ClientNotifyWhenLastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &NotificationConfig, "", &g_Config.m_ClNotifyWhenLastColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Frozen Tee Display ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Frozen Tee Display"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHud, Localize("Show frozen tee display"), &g_Config.m_ClShowFrozenHud, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHudSkins, Localize("Use skins instead of ninja tees"), &g_Config.m_ClShowFrozenHudSkins, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFrozenHudTeamOnly, Localize("Only show after joining a team"), &g_Config.m_ClFrozenHudTeamOnly, &Column, LineSize);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClFrozenMaxRows, &g_Config.m_ClFrozenMaxRows, &Button, Localize("Max Rows"), 1, 6);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClFrozenHudTeeSize, &g_Config.m_ClFrozenHudTeeSize, &Button, Localize("Tee Size"), 8, 27);

		{
			CUIRect CheckBoxRect, CheckBoxRect2;
			Column.HSplitTop(LineSize, &CheckBoxRect, &Column);
			Column.HSplitTop(LineSize, &CheckBoxRect2, &Column);
			if(DoButton_CheckBox(&g_Config.m_ClShowFrozenText, Localize("Tees Left Alive Text"), g_Config.m_ClShowFrozenText >= 1, &CheckBoxRect))
				g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText >= 1 ? 0 : 1;

			if(g_Config.m_ClShowFrozenText)
			{
				static int s_CountFrozenText = 0;
				if(DoButton_CheckBox(&s_CountFrozenText, Localize("Count Frozen Tees"), g_Config.m_ClShowFrozenText == 2, &CheckBoxRect2))
					g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText != 2 ? 2 : 1;
			}
		}
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Tile Outlines ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Tile Outlines"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutline, Localize("Show any enabled outlines"), &g_Config.m_ClOutline, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineEntities, Localize("Only show outlines in entities"), &g_Config.m_ClOutlineEntities, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineFreeze, Localize("Outline freeze & deep"), &g_Config.m_ClOutlineFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineSolid, Localize("Outline walls"), &g_Config.m_ClOutlineSolid, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineTele, Localize("Outline teleporter"), &g_Config.m_ClOutlineTele, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineUnFreeze, Localize("Outline unfreeze & undeep"), &g_Config.m_ClOutlineUnFreeze, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidth, &g_Config.m_ClOutlineWidth, &Button, Localize("Outline Width"), 1, 16);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClOutlineAlpha, &g_Config.m_ClOutlineAlpha, &Button, Localize("Outline Alpha"), 0, 100);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClOutlineAlphaSolid, &g_Config.m_ClOutlineAlphaSolid, &Button, Localize("Outline Alpha (walls)"), 0, 100);
		static CButtonContainer OutlineColorFreezeID, OutlineColorSolidID, OutlineColorTeleID, OutlineColorUnfreezeID;
		DoLine_ColorPicker(&OutlineColorFreezeID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Freeze Outline Color"), &g_Config.m_ClOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&OutlineColorSolidID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Walls Outline Color"), &g_Config.m_ClOutlineColorSolid, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&OutlineColorTeleID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Teleporter Outline Color"), &g_Config.m_ClOutlineColorTele, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&OutlineColorUnfreezeID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, Localize("Unfreeze Outline Color"), &g_Config.m_ClOutlineColorUnfreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Ghost Tools ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Ghost Tools"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersGhosts, Localize("Show unpredicted ghosts for other players"), &g_Config.m_ClShowOthersGhosts, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSwapGhosts, Localize("Swap ghosts and normal players"), &g_Config.m_ClSwapGhosts, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClPredGhostsAlpha, &g_Config.m_ClPredGhostsAlpha, &Button, Localize("Predicted Alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClUnpredGhostsAlpha, &g_Config.m_ClUnpredGhostsAlpha, &Button, Localize("Unpredicted Alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideFrozenGhosts, Localize("Hide ghosts of frozen players"), &g_Config.m_ClHideFrozenGhosts, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderGhostAsCircle, Localize("Render ghosts as circles"), &g_Config.m_ClRenderGhostAsCircle, &Column, LineSize);

		static CKeyInfo Key = CKeyInfo{"Toggle Ghosts Key", "toggle tc_show_others_ghosts 0 1", 0, 0};
		Key.m_ModifierCombination = Key.m_KeyId = 0;
		for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
		{
			for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
			{
				const char *pBind = m_pClient->m_Binds.Get(KeyId, Mod);
				if(!pBind[0])
					continue;

				if(str_comp(pBind, Key.m_pCommand) == 0)
				{
					Key.m_KeyId = KeyId;
					Key.m_ModifierCombination = Mod;
					break;
				}
			}
		}

		CUIRect KeyButton, KeyLabel;
		Column.HSplitTop(LineSize, &KeyButton, &Column);
		KeyButton.VSplitMid(&KeyLabel, &KeyButton);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize(Key.m_pName));
		Ui()->DoLabel(&KeyLabel, aBuf, 12.0f, TEXTALIGN_ML);
		int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
		int NewId = DoKeyReader(&Key, &KeyButton, OldId, OldModifierCombination, &NewModifierCombination);
		if(NewId != OldId || NewModifierCombination != OldModifierCombination)
		{
			if(OldId != 0 || NewId == 0)
				m_pClient->m_Binds.Bind(OldId, "", false, OldModifierCombination);
			if(NewId != 0)
				m_pClient->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		// ***** Rainbow ***** //
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Rainbow"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		static std::vector<const char *> s_DropDownNames = {Localize("Rainbow"), Localize("Pulse"), Localize("Black"), Localize("Random")};
		static CUi::SDropDownState s_RainbowDropDownState;
		static CScrollRegion s_RainbowDropDownScrollRegion;
		s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
		int RainbowSelectedOld = g_Config.m_ClRainbowMode - 1;
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbow, Localize("Rainbow"), &g_Config.m_ClRainbow, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowOthers, Localize("Rainbow Others"), &g_Config.m_ClRainbowOthers, &Column, LineSize);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		const int RainbowSelectedNew = Ui()->DoDropDown(&DropDownRect, RainbowSelectedOld, s_DropDownNames.data(), s_DropDownNames.size(), s_RainbowDropDownState);
		if(RainbowSelectedOld != RainbowSelectedNew)
		{
			g_Config.m_ClRainbowMode = RainbowSelectedNew + 1;
			RainbowSelectedOld = RainbowSelectedNew;
			dbg_msg("rainbow", "rainbow mode changed to %d", g_Config.m_ClRainbowMode);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Button, Localize("Rainbow speed"), 0, 5000, &CUi::ms_LogarithmicScrollbarScale, 0, "%");
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		// ***** END OF PAGE 1 SETTINGS ***** //
		RightView = Column;

		// Scroll
		CUIRect ScrollRegion;
		ScrollRegion.x = MainView.x;
		ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
		ScrollRegion.w = MainView.w;
		ScrollRegion.h = 0.0f;
		s_ScrollRegion.AddRect(ScrollRegion);
		s_ScrollRegion.End();
	}

	if(s_CurCustomTab == TCLIENT_TAB_BINDCHAT)
	{
		MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
		Column = MainView;

		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, Localize("Kaomoji"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		auto DoBindchat = [&](CLineInput &LineInput, const char *pLabel, const char *pName, const char *pCommand)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			char *BindCommand;
			int BindIndex = GameClient()->m_Bindchat.GetBind(pCommand);
			bool BindNew = BindIndex == -1;
			if(BindNew)
			{
				static char s_aBindCommandTemp[BINDCHAT_MAX_CMD] = "";
				BindCommand = s_aBindCommandTemp;
				BindNew = true; // Make a new bind, as we arent editing one
			}
			else
			{
				auto *Bind = GameClient()->m_Bindchat.Get(BindIndex);
				BindCommand = Bind->m_aName;
			}
			if(DoEditBoxWithLabel(&LineInput, &Button, pLabel, pName, BindCommand, BINDCHAT_MAX_CMD))
			{
				if(BindNew && BindCommand[0] != '\0' && LineInput.IsActive())
				{
					GameClient()->m_Bindchat.AddBind(BindCommand, pCommand);
					BindCommand[0] = '\0'; // Reset for new usage
				}
				if(!BindNew && BindCommand[0] == '\0')
					GameClient()->m_Bindchat.RemoveBind(BindIndex);
			}
		};

		static CLineInput s_KaomojiShrug;
		DoBindchat(s_KaomojiShrug, Localize("Shrug:"), "!shrug", "say ¯\\_(ツ)_/¯");

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CLineInput s_KaomojiFlip;
		DoBindchat(s_KaomojiFlip, Localize("Flip:"), "!flip", "say (╯°□°)╯︵ ┻━┻");

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CLineInput s_KaomojiUnflip;
		DoBindchat(s_KaomojiUnflip, Localize("Unflip:"), "!unflip", "say ┬─┬ノ( º _ ºノ)");

	}

	if(s_CurCustomTab == TCLIENT_TAB_BINDWHEEL)
	{
		MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
		MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

		const float Radius = minimum(RightView.w, RightView.h) / 2.0f;
		vec2 Pos{RightView.x + RightView.w / 2.0f, RightView.y + RightView.h / 2.0f};
		// Draw Circle
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
		Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 64);
		Graphics()->QuadsEnd();

		static char s_aBindName[BINDWHEEL_MAX_NAME];
		static char s_aBindCommand[BINDWHEEL_MAX_CMD];

		static int s_SelectedBindIndex = -1;
		int HoveringIndex = -1;

		float MouseDist = distance(Pos, Ui()->MousePos());
		if(MouseDist < Radius && MouseDist > Radius * 0.25f)
		{
			int SegmentCount = GameClient()->m_Bindwheel.m_vBinds.size();
			float SegmentAngle = 2 * pi / SegmentCount;

			float HoveringAngle = angle(Ui()->MousePos() - Pos) + SegmentAngle / 2;
			if(HoveringAngle < 0.0f)
				HoveringAngle += 2.0f * pi;

			HoveringIndex = (int)(HoveringAngle / (2 * pi) * SegmentCount);
			if(Ui()->MouseButtonClicked(0))
			{
				s_SelectedBindIndex = HoveringIndex;
				str_copy(s_aBindName, GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aName);
				str_copy(s_aBindCommand, GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aCommand);
			}
			else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
			{
				CBindwheel::CBind BindA = GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex];
				CBindwheel::CBind BindB = GameClient()->m_Bindwheel.m_vBinds[HoveringIndex];
				str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
				str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
				str_copy(GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
				str_copy(GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
			}
			else if(Ui()->MouseButtonClicked(2))
			{
				s_SelectedBindIndex = HoveringIndex;
			}
		}
		else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = -1;
			str_copy(s_aBindName, "");
			str_copy(s_aBindCommand, "");
		}

		const float Theta = pi * 2.0f / GameClient()->m_Bindwheel.m_vBinds.size();
		for(int i = 0; i < static_cast<int>(GameClient()->m_Bindwheel.m_vBinds.size()); i++)
		{
			float SegmentFontSize = FontSize * 1.2f;
			if(i == s_SelectedBindIndex)
			{
				SegmentFontSize = FontSize * 1.7f;
				TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
			}
			else if(i == HoveringIndex)
				SegmentFontSize = FontSize;

			const CBindwheel::CBind Bind = GameClient()->m_Bindwheel.m_vBinds[i];
			const float Angle = Theta * i;
			vec2 TextPos = direction(Angle);
			TextPos *= Radius * 0.75f;

			float Width = TextRender()->TextWidth(SegmentFontSize, Bind.m_aName);
			TextPos += Pos;
			TextPos.x -= Width / 2.0f;
			TextRender()->Text(TextPos.x, TextPos.y, SegmentFontSize, Bind.m_aName);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Ui()->DoLabel(&Label, Localize("Name:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_NameInput;
		s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
		s_NameInput.SetEmptyText("Name");
		Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Ui()->DoLabel(&Label, Localize("Command:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_BindInput;
		s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
		s_BindInput.SetEmptyText(Localize("Command"));
		Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

		static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_Menu(&s_OverrideButton, Localize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0)
		{
			CBindwheel::CBind TempBind;
			if(str_length(s_aBindName) == 0)
				str_copy(TempBind.m_aName, "*");
			else
				str_copy(TempBind.m_aName, s_aBindName);

			str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
			str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
		}
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		CUIRect ButtonAdd, ButtonRemove;
		Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
		if(DoButton_Menu(&s_AddButton, Localize("Add Bind"), 0, &ButtonAdd))
		{
			CBindwheel::CBind TempBind;
			if(str_length(s_aBindName) == 0)
				str_copy(TempBind.m_aName, "*");
			else
				str_copy(TempBind.m_aName, s_aBindName);

			GameClient()->m_Bindwheel.AddBind(TempBind.m_aName, s_aBindCommand);
			s_SelectedBindIndex = static_cast<int>(GameClient()->m_Bindwheel.m_vBinds.size()) - 1;
		}
		if(DoButton_Menu(&s_RemoveButton, Localize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
		{
			GameClient()->m_Bindwheel.RemoveBind(s_SelectedBindIndex);
			s_SelectedBindIndex = -1;
		}

		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Use left mouse to select"), FontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(LineSize, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Use right mouse to swap with selected"), FontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(LineSize, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Use middle mouse select without copy"), FontSize, TEXTALIGN_ML);

		// Do Settings Key
		CKeyInfo Key = CKeyInfo{"Bind Wheel Key", "+bindwheel", 0, 0};
		for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
		{
			for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
			{
				const char *pBind = m_pClient->m_Binds.Get(KeyId, Mod);
				if(!pBind[0])
					continue;

				if(str_comp(pBind, Key.m_pCommand) == 0)
				{
					Key.m_KeyId = KeyId;
					Key.m_ModifierCombination = Mod;
					break;
				}
			}
		}

		CUIRect KeyLabel;
		LeftView.HSplitBottom(LineSize, &LeftView, &Button);
		Button.VSplitLeft(120.0f, &KeyLabel, &Button);
		Button.VSplitLeft(100.0f, &Button, nullptr);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize((const char *)Key.m_pName));

		Ui()->DoLabel(&KeyLabel, aBuf, FontSize, TEXTALIGN_ML);
		int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
		int NewId = DoKeyReader((void *)&Key.m_pName, &Button, OldId, OldModifierCombination, &NewModifierCombination);
		if(NewId != OldId || NewModifierCombination != OldModifierCombination)
		{
			if(OldId != 0 || NewId == 0)
				m_pClient->m_Binds.Bind(OldId, "", false, OldModifierCombination);
			if(NewId != 0)
				m_pClient->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
		}
		LeftView.HSplitBottom(LineSize, &LeftView, &Button);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetBindWheelMouse, Localize("Reset position of mouse when opening bindwheel"), &g_Config.m_ClResetBindWheelMouse, &Button, LineSize);
	}

	if(s_CurCustomTab == TCLIENT_TAB_WARLIST)
	{
		RenderSettingsWarList(MainView);
	}

	if(s_CurCustomTab == TCLIENT_TAB_INFO)
	{
		RenderSettingsInfo(MainView);
	}
}

void CMenus::RenderSettingsWarList(CUIRect MainView)
{
	CUIRect RightView, LeftView, Column1, Column2, Column3, Column4, Button, ButtonL, ButtonR, Label;

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, Margin);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	// WAR LIST will have 4 columns
	//  [War entries] - [Entry Editing] - [Group Types] - [Recent Players]
	//									 [Group Editing]

	// putting this here so it can be updated by the entry list
	static char s_aEntryName[MAX_NAME_LENGTH];
	static char s_aEntryClan[MAX_CLAN_LENGTH];
	static char s_aEntryReason[MAX_WARLIST_REASON_LENGTH];
	static int s_IsClan = 0;
	static int s_IsName = 1;

	LeftView.VSplitMid(&Column1, &Column2, Margin);
	RightView.VSplitMid(&Column3, &Column4, Margin);

	// ======WAR ENTRIES======
	Column1.HSplitTop(HeadlineHeight, &Label, &Column1);
	Label.VSplitRight(25.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("War Entries"), HeadlineFontSize, TEXTALIGN_ML);
	Column1.HSplitTop(MarginSmall, nullptr, &Column1);

	static CButtonContainer s_ReverseEntries;
	static bool s_Reversed = true;
	if(DoButton_FontIcon(&s_ReverseEntries, FONT_ICON_CHEVRON_DOWN, 0, &Button, IGraphics::CORNER_ALL))
	{
		s_Reversed = !s_Reversed;
	}

	CUIRect EntriesSearch;
	Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
	EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);

	static CWarEntry *pSelectedEntry = nullptr;
	static CWarType *pSelectedType = GameClient()->m_WarList.m_WarTypes[0];


	// Filter the list
	static CLineInputBuffered<128> s_EntriesFilterInput;
	std::vector<CWarEntry *> vpFilteredEntries;
	for(size_t i = 0; i < GameClient()->m_WarList.m_WarEntries.size(); ++i)
	{
		CWarEntry *pEntry = &GameClient()->m_WarList.m_WarEntries[i];
		bool Matches = false;
		if(str_find_nocase(pEntry->m_aName, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(str_find_nocase(pEntry->m_aClan, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(str_find_nocase(pEntry->m_pWarType->m_aWarName, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(Matches)
			vpFilteredEntries.push_back(pEntry);
	}

	if(s_Reversed)
	{
		std::reverse(vpFilteredEntries.begin(), vpFilteredEntries.end());
	}
	int SelectedOldEntry = -1;
	static CListBox s_EntriesListBox;
	s_EntriesListBox.DoStart(35.0f, vpFilteredEntries.size(), 1, 2, SelectedOldEntry, &Column1);

	static std::vector<unsigned char> s_vItemIds;
	static std::vector<CButtonContainer> s_vDeleteButtons;

	const int MaxEntries = GameClient()->m_WarList.m_WarEntries.size();
	s_vItemIds.resize(MaxEntries);
	s_vDeleteButtons.resize(MaxEntries);

	for(size_t i = 0; i < vpFilteredEntries.size(); i++)
	{
		CWarEntry *pEntry = vpFilteredEntries[i];

		// idk why it wants this, it was complaining
		if(!pEntry)
			continue;

		if(pSelectedEntry && pEntry == pSelectedEntry)
			SelectedOldEntry = i;

		const CListboxItem Item = s_EntriesListBox.DoNextItem(&s_vItemIds[i], SelectedOldEntry >= 0 && (size_t)SelectedOldEntry == i);
		if(!Item.m_Visible)
			continue;

		CUIRect EntryRect, DeleteButton, EntryTypeRect, WarType, ToolTip;
		Item.m_Rect.Margin(0.0f, &EntryRect);
		EntryRect.VSplitLeft(26.0f, &DeleteButton, &EntryRect);
		DeleteButton.HMargin(7.5f, &DeleteButton);
		DeleteButton.VSplitLeft(MarginSmall, nullptr, &DeleteButton);
		DeleteButton.VSplitRight(MarginExtraSmall, &DeleteButton, nullptr);

		if(DoButton_FontIcon(&s_vDeleteButtons[i], FONT_ICON_TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
			GameClient()->m_WarList.RemoveWarEntry(pEntry);

		bool IsClan = false;
		char aBuf[32];
		if(str_comp(pEntry->m_aClan, "") != 0)
		{
			str_copy(aBuf, pEntry->m_aClan);
			IsClan = true;
		}
		else
		{
			str_copy(aBuf, pEntry->m_aName);
		}
		EntryRect.VSplitLeft(35.0f, &EntryTypeRect, &EntryRect);

		if(IsClan)

		{
			RenderFontIcon(EntryTypeRect, FONT_ICON_USERS, 18.0f, TEXTALIGN_MC);
		}
		else
		{
			// TODO: stop misusing this function
			// TODO: render the real skin with skin remembering component (to be added)
			RenderDevSkin(EntryTypeRect.Center(), 35.0f, "defualt", "default", false, 0, 0, 0, false);
		}

		if(str_comp(pEntry->m_aReason, "") != 0)
		{
			EntryRect.VSplitRight(20.0f, &EntryRect, &ToolTip);
			RenderFontIcon(ToolTip, FONT_ICON_COMMENT, 18.0f, TEXTALIGN_MC);
			GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &ToolTip, pEntry->m_aReason);
			GameClient()->m_Tooltips.SetFadeTime(&s_vItemIds[i], 0.0f);
		}

		EntryRect.HMargin(MarginExtraSmall, &EntryRect);
		EntryRect.HSplitMid(&EntryRect, &WarType, MarginSmall);

		Ui()->DoLabel(&EntryRect, aBuf, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(pEntry->m_pWarType->m_Color);
		Ui()->DoLabel(&WarType, pEntry->m_pWarType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedEntry = s_EntriesListBox.DoEnd();
	if(SelectedOldEntry != NewSelectedEntry || (SelectedOldEntry >= 0 && Ui()->HotItem() == &s_vItemIds[NewSelectedEntry] && Ui()->MouseButtonClicked(0)))
	{
		pSelectedEntry = vpFilteredEntries[NewSelectedEntry];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aEntryName, pSelectedEntry->m_aName);
			str_copy(s_aEntryClan, pSelectedEntry->m_aClan);
			str_copy(s_aEntryReason, pSelectedEntry->m_aReason);
			if(str_comp(pSelectedEntry->m_aClan, "") != 0)
			{
				s_IsName = 0;
				s_IsClan = 1;
			}
			else
			{
				s_IsName = 1;
				s_IsClan = 0;
			}
			pSelectedType = pSelectedEntry->m_pWarType;
		}
	}

	Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && m_pClient->m_GameConsole.IsClosed());

	// ======WAR ENTRY EDITING======

	Column2.HSplitTop(HeadlineHeight, nullptr, &Column2);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);

	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aEntryName, sizeof(s_aEntryName));
	s_NameInput.SetEmptyText("Name");
	if(s_IsName)
		Ui()->DoEditBox(&s_NameInput, &ButtonL, 12.0f);
	else
	{
		ButtonL.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonL);
		ButtonL.VMargin(2.0f, &ButtonL);
		s_NameInput.Render(&ButtonL, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	static CLineInput s_ClanInput;
	s_ClanInput.SetBuffer(s_aEntryClan, sizeof(s_aEntryClan));
	s_ClanInput.SetEmptyText("Clan");
	if(s_IsClan)
		Ui()->DoEditBox(&s_ClanInput, &ButtonR, 12.0f);
	else
	{
		ButtonR.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonR);
		ButtonR.VMargin(2.0f, &ButtonR);
		s_ClanInput.Render(&ButtonR, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static unsigned char s_NameRadio, s_ClanRadio;
	if(DoButton_CheckBox_Common(&s_NameRadio, "Name", s_IsName ? "X" : "", &ButtonL))
	{
		s_IsName = 1;
		s_IsClan = 0;
	}
	if(DoButton_CheckBox_Common(&s_ClanRadio, "Clan", s_IsClan ? "X" : "", &ButtonR))
	{
		s_IsName = 0;
		s_IsClan = 1;
	}
	if(!s_IsName)
		str_copy(s_aEntryName, "");
	if(!s_IsClan)
		str_copy(s_aEntryClan, "");

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);
	static CLineInput s_ReasonInput;
	s_ReasonInput.SetBuffer(s_aEntryReason, sizeof(s_aEntryReason));
	s_ReasonInput.SetEmptyText("Reason");
	Ui()->DoEditBox(&s_ReasonInput, &Button, 12.0f);

	static CButtonContainer s_AddButton, s_OverrideButton;

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize * 2.0f, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);

	if(DoButtonLineSize_Menu(&s_OverrideButton, Localize("Override Entry"), 0, &ButtonL, LineSize) && pSelectedEntry)
	{
		if(pSelectedEntry && pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(pSelectedEntry->m_aName, s_aEntryName);
			str_copy(pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(pSelectedEntry->m_aReason, s_aEntryReason);
			pSelectedEntry->m_pWarType = pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, Localize("Add Entry"), 0, &ButtonR, LineSize))
	{
		if(pSelectedType)
			GameClient()->m_WarList.AddWarEntry(s_aEntryName, s_aEntryClan, s_aEntryReason, pSelectedType->m_aWarName);
	}
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column2);
	if(pSelectedType)
	{
		float Shade = 0.0f;
		Button.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), 15, 3.0f);
		TextRender()->TextColor(pSelectedType->m_Color);
		Ui()->DoLabel(&Button, pSelectedType->m_aWarName, HeadlineFontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// ======WAR TYPE EDITING======

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, Localize("War Groups"), HeadlineFontSize, TEXTALIGN_ML);
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);

	static char s_aTypeName[MAX_WARLIST_TYPE_LENGTH];
	static ColorRGBA s_GroupColor = ColorRGBA(1, 1, 1, 1);

	CUIRect WarTypeList;
	Column3.HSplitBottom(180.0f, &WarTypeList, &Column3);
	m_pRemoveWarType = nullptr;
	int SelectedOldType = -1;
	static CListBox s_WarTypeListBox;
	s_WarTypeListBox.DoStart(25.0f, GameClient()->m_WarList.m_WarTypes.size(), 1, 2, SelectedOldType, &WarTypeList);

	static std::vector<unsigned char> s_vTypeItemIds;
	static std::vector<CButtonContainer> s_vTypeDeleteButtons;

	const int MaxTypes = GameClient()->m_WarList.m_WarTypes.size();
	s_vTypeItemIds.resize(MaxTypes);
	s_vTypeDeleteButtons.resize(MaxTypes);

	for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
	{
		CWarType *pType = GameClient()->m_WarList.m_WarTypes[i];

		if(!pType)
			continue;

		if(pSelectedType && pType == pSelectedType)
			SelectedOldType = i;

		const CListboxItem Item = s_WarTypeListBox.DoNextItem(&s_vTypeItemIds[i], SelectedOldType >= 0 && SelectedOldType == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TypeRect, DeleteButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);

		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vTypeDeleteButtons[i], FONT_ICON_TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				m_pRemoveWarType = pType;
		}
		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedType = s_WarTypeListBox.DoEnd();
	if((SelectedOldType != NewSelectedType && NewSelectedType >= 0) || (NewSelectedType >= 0 && Ui()->HotItem() == &s_vTypeItemIds[NewSelectedType] && Ui()->MouseButtonClicked(0)))
	{
		pSelectedType = GameClient()->m_WarList.m_WarTypes[NewSelectedType];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aTypeName, pSelectedType->m_aWarName);
			s_GroupColor = pSelectedType->m_Color;
		}
	}
	if(m_pRemoveWarType != nullptr)
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage),
			Localize("Are you sure that you want to remove '%s' from your war groups?"),
			m_pRemoveWarType->m_aWarName);
		PopupConfirm(Localize("Remove War Group"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmRemoveWarType);
	}

	static CLineInput s_TypeNameInput;
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	Column3.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column3);
	s_TypeNameInput.SetBuffer(s_aTypeName, sizeof(s_aTypeName));
	s_TypeNameInput.SetEmptyText("Group Name");
	Ui()->DoEditBox(&s_TypeNameInput, &Button, 12.0f);
	static CButtonContainer s_AddGroupButton, s_OverrideGroupButton, s_GroupColorPicker;

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int ColorValue = 0;
	ColorValue = color_cast<ColorHSLA>(s_GroupColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, Localize("Color"), &ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), false);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, Localize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && pSelectedType)
	{
		if(pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(pSelectedType->m_aWarName, s_aTypeName);
			pSelectedType->m_Color = s_GroupColor;
		}
	}
	bool AddDisabled = str_comp(GameClient()->m_WarList.FindWarType(s_aTypeName)->m_aWarName, "none") != 0 || str_comp(s_aTypeName, "none") == 0;
	if(DoButtonLineSize_Menu(&s_AddGroupButton, Localize("Add Group"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_WarList.AddWarType(s_aTypeName, s_GroupColor);
	}

	// ======ONLINE PLAYER LIST======

	Column4.HSplitTop(HeadlineHeight, &Label, &Column4);
	Ui()->DoLabel(&Label, Localize("Online Players"), HeadlineFontSize, TEXTALIGN_ML);
	Column4.HSplitTop(MarginSmall, nullptr, &Column4);

	CUIRect PlayerList;
	Column4.HSplitBottom(0.0f, &PlayerList, &Column4);
	static CListBox s_PlayerListBox;
	s_PlayerListBox.DoStart(30.0f, MAX_CLIENTS, 1, 2, -1, &PlayerList);

	static std::vector<unsigned char> s_vPlayerItemIds;
	static std::vector<CButtonContainer> s_vNameButtons;
	static std::vector<CButtonContainer> s_vClanButtons;

	s_vPlayerItemIds.resize(MAX_CLIENTS);
	s_vNameButtons.resize(MAX_CLIENTS);
	s_vClanButtons.resize(MAX_CLIENTS);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apPlayerInfos[i])
			continue;

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[i].m_RenderInfo;

		const CListboxItem Item = s_PlayerListBox.DoNextItem(&s_vPlayerItemIds[i], false);
		if(!Item.m_Visible)
			continue;

		CUIRect PlayerRect, TeeRect, NameRect, ClanRect;
		Item.m_Rect.Margin(0.0f, &PlayerRect);
		PlayerRect.VSplitLeft(25.0f, &TeeRect, &PlayerRect);
		// RenderDevSkin(TeeRect.Center(), 35.0f, TeeInfo., "default", false, 0, 0, 0, false);

		PlayerRect.VSplitMid(&NameRect, &ClanRect, 0);
		PlayerRect = NameRect;
		PlayerRect.x = TeeRect.x;
		PlayerRect.w += TeeRect.w;
		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_NameColor);
		ColorRGBA NameButtonColor = Ui()->CheckActiveItem(&s_vNameButtons[i]) ? ColorRGBA(1, 1, 1, 0.75f) :
											(Ui()->HotItem() == &s_vNameButtons[i] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		PlayerRect.Draw(NameButtonColor, IGraphics::CORNER_L, 5.0f);
		Ui()->DoLabel(&NameRect, GameClient()->m_aClients[i].m_aName, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vNameButtons[i], false, &PlayerRect))
		{
			s_IsName = 1;
			s_IsClan = 0;
			str_copy(s_aEntryName, GameClient()->m_aClients[i].m_aName);
		}

		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_ClanColor);
		ColorRGBA ClanButtonColor = Ui()->CheckActiveItem(&s_vClanButtons[i]) ? ColorRGBA(1, 1, 1, 0.75f) :
											(Ui()->HotItem() == &s_vClanButtons[i] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		ClanRect.Draw(ClanButtonColor, IGraphics::CORNER_R, 5.0f);
		Ui()->DoLabel(&ClanRect, GameClient()->m_aClients[i].m_aClan, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vClanButtons[i], false, &ClanRect))
		{
			s_IsName = 0;
			s_IsClan = 1;
			str_copy(s_aEntryClan, GameClient()->m_aClients[i].m_aClan);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		TeeInfo.m_Size = 25.0f;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.5f));
	}
	s_PlayerListBox.DoEnd();
}

void CMenus::PopupConfirmRemoveWarType()
{
	GameClient()->m_WarList.RemoveWarType(m_pRemoveWarType->m_aWarName);
	m_pRemoveWarType = nullptr;
}

void CMenus::RenderSettingsInfo(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("TClient Links"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	static CButtonContainer s_DiscordButton, s_WebsiteButton, s_GithubButton, s_SupportButton;
	CUIRect ButtonLeft, ButtonRight;

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);
	if(DoButton_Menu(&s_DiscordButton, Localize("Discord"), 0, &ButtonLeft, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/fBvhH93Bt6");
	if(DoButton_Menu(&s_WebsiteButton, Localize("Wesbite"), 0, &ButtonRight, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://tclient.app/");

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);

	if(DoButton_Menu(&s_GithubButton, Localize("Github"), 0, &ButtonLeft, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://github.com/sjrc6/TaterClient-ddnet");
	if(DoButton_Menu(&s_SupportButton, Localize("Support"), 0, &ButtonRight, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://ko-fi.com/Totar");

	// =======RIGHT VIEW========

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("TClient Developers"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	const float TeeSize = 50.0f;
	const float CardSize = TeeSize + MarginSmall;
	CUIRect TeeRect, DevCardRect;
	static CButtonContainer s_LinkButton1, s_LinkButton2, s_LinkButton3, s_LinkButton4;
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Tater"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Tater", LineSize, TEXTALIGN_ML);
		if(DoButton_FontIcon(&s_LinkButton1, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/sjrc6");
		RenderDevSkin(TeeRect.Center(), 50.0f, "glow_mermyfox", "mermyfox", true, 0, 0, 0, false, ColorRGBA(0.92f, 0.29f, 0.48f, 1.0f), ColorRGBA(0.55f, 0.64f, 0.76f, 1.0f));
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Solly"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Solly", LineSize, TEXTALIGN_ML);
		if(DoButton_FontIcon(&s_LinkButton3, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/SollyBunny");
		RenderDevSkin(TeeRect.Center(), 50.0f, "tuzi", "tuzi", false, 0, 0, 2, true);
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Daniel"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Daniel", LineSize, TEXTALIGN_ML);
		if(DoButton_FontIcon(&s_LinkButton2, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/danielkempf");
		RenderDevSkin(TeeRect.Center(), 50.0f, "greyfox", "greyfox", true, 0, 0, 2, false, ColorRGBA(0.00f, 0.09f, 1.00f, 1.00f), ColorRGBA(1.00f, 0.92f, 0.00f, 1.00f));
	}

	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Teero"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Teero", LineSize, TEXTALIGN_ML);
		if(DoButton_FontIcon(&s_LinkButton4, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/Teero888");
		RenderDevSkin(TeeRect.Center(), 50.0f, "glow_mermyfox", "mermyfox", true, 0, 0, 0, false, ColorRGBA(1.00f, 1.00f, 1.00f, 1.00f), ColorRGBA(1.00f, 0.02f, 0.13f, 1.00f));
	}
}

void CMenus::RenderSettingsProfiles(CUIRect MainView)
{
	CUIRect Label, LabelMid, Section, LabelRight;
	static int s_SelectedProfile = -1;

	char *pSkinName = g_Config.m_ClPlayerSkin;
	int *pUseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
	unsigned *pColorBody = &g_Config.m_ClPlayerColorBody;
	unsigned *pColorFeet = &g_Config.m_ClPlayerColorFeet;
	int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;

	if(m_Dummy)
	{
		pSkinName = g_Config.m_ClDummySkin;
		pUseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		pColorBody = &g_Config.m_ClDummyColorBody;
		pColorFeet = &g_Config.m_ClDummyColorFeet;
	}

	// skin info
	CTeeRenderInfo OwnSkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Find(pSkinName);
	OwnSkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	OwnSkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	OwnSkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	OwnSkinInfo.m_CustomColoredSkin = *pUseCustomColor;
	if(*pUseCustomColor)
	{
		OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(*pColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT));
		OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(*pColorFeet).UnclampLighting(ColorHSLA::DARKEST_LGT));
	}
	else
	{
		OwnSkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		OwnSkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	OwnSkinInfo.m_Size = 50.0f;

	//======YOUR PROFILE======
	char aTempBuf[256];
	str_format(aTempBuf, sizeof(aTempBuf), "%s:", Localize("Your profile"));
	MainView.HSplitTop(LineSize, &Label, &MainView);
	Ui()->DoLabel(&Label, aTempBuf, FontSize, TEXTALIGN_ML);
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(250.0f, &Label, &LabelMid);
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
	vec2 TeeRenderPos(Label.x + LineSize, Label.y + Label.h / 2.0f + OffsetToMid.y);
	int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos);

	char aName[64];
	char aClan[64];
	str_format(aName, sizeof(aName), "%s", m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
	str_format(aClan, sizeof(aClan), "%s", m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);

	CUIRect FlagRect;
	Label.VSplitLeft(90.0f, &FlagRect, &Label);

	Label.HSplitTop(LineSize, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), Localize("Name: %s"), aName);
	Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

	Label.HSplitTop(LineSize, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), Localize("Clan: %s"), aClan);
	Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

	Label.HSplitTop(LineSize, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), Localize("Skin: %s"), pSkinName);
	Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

	FlagRect.VSplitRight(50.0f, nullptr, &FlagRect);
	FlagRect.HSplitBottom(25.0f, nullptr, &FlagRect);
	FlagRect.y -= 10.0f;
	ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_pClient->m_CountryFlags.Render(m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

	bool DoSkin = g_Config.m_ClApplyProfileSkin;
	bool DoColors = g_Config.m_ClApplyProfileColors;
	bool DoEmote = g_Config.m_ClApplyProfileEmote;
	bool DoName = g_Config.m_ClApplyProfileName;
	bool DoClan = g_Config.m_ClApplyProfileClan;
	bool DoFlag = g_Config.m_ClApplyProfileFlag;

	//======AFTER LOAD======
	if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
	{
		CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
		MainView.HSplitTop(LineSize, nullptr, &MainView);
		MainView.HSplitTop(10.0f, &Label, &MainView);
		str_format(aTempBuf, sizeof(aTempBuf), "%s:", Localize("After Load"));
		Ui()->DoLabel(&Label, aTempBuf, FontSize, TEXTALIGN_ML);

		MainView.HSplitTop(50.0f, &Label, &MainView);
		Label.VSplitLeft(250.0f, &Label, nullptr);

		if(DoSkin && strlen(LoadProfile.SkinName) != 0)
		{
			const CSkin *pLoadSkin = m_pClient->m_Skins.Find(LoadProfile.SkinName);
			OwnSkinInfo.m_OriginalRenderSkin = pLoadSkin->m_OriginalSkin;
			OwnSkinInfo.m_ColorableRenderSkin = pLoadSkin->m_ColorableSkin;
			OwnSkinInfo.m_SkinMetrics = pLoadSkin->m_Metrics;
		}
		if(*pUseCustomColor && DoColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
		{
			OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		}

		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
		TeeRenderPos = vec2(Label.x + LineSize, Label.y + Label.h / 2.0f + OffsetToMid.y);
		int LoadEmote = Emote;
		if(DoEmote && LoadProfile.Emote != -1)
			LoadEmote = LoadProfile.Emote;
		RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, LoadEmote, vec2(1.0f, 0.0f), TeeRenderPos);

		if(DoName && strlen(LoadProfile.Name) != 0)
			str_format(aName, sizeof(aName), "%s", LoadProfile.Name);
		if(DoClan && strlen(LoadProfile.Clan) != 0)
			str_format(aClan, sizeof(aClan), "%s", LoadProfile.Clan);

		Label.VSplitLeft(90.0f, &FlagRect, &Label);

		Label.HSplitTop(LineSize, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), Localize("Name: %s"), aName);
		Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

		Label.HSplitTop(LineSize, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), Localize("Clan: %s"), aClan);
		Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

		Label.HSplitTop(LineSize, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), Localize("Skin: %s"), (DoSkin && strlen(LoadProfile.SkinName) != 0) ? LoadProfile.SkinName : pSkinName);
		Ui()->DoLabel(&Section, aTempBuf, FontSize, TEXTALIGN_ML);

		FlagRect.VSplitRight(50.0f, nullptr, &FlagRect);
		FlagRect.HSplitBottom(25.0f, nullptr, &FlagRect);
		FlagRect.y -= 10.0f;
		int RenderFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
		if(DoFlag && LoadProfile.CountryFlag != -2)
			RenderFlag = LoadProfile.CountryFlag;
		m_pClient->m_CountryFlags.Render(RenderFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		str_format(aName, sizeof(aName), "%s", m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
		str_format(aClan, sizeof(aClan), "%s", m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);
	}
	else
	{
		MainView.HSplitTop(80.0f, nullptr, &MainView);
	}

	//===BUTTONS AND CHECK BOX===
	CUIRect DummyCheck, CustomCheck;
	MainView.HSplitTop(30.0f, &DummyCheck, nullptr);
	DummyCheck.HSplitTop(13.0f, nullptr, &DummyCheck);

	DummyCheck.VSplitLeft(100.0f, &DummyCheck, &CustomCheck);
	CustomCheck.VSplitLeft(150.0f, &CustomCheck, nullptr);

	DoButton_CheckBoxAutoVMarginAndSet(&m_Dummy, Localize("Dummy"), (int *)&m_Dummy, &DummyCheck, LineSize);

	static int s_CustomColorID = 0;
	CustomCheck.HSplitTop(LineSize, &CustomCheck, nullptr);

	if(DoButton_CheckBox(&s_CustomColorID, Localize("Custom colors"), *pUseCustomColor, &CustomCheck))
	{
		*pUseCustomColor = *pUseCustomColor ? 0 : 1;
		SetNeedSendInfo();
	}

	LabelMid.VSplitLeft(20.0f, nullptr, &LabelMid);
	LabelMid.VSplitLeft(160.0f, &LabelMid, &LabelRight);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileSkin, Localize("Save/Load Skin"), &g_Config.m_ClApplyProfileSkin, &LabelMid, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileColors, Localize("Save/Load Colors"), &g_Config.m_ClApplyProfileColors, &LabelMid, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileEmote, Localize("Save/Load Emote"), &g_Config.m_ClApplyProfileEmote, &LabelMid, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileName, Localize("Save/Load Name"), &g_Config.m_ClApplyProfileName, &LabelMid, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileClan, Localize("Save/Load Clan"), &g_Config.m_ClApplyProfileClan, &LabelMid, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileFlag, Localize("Save/Load Flag"), &g_Config.m_ClApplyProfileFlag, &LabelMid, LineSize);

	CUIRect Button;
	LabelRight.VSplitLeft(150.0f, &LabelRight, nullptr);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_LoadButton;

	if(DoButton_Menu(&s_LoadButton, Localize("Load"), 0, &Button))
	{
		if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
		{
			CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
			if(!m_Dummy)
			{
				if(DoSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClPlayerSkin, LoadProfile.SkinName, sizeof(g_Config.m_ClPlayerSkin));
				if(DoColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClPlayerColorBody = LoadProfile.BodyColor;
					g_Config.m_ClPlayerColorFeet = LoadProfile.FeetColor;
				}
				if(DoEmote && LoadProfile.Emote != -1)
					g_Config.m_ClPlayerDefaultEyes = LoadProfile.Emote;
				if(DoName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_PlayerName, LoadProfile.Name, sizeof(g_Config.m_PlayerName));
				if(DoClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_PlayerClan, LoadProfile.Clan, sizeof(g_Config.m_PlayerClan));
				if(DoFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_PlayerCountry = LoadProfile.CountryFlag;
			}
			else
			{
				if(DoSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClDummySkin, LoadProfile.SkinName, sizeof(g_Config.m_ClDummySkin));
				if(DoColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClDummyColorBody = LoadProfile.BodyColor;
					g_Config.m_ClDummyColorFeet = LoadProfile.FeetColor;
				}
				if(DoEmote && LoadProfile.Emote != -1)
					g_Config.m_ClDummyDefaultEyes = LoadProfile.Emote;
				if(DoName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_ClDummyName, LoadProfile.Name, sizeof(g_Config.m_ClDummyName));
				if(DoClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_ClDummyClan, LoadProfile.Clan, sizeof(g_Config.m_ClDummyClan));
				if(DoFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_ClDummyCountry = LoadProfile.CountryFlag;
			}
		}
		SetNeedSendInfo();
	}
	LabelRight.HSplitTop(5.0f, nullptr, &LabelRight);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_SaveButton;
	if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button))
	{
		GameClient()->m_SkinProfiles.AddProfile(
			DoColors ? *pColorBody : -1,
			DoColors ? *pColorFeet : -1,
			DoFlag ? CurrentFlag : -2,
			DoEmote ? Emote : -1,
			DoSkin ? pSkinName : "",
			DoName ? aName : "",
			DoClan ? aClan : "");
		GameClient()->m_SkinProfiles.SaveProfiles();
	}
	LabelRight.HSplitTop(5.0f, nullptr, &LabelRight);

	static int s_AllowDelete;
	DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, Localizable("Enable Deleting"), &s_AllowDelete, &LabelRight, LineSize);
	LabelRight.HSplitTop(5.0f, nullptr, &LabelRight);

	if(s_AllowDelete)
	{
		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
		{
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + s_SelectedProfile);
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
		LabelRight.HSplitTop(5.0f, nullptr, &LabelRight);

		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_OverrideButton;
		if(DoButton_Menu(&s_OverrideButton, Localize("Override"), 0, &Button))
		{
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile] = CProfile(
					DoColors ? *pColorBody : -1,
					DoColors ? *pColorFeet : -1,
					DoFlag ? CurrentFlag : -2,
					DoEmote ? Emote : -1,
					DoSkin ? pSkinName : "",
					DoName ? aName : "",
					DoClan ? aClan : "");
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
	}

	//---RENDER THE SELECTOR---
	CUIRect FileButton;
	CUIRect SelectorRect;
	MainView.HSplitTop(50.0f, nullptr, &SelectorRect);
	SelectorRect.HSplitBottom(LineSize, &SelectorRect, &FileButton);
	SelectorRect.HSplitBottom(MarginSmall, &SelectorRect, nullptr);
	std::vector<CProfile> *pProfileList = &GameClient()->m_SkinProfiles.m_Profiles;

	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, pProfileList->size(), 4, 3, s_SelectedProfile, &SelectorRect, true);

	static bool s_Indexs[1024];

	for(size_t i = 0; i < pProfileList->size(); ++i)
	{
		CProfile CurrentProfile = GameClient()->m_SkinProfiles.m_Profiles[i];

		char RenderSkin[24];
		if(strlen(CurrentProfile.SkinName) == 0)
			str_copy(RenderSkin, pSkinName, sizeof(RenderSkin));
		else
			str_copy(RenderSkin, CurrentProfile.SkinName, sizeof(RenderSkin));

		const CSkin *pSkinToBeDraw = m_pClient->m_Skins.Find(RenderSkin);

		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexs[i], s_SelectedProfile >= 0 && (size_t)s_SelectedProfile == i);

		if(!Item.m_Visible)
			continue;

		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			Info.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_CustomColoredSkin = true;
			Info.m_OriginalRenderSkin = pSkinToBeDraw->m_OriginalSkin;
			Info.m_ColorableRenderSkin = pSkinToBeDraw->m_ColorableSkin;
			Info.m_SkinMetrics = pSkinToBeDraw->m_Metrics;
			Info.m_Size = 50.0f;
			if(CurrentProfile.BodyColor == -1 && CurrentProfile.FeetColor == -1)
			{
				Info.m_CustomColoredSkin = m_Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor;
				Info.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
				Info.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
			}

			CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &Info, OffsetToMid);

			int RenderEmote = CurrentProfile.Emote == -1 ? Emote : CurrentProfile.Emote;
			TeeRenderPos = vec2(Item.m_Rect.x + 30.0f, Item.m_Rect.y + Item.m_Rect.h / 2.0f + OffsetToMid.y);

			Item.m_Rect.VSplitLeft(60.0f, nullptr, &Item.m_Rect);
			CUIRect PlayerRect, ClanRect, FeetColorSquare, BodyColorSquare;

			Item.m_Rect.VSplitLeft(60.0f, nullptr, &BodyColorSquare); // Delete this maybe

			Item.m_Rect.VSplitRight(60.0f, &BodyColorSquare, &FlagRect);
			BodyColorSquare.x -= 11.0f;
			BodyColorSquare.VSplitLeft(10.0f, &BodyColorSquare, nullptr);
			BodyColorSquare.HSplitMid(&BodyColorSquare, &FeetColorSquare);
			BodyColorSquare.HSplitMid(nullptr, &BodyColorSquare);
			FeetColorSquare.HSplitMid(&FeetColorSquare, nullptr);
			FlagRect.HSplitBottom(10.0f, &FlagRect, nullptr);
			FlagRect.HSplitTop(10.0f, nullptr, &FlagRect);

			Item.m_Rect.HSplitMid(&PlayerRect, &ClanRect);

			SLabelProperties Props;
			Props.m_MaxWidth = Item.m_Rect.w;
			if(CurrentProfile.CountryFlag != -2)
				m_pClient->m_CountryFlags.Render(CurrentProfile.CountryFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

			if(CurrentProfile.BodyColor != -1 && CurrentProfile.FeetColor != -1)
			{
				ColorRGBA BodyColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
				ColorRGBA FeetColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));

				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BodyColor.r, BodyColor.g, BodyColor.b, 1.0f);
				IGraphics::CQuadItem Quads[2];
				Quads[0] = IGraphics::CQuadItem(BodyColorSquare.x, BodyColorSquare.y, BodyColorSquare.w, BodyColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[0], 1);
				Graphics()->SetColor(FeetColor.r, FeetColor.g, FeetColor.b, 1.0f);
				Quads[1] = IGraphics::CQuadItem(FeetColorSquare.x, FeetColorSquare.y, FeetColorSquare.w, FeetColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[1], 1);
				Graphics()->QuadsEnd();
			}
			RenderTools()->RenderTee(pIdleState, &Info, RenderEmote, vec2(1.0f, 0.0f), TeeRenderPos);

			if(strlen(CurrentProfile.Name) == 0 && strlen(CurrentProfile.Clan) == 0)
			{
				PlayerRect = Item.m_Rect;
				PlayerRect.y += MarginSmall;
				Ui()->DoLabel(&PlayerRect, CurrentProfile.SkinName, FontSize, TEXTALIGN_ML, Props);
			}
			else
			{
				Ui()->DoLabel(&PlayerRect, CurrentProfile.Name, FontSize, TEXTALIGN_ML, Props);
				Item.m_Rect.HSplitTop(LineSize, nullptr, &Item.m_Rect);
				Props.m_MaxWidth = Item.m_Rect.w;
				Ui()->DoLabel(&ClanRect, CurrentProfile.Clan, FontSize, TEXTALIGN_ML, Props);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(s_SelectedProfile != NewSelected)
	{
		s_SelectedProfile = NewSelected;
	}
	static CButtonContainer s_ProfilesFile;
	FileButton.VSplitLeft(130.0f, &FileButton, nullptr);
	if(DoButton_Menu(&s_ProfilesFile, Localize("Profiles file"), 0, &FileButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, PROFILES_FILE, aTempBuf, sizeof(aTempBuf));
		Client()->ViewFile(aTempBuf);
	}
}

void CMenus::RenderDevSkin(vec2 RenderPos, float Size, const char *pSkinName, const char *pBackupSkin, bool CustomColors, int FeetColor, int BodyColor, int Emote, bool Rainbow, ColorRGBA ColorFeet, ColorRGBA ColorBody)
{
	bool WhiteFeetTemp = g_Config.m_ClWhiteFeet;
	g_Config.m_ClWhiteFeet = false;

	float DefTick = std::fmod(s_Time, 1.0f);

	CTeeRenderInfo SkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Find(pSkinName);
	if(str_comp(pSkin->GetName(), pSkinName) != 0)
		pSkin = m_pClient->m_Skins.Find(pBackupSkin);

	SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	SkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	SkinInfo.m_CustomColoredSkin = CustomColors;
	if(SkinInfo.m_CustomColoredSkin)
	{
		SkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		SkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		if(ColorFeet.a != 0.0f)
		{
			SkinInfo.m_ColorBody = ColorBody;
			SkinInfo.m_ColorFeet = ColorFeet;
		}
	}
	else
	{
		SkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		SkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	if(Rainbow)
	{
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(DefTick, 1.0f, 0.5f));
		SkinInfo.m_ColorBody = Col;
		SkinInfo.m_ColorFeet = Col;
	}
	SkinInfo.m_Size = Size;
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &SkinInfo, OffsetToMid);
	vec2 TeeRenderPos(RenderPos.x, RenderPos.y + OffsetToMid.y);
	RenderTools()->RenderTee(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos);
	g_Config.m_ClWhiteFeet = WhiteFeetTemp;
}

void CMenus::RenderFontIcon(const CUIRect Rect, const char *pText, float Size, int Align)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&Rect, pText, Size, Align);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

int CMenus::DoButtonNoRect_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextSelectionColor());
	if(Ui()->HotItem() == pButtonContainer)
	{
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	CUIRect Temp;
	pRect->HMargin(0.0f, &Temp);
	Ui()->DoLabel(&Temp, pText, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect);
}
