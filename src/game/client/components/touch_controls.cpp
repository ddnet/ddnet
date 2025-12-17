#include "touch_controls.h"

#include <base/color.h>
#include <base/log.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/localization.h>

#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/console.h>
#include <game/client/components/controls.h>
#include <game/client/components/emoticon.h>
#include <game/client/components/menus.h>
#include <game/client/components/spectator.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <queue>

using namespace std::chrono_literals;

// TODO: Add combined weapon picker button that shows all currently available weapons
// TODO: Add "joystick-aim-relative", a virtual joystick that moves the mouse pointer relatively. And add "aim-relative" ingame direct touch input.
// TODO: Add "choice" predefined behavior which shows a selection popup for 2 or more other behaviors?
// TODO: Support changing labels of menu buttons (or support overriding label for all predefined button behaviors)?

static constexpr const char *const ACTION_NAMES[] = {Localizable("Aim"), Localizable("Fire"), Localizable("Hook")};
static constexpr const char *const ACTION_SWAP_NAMES[] = {/* unused */ "", Localizable("Active: Fire"), Localizable("Active: Hook")};
static constexpr const char *const ACTION_COMMANDS[] = {/* unused */ "", "+fire", "+hook"};

static constexpr std::chrono::milliseconds LONG_TOUCH_DURATION = 500ms;
static constexpr std::chrono::milliseconds BIND_REPEAT_INITIAL_DELAY = 250ms;
static constexpr std::chrono::nanoseconds BIND_REPEAT_RATE = std::chrono::nanoseconds(1s) / 15;

static constexpr const char *const CONFIGURATION_FILENAME = "touch_controls.json";

/* This is required for the localization script to find the labels of the default bind buttons specified in the configuration file:
Localizable("Move left") Localizable("Move right") Localizable("Jump") Localizable("Prev. weapon") Localizable("Next weapon")
Localizable("Zoom out") Localizable("Default zoom") Localizable("Zoom in") Localizable("Scoreboard") Localizable("Chat") Localizable("Team chat")
Localizable("Vote yes") Localizable("Vote no") Localizable("Toggle dummy")
*/

CTouchControls::CTouchButton::CTouchButton(CTouchControls *pTouchControls) :
	m_pTouchControls(pTouchControls),
	m_UnitRect({0, 0, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MINIMUM}),
	m_Shape(EButtonShape::RECT),
	m_pBehavior(nullptr),
	m_VisibilityCached(false)
{
}

CTouchControls::CTouchButton::CTouchButton(CTouchButton &&Other) noexcept :
	m_pTouchControls(Other.m_pTouchControls),
	m_UnitRect(Other.m_UnitRect),
	m_Shape(Other.m_Shape),
	m_vVisibilities(Other.m_vVisibilities),
	m_pBehavior(std::move(Other.m_pBehavior)),
	m_VisibilityCached(false)
{
	Other.m_pTouchControls = nullptr;
	UpdatePointers();
	UpdateScreenFromUnitRect();
}

CTouchControls::CTouchButton &CTouchControls::CTouchButton::operator=(CTouchButton &&Other) noexcept
{
	if(this == &Other)
	{
		return *this;
	}
	m_pTouchControls = Other.m_pTouchControls;
	Other.m_pTouchControls = nullptr;
	m_UnitRect = Other.m_UnitRect;
	m_Shape = Other.m_Shape;
	m_vVisibilities = Other.m_vVisibilities;
	m_pBehavior = std::move(Other.m_pBehavior);
	m_VisibilityCached = false;
	UpdatePointers();
	UpdateScreenFromUnitRect();
	return *this;
}

void CTouchControls::CTouchButton::UpdatePointers()
{
	m_pBehavior->Init(this);
}

void CTouchControls::CTouchButton::UpdateScreenFromUnitRect()
{
	m_ScreenRect = m_pTouchControls->CalculateScreenFromUnitRect(m_UnitRect, m_Shape);
}

CUIRect CTouchControls::CalculateScreenFromUnitRect(CUnitRect Unit, EButtonShape Shape) const
{
	Unit = CalculateHitbox(Unit, Shape);
	const vec2 ScreenSize = CalculateScreenSize();
	CUIRect ScreenRect;
	ScreenRect.x = Unit.m_X * ScreenSize.x / BUTTON_SIZE_SCALE;
	ScreenRect.y = Unit.m_Y * ScreenSize.y / BUTTON_SIZE_SCALE;
	ScreenRect.w = Unit.m_W * ScreenSize.x / BUTTON_SIZE_SCALE;
	ScreenRect.h = Unit.m_H * ScreenSize.y / BUTTON_SIZE_SCALE;
	return ScreenRect;
}

CTouchControls::CUnitRect CTouchControls::CalculateHitbox(const CUnitRect &Rect, EButtonShape Shape) const
{
	switch(Shape)
	{
	case EButtonShape::RECT: return Rect;
	case EButtonShape::CIRCLE:
	{
		const vec2 ScreenSize = CalculateScreenSize();
		CUnitRect Hitbox = Rect;
		if(ScreenSize.x * Rect.m_W < ScreenSize.y * Rect.m_H)
		{
			Hitbox.m_Y += (ScreenSize.y * Rect.m_H - ScreenSize.x * Rect.m_W) / 2 / ScreenSize.y;
			Hitbox.m_H = ScreenSize.x * Rect.m_W / ScreenSize.y;
		}
		else if(ScreenSize.x * Rect.m_W > ScreenSize.y * Rect.m_H)
		{
			Hitbox.m_X += (ScreenSize.x * Rect.m_W - ScreenSize.y * Rect.m_H) / 2 / ScreenSize.x;
			Hitbox.m_W = ScreenSize.y * Rect.m_H / ScreenSize.x;
		}
		return Hitbox;
	}
	default: dbg_assert_failed("Unhandled shape");
	}
}

void CTouchControls::CTouchButton::UpdateBackgroundCorners()
{
	if(m_Shape != EButtonShape::RECT)
	{
		m_BackgroundCorners = IGraphics::CORNER_NONE;
		return;
	}

	// Determine rounded corners based on button layout
	m_BackgroundCorners = IGraphics::CORNER_ALL;

	if(m_UnitRect.m_X == 0)
	{
		m_BackgroundCorners &= ~IGraphics::CORNER_L;
	}
	if(m_UnitRect.m_X + m_UnitRect.m_W == BUTTON_SIZE_SCALE)
	{
		m_BackgroundCorners &= ~IGraphics::CORNER_R;
	}
	if(m_UnitRect.m_Y == 0)
	{
		m_BackgroundCorners &= ~IGraphics::CORNER_T;
	}
	if(m_UnitRect.m_Y + m_UnitRect.m_H == BUTTON_SIZE_SCALE)
	{
		m_BackgroundCorners &= ~IGraphics::CORNER_B;
	}

	const auto &&PointInOrOnRect = [](ivec2 Point, CUnitRect Rect) {
		return Point.x >= Rect.m_X && Point.x <= Rect.m_X + Rect.m_W && Point.y >= Rect.m_Y && Point.y <= Rect.m_Y + Rect.m_H;
	};
	for(const CTouchButton &OtherButton : m_pTouchControls->m_vTouchButtons)
	{
		if(&OtherButton == this || OtherButton.m_Shape != EButtonShape::RECT || !OtherButton.IsVisible())
			continue;

		if((m_BackgroundCorners & IGraphics::CORNER_TL) && PointInOrOnRect(ivec2(m_UnitRect.m_X, m_UnitRect.m_Y), OtherButton.m_UnitRect))
		{
			m_BackgroundCorners &= ~IGraphics::CORNER_TL;
		}
		if((m_BackgroundCorners & IGraphics::CORNER_TR) && PointInOrOnRect(ivec2(m_UnitRect.m_X + m_UnitRect.m_W, m_UnitRect.m_Y), OtherButton.m_UnitRect))
		{
			m_BackgroundCorners &= ~IGraphics::CORNER_TR;
		}
		if((m_BackgroundCorners & IGraphics::CORNER_BL) && PointInOrOnRect(ivec2(m_UnitRect.m_X, m_UnitRect.m_Y + m_UnitRect.m_H), OtherButton.m_UnitRect))
		{
			m_BackgroundCorners &= ~IGraphics::CORNER_BL;
		}
		if((m_BackgroundCorners & IGraphics::CORNER_BR) && PointInOrOnRect(ivec2(m_UnitRect.m_X + m_UnitRect.m_W, m_UnitRect.m_Y + m_UnitRect.m_H), OtherButton.m_UnitRect))
		{
			m_BackgroundCorners &= ~IGraphics::CORNER_BR;
		}
		if(m_BackgroundCorners == IGraphics::CORNER_NONE)
		{
			break;
		}
	}
}

vec2 CTouchControls::CTouchButton::ClampTouchPosition(vec2 TouchPosition) const
{
	switch(m_Shape)
	{
	case EButtonShape::RECT:
	{
		TouchPosition.x = std::clamp(TouchPosition.x, m_ScreenRect.x, m_ScreenRect.x + m_ScreenRect.w);
		TouchPosition.y = std::clamp(TouchPosition.y, m_ScreenRect.y, m_ScreenRect.y + m_ScreenRect.h);
		break;
	}
	case EButtonShape::CIRCLE:
	{
		const vec2 Center = m_ScreenRect.Center();
		const float MaxLength = minimum(m_ScreenRect.w, m_ScreenRect.h) / 2.0f;
		const vec2 TouchDirection = TouchPosition - Center;
		const float Length = length(TouchDirection);
		if(Length > MaxLength)
		{
			TouchPosition = normalize_pre_length(TouchDirection, Length) * MaxLength + Center;
		}
		break;
	}
	default:
		dbg_assert_failed("Unhandled shape");
	}
	return TouchPosition;
}

bool CTouchControls::CTouchButton::IsInside(vec2 TouchPosition) const
{
	switch(m_Shape)
	{
	case EButtonShape::RECT:
		return m_ScreenRect.Inside(TouchPosition);
	case EButtonShape::CIRCLE:
		return distance(TouchPosition, m_ScreenRect.Center()) <= minimum(m_ScreenRect.w, m_ScreenRect.h) / 2.0f;
	default:
		dbg_assert_failed("Unhandled shape");
		return false;
	}
}

void CTouchControls::CTouchButton::UpdateVisibilityGame()
{
	const bool PrevVisibility = m_VisibilityCached;
	m_VisibilityCached = std::all_of(m_vVisibilities.begin(), m_vVisibilities.end(), [&](CButtonVisibility Visibility) {
		return m_pTouchControls->m_aVisibilityFunctions[(int)Visibility.m_Type].m_Function() == Visibility.m_Parity;
	});
	if(m_VisibilityCached && !PrevVisibility)
	{
		m_VisibilityStartTime = time_get_nanoseconds();
	}
}

void CTouchControls::CTouchButton::UpdateVisibilityEditor()
{
	const bool PrevVisibility = m_VisibilityCached;
	m_VisibilityCached = std::all_of(m_vVisibilities.begin(), m_vVisibilities.end(), [&](CButtonVisibility Visibility) {
		return m_pTouchControls->m_aVirtualVisibilities[(int)Visibility.m_Type] == Visibility.m_Parity;
	});
	if(m_VisibilityCached && !PrevVisibility)
	{
		m_VisibilityStartTime = time_get_nanoseconds();
	}
}

bool CTouchControls::CTouchButton::IsVisible() const
{
	return m_VisibilityCached;
}

