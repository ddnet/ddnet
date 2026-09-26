#include "tooltips.h"

#include <base/dbg.h>
#include <base/time.h>

#include <game/client/ui.h>

#include <limits>

CTooltips::CTooltips()
{
	CTooltips::OnReset();
}

void CTooltips::OnReset()
{
	m_HoverTime = -1;
	m_Tooltips.clear();
	ClearActiveTooltip();
}

void CTooltips::SetActiveTooltip(CTooltip &Tooltip)
{
	m_ActiveTooltip.emplace(Tooltip);
}

inline void CTooltips::ClearActiveTooltip()
{
	m_ActiveTooltip.reset();
	m_PreviousTooltip.reset();
}

void CTooltips::DoTruncationToolTip(const void *pId, const CUIRect *pNearRect, const char *pText, float FontSize, float WidthHint)
{
	DoToolTipImpl(pId, pNearRect, pText, FontSize, WidthHint, true);
}

void CTooltips::DoToolTip(const void *pId, const CUIRect *pNearRect, const char *pText, float WidthHint)
{
	constexpr float DefaultFontSize = 14.0f;
	DoToolTipImpl(pId, pNearRect, pText, DefaultFontSize, WidthHint, false);
}

void CTooltips::DoToolTipImpl(const void *pId, const CUIRect *pNearRect, const char *pText, float FontSize, float WidthHint, bool Truncated)
{
	uintptr_t Id = reinterpret_cast<uintptr_t>(pId);
	const auto &[Entry, WasInserted] = m_Tooltips.emplace(Id, CTooltip{
									  pId,
									  *pNearRect,
									  pText,
									  WidthHint,
									  FontSize,
									  Truncated});
	CTooltip &Tooltip = Entry->second;
	if(!WasInserted)
	{
		Tooltip.m_Rect = *pNearRect; // update in case of window resize
		Tooltip.m_pText = pText; // update in case of language change
	}

	Tooltip.m_OnScreen = true;

	if(Ui()->HotItem() == Tooltip.m_pId)
	{
		SetActiveTooltip(Tooltip);
	}
}

