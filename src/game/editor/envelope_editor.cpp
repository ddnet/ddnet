/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "editor.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>
#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/map.h>

static const char *const CURVE_TYPE_NAMES[] = {"Step", "Linear", "Slow", "Fast", "Smooth", "Bezier"};
static const char *const CURVE_TYPE_NAMES_SHORT[] = {"N", "L", "S", "F", "M", "B"};
static_assert(std::size(CURVE_TYPE_NAMES) == NUM_CURVETYPES);
static_assert(std::size(CURVE_TYPE_NAMES_SHORT) == NUM_CURVETYPES);

static const char *CurveTypeNameShort(int CurveType)
{
	if(in_range<int>(CurveType, 0, std::size(CURVE_TYPE_NAMES_SHORT) - 1))
	{
		return CURVE_TYPE_NAMES_SHORT[CurveType];
	}
	return "!?";
}

static float ClampDelta(float Val, float Delta, float Min, float Max)
{
	if(Val + Delta <= Min)
		return Min - Val;
	if(Val + Delta >= Max)
		return Max - Val;
	return Delta;
}

class CTimeStep
{
public:
	template<class T>
	CTimeStep(T t)
	{
		if constexpr(std::is_same_v<T, std::chrono::milliseconds>)
			m_Unit = ETimeUnit::MILLISECONDS;
		else if constexpr(std::is_same_v<T, std::chrono::seconds>)
			m_Unit = ETimeUnit::SECONDS;
		else
			m_Unit = ETimeUnit::MINUTES;

		m_Value = t;
	}

	CTimeStep operator*(int k) const
	{
		return CTimeStep(m_Value * k, m_Unit);
	}

	CTimeStep operator-(const CTimeStep &Other)
	{
		return CTimeStep(m_Value - Other.m_Value, m_Unit);
	}

	void Format(char *pBuffer, size_t BufferSize)
	{
		int Milliseconds = m_Value.count() % 1000;
		int Seconds = std::chrono::duration_cast<std::chrono::seconds>(m_Value).count() % 60;
		int Minutes = std::chrono::duration_cast<std::chrono::minutes>(m_Value).count();

		switch(m_Unit)
		{
		case ETimeUnit::MILLISECONDS:
			if(Minutes != 0)
				str_format(pBuffer, BufferSize, "%d:%02d.%03dmin", Minutes, Seconds, Milliseconds);
			else if(Seconds != 0)
				str_format(pBuffer, BufferSize, "%d.%03ds", Seconds, Milliseconds);
			else
				str_format(pBuffer, BufferSize, "%dms", Milliseconds);
			break;
		case ETimeUnit::SECONDS:
			if(Minutes != 0)
				str_format(pBuffer, BufferSize, "%d:%02dmin", Minutes, Seconds);
			else
				str_format(pBuffer, BufferSize, "%ds", Seconds);
			break;
		case ETimeUnit::MINUTES:
			str_format(pBuffer, BufferSize, "%dmin", Minutes);
			break;
		}
	}

	float AsSeconds() const
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(m_Value).count();
	}