// TODO: Optimization: Use text and quad containers for rendering
void CTouchControls::CTouchButton::Render(std::optional<bool> Selected, std::optional<CUnitRect> Rect) const
{
	dbg_assert(m_pBehavior != nullptr, "Touch button behavior is nullptr");
	CUIRect ScreenRect;
	if(Rect.has_value())
		ScreenRect = m_pTouchControls->CalculateScreenFromUnitRect(*Rect, m_Shape);
	else
		ScreenRect = m_ScreenRect;

	ColorRGBA ButtonColor;
	// "Selected" can decide which color to use, while not disturbing the original color check.
	ButtonColor = m_pBehavior->IsActive() || Selected.value_or(false) ? m_pTouchControls->m_BackgroundColorActive : m_pTouchControls->m_BackgroundColorInactive;
	if(!Selected.value_or(true))
		ButtonColor = m_pTouchControls->m_BackgroundColorInactive;
	switch(m_Shape)
	{
	case EButtonShape::RECT:
	{
		ScreenRect.Draw(ButtonColor, m_pTouchControls->m_EditingActive ? IGraphics::CORNER_NONE : m_BackgroundCorners, 10.0f);
		break;
	}
	case EButtonShape::CIRCLE:
	{
		const vec2 Center = ScreenRect.Center();
		const float Radius = minimum(ScreenRect.w, ScreenRect.h) / 2.0f;
		m_pTouchControls->Graphics()->TextureClear();
		m_pTouchControls->Graphics()->QuadsBegin();
		m_pTouchControls->Graphics()->SetColor(ButtonColor);
		m_pTouchControls->Graphics()->DrawCircle(Center.x, Center.y, Radius, maximum(round_truncate(Radius / 4.0f) & ~1, 32));
		m_pTouchControls->Graphics()->QuadsEnd();
		break;
	}
	default:
		dbg_assert_failed("Unhandled shape");
	}

	const float FontSize = 22.0f;
	CButtonLabel LabelData = m_pBehavior->GetLabel();
	CUIRect LabelRect;
	ScreenRect.Margin(10.0f, &LabelRect);
	SLabelProperties LabelProps;
	LabelProps.m_MaxWidth = LabelRect.w;
	if(LabelData.m_Type == CButtonLabel::EType::ICON)
	{
		m_pTouchControls->TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		m_pTouchControls->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		m_pTouchControls->Ui()->DoLabel(&LabelRect, LabelData.m_pLabel, FontSize, TEXTALIGN_MC, LabelProps);
		m_pTouchControls->TextRender()->SetRenderFlags(0);
		m_pTouchControls->TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
	else
	{
		const char *pLabel = LabelData.m_Type == CButtonLabel::EType::LOCALIZED ? Localize(LabelData.m_pLabel) : LabelData.m_pLabel;
		m_pTouchControls->Ui()->DoLabel(&LabelRect, pLabel, FontSize, TEXTALIGN_MC, LabelProps);
	}
}

void CTouchControls::CTouchButton::WriteToConfiguration(CJsonWriter *pWriter)
{
	char aBuf[256];

	pWriter->BeginObject();

	pWriter->WriteAttribute("x");
	pWriter->WriteIntValue(m_UnitRect.m_X);
	pWriter->WriteAttribute("y");
	pWriter->WriteIntValue(m_UnitRect.m_Y);
	pWriter->WriteAttribute("w");
	pWriter->WriteIntValue(m_UnitRect.m_W);
	pWriter->WriteAttribute("h");
	pWriter->WriteIntValue(m_UnitRect.m_H);

	pWriter->WriteAttribute("shape");
	pWriter->WriteStrValue(SHAPE_NAMES[(int)m_Shape]);

	pWriter->WriteAttribute("visibilities");
	pWriter->BeginArray();
	for(CButtonVisibility Visibility : m_vVisibilities)
	{
		str_format(aBuf, sizeof(aBuf), "%s%s", Visibility.m_Parity ? "" : "-", m_pTouchControls->m_aVisibilityFunctions[(int)Visibility.m_Type].m_pId);
		pWriter->WriteStrValue(aBuf);
	}
	pWriter->EndArray();

	pWriter->WriteAttribute("behavior");
	pWriter->BeginObject();
	m_pBehavior->WriteToConfiguration(pWriter);
	pWriter->EndObject();

	pWriter->EndObject();
}

void CTouchControls::CTouchButtonBehavior::Init(CTouchButton *pTouchButton)
{
	m_pTouchButton = pTouchButton;
	m_pTouchControls = pTouchButton->m_pTouchControls;
}

void CTouchControls::CTouchButtonBehavior::Reset()
{
	m_Active = false;
}

void CTouchControls::CTouchButtonBehavior::SetActive(const IInput::CTouchFingerState &FingerState)
{
	const vec2 ScreenSize = m_pTouchControls->CalculateScreenSize();
	const CUIRect ButtonScreenRect = m_pTouchButton->m_ScreenRect;
	const vec2 Position = (m_pTouchButton->ClampTouchPosition(FingerState.m_Position * ScreenSize) - ButtonScreenRect.TopLeft()) / ButtonScreenRect.Size();
	const vec2 Delta = FingerState.m_Delta * ScreenSize / ButtonScreenRect.Size();
	if(!m_Active)
	{
		m_Active = true;
		m_ActivePosition = Position;
		m_AccumulatedDelta = Delta;
		m_ActivationStartTime = time_get_nanoseconds();
		m_Finger = FingerState.m_Finger;
		OnActivate();
	}
	else if(m_Finger == FingerState.m_Finger)
	{
		m_ActivePosition = Position;
		m_AccumulatedDelta += Delta;
		OnUpdate();
	}
	else
	{
		dbg_assert_failed("Touch button must be inactive or use same finger");
	}
}

void CTouchControls::CTouchButtonBehavior::SetInactive(bool ByFinger)
{
	if(m_Active)
	{
		m_Active = false;
		OnDeactivate(ByFinger);
	}
}

bool CTouchControls::CTouchButtonBehavior::IsActive() const
{
	return m_Active;
}

bool CTouchControls::CTouchButtonBehavior::IsActive(const IInput::CTouchFinger &Finger) const
{
	return m_Active && m_Finger == Finger;
}

void CTouchControls::CPredefinedTouchButtonBehavior::WriteToConfiguration(CJsonWriter *pWriter)
{
	pWriter->WriteAttribute("type");
	pWriter->WriteStrValue(BEHAVIOR_TYPE);

	pWriter->WriteAttribute("id");
	pWriter->WriteStrValue(m_pId);
}

// Ingame menu button: always opens ingame menu.
CTouchControls::CButtonLabel CTouchControls::CIngameMenuTouchButtonBehavior::GetLabel() const
{
	return {CButtonLabel::EType::ICON, "\xEF\x85\x8E"};
}

void CTouchControls::CIngameMenuTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(!ByFinger)
		return;
	m_pTouchControls->GameClient()->m_Menus.SetActive(true);
}

// Extra menu button:
// - Short press: show/hide additional buttons (toggle extra-menu visibilities)
// - Long press: open ingame menu
CTouchControls::CExtraMenuTouchButtonBehavior::CExtraMenuTouchButtonBehavior(int Number) :
	CPredefinedTouchButtonBehavior(BEHAVIOR_ID),
	m_Number(Number)
{
	if(m_Number == 0)
	{
		str_copy(m_aLabel, "\xEF\x83\x89");
	}
	else
	{
		str_format(m_aLabel, sizeof(m_aLabel), "\xEF\x83\x89%d", m_Number + 1);
	}
}

CTouchControls::CButtonLabel CTouchControls::CExtraMenuTouchButtonBehavior::GetLabel() const
{
	if(m_Active && time_get_nanoseconds() - m_ActivationStartTime >= LONG_TOUCH_DURATION)
	{
		return {CButtonLabel::EType::ICON, "\xEF\x95\x90"};
	}
	else
	{
		return {CButtonLabel::EType::ICON, m_aLabel};
	}
}

void CTouchControls::CExtraMenuTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(!ByFinger)
		return;
	if(time_get_nanoseconds() - m_ActivationStartTime >= LONG_TOUCH_DURATION)
	{
		m_pTouchControls->GameClient()->m_Menus.SetActive(true);
	}
	else
	{
		m_pTouchControls->m_aExtraMenuActive[m_Number] = !m_pTouchControls->m_aExtraMenuActive[m_Number];
	}
}

void CTouchControls::CExtraMenuTouchButtonBehavior::WriteToConfiguration(CJsonWriter *pWriter)
{
	CPredefinedTouchButtonBehavior::WriteToConfiguration(pWriter);

	pWriter->WriteAttribute("number");
	pWriter->WriteIntValue(m_Number + 1);
}

// Emoticon button: keeps the emoticon HUD open, next touch in emoticon HUD will close it again.
CTouchControls::CButtonLabel CTouchControls::CEmoticonTouchButtonBehavior::GetLabel() const
{
	return {CButtonLabel::EType::LOCALIZED, Localizable("Emoticon")};
}

void CTouchControls::CEmoticonTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(!ByFinger)
		return;
	m_pTouchControls->Console()->ExecuteLineStroked(1, "+emote", IConsole::CLIENT_ID_UNSPECIFIED);
}

// Spectate button: keeps the spectate menu open, next touch in spectate menu will close it again.
CTouchControls::CButtonLabel CTouchControls::CSpectateTouchButtonBehavior::GetLabel() const
{
	return {CButtonLabel::EType::LOCALIZED, Localizable("Spectator mode")};
}

void CTouchControls::CSpectateTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(!ByFinger)
		return;
	m_pTouchControls->Console()->ExecuteLineStroked(1, "+spectate", IConsole::CLIENT_ID_UNSPECIFIED);
}

// Swap action button:
// - If joystick is currently active with one action: activate the other action.
// - Else: swap active action.
CTouchControls::CButtonLabel CTouchControls::CSwapActionTouchButtonBehavior::GetLabel() const
{
	if(m_ActiveAction != NUM_ACTIONS)
	{
		return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_ActiveAction]};
	}
	else if(m_pTouchControls->m_JoystickPressCount != 0)
	{
		return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_pTouchControls->NextActiveAction(m_pTouchControls->m_ActionSelected)]};
	}
	return {CButtonLabel::EType::LOCALIZED, ACTION_SWAP_NAMES[m_pTouchControls->m_ActionSelected]};
}

void CTouchControls::CSwapActionTouchButtonBehavior::OnActivate()
{
	if(m_pTouchControls->m_JoystickPressCount != 0)
	{
		m_ActiveAction = m_pTouchControls->NextActiveAction(m_pTouchControls->m_ActionSelected);
		m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
	}
	else
	{
		m_pTouchControls->m_ActionSelected = m_pTouchControls->NextActiveAction(m_pTouchControls->m_ActionSelected);
	}
}

void CTouchControls::CSwapActionTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(m_ActiveAction != NUM_ACTIONS)
	{
		m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
		m_ActiveAction = NUM_ACTIONS;
	}
}

// Use action button: always uses the active action.
CTouchControls::CButtonLabel CTouchControls::CUseActionTouchButtonBehavior::GetLabel() const
{
	if(m_ActiveAction != NUM_ACTIONS)
	{
		return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_ActiveAction]};
	}
	return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_pTouchControls->m_ActionSelected]};
}

void CTouchControls::CUseActionTouchButtonBehavior::OnActivate()
{
	m_ActiveAction = m_pTouchControls->m_ActionSelected;
	m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
}

void CTouchControls::CUseActionTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
	m_ActiveAction = NUM_ACTIONS;
}

// Generic joystick button behavior: aim with virtual joystick and use action (defined by subclass).
CTouchControls::CButtonLabel CTouchControls::CJoystickTouchButtonBehavior::GetLabel() const
{
	if(m_ActiveAction != NUM_ACTIONS)
	{
		return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_ActiveAction]};
	}
	return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[SelectedAction()]};
}

void CTouchControls::CJoystickTouchButtonBehavior::OnActivate()
{
	m_ActiveAction = SelectedAction();
	OnUpdate();
	if(m_ActiveAction != ACTION_AIM)
	{
		m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
	}
	m_pTouchControls->m_JoystickPressCount++;
}

void CTouchControls::CJoystickTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	if(m_ActiveAction != ACTION_AIM)
	{
		m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction], IConsole::CLIENT_ID_UNSPECIFIED);
	}
	m_ActiveAction = NUM_ACTIONS;
	m_pTouchControls->m_JoystickPressCount--;
}

void CTouchControls::CJoystickTouchButtonBehavior::OnUpdate()
{
	CControls &Controls = m_pTouchControls->GameClient()->m_Controls;
	if(m_pTouchControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		vec2 WorldScreenSize;
		m_pTouchControls->Graphics()->CalcScreenParams(m_pTouchControls->Graphics()->ScreenAspect(), m_pTouchControls->GameClient()->m_Camera.m_Zoom, &WorldScreenSize.x, &WorldScreenSize.y);
		Controls.m_aMousePos[g_Config.m_ClDummy] += -m_AccumulatedDelta * WorldScreenSize;
		Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::RELATIVE;
		Controls.m_aMousePos[g_Config.m_ClDummy].x = std::clamp(Controls.m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (m_pTouchControls->Collision()->GetWidth() + 201.0f) * 32.0f);
		Controls.m_aMousePos[g_Config.m_ClDummy].y = std::clamp(Controls.m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (m_pTouchControls->Collision()->GetHeight() + 201.0f) * 32.0f);
		m_AccumulatedDelta = vec2(0.0f, 0.0f);
	}
	else
	{
		const vec2 AbsolutePosition = (m_ActivePosition - vec2(0.5f, 0.5f)) * 2.0f;
		Controls.m_aMousePos[g_Config.m_ClDummy] = AbsolutePosition * (Controls.GetMaxMouseDistance() - Controls.GetMinMouseDistance()) + normalize(AbsolutePosition) * Controls.GetMinMouseDistance();
		Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::ABSOLUTE;
		if(length(Controls.m_aMousePos[g_Config.m_ClDummy]) < 0.001f)
		{
			Controls.m_aMousePos[g_Config.m_ClDummy].x = 0.001f;
			Controls.m_aMousePos[g_Config.m_ClDummy].y = 0.0f;
		}
	}
}

// Joystick that uses the active action.
void CTouchControls::CJoystickActionTouchButtonBehavior::Init(CTouchButton *pTouchButton)
{
	CPredefinedTouchButtonBehavior::Init(pTouchButton);
}

int CTouchControls::CJoystickActionTouchButtonBehavior::SelectedAction() const
{
	return m_pTouchControls->m_ActionSelected;
}

// Joystick that only aims.
int CTouchControls::CJoystickAimTouchButtonBehavior::SelectedAction() const
{
	return ACTION_AIM;
}

// Joystick that always uses fire.
int CTouchControls::CJoystickFireTouchButtonBehavior::SelectedAction() const
{
	return ACTION_FIRE;
}

// Joystick that always uses hook.
int CTouchControls::CJoystickHookTouchButtonBehavior::SelectedAction() const
{
	return ACTION_HOOK;
}

// Bind button behavior that executes a command like a bind.
CTouchControls::CButtonLabel CTouchControls::CBindTouchButtonBehavior::GetLabel() const
{
	return {m_LabelType, m_Label.c_str()};
}

