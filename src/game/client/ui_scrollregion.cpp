/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include "ui_scrollregion.h"

CScrollRegion::CScrollRegion()
{
	m_ScrollY = 0.0f;
	m_ContentH = 0.0f;
	m_RequestScrollY = -1.0f;
	m_ScrollDirection = SCROLLRELATIVE_NONE;
	m_ScrollSpeedMultiplier = 1.0f;

	m_AnimTimeMax = 0.0f;
	m_AnimTime = 0.0f;
	m_AnimInitScrollY = 0.0f;
	m_AnimTargetScrollY = 0.0f;

	m_SliderGrabPos = 0.0f;
	m_ContentScrollOff = vec2(0.0f, 0.0f);
	m_Params = CScrollRegionParams();
}

void CScrollRegion::Begin(CUIRect *pClipRect, vec2 *pOutOffset, const CScrollRegionParams *pParams)
{
	if(pParams)
		m_Params = *pParams;

	const bool ContentOverflows = m_ContentH > pClipRect->h;
	const bool ForceShowScrollbar = m_Params.m_Flags & CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;

	const bool HasScrollBar = ContentOverflows || ForceShowScrollbar;
	CUIRect ScrollBarBg;
	pClipRect->VSplitRight(m_Params.m_ScrollbarWidth, HasScrollBar ? pClipRect : nullptr, &ScrollBarBg);
	if(m_Params.m_ScrollbarNoMarginRight)
	{
		ScrollBarBg.HMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
		m_RailRect.VSplitLeft(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
	}
	else
		ScrollBarBg.Margin(m_Params.m_ScrollbarMargin, &m_RailRect);

	// only show scrollbar if required
	if(HasScrollBar)
	{
		if(m_Params.m_ScrollbarBgColor.a > 0.0f)
			ScrollBarBg.Draw(m_Params.m_ScrollbarBgColor, IGraphics::CORNER_R, 4.0f);
		if(m_Params.m_RailBgColor.a > 0.0f)
			m_RailRect.Draw(m_Params.m_RailBgColor, IGraphics::CORNER_ALL, m_RailRect.w / 2.0f);
	}
	if(!ContentOverflows)
		m_ContentScrollOff.y = 0.0f;

	if(m_Params.m_ClipBgColor.a > 0.0f)
		pClipRect->Draw(m_Params.m_ClipBgColor, HasScrollBar ? IGraphics::CORNER_L : IGraphics::CORNER_ALL, 4.0f);

	Ui()->ClipEnable(pClipRect);

	m_ClipRect = *pClipRect;
	m_ContentH = 0.0f;
	*pOutOffset = m_ContentScrollOff;
}

void CScrollRegion::End()
{
	Ui()->ClipDisable();

	// only show scrollbar if content overflows
	if(m_ContentH <= m_ClipRect.h)
		return;

	// scroll wheel
	CUIRect RegionRect = m_ClipRect;
	RegionRect.w += m_Params.m_ScrollbarWidth;

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
			const float ScrollUnit = IsPageScroll && !ProgrammaticScroll ? m_ClipRect.h : m_Params.m_ScrollUnit;

			m_AnimTimeMax = g_Config.m_UiSmoothScrollTime / 1000.0f;
			m_AnimTime = m_AnimTimeMax;
			m_AnimInitScrollY = m_ScrollY;
			m_AnimTargetScrollY = (ProgrammaticScroll ? m_ScrollY : m_AnimTargetScrollY) + (int)m_ScrollDirection * ScrollUnit * m_ScrollSpeedMultiplier;
			m_ScrollDirection = SCROLLRELATIVE_NONE;
			m_ScrollSpeedMultiplier = 1.0f;
		}
	}

	if(Ui()->Enabled() && Ui()->MouseHovered(&RegionRect))
	{
		Ui()->SetHotScrollRegion(this);
	}

	const float SliderHeight = maximum(m_Params.m_SliderMinHeight, m_ClipRect.h / m_ContentH * m_RailRect.h);

	CUIRect Slider = m_RailRect;
	Slider.h = SliderHeight;

	const float MaxSlider = m_RailRect.h - SliderHeight;
	const float MaxScroll = m_ContentH - m_ClipRect.h;

	if(m_RequestScrollY >= 0.0f)
	{
		m_AnimTargetScrollY = m_RequestScrollY;
		m_AnimTime = 0.0f;
		m_RequestScrollY = -1.0f;
	}

	m_AnimTargetScrollY = clamp(m_AnimTargetScrollY, 0.0f, MaxScroll);

	if(absolute(m_AnimInitScrollY - m_AnimTargetScrollY) < 0.5f)
		m_AnimTime = 0.0f;

	if(m_AnimTime > 0.0f)
	{
		m_AnimTime -= Client()->RenderFrameTime();
		float AnimProgress = (1.0f - std::pow(m_AnimTime / m_AnimTimeMax, 3.0f)); // cubic ease out
		m_ScrollY = m_AnimInitScrollY + (m_AnimTargetScrollY - m_AnimInitScrollY) * AnimProgress;
	}
	else
	{
		m_ScrollY = m_AnimTargetScrollY;
	}

	Slider.y += m_ScrollY / MaxScroll * MaxSlider;

	bool Grabbed = false;
	const void *pId = &m_ScrollY;
	const bool InsideSlider = Ui()->MouseHovered(&Slider);
	const bool InsideRail = Ui()->MouseHovered(&m_RailRect);

	if(Ui()->CheckActiveItem(pId) && Ui()->MouseButton(0))
	{
		float MouseY = Ui()->MouseY();
		m_ScrollY += (MouseY - (Slider.y + m_SliderGrabPos)) / MaxSlider * MaxScroll;
		m_SliderGrabPos = clamp(m_SliderGrabPos, 0.0f, SliderHeight);
		m_AnimTargetScrollY = m_ScrollY;
		m_AnimTime = 0.0f;
		Grabbed = true;
	}
	else if(InsideSlider)
	{
		if(!Ui()->MouseButton(0))
			Ui()->SetHotItem(pId);

		if(!Ui()->CheckActiveItem(pId) && Ui()->MouseButtonClicked(0))
		{
			Ui()->SetActiveItem(pId);
			m_SliderGrabPos = Ui()->MouseY() - Slider.y;
			m_AnimTargetScrollY = m_ScrollY;
			m_AnimTime = 0.0f;
		}
	}
	else if(InsideRail && Ui()->MouseButtonClicked(0))
	{
		m_ScrollY += (Ui()->MouseY() - (Slider.y + Slider.h / 2.0f)) / MaxSlider * MaxScroll;
		Ui()->SetHotItem(pId);
		Ui()->SetActiveItem(pId);
		m_SliderGrabPos = Slider.h / 2.0f;
		m_AnimTargetScrollY = m_ScrollY;
		m_AnimTime = 0.0f;
	}

	if(Ui()->CheckActiveItem(pId) && !Ui()->MouseButton(0))
	{
		Ui()->SetActiveItem(nullptr);
	}

	m_ScrollY = clamp(m_ScrollY, 0.0f, MaxScroll);
	m_ContentScrollOff.y = -m_ScrollY;

	Slider.Draw(m_Params.SliderColor(Grabbed, Ui()->HotItem() == pId), IGraphics::CORNER_ALL, Slider.w / 2.0f);
}