private:
	enum class ETimeUnit
	{
		MILLISECONDS,
		SECONDS,
		MINUTES,
	} m_Unit;
	std::chrono::milliseconds m_Value;

	CTimeStep(std::chrono::milliseconds Value, ETimeUnit Unit)
	{
		m_Value = Value;
		m_Unit = Unit;
	}
};

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	Map()->m_SelectedEnvelope = Map()->m_vpEnvelopes.empty() ? -1 : std::clamp(Map()->m_SelectedEnvelope, 0, (int)Map()->m_vpEnvelopes.size() - 1);
	std::shared_ptr<CEnvelope> pEnvelope = Map()->m_vpEnvelopes.empty() ? nullptr : Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];

	static EEnvelopeEditorOp s_Operation = EEnvelopeEditorOp::NONE;
	static std::vector<float> s_vAccurateDragValuesX = {};
	static std::vector<float> s_vAccurateDragValuesY = {};
	static float s_MouseXStart = 0.0f;
	static float s_MouseYStart = 0.0f;

	static CLineInput s_NameInput;

	CUIRect ToolBar, CurveBar, ColorBar, DragBar;
	View.HSplitTop(30.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	DoEditorDragBar(View, &DragBar, EDragSide::TOP, &m_aExtraEditorSplits[EXTRAEDITOR_ENVELOPES]);
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	bool CurrentEnvelopeSwitched = false;

	// do the toolbar
	static int s_ActiveChannels = 0xf;
	{
		CUIRect Button;

		// redo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		static int s_RedoButton = 0;
		if(DoButton_FontIcon(&s_RedoButton, FontIcon::REDO, Map()->m_EnvelopeEditorHistory.CanRedo() ? 0 : -1, &Button, BUTTONFLAG_LEFT, "[Ctrl+Y] Redo the last action.", IGraphics::CORNER_R, 11.0f) == 1)
		{
			Map()->m_EnvelopeEditorHistory.Redo();
		}

		// undo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
		static int s_UndoButton = 0;
		if(DoButton_FontIcon(&s_UndoButton, FontIcon::UNDO, Map()->m_EnvelopeEditorHistory.CanUndo() ? 0 : -1, &Button, BUTTONFLAG_LEFT, "[Ctrl+Z] Undo the last action.", IGraphics::CORNER_L, 11.0f) == 1)
		{
			Map()->m_EnvelopeEditorHistory.Undo();
		}

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_NewSoundButton = 0;
		if(DoButton_Editor(&s_NewSoundButton, "Sound+", 0, &Button, BUTTONFLAG_LEFT, "Create a new sound envelope."))
		{
			Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEnvelopeAdd>(Map(), CEnvelope::EType::SOUND));
			pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
			CurrentEnvelopeSwitched = true;
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, BUTTONFLAG_LEFT, "Create a new color envelope."))
		{
			Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEnvelopeAdd>(Map(), CEnvelope::EType::COLOR));
			pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
			CurrentEnvelopeSwitched = true;
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, BUTTONFLAG_LEFT, "Create a new position envelope."))
		{
			Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEnvelopeAdd>(Map(), CEnvelope::EType::POSITION));
			pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
			CurrentEnvelopeSwitched = true;
		}

		if(Map()->m_SelectedEnvelope >= 0)
		{
			// Delete button
			ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_DeleteButton = 0;
			if(DoButton_Editor(&s_DeleteButton, "✗", 0, &Button, BUTTONFLAG_LEFT, "Delete this envelope."))
			{
				auto vpObjectReferences = Map()->DeleteEnvelope(Map()->m_SelectedEnvelope);
				Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeDelete>(Map(), Map()->m_SelectedEnvelope, vpObjectReferences, pEnvelope));

				Map()->m_SelectedEnvelope = Map()->m_vpEnvelopes.empty() ? -1 : std::clamp(Map()->m_SelectedEnvelope, 0, (int)Map()->m_vpEnvelopes.size() - 1);
				pEnvelope = Map()->m_vpEnvelopes.empty() ? nullptr : Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
				Map()->OnModify();
			}
		}

		// check again, because the last envelope might has been deleted
		if(Map()->m_SelectedEnvelope >= 0)
		{
			// Move right button
			ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveRightButton = 0;
			if(DoButton_Ex(&s_MoveRightButton, "→", (Map()->m_SelectedEnvelope >= (int)Map()->m_vpEnvelopes.size() - 1 ? -1 : 0), &Button, BUTTONFLAG_LEFT, "Move this envelope to the right.", IGraphics::CORNER_R))
			{
				int MoveTo = Map()->m_SelectedEnvelope + 1;
				int MoveFrom = Map()->m_SelectedEnvelope;
				Map()->m_SelectedEnvelope = Map()->MoveEnvelope(MoveFrom, MoveTo);
				if(Map()->m_SelectedEnvelope != MoveFrom)
				{
					Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEdit>(Map(), Map()->m_SelectedEnvelope, CEditorActionEnvelopeEdit::EEditType::ORDER, MoveFrom, Map()->m_SelectedEnvelope));
					pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
					Map()->OnModify();
				}
			}

			// Move left button
			ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
			static int s_MoveLeftButton = 0;
			if(DoButton_Ex(&s_MoveLeftButton, "←", (Map()->m_SelectedEnvelope <= 0 ? -1 : 0), &Button, BUTTONFLAG_LEFT, "Move this envelope to the left.", IGraphics::CORNER_L))
			{
				int MoveTo = Map()->m_SelectedEnvelope - 1;
				int MoveFrom = Map()->m_SelectedEnvelope;
				Map()->m_SelectedEnvelope = Map()->MoveEnvelope(MoveFrom, MoveTo);
				if(Map()->m_SelectedEnvelope != MoveFrom)
				{
					Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEdit>(Map(), Map()->m_SelectedEnvelope, CEditorActionEnvelopeEdit::EEditType::ORDER, MoveFrom, Map()->m_SelectedEnvelope));
					pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
					Map()->OnModify();
				}
			}

			if(pEnvelope)
			{
				ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ZoomOutButton = 0;
				if(DoButton_FontIcon(&s_ZoomOutButton, FontIcon::MINUS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad-] Zoom out horizontally, hold shift to zoom vertically.", IGraphics::CORNER_R, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						m_ZoomEnvelopeY.ScaleValue(1.1f);
					else
						m_ZoomEnvelopeX.ScaleValue(1.1f);
				}

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ResetZoomButton = 0;
				if(DoButton_FontIcon(&s_ResetZoomButton, FontIcon::MAGNIFYING_GLASS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad*] Reset zoom to default value.", IGraphics::CORNER_NONE, 9.0f))
					ResetZoomEnvelope(pEnvelope, s_ActiveChannels);

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				static int s_ZoomInButton = 0;
				if(DoButton_FontIcon(&s_ZoomInButton, FontIcon::PLUS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad+] Zoom in horizontally, hold shift to zoom vertically.", IGraphics::CORNER_L, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						m_ZoomEnvelopeY.ScaleValue(1.0f / 1.1f);
					else
						m_ZoomEnvelopeX.ScaleValue(1.0f / 1.1f);
				}
			}

			// Margin on the right side
			ToolBar.VSplitRight(7.0f, &ToolBar, nullptr);
		}

		CUIRect Shifter, Inc, Dec;
		ToolBar.VSplitLeft(60.0f, &Shifter, &ToolBar);
		Shifter.VSplitRight(15.0f, &Shifter, &Inc);
		Shifter.VSplitLeft(15.0f, &Dec, &Shifter);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d/%d", Map()->m_SelectedEnvelope + 1, (int)Map()->m_vpEnvelopes.size());

		ColorRGBA EnvColor = ColorRGBA(1, 1, 1, 0.5f);
		if(!Map()->m_vpEnvelopes.empty())
		{
			EnvColor = Map()->IsEnvelopeUsed(Map()->m_SelectedEnvelope) ? ColorRGBA(1, 0.7f, 0.7f, 0.5f) : ColorRGBA(0.7f, 1, 0.7f, 0.5f);
		}

		static int s_EnvelopeSelector = 0;
		auto NewValueRes = UiDoValueSelector(&s_EnvelopeSelector, &Shifter, aBuf, Map()->m_SelectedEnvelope + 1, 1, Map()->m_vpEnvelopes.size(), 1, 1.0f, "Select the envelope.", false, false, IGraphics::CORNER_NONE, &EnvColor, false);
		int NewValue = NewValueRes.m_Value;
		if(NewValue - 1 != Map()->m_SelectedEnvelope)
		{
			Map()->m_SelectedEnvelope = NewValue - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_PrevButton = 0;
		if(DoButton_FontIcon(&s_PrevButton, FontIcon::MINUS, 0, &Dec, BUTTONFLAG_LEFT, "Select previous envelope.", IGraphics::CORNER_L, 7.0f))
		{
			Map()->m_SelectedEnvelope--;
			if(Map()->m_SelectedEnvelope < 0)
				Map()->m_SelectedEnvelope = Map()->m_vpEnvelopes.size() - 1;
			CurrentEnvelopeSwitched = true;
		}

		static int s_NextButton = 0;
		if(DoButton_FontIcon(&s_NextButton, FontIcon::PLUS, 0, &Inc, BUTTONFLAG_LEFT, "Select next envelope.", IGraphics::CORNER_R, 7.0f))
		{
			Map()->m_SelectedEnvelope++;
			if(Map()->m_SelectedEnvelope >= (int)Map()->m_vpEnvelopes.size())
				Map()->m_SelectedEnvelope = 0;
			CurrentEnvelopeSwitched = true;
		}

		if(pEnvelope)
		{
			ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
			ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);
			Ui()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_MR);

			ToolBar.VSplitLeft(3.0f, nullptr, &ToolBar);
			ToolBar.VSplitLeft(ToolBar.w > ToolBar.h * 40 ? 80.0f : 60.0f, &Button, &ToolBar);

			s_NameInput.SetBuffer(pEnvelope->m_aName, sizeof(pEnvelope->m_aName));
			if(DoEditBox(&s_NameInput, &Button, 10.0f, IGraphics::CORNER_ALL, "The name of the selected envelope."))
			{
				Map()->OnModify();
			}
		}
	}

	const bool ShowColorBar = pEnvelope && pEnvelope->GetChannels() == 4;
	if(ShowColorBar)
	{
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.HMargin(2.0f, &ColorBar);
	}

	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		if(m_ResetZoomEnvelope)
		{
			m_ResetZoomEnvelope = false;
			ResetZoomEnvelope(pEnvelope, s_ActiveChannels);
		}

		ColorRGBA aColors[] = {ColorRGBA(1, 0.2f, 0.2f), ColorRGBA(0.2f, 1, 0.2f), ColorRGBA(0.2f, 0.2f, 1), ColorRGBA(1, 1, 0.2f)};

		CUIRect Button;

		ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

		static const char *s_aapNames[4][CEnvPoint::MAX_CHANNELS] = {
			{"V", "", "", ""},
			{"", "", "", ""},
			{"X", "Y", "R", ""},
			{"R", "G", "B", "A"},
		};

		static const char *s_aapDescriptions[4][CEnvPoint::MAX_CHANNELS] = {
			{"Volume of the envelope.", "", "", ""},
			{"", "", "", ""},
			{"X-axis of the envelope.", "Y-axis of the envelope.", "Rotation of the envelope.", ""},
			{"Red value of the envelope.", "Green value of the envelope.", "Blue value of the envelope.", "Alpha value of the envelope."},
		};

		static int s_aChannelButtons[CEnvPoint::MAX_CHANNELS] = {0};
		int Bit = 1;

		for(int i = 0; i < CEnvPoint::MAX_CHANNELS; i++, Bit <<= 1)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			if(i < pEnvelope->GetChannels())
			{
				int Corners = IGraphics::CORNER_NONE;
				if(pEnvelope->GetChannels() == 1)
					Corners = IGraphics::CORNER_ALL;
				else if(i == 0)
					Corners = IGraphics::CORNER_L;
				else if(i == pEnvelope->GetChannels() - 1)
					Corners = IGraphics::CORNER_R;

				if(DoButton_Env(&s_aChannelButtons[i], s_aapNames[pEnvelope->GetChannels() - 1][i], s_ActiveChannels & Bit, &Button, s_aapDescriptions[pEnvelope->GetChannels() - 1][i], aColors[i], Corners))
					s_ActiveChannels ^= Bit;
			}
		}

		ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
		ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);

		static int s_EnvelopeEditorId = 0;
		static int s_EnvelopeEditorButtonUsed = -1;
		const bool ShouldPan = s_Operation == EEnvelopeEditorOp::NONE && (Ui()->MouseButton(2) || (Ui()->MouseButton(0) && Input()->ModifierIsPressed()));
		if(m_pContainerPanned == &s_EnvelopeEditorId)
		{
			if(!ShouldPan)
				m_pContainerPanned = nullptr;
			else
			{
				m_OffsetEnvelopeX += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w;
				m_OffsetEnvelopeY -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h;
			}
		}

		if(Ui()->MouseInside(&View) && m_Dialog == DIALOG_NONE)
		{
			Ui()->SetHotItem(&s_EnvelopeEditorId);

			if(ShouldPan && m_pContainerPanned == nullptr)
				m_pContainerPanned = &s_EnvelopeEditorId;

			if(Input()->KeyPress(KEY_KP_MULTIPLY) && CLineInput::GetActiveInput() == nullptr)
				ResetZoomEnvelope(pEnvelope, s_ActiveChannels);
			if(Input()->ShiftIsPressed())
			{
				if(Input()->KeyPress(KEY_KP_MINUS) && CLineInput::GetActiveInput() == nullptr)
					m_ZoomEnvelopeY.ScaleValue(1.1f);
				if(Input()->KeyPress(KEY_KP_PLUS) && CLineInput::GetActiveInput() == nullptr)
					m_ZoomEnvelopeY.ScaleValue(1.0f / 1.1f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeY.ScaleValue(1.1f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeY.ScaleValue(1.0f / 1.1f);
			}
			else
			{
				if(Input()->KeyPress(KEY_KP_MINUS) && CLineInput::GetActiveInput() == nullptr)
					m_ZoomEnvelopeX.ScaleValue(1.1f);
				if(Input()->KeyPress(KEY_KP_PLUS) && CLineInput::GetActiveInput() == nullptr)
					m_ZoomEnvelopeX.ScaleValue(1.0f / 1.1f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ZoomEnvelopeX.ScaleValue(1.1f);
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ZoomEnvelopeX.ScaleValue(1.0f / 1.1f);
			}
		}

		if(Ui()->HotItem() == &s_EnvelopeEditorId)
		{
			// do stuff
			if(Ui()->MouseButton(0))
			{
				s_EnvelopeEditorButtonUsed = 0;
				if(s_Operation != EEnvelopeEditorOp::BOX_SELECT && !Input()->ModifierIsPressed())
				{
					s_Operation = EEnvelopeEditorOp::BOX_SELECT;
					s_MouseXStart = Ui()->MouseX();
					s_MouseYStart = Ui()->MouseY();
				}
			}
			else if(s_EnvelopeEditorButtonUsed == 0)
			{
				if(Ui()->DoDoubleClickLogic(&s_EnvelopeEditorId) && !Input()->ModifierIsPressed())
				{
					// add point
					float Time = ScreenToEnvelopeX(View, Ui()->MouseX());
					ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					pEnvelope->Eval(std::clamp(Time, 0.0f, pEnvelope->EndTime()), Channels, 4);

					const CFixedTime FixedTime = CFixedTime::FromSeconds(Time);
					bool TimeFound = false;
					for(CEnvPoint &Point : pEnvelope->m_vPoints)
					{
						if(Point.m_Time == FixedTime)
							TimeFound = true;
					}

					if(!TimeFound)
						Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionAddEnvelopePoint>(Map(), Map()->m_SelectedEnvelope, FixedTime, Channels));

					if(FixedTime < CFixedTime(0))
						RemoveTimeOffsetEnvelope(pEnvelope);
					Map()->OnModify();
				}
				s_EnvelopeEditorButtonUsed = -1;
			}

			m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;
			str_copy(m_aTooltip, "Double click to create a new point. Use shift to change the zoom axis. Press S to scale selected envelope points.");
		}

		UpdateZoomEnvelopeX(View);
		UpdateZoomEnvelopeY(View);

		{
			float UnitsPerLineY = 0.001f;
			static const float s_aUnitPerLineOptionsY[] = {0.005f, 0.01f, 0.025f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 2 * 32.0f, 5 * 32.0f, 10 * 32.0f, 20 * 32.0f, 50 * 32.0f, 100 * 32.0f};
			for(float Value : s_aUnitPerLineOptionsY)
			{
				if(Value / m_ZoomEnvelopeY.GetValue() * View.h < 40.0f)
					UnitsPerLineY = Value;
			}
			int NumLinesY = m_ZoomEnvelopeY.GetValue() / UnitsPerLineY + 1;

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			float BaseValue = static_cast<int>(m_OffsetEnvelopeY * m_ZoomEnvelopeY.GetValue() / UnitsPerLineY) * UnitsPerLineY;
			for(int i = 0; i <= NumLinesY; i++)
			{
				float Value = UnitsPerLineY * i - BaseValue;
				IGraphics::CLineItem LineItem(View.x, EnvelopeToScreenY(View, Value), View.x + View.w, EnvelopeToScreenY(View, Value));
				Graphics()->LinesDraw(&LineItem, 1);
			}

			Graphics()->LinesEnd();

			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
			for(int i = 0; i <= NumLinesY; i++)
			{
				float Value = UnitsPerLineY * i - BaseValue;
				char aValueBuffer[16];
				if(UnitsPerLineY >= 1.0f)
				{
					str_format(aValueBuffer, sizeof(aValueBuffer), "%d", static_cast<int>(Value));
				}
				else
				{
					str_format(aValueBuffer, sizeof(aValueBuffer), "%.3f", Value);
				}
				Ui()->TextRender()->Text(View.x, EnvelopeToScreenY(View, Value) + 4.0f, 8.0f, aValueBuffer);
			}
			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Ui()->ClipDisable();
		}

		{
			using namespace std::chrono_literals;
			CTimeStep UnitsPerLineX = 1ms;
			static const CTimeStep s_aUnitPerLineOptionsX[] = {5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2s, 5s, 10s, 15s, 30s, 1min};
			for(CTimeStep Value : s_aUnitPerLineOptionsX)
			{
				if(Value.AsSeconds() / m_ZoomEnvelopeX.GetValue() * View.w < 160.0f)
					UnitsPerLineX = Value;
			}
			int NumLinesX = m_ZoomEnvelopeX.GetValue() / UnitsPerLineX.AsSeconds() + 1;

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			CTimeStep BaseValue = UnitsPerLineX * static_cast<int>(m_OffsetEnvelopeX * m_ZoomEnvelopeX.GetValue() / UnitsPerLineX.AsSeconds());
			for(int i = 0; i <= NumLinesX; i++)
			{
				float Value = UnitsPerLineX.AsSeconds() * i - BaseValue.AsSeconds();
				IGraphics::CLineItem LineItem(EnvelopeToScreenX(View, Value), View.y, EnvelopeToScreenX(View, Value), View.y + View.h);
				Graphics()->LinesDraw(&LineItem, 1);
			}

			Graphics()->LinesEnd();

			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
			for(int i = 0; i <= NumLinesX; i++)
			{
				CTimeStep Value = UnitsPerLineX * i - BaseValue;
				if(Value.AsSeconds() >= 0)
				{
					char aValueBuffer[16];
					Value.Format(aValueBuffer, sizeof(aValueBuffer));

					Ui()->TextRender()->Text(EnvelopeToScreenX(View, Value.AsSeconds()) + 1.0f, View.y + View.h - 8.0f, 8.0f, aValueBuffer);
				}
			}
			Ui()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			Ui()->ClipDisable();
		}

		// render tangents for bezier curves
		{
			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					float PosX = EnvelopeToScreenX(View, pEnvelope->m_vPoints[i].m_Time.AsSeconds());
					float PosY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));

					// Out-Tangent
					if(i < (int)pEnvelope->m_vPoints.size() - 1 && pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
					{
						float TangentX = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]).AsSeconds());
						float TangentY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));

						if(Map()->IsTangentOutPointSelected(i, c))
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(TangentX, TangentY, PosX, PosY);
						Graphics()->LinesDraw(&LineItem, 1);
					}

					// In-Tangent
					if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
					{
						float TangentX = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]).AsSeconds());
						float TangentY = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));

						if(Map()->IsTangentInPointSelected(i, c))
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(TangentX, TangentY, PosX, PosY);
						Graphics()->LinesDraw(&LineItem, 1);
					}
				}
			}
			Graphics()->LinesEnd();
			Ui()->ClipDisable();
		}

		// render lines
		{
			float EndTimeTotal = maximum(0.000001f, pEnvelope->EndTime());
			float EndX = std::clamp(EnvelopeToScreenX(View, EndTimeTotal), View.x, View.x + View.w);
			float StartX = std::clamp(View.x + View.w * m_OffsetEnvelopeX, View.x, View.x + View.w);

			float EndTime = ScreenToEnvelopeX(View, EndX);
			float StartTime = ScreenToEnvelopeX(View, StartX);

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			IGraphics::CLineItemBatch LineItemBatch;
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				Graphics()->LinesBatchBegin(&LineItemBatch);
				if(s_ActiveChannels & (1 << c))
					Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
				else
					Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

				const int Steps = static_cast<int>(((EndX - StartX) / Ui()->Screen()->w) * Graphics()->ScreenWidth());
				const float StepTime = (EndTime - StartTime) / static_cast<float>(Steps);
				const float StepSize = (EndX - StartX) / static_cast<float>(Steps);

				ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				pEnvelope->Eval(StartTime, Channels, c + 1);
				float PrevTime = StartTime;
				float PrevX = StartX;
				float PrevY = EnvelopeToScreenY(View, Channels[c]);
				for(int Step = 1; Step <= Steps; Step++)
				{
					float CurrentTime = StartTime + Step * StepTime;
					if(CurrentTime >= EndTime)
					{
						CurrentTime = EndTime - 0.001f;
						if(CurrentTime <= PrevTime)
							break;
					}

					Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					pEnvelope->Eval(CurrentTime, Channels, c + 1);
					const float CurrentX = StartX + Step * StepSize;
					const float CurrentY = EnvelopeToScreenY(View, Channels[c]);

					const IGraphics::CLineItem Item = IGraphics::CLineItem(PrevX, PrevY, CurrentX, CurrentY);
					Graphics()->LinesBatchDraw(&LineItemBatch, &Item, 1);

					PrevTime = CurrentTime;
					PrevX = CurrentX;
					PrevY = CurrentY;
				}
				Graphics()->LinesBatchEnd(&LineItemBatch);
			}
			Ui()->ClipDisable();
		}

		// render curve options
		{
			for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
			{
				float t0 = pEnvelope->m_vPoints[i].m_Time.AsSeconds();
				float t1 = pEnvelope->m_vPoints[i + 1].m_Time.AsSeconds();

				CUIRect CurveButton;
				CurveButton.x = EnvelopeToScreenX(View, t0 + (t1 - t0) * 0.5f);
				CurveButton.y = CurveBar.y;
				CurveButton.h = CurveBar.h;
				CurveButton.w = CurveBar.h;
				CurveButton.x -= CurveButton.w / 2.0f;
				const void *pId = &pEnvelope->m_vPoints[i].m_Curvetype;
				if(CurveButton.x >= View.x)
				{
					const int ButtonResult = DoButton_Editor(pId, CurveTypeNameShort(pEnvelope->m_vPoints[i].m_Curvetype), 0, &CurveButton, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT, "Switch curve type (N = step, L = linear, S = slow, F = fast, M = smooth, B = bezier).");
					if(ButtonResult == 1)
					{
						const int PrevCurve = pEnvelope->m_vPoints[i].m_Curvetype;
						const int Direction = Input()->ShiftIsPressed() ? -1 : 1;
						pEnvelope->m_vPoints[i].m_Curvetype = (pEnvelope->m_vPoints[i].m_Curvetype + Direction + NUM_CURVETYPES) % NUM_CURVETYPES;

						Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEditPoint>(Map(),
							Map()->m_SelectedEnvelope, i, 0, CEditorActionEnvelopeEditPoint::EEditType::CURVE_TYPE, PrevCurve, pEnvelope->m_vPoints[i].m_Curvetype));
						Map()->OnModify();
					}
					else if(ButtonResult == 2)
					{
						m_PopupEnvelopeSelectedPoint = i;
						static SPopupMenuId s_PopupCurvetypeId;
						Ui()->DoPopupMenu(&s_PopupCurvetypeId, Ui()->MouseX(), Ui()->MouseY(), 80, (float)NUM_CURVETYPES * 14.0f + 10.0f, this, PopupEnvelopeCurvetype);
					}
				}
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			RenderEnvelopeEditorColorBar(ColorBar, pEnvelope);
		}

		// render handles
		if(CurrentEnvelopeSwitched)
		{
			Map()->DeselectEnvPoints();
			m_ResetZoomEnvelope = true;
		}

		{
			static SPopupMenuId s_PopupEnvPointId;
			const auto &&ShowPopupEnvPoint = [&]() {
				Ui()->DoPopupMenu(&s_PopupEnvPointId, Ui()->MouseX(), Ui()->MouseY(), 150, 56 + (pEnvelope->GetChannels() == 4 && !Map()->IsTangentSelected() ? 16.0f : 0.0f), this, PopupEnvPoint);
			};

			if(s_Operation == EEnvelopeEditorOp::NONE)
			{
				UpdateHotEnvelopePoint(View, pEnvelope.get(), s_ActiveChannels);
				if(!Ui()->MouseButton(0))
					Map()->m_EnvOpTracker.Stop(false);
			}
			else
			{
				Map()->m_EnvOpTracker.Begin(s_Operation);
			}

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					// point handle
					{
						CUIRect Final;
						Final.x = EnvelopeToScreenX(View, pEnvelope->m_vPoints[i].m_Time.AsSeconds());
						Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));
						Final.x -= 2.0f;
						Final.y -= 2.0f;
						Final.w = 4.0f;
						Final.h = 4.0f;

						const void *pId = &pEnvelope->m_vPoints[i].m_aValues[c];

						if(Map()->IsEnvPointSelected(i, c))
						{
							Graphics()->SetColor(1, 1, 1, 1);
							CUIRect Background = {
								Final.x - 0.2f * Final.w,
								Final.y - 0.2f * Final.h,
								Final.w * 1.4f,
								Final.h * 1.4f};
							IGraphics::CQuadItem QuadItem(Background.x, Background.y, Background.w, Background.h);
							Graphics()->QuadsDrawTL(&QuadItem, 1);
						}

						if(Ui()->CheckActiveItem(pId))
						{
							m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;

							if(s_Operation == EEnvelopeEditorOp::SELECT)
							{
								float dx = s_MouseXStart - Ui()->MouseX();
								float dy = s_MouseYStart - Ui()->MouseY();

								if(dx * dx + dy * dy > 20.0f)
								{
									s_Operation = EEnvelopeEditorOp::DRAG_POINT;

									if(!Map()->IsEnvPointSelected(i, c))
										Map()->SelectEnvPoint(i, c);
								}
							}

							if(s_Operation == EEnvelopeEditorOp::DRAG_POINT || s_Operation == EEnvelopeEditorOp::DRAG_POINT_X || s_Operation == EEnvelopeEditorOp::DRAG_POINT_Y)
							{
								if(Input()->ShiftIsPressed())
								{
									if(s_Operation == EEnvelopeEditorOp::DRAG_POINT || s_Operation == EEnvelopeEditorOp::DRAG_POINT_Y)
									{
										s_Operation = EEnvelopeEditorOp::DRAG_POINT_X;
										s_vAccurateDragValuesX.clear();
										for(auto [SelectedIndex, _] : Map()->m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesX.push_back(pEnvelope->m_vPoints[SelectedIndex].m_Time.GetInternal());
									}
									else
									{
										float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);

										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
											CFixedTime BoundLow = CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x));
											CFixedTime BoundHigh = CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w));
											for(int j = 0; j < SelectedIndex; j++)
											{
												if(!Map()->IsEnvPointSelected(j))
													BoundLow = std::max(pEnvelope->m_vPoints[j].m_Time + CFixedTime(1), BoundLow);
											}
											for(int j = SelectedIndex + 1; j < (int)pEnvelope->m_vPoints.size(); j++)
											{
												if(!Map()->IsEnvPointSelected(j))
													BoundHigh = std::min(pEnvelope->m_vPoints[j].m_Time - CFixedTime(1), BoundHigh);
											}

											DeltaX = ClampDelta(s_vAccurateDragValuesX[k], DeltaX, BoundLow.GetInternal(), BoundHigh.GetInternal());
										}
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
											s_vAccurateDragValuesX[k] += DeltaX;
											pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round(s_vAccurateDragValuesX[k]));
										}
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
											if(SelectedIndex == 0 && pEnvelope->m_vPoints[SelectedIndex].m_Time != CFixedTime(0))
											{
												RemoveTimeOffsetEnvelope(pEnvelope);
												float Offset = s_vAccurateDragValuesX[k];
												for(auto &Value : s_vAccurateDragValuesX)
													Value -= Offset;
												break;
											}
										}
									}
								}
								else
								{
									if(s_Operation == EEnvelopeEditorOp::DRAG_POINT || s_Operation == EEnvelopeEditorOp::DRAG_POINT_X)
									{
										s_Operation = EEnvelopeEditorOp::DRAG_POINT_Y;
										s_vAccurateDragValuesY.clear();
										for(auto [SelectedIndex, SelectedChannel] : Map()->m_vSelectedEnvelopePoints)
											s_vAccurateDragValuesY.push_back(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel]);
									}
									else
									{
										float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
											s_vAccurateDragValuesY[k] -= DeltaY;
											pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vAccurateDragValuesY[k]);

											if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
											{
												pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::clamp(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel], 0, 1024);
												s_vAccurateDragValuesY[k] = std::clamp<float>(s_vAccurateDragValuesY[k], 0, 1024);
											}
										}
									}
								}
							}

							if(s_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
							{
								if(!Ui()->MouseButton(1))
								{
									if(Map()->m_vSelectedEnvelopePoints.size() == 1)
									{
										Map()->m_UpdateEnvPointInfo = true;
										ShowPopupEnvPoint();
									}
									else if(Map()->m_vSelectedEnvelopePoints.size() > 1)
									{
										static SPopupMenuId s_PopupEnvPointMultiId;
										Ui()->DoPopupMenu(&s_PopupEnvPointMultiId, Ui()->MouseX(), Ui()->MouseY(), 100, 22, this, PopupEnvPointMulti);
									}
									Ui()->SetActiveItem(nullptr);
									s_Operation = EEnvelopeEditorOp::NONE;
								}
							}
							else if(!Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(nullptr);
								Map()->m_SelectedQuadEnvelope = -1;

								if(s_Operation == EEnvelopeEditorOp::SELECT)
								{
									if(Input()->ShiftIsPressed())
										Map()->ToggleEnvPoint(i, c);
									else
										Map()->SelectEnvPoint(i, c);
								}

								s_Operation = EEnvelopeEditorOp::NONE;
								Map()->OnModify();
							}

							Graphics()->SetColor(1, 1, 1, 1);
						}
						else if(Ui()->HotItem() == pId)
						{
							if(Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(pId);
								s_Operation = EEnvelopeEditorOp::SELECT;
								Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;

								s_MouseXStart = Ui()->MouseX();
								s_MouseYStart = Ui()->MouseY();
							}
							else if(Ui()->MouseButtonClicked(1))
							{
								if(Input()->ShiftIsPressed())
								{
									Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionDeleteEnvelopePoint>(Map(), Map()->m_SelectedEnvelope, i));
								}
								else
								{
									s_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
									if(!Map()->IsEnvPointSelected(i, c))
										Map()->SelectEnvPoint(i, c);
									Ui()->SetActiveItem(pId);
								}
							}

							m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;
							Graphics()->SetColor(1, 1, 1, 1);
							str_copy(m_aTooltip, "Envelope point. Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time. Shift+right click to delete.");
							m_pUiGotContext = pId;
						}
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

						IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					// tangent handles for bezier curves
					if(i >= 0 && i < (int)pEnvelope->m_vPoints.size())
					{
						// Out-Tangent handle
						if(i < (int)pEnvelope->m_vPoints.size() - 1 && pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
						{
							CUIRect Final;
							Final.x = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]).AsSeconds());
							Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							const void *pId = &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c];

							if(Map()->IsTangentOutPointSelected(i, c))
							{
								Graphics()->SetColor(1, 1, 1, 1);
								IGraphics::CFreeformItem FreeformItem(
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w + 1,
									Final.y + Final.h + 1,
									Final.x - 1,
									Final.y + Final.h + 1);
								Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
							}

							if(Ui()->CheckActiveItem(pId))
							{
								m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;

								if(s_Operation == EEnvelopeEditorOp::SELECT)
								{
									float dx = s_MouseXStart - Ui()->MouseX();
									float dy = s_MouseYStart - Ui()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = EEnvelopeEditorOp::DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c].GetInternal())};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c])};

										if(!Map()->IsTangentOutPointSelected(i, c))
											Map()->SelectTangentOutPoint(i, c);
									}
								}

								if(s_Operation == EEnvelopeEditorOp::DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = CFixedTime(std::round(s_vAccurateDragValuesX[0]));
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = std::clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c], CFixedTime(0), CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
									s_vAccurateDragValuesX[0] = std::clamp<float>(s_vAccurateDragValuesX[0], 0, (CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time).GetInternal());
								}

								if(s_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(Map()->IsTangentOutPointSelected(i, c))
										{
											Map()->m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										s_Operation = EEnvelopeEditorOp::NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									Map()->m_SelectedQuadEnvelope = -1;

									if(s_Operation == EEnvelopeEditorOp::SELECT)
										Map()->SelectTangentOutPoint(i, c);

									s_Operation = EEnvelopeEditorOp::NONE;
									Map()->OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									s_Operation = EEnvelopeEditorOp::SELECT;
									Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;

									s_MouseXStart = Ui()->MouseX();
									s_MouseYStart = Ui()->MouseY();
								}
								else if(Ui()->MouseButtonClicked(1))
								{
									if(Input()->ShiftIsPressed())
									{
										Map()->SelectTangentOutPoint(i, c);
										pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = CFixedTime(0);
										pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = 0.0f;
										Map()->OnModify();
									}
									else
									{
										s_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
										Map()->SelectTangentOutPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(m_aTooltip, "Bezier out-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift+right click to reset.");
								m_pUiGotContext = pId;
							}
							else
								Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

							// draw triangle
							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}

						// In-Tangent handle
						if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
						{
							CUIRect Final;
							Final.x = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]).AsSeconds());
							Final.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							const void *pId = &pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c];

							if(Map()->IsTangentInPointSelected(i, c))
							{
								Graphics()->SetColor(1, 1, 1, 1);
								IGraphics::CFreeformItem FreeformItem(
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w / 2.0f,
									Final.y - 1,
									Final.x + Final.w + 1,
									Final.y + Final.h + 1,
									Final.x - 1,
									Final.y + Final.h + 1);
								Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
							}

							if(Ui()->CheckActiveItem(pId))
							{
								m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;

								if(s_Operation == EEnvelopeEditorOp::SELECT)
								{
									float dx = s_MouseXStart - Ui()->MouseX();
									float dy = s_MouseYStart - Ui()->MouseY();

									if(dx * dx + dy * dy > 20.0f)
									{
										s_Operation = EEnvelopeEditorOp::DRAG_POINT;

										s_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c].GetInternal())};
										s_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c])};

										if(!Map()->IsTangentInPointSelected(i, c))
											Map()->SelectTangentInPoint(i, c);
									}
								}

								if(s_Operation == EEnvelopeEditorOp::DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									s_vAccurateDragValuesX[0] += DeltaX;
									s_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = CFixedTime(std::round(s_vAccurateDragValuesX[0]));
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = std::round(s_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = std::clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c], CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, CFixedTime(0));
									s_vAccurateDragValuesX[0] = std::clamp<float>(s_vAccurateDragValuesX[0], (CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time).GetInternal(), 0);
								}

								if(s_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(Map()->IsTangentInPointSelected(i, c))
										{
											Map()->m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										s_Operation = EEnvelopeEditorOp::NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									Map()->m_SelectedQuadEnvelope = -1;

									if(s_Operation == EEnvelopeEditorOp::SELECT)
										Map()->SelectTangentInPoint(i, c);

									s_Operation = EEnvelopeEditorOp::NONE;
									Map()->OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									s_Operation = EEnvelopeEditorOp::SELECT;
									Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;

									s_MouseXStart = Ui()->MouseX();
									s_MouseYStart = Ui()->MouseY();
								}
								else if(Ui()->MouseButtonClicked(1))
								{
									if(Input()->ShiftIsPressed())
									{
										Map()->SelectTangentInPoint(i, c);
										pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = CFixedTime(0);
										pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = 0.0f;
										Map()->OnModify();
									}
									else
									{
										s_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
										Map()->SelectTangentInPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(m_aTooltip, "Bezier in-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift+right click to reset.");
								m_pUiGotContext = pId;
							}
							else
								Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

							// draw triangle
							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}
					}
				}
			}
			Graphics()->QuadsEnd();
			Ui()->ClipDisable();
		}

		// handle scaling
		static float s_ScaleFactorX = 1.0f;
		static float s_ScaleFactorY = 1.0f;
		static float s_MidpointX = 0.0f;
		static float s_MidpointY = 0.0f;
		static std::vector<float> s_vInitialPositionsX;
		static std::vector<float> s_vInitialPositionsY;
		if(s_Operation == EEnvelopeEditorOp::NONE && !s_NameInput.IsActive() && Input()->KeyIsPressed(KEY_S) && !Input()->ModifierIsPressed() && !Map()->m_vSelectedEnvelopePoints.empty())
		{
			s_Operation = EEnvelopeEditorOp::SCALE;
			s_ScaleFactorX = 1.0f;
			s_ScaleFactorY = 1.0f;
			auto [FirstPointIndex, FirstPointChannel] = Map()->m_vSelectedEnvelopePoints.front();

			float MaximumX = pEnvelope->m_vPoints[FirstPointIndex].m_Time.GetInternal();
			float MinimumX = MaximumX;
			s_vInitialPositionsX.clear();
			for(auto [SelectedIndex, _] : Map()->m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_Time.GetInternal();
				s_vInitialPositionsX.push_back(Value);
				MaximumX = maximum(MaximumX, Value);
				MinimumX = minimum(MinimumX, Value);
			}
			s_MidpointX = (MaximumX - MinimumX) / 2.0f + MinimumX;

			float MaximumY = pEnvelope->m_vPoints[FirstPointIndex].m_aValues[FirstPointChannel];
			float MinimumY = MaximumY;
			s_vInitialPositionsY.clear();
			for(auto [SelectedIndex, SelectedChannel] : Map()->m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
				s_vInitialPositionsY.push_back(Value);
				MaximumY = maximum(MaximumY, Value);
				MinimumY = minimum(MinimumY, Value);
			}
			s_MidpointY = (MaximumY - MinimumY) / 2.0f + MinimumY;
		}

		if(s_Operation == EEnvelopeEditorOp::SCALE)
		{
			str_copy(m_aTooltip, "Press shift to scale the time. Press alt to scale along midpoint. Press ctrl to be more precise.");

			if(Input()->ShiftIsPressed())
			{
				s_ScaleFactorX += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				float Midpoint = Input()->AltIsPressed() ? s_MidpointX : 0.0f;
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
					CFixedTime BoundLow = CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x));
					CFixedTime BoundHigh = CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w));
					for(int j = 0; j < SelectedIndex; j++)
					{
						if(!Map()->IsEnvPointSelected(j))
							BoundLow = std::max(pEnvelope->m_vPoints[j].m_Time + CFixedTime(1), BoundLow);
					}
					for(int j = SelectedIndex + 1; j < (int)pEnvelope->m_vPoints.size(); j++)
					{
						if(!Map()->IsEnvPointSelected(j))
							BoundHigh = std::min(pEnvelope->m_vPoints[j].m_Time - CFixedTime(1), BoundHigh);
					}

					float Value = s_vInitialPositionsX[k];
					float ScaleBoundLow = (BoundLow.GetInternal() - Midpoint) / (Value - Midpoint);
					float ScaleBoundHigh = (BoundHigh.GetInternal() - Midpoint) / (Value - Midpoint);
					float ScaleBoundMin = minimum(ScaleBoundLow, ScaleBoundHigh);
					float ScaleBoundMax = maximum(ScaleBoundLow, ScaleBoundHigh);
					s_ScaleFactorX = std::clamp(s_ScaleFactorX, ScaleBoundMin, ScaleBoundMax);
				}

				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
					float ScaleMinimum = s_vInitialPositionsX[k] - Midpoint > CFixedTime(1).AsSeconds() ? CFixedTime(1).AsSeconds() / (s_vInitialPositionsX[k] - Midpoint) : 0.0f;
					float ScaleFactor = maximum(ScaleMinimum, s_ScaleFactorX);
					pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round((s_vInitialPositionsX[k] - Midpoint) * ScaleFactor + Midpoint));
				}
				for(size_t k = 1; k < pEnvelope->m_vPoints.size(); k++)
				{
					if(pEnvelope->m_vPoints[k].m_Time <= pEnvelope->m_vPoints[k - 1].m_Time)
						pEnvelope->m_vPoints[k].m_Time = pEnvelope->m_vPoints[k - 1].m_Time + CFixedTime(1);
				}
				for(auto [SelectedIndex, _] : Map()->m_vSelectedEnvelopePoints)
				{
					if(SelectedIndex == 0 && pEnvelope->m_vPoints[SelectedIndex].m_Time != CFixedTime(0))
					{
						float Offset = pEnvelope->m_vPoints[0].m_Time.GetInternal();
						RemoveTimeOffsetEnvelope(pEnvelope);
						s_MidpointX -= Offset;
						for(auto &Value : s_vInitialPositionsX)
							Value -= Offset;
						break;
					}
				}
			}
			else
			{
				s_ScaleFactorY -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
					if(Input()->AltIsPressed())
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round((s_vInitialPositionsY[k] - s_MidpointY) * s_ScaleFactorY + s_MidpointY);
					else
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vInitialPositionsY[k] * s_ScaleFactorY);

					if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::clamp(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel], 0, 1024);
				}
			}

			if(Ui()->MouseButton(0))
			{
				s_Operation = EEnvelopeEditorOp::NONE;
				Map()->m_EnvOpTracker.Stop(false);
			}
			else if(Ui()->MouseButton(1) || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
			{
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
					pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round(s_vInitialPositionsX[k]));
				}
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
					pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(s_vInitialPositionsY[k]);
				}
				RemoveTimeOffsetEnvelope(pEnvelope);
				s_Operation = EEnvelopeEditorOp::NONE;
			}
		}

		// handle box selection
		if(s_Operation == EEnvelopeEditorOp::BOX_SELECT)
		{
			Ui()->ClipEnable(&View);
			CUIRect SelectionRect;
			SelectionRect.x = s_MouseXStart;
			SelectionRect.y = s_MouseYStart;
			SelectionRect.w = Ui()->MouseX() - s_MouseXStart;
			SelectionRect.h = Ui()->MouseY() - s_MouseYStart;
			SelectionRect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			Ui()->ClipDisable();

			if(!Ui()->MouseButton(0))
			{
				s_Operation = EEnvelopeEditorOp::NONE;
				Ui()->SetActiveItem(nullptr);

				float TimeStart = ScreenToEnvelopeX(View, s_MouseXStart);
				float TimeEnd = ScreenToEnvelopeX(View, Ui()->MouseX());
				float ValueStart = ScreenToEnvelopeY(View, s_MouseYStart);
				float ValueEnd = ScreenToEnvelopeY(View, Ui()->MouseY());

				float TimeMin = minimum(TimeStart, TimeEnd);
				float TimeMax = maximum(TimeStart, TimeEnd);
				float ValueMin = minimum(ValueStart, ValueEnd);
				float ValueMax = maximum(ValueStart, ValueEnd);

				if(!Input()->ShiftIsPressed())
					Map()->DeselectEnvPoints();

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
					{
						if(!(s_ActiveChannels & (1 << c)))
							continue;

						float Time = pEnvelope->m_vPoints[i].m_Time.AsSeconds();
						float Value = fx2f(pEnvelope->m_vPoints[i].m_aValues[c]);

						if(in_range(Time, TimeMin, TimeMax) && in_range(Value, ValueMin, ValueMax))
							Map()->ToggleEnvPoint(i, c);
					}
				}
			}
		}
	}
}

