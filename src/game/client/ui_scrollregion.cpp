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

	m_ClipRect = m_RailRect = m_LastAddedRect = CUIRect{0.0f, 0.0f, 0.0f, 0.0f};
	m_SliderGrabPos = 0.0f;
	m_ContentScrollOffset = 0.0f;
	m_Params = CScrollRegionParams();
}

void CScrollRegion::Begin(CUIRect *pClipRect, const CScrollRegionParams *pParams)
{
	if(pParams)
		m_Params = *pParams;
	m_ClipRect = *pClipRect;

	const bool ContentOverflows = m_Params.m_ScrollHorizontal ? m_ContentSize > m_ClipRect.w : m_ContentSize > m_ClipRect.h;
	const bool HasScrollBar = ContentOverflows || m_Params.m_ForceShowScrollbar;
	CUIRect ScrollBarBg;
	if(m_Params.m_ScrollHorizontal)
		m_ClipRect.HSplitBottom(m_Params.m_ScrollbarThickness, HasScrollBar ? &m_ClipRect : nullptr, &ScrollBarBg);
	else
		m_ClipRect.VSplitRight(m_Params.m_ScrollbarThickness, HasScrollBar ? &m_ClipRect : nullptr, &ScrollBarBg);
	if(m_Params.m_ScrollbarNoOuterMargin)
	{
		if(m_Params.m_ScrollHorizontal)
		{
			ScrollBarBg.VMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.HSplitTop(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
		else
		{
			ScrollBarBg.HMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.VSplitLeft(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
	}
	else
		ScrollBarBg.Margin(m_Params.m_ScrollbarMargin, &m_RailRect);

	// only show scrollbar if required
	if(HasScrollBar)
	{
		if(m_Params.m_ScrollbarBgColor.a > 0.0f)
		{
			int Corners = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_B : IGraphics::CORNER_R;
			ScrollBarBg.Draw(m_Params.m_ScrollbarBgColor, Corners, 4.0f);
		}
		if(m_Params.m_RailBgColor.a > 0.0f)
		{
			float Rounding = m_Params.m_ScrollHorizontal ? m_RailRect.h / 2.0f : m_RailRect.w / 2.0f;
			m_RailRect.Draw(m_Params.m_RailBgColor, IGraphics::CORNER_ALL, Rounding);
		}
	}
	if(!ContentOverflows)
		m_ContentScrollOffset = 0.0f;

	if(m_Params.m_ClipBgColor.a > 0.0f)
	{
		int CornersPartial = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_T : IGraphics::CORNER_L;
		m_ClipRect.Draw(m_Params.m_ClipBgColor, HasScrollBar ? CornersPartial : IGraphics::CORNER_ALL, 4.0f);
	}

	m_ContentSize = 0.0f;

	Ui()->ClipEnable(&m_ClipRect);
	*pClipRect = m_ClipRect;
	if(m_Params.m_ScrollHorizontal)
		pClipRect->x += m_ContentScrollOffset;
	else
		pClipRect->y += m_ContentScrollOffset;
}

void CScrollRegion::End()
{
	Ui()->ClipDisable();

	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;

	// only show scrollbar if content overflows
	if(m_ContentSize <= ClipSize)
		return;

	// scroll wheel
	CUIRect RegionRect = m_ClipRect;
	if(m_Params.m_ScrollHorizontal)
		RegionRect.h += m_Params.m_ScrollbarThickness;
	else
		RegionRect.w += m_Params.m_ScrollbarThickness;

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
			const float ScrollUnit = IsPageScroll && !ProgrammaticScroll ? ClipSize : m_Params.m_ScrollUnit;

			m_AnimTimeMax = g_Config.m_UiSmoothScrollTime / 1000.0f;
			m_AnimTime = m_AnimTimeMax;
			m_AnimInitScrollPos = m_ScrollPos;
			m_AnimTargetScrollPos = (ProgrammaticScroll ? m_ScrollPos : m_AnimTargetScrollPos) + (int)m_ScrollDirection * ScrollUnit * m_ScrollSpeedMultiplier;
			m_ScrollDirection = SCROLLRELATIVE_NONE;
			m_ScrollSpeedMultiplier = 1.0f;
		}
	}

	if(Ui()->Enabled() && Ui()->MouseHovered(&RegionRect))
	{
		Ui()->SetHotScrollRegion(this);
	}

	const float RailSize = m_Params.m_ScrollHorizontal ? m_RailRect.w : m_RailRect.h;
	const float SliderSize = maximum(m_Params.m_SliderMinSize, ClipSize / m_ContentSize * RailSize);

	CUIRect Slider = m_RailRect;
	float &SliderPos = m_Params.m_ScrollHorizontal ? Slider.x : Slider.y;
	if(m_Params.m_ScrollHorizontal)
		Slider.w = SliderSize;
	else
		Slider.h = SliderSize;

	const float MaxSlider = RailSize - SliderSize;
	const float MaxScroll = m_ContentSize - ClipSize;

	if(m_RequestScrollPos >= 0.0f)
	{
		m_AnimTargetScrollPos = m_RequestScrollPos;
		m_AnimTime = 0.0f;
		m_RequestScrollPos = -1.0f;
	}

	m_AnimTargetScrollPos = std::clamp(m_AnimTargetScrollPos, 0.0f, MaxScroll);

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

	SliderPos += m_ScrollPos / MaxScroll * MaxSlider;

	bool Grabbed = false;
	const void *pId = &m_ScrollPos;
	const bool InsideSlider = Ui()->MouseHovered(&Slider);
	const bool InsideRail = Ui()->MouseHovered(&m_RailRect);

	float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	if(Ui()->CheckActiveItem(pId) && Ui()->MouseButton(0))
	{
		m_ScrollPos += (MousePos - (SliderPos + m_SliderGrabPos)) / MaxSlider * MaxScroll;
		m_SliderGrabPos = std::clamp(m_SliderGrabPos, 0.0f, SliderSize);
		m_AnimTargetScrollPos = m_ScrollPos;
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
			m_SliderGrabPos = MousePos - SliderPos;
			m_AnimTargetScrollPos = m_ScrollPos;
			m_AnimTime = 0.0f;
		}
	}
	else if(InsideRail && Ui()->MouseButtonClicked(0))
	{
		m_ScrollPos += (MousePos - (SliderPos + SliderSize / 2.0f)) / MaxSlider * MaxScroll;
		Ui()->SetHotItem(pId);
		Ui()->SetActiveItem(pId);
		m_SliderGrabPos = SliderSize / 2.0f;
		m_AnimTargetScrollPos = m_ScrollPos;
		m_AnimTime = 0.0f;
	}

	if(Ui()->CheckActiveItem(pId) && !Ui()->MouseButton(0))
	{
		Ui()->SetActiveItem(nullptr);
	}

	m_ScrollPos = std::clamp(m_ScrollPos, 0.0f, MaxScroll);
	m_ContentScrollOffset = -m_ScrollPos;

	float Rounding = m_Params.m_ScrollHorizontal ? Slider.h / 2.0f : Slider.w / 2.0f;
	Slider.Draw(m_Params.SliderColor(Grabbed, Ui()->HotItem() == pId), IGraphics::CORNER_ALL, Rounding);
}

bool CScrollRegion::AddRect(const CUIRect &Rect, bool ShouldScrollHere)
{
	m_LastAddedRect = Rect;
	if(m_Params.m_ScrollHorizontal)
		m_ContentSize = maximum(std::ceil(Rect.x + Rect.w - (m_ClipRect.x + m_ContentScrollOffset)), m_ContentSize);
	else
		m_ContentSize = maximum(Rect.y + Rect.h - (m_ClipRect.y + m_ContentScrollOffset), m_ContentSize);
	if(ShouldScrollHere)
		ScrollHere();
	return !RectClipped(Rect);
}

void CScrollRegion::ScrollHere(EScrollOption Option)
{
	const float ClipPos = m_Params.m_ScrollHorizontal ? m_ClipRect.x : m_ClipRect.y;
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	const float LastAddedPos = m_Params.m_ScrollHorizontal ? m_LastAddedRect.x : m_LastAddedRect.y;
	const float LastAddedSize = m_Params.m_ScrollHorizontal ? m_LastAddedRect.w : m_LastAddedRect.h;
	const float MinHeight = minimum(ClipSize, LastAddedSize);
	const float TopScroll = LastAddedPos - (ClipPos + m_ContentScrollOffset);

	switch(Option)
	{
	case SCROLLHERE_TOP:
		m_RequestScrollPos = TopScroll;
		break;

	case SCROLLHERE_BOTTOM:
		m_RequestScrollPos = TopScroll - (ClipSize - MinHeight);
		break;

	case SCROLLHERE_KEEP_IN_VIEW:
	default:
		const float DeltaY = LastAddedPos - ClipPos;
		if(DeltaY < 0)
			m_RequestScrollPos = TopScroll;
		else if(DeltaY > (ClipSize - MinHeight))
			m_RequestScrollPos = TopScroll - (ClipSize - MinHeight);
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
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	m_RequestScrollPos = std::clamp(m_ScrollPos + ScrollAmount, 0.0f, m_ContentSize - ClipSize);
}

void CScrollRegion::DoEdgeScrolling()
{
	if(!ContentOverflows())
		return;

	const float ClipPos = m_Params.m_ScrollHorizontal ? m_ClipRect.x : m_ClipRect.y;
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	const float ScrollBorderSize = 20.0f;
	const float MaxScrollMultiplier = 2.0f;
	const float ScrollSpeedFactor = MaxScrollMultiplier / ScrollBorderSize;
	const float TopScrollPosition = ClipPos + ScrollBorderSize;
	const float BottomScrollPosition = ClipPos + ClipSize - ScrollBorderSize;
	const float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	if(MousePos < TopScrollPosition)
		ScrollRelative(SCROLLRELATIVE_UP, minimum(MaxScrollMultiplier, (TopScrollPosition - MousePos) * ScrollSpeedFactor));
	else if(MousePos > BottomScrollPosition)
		ScrollRelative(SCROLLRELATIVE_DOWN, minimum(MaxScrollMultiplier, (MousePos - BottomScrollPosition) * ScrollSpeedFactor));
}

bool CScrollRegion::RectClipped(const CUIRect &Rect) const
{
	return (m_ClipRect.x > (Rect.x + Rect.w) || (m_ClipRect.x + m_ClipRect.w) < Rect.x || m_ClipRect.y > (Rect.y + Rect.h) || (m_ClipRect.y + m_ClipRect.h) < Rect.y);
}

bool CScrollRegion::ContentOverflows() const
{
	return m_Params.m_ScrollHorizontal ? m_ContentSize > m_ClipRect.w : m_ContentSize > m_ClipRect.h;
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
