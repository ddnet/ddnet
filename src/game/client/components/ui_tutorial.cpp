#include "ui_tutorial.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

static constexpr float TRANSITION_DURATION = 0.3f;
static constexpr float INTRO_DURATION = 0.4f;
static constexpr float HIGHLIGHT_PADDING = 3.0f;
static constexpr float SCREEN_MARGIN = 10.0f;
static constexpr float INFOBOX_GAP = 10.0f;

static constexpr float BOX_BORDER = 1.0f;
static constexpr float BOX_PADDING = 8.0f;
static constexpr float BOX_SPACING = 4.0f;
static constexpr float TITLE_FONT_SIZE = 14.0f;
static constexpr float TITLE_HEIGHT = 18.0f;
static constexpr float TEXT_FONT_SIZE = 10.0f;
static constexpr float BUTTON_HEIGHT = 18.0f;
static constexpr float BUTTON_WIDTH = 70.0f;
static constexpr float BUTTON_SPACING = 5.0f;
static constexpr float CLOSE_BUTTON_SIZE = 16.0f;
static constexpr float MIN_BOX_WIDTH = 240.0f;

static const ColorRGBA INFOBOX_BORDER_COLOR = ColorRGBA(1.0f, 0.8f, 0.2f, 1.0f);

const CUiTutorial::STutorialInfo &CUiTutorial::TutorialInfo(ETutorial Tutorial)
{
	static const SStep s_aServerBrowserTutorialSteps[] = {
		{
			.m_pTitle = Localizable("Welcome to DDNet!"),
			.m_pText = Localizable("This short tutorial will show you how to find a server to play on. Click \"Play\" to open the server browser."),
			.m_Target = ETarget::START_PLAY,
			.m_CompletedByTarget = ETarget::BROWSER_LIST,
			.m_pfnOnEnter = [](CUiTutorial *pThis) {
				pThis->GameClient()->m_Menus.SetShowStart(true);
			},
		},
		{
			.m_pTitle = Localizable("Server browser"),
			.m_pText = Localizable("Use these tabs to switch between Internet servers, LAN servers, your Favorites, and your favorite communities. Try clicking them!"),
			.m_Target = ETarget::BROWSER_TABS,
			.m_AllowInteraction = true,
			.m_pfnOnEnter = [](CUiTutorial *pThis) { pThis->GameClient()->m_Menus.SetMenuPage(CMenus::PAGE_INTERNET); },
			.m_pfnOnLeave = [](CUiTutorial *pThis) { pThis->GameClient()->m_Menus.SetMenuPage(CMenus::PAGE_INTERNET); },
		},
		{
			.m_pTitle = Localizable("Server list"),
			.m_pText = Localizable("All servers matching your filters are shown here. Click on a column header to sort the list. Select a server to see more information, or double-click it to join."),
			.m_Target = ETarget::BROWSER_LIST,
			.m_AllowInteraction = true,
			.m_pfnOnLeave = [](CUiTutorial *) {
				// The user may have changed the sorting, keep the defaults because they are the best to find good servers
				g_Config.m_BrSort = DefaultConfig::BrSort;
				g_Config.m_BrSortOrder = DefaultConfig::BrSortOrder;
			},
		},
		{
			.m_pTitle = Localizable("Server filter"),

			.m_pText = Localizable("Here you can hide servers you are not interested in, such as empty or full servers. It's okay to leave these settings at their defaults."),
			.m_Target = ETarget::BROWSER_TOOLBOX,
			.m_IntroTarget = ETarget::BROWSER_TOOLBOX_TAB,
			.m_AllowInteraction = false, // don't allow interaction, the default settings are fine for new players
			.m_pfnOnEnter = [](CUiTutorial *) { g_Config.m_UiToolboxPage = CMenus::UI_TOOLBOX_PAGE_FILTERS; },
			.m_InfoBoxSide = EBoxSide::LEFT, // keep the tabs above the toolbox visible
		},
		{
			.m_pTitle = Localizable("Server info"),
			.m_pText = Localizable("This tab shows details about the selected server, such as the players currently online."),
			.m_Target = ETarget::BROWSER_TOOLBOX,
			.m_IntroTarget = ETarget::BROWSER_TOOLBOX_TAB,
			.m_AllowInteraction = true,
			.m_pfnOnEnter = [](CUiTutorial *pThis) {
				pThis->SelectTopServer();
				g_Config.m_UiToolboxPage = CMenus::UI_TOOLBOX_PAGE_INFO;
			},
			.m_InfoBoxSide = EBoxSide::LEFT,
		},
		{
			.m_pTitle = Localizable("Friends"),
			.m_pText = Localizable("You can add players to your friends to quickly see which server they're playing on right now."),
			.m_Target = ETarget::BROWSER_TOOLBOX,
			.m_IntroTarget = ETarget::BROWSER_TOOLBOX_TAB,
			.m_AllowInteraction = true,
			.m_pfnOnEnter = [](CUiTutorial *) { g_Config.m_UiToolboxPage = CMenus::UI_TOOLBOX_PAGE_FRIENDS; },
			.m_InfoBoxSide = EBoxSide::LEFT,
		},
		{
			.m_pTitle = Localizable("Search and exclude"),
			.m_pText = Localizable("You can search for \"Novice\" to find easy maps, or for \"Moderate\", \"Brutal\" and \"Insane\" to find harder ones. As a new player, you should start with Novice maps! Servers matching the exclude field are hidden."),
			.m_Target = ETarget::BROWSER_SEARCH,
			.m_AllowInteraction = true,
		},
		{
			.m_pTitle = Localizable("Join a server"),
			.m_pText = Localizable("This button joins the selected server. You can also double-click a server in the list to join it. Have fun!"),
			.m_Target = ETarget::BROWSER_CONNECT,
		},
	};

	static const STutorialInfo s_aTutorials[] = {
		{"server_browser", s_aServerBrowserTutorialSteps, (int)std::size(s_aServerBrowserTutorialSteps), true},
	};
	static_assert(std::size(s_aTutorials) == (size_t)ETutorial::NUM);

	return s_aTutorials[(int)Tutorial];
}

