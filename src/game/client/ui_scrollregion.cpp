/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_scrollregion.h"

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

CScrollRegion::CScrollRegion()
{
	Reset();
}

void CScrollRegion::Reset()
{
	m_ScrollPos = 0.0f;
	m_ContentSize = 0.0f;
	m_RequestScrollPos = -1.0f;
	m_ScrollDirection = SCROLLRELATIVE_NONE;
	m_ScrollSpeedMultiplier = 1.0f;

	m_AnimTimeMax = 0.0f;
	m_AnimTime = 0.0f;
	m_AnimInitScrollPos = 0.0f;
	m_AnimTargetScrollPos = 0.0f;

	m_ContentAreaRect = m_RailRect = m_LastAddedRect = CUIRect{0.0f, 0.0f, 0.0f, 0.0f};
	m_SliderGrabPos = 0.0f;
	m_ContentScrollOffset = 0.0f;
	m_Params = CScrollRegionParams();
}

void CScrollRegion::Begin(CUIRect *pClipRect, const CScrollRegionParams *pParams)
{
	if(pParams)
		m_Params = *pParams;
	m_ContentAreaRect = *pClipRect;

	CUIRect ScrollbarBg = SplitContentArea();
	DrawBackground(ScrollbarBg);

	if(!ContentOverflows())
		m_ContentScrollOffset = 0.0f;
	m_ContentSize = 0.0f;

	Ui()->ClipEnable(&m_ContentAreaRect);
	*pClipRect = m_ContentAreaRect;
	if(m_Params.m_ScrollHorizontal)
		pClipRect->x += m_ContentScrollOffset;
	else
		pClipRect->y += m_ContentScrollOffset;
}

void CScrollRegion::End()
{
	Ui()->ClipDisable();

	// only show scrollbar if content overflows
	if(!ContentOverflows())
		return;

	DoScrollInput();
	UpdateHotScrollRegion();
	AdvanceAnimation();
	DoSlider();
}

bool CScrollRegion::AddRect(const CUIRect &Rect, bool ShouldScrollHere)
{
	m_LastAddedRect = Rect;
	if(m_Params.m_ScrollHorizontal)
		m_ContentSize = std::max(std::ceil(Rect.x + Rect.w - (m_ContentAreaRect.x + m_ContentScrollOffset)), m_ContentSize);
	else
		m_ContentSize = std::max(std::ceil(Rect.y + Rect.h - (m_ContentAreaRect.y + m_ContentScrollOffset)), m_ContentSize);
	if(ShouldScrollHere)
		ScrollHere();
	return !RectClipped(Rect);
}

void CScrollRegion::ScrollHere(EScrollOption Option)
{
	const float LastAddedPos = m_Params.m_ScrollHorizontal ? m_LastAddedRect.x : m_LastAddedRect.y;
	const float LastAddedSize = m_Params.m_ScrollHorizontal ? m_LastAddedRect.w : m_LastAddedRect.h;
	const float MinHeight = std::min(ContentAreaSize(), LastAddedSize);
	const float TopScroll = LastAddedPos - (ContentAreaPos() + m_ContentScrollOffset);

	switch(Option)
	{
	case SCROLLHERE_TOP:
		m_RequestScrollPos = TopScroll;
		break;

	case SCROLLHERE_BOTTOM:
		m_RequestScrollPos = TopScroll - (ContentAreaSize() - MinHeight);
		break;

	case SCROLLHERE_KEEP_IN_VIEW:
	default:
		const float DeltaPos = LastAddedPos - ContentAreaPos();
		if(DeltaPos < 0)
			m_RequestScrollPos = TopScroll;
		else if(DeltaPos > (ContentAreaSize() - MinHeight))
			m_RequestScrollPos = TopScroll - (ContentAreaSize() - MinHeight);
		break;
	}
}

void CScrollRegion::ScrollRelative(EScrollRelative Direction, float SpeedMultiplier)
{
	m_ScrollDirection = Direction;
	m_ScrollSpeedMultiplier = SpeedMultiplier;
}

void CScrollRegion::ScrollRelativeDirect(vec2 ScrollAmount)
{
	if(m_Params.m_ScrollHorizontal)
		m_RequestScrollPos = std::clamp(m_ScrollPos + ScrollAmount.x, 0.0f, m_ContentSize - ContentAreaSize());
	else
		m_RequestScrollPos = std::clamp(m_ScrollPos + ScrollAmount.y, 0.0f, m_ContentSize - ContentAreaSize());
}