void CTooltips::OnRender()
{
	if(m_ActiveTooltip.has_value())
	{
		CTooltip &Tooltip = m_ActiveTooltip.value();

		if(Ui()->HotItem() != Tooltip.m_pId || !Tooltip.m_Rect.Inside(Ui()->MousePos()))
		{
			Tooltip.m_OnScreen = false;
			ClearActiveTooltip();
			return;
		}
		if(!Tooltip.m_OnScreen)
			return;

		// Reset hover time if a different tooltip is active.
		// Only reset hover time when rendering, because multiple tooltips can be
		// activated in the same frame, but only the last one should be rendered.
		if(!m_PreviousTooltip.has_value() || m_PreviousTooltip.value().get().m_pText != Tooltip.m_pText)
			m_HoverTime = time_get();
		m_PreviousTooltip.emplace(Tooltip);

		// Delay tooltip until 1 second passed. Start fade-in in the last 0.25 seconds.
		constexpr float SecondsBeforeFadeIn = 0.75f;
		const float SecondsSinceActivation = (time_get() - m_HoverTime) / (float)time_freq();
		if(SecondsSinceActivation < SecondsBeforeFadeIn)
			return;
		constexpr float SecondsFadeIn = 0.25f;
		const float AlphaFactor = SecondsSinceActivation < SecondsBeforeFadeIn + SecondsFadeIn ? (SecondsSinceActivation - SecondsBeforeFadeIn) / SecondsFadeIn : 1.0f;

		constexpr float Margin = 5.0f;
		constexpr float Padding = 5.0f;

		const CUIRect *pScreen = Ui()->Screen();
		// The x-bearing might not be applied without a width, causing 1 px offset
		const float LineWidth = Tooltip.m_Truncated && Tooltip.m_WidthHint <= 0.0f ? pScreen->w - 4 * Margin : Tooltip.m_WidthHint;

		const STextBoundingBox BoundingBox = TextRender()->TextBoundingBox(Tooltip.m_FontSize, Tooltip.m_pText, -1, LineWidth);
		CUIRect Rect;
		Rect.w = BoundingBox.m_W + 2 * Padding;
		Rect.h = BoundingBox.m_H + 2 * Padding;

		Rect.w = std::min(Rect.w, pScreen->w - 2 * Margin);
		Rect.h = std::min(Rect.h, pScreen->h - 2 * Margin);

		// On top of the current Rect
		if(Tooltip.m_Truncated && Tooltip.m_Rect.x + Rect.w < pScreen->w && Tooltip.m_Rect.y + Rect.h < pScreen->h)
		{
			Rect.x = Tooltip.m_Rect.x;
			Rect.y = Tooltip.m_Rect.y;

			// try to overlay perfectly
			Rect.y += (Tooltip.m_Rect.h - Rect.h) / 2.0f;
			Rect.x -= Margin;
		}
		else if(Tooltip.m_Truncated)
		{
			// the tooltip wants to go offscreen, move it to the left until it matches
			float OffscreenRight = std::max(Tooltip.m_Rect.x + Rect.w - pScreen->w, 0.0f);
			float OffscreenBottom = std::max(Tooltip.m_Rect.y + Rect.h - pScreen->h, 0.0f);

			Rect.x = Tooltip.m_Rect.x;
			Rect.y = Tooltip.m_Rect.y;

			Rect.y += (Tooltip.m_Rect.h - Rect.h) / 2.0f;
			Rect.x -= Margin;

			Rect.x = std::max(0.0f, Rect.x - OffscreenRight);
			Rect.y = std::max(0.0f, Rect.y - OffscreenBottom);
		}
		// Try the top side.
		else if(Tooltip.m_Rect.y - Rect.h - Margin > pScreen->y)
		{
			Rect.x = std::clamp(Ui()->MouseX() - Rect.w / 2.0f, Margin, pScreen->w - Rect.w - Margin);
			Rect.y = Tooltip.m_Rect.y - Rect.h - Margin;
		}
		// Try the bottom side.
		else if(Tooltip.m_Rect.y + Tooltip.m_Rect.h + Margin < pScreen->h)
		{
			Rect.x = std::clamp(Ui()->MouseX() - Rect.w / 2.0f, Margin, pScreen->w - Rect.w - Margin);
			Rect.y = Tooltip.m_Rect.y + Tooltip.m_Rect.h + Margin;
		}
		// Try the right side.
		else if(Tooltip.m_Rect.x + Tooltip.m_Rect.w + Margin + Rect.w < pScreen->w)
		{
			Rect.x = Tooltip.m_Rect.x + Tooltip.m_Rect.w + Margin;
			Rect.y = std::clamp(Ui()->MouseY() - Rect.h / 2.0f, Margin, pScreen->h - Rect.h - Margin);
		}
		// Try the left side.
		else if(Tooltip.m_Rect.x - Rect.w - Margin > pScreen->x)
		{
			Rect.x = Tooltip.m_Rect.x - Rect.w - Margin;
			Rect.y = std::clamp(Ui()->MouseY() - Rect.h / 2.0f, Margin, pScreen->h - Rect.h - Margin);
		}

		Rect.Draw(ColorRGBA(0.2f, 0.2f, 0.2f, 0.8f * AlphaFactor), IGraphics::CORNER_ALL, Padding);
		Rect.Margin(Padding, &Rect);

		CTextCursor Cursor;
		Cursor.SetPosition(Rect.TopLeft());
		Cursor.m_FontSize = Tooltip.m_FontSize;
		Cursor.m_LineWidth = LineWidth;

		STextContainerIndex TextContainerIndex;
		const unsigned OldRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(OldRenderFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
		TextRender()->CreateTextContainer(TextContainerIndex, &Cursor, Tooltip.m_pText);
		TextRender()->SetRenderFlags(OldRenderFlags);

		if(TextContainerIndex.Valid())
		{
			ColorRGBA TextColor = TextRender()->DefaultTextColor();
			TextColor.a *= AlphaFactor;
			ColorRGBA OutlineColor = TextRender()->DefaultTextOutlineColor();
			OutlineColor.a *= AlphaFactor;
			TextRender()->RenderTextContainer(TextContainerIndex, TextColor, OutlineColor);
		}

		TextRender()->DeleteTextContainer(TextContainerIndex);

		Tooltip.m_OnScreen = false;
	}
}
