/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_RECT_H
#define GAME_CLIENT_UI_RECT_H

#include <base/color.h>

class IGraphics;

class CUIRect
{
	static IGraphics *s_pGraphics;

public:
	static void Init(IGraphics *pGraphics) { s_pGraphics = pGraphics; }

	float x, y, w, h;

	/**
	 * Splits 2 CUIRect inside *this* CUIRect horizontally. You can pass null pointers.
	 *
	 * @param pTop This rect will end up taking the top half of this CUIRect.
	 * @param pBottom This rect will end up taking the bottom half of this CUIRect.
	 * @param Spacing Total size of margin between split rects.
	 */
	void HSplitMid(CUIRect *pTop, CUIRect *pBottom, float Spacing = 0.0f) const;
	/**
	 * Splits 2 CUIRect inside *this* CUIRect.
	 *
	 * The cut parameter determines the height of the top rect, so it allows more customization than HSplitMid.
	 *
	 * This method doesn't check if Cut is bigger than *this* rect height.
	 *
	 * @param Cut The height of the pTop rect.
	 * @param pTop The rect that ends up at the top with a height equal to Cut.
	 * @param pBottom The rect that ends up at the bottom with a height equal to *this* rect minus the Cut.
	 */
	void HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	/**
	 * Splits 2 CUIRect inside *this* CUIRect.
	 *
	 * The cut parameter determines the height of the bottom rect, so it allows more customization than HSplitMid.
	 *
	 * This method doesn't check if Cut is bigger than *this* rect height.
	 *
	 * @param Cut The height of the pBottom rect.
	 * @param pTop The rect that ends up at the top with a height equal to *this* CUIRect height minus Cut.
	 * @param pBottom The rect that ends up at the bottom with a height equal to Cut.
	 */
	void HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	/**
	 * Splits 2 CUIRect inside *this* CUIRect vertically. You can pass null pointers.
	 *
	 * @param pLeft This rect will take up the left half of *this* CUIRect.
	 * @param pRight This rect will take up the right half of *this* CUIRect.
	 * @param Spacing Total size of margin between split rects.
	 */
	void VSplitMid(CUIRect *pLeft, CUIRect *pRight, float Spacing = 0.0f) const;
	/**
	 * Splits 2 CUIRect inside *this* CUIRect.
	 *
	 * The cut parameter determines the width of the left rect, so it allows more customization than VSplitMid.
	 *
	 * This method doesn't check if Cut is bigger than *this* rect width.
	 *
	 * @param Cut The width of the pLeft rect.
	 * @param pLeft The rect that ends up at the left with a width equal to Cut.
	 * @param pRight The rect that ends up at the right with a width equal to *this* rect minus the Cut.
	 */
	void VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const;
	/**
	 * Splits 2 CUIRect inside *this* CUIRect.
	 *
	 * The cut parameter determines the width of the right rect, so it allows more customization than VSplitMid.
	 *
	 * This method doesn't check if Cut is bigger than *this* rect width.
	 *
	 * @param Cut The width of the pRight rect.
	 * @param pLeft The rect that ends up at the left with a width equal to *this* CUIRect width minus Cut.
	 * @param pRight The rect that ends up at the right with a width equal to Cut.
	 */
	void VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const;

	/**
	 * Places pOtherRect inside *this* CUIRect with Cut as the margin.
	 *
	 * @param Cut The margin as a vec2.
	 * The x component applies to the vertical axis.
	 * The y component applies to the horizontal axis.
	 * @param pOtherRect The CUIRect to place inside *this* CUIRect.
	 */
	void Margin(vec2 Cut, CUIRect *pOtherRect) const;
	/**
	 * Places pOtherRect inside *this* CUIRect with Cut as the margin.
	 *
	 * @param Cut The margin.
	 * @param pOtherRect The CUIRect to place inside *this* CUIRect.
	 */
	void Margin(float Cut, CUIRect *pOtherRect) const;
	/**
	 * Places pOtherRect inside *this* CUIRect applying Cut as the margin only on the vertical axis.
	 *
	 * @param Cut The margin.
	 * @param pOtherRect The CUIRect to place inside *this* CUIRect
	 */
	void VMargin(float Cut, CUIRect *pOtherRect) const;
	/**
	 * Places pOtherRect inside *this* CUIRect applying Cut as the margin only on the horizontal axis.
	 *
	 * @param Cut The margin.
	 * @param pOtherRect The CUIRect to place inside *this* CUIRect
	 */
	void HMargin(float Cut, CUIRect *pOtherRect) const;

	/**
	 * Checks whether a point is inside *this* CUIRect.
	 *
	 * @param Point The point's position.
	 * @return true iff the given point is inside *this* CUIRect.
	 */
	bool Inside(vec2 Point) const;

	/**
	 * Fill background of *this* CUIRect.
	 *
	 * @note Example of filling a black half transparent background with 5px rounded edges on all sides
	 * @note ```MyCuiRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 5.0f);```
	 *
	 * @note Example of filling a red background with sharp edges
	 * @note ```MyCuiRect.Draw(ColorRGBA(1.0f, 0.0f, 0.0f), IGraphics::CORNER_NONE, 0.0f);```
	 *
	 * @param Color
	 * @param Corners
	 * @param Rounding
	 */
	void Draw(ColorRGBA Color, int Corners, float Rounding) const;
	void Draw4(ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, int Corners, float Rounding) const;

	/**
	 * Returns the top-left position of *this* CUIRect as a vec2.
	 *
	 * @return Top-left position as vec2.
	 */
	vec2 TopLeft() const { return vec2(x, y); }
	/**
	 * Returns the size of *this* CUIRect as a vec2.
	 *
	 * @return Size as vec2.
	 */
	vec2 Size() const { return vec2(w, h); }
	/**
	 * Returns the center position of *this* CUIRect as a vec2.
	 *
	 * @return Center position as vec2.
	 */
	vec2 Center() const { return TopLeft() + Size() / 2.0f; }
};

#endif