void CEditor::RenderEnvelopeEditorColorBar(CUIRect ColorBar, const std::shared_ptr<CEnvelope> &pEnvelope)
{
	if(pEnvelope->m_vPoints.size() < 2)
	{
		return;
	}
	const float ViewStartTime = ScreenToEnvelopeX(ColorBar, ColorBar.x);
	const float ViewEndTime = ScreenToEnvelopeX(ColorBar, ColorBar.x + ColorBar.w);
	if(ViewEndTime < 0.0f || ViewStartTime > pEnvelope->EndTime())
	{
		return;
	}
	const float StartX = maximum(EnvelopeToScreenX(ColorBar, 0.0f), ColorBar.x);
	const float TotalWidth = minimum(EnvelopeToScreenX(ColorBar, pEnvelope->EndTime()) - StartX, ColorBar.x + ColorBar.w - StartX);

	Ui()->ClipEnable(&ColorBar);
	CUIRect ColorBarBackground = CUIRect{StartX, ColorBar.y, TotalWidth, ColorBar.h};
	RenderBackground(ColorBarBackground, m_CheckerTexture, ColorBarBackground.h, 1.0f);
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	int PointBeginIndex = pEnvelope->FindPointIndex(CFixedTime::FromSeconds(ViewStartTime));
	if(PointBeginIndex == -1)
	{
		PointBeginIndex = 0;
	}
	int PointEndIndex = pEnvelope->FindPointIndex(CFixedTime::FromSeconds(ViewEndTime));
	if(PointEndIndex == -1)
	{
		PointEndIndex = (int)pEnvelope->m_vPoints.size() - 2;
	}
	for(int PointIndex = PointBeginIndex; PointIndex <= PointEndIndex; PointIndex++)
	{
		const auto &PointStart = pEnvelope->m_vPoints[PointIndex];
		const auto &PointEnd = pEnvelope->m_vPoints[PointIndex + 1];
		const float PointStartTime = PointStart.m_Time.AsSeconds();
		const float PointEndTime = PointEnd.m_Time.AsSeconds();

		int Steps;
		if(PointStart.m_Curvetype == CURVETYPE_LINEAR || PointStart.m_Curvetype == CURVETYPE_STEP)
		{
			Steps = 1; // let the GPU do the work
		}
		else
		{
			const float ClampedPointStartX = maximum(EnvelopeToScreenX(ColorBar, PointStartTime), ColorBar.x);
			const float ClampedPointEndX = minimum(EnvelopeToScreenX(ColorBar, PointEndTime), ColorBar.x + ColorBar.w);
			Steps = std::clamp((int)std::sqrt(5.0f * (ClampedPointEndX - ClampedPointStartX)), 1, 250);
		}
		const float OverallSectionStartTime = Steps == 1 ? PointStartTime : maximum(PointStartTime, ViewStartTime);
		const float OverallSectionEndTime = Steps == 1 ? PointEndTime : minimum(PointEndTime, ViewEndTime);
		float SectionStartTime = OverallSectionStartTime;
		float SectionStartX = EnvelopeToScreenX(ColorBar, SectionStartTime);
		for(int Step = 1; Step <= Steps; Step++)
		{
			const float SectionEndTime = OverallSectionStartTime + (OverallSectionEndTime - OverallSectionStartTime) * (Step / (float)Steps);
			const float SectionEndX = EnvelopeToScreenX(ColorBar, SectionEndTime);

			ColorRGBA StartColor;
			if(Step == 1 && OverallSectionStartTime == PointStartTime)
			{
				StartColor = PointStart.ColorValue();
			}
			else
			{
				StartColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				pEnvelope->Eval(SectionStartTime, StartColor, 4);
			}

			ColorRGBA EndColor;
			if(PointStart.m_Curvetype == CURVETYPE_STEP)
			{
				EndColor = StartColor;
			}
			else if(Step == Steps && OverallSectionEndTime == PointEndTime)
			{
				EndColor = PointEnd.ColorValue();
			}
			else
			{
				EndColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
				pEnvelope->Eval(SectionEndTime, EndColor, 4);
			}

			Graphics()->SetColor4(StartColor, EndColor, StartColor, EndColor);
			const IGraphics::CQuadItem QuadItem(SectionStartX, ColorBar.y, SectionEndX - SectionStartX, ColorBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);

			SectionStartTime = SectionEndTime;
			SectionStartX = SectionEndX;
		}
	}
	Graphics()->QuadsEnd();
	Ui()->ClipDisable();
	ColorBarBackground.h -= Ui()->Screen()->h / Graphics()->ScreenHeight(); // hack to fix alignment of bottom border
	ColorBarBackground.DrawOutline(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
}

void CEditor::UpdateHotEnvelopePoint(const CUIRect &View, const CEnvelope *pEnvelope, int ActiveChannels)
{
	if(!Ui()->MouseInside(&View))
		return;

	const vec2 MousePos = Ui()->MousePos();

	float MinDist = 200.0f;
	const void *pMinPointId = nullptr;

	const auto UpdateMinimum = [&](vec2 Position, const void *pId) {
		const float CurrDist = length_squared(Position - MousePos);
		if(CurrDist < MinDist)
		{
			MinDist = CurrDist;
			pMinPointId = pId;
		}
	};

	for(size_t i = 0; i < pEnvelope->m_vPoints.size(); i++)
	{
		for(int c = pEnvelope->GetChannels() - 1; c >= 0; c--)
		{
			if(!(ActiveChannels & (1 << c)))
				continue;

			if(i > 0 && pEnvelope->m_vPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
			{
				vec2 Position;
				Position.x = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]).AsSeconds());
				Position.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c]));
				UpdateMinimum(Position, &pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c]);
			}

			if(i < pEnvelope->m_vPoints.size() - 1 && pEnvelope->m_vPoints[i].m_Curvetype == CURVETYPE_BEZIER)
			{
				vec2 Position;
				Position.x = EnvelopeToScreenX(View, (pEnvelope->m_vPoints[i].m_Time + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]).AsSeconds());
				Position.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c] + pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c]));
				UpdateMinimum(Position, &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c]);
			}

			vec2 Position;
			Position.x = EnvelopeToScreenX(View, pEnvelope->m_vPoints[i].m_Time.AsSeconds());
			Position.y = EnvelopeToScreenY(View, fx2f(pEnvelope->m_vPoints[i].m_aValues[c]));
			UpdateMinimum(Position, &pEnvelope->m_vPoints[i].m_aValues[c]);
		}
	}

	if(pMinPointId != nullptr)
	{
		Ui()->SetHotItem(pMinPointId);
	}
}

