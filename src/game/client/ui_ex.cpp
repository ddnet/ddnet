#include "ui_ex.h"

#include <base/system.h>

#include <base/math.h>
#include <engine/textrender.h>

#include <engine/client/input.h>
#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/render.h>

#include <limits>

CUIEx::CUIEx()
{
	m_MouseSlow = false;
}

void CUIEx::Init(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools, IInput::CEvent *pInputEventsArray, int *pInputEventCount)
{
	m_pUI = pUI;
	m_pKernel = pKernel;
	m_pRenderTools = pRenderTools;
	m_pInputEventsArray = pInputEventsArray;
	m_pInputEventCount = pInputEventCount;

	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
}

void CUIEx::ConvertMouseMove(float *pX, float *pY) const
{
	UI()->ConvertMouseMove(pX, pY);

	if(m_MouseSlow)
	{
		const float SlowMouseFactor = 0.05f;
		*pX *= SlowMouseFactor;
		*pY *= SlowMouseFactor;
	}
}

float CUIEx::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	pRect->Margin(5.0f, &Rail);

	CUIRect Handle;
	Rail.HSplitTop(clamp(33.0f, Rail.w, Rail.h / 3.0f), &Handle, 0);
	Handle.y = Rail.y + (Rail.h - Handle.h) * Current;

	// logic
	static float s_OffsetY;
	const bool InsideRail = UI()->MouseInside(&Rail);
	const bool InsideHandle = UI()->MouseInside(&Handle);
	bool Grabbed = false; // whether to apply the offset

	if(UI()->ActiveItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			Grabbed = true;
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				m_MouseSlow = true;
		}
		else
		{
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			s_OffsetY = UI()->MouseY() - Handle.y;
			Grabbed = true;
		}
	}
	else if(UI()->MouseButtonClicked(0) && !InsideHandle && InsideRail)
	{
		UI()->SetActiveItem(pID);
		s_OffsetY = Handle.h / 2.0f;
		Grabbed = true;
	}

	if(InsideHandle)
	{
		UI()->SetHotItem(pID);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.y;
		const float Max = Rail.h - Handle.h;
		const float Cur = UI()->MouseY() - s_OffsetY;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	RenderTools()->DrawUIRect(&Rail, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, Rail.w / 2.0f);

	float ColorSlider;
	if(UI()->ActiveItem() == pID)
		ColorSlider = 0.9f;
	else if(UI()->HotItem() == pID)
		ColorSlider = 1.0f;
	else
		ColorSlider = 0.8f;

	RenderTools()->DrawUIRect(&Handle, ColorRGBA(ColorSlider, ColorSlider, ColorSlider, 1.0f), CUI::CORNER_ALL, Handle.w / 2.0f);

	return ReturnValue;
}

float CUIEx::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	if(pColorInner)
		Rail = *pRect;
	else
		pRect->HMargin(5.0f, &Rail);

	CUIRect Handle;
	Rail.VSplitLeft(pColorInner ? 8.0f : clamp(33.0f, Rail.h, Rail.w / 3.0f), &Handle, 0);
	Handle.x += (Rail.w - Handle.w) * Current;

	// logic
	static float s_OffsetX;
	const bool InsideRail = UI()->MouseInside(&Rail);
	const bool InsideHandle = UI()->MouseInside(&Handle);
	bool Grabbed = false; // whether to apply the offset

	if(UI()->ActiveItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			Grabbed = true;
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				m_MouseSlow = true;
		}
		else
		{
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			s_OffsetX = UI()->MouseX() - Handle.x;
			Grabbed = true;
		}
	}
	else if(UI()->MouseButtonClicked(0) && !InsideHandle && InsideRail)
	{
		UI()->SetActiveItem(pID);
		s_OffsetX = Handle.w / 2.0f;
		Grabbed = true;
	}

	if(InsideHandle)
	{
		UI()->SetHotItem(pID);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.x;
		const float Max = Rail.w - Handle.w;
		const float Cur = UI()->MouseX() - s_OffsetX;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	if(pColorInner)
	{
		CUIRect Slider;
		Handle.VMargin(-2.0f, &Slider);
		Slider.HMargin(-3.0f, &Slider);
		RenderTools()->DrawUIRect(&Slider, ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), CUI::CORNER_ALL, 5.0f);
		Slider.Margin(2.0f, &Slider);
		RenderTools()->DrawUIRect(&Slider, *pColorInner, CUI::CORNER_ALL, 3.0f);
	}
	else
	{
		RenderTools()->DrawUIRect(&Rail, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, Rail.h / 2.0f);

		float ColorSlider;
		if(UI()->ActiveItem() == pID)
			ColorSlider = 0.9f;
		else if(UI()->HotItem() == pID)
			ColorSlider = 1.0f;
		else
			ColorSlider = 0.8f;

		RenderTools()->DrawUIRect(&Handle, ColorRGBA(ColorSlider, ColorSlider, ColorSlider, 1.0f), CUI::CORNER_ALL, Handle.h / 2.0f);
	}

	return ReturnValue;
}