void CScrollRegion::DoEdgeScrolling()
{
	if(!ContentOverflows())
		return;

	const float ScrollBorderSize = 20.0f;
	const float MaxScrollMultiplier = 2.0f;
	const float ScrollSpeedFactor = MaxScrollMultiplier / ScrollBorderSize;
	const float TopScrollPosition = ContentAreaPos() + ScrollBorderSize;
	const float BottomScrollPosition = ContentAreaPos() + ContentAreaSize() - ScrollBorderSize;
	const float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	if(MousePos < TopScrollPosition)
		ScrollRelative(SCROLLRELATIVE_UP, std::min(MaxScrollMultiplier, (TopScrollPosition - MousePos) * ScrollSpeedFactor));
	else if(MousePos > BottomScrollPosition)
		ScrollRelative(SCROLLRELATIVE_DOWN, std::min(MaxScrollMultiplier, (MousePos - BottomScrollPosition) * ScrollSpeedFactor));
}

bool CScrollRegion::RectClipped(const CUIRect &Rect) const
{
	return (m_ContentAreaRect.x > (Rect.x + Rect.w) || (m_ContentAreaRect.x + m_ContentAreaRect.w) < Rect.x || m_ContentAreaRect.y > (Rect.y + Rect.h) || (m_ContentAreaRect.y + m_ContentAreaRect.h) < Rect.y);
}

bool CScrollRegion::ContentOverflows() const
{
	return m_Params.m_ScrollHorizontal ? m_ContentSize > m_ContentAreaRect.w : m_ContentSize > m_ContentAreaRect.h;
}

bool CScrollRegion::ScrollbarShown() const
{
	return ContentOverflows() || m_Params.m_ForceShowScrollbar;
}

bool CScrollRegion::Animating() const
{
	return m_AnimTime > 0.0f;
}

bool CScrollRegion::Active() const
{
	return Ui()->ActiveItem() == &m_ScrollPos;
}

float CScrollRegion::ContentAreaPos() const
{
	return m_Params.m_ScrollHorizontal ? m_ContentAreaRect.x : m_ContentAreaRect.y;
}

float CScrollRegion::ContentAreaSize() const
{
	return m_Params.m_ScrollHorizontal ? m_ContentAreaRect.w : m_ContentAreaRect.h;
}

float CScrollRegion::MaxScroll() const
{
	return m_ContentSize - ContentAreaSize();
}

CUIRect CScrollRegion::SplitContentArea()
{
	CUIRect ScrollbarBg;
	if(m_Params.m_ScrollHorizontal)
		m_ContentAreaRect.HSplitBottom(m_Params.m_ScrollbarThickness, ScrollbarShown() ? &m_ContentAreaRect : nullptr, &ScrollbarBg);
	else
		m_ContentAreaRect.VSplitRight(m_Params.m_ScrollbarThickness, ScrollbarShown() ? &m_ContentAreaRect : nullptr, &ScrollbarBg);
	if(m_Params.m_ScrollbarNoOuterMargin)
	{
		if(m_Params.m_ScrollHorizontal)
		{
			ScrollbarBg.VMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.HSplitTop(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
		else
		{
			ScrollbarBg.HMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.VSplitLeft(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
	}
	else
	{
		ScrollbarBg.Margin(m_Params.m_ScrollbarMargin, &m_RailRect);
	}

	return ScrollbarBg;
}

void CScrollRegion::DrawBackground(const CUIRect &ScrollbarBg)
{
	// only show scrollbar if required
	if(ScrollbarShown())
	{
		if(m_Params.m_ScrollbarBgColor.a > 0.0f)
		{
			int Corners = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_B : IGraphics::CORNER_R;
			ScrollbarBg.Draw(m_Params.m_ScrollbarBgColor, Corners, 4.0f);
		}
		if(m_Params.m_RailBgColor.a > 0.0f)
		{
			float Rounding = m_Params.m_ScrollHorizontal ? m_RailRect.h / 2.0f : m_RailRect.w / 2.0f;
			m_RailRect.Draw(m_Params.m_RailBgColor, IGraphics::CORNER_ALL, Rounding);
		}
	}
	if(m_Params.m_ClipBgColor.a > 0.0f)
	{
		int CornersPartial = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_T : IGraphics::CORNER_L;
		m_ContentAreaRect.Draw(m_Params.m_ClipBgColor, ScrollbarShown() ? CornersPartial : IGraphics::CORNER_ALL, 4.0f);
	}
}

void CScrollRegion::DoScrollInput()
{
	if(m_ScrollDirection != SCROLLRELATIVE_NONE || Ui()->HotScrollRegion() == this)
	{
		bool ProgrammaticScroll = false;
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP))
			m_ScrollDirection = SCROLLRELATIVE_UP;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN))
			m_ScrollDirection = SCROLLRELATIVE_DOWN;
		else
			ProgrammaticScroll = true;

		if(!ProgrammaticScroll)
			m_ScrollSpeedMultiplier = 1.0f;

		if(m_ScrollDirection != SCROLLRELATIVE_NONE)
		{
			const bool IsPageScroll = Input()->AltIsPressed();
			const float ScrollUnit = IsPageScroll && !ProgrammaticScroll ? ContentAreaSize() : m_Params.m_ScrollUnit;

			m_AnimTimeMax = g_Config.m_UiSmoothScrollTime / 1000.0f;
			m_AnimTime = m_AnimTimeMax;
			m_AnimInitScrollPos = m_ScrollPos;
			m_AnimTargetScrollPos = (ProgrammaticScroll ? m_ScrollPos : m_AnimTargetScrollPos) + (int)m_ScrollDirection * ScrollUnit * m_ScrollSpeedMultiplier;
			m_ScrollDirection = SCROLLRELATIVE_NONE;
			m_ScrollSpeedMultiplier = 1.0f;
		}
	}
}