void CTouchControls::CBindTouchButtonBehavior::OnActivate()
{
	m_pTouchControls->Console()->ExecuteLineStroked(1, m_Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
	m_Repeating = false;
}

void CTouchControls::CBindTouchButtonBehavior::OnDeactivate(bool ByFinger)
{
	m_pTouchControls->Console()->ExecuteLineStroked(0, m_Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
}

void CTouchControls::CBindTouchButtonBehavior::OnUpdate()
{
	const auto Now = time_get_nanoseconds();
	if(m_Repeating)
	{
		m_AccumulatedRepeatingTime += Now - m_LastUpdateTime;
		m_LastUpdateTime = Now;
		if(m_AccumulatedRepeatingTime >= BIND_REPEAT_RATE)
		{
			m_AccumulatedRepeatingTime -= BIND_REPEAT_RATE;
			m_pTouchControls->Console()->ExecuteLineStroked(1, m_Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
		}
	}
	else if(Now - m_ActivationStartTime >= BIND_REPEAT_INITIAL_DELAY)
	{
		m_Repeating = true;
		m_LastUpdateTime = Now;
		m_AccumulatedRepeatingTime = 0ns;
	}
}

void CTouchControls::CBindTouchButtonBehavior::WriteToConfiguration(CJsonWriter *pWriter)
{
	pWriter->WriteAttribute("type");
	pWriter->WriteStrValue(BEHAVIOR_TYPE);

	pWriter->WriteAttribute("label");
	pWriter->WriteStrValue(m_Label.c_str());

	pWriter->WriteAttribute("label-type");
	pWriter->WriteStrValue(LABEL_TYPE_NAMES[(int)m_LabelType]);

	pWriter->WriteAttribute("command");
	pWriter->WriteStrValue(m_Command.c_str());
}

// Bind button behavior that switches between executing one of two or more console commands.
CTouchControls::CButtonLabel CTouchControls::CBindToggleTouchButtonBehavior::GetLabel() const
{
	const auto &ActiveCommand = m_vCommands[m_ActiveCommandIndex];
	return {ActiveCommand.m_LabelType, ActiveCommand.m_Label.c_str()};
}

void CTouchControls::CBindToggleTouchButtonBehavior::OnActivate()
{
	m_pTouchControls->Console()->ExecuteLine(m_vCommands[m_ActiveCommandIndex].m_Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
	m_ActiveCommandIndex = (m_ActiveCommandIndex + 1) % m_vCommands.size();
}

void CTouchControls::CBindToggleTouchButtonBehavior::WriteToConfiguration(CJsonWriter *pWriter)
{
	pWriter->WriteAttribute("type");
	pWriter->WriteStrValue(BEHAVIOR_TYPE);

	pWriter->WriteAttribute("commands");
	pWriter->BeginArray();

	for(const auto &Command : m_vCommands)
	{
		pWriter->BeginObject();

		pWriter->WriteAttribute("label");
		pWriter->WriteStrValue(Command.m_Label.c_str());

		pWriter->WriteAttribute("label-type");
		pWriter->WriteStrValue(LABEL_TYPE_NAMES[(int)Command.m_LabelType]);

		pWriter->WriteAttribute("command");
		pWriter->WriteStrValue(Command.m_Command.c_str());

		pWriter->EndObject();
	}

	pWriter->EndArray();
}

void CTouchControls::OnInit()
{
	InitVisibilityFunctions();
	if(!LoadConfigurationFromFile(IStorage::TYPE_ALL))
	{
		Client()->AddWarning(SWarning(Localize("Error loading touch controls"), Localize("Could not load touch controls from file. See local console for details.")));
	}
}

void CTouchControls::OnReset()
{
	ResetButtons();
	m_EditingActive = false;
}

void CTouchControls::OnWindowResize()
{
	ResetButtons();
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateScreenFromUnitRect();
	}
}

bool CTouchControls::OnTouchState(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	if(!g_Config.m_ClTouchControls)
		return false;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return false;
	if(GameClient()->m_Chat.IsActive() ||
		GameClient()->m_GameConsole.IsActive() ||
		GameClient()->m_Menus.IsActive() ||
		GameClient()->m_Emoticon.IsActive() ||
		GameClient()->m_Spectator.IsActive() ||
		m_PreviewAllButtons)
	{
		ResetButtons();
		return false;
	}

	if(m_EditingActive)
		UpdateButtonsEditor(vTouchFingerStates);
	else
		UpdateButtonsGame(vTouchFingerStates);
	return true;
}

void CTouchControls::OnRender()
{
	if(!g_Config.m_ClTouchControls)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(GameClient()->m_Chat.IsActive() ||
		GameClient()->m_Emoticon.IsActive() ||
		GameClient()->m_Spectator.IsActive())
	{
		return;
	}

	const vec2 ScreenSize = CalculateScreenSize();
	Graphics()->MapScreen(0.0f, 0.0f, ScreenSize.x, ScreenSize.y);

	if(m_EditingActive)
	{
		RenderButtonsEditor();
		return;
	}
	// If not editing, deselect it.
	m_pSelectedButton = nullptr;
	m_pSampleButton = nullptr;
	m_UnsavedChanges = false;
	RenderButtonsGame();
}

bool CTouchControls::LoadConfigurationFromFile(int StorageType)
{
	void *pFileData;
	unsigned FileLength;
	if(!Storage()->ReadFile(CONFIGURATION_FILENAME, StorageType, &pFileData, &FileLength))
	{
		log_error("touch_controls", "Failed to read configuration from '%s'", CONFIGURATION_FILENAME);
		return false;
	}

	const bool Result = ParseConfiguration(pFileData, FileLength);
	free(pFileData);
	return Result;
}

bool CTouchControls::LoadConfigurationFromClipboard()
{
	std::string Clipboard = Input()->GetClipboardText();
	return ParseConfiguration(Clipboard.c_str(), Clipboard.size());
}

bool CTouchControls::SaveConfigurationToFile()
{
	IOHANDLE File = Storage()->OpenFile(CONFIGURATION_FILENAME, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("touch_controls", "Failed to open '%s' for writing configuration", CONFIGURATION_FILENAME);
		return false;
	}

	CJsonFileWriter Writer(File);
	WriteConfiguration(&Writer);
	return true;
}

void CTouchControls::SaveConfigurationToClipboard()
{
	CJsonStringWriter Writer;
	WriteConfiguration(&Writer);
	std::string ConfigurationString = Writer.GetOutputString();
	Input()->SetClipboardText(ConfigurationString.c_str());
}

void CTouchControls::InitVisibilityFunctions()
{
	m_aVisibilityFunctions[(int)EButtonVisibility::INGAME].m_pId = "ingame";
	m_aVisibilityFunctions[(int)EButtonVisibility::INGAME].m_Function = [&]() {
		return !GameClient()->m_Snap.m_SpecInfo.m_Active;
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::ZOOM_ALLOWED].m_pId = "zoom-allowed";
	m_aVisibilityFunctions[(int)EButtonVisibility::ZOOM_ALLOWED].m_Function = [&]() {
		return GameClient()->m_Camera.ZoomAllowed();
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::VOTE_ACTIVE].m_pId = "vote-active";
	m_aVisibilityFunctions[(int)EButtonVisibility::VOTE_ACTIVE].m_Function = [&]() {
		return GameClient()->m_Voting.IsVoting();
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::DUMMY_ALLOWED].m_pId = "dummy-allowed";
	m_aVisibilityFunctions[(int)EButtonVisibility::DUMMY_ALLOWED].m_Function = [&]() {
		return Client()->DummyAllowed();
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::DUMMY_CONNECTED].m_pId = "dummy-connected";
	m_aVisibilityFunctions[(int)EButtonVisibility::DUMMY_CONNECTED].m_Function = [&]() {
		return Client()->DummyConnected();
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::RCON_AUTHED].m_pId = "rcon-authed";
	m_aVisibilityFunctions[(int)EButtonVisibility::RCON_AUTHED].m_Function = [&]() {
		return Client()->RconAuthed();
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::DEMO_PLAYER].m_pId = "demo-player";
	m_aVisibilityFunctions[(int)EButtonVisibility::DEMO_PLAYER].m_Function = [&]() {
		return Client()->State() == IClient::STATE_DEMOPLAYBACK;
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_1].m_pId = "extra-menu";
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_1].m_Function = [&]() {
		return m_aExtraMenuActive[0];
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_2].m_pId = "extra-menu-2";
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_2].m_Function = [&]() {
		return m_aExtraMenuActive[1];
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_3].m_pId = "extra-menu-3";
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_3].m_Function = [&]() {
		return m_aExtraMenuActive[2];
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_4].m_pId = "extra-menu-4";
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_4].m_Function = [&]() {
		return m_aExtraMenuActive[3];
	};
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_5].m_pId = "extra-menu-5";
	m_aVisibilityFunctions[(int)EButtonVisibility::EXTRA_MENU_5].m_Function = [&]() {
		return m_aExtraMenuActive[4];
	};
}

int CTouchControls::NextActiveAction(int Action) const
{
	switch(Action)
	{
	case ACTION_FIRE:
		return ACTION_HOOK;
	case ACTION_HOOK:
		return ACTION_FIRE;
	default:
		dbg_assert_failed("Action invalid for NextActiveAction");
	}
}

int CTouchControls::NextDirectTouchAction() const
{
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		switch(m_DirectTouchSpectate)
		{
		case EDirectTouchSpectateMode::DISABLED:
			return NUM_ACTIONS;
		case EDirectTouchSpectateMode::AIM:
			return ACTION_AIM;
		default:
			dbg_assert_failed("m_DirectTouchSpectate invalid");
		}
	}
	else
	{
		switch(m_DirectTouchIngame)
		{
		case EDirectTouchIngameMode::DISABLED:
			return NUM_ACTIONS;
		case EDirectTouchIngameMode::ACTION:
			return m_ActionSelected;
		case EDirectTouchIngameMode::AIM:
			return ACTION_AIM;
		case EDirectTouchIngameMode::FIRE:
			return ACTION_FIRE;
		case EDirectTouchIngameMode::HOOK:
			return ACTION_HOOK;
		default:
			dbg_assert_failed("m_DirectTouchIngame invalid");
		}
	}
}

