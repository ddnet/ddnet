#include "ui_ex.h"

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
	static int s_AtIndex = 0;
	static bool s_DoScroll = false;
	static float s_ScrollStart = 0.0f;

	FontSize *= UI()->Scale();

	if(UI()->LastActiveItem() == pID)
	{
		int Len = str_length(pStr);
		if(Len == 0)
			s_AtIndex = 0;

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_V))
		{
			const char *Text = Input()->GetClipboardText();
			if(Text)
			{
				int Offset = str_length(pStr);
				int CharsLeft = StrSize - Offset;
				char *pCur = pStr + Offset;
				str_utf8_copy(pCur, Text, CharsLeft);
				for(int i = 0; i < CharsLeft; i++)
				{
					if(pCur[i] == 0)
						break;
					else if(pCur[i] == '\r')
						pCur[i] = ' ';
					else if(pCur[i] == '\n')
						pCur[i] = ' ';
				}
				s_AtIndex = str_length(pStr);
				ReturnValue = true;
			}
		}

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_C))
		{
			Input()->SetClipboardText(pStr);
		}

		/* TODO: Doesn't work, SetClipboardText doesn't retain the string quickly enough?
		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_X))
		{
			Input()->SetClipboardText(pStr);
			pStr[0] = '\0';
			s_AtIndex = 0;
			ReturnValue = true;
		}
		*/

		if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyPress(KEY_U))
		{
			pStr[0] = '\0';
			s_AtIndex = 0;
			ReturnValue = true;
		}

		if(Inside && UI()->MouseButton(0))
		{
			s_DoScroll = true;
			s_ScrollStart = UI()->MouseX();
			int MxRel = (int)(UI()->MouseX() - pRect->x);

			for(int i = 1; i <= Len; i++)
			{
				if(TextRender()->TextWidth(0, FontSize, pStr, i, std::numeric_limits<float>::max()) - *Offset > MxRel)
				{
					s_AtIndex = i - 1;
					break;
				}

				if(i == Len)
					s_AtIndex = Len;
			}
		}
		else if(!UI()->MouseButton(0))
			s_DoScroll = false;
		else if(s_DoScroll)
		{
			// do scrolling
			if(UI()->MouseX() < pRect->x && s_ScrollStart - UI()->MouseX() > 10.0f)
			{
				s_AtIndex = maximum(0, s_AtIndex - 1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
			else if(UI()->MouseX() > pRect->x + pRect->w && UI()->MouseX() - s_ScrollStart > 10.0f)
			{
				s_AtIndex = minimum(Len, s_AtIndex + 1);
				s_ScrollStart = UI()->MouseX();
				UpdateOffset = true;
			}
		}

		for(int i = 0; i < *m_pInputEventCount; i++)
		{
			Len = str_length(pStr);
			int NumChars = Len;
			ReturnValue |= CLineInput::Manipulate(m_pInputEventsArray[i], pStr, StrSize, StrSize, &Len, &s_AtIndex, &NumChars);
		}
	}

	bool JustGotActive = false;

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
		{
			s_AtIndex = minimum(s_AtIndex, str_length(pStr));
			s_DoScroll = false;
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
	int DispCursorPos = s_AtIndex;
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
		for(int i = str_length(aDispEditingText) - 1; i >= s_AtIndex; i--)
			aDispEditingText[i + FillCharLen] = aDispEditingText[i];
		for(int i = 0; i < FillCharLen; i++)
			aDispEditingText[s_AtIndex + i] = aEditingText[i];
		DispCursorPos = s_AtIndex + EditingTextCursor + 1;
		pDisplayStr = aDispEditingText;
		UpdateOffset = true;
	}

	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = pEmptyText;
		TextRender()->TextColor(1, 1, 1, 0.75f);
	}

	DispCursorPos = minimum(DispCursorPos, str_length(pDisplayStr));

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

	UI()->DoLabel(&Textbox, pDisplayStr, FontSize, -1);

	TextRender()->TextColor(1, 1, 1, 1);

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float OnePixelWidth = ((ScreenX1 - ScreenX0) / Graphics()->ScreenWidth());

	// render the cursor
	if(UI()->LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		Textbox.x += w;

		if((2 * time_get() / time_freq()) % 2)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 0, 0.3f);
			float PosToMid = (Textbox.h - FontSize) / 2.0f;
			IGraphics::CQuadItem CursorTBack(Textbox.x - (OnePixelWidth * 2.0f) / 2.0f, Textbox.y + PosToMid, OnePixelWidth * 2 * 2.0f, FontSize);
			Graphics()->QuadsDrawTL(&CursorTBack, 1);
			Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem CursorT(Textbox.x, Textbox.y + PosToMid + OnePixelWidth * 1.5f, OnePixelWidth * 2.0f, FontSize - OnePixelWidth * 1.5f * 2);
			Graphics()->QuadsDrawTL(&CursorT, 1);
			Graphics()->QuadsEnd();
		}

		Input()->SetEditingPosition(Textbox.x, Textbox.y + FontSize);
	}

	UI()->ClipDisable();

	return ReturnValue;
}