void CUiTutorial::OnConsoleInit()
{
	Console()->Register("start_tutorial", "s['server_browser']", CFGFLAG_CLIENT, ConUiTutorial, this, "Start a tutorial explaining the user interface");
	Console()->Register("stop_tutorial", "", CFGFLAG_CLIENT, ConUiTutorialStop, this, "Stop the active tutorial");
}

void CUiTutorial::OnInit()
{
	if(g_Config.m_ClShowWelcome)
	{
		// Remember to show the server browser tutorial after the first launch of the client
		g_Config.m_ClShowBrowserTutorial = 1;
	}
}

void CUiTutorial::OnStateChange(int NewState, int OldState)
{
	// The tutorial is started again when the client is offline, unless the player finished or closed it
	if(IsActive() && TutorialInfo(*m_ActiveTutorial).m_OfflineOnly && NewState != IClient::STATE_OFFLINE)
	{
		Reset();
	}
}

void CUiTutorial::ConUiTutorial(IConsole::IResult *pResult, void *pUserData)
{
	CUiTutorial *pThis = static_cast<CUiTutorial *>(pUserData);
	const char *pName = pResult->GetString(0);
	for(int Tutorial = 0; Tutorial < (int)ETutorial::NUM; ++Tutorial)
	{
		if(str_comp_nocase(pName, TutorialInfo((ETutorial)Tutorial).m_pName) == 0)
		{
			pThis->Start((ETutorial)Tutorial);
			return;
		}
	}
	log_error("ui_tutorial", "Unknown tutorial '%s'", pName);
}

void CUiTutorial::ConUiTutorialStop(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CUiTutorial *>(pUserData)->Stop();
}