bool CUIEx::DoEditBox(const void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners, const SUIExEditBoxProperties &Properties)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;

	FontSize *= UI()->Scale();

	auto &&SetHasSelection = [&](bool HasSelection) {
		m_HasSelection = HasSelection;
		m_pSelItem = m_HasSelection ? pID : nullptr;
	};

	auto &&SelectAllText = [&]() {
		m_CurSelStart = 0;
		int StrLen = str_length(pStr);
		TextRender()->UTF8OffToDecodedOff(pStr, StrLen, m_CurSelEnd);
		SetHasSelection(true);
		m_CurCursor = StrLen;
	};

	if(UI()->LastActiveItem() == pID)
	{
		if(m_HasSelection && m_pSelItem != pID)
		{
			SetHasSelection(false);
		}

		m_CurCursor = minimum(str_length(pStr), m_CurCursor);

		bool IsShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);
		bool IsModPressed = Input()->ModifierIsPressed();

		if(!IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_V))
		{
			const char *pText = Input()->GetClipboardText();
			if(pText)
			{
				int OffsetL = clamp(m_CurCursor, 0, str_length(pStr));
				int OffsetR = OffsetL;

				if(m_HasSelection)
				{
					int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
					int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
					int UTF8SelLeft = -1;
					int UTF8SelRight = -1;
					if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft, SelRight, UTF8SelLeft, UTF8SelRight))
					{
						OffsetL = UTF8SelLeft;
						OffsetR = UTF8SelRight;
						SetHasSelection(false);
					}
				}

				std::string NewStr(pStr, OffsetL);

				int WrittenChars = 0;

				const char *pIt = pText;
				while(*pIt)
				{
					const char *pTmp = pIt;
					int Character = str_utf8_decode(&pTmp);
					if(Character == -1 || Character == 0)
						break;

					if(Character == '\r' || Character == '\n')
					{
						NewStr.append(1, ' ');
						++WrittenChars;
					}
					else
					{
						NewStr.append(pIt, (std::intptr_t)(pTmp - pIt));
						WrittenChars += (int)(std::intptr_t)(pTmp - pIt);
					}

					pIt = pTmp;
				}

				NewStr.append(pStr + OffsetR);

				str_copy(pStr, NewStr.c_str(), StrSize);

				m_CurCursor = OffsetL + WrittenChars;
				ReturnValue = true;
			}
		}

		if(!IsShiftPressed && IsModPressed && (Input()->KeyPress(KEY_C) || Input()->KeyPress(KEY_X)))
		{
			if(m_HasSelection)
			{
				int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
				int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
				int UTF8SelLeft = -1;
				int UTF8SelRight = -1;
				if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft, SelRight, UTF8SelLeft, UTF8SelRight))
				{
					std::string NewStr(&pStr[UTF8SelLeft], UTF8SelRight - UTF8SelLeft);
					Input()->SetClipboardText(NewStr.c_str());
					if(Input()->KeyPress(KEY_X))
					{
						NewStr = std::string(pStr, UTF8SelLeft) + std::string(pStr + UTF8SelRight);
						str_copy(pStr, NewStr.c_str(), StrSize);
						SetHasSelection(false);
						if(m_CurCursor > UTF8SelLeft)
							m_CurCursor = maximum(0, m_CurCursor - (UTF8SelRight - UTF8SelLeft));
						else
							m_CurCursor = UTF8SelLeft;
					}
				}
			}
			else
				Input()->SetClipboardText(pStr);
		}

		if(Properties.m_SelectText || (!IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_A)))
		{
			SelectAllText();
		}

		if(!IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_U))
		{
			pStr[0] = '\0';
			m_CurCursor = 0;
			ReturnValue = true;
		}

		for(int i = 0; i < *m_pInputEventCount; i++)
		{
			int LastCursor = m_CurCursor;
			int Len, NumChars;
			str_utf8_stats(pStr, StrSize, StrSize, &Len, &NumChars);
			int32_t ManipulateChanges = CLineInput::Manipulate(m_pInputEventsArray[i], pStr, StrSize, StrSize, &Len, &m_CurCursor, &NumChars, m_HasSelection ? CLineInput::LINE_INPUT_MODIFY_DONT_DELETE : 0, IsModPressed ? KEY_LCTRL : 0);
			ReturnValue |= (ManipulateChanges & (CLineInput::LINE_INPUT_CHANGE_STRING | CLineInput::LINE_INPUT_CHANGE_CHARACTERS_DELETE)) != 0;

			// if cursor changed, reset selection
			if(ManipulateChanges != 0)
			{
				if(m_HasSelection && (ManipulateChanges & (CLineInput::LINE_INPUT_CHANGE_STRING | CLineInput::LINE_INPUT_CHANGE_CHARACTERS_DELETE)) != 0)
				{
					int OffsetL = 0;
					int OffsetR = 0;

					bool IsReverseSel = m_CurSelStart > m_CurSelEnd;

					int ExtraNew = 0;
					int ExtraOld = 0;
					// selection correction from added chars
					if(IsReverseSel)
					{
						TextRender()->UTF8OffToDecodedOff(pStr, m_CurCursor, ExtraNew);
						TextRender()->UTF8OffToDecodedOff(pStr, LastCursor, ExtraOld);
					}

					int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
					int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
					int UTF8SelLeft = -1;
					int UTF8SelRight = -1;
					if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft + (ExtraNew - ExtraOld), SelRight + (ExtraNew - ExtraOld), UTF8SelLeft, UTF8SelRight))
					{
						OffsetL = UTF8SelLeft;
						OffsetR = UTF8SelRight;
						SetHasSelection(false);
					}

					std::string NewStr(pStr, OffsetL);

					NewStr.append(pStr + OffsetR);

					str_copy(pStr, NewStr.c_str(), StrSize);

					if(!IsReverseSel)
						m_CurCursor = clamp<int>(m_CurCursor - (UTF8SelRight - UTF8SelLeft), 0, NewStr.length());
				}

				if(IsShiftPressed && (ManipulateChanges & CLineInput::LINE_INPUT_CHANGE_STRING) == 0)
				{
					int CursorPosDecoded = -1;
					int LastCursorPosDecoded = -1;

					if(!m_HasSelection)
					{
						m_CurSelStart = -1;
						m_CurSelEnd = -1;
					}

					if(TextRender()->UTF8OffToDecodedOff(pStr, m_CurCursor, CursorPosDecoded))
					{
						if(TextRender()->UTF8OffToDecodedOff(pStr, LastCursor, LastCursorPosDecoded))
						{
							if(!m_HasSelection)
							{
								m_CurSelStart = LastCursorPosDecoded;
								m_CurSelEnd = LastCursorPosDecoded;
							}
							m_CurSelEnd += (CursorPosDecoded - LastCursorPosDecoded);
						}
					}
					if(m_CurSelStart == m_CurSelEnd)
						SetHasSelection(false);
					else
						SetHasSelection(true);
				}
				else
				{
					if(m_HasSelection && (ManipulateChanges & CLineInput::LINE_INPUT_CHANGE_CURSOR) != 0)
					{
						if(m_CurSelStart < m_CurSelEnd)
						{
							if(m_CurCursor >= LastCursor)
								m_CurCursor = LastCursor;
							else
								TextRender()->DecodedOffToUTF8Off(pStr, m_CurSelStart, m_CurCursor);
						}
						else
						{
							if(m_CurCursor <= LastCursor)
								m_CurCursor = LastCursor;
							else
								TextRender()->DecodedOffToUTF8Off(pStr, m_CurSelStart, m_CurCursor);
						}
					}
					SetHasSelection(false);
				}
			}
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(pID);
	}

	CUIRect Textbox = *pRect;
	RenderTools()->DrawUIRect(&Textbox, ColorRGBA(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.VMargin(2.0f, &Textbox);
	Textbox.HMargin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pDisplayStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars) - 1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	char aDispEditingText[128 + IInput::INPUT_TEXT_SIZE + 2] = {0};
	int DispCursorPos = m_CurCursor;
	if(UI()->LastActiveItem() == pID && Input()->GetIMEEditingTextLength() > -1)
	{
		int EditingTextCursor = Input()->GetEditingCursor();
		str_copy(aDispEditingText, pDisplayStr, sizeof(aDispEditingText));
		char aEditingText[IInput::INPUT_TEXT_SIZE + 2];
		if(Hidden)
		{
			// Do not show editing text in password field
			str_copy(aEditingText, "[*]", sizeof(aEditingText));
			EditingTextCursor = 1;
		}
		else
		{
			str_format(aEditingText, sizeof(aEditingText), "[%s]", Input()->GetIMEEditingText());
		}
		int NewTextLen = str_length(aEditingText);
		int CharsLeft = (int)sizeof(aDispEditingText) - str_length(aDispEditingText) - 1;
		int FillCharLen = minimum(NewTextLen, CharsLeft);
		for(int i = str_length(aDispEditingText) - 1; i >= m_CurCursor; i--)
			aDispEditingText[i + FillCharLen] = aDispEditingText[i];
		for(int i = 0; i < FillCharLen; i++)
			aDispEditingText[m_CurCursor + i] = aEditingText[i];
		DispCursorPos = m_CurCursor + EditingTextCursor + 1;
		pDisplayStr = aDispEditingText;
		UpdateOffset = true;
	}

	bool IsEmptyText = false;
	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = Properties.m_pEmptyText;
		IsEmptyText = true;
		TextRender()->TextColor(1, 1, 1, 0.75f);
	}

	DispCursorPos = minimum(DispCursorPos, str_length(pDisplayStr));

	bool JustGotActive = false;
	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			if(UI()->LastActiveItem() != pID)
				JustGotActive = true;
			UI()->SetActiveItem(pID);
		}
	}

	// check if the text has to be moved
	if(UI()->LastActiveItem() == pID && !JustGotActive && (UpdateOffset || *m_pInputEventCount))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		if(w - *pOffset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1, std::numeric_limits<float>::max());
			do
			{
				*pOffset += minimum(wt - *pOffset - Textbox.w, Textbox.w / 3);
			} while(w - *pOffset > Textbox.w + 0.0001f);
		}
		else if(w - *pOffset < 0.0f)
		{
			// move to the right
			do
			{
				*pOffset = maximum(0.0f, *pOffset - Textbox.w / 3);
			} while(w - *pOffset < -0.0001f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *pOffset;

	CTextCursor SelCursor;
	TextRender()->SetCursor(&SelCursor, 0, 0, 16, 0);

	bool HasMouseSel = false;
	if(UI()->LastActiveItem() == pID)
	{
		if(!m_MouseIsPress && UI()->MouseButtonClicked(0))
		{
			m_MouseIsPress = true;
			m_MousePressX = UI()->MouseX();
			m_MousePressY = UI()->MouseY();
		}
	}

	if(m_MouseIsPress)
	{
		m_MouseCurX = UI()->MouseX();
		m_MouseCurY = UI()->MouseY();
	}
	HasMouseSel = m_MouseIsPress && !IsEmptyText;
	if(m_MouseIsPress && UI()->MouseButtonReleased(0))
	{
		m_MouseIsPress = false;
	}

	if(UI()->LastActiveItem() == pID)
	{
		int CursorPos = -1;
		TextRender()->UTF8OffToDecodedOff(pDisplayStr, DispCursorPos, CursorPos);

		SelCursor.m_CursorMode = HasMouseSel ? TEXT_CURSOR_CURSOR_MODE_CALCULATE : TEXT_CURSOR_CURSOR_MODE_SET;
		SelCursor.m_CursorCharacter = CursorPos;
		SelCursor.m_CalculateSelectionMode = HasMouseSel ? TEXT_CURSOR_SELECTION_MODE_CALCULATE : (m_HasSelection ? TEXT_CURSOR_SELECTION_MODE_SET : TEXT_CURSOR_SELECTION_MODE_NONE);
		SelCursor.m_PressMouseX = m_MousePressX;
		SelCursor.m_PressMouseY = m_MousePressY;
		SelCursor.m_ReleaseMouseX = m_MouseCurX;
		SelCursor.m_ReleaseMouseY = m_MouseCurY;
		SelCursor.m_SelectionStart = m_CurSelStart;
		SelCursor.m_SelectionEnd = m_CurSelEnd;
	}

	SLabelProperties Props;
	Props.m_pSelCursor = &SelCursor;
	Props.m_EnableWidthCheck = IsEmptyText;
	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, TEXTALIGN_LEFT, Props);

	if(UI()->LastActiveItem() == pID)
	{
		if(SelCursor.m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
		{
			m_CurSelStart = SelCursor.m_SelectionStart;
			m_CurSelEnd = SelCursor.m_SelectionEnd;
			SetHasSelection(m_CurSelStart != m_CurSelEnd);
		}
		if(SelCursor.m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE)
		{
			TextRender()->DecodedOffToUTF8Off(pDisplayStr, SelCursor.m_CursorCharacter, DispCursorPos);
			m_CurCursor = DispCursorPos;
		}
	}

	TextRender()->TextColor(1, 1, 1, 1);

	// set the ime cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		Textbox.x += w;
		Input()->SetEditingPosition(Textbox.x, Textbox.y + FontSize);
	}

	UI()->ClipDisable();

	return ReturnValue;
}

bool CUIEx::DoClearableEditBox(const void *pID, const void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners, const SUIExEditBoxProperties &Properties)
{
	CUIRect EditBox;
	CUIRect ClearButton;
	pRect->VSplitRight(15.0f, &EditBox, &ClearButton);
	bool ReturnValue = DoEditBox(pID, &EditBox, pStr, StrSize, FontSize, pOffset, Hidden, Corners & ~CUI::CORNER_R, Properties);

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
	RenderTools()->DrawUIRect(&ClearButton, ColorRGBA(1, 1, 1, 0.33f * UI()->ButtonColorMul(pClearID)), Corners & ~CUI::CORNER_L, 3.0f);

	SLabelProperties Props;
	Props.m_AlignVertically = 0;
	UI()->DoLabel(&ClearButton, "×", ClearButton.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
	TextRender()->SetRenderFlags(0);
	if(UI()->DoButtonLogic(pClearID, "×", 0, &ClearButton))
	{
		pStr[0] = 0;
		UI()->SetActiveItem(pID);
		ReturnValue = true;
	}
	return ReturnValue;
}
