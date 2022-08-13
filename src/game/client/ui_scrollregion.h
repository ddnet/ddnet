/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_SCROLLREGION_H
#define GAME_CLIENT_UI_SCROLLREGION_H

#include "ui.h"

struct CScrollRegionParams
{
	float m_ScrollbarWidth;
	float m_ScrollbarMargin;
	float m_SliderMinHeight;
	float m_ScrollUnit;
	ColorRGBA m_ClipBgColor;
	ColorRGBA m_ScrollbarBgColor;
	ColorRGBA m_RailBgColor;
	ColorRGBA m_SliderColor;
	ColorRGBA m_SliderColorHover;
	ColorRGBA m_SliderColorGrabbed;
	unsigned m_Flags;

	enum
	{
		FLAG_CONTENT_STATIC_WIDTH = 1 << 0,
	};

	CScrollRegionParams()
	{
		m_ScrollbarWidth = 20.0f;
		m_ScrollbarMargin = 5.0f;
		m_SliderMinHeight = 25.0f;
		m_ScrollUnit = 10.0f;
		m_ClipBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_ScrollbarBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		m_RailBgColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);
		m_SliderColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
		m_SliderColorHover = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_SliderColorGrabbed = ColorRGBA(0.9f, 0.9f, 0.9f, 1.0f);
		m_Flags = 0;
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
Usage:
	-- Initialization --
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0, 0);
	s_ScrollRegion.Begin(&ScrollRegionRect, &ScrollOffset);
	Content = ScrollRegionRect;
	Content.y += ScrollOffset.y;

	-- "Register" your content rects --
	CUIRect Rect;
	Content.HSplitTop(SomeValue, &Rect, &Content);
	s_ScrollRegion.AddRect(Rect);

	-- [Optional] Knowing if a rect is clipped --
	s_ScrollRegion.IsRectClipped(Rect);

	-- [Optional] Scroll to a rect (to the last added rect)--
	...
	s_ScrollRegion.AddRect(Rect);
	s_ScrollRegion.ScrollHere(Option);

	-- [Convenience] Add rect and check for visibility at the same time
	if(s_ScrollRegion.AddRect(Rect))
		// The rect is visible (not clipped)

	-- [Convenience] Add rect and scroll to it if it's selected
	if(s_ScrollRegion.AddRect(Rect, ScrollToSelection && IsSelected))
		// The rect is visible (not clipped)

	-- End --
	s_ScrollRegion.End();
*/

// Instances of CScrollRegion must be static, as member addresses are used as UI item IDs
class CScrollRegion : private CUIElementBase
{
private:
	float m_ScrollY;
	float m_ContentH;
	float m_RequestScrollY; // [0, ContentHeight]

	float m_AnimTime;
	float m_AnimInitScrollY;
	float m_AnimTargetScrollY;

	CUIRect m_ClipRect;
	CUIRect m_RailRect;
	CUIRect m_LastAddedRect; // saved for ScrollHere()
	vec2 m_SliderGrabPos; // where did user grab the slider
	vec2 m_ContentScrollOff;
	CScrollRegionParams m_Params;

public:
	enum EScrollOption
	{
		SCROLLHERE_KEEP_IN_VIEW = 0,
		SCROLLHERE_TOP,
		SCROLLHERE_BOTTOM,
	};

	CScrollRegion();
	void Begin(CUIRect *pClipRect, vec2 *pOutOffset, CScrollRegionParams *pParams = nullptr);
	void End();
	bool AddRect(const CUIRect &Rect, bool ShouldScrollHere = false); // returns true if the added rect is visible (not clipped)
	void ScrollHere(EScrollOption Option = SCROLLHERE_KEEP_IN_VIEW);
	bool IsRectClipped(const CUIRect &Rect) const;
	bool IsScrollbarShown() const;
	bool IsAnimating() const;
};

#endif