void CUiTutorial::Start(ETutorial Tutorial)
{
	const STutorialInfo &Info = TutorialInfo(Tutorial);
	if(Info.m_OfflineOnly && Client()->State() != IClient::STATE_OFFLINE)
	{
		log_error("ui_tutorial", "The tutorial '%s' can only be started in the main menu", Info.m_pName);
		return;
	}

	Ui()->ClosePopupMenus();
	m_ActiveTutorial = Tutorial;
	m_Step = 0;
	m_LastHighlight.reset();
	m_LastBoxPosition.reset();
	SetStep(0);
}

void CUiTutorial::Stop()
{
	if(IsActive() && CurrentStep().m_pfnOnLeave != nullptr)
	{
		CurrentStep().m_pfnOnLeave(this);
	}

	if(m_ActiveTutorial == ETutorial::SERVER_BROWSER)
	{
		g_Config.m_ClShowBrowserTutorial = 0;
	}
	Reset();
}

void CUiTutorial::Reset()
{
	m_ActiveTutorial.reset();
	m_Visible = false;
	m_Interactive = false;
}

const CUiTutorial::SStep &CUiTutorial::CurrentStep() const
{
	return TutorialInfo(*m_ActiveTutorial).m_pSteps[m_Step];
}

int CUiTutorial::NumSteps() const
{
	return TutorialInfo(*m_ActiveTutorial).m_NumSteps;
}

void CUiTutorial::SetStep(int Step)
{
	if(Step < 0 || Step >= NumSteps())
	{
		Stop();
		return;
	}

	if(Step != m_Step && CurrentStep().m_pfnOnLeave != nullptr)
	{
		CurrentStep().m_pfnOnLeave(this);
	}

	DeactivateMenuTextInput();
	m_Step = Step;
	m_StepStartTime = Client()->GlobalTime();
	m_TransitionFrom = m_LastHighlight;
	m_BoxTransitionFrom = m_LastBoxPosition;

	const SStep &NewStep = CurrentStep();
	if(NewStep.m_pfnOnEnter != nullptr)
	{
		NewStep.m_pfnOnEnter(this);
	}
}

void CUiTutorial::DeactivateMenuTextInput()
{
	// The text fields of the console and chat must keep working, e.g. when the tutorial is started from the console
	const CLineInput *pActiveInput = CLineInput::GetActiveInput();
	if(pActiveInput == nullptr || GameClient()->m_GameConsole.IsActive() || GameClient()->m_Chat.IsActive())
	{
		return;
	}

	// Also change the last active item, which only changes with a non-null active item
	Ui()->SetActiveItem(&m_BlockerId);
	Ui()->SetActiveItem(nullptr);
	pActiveInput->Deactivate();
}

void CUiTutorial::NextStep()
{
	SetStep(m_Step + 1);
}

void CUiTutorial::PreviousStep()
{
	if(CanGoBack())
	{
		SetStep(m_Step - 1);
	}
}

bool CUiTutorial::CanGoBack() const
{
	// Can't go back to steps that are completed by an action
	return IsActive() && m_Step > 0 && TutorialInfo(*m_ActiveTutorial).m_pSteps[m_Step - 1].m_CompletedByTarget == ETarget::NONE;
}

bool CUiTutorial::SelectTopServer()
{
	// Keep the server that the user selected in the list
	for(int Index = 0; Index < ServerBrowser()->NumSortedServers(); ++Index)
	{
		if(str_comp(ServerBrowser()->SortedGet(Index)->m_aAddress, g_Config.m_UiServerAddress) == 0)
			return true;
	}

	// Otherwise select the top DDNet server
	for(int Index = 0; Index < ServerBrowser()->NumSortedServers(); ++Index)
	{
		const CServerInfo *pInfo = ServerBrowser()->SortedGet(Index);
		if(str_comp(pInfo->m_aCommunityId, IServerBrowser::COMMUNITY_DDNET) == 0)
		{
			GameClient()->m_Menus.SelectServer(pInfo->m_aAddress);
			return true;
		}
	}
	return false;
}