void CTouchControls::UpdateButtonsGame(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	// Update cached button visibilities and store time that buttons become visible.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibilityGame();
	}

	const int DirectTouchAction = NextDirectTouchAction();
	const vec2 ScreenSize = CalculateScreenSize();

	std::vector<IInput::CTouchFingerState> vRemainingTouchFingerStates = vTouchFingerStates;

	if(!m_vStaleFingers.empty())
	{
		// Remove stale fingers that are not pressed anymore.
		m_vStaleFingers.erase(
			std::remove_if(m_vStaleFingers.begin(), m_vStaleFingers.end(), [&](const IInput::CTouchFinger &Finger) {
				return std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
					return TouchFingerState.m_Finger == Finger;
				}) == vRemainingTouchFingerStates.end();
			}),
			m_vStaleFingers.end());
		// Prevent stale fingers from activating touch buttons and direct touch.
		vRemainingTouchFingerStates.erase(
			std::remove_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
				return std::find_if(m_vStaleFingers.begin(), m_vStaleFingers.end(), [&](const IInput::CTouchFinger &Finger) {
					return TouchFingerState.m_Finger == Finger;
				}) != m_vStaleFingers.end();
			}),
			vRemainingTouchFingerStates.end());
	}

	// Remove remaining finger states for fingers which are responsible for active actions
	// and release action when the finger responsible for it is not pressed down anymore.
	bool GotDirectFingerState = false; // Whether DirectFingerState is valid
	IInput::CTouchFingerState DirectFingerState{}; // The finger that will be used to update the mouse position
	for(int Action = ACTION_AIM; Action < NUM_ACTIONS; ++Action)
	{
		if(!m_aDirectTouchActionStates[Action].m_Active)
		{
			continue;
		}

		const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchFingerState.m_Finger == m_aDirectTouchActionStates[Action].m_Finger;
		});
		if(ActiveFinger == vRemainingTouchFingerStates.end() || DirectTouchAction == NUM_ACTIONS)
		{
			m_aDirectTouchActionStates[Action].m_Active = false;
			if(Action != ACTION_AIM)
			{
				Console()->ExecuteLineStroked(0, ACTION_COMMANDS[Action], IConsole::CLIENT_ID_UNSPECIFIED);
			}
		}
		else
		{
			if(Action == m_DirectTouchLastAction)
			{
				GotDirectFingerState = true;
				DirectFingerState = *ActiveFinger;
			}
			vRemainingTouchFingerStates.erase(ActiveFinger);
		}
	}

	// Update touch button states after the active action fingers were removed from the vector
	// so that current cursor movement can cross over touch buttons without activating them.

	// Activate visible, inactive buttons with hovered finger. Deactivate previous button being
	// activated by the same finger. Touch buttons are only activated if they became visible
	// before the respective touch finger was pressed down, to prevent repeatedly activating
	// overlapping buttons of excluding visibilities.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.IsVisible() || TouchButton.m_pBehavior->IsActive())
		{
			continue;
		}
		const auto FingerInsideButton = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchButton.m_VisibilityStartTime < TouchFingerState.m_PressTime &&
			       TouchButton.IsInside(TouchFingerState.m_Position * ScreenSize);
		});
		if(FingerInsideButton == vRemainingTouchFingerStates.end())
		{
			continue;
		}
		const auto OtherHoveredTouchButton = std::find_if(m_vTouchButtons.begin(), m_vTouchButtons.end(), [&](const CTouchButton &Button) {
			return &Button != &TouchButton && Button.IsVisible() && Button.IsInside(FingerInsideButton->m_Position * ScreenSize);
		});
		if(OtherHoveredTouchButton != m_vTouchButtons.end())
		{
			// Do not activate any button if multiple overlapping buttons are hovered.
			// TODO: Prevent overlapping buttons entirely when parsing the button configuration?
			vRemainingTouchFingerStates.erase(FingerInsideButton);
			continue;
		}
		auto PrevActiveTouchButton = std::find_if(m_vTouchButtons.begin(), m_vTouchButtons.end(), [&](const CTouchButton &Button) {
			return Button.m_pBehavior->IsActive(FingerInsideButton->m_Finger);
		});
		if(PrevActiveTouchButton != m_vTouchButtons.end())
		{
			PrevActiveTouchButton->m_pBehavior->SetInactive(true);
		}
		TouchButton.m_pBehavior->SetActive(*FingerInsideButton);
	}

	// Deactivate touch buttons only when the respective finger is released, so touch buttons
	// are kept active also if the finger is moved outside the button.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.IsVisible())
		{
			if(TouchButton.m_pBehavior->IsActive())
			{
				// Remember fingers responsible for buttons that were deactivated due to becoming invisible,
				// to ensure that these fingers will not activate direct touch input or touch buttons.
				m_vStaleFingers.push_back(TouchButton.m_pBehavior->m_Finger);
				const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
					return TouchFingerState.m_Finger == TouchButton.m_pBehavior->m_Finger;
				});
				// ActiveFinger could be released during this progress.
				if(ActiveFinger != vRemainingTouchFingerStates.end())
					vRemainingTouchFingerStates.erase(ActiveFinger);
			}
			TouchButton.m_pBehavior->SetInactive(false);
			continue;
		}
		if(!TouchButton.m_pBehavior->IsActive())
		{
			continue;
		}
		const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchFingerState.m_Finger == TouchButton.m_pBehavior->m_Finger;
		});
		if(ActiveFinger == vRemainingTouchFingerStates.end())
		{
			TouchButton.m_pBehavior->SetInactive(true);
		}
		else
		{
			// Update the already active touch button with the current finger state
			TouchButton.m_pBehavior->SetActive(*ActiveFinger);
		}
	}

	// Remove remaining fingers for active buttons after updating the buttons.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.m_pBehavior->IsActive())
		{
			continue;
		}
		const auto ActiveFinger = std::find_if(vRemainingTouchFingerStates.begin(), vRemainingTouchFingerStates.end(), [&](const IInput::CTouchFingerState &TouchFingerState) {
			return TouchFingerState.m_Finger == TouchButton.m_pBehavior->m_Finger;
		});
		dbg_assert(ActiveFinger != vRemainingTouchFingerStates.end(), "Active button finger not found");
		vRemainingTouchFingerStates.erase(ActiveFinger);
	}

	// TODO: Support standard gesture to zoom (enabled separately for ingame and spectator)

	// Activate action if there is an unhandled pressed down finger.
	int ActivateAction = NUM_ACTIONS;
	if(DirectTouchAction != NUM_ACTIONS && !vRemainingTouchFingerStates.empty() && !m_aDirectTouchActionStates[DirectTouchAction].m_Active)
	{
		GotDirectFingerState = true;
		DirectFingerState = vRemainingTouchFingerStates[0];
		vRemainingTouchFingerStates.erase(vRemainingTouchFingerStates.begin());
		m_aDirectTouchActionStates[DirectTouchAction].m_Active = true;
		m_aDirectTouchActionStates[DirectTouchAction].m_Finger = DirectFingerState.m_Finger;
		m_DirectTouchLastAction = DirectTouchAction;
		ActivateAction = DirectTouchAction;
	}

	// Update mouse position based on the finger responsible for the last active action.
	if(GotDirectFingerState)
	{
		const float Zoom = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Camera.m_Zoom : 1.0f;
		vec2 WorldScreenSize;
		Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), Zoom, &WorldScreenSize.x, &WorldScreenSize.y);
		CControls &Controls = GameClient()->m_Controls;
		if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			Controls.m_aMousePos[g_Config.m_ClDummy] += -DirectFingerState.m_Delta * WorldScreenSize;
			Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::RELATIVE;
			Controls.m_aMousePos[g_Config.m_ClDummy].x = std::clamp(Controls.m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (Collision()->GetWidth() + 201.0f) * 32.0f);
			Controls.m_aMousePos[g_Config.m_ClDummy].y = std::clamp(Controls.m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (Collision()->GetHeight() + 201.0f) * 32.0f);
		}
		else
		{
			Controls.m_aMousePos[g_Config.m_ClDummy] = (DirectFingerState.m_Position - vec2(0.5f, 0.5f)) * WorldScreenSize;
			Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::ABSOLUTE;
		}
	}

	// Activate action after the mouse position is set.
	if(ActivateAction != ACTION_AIM && ActivateAction != NUM_ACTIONS)
	{
		Console()->ExecuteLineStroked(1, ACTION_COMMANDS[ActivateAction], IConsole::CLIENT_ID_UNSPECIFIED);
	}
}

void CTouchControls::ResetButtons()
{
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.m_pBehavior->Reset();
	}
	for(CActionState &ActionState : m_aDirectTouchActionStates)
	{
		ActionState.m_Active = false;
	}
}

void CTouchControls::RenderButtonsGame()
{
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibilityGame();
	}
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.IsVisible())
		{
			continue;
		}
		TouchButton.UpdateBackgroundCorners();
		TouchButton.UpdateScreenFromUnitRect();
		TouchButton.Render();
	}
}

vec2 CTouchControls::CalculateScreenSize() const
{
	const float ScreenHeight = 400.0f * 3.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	return vec2(ScreenWidth, ScreenHeight);
}

bool CTouchControls::ParseConfiguration(const void *pFileData, unsigned FileLength)
{
	json_settings JsonSettings{};
	char aError[256];
	json_value *pConfiguration = json_parse_ex(&JsonSettings, static_cast<const json_char *>(pFileData), FileLength, aError);

	if(pConfiguration == nullptr)
	{
		log_error("touch_controls", "Failed to parse configuration (invalid json): '%s'", aError);
		return false;
	}
	if(pConfiguration->type != json_object)
	{
		log_error("touch_controls", "Failed to parse configuration: root must be an object");
		json_value_free(pConfiguration);
		return false;
	}

	std::optional<EDirectTouchIngameMode> ParsedDirectTouchIngame = ParseDirectTouchIngameMode(&(*pConfiguration)["direct-touch-ingame"]);
	if(!ParsedDirectTouchIngame.has_value())
	{
		json_value_free(pConfiguration);
		return false;
	}

	std::optional<EDirectTouchSpectateMode> ParsedDirectTouchSpectate = ParseDirectTouchSpectateMode(&(*pConfiguration)["direct-touch-spectate"]);
	if(!ParsedDirectTouchSpectate.has_value())
	{
		json_value_free(pConfiguration);
		return false;
	}

	std::optional<ColorRGBA> ParsedBackgroundColorInactive =
		ParseColor(&(*pConfiguration)["background-color-inactive"], "background-color-inactive", ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f));
	if(!ParsedBackgroundColorInactive.has_value())
	{
		json_value_free(pConfiguration);
		return false;
	}

	std::optional<ColorRGBA> ParsedBackgroundColorActive =
		ParseColor(&(*pConfiguration)["background-color-active"], "background-color-active", ColorRGBA(0.2f, 0.2f, 0.2f, 0.25f));
	if(!ParsedBackgroundColorActive.has_value())
	{
		json_value_free(pConfiguration);
		return false;
	}

	const json_value &TouchButtons = (*pConfiguration)["touch-buttons"];
	if(TouchButtons.type != json_array)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute 'touch-buttons' must specify an array");
		json_value_free(pConfiguration);
		return false;
	}

	std::vector<CTouchButton> vParsedTouchButtons;
	vParsedTouchButtons.reserve(TouchButtons.u.array.length);
	for(unsigned ButtonIndex = 0; ButtonIndex < TouchButtons.u.array.length; ++ButtonIndex)
	{
		std::optional<CTouchButton> ParsedButton = ParseButton(&TouchButtons[ButtonIndex]);
		if(!ParsedButton.has_value())
		{
			log_error("touch_controls", "Failed to parse configuration: could not parse button at index '%d'", ButtonIndex);
			json_value_free(pConfiguration);
			return false;
		}

		vParsedTouchButtons.push_back(std::move(ParsedButton.value()));
	}

	// Parsing successful. Apply parsed configuration.
	m_DirectTouchIngame = ParsedDirectTouchIngame.value();
	m_DirectTouchSpectate = ParsedDirectTouchSpectate.value();
	m_BackgroundColorInactive = ParsedBackgroundColorInactive.value();
	m_BackgroundColorActive = ParsedBackgroundColorActive.value();

	m_vTouchButtons = std::move(vParsedTouchButtons);
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdatePointers();
		TouchButton.UpdateScreenFromUnitRect();
	}

	json_value_free(pConfiguration);

	// If successfully parsing buttons, deselect it.
	m_pSelectedButton = nullptr;
	m_pSampleButton = nullptr;
	m_UnsavedChanges = false;

	return true;
}

std::optional<CTouchControls::EDirectTouchIngameMode> CTouchControls::ParseDirectTouchIngameMode(const json_value *pModeValue)
{
	// TODO: Remove json_boolean backwards compatibility
	const json_value &DirectTouchIngame = *pModeValue;
	if(DirectTouchIngame.type != json_boolean && DirectTouchIngame.type != json_string)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute 'direct-touch-ingame' must specify a string");
		return {};
	}
	if(DirectTouchIngame.type == json_boolean)
	{
		return DirectTouchIngame.u.boolean ? EDirectTouchIngameMode::ACTION : EDirectTouchIngameMode::DISABLED;
	}
	EDirectTouchIngameMode ParsedDirectTouchIngame = EDirectTouchIngameMode::NUM_STATES;
	for(int CurrentMode = (int)EDirectTouchIngameMode::DISABLED; CurrentMode < (int)EDirectTouchIngameMode::NUM_STATES; ++CurrentMode)
	{
		if(str_comp(DirectTouchIngame.u.string.ptr, DIRECT_TOUCH_INGAME_MODE_NAMES[CurrentMode]) == 0)
		{
			ParsedDirectTouchIngame = (EDirectTouchIngameMode)CurrentMode;
			break;
		}
	}
	if(ParsedDirectTouchIngame == EDirectTouchIngameMode::NUM_STATES)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute 'direct-touch-ingame' specifies unknown value '%s'", DirectTouchIngame.u.string.ptr);
		return {};
	}
	return ParsedDirectTouchIngame;
}

std::optional<CTouchControls::EDirectTouchSpectateMode> CTouchControls::ParseDirectTouchSpectateMode(const json_value *pModeValue)
{
	// TODO: Remove json_boolean backwards compatibility
	const json_value &DirectTouchSpectate = *pModeValue;
	if(DirectTouchSpectate.type != json_boolean && DirectTouchSpectate.type != json_string)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute 'direct-touch-spectate' must specify a string");
		return {};
	}
	if(DirectTouchSpectate.type == json_boolean)
	{
		return DirectTouchSpectate.u.boolean ? EDirectTouchSpectateMode::AIM : EDirectTouchSpectateMode::DISABLED;
	}
	EDirectTouchSpectateMode ParsedDirectTouchSpectate = EDirectTouchSpectateMode::NUM_STATES;
	for(int CurrentMode = (int)EDirectTouchSpectateMode::DISABLED; CurrentMode < (int)EDirectTouchSpectateMode::NUM_STATES; ++CurrentMode)
	{
		if(str_comp(DirectTouchSpectate.u.string.ptr, DIRECT_TOUCH_SPECTATE_MODE_NAMES[CurrentMode]) == 0)
		{
			ParsedDirectTouchSpectate = (EDirectTouchSpectateMode)CurrentMode;
			break;
		}
	}
	if(ParsedDirectTouchSpectate == EDirectTouchSpectateMode::NUM_STATES)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute 'direct-touch-spectate' specifies unknown value '%s'", DirectTouchSpectate.u.string.ptr);
		return {};
	}
	return ParsedDirectTouchSpectate;
}

std::optional<ColorRGBA> CTouchControls::ParseColor(const json_value *pColorValue, const char *pAttributeName, std::optional<ColorRGBA> DefaultColor) const
{
	const json_value &Color = *pColorValue;
	if(Color.type == json_none && DefaultColor.has_value())
	{
		return DefaultColor;
	}
	if(Color.type != json_string)
	{
		log_error("touch_controls", "Failed to parse configuration: attribute '%s' must specify a string", pAttributeName);
		return {};
	}
	std::optional<ColorRGBA> ParsedColor = color_parse<ColorRGBA>(Color.u.string.ptr);
	if(!ParsedColor.has_value())
	{
		log_error("touch_controls", "Failed to parse configuration: attribute '%s' specifies invalid color value '%s'", pAttributeName, Color.u.string.ptr);
		return {};
	}
	return ParsedColor;
}