void CEditor::ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View)
{
	float PosX = g_Config.m_EdZoomTarget ? (Ui()->MouseX() - View.x) / View.w : 0.5f;
	m_OffsetEnvelopeX = PosX - (PosX - m_OffsetEnvelopeX) * ZoomFactor;
}

void CEditor::UpdateZoomEnvelopeX(const CUIRect &View)
{
	float OldZoom = m_ZoomEnvelopeX.GetValue();
	if(m_ZoomEnvelopeX.UpdateValue())
		ZoomAdaptOffsetX(OldZoom / m_ZoomEnvelopeX.GetValue(), View);
}

void CEditor::ZoomAdaptOffsetY(float ZoomFactor, const CUIRect &View)
{
	float PosY = g_Config.m_EdZoomTarget ? 1.0f - (Ui()->MouseY() - View.y) / View.h : 0.5f;
	m_OffsetEnvelopeY = PosY - (PosY - m_OffsetEnvelopeY) * ZoomFactor;
}

void CEditor::UpdateZoomEnvelopeY(const CUIRect &View)
{
	float OldZoom = m_ZoomEnvelopeY.GetValue();
	if(m_ZoomEnvelopeY.UpdateValue())
		ZoomAdaptOffsetY(OldZoom / m_ZoomEnvelopeY.GetValue(), View);
}