void CUiTutorial::SetTargetRect(ETarget Target, const CUIRect &Rect)
{
	dbg_assert(Target > ETarget::NONE && Target < ETarget::NUM, "Invalid tutorial target %d", (int)Target);
	m_aTargetRects[(size_t)Target] = Rect;
}

void CUiTutorial::OnRender()
{
	if(g_Config.m_ClShowBrowserTutorial && !IsActive() && Client()->State() == IClient::STATE_OFFLINE)
	{
		Start(ETutorial::SERVER_BROWSER);
	}

	// The overlay is only rendered by the menus, so it is not visible while they are closed
	if(!GameClient()->m_Menus.IsActive())
	{
		m_Visible = false;
	}
}

void CUiTutorial::RenderMenuOverlay()
{
	if(GameClient()->m_Menus.IsActive())
	{
		RenderOverlay();
	}
	m_aTargetRects.fill(std::nullopt);
}

static CUIRect PadHighlight(CUIRect Rect)
{
	Rect.x -= HIGHLIGHT_PADDING;
	Rect.y -= HIGHLIGHT_PADDING;
	Rect.w += 2.0f * HIGHLIGHT_PADDING;
	Rect.h += 2.0f * HIGHLIGHT_PADDING;
	return Rect;
}

std::optional<CUIRect> CUiTutorial::FindTargetRect(ETarget Target) const
{
	if(Target == ETarget::NONE || !m_aTargetRects[(size_t)Target].has_value())
		return std::nullopt;
	return PadHighlight(*m_aTargetRects[(size_t)Target]);
}

std::optional<CUIRect> CUiTutorial::FindHighlightRect(const SStep &Step, const CUIRect &Screen) const
{
	if(Step.m_Target != ETarget::NONE)
		return FindTargetRect(Step.m_Target);

	if(Step.m_Area.w > 0.0f && Step.m_Area.h > 0.0f)
		return PadHighlight({Step.m_Area.x * Screen.w, Step.m_Area.y * Screen.h, Step.m_Area.w * Screen.w, Step.m_Area.h * Screen.h});

	return std::nullopt;
}

void CUiTutorial::RenderOverlay()
{
	m_Visible = false;
	m_Interactive = false;
	m_PassThrough = false;

	if(!IsActive())
		return;

	if(!GameClient()->m_Menus.CanDisplayWarning())
		return; // a fullscreen popup is open

	// Complete interactive steps once the element we are waiting for is shown
	while(CurrentStep().m_CompletedByTarget != ETarget::NONE && m_aTargetRects[(size_t)CurrentStep().m_CompletedByTarget].has_value())
	{
		NextStep();
		if(!IsActive())
			return;
	}

	const SStep &Step = CurrentStep();

	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const std::optional<CUIRect> Target = FindHighlightRect(Step, Screen);
	m_Interactive = Step.m_CompletedByTarget != ETarget::NONE;

	const auto &&EaseOut = [](float Time) {
		const float Progress = std::clamp(Time / TRANSITION_DURATION, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - Progress, 3.0f);
	};

	const float StepTime = Client()->GlobalTime() - m_StepStartTime;

	// Highlight the intro element first, then change the highlight to the target
	std::optional<CUIRect> HighlightTarget = Target;
	std::optional<CUIRect> TransitionFrom = m_TransitionFrom;
	float TransitionTime = StepTime;
	bool IntroShown = false;
	const std::optional<CUIRect> IntroTarget = FindTargetRect(Step.m_IntroTarget);
	if(IntroTarget.has_value() && Target.has_value())
	{
		if(StepTime < TRANSITION_DURATION + INTRO_DURATION)
		{
			HighlightTarget = IntroTarget;
			IntroShown = true;
		}
		else
		{
			TransitionFrom = IntroTarget;
			TransitionTime = StepTime - (TRANSITION_DURATION + INTRO_DURATION);
		}
	}

	const float Ease = EaseOut(TransitionTime);

	// Move the highlight from the previous to the new element, or fade in the overlay when it is shown first
	std::optional<CUIRect> Highlight = HighlightTarget;
	float DimAlpha = 1.0f;
	if(HighlightTarget.has_value() && TransitionFrom.has_value())
	{
		Highlight->x = mix(TransitionFrom->x, HighlightTarget->x, Ease);
		Highlight->y = mix(TransitionFrom->y, HighlightTarget->y, Ease);
		Highlight->w = mix(TransitionFrom->w, HighlightTarget->w, Ease);
		Highlight->h = mix(TransitionFrom->h, HighlightTarget->h, Ease);
	}
	else if(!TransitionFrom.has_value())
	{
		DimAlpha = Ease;
	}

	m_LastHighlight = Highlight;

	// Don't block if the element is not visible, so the user can navigate back to it
	const bool Blocking = !m_Interactive || Highlight.has_value();
	if(Blocking)
	{
		RenderDim(Screen, Highlight, DimAlpha);
	}

	CUIRect Box = PlaceInfoBox(Screen, Target, Step);
	if(m_BoxTransitionFrom.has_value())
	{
		// Don't move the info box during intro
		const float BoxEase = EaseOut(StepTime);
		Box.x = mix(m_BoxTransitionFrom->x, Box.x, BoxEase);
		Box.y = mix(m_BoxTransitionFrom->y, Box.y, BoxEase);
	}
	m_LastBoxPosition = Box.TopLeft();

	// Prevent UI elements below the overlay from being activated
	bool InsideClickableArea = false;
	if((m_Interactive || Step.m_AllowInteraction) && Target.has_value() && !IntroShown)
	{
		CUIRect ClickableArea;
		Target->Margin(HIGHLIGHT_PADDING, &ClickableArea);
		InsideClickableArea = Ui()->MouseInside(&ClickableArea) && !Ui()->MouseInside(&Box);
	}
	const bool PassThrough = !Blocking || InsideClickableArea;
	m_PassThrough = PassThrough;
	if(!PassThrough)
	{
		Ui()->SetHotItem(&m_BlockerId);
		Ui()->SetHotScrollRegion(nullptr);
	}

	m_Visible = true;
	RenderInfoBox(Box, Step);
}