std::optional<CTouchControls::CTouchButton> CTouchControls::ParseButton(const json_value *pButtonObject)
{
	const json_value &ButtonObject = *pButtonObject;
	if(ButtonObject.type != json_object)
	{
		log_error("touch_controls", "Failed to parse touch button: must be an object");
		return {};
	}

	const auto &&ParsePositionSize = [&](const char *pAttribute, int &ParsedValue, int Min, int Max) {
		const json_value &AttributeValue = ButtonObject[pAttribute];
		if(AttributeValue.type != json_integer || !in_range<json_int_t>(AttributeValue.u.integer, Min, Max))
		{
			log_error("touch_controls", "Failed to parse touch button: attribute '%s' must specify an integer between '%d' and '%d'", pAttribute, Min, Max);
			return false;
		}
		ParsedValue = AttributeValue.u.integer;
		return true;
	};
	CUnitRect ParsedUnitRect;
	if(!ParsePositionSize("w", ParsedUnitRect.m_W, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MAXIMUM) ||
		!ParsePositionSize("h", ParsedUnitRect.m_H, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MAXIMUM))
	{
		return {};
	}
	if(!ParsePositionSize("x", ParsedUnitRect.m_X, 0, BUTTON_SIZE_SCALE - ParsedUnitRect.m_W) ||
		!ParsePositionSize("y", ParsedUnitRect.m_Y, 0, BUTTON_SIZE_SCALE - ParsedUnitRect.m_H))
	{
		return {};
	}

	const json_value &Shape = ButtonObject["shape"];
	if(Shape.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button: attribute 'shape' must specify a string");
		return {};
	}
	EButtonShape ParsedShape = EButtonShape::NUM_SHAPES;
	for(int CurrentShape = (int)EButtonShape::RECT; CurrentShape < (int)EButtonShape::NUM_SHAPES; ++CurrentShape)
	{
		if(str_comp(Shape.u.string.ptr, SHAPE_NAMES[CurrentShape]) == 0)
		{
			ParsedShape = (EButtonShape)CurrentShape;
			break;
		}
	}
	if(ParsedShape == EButtonShape::NUM_SHAPES)
	{
		log_error("touch_controls", "Failed to parse touch button: attribute 'shape' specifies unknown value '%s'", Shape.u.string.ptr);
		return {};
	}

	const json_value &Visibilities = ButtonObject["visibilities"];
	if(Visibilities.type != json_array)
	{
		log_error("touch_controls", "Failed to parse touch button: attribute 'visibilities' must specify an array");
		return {};
	}
	std::vector<CButtonVisibility> vParsedVisibilities;
	vParsedVisibilities.reserve(Visibilities.u.array.length);
	for(unsigned VisibilityIndex = 0; VisibilityIndex < Visibilities.u.array.length; ++VisibilityIndex)
	{
		const json_value &Visibility = Visibilities[VisibilityIndex];
		if(Visibility.type != json_string)
		{
			log_error("touch_controls", "Failed to parse touch button: attribute 'visibilities' does not specify string at index '%d'", VisibilityIndex);
			return {};
		}
		EButtonVisibility ParsedVisibility = EButtonVisibility::NUM_VISIBILITIES;
		const bool ParsedParity = Visibility.u.string.ptr[0] != '-';
		const char *pVisibilityString = ParsedParity ? Visibility.u.string.ptr : &Visibility.u.string.ptr[1];
		for(int CurrentVisibility = (int)EButtonVisibility::INGAME; CurrentVisibility < (int)EButtonVisibility::NUM_VISIBILITIES; ++CurrentVisibility)
		{
			if(str_comp(pVisibilityString, m_aVisibilityFunctions[CurrentVisibility].m_pId) == 0)
			{
				ParsedVisibility = (EButtonVisibility)CurrentVisibility;
				break;
			}
		}
		if(ParsedVisibility == EButtonVisibility::NUM_VISIBILITIES)
		{
			log_error("touch_controls", "Failed to parse touch button: attribute 'visibilities' specifies unknown value '%s' at index '%d'", pVisibilityString, VisibilityIndex);
			return {};
		}
		const bool VisibilityAlreadyUsed = std::any_of(vParsedVisibilities.begin(), vParsedVisibilities.end(), [&](CButtonVisibility OtherParsedVisibility) {
			return OtherParsedVisibility.m_Type == ParsedVisibility;
		});
		if(VisibilityAlreadyUsed)
		{
			log_error("touch_controls", "Failed to parse touch button: attribute 'visibilities' specifies duplicate value '%s' at '%d'", pVisibilityString, VisibilityIndex);
			return {};
		}
		vParsedVisibilities.emplace_back(ParsedVisibility, ParsedParity);
	}

	std::unique_ptr<CTouchButtonBehavior> pParsedBehavior = ParseBehavior(&ButtonObject["behavior"]);
	if(pParsedBehavior == nullptr)
	{
		log_error("touch_controls", "Failed to parse touch button: failed to parse attribute 'behavior' (see details above)");
		return {};
	}

	CTouchButton Button(this);
	Button.m_UnitRect = ParsedUnitRect;
	Button.m_Shape = ParsedShape;
	Button.m_vVisibilities = std::move(vParsedVisibilities);
	Button.m_pBehavior = std::move(pParsedBehavior);
	return Button;
}

std::unique_ptr<CTouchControls::CTouchButtonBehavior> CTouchControls::ParseBehavior(const json_value *pBehaviorObject)
{
	const json_value &BehaviorObject = *pBehaviorObject;
	if(BehaviorObject.type != json_object)
	{
		log_error("touch_controls", "Failed to parse touch button behavior: must be an object");
		return nullptr;
	}

	const json_value &BehaviorType = BehaviorObject["type"];
	if(BehaviorType.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button behavior: attribute 'type' must specify a string");
		return nullptr;
	}

	if(str_comp(BehaviorType.u.string.ptr, CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		return ParsePredefinedBehavior(&BehaviorObject);
	}
	else if(str_comp(BehaviorType.u.string.ptr, CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		return ParseBindBehavior(&BehaviorObject);
	}
	else if(str_comp(BehaviorType.u.string.ptr, CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
	{
		return ParseBindToggleBehavior(&BehaviorObject);
	}
	else
	{
		log_error("touch_controls", "Failed to parse touch button behavior: attribute 'type' specifies unknown value '%s'", BehaviorType.u.string.ptr);
		return nullptr;
	}
}

std::unique_ptr<CTouchControls::CPredefinedTouchButtonBehavior> CTouchControls::ParsePredefinedBehavior(const json_value *pBehaviorObject)
{
	const json_value &BehaviorObject = *pBehaviorObject;
	const json_value &PredefinedId = BehaviorObject["id"];
	if(PredefinedId.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'id' must specify a string", CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE);
		return nullptr;
	}

	class CBehaviorFactory
	{
	public:
		const char *m_pId;
		std::function<std::unique_ptr<CPredefinedTouchButtonBehavior>(const json_value *pBehaviorObject)> m_Factory;
	};
	static const CBehaviorFactory BEHAVIOR_FACTORIES[] = {
		{CIngameMenuTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CIngameMenuTouchButtonBehavior>(); }},
		{CExtraMenuTouchButtonBehavior::BEHAVIOR_ID, [&](const json_value *pBehavior) { return ParseExtraMenuBehavior(pBehavior); }},
		{CEmoticonTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CEmoticonTouchButtonBehavior>(); }},
		{CSpectateTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CSpectateTouchButtonBehavior>(); }},
		{CSwapActionTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CSwapActionTouchButtonBehavior>(); }},
		{CUseActionTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CUseActionTouchButtonBehavior>(); }},
		{CJoystickActionTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CJoystickActionTouchButtonBehavior>(); }},
		{CJoystickAimTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CJoystickAimTouchButtonBehavior>(); }},
		{CJoystickFireTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CJoystickFireTouchButtonBehavior>(); }},
		{CJoystickHookTouchButtonBehavior::BEHAVIOR_ID, [](const json_value *pBehavior) { return std::make_unique<CJoystickHookTouchButtonBehavior>(); }}};
	for(const CBehaviorFactory &BehaviorFactory : BEHAVIOR_FACTORIES)
	{
		if(str_comp(PredefinedId.u.string.ptr, BehaviorFactory.m_pId) == 0)
		{
			return BehaviorFactory.m_Factory(&BehaviorObject);
		}
	}

	log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'id' specifies unknown value '%s'", CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE, PredefinedId.u.string.ptr);
	return nullptr;
}

std::unique_ptr<CTouchControls::CExtraMenuTouchButtonBehavior> CTouchControls::ParseExtraMenuBehavior(const json_value *pBehaviorObject)
{
	const json_value &BehaviorObject = *pBehaviorObject;
	const json_value &MenuNumber = BehaviorObject["number"];
	// TODO: Remove json_none backwards compatibility
	if(MenuNumber.type != json_none && (MenuNumber.type != json_integer || !in_range<json_int_t>(MenuNumber.u.integer, 1, MAX_EXTRA_MENU_NUMBER)))
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s' and ID '%s': attribute 'number' must specify an integer between '%d' and '%d'",
			CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE, CExtraMenuTouchButtonBehavior::BEHAVIOR_ID, 1, MAX_EXTRA_MENU_NUMBER);
		return nullptr;
	}
	int ParsedMenuNumber = MenuNumber.type == json_none ? 0 : (MenuNumber.u.integer - 1);

	return std::make_unique<CExtraMenuTouchButtonBehavior>(ParsedMenuNumber);
}

std::unique_ptr<CTouchControls::CBindTouchButtonBehavior> CTouchControls::ParseBindBehavior(const json_value *pBehaviorObject)
{
	const json_value &BehaviorObject = *pBehaviorObject;
	const json_value &Label = BehaviorObject["label"];
	if(Label.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'label' must specify a string", CBindTouchButtonBehavior::BEHAVIOR_TYPE);
		return nullptr;
	}

	const json_value &LabelType = BehaviorObject["label-type"];
	if(LabelType.type != json_string && LabelType.type != json_none)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'label-type' must specify a string", CBindTouchButtonBehavior::BEHAVIOR_TYPE);
		return {};
	}
	CButtonLabel::EType ParsedLabelType = CButtonLabel::EType::NUM_TYPES;
	if(LabelType.type == json_none)
	{
		ParsedLabelType = CButtonLabel::EType::PLAIN;
	}
	else
	{
		for(int CurrentType = (int)CButtonLabel::EType::PLAIN; CurrentType < (int)CButtonLabel::EType::NUM_TYPES; ++CurrentType)
		{
			if(str_comp(LabelType.u.string.ptr, LABEL_TYPE_NAMES[CurrentType]) == 0)
			{
				ParsedLabelType = (CButtonLabel::EType)CurrentType;
				break;
			}
		}
	}
	if(ParsedLabelType == CButtonLabel::EType::NUM_TYPES)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'label-type' specifies unknown value '%s'", CBindTouchButtonBehavior::BEHAVIOR_TYPE, LabelType.u.string.ptr);
		return {};
	}

	const json_value &Command = BehaviorObject["command"];
	if(Command.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'command' must specify a string", CBindTouchButtonBehavior::BEHAVIOR_TYPE);
		return nullptr;
	}

	return std::make_unique<CBindTouchButtonBehavior>(Label.u.string.ptr, ParsedLabelType, Command.u.string.ptr);
}

std::unique_ptr<CTouchControls::CBindToggleTouchButtonBehavior> CTouchControls::ParseBindToggleBehavior(const json_value *pBehaviorObject)
{
	const json_value &CommandsObject = (*pBehaviorObject)["commands"];
	if(CommandsObject.type != json_array || CommandsObject.u.array.length < 2)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'commands' must specify an array with at least 2 entries", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE);
		return {};
	}

	std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vCommands;
	vCommands.reserve(CommandsObject.u.array.length);
	for(unsigned CommandIndex = 0; CommandIndex < CommandsObject.u.array.length; ++CommandIndex)
	{
		const json_value &CommandObject = CommandsObject[CommandIndex];
		if(CommandObject.type != json_object)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'commands' must specify an array of objects", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex);
			return nullptr;
		}

		const json_value &Label = CommandObject["label"];
		if(Label.type != json_string)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'label' must specify a string", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex);
			return nullptr;
		}

		const json_value &LabelType = CommandObject["label-type"];
		if(LabelType.type != json_string && LabelType.type != json_none)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'label-type' must specify a string", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex);
			return {};
		}
		CButtonLabel::EType ParsedLabelType = CButtonLabel::EType::NUM_TYPES;
		if(LabelType.type == json_none)
		{
			ParsedLabelType = CButtonLabel::EType::PLAIN;
		}
		else
		{
			for(int CurrentType = (int)CButtonLabel::EType::PLAIN; CurrentType < (int)CButtonLabel::EType::NUM_TYPES; ++CurrentType)
			{
				if(str_comp(LabelType.u.string.ptr, LABEL_TYPE_NAMES[CurrentType]) == 0)
				{
					ParsedLabelType = (CButtonLabel::EType)CurrentType;
					break;
				}
			}
		}
		if(ParsedLabelType == CButtonLabel::EType::NUM_TYPES)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'label-type' specifies unknown value '%s'", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex, LabelType.u.string.ptr);
			return {};
		}

		const json_value &Command = CommandObject["command"];
		if(Command.type != json_string)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'command' must specify a string", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex);
			return nullptr;
		}
		vCommands.emplace_back(Label.u.string.ptr, ParsedLabelType, Command.u.string.ptr);
	}
	return std::make_unique<CBindToggleTouchButtonBehavior>(std::move(vCommands));
}

