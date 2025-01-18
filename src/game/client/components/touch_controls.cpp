#include "touch_controls.h"

#include <base/log.h>
#include <base/system.h>

#include <engine/client.h>
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
#include <game/client/ui.h>
#include <game/localization.h>

using namespace std::chrono_literals;

// TODO: Add user interface to adjust button layout
// TODO: Add "color" property for touch buttons?
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
static constexpr int BUTTON_SIZE_SCALE = 1000000;
static constexpr int BUTTON_SIZE_MINIMUM = 50000;
static constexpr int BUTTON_SIZE_MAXIMUM = 500000;

/* This is required for the localization script to find the labels of the default bind buttons specified in the configuration file:
Localizable("Move left") Localizable("Move right") Localizable("Jump") Localizable("Prev. weapon") Localizable("Next weapon")
Localizable("Zoom out") Localizable("Default zoom") Localizable("Zoom in") Localizable("Scoreboard") Localizable("Chat") Localizable("Team chat")
Localizable("Vote yes") Localizable("Vote no") Localizable("Toggle dummy")
*/

CTouchControls::CTouchButton::CTouchButton(CTouchControls *pTouchControls) :
	m_pTouchControls(pTouchControls),
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
}

CTouchControls::CTouchButton &CTouchControls::CTouchButton::operator=(CTouchButton &&Other) noexcept
{
	m_pTouchControls = Other.m_pTouchControls;
	Other.m_pTouchControls = nullptr;
	m_UnitRect = Other.m_UnitRect;
	m_Shape = Other.m_Shape;
	m_vVisibilities = Other.m_vVisibilities;
	m_pBehavior = std::move(Other.m_pBehavior);
	m_VisibilityCached = false;
	return *this;
}

void CTouchControls::CTouchButton::UpdatePointers()
{
	m_pBehavior->Init(this);
}

void CTouchControls::CTouchButton::UpdateScreenFromUnitRect()
{
	const vec2 ScreenSize = m_pTouchControls->CalculateScreenSize();
	m_ScreenRect.x = m_UnitRect.m_X * ScreenSize.x / BUTTON_SIZE_SCALE;
	m_ScreenRect.y = m_UnitRect.m_Y * ScreenSize.y / BUTTON_SIZE_SCALE;
	m_ScreenRect.w = m_UnitRect.m_W * ScreenSize.x / BUTTON_SIZE_SCALE;
	m_ScreenRect.h = m_UnitRect.m_H * ScreenSize.y / BUTTON_SIZE_SCALE;

	// Enforce circle shape so the screen rect can be used for mapping the touch input position
	if(m_Shape == EButtonShape::CIRCLE)
	{
		if(m_ScreenRect.h > m_ScreenRect.w)
		{
			m_ScreenRect.y += (m_ScreenRect.h - m_ScreenRect.w) / 2.0f;
			m_ScreenRect.h = m_ScreenRect.w;
		}
		else if(m_ScreenRect.w > m_ScreenRect.h)
		{
			m_ScreenRect.x += (m_ScreenRect.w - m_ScreenRect.h) / 2.0f;
			m_ScreenRect.w = m_ScreenRect.h;
		}
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
		if(&OtherButton == this || OtherButton.m_Shape != EButtonShape::RECT)
			continue;
		// TODO: This does not consider that button visibilities can change independently, also update corners when any visibility changed
		const bool ExcludingVisibilities = std::any_of(OtherButton.m_vVisibilities.begin(), OtherButton.m_vVisibilities.end(), [&](const CButtonVisibility &OtherVisibility) {
			return std::any_of(m_vVisibilities.begin(), m_vVisibilities.end(), [&](const CButtonVisibility &OurVisibility) {
				return OtherVisibility.m_Type == OurVisibility.m_Type && OtherVisibility.m_Parity != OurVisibility.m_Parity;
			});
		});
		if(ExcludingVisibilities)
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
		TouchPosition.x = clamp(TouchPosition.x, m_ScreenRect.x, m_ScreenRect.x + m_ScreenRect.w);
		TouchPosition.y = clamp(TouchPosition.y, m_ScreenRect.y, m_ScreenRect.y + m_ScreenRect.h);
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
		dbg_assert(false, "Unhandled shape");
		break;
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
		dbg_assert(false, "Unhandled shape");
		return false;
	}
}