void CUiTutorial::RenderDim(const CUIRect &Screen, const std::optional<CUIRect> &Highlight, float Alpha) const
{
	const ColorRGBA DimColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.65f * Alpha);
	if(!Highlight.has_value())
	{
		Screen.Draw(DimColor, IGraphics::CORNER_NONE, 0.0f);
		return;
	}

	const CUIRect &H = *Highlight;
	const CUIRect aDimRects[] = {
		{Screen.x, Screen.y, Screen.w, H.y - Screen.y},
		{Screen.x, H.y + H.h, Screen.w, Screen.y + Screen.h - (H.y + H.h)},
		{Screen.x, H.y, H.x - Screen.x, H.h},
		{H.x + H.w, H.y, Screen.x + Screen.w - (H.x + H.w), H.h},
	};

	for(const auto &DimRect : aDimRects)
	{
		if(DimRect.w > 0.0f && DimRect.h > 0.0f)
		{
			DimRect.Draw(DimColor, IGraphics::CORNER_NONE, 0.0f);
		}
	}
}

float CUiTutorial::InfoBoxHeight(const SStep &Step, float Width) const
{
	const float InnerWidth = Width - 2.0f * (BOX_BORDER + BOX_PADDING);
	const float TextHeight = TextRender()->TextBoundingBox(TEXT_FONT_SIZE, Localize(Step.m_pText), -1, InnerWidth).m_H;
	return 2.0f * (BOX_BORDER + BOX_PADDING) + TITLE_HEIGHT + BOX_SPACING + TextHeight + 2.0f * BOX_SPACING + BUTTON_HEIGHT;
}

