#include "ui_ex.h"

#include <base/system.h>

#include <base/math.h>
#include <engine/textrender.h>

#include <engine/client/input.h>
#include <engine/keys.h>

#include <game/client/lineinput.h>
#include <game/client/render.h>

#include <limits>

CUIEx::CUIEx(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools, IInput::CEvent *pInputEventsArray, int *pInputEventCount)
{
	Init(pUI, pKernel, pRenderTools, pInputEventsArray, pInputEventCount);
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

int CUIEx::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners, const char *pEmptyText)
{
	int Inside = UI()->MouseInside(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		m_CurCursor = minimum(str_length(pStr), m_CurCursor);

		bool IsShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);
		bool IsCtrlPressed = Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL);

		if(!IsShiftPressed && IsCtrlPressed && Input()->KeyPress(KEY_V))
		{
			const char *pText = Input()->GetClipboardText();
			if(pText)
			{
				int OffsetL = minimum(str_length(pStr), m_CurCursor);
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
						m_HasSelection = false;
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

		if(!IsShiftPressed && IsCtrlPressed && (Input()->KeyPress(KEY_C) || Input()->KeyPress(KEY_X)))
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
						m_HasSelection = false;
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

		if(!IsShiftPressed && IsCtrlPressed && Input()->KeyPress(KEY_A))
		{
			m_CurSelStart = 0;
			int StrLen = str_length(pStr);
			TextRender()->UTF8OffToDecodedOff(pStr, StrLen, m_CurSelEnd);
			m_HasSelection = true;
			m_CurCursor = StrLen;
		}

		if(!IsShiftPressed && IsCtrlPressed && Input()->KeyPress(KEY_U))
		{
			pStr[0] = '\0';
			m_CurCursor = 0;
			ReturnValue = true;
		}

		for(int i = 0; i < *m_pInputEventCount; i++)
		{
			int32_t ManipulateChanges = 0;
			int LastCursor = m_CurCursor;
			int Len = str_length(pStr);
			int NumChars = Len;
			ManipulateChanges = CLineInput::Manipulate(m_pInputEventsArray[i], pStr, StrSize, StrSize, &Len, &m_CurCursor, &NumChars, m_HasSelection ? CLineInput::LINE_INPUT_MODIFY_DONT_DELETE : 0, IsCtrlPressed ? KEY_LCTRL : 0);
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
						m_HasSelection = false;
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
						m_HasSelection = false;
					else
						m_HasSelection = true;
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
					m_HasSelection = false;
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

	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = pEmptyText;
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
		if(w - *Offset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1, std::numeric_limits<float>::max());
			do
			{
				*Offset += minimum(wt - *Offset - Textbox.w, Textbox.w / 3);
			} while(w - *Offset > Textbox.w + 0.0001f);
		}
		else if(w - *Offset < 0.0f)
		{
			// move to the right
			do
			{
				*Offset = maximum(0.0f, *Offset - Textbox.w / 3);
			} while(w - *Offset < -0.0001f);
		}
	}
	UI()->ClipEnable(pRect);
	Textbox.x -= *Offset;

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
	HasMouseSel = m_MouseIsPress;
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

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1, -1, 1, &SelCursor);

	if(UI()->LastActiveItem() == pID)
	{
		if(SelCursor.m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
		{
			m_CurSelStart = SelCursor.m_SelectionStart;
			m_CurSelEnd = SelCursor.m_SelectionEnd;
			m_HasSelection = m_CurSelStart != m_CurSelEnd;
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