bool CScrollRegion::AddRect(const CUIRect &Rect, bool ShouldScrollHere)
{
	m_LastAddedRect = Rect;
	// Round up and add magic to fix pixel clipping at the end of the scrolling area
	m_ContentH = maximum(std::ceil(Rect.y + Rect.h - (m_ClipRect.y + m_ContentScrollOff.y)) + HEIGHT_MAGIC_FIX, m_ContentH);
	if(ShouldScrollHere)
		ScrollHere();
	return !RectClipped(Rect);
}

void CScrollRegion::ScrollHere(EScrollOption Option)
{
	const float MinHeight = minimum(m_ClipRect.h, m_LastAddedRect.h);
	const float TopScroll = m_LastAddedRect.y - (m_ClipRect.y + m_ContentScrollOff.y);

	switch(Option)
	{
	case SCROLLHERE_TOP:
		m_RequestScrollY = TopScroll;
		break;

	case SCROLLHERE_BOTTOM:
		m_RequestScrollY = TopScroll - (m_ClipRect.h - MinHeight);
		break;

	case SCROLLHERE_KEEP_IN_VIEW:
	default:
		const float DeltaY = m_LastAddedRect.y - m_ClipRect.y;
		if(DeltaY < 0)
			m_RequestScrollY = TopScroll;
		else if(DeltaY > (m_ClipRect.h - MinHeight))
			m_RequestScrollY = TopScroll - (m_ClipRect.h - MinHeight);
		break;
	}
}

void CScrollRegion::ScrollRelative(EScrollRelative Direction, float SpeedMultiplier)
{
	m_ScrollDirection = Direction;
	m_ScrollSpeedMultiplier = SpeedMultiplier;
}

void CScrollRegion::ScrollRelativeDirect(float ScrollAmount)
{
	m_RequestScrollY = clamp(m_ScrollY + ScrollAmount, 0.0f, m_ContentH - m_ClipRect.h);
}

void CScrollRegion::DoEdgeScrolling()
{
	if(!ScrollbarShown())
		return;

	const float ScrollBorderSize = 20.0f;
	const float MaxScrollMultiplier = 2.0f;
	const float ScrollSpeedFactor = MaxScrollMultiplier / ScrollBorderSize;
	const float TopScrollPosition = m_ClipRect.y + ScrollBorderSize;
	const float BottomScrollPosition = m_ClipRect.y + m_ClipRect.h - ScrollBorderSize;
	if(Ui()->MouseY() < TopScrollPosition)
		ScrollRelative(SCROLLRELATIVE_UP, minimum(MaxScrollMultiplier, (TopScrollPosition - Ui()->MouseY()) * ScrollSpeedFactor));
	else if(Ui()->MouseY() > BottomScrollPosition)
		ScrollRelative(SCROLLRELATIVE_DOWN, minimum(MaxScrollMultiplier, (Ui()->MouseY() - BottomScrollPosition) * ScrollSpeedFactor));
}

bool CScrollRegion::RectClipped(const CUIRect &Rect) const
{
	return (m_ClipRect.x > (Rect.x + Rect.w) || (m_ClipRect.x + m_ClipRect.w) < Rect.x || m_ClipRect.y > (Rect.y + Rect.h) || (m_ClipRect.y + m_ClipRect.h) < Rect.y);
}

bool CScrollRegion::ScrollbarShown() const
{
	return m_ContentH > m_ClipRect.h;
}

bool CScrollRegion::Animating() const
{
	return m_AnimTime > 0.0f;
}

bool CScrollRegion::Active() const
{
	return Ui()->ActiveItem() == &m_ScrollY;
}
