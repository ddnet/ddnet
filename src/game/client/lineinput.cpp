/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/keys.h>
#include <engine/shared/config.h>

#include "lineinput.h"
#include "ui.h"

IInput *CLineInput::ms_pInput = nullptr;
ITextRender *CLineInput::ms_pTextRender = nullptr;
IGraphics *CLineInput::ms_pGraphics = nullptr;
IClient *CLineInput::ms_pClient = nullptr;

CLineInput *CLineInput::ms_pActiveInput = nullptr;
EInputPriority CLineInput::ms_ActiveInputPriority = EInputPriority::NONE;

vec2 CLineInput::ms_CompositionWindowPosition = vec2(0.0f, 0.0f);
float CLineInput::ms_CompositionLineHeight = 0.0f;

char CLineInput::ms_aStars[128] = {'\0'};

void CLineInput::SetBuffer(char *pStr, size_t MaxSize, size_t MaxChars)
{
	if(m_pStr && m_pStr == pStr)
		return;
	const char *pLastStr = m_pStr;
	m_pStr = pStr;
	m_MaxSize = MaxSize;
	m_MaxChars = MaxChars;
	m_WasChanged = m_pStr && pLastStr && m_WasChanged;
	if(!pLastStr)
	{
		m_CursorPos = m_SelectionStart = m_SelectionEnd = m_LastCompositionCursorPos = 0;
		m_ScrollOffset = m_ScrollOffsetChange = 0.0f;
		m_CaretPosition = vec2(0.0f, 0.0f);
		m_MouseSelection.m_Selecting = false;
		m_Hidden = false;
		m_pEmptyText = nullptr;
		m_WasRendered = false;
	}
	if(m_pStr && m_pStr != pLastStr)
		UpdateStrData();
}

void CLineInput::Clear()
{
	mem_zero(m_pStr, m_MaxSize);
	UpdateStrData();
}

void CLineInput::Set(const char *pString)
{
	str_copy(m_pStr, pString, m_MaxSize);
	UpdateStrData();
	SetCursorOffset(m_Len);
}

void CLineInput::SetRange(const char *pString, size_t Begin, size_t End)
{
	if(Begin > End)
		std::swap(Begin, End);
	Begin = clamp<size_t>(Begin, 0, m_Len);
	End = clamp<size_t>(End, 0, m_Len);

	size_t RemovedCharSize, RemovedCharCount;
	str_utf8_stats(m_pStr + Begin, End - Begin + 1, m_MaxChars, &RemovedCharSize, &RemovedCharCount);

	size_t AddedCharSize, AddedCharCount;
	str_utf8_stats(pString, m_MaxSize - m_Len + RemovedCharSize, m_MaxChars - m_NumChars + RemovedCharCount, &AddedCharSize, &AddedCharCount);

	if(RemovedCharSize || AddedCharSize)
	{
		if(AddedCharSize < RemovedCharSize)
		{
			if(AddedCharSize)
				mem_copy(m_pStr + Begin, pString, AddedCharSize);
			mem_move(m_pStr + Begin + AddedCharSize, m_pStr + Begin + RemovedCharSize, m_Len - Begin - AddedCharSize);
		}
		else if(AddedCharSize > RemovedCharSize)
			mem_move(m_pStr + End + AddedCharSize - RemovedCharSize, m_pStr + End, m_Len - End);

		if(AddedCharSize >= RemovedCharSize)
			mem_copy(m_pStr + Begin, pString, AddedCharSize);

		m_CursorPos = End - RemovedCharSize + AddedCharSize;
		m_Len += AddedCharSize - RemovedCharSize;
		m_NumChars += AddedCharCount - RemovedCharCount;
		m_WasChanged = true;
		m_WasCursorChanged = true;
		m_pStr[m_Len] = '\0';
		m_SelectionStart = m_SelectionEnd = m_CursorPos;
	}
}

void CLineInput::Insert(const char *pString, size_t Begin)
{
	SetRange(pString, Begin, Begin);
}

void CLineInput::Append(const char *pString)
{
	Insert(pString, m_Len);
}

