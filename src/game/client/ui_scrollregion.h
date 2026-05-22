/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_SCROLLREGION_H
#define GAME_CLIENT_UI_SCROLLREGION_H

#include "ui.h"

struct CScrollRegionParams
{
	float m_ScrollbarThickness;
	float m_ScrollbarMargin;
	bool m_ScrollbarNoOuterMargin;
	float m_SliderMinSize;
	float m_ScrollUnit;
	ColorRGBA m_ClipBgColor;
	ColorRGBA m_ScrollbarBgColor;
	ColorRGBA m_RailBgColor;
	ColorRGBA m_SliderColor;
	ColorRGBA m_SliderColorHover;
	ColorRGBA m_SliderColorGrabbed;
	bool m_ForceShowScrollbar;
	bool m_ScrollHorizontal;

	CScrollRegionParams()
	{
		m_ScrollbarThickness = 20.0f;
		m_ScrollbarMargin = 5.0f;
		m_ScrollbarNoOuterMargin = false;
		m_SliderMinSize = 25.0f;
		m_ScrollUnit = 10.0f;
		m_ClipBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_ScrollbarBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_RailBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);
		m_SliderColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
		m_SliderColorHover = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_SliderColorGrabbed = ColorRGBA(0.9f, 0.9f, 0.9f, 1.0f);
		m_ForceShowScrollbar = false;
		m_ScrollHorizontal = false;
	}

	ColorRGBA SliderColor(bool Active, bool Hovered) const
	{
		if(Active)
			return m_SliderColorGrabbed;
		else if(Hovered)
			return m_SliderColorHover;
		return m_SliderColor;
	}
};

/*
Usage example:

	// -- Layout --
	CUIRect View = ...; // parent UI rect initialized elsewhere
	CUIRect Content; // rect for scrollable content
	View.HSplitTop(500.0f, &Content, &View); // split maximum size of scrollable content

	// -- Initialization --
	static CScrollRegion s_ScrollRegion;
	s_ScrollRegion.Begin(&Content);
	// Content rect is now offset by the scroll offset

	// -- [Optional] Initialization with parameters --
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 3 * LineHeight;
	s_ScrollRegion.Begin(&Content, &ScrollParams);
	// Content rect is now offset by the scroll offset

	// -- "Register" your content rects --
	CUIRect Rect;
	Content.HSplitTop(SomeValue, &Rect, &Content);
	s_ScrollRegion.AddRect(Rect);

	// -- [Optional] Knowing if a rect is clipped --
	s_ScrollRegion.RectClipped(Rect);

	// -- [Optional] Scroll to the last added rect --
	s_ScrollRegion.AddRect(Rect);
	s_ScrollRegion.ScrollHere(Option);

	// -- [Convenience] Add rect and check for visibility at the same time --
	if(s_ScrollRegion.AddRect(Rect))
	{
		// The rect is visible (not clipped)
	}

	// -- [Convenience] Add rect and scroll to it if it's selected --
	if(s_ScrollRegion.AddRect(Rect, ScrollToSelection && IsSelected))
	{
		// The rect is visible (not clipped)
	}

	// -- End --
	s_ScrollRegion.End();
*/

// Instances of CScrollRegion must be static, as member addresses are used as UI item IDs
class CScrollRegion : private CUIElementBase
{
public:
	enum EScrollRelative
	{
		SCROLLRELATIVE_UP = -1,
		SCROLLRELATIVE_NONE = 0,
		SCROLLRELATIVE_DOWN = 1,
	};

private:
	float m_ScrollPos;
	float m_ContentSize;
	float m_RequestScrollPos; // [0, ContentSize]
	EScrollRelative m_ScrollDirection;
	float m_ScrollSpeedMultiplier;

	float m_AnimTimeMax;
	float m_AnimTime;
	float m_AnimInitScrollPos;
	float m_AnimTargetScrollPos;

	CUIRect m_ContentAreaRect;
	CUIRect m_RailRect;
	CUIRect m_LastAddedRect; // saved for ScrollHere()
	float m_SliderGrabPos; // where did user grab the slider
	float m_ContentScrollOffset;
	CScrollRegionParams m_Params;

public:
	enum EScrollOption
	{
		SCROLLHERE_KEEP_IN_VIEW = 0,
		SCROLLHERE_TOP,
		SCROLLHERE_BOTTOM,
	};

	CScrollRegion();
	void Reset();

	void Begin(CUIRect *pClipRect, const CScrollRegionParams *pParams = nullptr);
	void End();
	bool AddRect(const CUIRect &Rect, bool ShouldScrollHere = false); // returns true if the added rect is visible (not clipped)
	void ScrollHere(EScrollOption Option = SCROLLHERE_KEEP_IN_VIEW);
	void ScrollRelative(EScrollRelative Direction, float SpeedMultiplier = 1.0f);
	void ScrollRelativeDirect(vec2 ScrollAmount);
	void DoEdgeScrolling();
	bool RectClipped(const CUIRect &Rect) const;
	bool ContentOverflows() const;
	bool ScrollbarShown() const;
	bool Animating() const;
	bool Active() const;

private:
	float ContentAreaPos() const;
	float ContentAreaSize() const;
	float MaxScroll() const;
	CUIRect SplitContentArea();
	void DrawBackground(const CUIRect &ScrollbarBg);
	void DoScrollInput();
	void UpdateHotScrollRegion();
	void AdvanceAnimation();
	void DoSlider();
};

#endif
