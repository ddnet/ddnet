/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_H
#define GAME_CLIENT_UI_H

#include <base/system.h>
#include <engine/textrender.h>
#include <string>
#include <vector>

class CUIRect
{
	// TODO: Refactor: Redo UI scaling
	float Scale() const;

public:
	float x, y, w, h;

	/**
	 * Splits 2 CUIRect inside *this* CUIRect horizontally. You can pass null pointers.
	 * 
	 * @param pTop This rect will end up taking the top half of this CUIRect
	 * @param pBottom This rect will end up taking the bottom half of this CUIRect
	 */
	void HSplitMid(CUIRect *pTop, CUIRect *pBottom) const;
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
	 */
	void VSplitMid(CUIRect *pLeft, CUIRect *pRight) const;
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
};

struct SUIAnimator
{
	bool m_Active;
	bool m_ScaleLabel;
	bool m_RepositionLabel;

	int64 m_Time;
	float m_Value;

	float m_XOffset;
	float m_YOffset;
	float m_WOffset;
	float m_HOffset;
};

class CUI;

class CUIElement
{
	friend class CUI;

	CUIElement(CUI *pUI) { Init(pUI); }

public:
	struct SUIElementRect
	{
	public:
		int m_UIRectQuadContainer;
		int m_UITextContainer;

		float m_X;
		float m_Y;
		float m_Width;
		float m_Height;

		std::string m_Text;

		CTextCursor m_Cursor;

		STextRenderColor m_TextColor;
		STextRenderColor m_TextOutlineColor;

		SUIElementRect();
	};

protected:
	std::vector<SUIElementRect> m_UIRects;

	// used for marquees or other user implemented things
	int64 m_ElementTime;

public:
	CUIElement() = default;

	void Init(CUI *pUI);

	SUIElementRect *Get(size_t Index)
	{
		return &m_UIRects[Index];
	}

	size_t Size()
	{
		return m_UIRects.size();
	}

	void Clear() { m_UIRects.clear(); }

	void Add(SUIElementRect &ElRect)
	{
		m_UIRects.push_back(ElRect);
	}
};

class CUI
{
	const void *m_pHotItem;
	const void *m_pActiveItem;
	const void *m_pLastActiveItem;
	const void *m_pBecommingHotItem;
	float m_MouseX, m_MouseY; // in gui space
	float m_MouseDeltaX, m_MouseDeltaY; // in gui space
	float m_MouseWorldX, m_MouseWorldY; // in world space
	unsigned m_MouseButtons;
	unsigned m_LastMouseButtons;

	CUIRect m_Screen;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;

	std::vector<CUIElement *> m_OwnUIElements; // ui elements maintained by CUI class
	std::vector<CUIElement *> m_UIElements;

public:
	// TODO: Refactor: Fill this in
	void SetGraphics(class IGraphics *pGraphics, class ITextRender *pTextRender)
	{
		m_pGraphics = pGraphics;
		m_pTextRender = pTextRender;
	}
	class IGraphics *Graphics() { return m_pGraphics; }
	class ITextRender *TextRender() { return m_pTextRender; }

	CUI();
	~CUI();

	void ResetUIElement(CUIElement &UIElement);

	CUIElement *GetNewUIElement();

	void AddUIElement(CUIElement *pElement);
	void OnElementsReset();
	void OnWindowResize();
	void OnLanguageChange();

	enum
	{
		CORNER_TL = 1,
		CORNER_TR = 2,
		CORNER_BL = 4,
		CORNER_BR = 8,

		CORNER_T = CORNER_TL | CORNER_TR,
		CORNER_B = CORNER_BL | CORNER_BR,
		CORNER_R = CORNER_TR | CORNER_BR,
		CORNER_L = CORNER_TL | CORNER_BL,

		CORNER_ALL = CORNER_T | CORNER_B
	};

	int Update(float mx, float my, float Mwx, float Mwy, int m_Buttons);

	float MouseDeltaX() const { return m_MouseDeltaX; }
	float MouseDeltaY() const { return m_MouseDeltaY; }
	float MouseX() const { return m_MouseX; }
	float MouseY() const { return m_MouseY; }
	float MouseWorldX() const { return m_MouseWorldX; }
	float MouseWorldY() const { return m_MouseWorldY; }
	int MouseButton(int Index) const { return (m_MouseButtons >> Index) & 1; }
	int MouseButtonClicked(int Index) { return MouseButton(Index) && !((m_LastMouseButtons >> Index) & 1); }
	int MouseButtonReleased(int Index) { return ((m_LastMouseButtons >> Index) & 1) && !MouseButton(Index); }

	void SetHotItem(const void *pID) { m_pBecommingHotItem = pID; }
	void SetActiveItem(const void *pID)
	{
		m_pActiveItem = pID;
		if(pID)
			m_pLastActiveItem = pID;
	}
	void ClearLastActiveItem() { m_pLastActiveItem = 0; }
	const void *HotItem() const { return m_pHotItem; }
	const void *NextHotItem() const { return m_pBecommingHotItem; }
	const void *ActiveItem() const { return m_pActiveItem; }
	const void *LastActiveItem() const { return m_pLastActiveItem; }

	int MouseInside(const CUIRect *pRect);
	void ConvertMouseMove(float *x, float *y);

	CUIRect *Screen();
	float PixelSize();
	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();

	// TODO: Refactor: Redo UI scaling
	void SetScale(float s);
	float Scale();

	int DoButtonLogic(const void *pID, int Checked, const CUIRect *pRect);
	int DoButtonLogic(const void *pID, const char *pText /* TODO: Refactor: Remove */, int Checked, const CUIRect *pRect);
	int DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY);

	// TODO: Refactor: Remove this?
	void DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1);
	void DoLabelScaled(const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1);

	void DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1, bool StopAtEnd = false, int StrLen = -1, class CTextCursor *pReadCursor = NULL);
	void DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1, bool StopAtEnd = false, int StrLen = -1, class CTextCursor *pReadCursor = NULL);
};

#endif