void CTouchControls::WriteConfiguration(CJsonWriter *pWriter)
{
	pWriter->BeginObject();

	pWriter->WriteAttribute("direct-touch-ingame");
	pWriter->WriteStrValue(DIRECT_TOUCH_INGAME_MODE_NAMES[(int)m_DirectTouchIngame]);

	pWriter->WriteAttribute("direct-touch-spectate");
	pWriter->WriteStrValue(DIRECT_TOUCH_SPECTATE_MODE_NAMES[(int)m_DirectTouchSpectate]);

	char aColor[9];
	str_format(aColor, sizeof(aColor), "%08X", m_BackgroundColorInactive.PackAlphaLast());
	pWriter->WriteAttribute("background-color-inactive");
	pWriter->WriteStrValue(aColor);

	str_format(aColor, sizeof(aColor), "%08X", m_BackgroundColorActive.PackAlphaLast());
	pWriter->WriteAttribute("background-color-active");
	pWriter->WriteStrValue(aColor);

	pWriter->WriteAttribute("touch-buttons");
	pWriter->BeginArray();
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.WriteToConfiguration(pWriter);
	}
	pWriter->EndArray();

	pWriter->EndObject();
}

// This is called when the checkbox "Edit touch controls" is selected, so virtual visibility could be set as the real visibility on entering.
void CTouchControls::ResetVirtualVisibilities()
{
	// Update virtual visibilities.
	for(int Visibility = (int)EButtonVisibility::INGAME; Visibility < (int)EButtonVisibility::NUM_VISIBILITIES; ++Visibility)
		m_aVirtualVisibilities[Visibility] = m_aVisibilityFunctions[Visibility].m_Function();
}

void CTouchControls::UpdateButtonsEditor(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	std::vector<CUnitRect> vVisibleButtonRects;
	const vec2 ScreenSize = CalculateScreenSize();
	bool LongPress = false;
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibilityEditor();
	}

	if(vTouchFingerStates.empty())
		m_PreventSaving = false;

	// Remove if the finger deleted has released.
	if(!m_vDeletedFingerState.empty())
	{
		const auto &Remove = std::remove_if(m_vDeletedFingerState.begin(), m_vDeletedFingerState.end(), [&vTouchFingerStates](const auto &TargetState) {
			return std::none_of(vTouchFingerStates.begin(), vTouchFingerStates.end(), [&](const auto &State) {
				return State.m_Finger == TargetState.m_Finger;
			});
		});
		m_vDeletedFingerState.erase(Remove, m_vDeletedFingerState.end());
	}
	// Delete fingers if they are press later. So they cant be the longpress finger.
	if(vTouchFingerStates.size() > 1)
	{
		std::for_each(vTouchFingerStates.begin() + 1, vTouchFingerStates.end(), [&](const auto &State) {
			m_vDeletedFingerState.push_back(State);
		});
	}

	// If released, and there is finger on screen, and the "first finger" is not deleted(new finger), then it can be a LongPress candidate.
	if(!vTouchFingerStates.empty() && !std::any_of(m_vDeletedFingerState.begin(), m_vDeletedFingerState.end(), [&vTouchFingerStates](const auto &State) {
		   return vTouchFingerStates[0].m_Finger == State.m_Finger;
	   }))
	{
		// If has different finger, reset the accumulated delta.
		if(m_LongPressFingerState.has_value() && (*m_LongPressFingerState).m_Finger != vTouchFingerStates[0].m_Finger)
			m_AccumulatedDelta = vec2(0.0f, 0.0f);
		// Update the LongPress candidate state.
		m_LongPressFingerState = vTouchFingerStates[0];
	}
	// If no suitable finger for long press, then clear it.
	else
	{
		m_LongPressFingerState = std::nullopt;
	}

	// Find long press button. LongPress == true means the first fingerstate long pressed.
	if(m_LongPressFingerState.has_value())
	{
		m_AccumulatedDelta += (*m_LongPressFingerState).m_Delta;
		// If slided, then delete.
		if(length(m_AccumulatedDelta) > 0.005f)
		{
			m_AccumulatedDelta = vec2(0.0f, 0.0f);
			m_vDeletedFingerState.push_back(*m_LongPressFingerState);
			m_LongPressFingerState = std::nullopt;
			m_PreventSaving = true;
		}
		// Till now, this else contains: if the finger hasn't slided, have no fingers that remain pressed down when it pressed, hasn't been a longpress already, the candidate is always the first finger.
		else
		{
			const auto Now = time_get_nanoseconds();
			if(!m_PreventSaving && Now - (*m_LongPressFingerState).m_PressTime > LONG_TOUCH_DURATION)
			{
				LongPress = true;
				m_vDeletedFingerState.push_back(*m_LongPressFingerState);
				// LongPress will be used this frame for sure, so reset delta.
				m_AccumulatedDelta = vec2(0.0f, 0.0f);
			}
		}
	}

	// Update active and zoom fingerstate. The first finger will be used for moving button.
	if(!vTouchFingerStates.empty())
		m_ActiveFingerState = vTouchFingerStates[0];
	else
	{
		m_ActiveFingerState = std::nullopt;
		if(m_pSampleButton != nullptr && m_ShownRect.has_value())
			m_pSampleButton->m_UnitRect = (*m_ShownRect);
	}
	// Only the second finger will be used for zooming button.
	if(vTouchFingerStates.size() > 1)
	{
		// If zoom finger is pressed now, reset the zoom startpos
		if(!m_ZoomFingerState.has_value())
			m_ZoomStartPos = m_ActiveFingerState.value().m_Position - vTouchFingerStates[1].m_Position;
		m_ZoomFingerState = vTouchFingerStates[1];
		m_PreventSaving = true;

		// If Zooming started, update it's x,y value so it's width and height could be calculated correctly.
		if(m_pSampleButton != nullptr && m_ShownRect.has_value())
		{
			m_pSampleButton->m_UnitRect.m_X = m_ShownRect->m_X;
			m_pSampleButton->m_UnitRect.m_Y = m_ShownRect->m_Y;
		}
	}
	else
	{
		m_ZoomFingerState = std::nullopt;
		m_ZoomStartPos = vec2(0.0f, 0.0f);
		if(m_pSampleButton != nullptr && m_ShownRect.has_value())
		{
			m_pSampleButton->m_UnitRect.m_W = m_ShownRect->m_W;
			m_pSampleButton->m_UnitRect.m_H = m_ShownRect->m_H;
		}
	}
	for(auto &TouchButton : m_vTouchButtons)
	{
		if(TouchButton.m_VisibilityCached)
		{
			if(m_pSelectedButton == &TouchButton)
				continue;
			// Only Long Pressed finger "in visible button" is used for selecting a button.
			if(LongPress && !vTouchFingerStates.empty() && TouchButton.IsInside((*m_LongPressFingerState).m_Position * ScreenSize))
			{
				// If m_pSelectedButton changes, Confirm if saving changes, then change.
				// LongPress used.
				LongPress = false;
				// Note: Even after the popup is opened by ChangeSelectedButtonWhile..., the fingerstate still exists. So we have to add it to m_vDeletedFingerState.
				m_vDeletedFingerState.push_back(*m_LongPressFingerState);
				m_LongPressFingerState = std::nullopt;
				if(m_UnsavedChanges)
				{
					// Update sample button before saving, or sample button's position value might be not updated.
					if(m_pSampleButton != nullptr && m_ShownRect.has_value())
						m_pSampleButton->m_UnitRect = *m_ShownRect;
					m_PopupParam.m_KeepMenuOpen = false;
					m_PopupParam.m_pOldSelectedButton = m_pSelectedButton;
					m_PopupParam.m_pNewSelectedButton = &TouchButton;
					m_PopupParam.m_PopupType = EPopupType::BUTTON_CHANGED;
					GameClient()->m_Menus.SetActive(true);
					// End the function.
					return;
				}
				m_pSelectedButton = &TouchButton;
				// Update illegal position when Long press the button. Or later it will keep saying unsavedchanges.
				if(IsRectOverlapping(TouchButton.m_UnitRect, TouchButton.m_Shape))
				{
					std::optional<CUnitRect> FreeRect = UpdatePosition(TouchButton.m_UnitRect, TouchButton.m_Shape);
					m_UnsavedChanges = true;
					if(!FreeRect.has_value())
					{
						m_PopupParam.m_PopupType = EPopupType::NO_SPACE;
						m_PopupParam.m_KeepMenuOpen = true;
						GameClient()->m_Menus.SetActive(true);
						return;
					}
					TouchButton.m_UnitRect = FreeRect.value();
					TouchButton.UpdateScreenFromUnitRect();
				}
				m_aIssueParam[(int)EIssueType::CACHE_SETTINGS].m_pTargetButton = m_pSelectedButton;
				m_aIssueParam[(int)EIssueType::CACHE_SETTINGS].m_Resolved = false;
				RemakeSampleButton();
				UpdateSampleButton(*m_pSelectedButton);
				// Don't insert the long pressed button. It is selected button now.
				continue;
			}
			// Insert visible but not selected buttons.
			vVisibleButtonRects.emplace_back(CalculateHitbox(TouchButton.m_UnitRect, TouchButton.m_Shape));
		}
		// If selected button not visible, unselect it.
		else if(m_pSelectedButton == &TouchButton && !GameClient()->m_Menus.IsActive())
		{
			m_PopupParam.m_PopupType = EPopupType::BUTTON_INVISIBLE;
			GameClient()->m_Menus.SetActive(true);
			return;
		}
	}

	// Nothing left to do if no button selected.
	if(m_pSampleButton == nullptr)
		return;

	// If LongPress == true, LongPress finger has to be outside of all visible buttons.(Except m_pSampleButton. This button hasn't been checked)
	if(LongPress)
	{
		// LongPress should be set to false, but to pass clang-tidy check, this shouldn't be set now
		// LongPress = false;
		bool IsInside = CalculateScreenFromUnitRect(*m_ShownRect).Inside(m_LongPressFingerState->m_Position * ScreenSize);
		m_vDeletedFingerState.push_back(*m_LongPressFingerState);
		m_LongPressFingerState = std::nullopt;
		if(m_UnsavedChanges && !IsInside)
		{
			m_pSampleButton->m_UnitRect = *m_ShownRect;
			m_PopupParam.m_pNewSelectedButton = nullptr;
			m_PopupParam.m_pOldSelectedButton = m_pSelectedButton;
			m_PopupParam.m_KeepMenuOpen = false;
			m_PopupParam.m_PopupType = EPopupType::BUTTON_CHANGED;
			GameClient()->m_Menus.SetActive(true);
		}
		else if(!IsInside)
		{
			m_UnsavedChanges = false;
			ResetButtonPointers();
			// No need for caching settings issue. So the issue is set to finished.
			m_aIssueParam[(int)EIssueType::CACHE_SETTINGS].m_Resolved = true;
			m_aIssueParam[(int)EIssueType::SAVE_SETTINGS].m_Resolved = true;
			m_aIssueParam[(int)EIssueType::CACHE_POSITION].m_Resolved = true;
		}
	}

	if(m_pSampleButton != nullptr)
	{
		if(m_ActiveFingerState.has_value() && m_ZoomFingerState == std::nullopt)
		{
			vec2 UnitXYDelta = m_ActiveFingerState->m_Delta * BUTTON_SIZE_SCALE;
			m_pSampleButton->m_UnitRect.m_X += UnitXYDelta.x;
			m_pSampleButton->m_UnitRect.m_Y += UnitXYDelta.y;
			auto Hitbox = CalculateHitbox(m_pSampleButton->m_UnitRect, m_pSampleButton->m_Shape);
			m_ShownRect = FindPositionXY(vVisibleButtonRects, Hitbox);
			dbg_assert(m_ShownRect.has_value(), "Unexpected nullopt in m_ShownRect. Original rect: %d %d %d %d", Hitbox.m_X, Hitbox.m_Y, Hitbox.m_W, Hitbox.m_H);
			if(m_pSelectedButton != nullptr)
			{
				unsigned Movement = std::abs(m_pSelectedButton->m_UnitRect.m_X - m_ShownRect->m_X) + std::abs(m_pSelectedButton->m_UnitRect.m_Y - m_ShownRect->m_Y);
				if(Movement > 10000)
				{
					// Moved a lot, meaning changes made.
					m_UnsavedChanges = true;
				}
			}
		}
		else if(m_ActiveFingerState.has_value() && m_ZoomFingerState.has_value())
		{
			m_ShownRect = m_pSampleButton->m_UnitRect;
			vec2 UnitWHDelta;
			UnitWHDelta.x = (std::abs(m_ActiveFingerState.value().m_Position.x - m_ZoomFingerState.value().m_Position.x) - std::abs(m_ZoomStartPos.x)) * BUTTON_SIZE_SCALE;
			UnitWHDelta.y = (std::abs(m_ActiveFingerState.value().m_Position.y - m_ZoomFingerState.value().m_Position.y) - std::abs(m_ZoomStartPos.y)) * BUTTON_SIZE_SCALE;
			m_ShownRect->m_W = m_pSampleButton->m_UnitRect.m_W + UnitWHDelta.x;
			m_ShownRect->m_H = m_pSampleButton->m_UnitRect.m_H + UnitWHDelta.y;
			m_ShownRect->m_W = std::clamp(m_ShownRect->m_W, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MAXIMUM);
			m_ShownRect->m_H = std::clamp(m_ShownRect->m_H, BUTTON_SIZE_MINIMUM, BUTTON_SIZE_MAXIMUM);
			if(m_ShownRect->m_W + m_ShownRect->m_X > BUTTON_SIZE_SCALE)
				m_ShownRect->m_W = BUTTON_SIZE_SCALE - m_ShownRect->m_X;
			if(m_ShownRect->m_H + m_ShownRect->m_Y > BUTTON_SIZE_SCALE)
				m_ShownRect->m_H = BUTTON_SIZE_SCALE - m_ShownRect->m_Y;
			// Clamp the biggest W and H so they won't overlap with other buttons. Known as "FindPositionWH".
			std::optional<int> BiggestW;
			std::optional<int> BiggestH;
			std::optional<int> LimitH, LimitW;
			for(const auto &Rect : vVisibleButtonRects)
			{
				// If Overlap
				if(!(Rect.m_X + Rect.m_W <= m_ShownRect->m_X || m_ShownRect->m_X + m_ShownRect->m_W <= Rect.m_X || Rect.m_Y + Rect.m_H <= m_ShownRect->m_Y || m_ShownRect->m_Y + m_ShownRect->m_H <= Rect.m_Y))
				{
					// Calculate the biggest Height and Width it could have.
					LimitH = Rect.m_Y - m_ShownRect->m_Y;
					LimitW = Rect.m_X - m_ShownRect->m_X;
					if(LimitH < BUTTON_SIZE_MINIMUM)
						LimitH = std::nullopt;
					if(LimitW < BUTTON_SIZE_MINIMUM)
						LimitW = std::nullopt;
					if(LimitH.has_value() && LimitW.has_value())
					{
						if(std::abs(*LimitH - m_ShownRect->m_H) < std::abs(*LimitW - m_ShownRect->m_W))
						{
							BiggestH = std::min(*LimitH, BiggestH.value_or(BUTTON_SIZE_SCALE));
						}
						else
						{
							BiggestW = std::min(*LimitW, BiggestW.value_or(BUTTON_SIZE_SCALE));
						}
					}
					else
					{
						if(LimitH.has_value())
							BiggestH = std::min(*LimitH, BiggestH.value_or(BUTTON_SIZE_SCALE));
						else if(LimitW.has_value())
							BiggestW = std::min(*LimitW, BiggestW.value_or(BUTTON_SIZE_SCALE));
						else
						{
							/*
							 * LimitH and W can be nullopt at the same time, because two buttons may be overlapping.
							 * Holding for long press while another finger is pressed.
							 * Then it will instantly enter zoom mode while buttons are overlapping with each other.
							 */
							auto Hitbox = CalculateHitbox(m_pSampleButton->m_UnitRect, m_pSampleButton->m_Shape);
							m_ShownRect = FindPositionXY(vVisibleButtonRects, Hitbox);
							dbg_assert(m_ShownRect.has_value(), "Unexpected nullopt in m_ShownRect. Original rect: %d %d %d %d", Hitbox.m_X, Hitbox.m_Y, Hitbox.m_W, Hitbox.m_H);
							BiggestW = std::nullopt;
							BiggestH = std::nullopt;
							break;
						}
					}
				}
			}
			m_ShownRect->m_W = BiggestW.value_or(m_ShownRect->m_W);
			m_ShownRect->m_H = BiggestH.value_or(m_ShownRect->m_H);
			m_UnsavedChanges = true;
		}
		// No finger on screen, then show it as is.
		else
		{
			m_ShownRect = m_pSampleButton->m_UnitRect;
		}
		// Finished moving, no finger on screen.
		if(vTouchFingerStates.empty())
		{
			m_AccumulatedDelta = vec2(0.0f, 0.0f);
			std::optional<CUnitRect> OldRect = m_ShownRect;
			auto Hitbox = CalculateHitbox(m_pSampleButton->m_UnitRect, m_pSampleButton->m_Shape);
			m_ShownRect = FindPositionXY(vVisibleButtonRects, Hitbox);
			dbg_assert(m_ShownRect.has_value(), "Unexpected nullopt in m_ShownRect. Original rect: %d %d %d %d", Hitbox.m_X, Hitbox.m_Y, Hitbox.m_W, Hitbox.m_H);
			m_UnsavedChanges |= OldRect != m_ShownRect;
			m_pSampleButton->m_UnitRect = (*m_ShownRect);
			m_aIssueParam[(int)EIssueType::CACHE_POSITION].m_pTargetButton = m_pSampleButton.get();
			m_aIssueParam[(int)EIssueType::CACHE_POSITION].m_Resolved = false;
			m_pSampleButton->UpdateScreenFromUnitRect();
		}
		if(m_ShownRect->m_X == -1)
		{
			m_UnsavedChanges = true;
			m_PopupParam.m_PopupType = EPopupType::NO_SPACE;
			m_PopupParam.m_KeepMenuOpen = true;
			GameClient()->m_Menus.SetActive(true);
			return;
		}
		m_pSampleButton->UpdateScreenFromUnitRect();
	}
}