void CLineInput::UpdateStrData()
{
	str_utf8_stats(m_pStr, m_MaxSize, m_MaxChars, &m_Len, &m_NumChars);
	if(!in_range<size_t>(m_CursorPos, 0, m_Len))
		SetCursorOffset(m_CursorPos);
	if(!in_range<size_t>(m_SelectionStart, 0, m_Len) || !in_range<size_t>(m_SelectionEnd, 0, m_Len))
		SetSelection(m_SelectionStart, m_SelectionEnd);
}

const char *CLineInput::GetDisplayedString()
{
	if(m_pfnDisplayTextCallback)
		return m_pfnDisplayTextCallback(m_pStr, GetNumChars());

	if(!IsHidden())
		return m_pStr;

	const size_t NumStars = minimum(GetNumChars(), sizeof(ms_aStars) - 1);
	for(size_t i = 0; i < NumStars; ++i)
		ms_aStars[i] = '*';
	ms_aStars[NumStars] = '\0';
	return ms_aStars;
}

void CLineInput::MoveCursor(EMoveDirection Direction, bool MoveWord, const char *pStr, size_t MaxSize, size_t *pCursorPos)
{
	// Check whether cursor position is initially on space or non-space character.
	// When forwarding, check character to the right of the cursor position.
	// When rewinding, check character to the left of the cursor position (rewind first).
	size_t PeekCursorPos = Direction == FORWARD ? *pCursorPos : str_utf8_rewind(pStr, *pCursorPos);
	const char *pTemp = pStr + PeekCursorPos;
	bool AnySpace = str_utf8_isspace(str_utf8_decode(&pTemp));
	bool AnyWord = !AnySpace;
	while(true)
	{
		if(Direction == FORWARD)
			*pCursorPos = str_utf8_forward(pStr, *pCursorPos);
		else
			*pCursorPos = str_utf8_rewind(pStr, *pCursorPos);
		if(!MoveWord || *pCursorPos <= 0 || *pCursorPos >= MaxSize)
			break;
		PeekCursorPos = Direction == FORWARD ? *pCursorPos : str_utf8_rewind(pStr, *pCursorPos);
		pTemp = pStr + PeekCursorPos;
		const bool CurrentSpace = str_utf8_isspace(str_utf8_decode(&pTemp));
		const bool CurrentWord = !CurrentSpace;
		if(Direction == FORWARD && AnySpace && !CurrentSpace)
			break; // Forward: Stop when next (right) character is non-space after seeing at least one space character.
		else if(Direction == REWIND && AnyWord && !CurrentWord)
			break; // Rewind: Stop when next (left) character is space after seeing at least one non-space character.
		AnySpace |= CurrentSpace;
		AnyWord |= CurrentWord;
	}
}

void CLineInput::SetCursorOffset(size_t Offset)
{
	m_SelectionStart = m_SelectionEnd = m_LastCompositionCursorPos = m_CursorPos = clamp<size_t>(Offset, 0, m_Len);
	m_WasCursorChanged = true;
}

void CLineInput::SetSelection(size_t Start, size_t End)
{
	dbg_assert(m_CursorPos == Start || m_CursorPos == End, "Selection and cursor offset got desynchronized");
	if(Start > End)
		std::swap(Start, End);
	m_SelectionStart = clamp<size_t>(Start, 0, m_Len);
	m_SelectionEnd = clamp<size_t>(End, 0, m_Len);
	m_WasCursorChanged = true;
}

size_t CLineInput::OffsetFromActualToDisplay(size_t ActualOffset)
{
	if(IsHidden() || (m_pfnCalculateOffsetCallback && m_pfnCalculateOffsetCallback()))
		return str_utf8_offset_bytes_to_chars(m_pStr, ActualOffset);
	return ActualOffset;
}

size_t CLineInput::OffsetFromDisplayToActual(size_t DisplayOffset)
{
	if(IsHidden() || (m_pfnCalculateOffsetCallback && m_pfnCalculateOffsetCallback()))
		return str_utf8_offset_bytes_to_chars(m_pStr, DisplayOffset);
	return DisplayOffset;
}