void CEditor::ResetZoomEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels)
{
	auto [Bottom, Top] = pEnvelope->GetValueRange(ActiveChannels);
	float EndTime = pEnvelope->EndTime();
	float ValueRange = absolute(Top - Bottom);

	if(ValueRange < m_ZoomEnvelopeY.GetMinValue())
	{
		// Set view to some sane default if range is too small
		m_OffsetEnvelopeY = 0.5f - ValueRange / m_ZoomEnvelopeY.GetMinValue() / 2.0f - Bottom / m_ZoomEnvelopeY.GetMinValue();
		m_ZoomEnvelopeY.SetValueInstant(m_ZoomEnvelopeY.GetMinValue());
	}
	else if(ValueRange > m_ZoomEnvelopeY.GetMaxValue())
	{
		m_OffsetEnvelopeY = -Bottom / m_ZoomEnvelopeY.GetMaxValue();
		m_ZoomEnvelopeY.SetValueInstant(m_ZoomEnvelopeY.GetMaxValue());
	}
	else
	{
		// calculate biggest possible spacing
		float SpacingFactor = minimum(1.25f, m_ZoomEnvelopeY.GetMaxValue() / ValueRange);
		m_ZoomEnvelopeY.SetValueInstant(SpacingFactor * ValueRange);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;

		if(Top >= 0 && Bottom >= 0)
			m_OffsetEnvelopeY = Spacing - Bottom / m_ZoomEnvelopeY.GetValue();
		else if(Top <= 0 && Bottom <= 0)
			m_OffsetEnvelopeY = Spacing - Bottom / m_ZoomEnvelopeY.GetValue();
		else
			m_OffsetEnvelopeY = Spacing + Space * absolute(Bottom) / ValueRange;
	}

	if(EndTime < m_ZoomEnvelopeX.GetMinValue())
	{
		m_OffsetEnvelopeX = 0.5f - EndTime / m_ZoomEnvelopeX.GetMinValue();
		m_ZoomEnvelopeX.SetValueInstant(m_ZoomEnvelopeX.GetMinValue());
	}
	else if(EndTime > m_ZoomEnvelopeX.GetMaxValue())
	{
		m_OffsetEnvelopeX = 0.0f;
		m_ZoomEnvelopeX.SetValueInstant(m_ZoomEnvelopeX.GetMaxValue());
	}
	else
	{
		float SpacingFactor = minimum(1.25f, m_ZoomEnvelopeX.GetMaxValue() / EndTime);
		m_ZoomEnvelopeX.SetValueInstant(SpacingFactor * EndTime);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;

		m_OffsetEnvelopeX = Spacing;
	}
}