void CTouchControls::CTouchButton::UpdateVisibility()
{
	const bool PrevVisibility = m_VisibilityCached;
	m_VisibilityCached = m_pTouchControls->m_EditingActive || std::all_of(m_vVisibilities.begin(), m_vVisibilities.end(), [&](CButtonVisibility Visibility) {
		return m_pTouchControls->m_aVisibilityFunctions[(int)Visibility.m_Type].m_Function() == Visibility.m_Parity;
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
void CTouchControls::CTouchButton::Render() const
{
	const ColorRGBA ButtonColor = m_pBehavior->IsActive() ? ColorRGBA(0.2f, 0.2f, 0.2f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

	switch(m_Shape)
	{
	case EButtonShape::RECT:
	{
		m_ScreenRect.Draw(ButtonColor, m_BackgroundCorners, 10.0f);
		break;
	}
	case EButtonShape::CIRCLE:
	{
		const vec2 Center = m_ScreenRect.Center();
		const float Radius = minimum(m_ScreenRect.w, m_ScreenRect.h) / 2.0f;
		m_pTouchControls->Graphics()->TextureClear();
		m_pTouchControls->Graphics()->QuadsBegin();
		m_pTouchControls->Graphics()->SetColor(ButtonColor);
		m_pTouchControls->Graphics()->DrawCircle(Center.x, Center.y, Radius, maximum(round_truncate(Radius / 4.0f) & ~1, 32));
		m_pTouchControls->Graphics()->QuadsEnd();
		break;
	}
	default:
		dbg_assert(false, "Unhandled shape");
		break;
	}

	const float FontSize = 22.0f;
	CButtonLabel LabelData = m_pBehavior->GetLabel();
	CUIRect LabelRect;
	m_ScreenRect.Margin(10.0f, &LabelRect);
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
		dbg_assert(false, "Touch button must be inactive or use same finger");
	}
}

void CTouchControls::CTouchButtonBehavior::SetInactive()
{
	if(m_Active)
	{
		m_Active = false;
		OnDeactivate();
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

void CTouchControls::CIngameMenuTouchButtonBehavior::OnDeactivate()
{
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

void CTouchControls::CExtraMenuTouchButtonBehavior::OnDeactivate()
{
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

void CTouchControls::CEmoticonTouchButtonBehavior::OnDeactivate()
{
	m_pTouchControls->Console()->ExecuteLineStroked(1, "+emote");
}

// Spectate button: keeps the spectate menu open, next touch in spectate menu will close it again.
CTouchControls::CButtonLabel CTouchControls::CSpectateTouchButtonBehavior::GetLabel() const
{
	return {CButtonLabel::EType::LOCALIZED, Localizable("Spectator mode")};
}

void CTouchControls::CSpectateTouchButtonBehavior::OnDeactivate()
{
	m_pTouchControls->Console()->ExecuteLineStroked(1, "+spectate");
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
	else if(m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior != nullptr &&
		m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior->ActiveAction() != NUM_ACTIONS)
	{
		return {CButtonLabel::EType::LOCALIZED, ACTION_NAMES[m_pTouchControls->NextActiveAction(m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior->ActiveAction())]};
	}
	return {CButtonLabel::EType::LOCALIZED, ACTION_SWAP_NAMES[m_pTouchControls->m_ActionSelected]};
}

void CTouchControls::CSwapActionTouchButtonBehavior::OnActivate()
{
	if(m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior != nullptr &&
		m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior->ActiveAction() != NUM_ACTIONS)
	{
		m_ActiveAction = m_pTouchControls->NextActiveAction(m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior->ActiveAction());
		m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction]);
	}
	else
	{
		m_pTouchControls->m_ActionSelected = m_pTouchControls->NextActiveAction(m_pTouchControls->m_ActionSelected);
	}
}

void CTouchControls::CSwapActionTouchButtonBehavior::OnDeactivate()
{
	if(m_ActiveAction != NUM_ACTIONS)
	{
		m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction]);
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
	m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction]);
}

void CTouchControls::CUseActionTouchButtonBehavior::OnDeactivate()
{
	m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction]);
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
		m_pTouchControls->Console()->ExecuteLineStroked(1, ACTION_COMMANDS[m_ActiveAction]);
	}
}

