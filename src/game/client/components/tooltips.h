#ifndef GAME_CLIENT_COMPONENTS_TOOLTIPS_H
#define GAME_CLIENT_COMPONENTS_TOOLTIPS_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

struct CTooltip
{
	const void *m_pId;
	CUIRect m_Rect;
	const char *m_pText;
	float m_WidthHint;
	bool m_OnScreen; // used to know if the tooltip should be rendered.
};

/**
 * A component that manages and renders UI tooltips.
 *
 * Should be among the last components to render.
 */
class CTooltips : public CComponent
{
	std::unordered_map<uintptr_t, CTooltip> m_Tooltips;
	std::optional<std::reference_wrapper<CTooltip>> m_ActiveTooltip;
	std::optional<std::reference_wrapper<CTooltip>> m_PreviousTooltip;
	int64_t m_HoverTime;

	/**
	 * @param Tooltip A reference to the tooltip that should be active.
	 */
	void SetActiveTooltip(CTooltip &Tooltip);

	inline void ClearActiveTooltip();

public:
	CTooltips();
	virtual int Sizeof() const override { return sizeof(*this); }

	/**
	 * Adds the tooltip to a cache and renders it when active.
	 *
	 * On the first call to this function, the data passed is cached, afterwards the calls are used to detect if the tooltip should be activated.
	 * If multiple tooltips cover the same rect or the rects intersect, then the tooltip that is added later has priority.
	 *
	 * @param pId The ID of the tooltip. Usually a reference to some g_Config value.
	 * @param pNearRect Place the tooltip near this rect.
	 * @param pText The text to display in the tooltip.
	 * @param WidthHint The maximum width of the tooltip, or -1.0f for unlimited.
	 */
	void DoToolTip(const void *pId, const CUIRect *pNearRect, const char *pText, float WidthHint = -1.0f);

	virtual void OnReset() override;
	virtual void OnRender() override;
};

#endif