float CEditor::ScreenToEnvelopeX(const CUIRect &View, float x) const
{
	return (x - View.x - View.w * m_OffsetEnvelopeX) / View.w * m_ZoomEnvelopeX.GetValue();
}

float CEditor::EnvelopeToScreenX(const CUIRect &View, float x) const
{
	return View.x + View.w * m_OffsetEnvelopeX + x / m_ZoomEnvelopeX.GetValue() * View.w;
}

float CEditor::ScreenToEnvelopeY(const CUIRect &View, float y) const
{
	return (View.h - y + View.y) / View.h * m_ZoomEnvelopeY.GetValue() - m_OffsetEnvelopeY * m_ZoomEnvelopeY.GetValue();
}

float CEditor::EnvelopeToScreenY(const CUIRect &View, float y) const
{
	return View.y + View.h - y / m_ZoomEnvelopeY.GetValue() * View.h - m_OffsetEnvelopeY * View.h;
}

float CEditor::ScreenToEnvelopeDX(const CUIRect &View, float DeltaX)
{
	return DeltaX / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w * m_ZoomEnvelopeX.GetValue();
}

float CEditor::ScreenToEnvelopeDY(const CUIRect &View, float DeltaY)
{
	return DeltaY / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h * m_ZoomEnvelopeY.GetValue();
}