void CTouchControls::RenderButtonsEditor()
{
	for(auto &TouchButton : m_vTouchButtons)
	{
		if(&TouchButton == m_pSelectedButton)
			continue;
		TouchButton.UpdateVisibilityEditor();
		if(TouchButton.m_VisibilityCached || m_PreviewAllButtons)
		{
			TouchButton.UpdateScreenFromUnitRect();
			TouchButton.Render(false);
		}
	}

	if(m_pSampleButton != nullptr && m_ShownRect.has_value())
	{
		m_pSampleButton->Render(true, m_ShownRect);
	}
}

std::optional<CTouchControls::CUnitRect> CTouchControls::FindPositionXY(std::vector<CUnitRect> &vVisibleButtonRects, CUnitRect MyRect)
{
	// Border clamp
	MyRect.m_X = std::clamp(MyRect.m_X, 0, BUTTON_SIZE_SCALE - MyRect.m_W);
	MyRect.m_Y = std::clamp(MyRect.m_Y, 0, BUTTON_SIZE_SCALE - MyRect.m_H);
	// Not overlapping with any rects
	{
		bool IfOverlap = std::any_of(vVisibleButtonRects.begin(), vVisibleButtonRects.end(), [&MyRect](const auto &Rect) {
			return MyRect.IsOverlap(Rect);
		});
		if(!IfOverlap)
			return MyRect;
	}
	if(vVisibleButtonRects != m_vLastUpdateRects || MyRect.m_W != m_LastWidth || MyRect.m_H != m_LastHeight)
	{
		m_LastWidth = MyRect.m_W;
		m_LastHeight = MyRect.m_H;
		m_vLastUpdateRects = vVisibleButtonRects;
		BuildPositionXY(m_vLastUpdateRects, MyRect);
	}
	std::optional<CUnitRect> Result;
	CUnitRect SampleRect;
	for(const ivec2 &Target : m_vTargets)
	{
		SampleRect = {Target.x, Target.y, MyRect.m_W, MyRect.m_H};
		if(!Result.has_value() || MyRect.Distance(Result.value()) > MyRect.Distance(SampleRect))
			Result = SampleRect;
	}
	int BestXPosition = -BUTTON_SIZE_SCALE, BestYPosition = -BUTTON_SIZE_SCALE, Cur = 0;
	std::vector<CUnitRect> vTargetRects;
	vTargetRects.reserve(m_vLastUpdateRects.size());
	std::copy_if(m_vXSortedRects.begin(), m_vXSortedRects.end(), std::back_inserter(vTargetRects), [&](const CUnitRect &Rect) {
		return !(Rect.m_Y + Rect.m_H <= MyRect.m_Y || MyRect.m_Y + MyRect.m_H <= Rect.m_Y);
	});
	for(const CUnitRect &Rect : vTargetRects)
	{
		if(Cur >= Rect.m_X + Rect.m_W)
			continue;
		SampleRect = {Cur, MyRect.m_Y, MyRect.m_W, MyRect.m_H};
		if(Cur + MyRect.m_W <= BUTTON_SIZE_SCALE && !SampleRect.IsOverlap(Rect))
		{
			BestXPosition = std::abs(Cur - MyRect.m_X) < std::abs(BestXPosition - MyRect.m_X) ? Cur : BestXPosition;
			BestXPosition = std::abs(Rect.m_X - MyRect.m_W - MyRect.m_X) < std::abs(BestXPosition - MyRect.m_X) ? Rect.m_X - MyRect.m_W : BestXPosition;
		}
		Cur = Rect.m_X + Rect.m_W;
	}
	if(Cur + MyRect.m_W <= BUTTON_SIZE_SCALE)
	{
		BestXPosition = std::abs(Cur - MyRect.m_X) < std::abs(BestXPosition - MyRect.m_X) ? Cur : BestXPosition;
	}

	vTargetRects.clear();
	std::copy_if(m_vYSortedRects.begin(), m_vYSortedRects.end(), std::back_inserter(vTargetRects), [&](const CUnitRect &Rect) {
		return !(Rect.m_X + Rect.m_W <= MyRect.m_X || MyRect.m_X + MyRect.m_W <= Rect.m_X);
	});
	Cur = 0;
	for(const CUnitRect &Rect : vTargetRects)
	{
		if(Cur >= Rect.m_Y + Rect.m_H)
			continue;
		SampleRect = {MyRect.m_X, Cur, MyRect.m_W, MyRect.m_H};
		if(Cur + MyRect.m_H <= BUTTON_SIZE_SCALE && !SampleRect.IsOverlap(Rect))
		{
			BestYPosition = std::abs(Cur - MyRect.m_Y) < std::abs(BestYPosition - MyRect.m_Y) ? Cur : BestYPosition;
			BestYPosition = std::abs(Rect.m_Y - MyRect.m_H - MyRect.m_Y) < std::abs(BestYPosition - MyRect.m_Y) ? Rect.m_Y - MyRect.m_H : BestYPosition;
		}
		Cur = Rect.m_Y + Rect.m_H;
	}
	if(Cur + MyRect.m_H <= BUTTON_SIZE_SCALE)
	{
		BestYPosition = std::abs(Cur - MyRect.m_Y) < std::abs(BestYPosition - MyRect.m_Y) ? Cur : BestYPosition;
	}

	if(BestXPosition != -BUTTON_SIZE_SCALE)
	{
		SampleRect = {BestXPosition, MyRect.m_Y, MyRect.m_W, MyRect.m_H};
		if(!Result.has_value() || MyRect.Distance(Result.value()) > MyRect.Distance(SampleRect))
			Result = SampleRect;
	}
	if(BestYPosition != -BUTTON_SIZE_SCALE)
	{
		SampleRect = {MyRect.m_X, BestYPosition, MyRect.m_W, MyRect.m_H};
		if(!Result.has_value() || MyRect.Distance(Result.value()) > MyRect.Distance(SampleRect))
			Result = SampleRect;
	}
	return Result;
}