CUIRect CUiTutorial::PlaceInfoBox(const CUIRect &Screen, const std::optional<CUIRect> &Highlight, const SStep &Step) const
{
	const float MaxWidth = std::min(300.0f, Screen.w - 2.0f * SCREEN_MARGIN);

	if(!Highlight.has_value())
	{
		const float Height = InfoBoxHeight(Step, MaxWidth);
		// Interactive steps without a visible element do not dim the screen, so keep the box out of the way
		const float Y = m_Interactive ? Screen.y + Screen.h - SCREEN_MARGIN - Height : Screen.y + (Screen.h - Height) / 2.0f;
		return {Screen.x + (Screen.w - MaxWidth) / 2.0f, Y, MaxWidth, Height};
	}

	const CUIRect &H = *Highlight;
	const vec2 Center = H.Center();
	const float ScreenRight = Screen.x + Screen.w;
	const float ScreenBottom = Screen.y + Screen.h;

	const auto &&TryPlace = [&](EBoxSide Side) -> std::optional<CUIRect> {
		if(Side == EBoxSide::BELOW || Side == EBoxSide::ABOVE)
		{
			const float Height = InfoBoxHeight(Step, MaxWidth);
			const float X = std::clamp(Center.x - MaxWidth / 2.0f, Screen.x + SCREEN_MARGIN, ScreenRight - SCREEN_MARGIN - MaxWidth);
			if(Side == EBoxSide::BELOW && H.y + H.h + INFOBOX_GAP + Height + SCREEN_MARGIN <= ScreenBottom)
				return CUIRect{X, H.y + H.h + INFOBOX_GAP, MaxWidth, Height};
			if(Side == EBoxSide::ABOVE && H.y - INFOBOX_GAP - Height >= Screen.y + SCREEN_MARGIN)
				return CUIRect{X, H.y - INFOBOX_GAP - Height, MaxWidth, Height};
			return std::nullopt;
		}

		// Next to the highlighted area, with a smaller width if necessary
		for(const float Width : {MaxWidth, MIN_BOX_WIDTH})
		{
			if(Width > MaxWidth)
				continue;
			const float Height = InfoBoxHeight(Step, Width);
			const float Y = std::clamp(Center.y - Height / 2.0f, Screen.y + SCREEN_MARGIN, ScreenBottom - SCREEN_MARGIN - Height);
			if(Side == EBoxSide::RIGHT && H.x + H.w + INFOBOX_GAP + Width + SCREEN_MARGIN <= ScreenRight)
				return CUIRect{H.x + H.w + INFOBOX_GAP, Y, Width, Height};
			if(Side == EBoxSide::LEFT && H.x - INFOBOX_GAP - Width >= Screen.x + SCREEN_MARGIN)
				return CUIRect{H.x - INFOBOX_GAP - Width, Y, Width, Height};
		}
		return std::nullopt;
	};

	if(Step.m_InfoBoxSide != EBoxSide::AUTO)
	{
		if(const std::optional<CUIRect> Box = TryPlace(Step.m_InfoBoxSide))
			return *Box;
	}
	for(const EBoxSide Side : {EBoxSide::BELOW, EBoxSide::ABOVE, EBoxSide::RIGHT, EBoxSide::LEFT})
	{
		if(const std::optional<CUIRect> Box = TryPlace(Side))
			return *Box;
	}

	// If the highlighted area covers most of the screen, show the box inside of it
	const float Height = InfoBoxHeight(Step, MaxWidth);
	const float X = std::clamp(Center.x - MaxWidth / 2.0f, Screen.x + SCREEN_MARGIN, ScreenRight - SCREEN_MARGIN - MaxWidth);
	const float Y = std::clamp(H.y + H.h - Height - 2.0f * SCREEN_MARGIN, Screen.y + SCREEN_MARGIN, ScreenBottom - SCREEN_MARGIN - Height);
	return {X, Y, MaxWidth, Height};
}