void CTouchControls::CJoystickTouchButtonBehavior::OnDeactivate()
{
	if(m_ActiveAction != ACTION_AIM)
	{
		m_pTouchControls->Console()->ExecuteLineStroked(0, ACTION_COMMANDS[m_ActiveAction]);
	}
	m_ActiveAction = NUM_ACTIONS;
}

void CTouchControls::CJoystickTouchButtonBehavior::OnUpdate()
{
	CControls &Controls = m_pTouchControls->GameClient()->m_Controls;
	if(m_pTouchControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		vec2 WorldScreenSize;
		m_pTouchControls->RenderTools()->CalcScreenParams(m_pTouchControls->Graphics()->ScreenAspect(), m_pTouchControls->GameClient()->m_Camera.m_Zoom, &WorldScreenSize.x, &WorldScreenSize.y);
		Controls.m_aMousePos[g_Config.m_ClDummy] += -m_AccumulatedDelta * WorldScreenSize;
		Controls.m_aMousePos[g_Config.m_ClDummy].x = clamp(Controls.m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (m_pTouchControls->Collision()->GetWidth() + 201.0f) * 32.0f);
		Controls.m_aMousePos[g_Config.m_ClDummy].y = clamp(Controls.m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (m_pTouchControls->Collision()->GetHeight() + 201.0f) * 32.0f);
		m_AccumulatedDelta = vec2(0.0f, 0.0f);
	}
	else
	{
		const vec2 AbsolutePosition = (m_ActivePosition - vec2(0.5f, 0.5f)) * 2.0f;
		Controls.m_aMousePos[g_Config.m_ClDummy] = AbsolutePosition * (Controls.GetMaxMouseDistance() - Controls.GetMinMouseDistance()) + normalize(AbsolutePosition) * Controls.GetMinMouseDistance();
		if(length(Controls.m_aMousePos[g_Config.m_ClDummy]) < 0.001f)
		{
			Controls.m_aMousePos[g_Config.m_ClDummy].x = 0.001f;
			Controls.m_aMousePos[g_Config.m_ClDummy].y = 0.0f;
		}
	}
}

// Joystick that uses the active action. Registers itself as the primary joystick.
void CTouchControls::CJoystickActionTouchButtonBehavior::Init(CTouchButton *pTouchButton)
{
	CPredefinedTouchButtonBehavior::Init(pTouchButton);
	m_pTouchControls->m_pPrimaryJoystickTouchButtonBehavior = this;
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
	m_pTouchControls->Console()->ExecuteLineStroked(1, m_Command.c_str());
	m_Repeating = false;
}

void CTouchControls::CBindTouchButtonBehavior::OnDeactivate()
{
	m_pTouchControls->Console()->ExecuteLineStroked(0, m_Command.c_str());
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
			m_pTouchControls->Console()->ExecuteLineStroked(1, m_Command.c_str());
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
	m_pTouchControls->Console()->ExecuteLine(m_vCommands[m_ActiveCommandIndex].m_Command.c_str());
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
		GameClient()->m_Spectator.IsActive())
	{
		ResetButtons();
		return false;
	}

	UpdateButtons(vTouchFingerStates);
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

	RenderButtons();
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
		dbg_assert(false, "Action invalid for NextActiveAction");
		return NUM_ACTIONS;
	}
}

int CTouchControls::NextDirectTouchAction() const
{
	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		switch(m_DirectTouchSpectate)
		{
		case EDirectTouchSpectateMode::DISABLED:
			return NUM_ACTIONS;
		case EDirectTouchSpectateMode::AIM:
			return ACTION_AIM;
		default:
			dbg_assert(false, "m_DirectTouchSpectate invalid");
			return NUM_ACTIONS;
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
			dbg_assert(false, "m_DirectTouchIngame invalid");
			return NUM_ACTIONS;
		}
	}
}