void CTouchControls::BuildPositionXY(std::vector<CUnitRect> vVisibleButtonRects, CUnitRect MyRect)
{
	m_vTargets.clear();
	m_vTargets.reserve(vVisibleButtonRects.size() * 4);
	m_vXSortedRects = m_vYSortedRects = vVisibleButtonRects;
	std::sort(m_vXSortedRects.begin(), m_vXSortedRects.end(), [](const CUnitRect &Lhs, const CUnitRect &Rhs) {
		return Lhs.m_X < Rhs.m_X;
	});
	std::sort(m_vYSortedRects.begin(), m_vYSortedRects.end(), [](const CUnitRect &Lhs, const CUnitRect &Rhs) {
		return Lhs.m_Y < Rhs.m_Y;
	});
	std::sort(vVisibleButtonRects.begin(), vVisibleButtonRects.end(), [](const CUnitRect &Lhs, const CUnitRect &Rhs) {
		return Lhs.m_X < Rhs.m_X;
	});
	class CTree
	{
	public:
		void Init(const std::vector<CUnitRect> &vRects)
		{
			m_vOrder.reserve(vRects.size() * 2);
			for(const CUnitRect &Rect : vRects)
			{
				m_vOrder.emplace_back(Rect.m_Y);
				m_vOrder.emplace_back(Rect.m_Y + Rect.m_H);
			}
			m_vOrder.emplace_back(0);
			m_vOrder.emplace_back(BUTTON_SIZE_SCALE);
			std::sort(m_vOrder.begin(), m_vOrder.end());
			m_vOrder.erase(std::unique(m_vOrder.begin(), m_vOrder.end()), m_vOrder.end());
			m_vTree.resize(m_vOrder.size() * 4, {-1, -1, 0, 0});
			New(0, m_vOrder.size() - 2, 0);
			m_vZone.reserve(m_vTree.size());
		}
		void New(int Start, int End, unsigned Cur)
		{
			if(m_vTree[Cur].x != -1)
				return;
			m_vTree[Cur].x = Start;
			m_vTree[Cur].y = End;
			m_vTree[Cur].z = 0;
			m_vTree[Cur].w = 0;
		}
		void Add(int Start, int End, unsigned Cur)
		{
			m_vTree[Cur].z++;
			if(m_vTree[Cur].x == Start && m_vTree[Cur].y == End)
			{
				m_vTree[Cur].w++;
				return;
			}
			int Mid = (m_vTree[Cur].x + m_vTree[Cur].y) / 2;
			New(Mid + 1, m_vTree[Cur].y, Cur * 2 + 2);
			New(m_vTree[Cur].x, Mid, Cur * 2 + 1);
			if(Start <= Mid)
			{
				Add(Start, minimum<int>(Mid, End), Cur * 2 + 1);
			}
			if(End >= Mid + 1)
			{
				Add(maximum<int>(Mid + 1, Start), End, Cur * 2 + 2);
			}
		}
		void Delete(int Start, int End, unsigned Cur)
		{
			m_vTree[Cur].z--;
			if(m_vTree[Cur].x == Start && m_vTree[Cur].y == End)
			{
				m_vTree[Cur].w--;
				return;
			}
			int Mid = (m_vTree[Cur].x + m_vTree[Cur].y) / 2;
			if(Start <= Mid)
			{
				Delete(Start, minimum<int>(Mid, End), Cur * 2 + 1);
			}
			if(End >= Mid + 1)
			{
				Delete(maximum<int>(Mid + 1, Start), End, Cur * 2 + 2);
			}
		}
		void InnerQuery(unsigned Start)
		{
			std::vector<unsigned> vStack;
			vStack.reserve(BUTTON_SIZE_SCALE / BUTTON_SIZE_MINIMUM);
			vStack.push_back(Start);
			while(!vStack.empty())
			{
				unsigned Cur = vStack.back();
				vStack.pop_back();
				if(m_vTree[Cur].w > 0)
				{
					m_vZone.emplace_back(m_vTree[Cur].x, m_vTree[Cur].y);
					continue;
				}
				if(m_vTree[Cur].x == m_vTree[Cur].y || m_vTree[Cur].z == 0)
					continue;
				if(m_vTree[Cur * 2 + 2].x != -1 && m_vTree[Cur * 2 + 2].z > 0)
				{
					vStack.push_back((Cur << 1) + 2);
				}
				if(m_vTree[Cur * 2 + 1].x != -1 && m_vTree[Cur * 2 + 1].z > 0)
				{
					vStack.push_back((Cur << 1) + 1);
				}
			}
		}
		std::vector<ivec2> Query(int Length)
		{
			m_vZone.clear();
			InnerQuery(0);
			if(m_vZone.empty())
			{
				return {{0, BUTTON_SIZE_SCALE}};
			}

			// Inverse discretization
			for(ivec2 &Zone : m_vZone)
			{
				Zone.x = m_vOrder[Zone.x];
				Zone.y = m_vOrder[Zone.y + 1];
			}
			if(m_vZone[0].x < Length)
				m_vZone[0].x = 0;
			// Merge segments.
			for(unsigned Index = 1; Index < m_vZone.size(); Index++)
			{
				if(m_vZone[Index - 1].y + Length <= m_vZone[Index].x)
					continue;
				m_vZone[Index].x = m_vZone[Index - 1].x;
				m_vZone[Index - 1].x = -1;
			}
			if(m_vZone.back().y + Length > BUTTON_SIZE_SCALE)
				m_vZone.back().y = BUTTON_SIZE_SCALE;
			m_vZone.erase(std::remove_if(m_vZone.begin(), m_vZone.end(), [](const ivec2 &Zone) { return Zone.x == -1; }),
				m_vZone.end());
			// Result stores obstacles, now turn it into free spaces.
			std::vector<ivec2> vFree;
			vFree.reserve(m_vZone.size());
			if(m_vZone[0].x != 0)
				vFree.emplace_back(0, m_vZone[0].x);
			for(unsigned Index = 1; Index < m_vZone.size(); Index++)
			{
				vFree.emplace_back(m_vZone[Index - 1].y, m_vZone[Index].x);
			}
			if(m_vZone.back().y != BUTTON_SIZE_SCALE)
				vFree.emplace_back(m_vZone.back().y, BUTTON_SIZE_SCALE);
			return vFree;
		}
		ivec2 Discretization(int Start, int End)
		{
			ivec2 Result;
			auto It = std::lower_bound(m_vOrder.begin(), m_vOrder.end(), Start);
			Result.x = std::distance(m_vOrder.begin(), It);
			It = std::lower_bound(m_vOrder.begin(), m_vOrder.end(), End);
			Result.y = std::distance(m_vOrder.begin(), It) - 1;
			return Result;
		}

	private:
		std::vector<int> m_vOrder;
		std::vector<ivec4> m_vTree;
		std::vector<ivec2> m_vZone;
	} Tree;

	std::set<int> CandidateX;
	for(const CUnitRect &Rect : vVisibleButtonRects)
	{
		// Rect right border.
		int Pos = Rect.m_X + Rect.m_W;
		if(Pos + MyRect.m_W <= BUTTON_SIZE_SCALE)
			CandidateX.insert(Pos);
		// Rect left border.
		Pos = Rect.m_X - MyRect.m_W;
		if(Pos >= 0)
			CandidateX.insert(Pos);
	}
	CandidateX.insert(CandidateX.begin(), 0);
	CandidateX.insert(BUTTON_SIZE_SCALE - MyRect.m_W);
	Tree.Init(vVisibleButtonRects);

	auto Cmp = [&vVisibleButtonRects](int Lhs, int Rhs) -> bool {
		return vVisibleButtonRects[Lhs].m_X + vVisibleButtonRects[Lhs].m_W > vVisibleButtonRects[Rhs].m_X + vVisibleButtonRects[Rhs].m_W;
	};
	std::priority_queue<int, std::vector<int>, decltype(Cmp)> Out(Cmp);

	unsigned Index = 0;

	for(int CurrentX : CandidateX)
	{
		while(Index < vVisibleButtonRects.size() && vVisibleButtonRects[Index].m_X < CurrentX + MyRect.m_W)
		{
			auto Segment = Tree.Discretization(vVisibleButtonRects[Index].m_Y, vVisibleButtonRects[Index].m_Y + vVisibleButtonRects[Index].m_H);
			Tree.Add(Segment.x, Segment.y, 0);
			Out.emplace(Index++);
		}
		while(!Out.empty() && vVisibleButtonRects[Out.top()].m_X + vVisibleButtonRects[Out.top()].m_W <= CurrentX)
		{
			auto Segment = Tree.Discretization(vVisibleButtonRects[Out.top()].m_Y, vVisibleButtonRects[Out.top()].m_Y + vVisibleButtonRects[Out.top()].m_H);
			Tree.Delete(Segment.x, Segment.y, 0);
			Out.pop();
		}
		auto Spaces = Tree.Query(MyRect.m_H);
		for(ivec2 &Space : Spaces)
		{
			m_vTargets.emplace_back(CurrentX, Space.x);
			m_vTargets.emplace_back(CurrentX, Space.y - MyRect.m_H);
		}
	}
}

// Create a new button and push_back to m_vTouchButton, then return a pointer.
CTouchControls::CTouchButton *CTouchControls::NewButton()
{
	// Ensure m_pSelectedButton doesn't go wild.
	int Target = -1;
	if(m_pSelectedButton != nullptr)
		Target = std::distance(m_vTouchButtons.data(), m_pSelectedButton);
	CTouchButton NewButton(this);
	NewButton.m_pBehavior = std::make_unique<CBindTouchButtonBehavior>("", CButtonLabel::EType::PLAIN, "");
	// So the vector's elements might be moved. If moved all button's m_VisibilityCached will be set to false. This should be prevented.
	std::vector<bool> vCachedVisibilities;
	vCachedVisibilities.reserve(m_vTouchButtons.size());
	for(const auto &Button : m_vTouchButtons)
	{
		vCachedVisibilities.emplace_back(Button.m_VisibilityCached);
	}
	for(unsigned Iterator = 0; Iterator < vCachedVisibilities.size(); Iterator++)
	{
		m_vTouchButtons[Iterator].m_VisibilityCached = vCachedVisibilities[Iterator];
	}
	m_vTouchButtons.push_back(std::move(NewButton));
	if(Target != -1)
		m_pSelectedButton = &m_vTouchButtons[Target];
	return &m_vTouchButtons.back();
}

void CTouchControls::DeleteSelectedButton()
{
	if(m_pSelectedButton != nullptr)
	{
		auto DeleteIt = m_vTouchButtons.begin() + (m_pSelectedButton - m_vTouchButtons.data());
		m_vTouchButtons.erase(DeleteIt);
	}
	ResetButtonPointers();
	m_UnsavedChanges = false;
}

bool CTouchControls::IsRectOverlapping(CUnitRect MyRect, EButtonShape Shape) const
{
	MyRect = CalculateHitbox(MyRect, Shape);
	for(const auto &TouchButton : m_vTouchButtons)
	{
		if(m_pSelectedButton == &TouchButton)
			continue;
		bool IsVisible = std::all_of(TouchButton.m_vVisibilities.begin(), TouchButton.m_vVisibilities.end(), [&](const auto &Visibility) {
			return Visibility.m_Parity == m_aVirtualVisibilities[(int)Visibility.m_Type];
		});
		if(IsVisible && MyRect.IsOverlap(CalculateHitbox(TouchButton.m_UnitRect, TouchButton.m_Shape)))
			return true;
	}
	return false;
}

std::optional<CTouchControls::CUnitRect> CTouchControls::UpdatePosition(CUnitRect MyRect, EButtonShape Shape, bool Ignore)
{
	MyRect = CalculateHitbox(MyRect, Shape);
	std::vector<CUnitRect> vVisibleButtonRects;
	for(const auto &TouchButton : m_vTouchButtons)
	{
		if(m_pSelectedButton == &TouchButton && !Ignore)
			continue;
		bool IsVisible = std::all_of(TouchButton.m_vVisibilities.begin(), TouchButton.m_vVisibilities.end(), [&](const auto &Visibility) {
			return Visibility.m_Parity == m_aVirtualVisibilities[(int)Visibility.m_Type];
		});
		if(!IsVisible)
			continue;
		vVisibleButtonRects.emplace_back(CalculateHitbox(TouchButton.m_UnitRect, TouchButton.m_Shape));
	}
	return FindPositionXY(vVisibleButtonRects, MyRect);
}

void CTouchControls::ResetButtonPointers()
{
	m_pSelectedButton = nullptr;
	m_pSampleButton = nullptr;
	m_ShownRect = std::nullopt;
}

// After sending the type, the popup should be reset immediately.
CTouchControls::CPopupParam CTouchControls::RequiredPopup()
{
	CPopupParam ReturnPopup = m_PopupParam;
	// Reset type so it won't be called for multiple times.
	m_PopupParam.m_PopupType = EPopupType::NUM_POPUPS;
	return ReturnPopup;
}

// Return true if any issue is not finished.
bool CTouchControls::AnyIssueNotResolved() const
{
	return std::any_of(m_aIssueParam.begin(), m_aIssueParam.end(), [](const auto &Issue) {
		return !Issue.m_Resolved;
	});
}

std::array<CTouchControls::CIssueParam, (unsigned)CTouchControls::EIssueType::NUM_ISSUES> CTouchControls::Issues()
{
	std::array<CIssueParam, (unsigned)EIssueType::NUM_ISSUES> aUnresolvedIssues;
	for(int Issue = 0; Issue < (int)EIssueType::NUM_ISSUES; Issue++)
	{
		aUnresolvedIssues[Issue] = m_aIssueParam[Issue];
		m_aIssueParam[Issue].m_Resolved = true;
	}
	return aUnresolvedIssues;
}

// Make it look like the button, only have bind behavior. This is only used on m_pSampleButton.
void CTouchControls::UpdateSampleButton(const CTouchButton &SrcButton)
{
	dbg_assert(m_pSampleButton != nullptr, "Sample button not created");
	m_pSampleButton->m_UnitRect = SrcButton.m_UnitRect;
	m_pSampleButton->m_Shape = SrcButton.m_Shape;
	m_pSampleButton->m_vVisibilities = SrcButton.m_vVisibilities;
	CButtonLabel Label = SrcButton.m_pBehavior->GetLabel();
	m_pSampleButton->m_pBehavior = std::make_unique<CBindTouchButtonBehavior>(Label.m_pLabel, Label.m_Type, "");
	m_pSampleButton->UpdatePointers();
	m_pSampleButton->m_UnitRect = CalculateHitbox(m_pSampleButton->m_UnitRect, m_pSampleButton->m_Shape);
	m_pSampleButton->UpdateScreenFromUnitRect();
}

std::vector<CTouchControls::CTouchButton *> CTouchControls::GetButtonsEditor()
{
	std::vector<CTouchButton *> vpButtons;
	vpButtons.reserve(m_vTouchButtons.size());
	for(auto &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibilityEditor();
		vpButtons.emplace_back(&TouchButton);
	}
	return vpButtons;
}

float CTouchControls::CUnitRect::Distance(const CUnitRect &Other) const
{
	vec2 Delta;
	Delta.x = Other.m_X + Other.m_W / 2.0f - m_X - m_W / 2.0f;
	Delta.y = Other.m_Y + Other.m_H / 2.0f - m_Y - m_H / 2.0f;
	return length(Delta / BUTTON_SIZE_SCALE);
}