void CUiTutorial::RenderInfoBox(const CUIRect &Box, const SStep &Step)
{
	Box.Draw(INFOBOX_BORDER_COLOR.WithAlpha(0.8f), IGraphics::CORNER_ALL, 6.0f);
	CUIRect Inner;
	Box.Margin(BOX_BORDER, &Inner);
	Inner.Draw(ColorRGBA(0.08f, 0.08f, 0.1f, 0.95f), IGraphics::CORNER_ALL, 5.0f);
	Inner.Margin(BOX_PADDING, &Inner);

	CUIRect TitleRow, Title, Counter, CloseButton, Content, Bottom;
	Inner.HSplitTop(TITLE_HEIGHT, &TitleRow, &Content);
	Content.HSplitTop(BOX_SPACING, nullptr, &Content);
	TitleRow.VSplitRight(CLOSE_BUTTON_SIZE, &TitleRow, &CloseButton);
	CloseButton.HMargin((CloseButton.h - CLOSE_BUTTON_SIZE) / 2.0f, &CloseButton);
	TitleRow.VSplitRight(BOX_SPACING, &TitleRow, nullptr);
	TitleRow.VSplitRight(40.0f, &Title, &Counter);

	Ui()->DoLabel(&Title, Localize(Step.m_pTitle), TITLE_FONT_SIZE, TEXTALIGN_ML, {.m_MaxWidth = Title.w, .m_EllipsisAtEnd = true});

	char aCounter[16];
	str_format(aCounter, sizeof(aCounter), "%d/%d", m_Step + 1, NumSteps());
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f));
	Ui()->DoLabel(&Counter, aCounter, TEXT_FONT_SIZE, TEXTALIGN_MR);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	Content.HSplitBottom(BUTTON_HEIGHT, &Content, &Bottom);
	Content.HSplitBottom(2.0f * BOX_SPACING, &Content, nullptr);
	Ui()->DoLabel(&Content, Localize(Step.m_pText), TEXT_FONT_SIZE, TEXTALIGN_TL, {.m_MaxWidth = Content.w});

	CUIRect BackButton, NextButton;
	Bottom.VSplitRight(BUTTON_WIDTH, &Bottom, &NextButton);
	Bottom.VSplitRight(BUTTON_SPACING, &Bottom, nullptr);
	Bottom.VSplitRight(BUTTON_WIDTH, &Bottom, &BackButton);

	const bool SkipClicked = Ui()->DoButton_FontIcon(&m_CloseButton, FontIcon::XMARK, 0, &CloseButton, BUTTONFLAG_LEFT);
	Ui()->DoToolTip(&m_CloseButton, &CloseButton, Localize("Skip tutorial"));
	if(SkipClicked)
	{
		Stop();
		return;
	}

	if(CanGoBack() && GameClient()->m_Menus.DoButton_Menu(&m_BackButton, Localize("Back"), 0, &BackButton))
	{
		PreviousStep();
		return;
	}

	if(!m_Interactive)
	{
		const bool LastStep = m_Step == NumSteps() - 1;
		if(GameClient()->m_Menus.DoButton_Menu(&m_NextButton, LastStep ? Localize("Finish") : Localize("Next"), 0, &NextButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.5f, 1.0f, 0.5f, 0.5f)))
		{
			NextStep();
		}
	}
}

bool CUiTutorial::OnInput(const IInput::CEvent &Event)
{
	if(!m_Visible || !IsActive())
		return false;

	const bool Pressed = (Event.m_Flags & IInput::FLAG_PRESS) != 0 && (Event.m_Flags & IInput::FLAG_REPEAT) == 0;
	if(Pressed && Event.m_Key == KEY_ESCAPE)
	{
		// Only the close button ends the tutorial
		DeactivateMenuTextInput();
		return true;
	}

	// Text fields in the highlighted area of steps that allow interaction
	const bool Typing = CurrentStep().m_AllowInteraction && CLineInput::GetActiveInput() != nullptr;

	if(Typing &&
		Event.m_Key != KEY_TAB && Event.m_Key != KEY_RETURN && Event.m_Key != KEY_KP_ENTER &&
		Event.m_Key != KEY_UP && Event.m_Key != KEY_DOWN && Event.m_Key != KEY_PAGEUP && Event.m_Key != KEY_PAGEDOWN)
		return false;

	// Allow scrolling the highlighted area while the mouse is over it
	if(m_PassThrough && (Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN))
		return false;

	return !m_Interactive;
}
