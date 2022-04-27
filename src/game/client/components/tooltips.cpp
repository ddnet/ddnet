#include "tooltips.h"

#include <game/client/render.h>

CTooltips::CTooltips()
{
	OnReset();
}

void CTooltips::OnReset()
{
	HoverTime = -1;
	m_Tooltips.clear();
}

void CTooltips::SetActiveTooltip(CTooltip &Tooltip)
{
	if(m_pActiveTooltip != nullptr)
		return;

	m_pActiveTooltip = &Tooltip;
	HoverTime = time_get();
}

inline void CTooltips::ClearActiveTooltip()
{
	m_pActiveTooltip = nullptr;
}

void CTooltips::DoToolTip(const void *pID, const CUIRect *pNearRect, const char *pText, float WidthHint)
{
	uintptr_t ID = reinterpret_cast<uintptr_t>(pID);
	const auto result = m_Tooltips.emplace(ID, CTooltip{
							   *pNearRect,
							   pText,
							   WidthHint});
	CTooltip &Tooltip = result.first->second;

	if(!result.second)
	{
		Tooltip.m_Rect = *pNearRect; // update in case of window resize
	}
	if(UI()->MouseInside(&Tooltip.m_Rect))
	{
		SetActiveTooltip(Tooltip);
	}
}

void CTooltips::OnRender()
{
	if(m_pActiveTooltip != nullptr)
	{
		CTooltip &Tooltip = *m_pActiveTooltip;

		if(!UI()->MouseInside(&Tooltip.m_Rect))
		{
			ClearActiveTooltip();
			return;
		}

		// Delay tooltip until 1 second passed.
		if(HoverTime > time_get() - time_freq())
			return;

		const float MARGIN = 5.0f;

		CUIRect Rect;
		Rect.w = Tooltip.m_WidthHint;
		if(Tooltip.m_WidthHint < 0.0f)
			Rect.w = TextRender()->TextWidth(0, 14.0f, Tooltip.m_pText, -1, -1.0f) + 4.0f;
		Rect.h = 30.0f;

		CUIRect *pScreen = UI()->Screen();

		// Try the top side.
		if(Tooltip.m_Rect.y - Rect.h - MARGIN > pScreen->y)
		{
			Rect.x = clamp(UI()->MouseX() - Rect.w / 2.0f, MARGIN, pScreen->w - Rect.w - MARGIN);
			Rect.y = Tooltip.m_Rect.y - Rect.h - MARGIN;
		}
		// Try the bottom side.
		else if(Tooltip.m_Rect.y + Tooltip.m_Rect.h + MARGIN < pScreen->h)
		{
			Rect.x = clamp(UI()->MouseX() - Rect.w / 2.0f, MARGIN, pScreen->w - Rect.w - MARGIN);
			Rect.y = Tooltip.m_Rect.y + Tooltip.m_Rect.h + MARGIN;
		}
		// Try the right side.
		else if(Tooltip.m_Rect.x + Tooltip.m_Rect.w + MARGIN + Rect.w < pScreen->w)
		{
			Rect.x = Tooltip.m_Rect.x + Tooltip.m_Rect.w + MARGIN;
			Rect.y = clamp(UI()->MouseY() - Rect.h / 2.0f, MARGIN, pScreen->h - Rect.h - MARGIN);
		}
		// Try the left side.
		else if(Tooltip.m_Rect.x - Rect.w - MARGIN > pScreen->x)
		{
			Rect.x = Tooltip.m_Rect.x - Rect.w - MARGIN;
			Rect.y = clamp(UI()->MouseY() - Rect.h / 2.0f, MARGIN, pScreen->h - Rect.h - MARGIN);
		}

		RenderTools()->DrawUIRect(&Rect, ColorRGBA(0.2, 0.2, 0.2, 0.80f), CUI::CORNER_ALL, 5.0f);
		Rect.Margin(2.0f, &Rect);
		UI()->DoLabel(&Rect, Tooltip.m_pText, 14.0f, TEXTALIGN_LEFT);
	}
}