void CScrollRegion::UpdateHotScrollRegion()
{
	CUIRect RegionRect = m_ContentAreaRect;
	if(m_Params.m_ScrollHorizontal)
		RegionRect.h += m_Params.m_ScrollbarThickness;
	else
		RegionRect.w += m_Params.m_ScrollbarThickness;

	if(Ui()->Enabled() && Ui()->MouseHovered(&RegionRect))
	{
		Ui()->SetHotScrollRegion(this);
	}
}

void CScrollRegion::AdvanceAnimation()
{
	if(m_RequestScrollPos >= 0.0f)
	{
		m_AnimTargetScrollPos = m_RequestScrollPos;
		m_AnimTime = 0.0f;
		m_RequestScrollPos = -1.0f;
	}

	m_AnimTargetScrollPos = std::clamp(m_AnimTargetScrollPos, 0.0f, MaxScroll());

	if(absolute(m_AnimInitScrollPos - m_AnimTargetScrollPos) < 0.5f)
		m_AnimTime = 0.0f;

	if(m_AnimTime > 0.0f)
	{
		m_AnimTime -= Client()->RenderFrameTime();
		if(m_AnimTime < 0.0f)
		{
			m_AnimTime = 0.0f;
		}
		float AnimProgress = (1.0f - std::pow(m_AnimTime / m_AnimTimeMax, 3.0f)); // cubic ease out
		m_ScrollPos = m_AnimInitScrollPos + (m_AnimTargetScrollPos - m_AnimInitScrollPos) * AnimProgress;
	}
	else
	{
		m_ScrollPos = m_AnimTargetScrollPos;
	}
}

void CScrollRegion::DoSlider()
{
	const float RailSize = m_Params.m_ScrollHorizontal ? m_RailRect.w : m_RailRect.h;
	const float SliderSize = std::max(m_Params.m_SliderMinSize, ContentAreaSize() / m_ContentSize * RailSize);

	CUIRect Slider = m_RailRect;
	float &SliderPos = m_Params.m_ScrollHorizontal ? Slider.x : Slider.y;
	if(m_Params.m_ScrollHorizontal)
		Slider.w = SliderSize;
	else
		Slider.h = SliderSize;

	const float MaxSlider = RailSize - SliderSize;

	SliderPos += m_ScrollPos / MaxScroll() * MaxSlider;

	const void *pId = &m_ScrollPos;
	const float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	const bool WasActive = Ui()->ActiveItem() == pId;
	Ui()->DoButtonLogic(pId, 0, &m_RailRect, BUTTONFLAG_LEFT); // Result ignored, we only care about the button becoming and being active
	if(Ui()->CheckActiveItem(pId))
	{
		if(!WasActive)
		{
			m_SliderGrabPos = Ui()->MouseHovered(&Slider) ? (MousePos - SliderPos) : (SliderSize / 2.0f);
			m_SliderGrabPos = std::clamp(m_SliderGrabPos, 0.0f, SliderSize);
		}
		m_ScrollPos += (MousePos - (SliderPos + m_SliderGrabPos)) / MaxSlider * MaxScroll();
		m_AnimTargetScrollPos = m_ScrollPos;
		m_AnimTime = 0.0f;
	}

	m_ScrollPos = std::clamp(m_ScrollPos, 0.0f, MaxScroll());
	m_ContentScrollOffset = -m_ScrollPos;

	const float Rounding = m_Params.m_ScrollHorizontal ? Slider.h / 2.0f : Slider.w / 2.0f;
	Slider.Draw(m_Params.SliderColor(Ui()->CheckActiveItem(pId), Ui()->HotItem() == pId), IGraphics::CORNER_ALL, Rounding);
}