void CTouchControls::UpdateButtons(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates)
{
	// Update cached button visibilities and store time that buttons become visible.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibility();
	}

	const int DirectTouchAction = NextDirectTouchAction();
	const vec2 ScreenSize = CalculateScreenSize();

	std::vector<IInput::CTouchFingerState> vRemainingTouchFingerStates = vTouchFingerStates;

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
				Console()->ExecuteLineStroked(0, ACTION_COMMANDS[Action]);
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
			PrevActiveTouchButton->m_pBehavior->SetInactive();
		}
		TouchButton.m_pBehavior->SetActive(*FingerInsideButton);
	}

	// Deactivate touch buttons only when the respective finger is released, so touch buttons
	// are kept active also if the finger is moved outside the button.
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		if(!TouchButton.IsVisible())
		{
			TouchButton.m_pBehavior->SetInactive();
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
			TouchButton.m_pBehavior->SetInactive();
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
		const float Zoom = m_pClient->m_Snap.m_SpecInfo.m_Active ? m_pClient->m_Camera.m_Zoom : 1.0f;
		vec2 WorldScreenSize;
		RenderTools()->CalcScreenParams(Graphics()->ScreenAspect(), Zoom, &WorldScreenSize.x, &WorldScreenSize.y);
		CControls &Controls = GameClient()->m_Controls;
		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			Controls.m_aMousePos[g_Config.m_ClDummy] += -DirectFingerState.m_Delta * WorldScreenSize;
			Controls.m_aMousePos[g_Config.m_ClDummy].x = clamp(Controls.m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (Collision()->GetWidth() + 201.0f) * 32.0f);
			Controls.m_aMousePos[g_Config.m_ClDummy].y = clamp(Controls.m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (Collision()->GetHeight() + 201.0f) * 32.0f);
		}
		else
		{
			Controls.m_aMousePos[g_Config.m_ClDummy] = (DirectFingerState.m_Position - vec2(0.5f, 0.5f)) * WorldScreenSize;
		}
	}

	// Activate action after the mouse position is set.
	if(ActivateAction != ACTION_AIM && ActivateAction != NUM_ACTIONS)
	{
		Console()->ExecuteLineStroked(1, ACTION_COMMANDS[ActivateAction]);
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

void CTouchControls::RenderButtons()
{
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdateVisibility();
		if(!TouchButton.IsVisible())
		{
			continue;
		}
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

	m_pPrimaryJoystickTouchButtonBehavior = nullptr;
	m_vTouchButtons = std::move(vParsedTouchButtons);
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.UpdatePointers();
		TouchButton.UpdateScreenFromUnitRect();
		TouchButton.UpdateBackgroundCorners();
	}

	json_value_free(pConfiguration);

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
	const int MaxNumber = (int)EButtonVisibility::EXTRA_MENU_5 - (int)EButtonVisibility::EXTRA_MENU_1 + 1;
	if(MenuNumber.type != json_none && (MenuNumber.type != json_integer || !in_range<json_int_t>(MenuNumber.u.integer, 1, MaxNumber)))
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s' and ID '%s': attribute 'number' must specify an integer between '%d' and '%d'",
			CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE, CExtraMenuTouchButtonBehavior::BEHAVIOR_ID, 1, MaxNumber);
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
	if(LabelType.type != json_string)
	{
		log_error("touch_controls", "Failed to parse touch button behavior of type '%s': attribute 'label-type' must specify a string", CBindTouchButtonBehavior::BEHAVIOR_TYPE);
		return {};
	}
	CButtonLabel::EType ParsedLabelType = CButtonLabel::EType::NUM_TYPES;
	for(int CurrentType = (int)CButtonLabel::EType::PLAIN; CurrentType < (int)CButtonLabel::EType::NUM_TYPES; ++CurrentType)
	{
		if(str_comp(LabelType.u.string.ptr, LABEL_TYPE_NAMES[CurrentType]) == 0)
		{
			ParsedLabelType = (CButtonLabel::EType)CurrentType;
			break;
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
		if(LabelType.type != json_string)
		{
			log_error("touch_controls", "Failed to parse touch button behavior of type '%s': failed to parse command at index '%d': attribute 'label-type' must specify a string", CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE, CommandIndex);
			return {};
		}
		CButtonLabel::EType ParsedLabelType = CButtonLabel::EType::NUM_TYPES;
		for(int CurrentType = (int)CButtonLabel::EType::PLAIN; CurrentType < (int)CButtonLabel::EType::NUM_TYPES; ++CurrentType)
		{
			if(str_comp(LabelType.u.string.ptr, LABEL_TYPE_NAMES[CurrentType]) == 0)
			{
				ParsedLabelType = (CButtonLabel::EType)CurrentType;
				break;
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

	pWriter->WriteAttribute("touch-buttons");
	pWriter->BeginArray();
	for(CTouchButton &TouchButton : m_vTouchButtons)
	{
		TouchButton.WriteToConfiguration(pWriter);
	}
	pWriter->EndArray();

	pWriter->EndObject();
}