bool CLineInput::ProcessInput(const IInput::CEvent &Event)
{
	// update derived attributes to handle external changes to the buffer
	UpdateStrData();

	const size_t OldCursorPos = m_CursorPos;
	const bool Selecting = Input()->ShiftIsPressed();
	const size_t SelectionLength = GetSelectionLength();
	bool KeyHandled = false;

	if(Event.m_Flags & IInput::FLAG_TEXT)
	{
		SetRange(Event.m_aText, m_SelectionStart, m_SelectionEnd);
		KeyHandled = true;
	}

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		const bool ModPressed = Input()->ModifierIsPressed();
		const bool AltPressed = Input()->AltIsPressed();

#ifdef CONF_PLATFORM_MACOSX
		const bool MoveWord = AltPressed && !ModPressed;
#else
		const bool MoveWord = ModPressed && !AltPressed;
#endif

		if(Event.m_Key == KEY_BACKSPACE)
		{
			if(SelectionLength)
			{
				SetRange("", m_SelectionStart, m_SelectionEnd);
			}
			else
			{
				// If in MoveWord-mode, backspace will delete the word before the selection
				if(SelectionLength)
					m_SelectionEnd = m_CursorPos = m_SelectionStart;
				if(m_CursorPos > 0)
				{
					size_t NewCursorPos = m_CursorPos;
					MoveCursor(REWIND, MoveWord, m_pStr, m_Len, &NewCursorPos);
					SetRange("", NewCursorPos, m_CursorPos);
				}
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
			KeyHandled = true;
		}
		else if(Event.m_Key == KEY_DELETE)
		{
			if(SelectionLength)
			{
				SetRange("", m_SelectionStart, m_SelectionEnd);
			}
			else
			{
				// If in MoveWord-mode, delete will delete the word after the selection
				if(SelectionLength)
					m_SelectionStart = m_CursorPos = m_SelectionEnd;
				if(m_CursorPos < m_Len)
				{
					size_t EndCursorPos = m_CursorPos;
					MoveCursor(FORWARD, MoveWord, m_pStr, m_Len, &EndCursorPos);
					SetRange("", m_CursorPos, EndCursorPos);
				}
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
			KeyHandled = true;
		}
		else if(Event.m_Key == KEY_LEFT)
		{
			if(SelectionLength && !Selecting)
			{
				m_CursorPos = m_SelectionStart;
			}
			else if(m_CursorPos > 0)
			{
				MoveCursor(REWIND, MoveWord, m_pStr, m_Len, &m_CursorPos);
				if(Selecting)
				{
					if(m_SelectionStart == OldCursorPos) // expand start first
						m_SelectionStart = m_CursorPos;
					else if(m_SelectionEnd == OldCursorPos)
						m_SelectionEnd = m_CursorPos;
				}
			}

			if(!Selecting)
			{
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
			KeyHandled = true;
		}
		else if(Event.m_Key == KEY_RIGHT)
		{
			if(SelectionLength && !Selecting)
			{
				m_CursorPos = m_SelectionEnd;
			}
			else if(m_CursorPos < m_Len)
			{
				MoveCursor(FORWARD, MoveWord, m_pStr, m_Len, &m_CursorPos);
				if(Selecting)
				{
					if(m_SelectionEnd == OldCursorPos) // expand end first
						m_SelectionEnd = m_CursorPos;
					else if(m_SelectionStart == OldCursorPos)
						m_SelectionStart = m_CursorPos;
				}
			}

			if(!Selecting)
			{
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
			KeyHandled = true;
		}
		else if(Event.m_Key == KEY_HOME)
		{
			if(Selecting)
			{
				if(SelectionLength && m_CursorPos == m_SelectionEnd)
					m_SelectionEnd = m_SelectionStart;
			}
			else
				m_SelectionEnd = 0;
			m_CursorPos = 0;
			m_SelectionStart = 0;
			KeyHandled = true;
		}
		else if(Event.m_Key == KEY_END)
		{
			if(Selecting)
			{
				if(SelectionLength && m_CursorPos == m_SelectionStart)
					m_SelectionStart = m_SelectionEnd;
			}
			else
				m_SelectionStart = m_Len;
			m_CursorPos = m_Len;
			m_SelectionEnd = m_Len;
			KeyHandled = true;
		}
		else if(ModPressed && !AltPressed && Event.m_Key == KEY_V)
		{
			std::string ClipboardText = Input()->GetClipboardText();
			if(!ClipboardText.empty())
			{
				if(m_pfnClipboardLineCallback)
				{
					// Split clipboard text into multiple lines. Send all complete lines to callback.
					// The lineinput is set to the last clipboard line.
					bool FirstLine = true;
					size_t i, Begin = 0;
					for(i = 0; i < ClipboardText.length(); i++)
					{
						if(ClipboardText[i] == '\n')
						{
							if(i == Begin)
							{
								Begin++;
								continue;
							}
							std::string Line = ClipboardText.substr(Begin, i - Begin + 1);
							if(FirstLine)
							{
								str_sanitize_cc(Line.data());
								SetRange(Line.c_str(), m_SelectionStart, m_SelectionEnd);
								FirstLine = false;
								Line = GetString();
							}
							Begin = i + 1;
							str_sanitize_cc(Line.data());
							m_pfnClipboardLineCallback(Line.c_str());
						}
					}
					std::string Line = ClipboardText.substr(Begin, i - Begin + 1);
					str_sanitize_cc(Line.data());
					if(FirstLine)
						SetRange(Line.c_str(), m_SelectionStart, m_SelectionEnd);
					else
						Set(Line.c_str());
				}
				else
				{
					str_sanitize_cc(ClipboardText.data());
					SetRange(ClipboardText.c_str(), m_SelectionStart, m_SelectionEnd);
				}
			}
			KeyHandled = true;
		}
		else if(ModPressed && !AltPressed && (Event.m_Key == KEY_C || Event.m_Key == KEY_X) && SelectionLength)
		{
			char *pSelection = m_pStr + m_SelectionStart;
			const char TempChar = pSelection[SelectionLength];
			pSelection[SelectionLength] = '\0';
			Input()->SetClipboardText(pSelection);
			pSelection[SelectionLength] = TempChar;
			if(Event.m_Key == KEY_X)
				SetRange("", m_SelectionStart, m_SelectionEnd);
			KeyHandled = true;
		}
		else if(ModPressed && !AltPressed && Event.m_Key == KEY_A)
		{
			m_SelectionStart = 0;
			m_SelectionEnd = m_CursorPos = m_Len;
			KeyHandled = true;
		}
	}

	m_WasCursorChanged |= OldCursorPos != m_CursorPos;
	m_WasCursorChanged |= SelectionLength != GetSelectionLength();
	return KeyHandled;
}

STextBoundingBox CLineInput::Render(const CUIRect *pRect, float FontSize, int Align, bool Changed, float LineWidth, float LineSpacing, const std::vector<STextColorSplit> &vColorSplits)
{
	// update derived attributes to handle external changes to the buffer
	UpdateStrData();

	m_WasRendered = true;

	const char *pDisplayStr = GetDisplayedString();
	const bool HasComposition = Input()->HasComposition();

	if(pDisplayStr[0] == '\0' && !HasComposition && m_pEmptyText != nullptr)
	{
		pDisplayStr = m_pEmptyText;
		m_MouseSelection.m_Selecting = false;
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.75f);
	}

	CTextCursor Cursor;
	if(IsActive())
	{
		const size_t CursorOffset = GetCursorOffset();
		const size_t DisplayCursorOffset = OffsetFromActualToDisplay(CursorOffset);
		const size_t CompositionStart = CursorOffset + Input()->GetCompositionCursor();
		const size_t DisplayCompositionStart = OffsetFromActualToDisplay(CompositionStart);
		const size_t CaretOffset = HasComposition ? DisplayCompositionStart : DisplayCursorOffset;

		std::string DisplayStrBuffer;
		if(HasComposition)
		{
			const std::string DisplayStr = std::string(pDisplayStr);
			DisplayStrBuffer = DisplayStr.substr(0, DisplayCursorOffset) + Input()->GetComposition() + DisplayStr.substr(DisplayCursorOffset);
			pDisplayStr = DisplayStrBuffer.c_str();
		}

		const STextBoundingBox BoundingBox = TextRender()->TextBoundingBox(FontSize, pDisplayStr, -1, LineWidth, LineSpacing);
		const vec2 CursorPos = CUi::CalcAlignedCursorPos(pRect, BoundingBox.Size(), Align);

		TextRender()->SetCursor(&Cursor, CursorPos.x, CursorPos.y, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;
		Cursor.m_ForceCursorRendering = Changed;
		Cursor.m_LineSpacing = LineSpacing;
		Cursor.m_PressMouse.x = m_MouseSelection.m_PressMouse.x;
		Cursor.m_ReleaseMouse.x = m_MouseSelection.m_ReleaseMouse.x;
		Cursor.m_vColorSplits = vColorSplits;
		if(LineWidth < 0.0f)
		{
			// Using a Y position that's always inside the line input makes it so the selection does not reset when
			// the mouse is moved outside the line input while selecting, which would otherwise be very inconvenient.
			// This is a single line cursor, so we don't need the Y position to support selection over multiple lines.
			Cursor.m_PressMouse.y = CursorPos.y + BoundingBox.m_H / 2.0f;
			Cursor.m_ReleaseMouse.y = CursorPos.y + BoundingBox.m_H / 2.0f;
		}
		else
		{
			Cursor.m_PressMouse.y = m_MouseSelection.m_PressMouse.y;
			Cursor.m_ReleaseMouse.y = m_MouseSelection.m_ReleaseMouse.y;
		}

		if(HasComposition)
		{
			// We need to track the last composition cursor position separately, because the composition
			// cursor movement does not cause an input event that would set the Changed variable.
			Cursor.m_ForceCursorRendering |= m_LastCompositionCursorPos != CaretOffset;
			m_LastCompositionCursorPos = CaretOffset;
			const size_t DisplayCompositionEnd = DisplayCursorOffset + Input()->GetCompositionLength();
			Cursor.m_CursorMode = TEXT_CURSOR_CURSOR_MODE_SET;
			Cursor.m_CursorCharacter = str_utf8_offset_bytes_to_chars(pDisplayStr, CaretOffset);
			Cursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_SET;
			Cursor.m_SelectionHeightFactor = 0.1f;
			Cursor.m_SelectionStart = str_utf8_offset_bytes_to_chars(pDisplayStr, DisplayCursorOffset);
			Cursor.m_SelectionEnd = str_utf8_offset_bytes_to_chars(pDisplayStr, DisplayCompositionEnd);
			TextRender()->TextSelectionColor(1.0f, 1.0f, 1.0f, 0.8f);
			TextRender()->TextEx(&Cursor, pDisplayStr);
			TextRender()->TextSelectionColor(TextRender()->DefaultTextSelectionColor());
		}
		else if(GetSelectionLength())
		{
			const size_t Start = OffsetFromActualToDisplay(GetSelectionStart());
			const size_t End = OffsetFromActualToDisplay(GetSelectionEnd());
			Cursor.m_CursorMode = m_MouseSelection.m_Selecting ? TEXT_CURSOR_CURSOR_MODE_CALCULATE : TEXT_CURSOR_CURSOR_MODE_SET;
			Cursor.m_CursorCharacter = str_utf8_offset_bytes_to_chars(pDisplayStr, CaretOffset);
			Cursor.m_CalculateSelectionMode = m_MouseSelection.m_Selecting ? TEXT_CURSOR_SELECTION_MODE_CALCULATE : TEXT_CURSOR_SELECTION_MODE_SET;
			Cursor.m_SelectionStart = str_utf8_offset_bytes_to_chars(pDisplayStr, Start);
			Cursor.m_SelectionEnd = str_utf8_offset_bytes_to_chars(pDisplayStr, End);
			TextRender()->TextEx(&Cursor, pDisplayStr);
		}
		else
		{
			Cursor.m_CursorMode = m_MouseSelection.m_Selecting ? TEXT_CURSOR_CURSOR_MODE_CALCULATE : TEXT_CURSOR_CURSOR_MODE_SET;
			Cursor.m_CursorCharacter = str_utf8_offset_bytes_to_chars(pDisplayStr, CaretOffset);
			Cursor.m_CalculateSelectionMode = m_MouseSelection.m_Selecting ? TEXT_CURSOR_SELECTION_MODE_CALCULATE : TEXT_CURSOR_SELECTION_MODE_NONE;
			TextRender()->TextEx(&Cursor, pDisplayStr);
		}

		if(Cursor.m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE && Cursor.m_CursorCharacter >= 0)
		{
			const size_t NewCursorOffset = str_utf8_offset_chars_to_bytes(pDisplayStr, Cursor.m_CursorCharacter);
			SetCursorOffset(OffsetFromDisplayToActual(NewCursorOffset));
		}
		if(Cursor.m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE && Cursor.m_SelectionStart >= 0 && Cursor.m_SelectionEnd >= 0)
		{
			const size_t NewSelectionStart = str_utf8_offset_chars_to_bytes(pDisplayStr, Cursor.m_SelectionStart);
			const size_t NewSelectionEnd = str_utf8_offset_chars_to_bytes(pDisplayStr, Cursor.m_SelectionEnd);
			SetSelection(OffsetFromDisplayToActual(NewSelectionStart), OffsetFromDisplayToActual(NewSelectionEnd));
		}

		m_CaretPosition = Cursor.m_CursorRenderedPosition;

		CTextCursor CaretCursor;
		TextRender()->SetCursor(&CaretCursor, CursorPos.x, CursorPos.y, FontSize, 0);
		CaretCursor.m_LineWidth = LineWidth;
		CaretCursor.m_LineSpacing = LineSpacing;
		CaretCursor.m_CursorMode = TEXT_CURSOR_CURSOR_MODE_SET;
		CaretCursor.m_CursorCharacter = str_utf8_offset_bytes_to_chars(pDisplayStr, DisplayCursorOffset);
		TextRender()->TextEx(&CaretCursor, pDisplayStr);
		SetCompositionWindowPosition(CaretCursor.m_CursorRenderedPosition + vec2(0.0f, CaretCursor.m_AlignedFontSize / 2.0f), CaretCursor.m_AlignedFontSize);
	}
	else
	{
		const STextBoundingBox BoundingBox = TextRender()->TextBoundingBox(FontSize, pDisplayStr, -1, LineWidth, LineSpacing);
		const vec2 CursorPos = CUi::CalcAlignedCursorPos(pRect, BoundingBox.Size(), Align);
		TextRender()->SetCursor(&Cursor, CursorPos.x, CursorPos.y, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;
		Cursor.m_LineSpacing = LineSpacing;
		Cursor.m_vColorSplits = vColorSplits;
		TextRender()->TextEx(&Cursor, pDisplayStr);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());

	return Cursor.BoundingBox();
}

void CLineInput::RenderCandidates()
{
	// Check if the active line input was not rendered and deactivate it in that case.
	// This can happen e.g. when an input in the ingame menu is active and the menu is
	// closed or when switching between menu and editor with an active input.
	CLineInput *pActiveInput = GetActiveInput();
	if(pActiveInput != nullptr)
	{
		if(pActiveInput->m_WasRendered)
		{
			pActiveInput->m_WasRendered = false;
		}
		else
		{
			pActiveInput->Deactivate();
			return;
		}
	}

	if(!Input()->HasComposition() || !Input()->GetCandidateCount())
		return;

	const float FontSize = 7.0f;
	const float Padding = 1.0f;
	const float Margin = 4.0f;
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	const int ScreenWidth = Graphics()->ScreenWidth();
	const int ScreenHeight = Graphics()->ScreenHeight();

	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	// Determine longest candidate width
	float LongestCandidateWidth = 0.0f;
	for(int i = 0; i < Input()->GetCandidateCount(); ++i)
		LongestCandidateWidth = maximum(LongestCandidateWidth, TextRender()->TextWidth(FontSize, Input()->GetCandidate(i)));

	const float NumOffset = 8.0f;
	const float RectWidth = LongestCandidateWidth + Margin + NumOffset + 2.0f * Padding;
	const float RectHeight = Input()->GetCandidateCount() * (FontSize + 2.0f * Padding) + Margin;

	vec2 Position = ms_CompositionWindowPosition / vec2(ScreenWidth, ScreenHeight) * vec2(Width, Height);
	Position.y += Margin;

	// Move candidate window left if needed
	if(Position.x + RectWidth + Margin > Width)
		Position.x -= Position.x + RectWidth + Margin - Width;

	// Move candidate window up if needed
	if(Position.y + RectHeight + Margin > Height)
		Position.y -= RectHeight + ms_CompositionLineHeight / ScreenHeight * Height + 2.0f * Margin;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->BlendNormal();

	// Draw window shadow
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.8f);
	IGraphics::CQuadItem Quad = IGraphics::CQuadItem(Position.x + 0.75f, Position.y + 0.75f, RectWidth, RectHeight);
	Graphics()->QuadsDrawTL(&Quad, 1);

	// Draw window background
	Graphics()->SetColor(0.15f, 0.15f, 0.15f, 1.0f);
	Quad = IGraphics::CQuadItem(Position.x, Position.y, RectWidth, RectHeight);
	Graphics()->QuadsDrawTL(&Quad, 1);

	// Draw selected entry highlight
	Graphics()->SetColor(0.1f, 0.4f, 0.8f, 1.0f);
	Quad = IGraphics::CQuadItem(Position.x + Margin / 4.0f, Position.y + Margin / 2.0f + Input()->GetCandidateSelectedIndex() * (FontSize + 2.0f * Padding), RectWidth - Margin / 2.0f, FontSize + 2.0f * Padding);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();

	// Draw candidates
	for(int i = 0; i < Input()->GetCandidateCount(); ++i)
	{
		char aBuf[3];
		str_format(aBuf, sizeof(aBuf), "%d.", (i + 1) % 10);

		const float PosX = Position.x + Margin / 2.0f + Padding;
		const float PosY = Position.y + Margin / 2.0f + i * (FontSize + 2.0f * Padding) + Padding;
		TextRender()->TextColor(0.6f, 0.6f, 0.6f, 1.0f);
		TextRender()->Text(PosX, PosY, FontSize, aBuf);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		TextRender()->Text(PosX + NumOffset, PosY, FontSize, Input()->GetCandidate(i));
	}
}

void CLineInput::SetCompositionWindowPosition(vec2 Anchor, float LineHeight)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	const int ScreenWidth = Graphics()->ScreenWidth();
	const int ScreenHeight = Graphics()->ScreenHeight();
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	const vec2 ScreenScale = vec2(ScreenWidth / (ScreenX1 - ScreenX0), ScreenHeight / (ScreenY1 - ScreenY0));
	ms_CompositionWindowPosition = Anchor * ScreenScale;
	ms_CompositionLineHeight = LineHeight * ScreenScale.y;
	Input()->SetCompositionWindowPosition(ms_CompositionWindowPosition.x, ms_CompositionWindowPosition.y, ms_CompositionLineHeight);
}

void CLineInput::Activate(EInputPriority Priority)
{
	if(IsActive())
		return;
	if(ms_ActiveInputPriority != EInputPriority::NONE && Priority < ms_ActiveInputPriority)
		return; // do not replace a higher priority input
	if(ms_pActiveInput)
		ms_pActiveInput->OnDeactivate();
	ms_pActiveInput = this;
	ms_pActiveInput->OnActivate();
	ms_ActiveInputPriority = Priority;
}

void CLineInput::Deactivate() const
{
	if(!IsActive())
		return;
	ms_pActiveInput->OnDeactivate();
	ms_pActiveInput = nullptr;
	ms_ActiveInputPriority = EInputPriority::NONE;
}

void CLineInput::OnActivate()
{
	Input()->StartTextInput();
}

void CLineInput::OnDeactivate()
{
	Input()->StopTextInput();
	m_MouseSelection.m_Selecting = false;
}

void CLineInputNumber::SetInteger(int Number, int Base, int HexPrefix)
{
	char aBuf[32];
	switch(Base)
	{
	case 10:
		str_format(aBuf, sizeof(aBuf), "%d", Number);
		break;
	case 16:
		str_format(aBuf, sizeof(aBuf), "%0*X", HexPrefix, Number);
		break;
	default:
		dbg_assert(false, "Base unsupported");
		return;
	}
	if(str_comp(aBuf, GetString()) != 0)
		Set(aBuf);
}

int CLineInputNumber::GetInteger(int Base) const
{
	return str_toint_base(GetString(), Base);
}

void CLineInputNumber::SetInteger64(int64_t Number, int Base, int HexPrefix)
{
	char aBuf[64];
	switch(Base)
	{
	case 10:
		str_format(aBuf, sizeof(aBuf), "%" PRId64, Number);
		break;
	case 16:
		str_format(aBuf, sizeof(aBuf), "%0*" PRIX64, HexPrefix, Number);
		break;
	default:
		dbg_assert(false, "Base unsupported");
		return;
	}
	if(str_comp(aBuf, GetString()) != 0)
		Set(aBuf);
}

int64_t CLineInputNumber::GetInteger64(int Base) const
{
	return str_toint64_base(GetString(), Base);
}

void CLineInputNumber::SetFloat(float Number)
{
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.3f", Number);
	if(str_comp(aBuf, GetString()) != 0)
		Set(aBuf);
}

float CLineInputNumber::GetFloat() const
{
	return str_tofloat(GetString());
}