void CEditor::RemoveTimeOffsetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope)
{
	CFixedTime TimeOffset = pEnvelope->m_vPoints[0].m_Time;
	for(auto &Point : pEnvelope->m_vPoints)
		Point.m_Time -= TimeOffset;

	m_OffsetEnvelopeX += TimeOffset.AsSeconds() / m_ZoomEnvelopeX.GetValue();
}

CUi::EPopupMenuFunctionResult CEditor::PopupEnvPoint(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	if(pEditor->Map()->m_SelectedEnvelope < 0 || pEditor->Map()->m_SelectedEnvelope >= (int)pEditor->Map()->m_vpEnvelopes.size())
		return CUi::POPUP_CLOSE_CURRENT;

	const float RowHeight = 12.0f;
	CUIRect Row, Label, EditBox;

	pEditor->m_ActiveEnvelopePreview = EEnvelopePreview::SELECTED;

	std::shared_ptr<CEnvelope> pEnvelope = pEditor->Map()->m_vpEnvelopes[pEditor->Map()->m_SelectedEnvelope];

	if(pEnvelope->GetChannels() == 4 && !pEditor->Map()->IsTangentSelected())
	{
		View.HSplitTop(RowHeight, &Row, &View);
		View.HSplitTop(4.0f, nullptr, &View);
		Row.VSplitLeft(60.0f, &Label, &Row);
		Row.VSplitLeft(10.0f, nullptr, &EditBox);
		pEditor->Ui()->DoLabel(&Label, "Color:", RowHeight - 2.0f, TEXTALIGN_ML);

		const auto SelectedPoint = pEditor->Map()->m_vSelectedEnvelopePoints.front();
		const int SelectedIndex = SelectedPoint.first;
		auto *pValues = pEnvelope->m_vPoints[SelectedIndex].m_aValues;
		const ColorRGBA Color = pEnvelope->m_vPoints[SelectedIndex].ColorValue();
		const auto &&SetColor = [&](ColorRGBA NewColor) {
			if(Color == NewColor && pEditor->m_ColorPickerPopupContext.m_State == EEditState::EDITING)
				return;

			static int s_Values[4];

			if(pEditor->m_ColorPickerPopupContext.m_State == EEditState::START || pEditor->m_ColorPickerPopupContext.m_State == EEditState::ONE_GO)
			{
				for(int Channel = 0; Channel < 4; ++Channel)
					s_Values[Channel] = pValues[Channel];
			}

			pEnvelope->m_vPoints[SelectedIndex].SetColorValue(NewColor);

			if(pEditor->m_ColorPickerPopupContext.m_State == EEditState::END || pEditor->m_ColorPickerPopupContext.m_State == EEditState::ONE_GO)
			{
				std::vector<std::shared_ptr<IEditorAction>> vpActions(4);

				for(int Channel = 0; Channel < 4; ++Channel)
				{
					vpActions[Channel] = std::make_shared<CEditorActionEnvelopeEditPoint>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, Channel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, s_Values[Channel], f2fx(NewColor[Channel]));
				}

				char aDisplay[256];
				str_format(aDisplay, sizeof(aDisplay), "Edit color of point %d of envelope %d", SelectedIndex, pEditor->Map()->m_SelectedEnvelope);
				pEditor->Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(pEditor->Map(), vpActions, aDisplay));
			}

			pEditor->Map()->m_UpdateEnvPointInfo = true;
			pEditor->Map()->OnModify();
		};
		static char s_ColorPickerButton;
		pEditor->DoColorPickerButton(&s_ColorPickerButton, &EditBox, Color, SetColor);
	}

	static CLineInputNumber s_CurValueInput;
	static CLineInputNumber s_CurTimeInput;

	static float s_CurrentTime = 0;
	static float s_CurrentValue = 0;

	if(pEditor->Map()->m_UpdateEnvPointInfo)
	{
		pEditor->Map()->m_UpdateEnvPointInfo = false;

		const auto &[CurrentTime, CurrentValue] = pEditor->Map()->SelectedEnvelopeTimeAndValue();

		// update displayed text
		s_CurValueInput.SetFloat(fx2f(CurrentValue));
		s_CurTimeInput.SetFloat(CurrentTime.AsSeconds());

		s_CurrentTime = s_CurTimeInput.GetFloat();
		s_CurrentValue = s_CurValueInput.GetFloat();
	}

	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->Ui()->DoLabel(&Label, "Value:", RowHeight - 2.0f, TEXTALIGN_ML);
	pEditor->DoEditBox(&s_CurValueInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The value of the selected envelope point.");

	View.HSplitTop(4.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->Ui()->DoLabel(&Label, "Time (in s):", RowHeight - 2.0f, TEXTALIGN_ML);
	pEditor->DoEditBox(&s_CurTimeInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The time of the selected envelope point.");

	if(pEditor->Input()->KeyIsPressed(KEY_RETURN) || pEditor->Input()->KeyIsPressed(KEY_KP_ENTER))
	{
		float CurrentTime = s_CurTimeInput.GetFloat();
		float CurrentValue = s_CurValueInput.GetFloat();
		if(!(absolute(CurrentTime - s_CurrentTime) < 0.0001f && absolute(CurrentValue - s_CurrentValue) < 0.0001f))
		{
			const auto &[OldTime, OldValue] = pEditor->Map()->SelectedEnvelopeTimeAndValue();

			if(pEditor->Map()->IsTangentInSelected())
			{
				auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_SelectedTangentInPoint;

				pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::TANGENT_IN, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));
				CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel]).AsSeconds();
			}
			else if(pEditor->Map()->IsTangentOutSelected())
			{
				auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_SelectedTangentOutPoint;

				pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::TANGENT_OUT, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));
				CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel]).AsSeconds();
			}
			else
			{
				auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_vSelectedEnvelopePoints.front();
				pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::POINT, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));

				if(SelectedIndex != 0)
				{
					CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time.AsSeconds();
				}
				else
				{
					CurrentTime = 0.0f;
					pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(0);
				}
			}

			s_CurTimeInput.SetFloat(CFixedTime::FromSeconds(CurrentTime).AsSeconds());
			s_CurValueInput.SetFloat(fx2f(f2fx(CurrentValue)));

			s_CurrentTime = s_CurTimeInput.GetFloat();
			s_CurrentValue = s_CurValueInput.GetFloat();
		}
	}

	View.HSplitTop(6.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	static int s_DeleteButtonId = 0;
	const char *pButtonText = pEditor->Map()->IsTangentSelected() ? "Reset" : "Delete";
	const char *pTooltip = pEditor->Map()->IsTangentSelected() ? "Reset tangent point to default value." : "Delete current envelope point in all channels.";
	if(pEditor->DoButton_Editor(&s_DeleteButtonId, pButtonText, 0, &Row, BUTTONFLAG_LEFT, pTooltip))
	{
		if(pEditor->Map()->IsTangentInSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_SelectedTangentInPoint;
			const auto &[OldTime, OldValue] = pEditor->Map()->SelectedEnvelopeTimeAndValue();
			pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionResetEnvelopePointTangent>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, true, OldTime, OldValue));
		}
		else if(pEditor->Map()->IsTangentOutSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_SelectedTangentOutPoint;
			const auto &[OldTime, OldValue] = pEditor->Map()->SelectedEnvelopeTimeAndValue();
			pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionResetEnvelopePointTangent>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, false, OldTime, OldValue));
		}
		else
		{
			auto [SelectedIndex, SelectedChannel] = pEditor->Map()->m_vSelectedEnvelopePoints.front();
			pEditor->Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionDeleteEnvelopePoint>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex));
		}

		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CEditor::PopupEnvPointMulti(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	const float RowHeight = 12.0f;

	static int s_CurveButtonId = 0;
	CUIRect CurveButton;
	View.HSplitTop(RowHeight, &CurveButton, &View);
	if(pEditor->DoButton_MenuItem(&s_CurveButtonId, "Project onto", 0, &CurveButton, BUTTONFLAG_LEFT, "Project all selected envelopes onto the curve between the first and last selected envelope."))
	{
		static SPopupMenuId s_PopupCurveTypeId;
		pEditor->Ui()->DoPopupMenu(&s_PopupCurveTypeId, pEditor->Ui()->MouseX(), pEditor->Ui()->MouseY(), 80, 80, pEditor, PopupEnvPointCurveType);
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CEditor::PopupEnvPointCurveType(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);
	const float RowHeight = 14.0f;

	int CurveType = -1;

	static int s_ButtonLinearId;
	CUIRect ButtonLinear;
	View.HSplitTop(RowHeight, &ButtonLinear, &View);
	if(pEditor->DoButton_MenuItem(&s_ButtonLinearId, CURVE_TYPE_NAMES[CURVETYPE_LINEAR], 0, &ButtonLinear))
		CurveType = CURVETYPE_LINEAR;

	static int s_ButtonSlowId;
	CUIRect ButtonSlow;
	View.HSplitTop(RowHeight, &ButtonSlow, &View);
	if(pEditor->DoButton_MenuItem(&s_ButtonSlowId, CURVE_TYPE_NAMES[CURVETYPE_SLOW], 0, &ButtonSlow))
		CurveType = CURVETYPE_SLOW;

	static int s_ButtonFastId;
	CUIRect ButtonFast;
	View.HSplitTop(RowHeight, &ButtonFast, &View);
	if(pEditor->DoButton_MenuItem(&s_ButtonFastId, CURVE_TYPE_NAMES[CURVETYPE_FAST], 0, &ButtonFast))
		CurveType = CURVETYPE_FAST;

	static int s_ButtonStepId;
	CUIRect ButtonStep;
	View.HSplitTop(RowHeight, &ButtonStep, &View);
	if(pEditor->DoButton_MenuItem(&s_ButtonStepId, CURVE_TYPE_NAMES[CURVETYPE_STEP], 0, &ButtonStep))
		CurveType = CURVETYPE_STEP;

	static int s_ButtonSmoothId;
	CUIRect ButtonSmooth;
	View.HSplitTop(RowHeight, &ButtonSmooth, &View);
	if(pEditor->DoButton_MenuItem(&s_ButtonSmoothId, CURVE_TYPE_NAMES[CURVETYPE_SMOOTH], 0, &ButtonSmooth))
		CurveType = CURVETYPE_SMOOTH;

	std::vector<std::shared_ptr<IEditorAction>> vpActions;

	if(CurveType >= 0)
	{
		std::shared_ptr<CEnvelope> pEnvelope = pEditor->Map()->m_vpEnvelopes.at(pEditor->Map()->m_SelectedEnvelope);

		for(int c = 0; c < pEnvelope->GetChannels(); c++)
		{
			int FirstSelectedIndex = pEnvelope->m_vPoints.size();
			int LastSelectedIndex = -1;
			for(auto [SelectedIndex, SelectedChannel] : pEditor->Map()->m_vSelectedEnvelopePoints)
			{
				if(SelectedChannel == c)
				{
					FirstSelectedIndex = minimum(FirstSelectedIndex, SelectedIndex);
					LastSelectedIndex = maximum(LastSelectedIndex, SelectedIndex);
				}
			}

			if(FirstSelectedIndex < (int)pEnvelope->m_vPoints.size() && LastSelectedIndex >= 0 && FirstSelectedIndex != LastSelectedIndex)
			{
				CEnvPoint FirstPoint = pEnvelope->m_vPoints[FirstSelectedIndex];
				CEnvPoint LastPoint = pEnvelope->m_vPoints[LastSelectedIndex];

				CEnvelope HelperEnvelope(1);
				HelperEnvelope.AddPoint(FirstPoint.m_Time, {FirstPoint.m_aValues[c], 0, 0, 0});
				HelperEnvelope.AddPoint(LastPoint.m_Time, {LastPoint.m_aValues[c], 0, 0, 0});
				HelperEnvelope.m_vPoints[0].m_Curvetype = CurveType;

				for(auto [SelectedIndex, SelectedChannel] : pEditor->Map()->m_vSelectedEnvelopePoints)
				{
					if(SelectedChannel == c)
					{
						if(SelectedIndex != FirstSelectedIndex && SelectedIndex != LastSelectedIndex)
						{
							CEnvPoint &CurrentPoint = pEnvelope->m_vPoints[SelectedIndex];
							ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
							HelperEnvelope.Eval(CurrentPoint.m_Time.AsSeconds(), Channels, 1);
							int PrevValue = CurrentPoint.m_aValues[c];
							CurrentPoint.m_aValues[c] = f2fx(Channels.r);
							vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPoint>(pEditor->Map(), pEditor->Map()->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, PrevValue, CurrentPoint.m_aValues[c]));
						}
					}
				}
			}
		}

		if(!vpActions.empty())
		{
			pEditor->Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(pEditor->Map(), vpActions, "Project points"));
		}

		pEditor->Map()->OnModify();
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CEditor::PopupEnvelopeCurvetype(void *pContext, CUIRect View, bool Active)
{
	CEditor *pEditor = static_cast<CEditor *>(pContext);

	if(pEditor->Map()->m_SelectedEnvelope < 0 || pEditor->Map()->m_SelectedEnvelope >= (int)pEditor->Map()->m_vpEnvelopes.size())
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}
	std::shared_ptr<CEnvelope> pEnvelope = pEditor->Map()->m_vpEnvelopes[pEditor->Map()->m_SelectedEnvelope];

	if(pEditor->m_PopupEnvelopeSelectedPoint < 0 || pEditor->m_PopupEnvelopeSelectedPoint >= (int)pEnvelope->m_vPoints.size())
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}
	CEnvPoint_runtime &SelectedPoint = pEnvelope->m_vPoints[pEditor->m_PopupEnvelopeSelectedPoint];

	static char s_aButtonIds[NUM_CURVETYPES] = {0};

	for(int Type = 0; Type < NUM_CURVETYPES; Type++)
	{
		CUIRect Button;
		View.HSplitTop(14.0f, &Button, &View);

		if(pEditor->DoButton_MenuItem(&s_aButtonIds[Type], CURVE_TYPE_NAMES[Type], Type == SelectedPoint.m_Curvetype, &Button))
		{
			const int PrevCurve = SelectedPoint.m_Curvetype;
			if(PrevCurve != Type)
			{
				SelectedPoint.m_Curvetype = Type;
				pEditor->Map()->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEditPoint>(pEditor->Map(),
					pEditor->Map()->m_SelectedEnvelope, pEditor->m_PopupEnvelopeSelectedPoint, 0, CEditorActionEnvelopeEditPoint::EEditType::CURVE_TYPE, PrevCurve, SelectedPoint.m_Curvetype));
				pEditor->Map()->OnModify();
				return CUi::POPUP_CLOSE_CURRENT;
			}
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}
