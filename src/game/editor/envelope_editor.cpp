/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "envelope_editor.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>
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

static const char *const CURVE_TYPE_NAMES[NUM_CURVETYPES] = {"Step", "Linear", "Slow", "Fast", "Smooth", "Bezier"};
static const char *const CURVE_TYPE_NAMES_SHORT[NUM_CURVETYPES] = {"N", "L", "S", "F", "M", "B"};

static const char *GetCurveTypeNameShort(int CurveType)
{
	if(in_range<int>(CurveType, 0, std::size(CURVE_TYPE_NAMES_SHORT)))
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

void CEnvelopeEditor::CState::Reset(CEditor *pEditor)
{
	m_ZoomX = CSmoothValue(1.0f, 0.1f, 600.0f);
	m_ZoomY = CSmoothValue(640.0f, 0.1f, 32000.0f);
	m_ZoomX.OnInit(pEditor);
	m_ZoomY.OnInit(pEditor);
	m_ResetZoom = true;
	m_Offset = vec2(0.1f, 0.5f);
	m_ActiveChannels = 0b1111;
}

void CEnvelopeEditor::OnReset()
{
	m_NameInput.SetBuffer(nullptr, 0, 0);
	m_EnvelopeEditorButtonUsed = -1;
	m_Operation = EEnvelopeEditorOp::NONE;
	m_vAccurateDragValuesX.clear();
	m_vAccurateDragValuesY.clear();
	m_MouseStart = vec2(0.0f, 0.0f);
	m_ScaleFactor = vec2(1.0f, 1.0f);
	m_Midpoint = vec2(0.0f, 0.0f);
	m_vInitialPositionsX.clear();
	m_vInitialPositionsY.clear();
}

void CEnvelopeEditor::Render(CUIRect View)
{
	Map()->m_SelectedEnvelope = Map()->m_vpEnvelopes.empty() ? -1 : std::clamp(Map()->m_SelectedEnvelope, 0, (int)Map()->m_vpEnvelopes.size() - 1);
	std::shared_ptr<CEnvelope> pEnvelope = Map()->m_vpEnvelopes.empty() ? nullptr : Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
	CState &State = Map()->m_EnvelopeEditorState;

	CUIRect ToolBar, CurveBar, ColorBar, DragBar;
	View.HSplitTop(30.0f, &DragBar, nullptr);
	DragBar.y -= 2.0f;
	DragBar.w += 2.0f;
	DragBar.h += 4.0f;
	Editor()->DoEditorDragBar(View, &DragBar, CEditor::EDragSide::TOP, &Editor()->m_aExtraEditorSplits[CEditor::EXTRAEDITOR_ENVELOPES]);
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	bool CurrentEnvelopeSwitched = false;

	// do the toolbar
	{
		CUIRect Button;

		// redo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		if(Editor()->DoButton_FontIcon(&m_RedoButtonId, FontIcon::REDO, Map()->m_EnvelopeEditorHistory.CanRedo() ? 0 : -1, &Button, BUTTONFLAG_LEFT, "[Ctrl+Y] Redo the last action.", IGraphics::CORNER_R, 11.0f) == 1)
		{
			Map()->m_EnvelopeEditorHistory.Redo();
		}

		// undo button
		ToolBar.VSplitRight(25.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(10.0f, &ToolBar, nullptr);
		if(Editor()->DoButton_FontIcon(&m_UndoButtonId, FontIcon::UNDO, Map()->m_EnvelopeEditorHistory.CanUndo() ? 0 : -1, &Button, BUTTONFLAG_LEFT, "[Ctrl+Z] Undo the last action.", IGraphics::CORNER_L, 11.0f) == 1)
		{
			Map()->m_EnvelopeEditorHistory.Undo();
		}

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		if(Editor()->DoButton_Editor(&m_NewSoundEnvelopeButtonId, "Sound+", 0, &Button, BUTTONFLAG_LEFT, "Create a new sound envelope."))
		{
			Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEnvelopeAdd>(Map(), CEnvelope::EType::SOUND));
			pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
			CurrentEnvelopeSwitched = true;
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		if(Editor()->DoButton_Editor(&m_NewColorEnvelopeButtonId, "Color+", 0, &Button, BUTTONFLAG_LEFT, "Create a new color envelope."))
		{
			Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEnvelopeAdd>(Map(), CEnvelope::EType::COLOR));
			pEnvelope = Map()->m_vpEnvelopes[Map()->m_SelectedEnvelope];
			CurrentEnvelopeSwitched = true;
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, nullptr);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		if(Editor()->DoButton_Editor(&m_NewPositionEnvelopeButtonId, "Pos.+", 0, &Button, BUTTONFLAG_LEFT, "Create a new position envelope."))
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
			if(Editor()->DoButton_Editor(&m_DeleteButtonId, "✗", 0, &Button, BUTTONFLAG_LEFT, "Delete this envelope."))
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
			if(Editor()->DoButton_Ex(&m_MoveRightButtonId, "→", (Map()->m_SelectedEnvelope >= (int)Map()->m_vpEnvelopes.size() - 1 ? -1 : 0), &Button, BUTTONFLAG_LEFT, "Move this envelope to the right.", IGraphics::CORNER_R))
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
			if(Editor()->DoButton_Ex(&m_MoveLeftButtonId, "←", (Map()->m_SelectedEnvelope <= 0 ? -1 : 0), &Button, BUTTONFLAG_LEFT, "Move this envelope to the left.", IGraphics::CORNER_L))
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
				if(Editor()->DoButton_FontIcon(&m_ZoomOutButtonId, FontIcon::MINUS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad-] Zoom out horizontally, hold shift to zoom vertically.", IGraphics::CORNER_R, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						State.m_ZoomY.ChangeValue(0.1f * State.m_ZoomY.GetValue());
					else
						State.m_ZoomX.ChangeValue(0.1f * State.m_ZoomX.GetValue());
				}

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				if(Editor()->DoButton_FontIcon(&m_ResetZoomButtonId, FontIcon::MAGNIFYING_GLASS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad*] Reset zoom to default value.", IGraphics::CORNER_NONE, 9.0f))
					ResetZoomEnvelope(pEnvelope, State.m_ActiveChannels);

				ToolBar.VSplitRight(20.0f, &ToolBar, &Button);
				if(Editor()->DoButton_FontIcon(&m_ZoomInButtonId, FontIcon::PLUS, 0, &Button, BUTTONFLAG_LEFT, "[NumPad+] Zoom in horizontally, hold shift to zoom vertically.", IGraphics::CORNER_L, 9.0f))
				{
					if(Input()->ShiftIsPressed())
						State.m_ZoomY.ChangeValue(-0.1f * State.m_ZoomY.GetValue());
					else
						State.m_ZoomX.ChangeValue(-0.1f * State.m_ZoomX.GetValue());
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

		auto NewValueRes = Editor()->UiDoValueSelector(&m_EnvelopeSelectorId, &Shifter, aBuf, Map()->m_SelectedEnvelope + 1, 1, Map()->m_vpEnvelopes.size(), 1, 1.0f, "Select the envelope.", false, false, IGraphics::CORNER_NONE, &EnvColor, false);
		int NewValue = NewValueRes.m_Value;
		if(NewValue - 1 != Map()->m_SelectedEnvelope)
		{
			Map()->m_SelectedEnvelope = NewValue - 1;
			CurrentEnvelopeSwitched = true;
		}

		if(Editor()->DoButton_FontIcon(&m_PrevEnvelopeButtonId, FontIcon::MINUS, 0, &Dec, BUTTONFLAG_LEFT, "Select previous envelope.", IGraphics::CORNER_L, 7.0f))
		{
			Map()->m_SelectedEnvelope--;
			if(Map()->m_SelectedEnvelope < 0)
				Map()->m_SelectedEnvelope = Map()->m_vpEnvelopes.size() - 1;
			CurrentEnvelopeSwitched = true;
		}

		if(Editor()->DoButton_FontIcon(&m_NextEnvelopeButtonId, FontIcon::PLUS, 0, &Inc, BUTTONFLAG_LEFT, "Select next envelope.", IGraphics::CORNER_R, 7.0f))
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

			m_NameInput.SetBuffer(pEnvelope->m_aName, sizeof(pEnvelope->m_aName));
			if(Editor()->DoEditBox(&m_NameInput, &Button, 10.0f, IGraphics::CORNER_ALL, "The name of the selected envelope."))
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

	Editor()->RenderBackground(View, Editor()->m_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		if(State.m_ResetZoom)
		{
			State.m_ResetZoom = false;
			ResetZoomEnvelope(pEnvelope, State.m_ActiveChannels);
		}

		ColorRGBA aColors[] = {ColorRGBA(1, 0.2f, 0.2f), ColorRGBA(0.2f, 1, 0.2f), ColorRGBA(0.2f, 0.2f, 1), ColorRGBA(1, 1, 0.2f)};

		CUIRect Button;
		ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

		static const char *const CHANNEL_NAMES[CEnvPoint::MAX_CHANNELS][CEnvPoint::MAX_CHANNELS] = {
			{"V", "", "", ""},
			{"", "", "", ""},
			{"X", "Y", "R", ""},
			{"R", "G", "B", "A"},
		};

		static const char *const CHANNEL_DESCRIPTIONS[CEnvPoint::MAX_CHANNELS][CEnvPoint::MAX_CHANNELS] = {
			{"Volume of the envelope.", "", "", ""},
			{"", "", "", ""},
			{"X-axis of the envelope.", "Y-axis of the envelope.", "Rotation of the envelope.", ""},
			{"Red value of the envelope.", "Green value of the envelope.", "Blue value of the envelope.", "Alpha value of the envelope."},
		};

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

				if(Editor()->DoButton_Env(&m_aChannelButtonIds[i], CHANNEL_NAMES[pEnvelope->GetChannels() - 1][i], State.m_ActiveChannels & Bit, &Button, CHANNEL_DESCRIPTIONS[pEnvelope->GetChannels() - 1][i], aColors[i], Corners))
					State.m_ActiveChannels ^= Bit;
			}
		}

		ToolBar.VSplitLeft(15.0f, nullptr, &ToolBar);
		ToolBar.VSplitLeft(40.0f, &Button, &ToolBar);

		const bool ShouldPan = m_Operation == EEnvelopeEditorOp::NONE && (Ui()->MouseButton(2) || (Ui()->MouseButton(0) && Input()->ModifierIsPressed()));
		if(Editor()->m_pContainerPanned == &m_EnvelopeEditorId)
		{
			if(!ShouldPan)
				Editor()->m_pContainerPanned = nullptr;
			else
			{
				State.m_Offset.x += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w;
				State.m_Offset.y -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h;
			}
		}

		if(Ui()->MouseInside(&View) && Editor()->m_Dialog == DIALOG_NONE)
		{
			Ui()->SetHotItem(&m_EnvelopeEditorId);

			if(ShouldPan && Editor()->m_pContainerPanned == nullptr)
				Editor()->m_pContainerPanned = &m_EnvelopeEditorId;

			if(Input()->KeyPress(KEY_KP_MULTIPLY) && CLineInput::GetActiveInput() == nullptr)
				ResetZoomEnvelope(pEnvelope, State.m_ActiveChannels);
			if(Input()->ShiftIsPressed())
			{
				if(Input()->KeyPress(KEY_KP_MINUS) && CLineInput::GetActiveInput() == nullptr)
					State.m_ZoomY.ChangeValue(0.1f * State.m_ZoomY.GetValue());
				if(Input()->KeyPress(KEY_KP_PLUS) && CLineInput::GetActiveInput() == nullptr)
					State.m_ZoomY.ChangeValue(-0.1f * State.m_ZoomY.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					State.m_ZoomY.ChangeValue(0.1f * State.m_ZoomY.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					State.m_ZoomY.ChangeValue(-0.1f * State.m_ZoomY.GetValue());
			}
			else
			{
				if(Input()->KeyPress(KEY_KP_MINUS) && CLineInput::GetActiveInput() == nullptr)
					State.m_ZoomX.ChangeValue(0.1f * State.m_ZoomX.GetValue());
				if(Input()->KeyPress(KEY_KP_PLUS) && CLineInput::GetActiveInput() == nullptr)
					State.m_ZoomX.ChangeValue(-0.1f * State.m_ZoomX.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					State.m_ZoomX.ChangeValue(0.1f * State.m_ZoomX.GetValue());
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					State.m_ZoomX.ChangeValue(-0.1f * State.m_ZoomX.GetValue());
			}
		}

		if(Ui()->HotItem() == &m_EnvelopeEditorId)
		{
			// do stuff
			if(Ui()->MouseButton(0))
			{
				m_EnvelopeEditorButtonUsed = 0;
				if(m_Operation != EEnvelopeEditorOp::BOX_SELECT && !Input()->ModifierIsPressed())
				{
					m_Operation = EEnvelopeEditorOp::BOX_SELECT;
					m_MouseStart = Ui()->MousePos();
				}
			}
			else if(m_EnvelopeEditorButtonUsed == 0)
			{
				if(Ui()->DoDoubleClickLogic(&m_EnvelopeEditorId) && !Input()->ModifierIsPressed())
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
				m_EnvelopeEditorButtonUsed = -1;
			}

			Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;
			str_copy(Editor()->m_aTooltip, "Double click to create a new point. Use shift to change the zoom axis. Press S to scale selected envelope points.");
		}

		UpdateZoomEnvelopeX(View);
		UpdateZoomEnvelopeY(View);

		{
			float UnitsPerLineY = 0.001f;
			static const float UNIT_PER_LINE_OPTIONS_Y[] = {0.005f, 0.01f, 0.025f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 2 * 32.0f, 5 * 32.0f, 10 * 32.0f, 20 * 32.0f, 50 * 32.0f, 100 * 32.0f};
			for(float Value : UNIT_PER_LINE_OPTIONS_Y)
			{
				if(Value / State.m_ZoomY.GetValue() * View.h < 40.0f)
					UnitsPerLineY = Value;
			}
			int NumLinesY = State.m_ZoomY.GetValue() / UnitsPerLineY + 1;

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			float BaseValue = static_cast<int>(State.m_Offset.y * State.m_ZoomY.GetValue() / UnitsPerLineY) * UnitsPerLineY;
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
			static const CTimeStep UNIT_PER_LINE_OPTIONS_Y[] = {5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 2s, 5s, 10s, 15s, 30s, 1min};
			for(CTimeStep Value : UNIT_PER_LINE_OPTIONS_Y)
			{
				if(Value.AsSeconds() / State.m_ZoomX.GetValue() * View.w < 160.0f)
					UnitsPerLineX = Value;
			}
			int NumLinesX = State.m_ZoomX.GetValue() / UnitsPerLineX.AsSeconds() + 1;

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.2f);

			CTimeStep BaseValue = UnitsPerLineX * static_cast<int>(State.m_Offset.x * State.m_ZoomX.GetValue() / UnitsPerLineX.AsSeconds());
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
				if(!(State.m_ActiveChannels & (1 << c)))
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
			float EndTimeTotal = std::max(0.000001f, pEnvelope->EndTime());
			float EndX = std::clamp(EnvelopeToScreenX(View, EndTimeTotal), View.x, View.x + View.w);
			float StartX = std::clamp(View.x + View.w * State.m_Offset.x, View.x, View.x + View.w);

			float EndTime = ScreenToEnvelopeX(View, EndX);
			float StartTime = ScreenToEnvelopeX(View, StartX);

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			IGraphics::CLineItemBatch LineItemBatch;
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				Graphics()->LinesBatchBegin(&LineItemBatch);
				if(State.m_ActiveChannels & (1 << c))
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
					const int ButtonResult = Editor()->DoButton_Editor(pId, GetCurveTypeNameShort(pEnvelope->m_vPoints[i].m_Curvetype), 0, &CurveButton, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT, "Switch curve type (N = step, L = linear, S = slow, F = fast, M = smooth, B = bezier).");
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
						m_PopupEnvelopeCurveType.m_pEnvelopeEditor = this;
						m_PopupEnvelopeCurveType.m_SelectedPoint = i;
						Ui()->DoPopupMenu(&m_PopupEnvelopeCurveType, Ui()->MouseX(), Ui()->MouseY(), 80, (float)NUM_CURVETYPES * 14.0f + 10.0f, &m_PopupEnvelopeCurveType, CPopupEnvelopeCurveType::Render);
					}
				}
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			RenderColorBar(ColorBar, pEnvelope);
		}

		// render handles
		if(CurrentEnvelopeSwitched)
		{
			Map()->DeselectEnvPoints();
			State.m_ResetZoom = true;
		}

		{
			const auto &&ShowPopupEnvPoint = [&]() {
				m_PopupEnvelopePoint.m_pEnvelopeEditor = this;
				Ui()->DoPopupMenu(&m_PopupEnvelopePoint, Ui()->MouseX(), Ui()->MouseY(), 150, 56 + (pEnvelope->GetChannels() == 4 && !Map()->IsTangentSelected() ? 16.0f : 0.0f), &m_PopupEnvelopePoint, CPopupEnvelopePoint::Render);
			};

			if(m_Operation == EEnvelopeEditorOp::NONE)
			{
				UpdateHotEnvelopePoint(View, pEnvelope.get(), State.m_ActiveChannels);
				if(!Ui()->MouseButton(0))
					Map()->m_EnvOpTracker.Stop(false);
			}
			else
			{
				Map()->m_EnvOpTracker.Begin(m_Operation);
			}

			Ui()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->GetChannels(); c++)
			{
				if(!(State.m_ActiveChannels & (1 << c)))
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
							Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;

							if(m_Operation == EEnvelopeEditorOp::SELECT)
							{
								if(length_squared(m_MouseStart - Ui()->MousePos()) > 20.0f)
								{
									m_Operation = EEnvelopeEditorOp::DRAG_POINT;

									if(!Map()->IsEnvPointSelected(i, c))
										Map()->SelectEnvPoint(i, c);
								}
							}

							if(m_Operation == EEnvelopeEditorOp::DRAG_POINT || m_Operation == EEnvelopeEditorOp::DRAG_POINT_X || m_Operation == EEnvelopeEditorOp::DRAG_POINT_Y)
							{
								if(Input()->ShiftIsPressed())
								{
									if(m_Operation == EEnvelopeEditorOp::DRAG_POINT || m_Operation == EEnvelopeEditorOp::DRAG_POINT_Y)
									{
										m_Operation = EEnvelopeEditorOp::DRAG_POINT_X;
										m_vAccurateDragValuesX.clear();
										for(auto [SelectedIndex, _] : Map()->m_vSelectedEnvelopePoints)
											m_vAccurateDragValuesX.push_back(pEnvelope->m_vPoints[SelectedIndex].m_Time.GetInternal());
									}
									else
									{
										float DeltaX = ScreenToEnvelopeDeltaX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);

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

											DeltaX = ClampDelta(m_vAccurateDragValuesX[k], DeltaX, BoundLow.GetInternal(), BoundHigh.GetInternal());
										}
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
											m_vAccurateDragValuesX[k] += DeltaX;
											pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round(m_vAccurateDragValuesX[k]));
										}
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
											if(SelectedIndex == 0 && pEnvelope->m_vPoints[SelectedIndex].m_Time != CFixedTime(0))
											{
												RemoveTimeOffsetEnvelope(pEnvelope);
												float Offset = m_vAccurateDragValuesX[k];
												for(auto &Value : m_vAccurateDragValuesX)
													Value -= Offset;
												break;
											}
										}
									}
								}
								else
								{
									if(m_Operation == EEnvelopeEditorOp::DRAG_POINT || m_Operation == EEnvelopeEditorOp::DRAG_POINT_X)
									{
										m_Operation = EEnvelopeEditorOp::DRAG_POINT_Y;
										m_vAccurateDragValuesY.clear();
										for(auto [SelectedIndex, SelectedChannel] : Map()->m_vSelectedEnvelopePoints)
											m_vAccurateDragValuesY.push_back(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel]);
									}
									else
									{
										float DeltaY = ScreenToEnvelopeDeltaY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
										for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
										{
											auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
											m_vAccurateDragValuesY[k] -= DeltaY;
											pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(m_vAccurateDragValuesY[k]);

											if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
											{
												pEnvelope->m_vPoints[i].m_aValues[c] = std::clamp(pEnvelope->m_vPoints[i].m_aValues[c], 0, 1024);
												m_vAccurateDragValuesY[k] = std::clamp<float>(m_vAccurateDragValuesY[k], 0, 1024);
											}
										}
									}
								}
							}

							if(m_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
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
										m_PopupEnvelopePointMulti.m_pEnvelopeEditor = this;
										Ui()->DoPopupMenu(&m_PopupEnvelopePointMulti, Ui()->MouseX(), Ui()->MouseY(), 80, 22, &m_PopupEnvelopePointMulti, CPopupEnvelopePointMulti::Render);
									}
									Ui()->SetActiveItem(nullptr);
									m_Operation = EEnvelopeEditorOp::NONE;
								}
							}
							else if(!Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(nullptr);
								Map()->m_SelectedQuadEnvelope = -1;

								if(m_Operation == EEnvelopeEditorOp::SELECT)
								{
									if(Input()->ShiftIsPressed())
										Map()->ToggleEnvPoint(i, c);
									else
										Map()->SelectEnvPoint(i, c);
								}

								m_Operation = EEnvelopeEditorOp::NONE;
								Map()->OnModify();
							}

							Graphics()->SetColor(1, 1, 1, 1);
						}
						else if(Ui()->HotItem() == pId)
						{
							if(Ui()->MouseButton(0))
							{
								Ui()->SetActiveItem(pId);
								m_Operation = EEnvelopeEditorOp::SELECT;
								Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;
								m_MouseStart = Ui()->MousePos();
							}
							else if(Ui()->MouseButtonClicked(1))
							{
								if(Input()->ShiftIsPressed())
								{
									Map()->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionDeleteEnvelopePoint>(Map(), Map()->m_SelectedEnvelope, i));
								}
								else
								{
									m_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
									if(!Map()->IsEnvPointSelected(i, c))
										Map()->SelectEnvPoint(i, c);
									Ui()->SetActiveItem(pId);
								}
							}

							Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;
							Graphics()->SetColor(1, 1, 1, 1);
							str_copy(Editor()->m_aTooltip, "Envelope point. Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time. Shift+right click to delete.");
							Editor()->m_pUiGotContext = pId;
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
								Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;

								if(m_Operation == EEnvelopeEditorOp::SELECT)
								{
									if(length_squared(m_MouseStart - Ui()->MousePos()) > 20.0f)
									{
										m_Operation = EEnvelopeEditorOp::DRAG_POINT;

										m_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c].GetInternal())};
										m_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c])};

										if(!Map()->IsTangentOutPointSelected(i, c))
											Map()->SelectTangentOutPoint(i, c);
									}
								}

								if(m_Operation == EEnvelopeEditorOp::DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDeltaX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDeltaY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									m_vAccurateDragValuesX[0] += DeltaX;
									m_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = CFixedTime(std::round(m_vAccurateDragValuesX[0]));
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaY[c] = std::round(m_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c] = std::clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c], CFixedTime(0), CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time);
									m_vAccurateDragValuesX[0] = std::clamp<float>(m_vAccurateDragValuesX[0], 0, (CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x + View.w)) - pEnvelope->m_vPoints[i].m_Time).GetInternal());
								}

								if(m_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(Map()->IsTangentOutPointSelected(i, c))
										{
											Map()->m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										m_Operation = EEnvelopeEditorOp::NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									Map()->m_SelectedQuadEnvelope = -1;

									if(m_Operation == EEnvelopeEditorOp::SELECT)
										Map()->SelectTangentOutPoint(i, c);

									m_Operation = EEnvelopeEditorOp::NONE;
									Map()->OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									m_Operation = EEnvelopeEditorOp::SELECT;
									Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;
									m_MouseStart = Ui()->MousePos();
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
										m_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
										Map()->SelectTangentOutPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(Editor()->m_aTooltip, "Bezier out-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift+right click to reset.");
								Editor()->m_pUiGotContext = pId;
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
								Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;

								if(m_Operation == EEnvelopeEditorOp::SELECT)
								{
									if(length_squared(m_MouseStart - Ui()->MousePos()) > 20.0f)
									{
										m_Operation = EEnvelopeEditorOp::DRAG_POINT;

										m_vAccurateDragValuesX = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c].GetInternal())};
										m_vAccurateDragValuesY = {static_cast<float>(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c])};

										if(!Map()->IsTangentInPointSelected(i, c))
											Map()->SelectTangentInPoint(i, c);
									}
								}

								if(m_Operation == EEnvelopeEditorOp::DRAG_POINT)
								{
									float DeltaX = ScreenToEnvelopeDeltaX(View, Ui()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 50.0f : 1000.0f);
									float DeltaY = ScreenToEnvelopeDeltaY(View, Ui()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 51.2f : 1024.0f);
									m_vAccurateDragValuesX[0] += DeltaX;
									m_vAccurateDragValuesY[0] -= DeltaY;

									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = CFixedTime(std::round(m_vAccurateDragValuesX[0]));
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaY[c] = std::round(m_vAccurateDragValuesY[0]);

									// clamp time value
									pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c] = std::clamp(pEnvelope->m_vPoints[i].m_Bezier.m_aInTangentDeltaX[c], CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time, CFixedTime(0));
									m_vAccurateDragValuesX[0] = std::clamp<float>(m_vAccurateDragValuesX[0], (CFixedTime::FromSeconds(ScreenToEnvelopeX(View, View.x)) - pEnvelope->m_vPoints[i].m_Time).GetInternal(), 0);
								}

								if(m_Operation == EEnvelopeEditorOp::CONTEXT_MENU)
								{
									if(!Ui()->MouseButton(1))
									{
										if(Map()->IsTangentInPointSelected(i, c))
										{
											Map()->m_UpdateEnvPointInfo = true;
											ShowPopupEnvPoint();
										}
										Ui()->SetActiveItem(nullptr);
										m_Operation = EEnvelopeEditorOp::NONE;
									}
								}
								else if(!Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(nullptr);
									Map()->m_SelectedQuadEnvelope = -1;

									if(m_Operation == EEnvelopeEditorOp::SELECT)
										Map()->SelectTangentInPoint(i, c);

									m_Operation = EEnvelopeEditorOp::NONE;
									Map()->OnModify();
								}

								Graphics()->SetColor(1, 1, 1, 1);
							}
							else if(Ui()->HotItem() == pId)
							{
								if(Ui()->MouseButton(0))
								{
									Ui()->SetActiveItem(pId);
									m_Operation = EEnvelopeEditorOp::SELECT;
									Map()->m_SelectedQuadEnvelope = Map()->m_SelectedEnvelope;
									m_MouseStart = Ui()->MousePos();
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
										m_Operation = EEnvelopeEditorOp::CONTEXT_MENU;
										Map()->SelectTangentInPoint(i, c);
										Ui()->SetActiveItem(pId);
									}
								}

								Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;
								Graphics()->SetColor(1, 1, 1, 1);
								str_copy(Editor()->m_aTooltip, "Bezier in-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift+right click to reset.");
								Editor()->m_pUiGotContext = pId;
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
		if(m_Operation == EEnvelopeEditorOp::NONE && !m_NameInput.IsActive() && Input()->KeyIsPressed(KEY_S) && !Input()->ModifierIsPressed() && !Map()->m_vSelectedEnvelopePoints.empty())
		{
			m_Operation = EEnvelopeEditorOp::SCALE;
			m_ScaleFactor.x = 1.0f;
			m_ScaleFactor.y = 1.0f;
			auto [FirstPointIndex, FirstPointChannel] = Map()->m_vSelectedEnvelopePoints.front();

			float MaximumX = pEnvelope->m_vPoints[FirstPointIndex].m_Time.GetInternal();
			float MinimumX = MaximumX;
			m_vInitialPositionsX.clear();
			for(auto [SelectedIndex, _] : Map()->m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_Time.GetInternal();
				m_vInitialPositionsX.push_back(Value);
				MaximumX = std::max(MaximumX, Value);
				MinimumX = std::min(MinimumX, Value);
			}
			m_Midpoint.x = (MaximumX - MinimumX) / 2.0f + MinimumX;

			float MaximumY = pEnvelope->m_vPoints[FirstPointIndex].m_aValues[FirstPointChannel];
			float MinimumY = MaximumY;
			m_vInitialPositionsY.clear();
			for(auto [SelectedIndex, SelectedChannel] : Map()->m_vSelectedEnvelopePoints)
			{
				float Value = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
				m_vInitialPositionsY.push_back(Value);
				MaximumY = std::max(MaximumY, Value);
				MinimumY = std::min(MinimumY, Value);
			}
			m_Midpoint.y = (MaximumY - MinimumY) / 2.0f + MinimumY;
		}

		if(m_Operation == EEnvelopeEditorOp::SCALE)
		{
			str_copy(Editor()->m_aTooltip, "Press shift to scale the time. Press alt to scale along midpoint. Press ctrl to be more precise.");

			if(Input()->ShiftIsPressed())
			{
				m_ScaleFactor.x += Ui()->MouseDeltaX() / Graphics()->ScreenWidth() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				float Midpoint = Input()->AltIsPressed() ? m_Midpoint.x : 0.0f;
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

					float Value = m_vInitialPositionsX[k];
					float ScaleBoundLow = (BoundLow.GetInternal() - Midpoint) / (Value - Midpoint);
					float ScaleBoundHigh = (BoundHigh.GetInternal() - Midpoint) / (Value - Midpoint);
					float ScaleBoundMin = std::min(ScaleBoundLow, ScaleBoundHigh);
					float ScaleBoundMax = std::max(ScaleBoundLow, ScaleBoundHigh);
					m_ScaleFactor.x = std::clamp(m_ScaleFactor.x, ScaleBoundMin, ScaleBoundMax);
				}

				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
					float ScaleMinimum = m_vInitialPositionsX[k] - Midpoint > CFixedTime(1).AsSeconds() ? CFixedTime(1).AsSeconds() / (m_vInitialPositionsX[k] - Midpoint) : 0.0f;
					float ScaleFactor = std::max(ScaleMinimum, m_ScaleFactor.x);
					pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round((m_vInitialPositionsX[k] - Midpoint) * ScaleFactor + Midpoint));
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
						m_Midpoint.x -= Offset;
						for(auto &Value : m_vInitialPositionsX)
							Value -= Offset;
						break;
					}
				}
			}
			else
			{
				m_ScaleFactor.y -= Ui()->MouseDeltaY() / Graphics()->ScreenHeight() * (Input()->ModifierIsPressed() ? 0.5f : 10.0f);
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
					if(Input()->AltIsPressed())
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round((m_vInitialPositionsY[k] - m_Midpoint.y) * m_ScaleFactor.y + m_Midpoint.y);
					else
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(m_vInitialPositionsY[k] * m_ScaleFactor.y);

					if(pEnvelope->GetChannels() == 1 || pEnvelope->GetChannels() == 4)
						pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::clamp(pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel], 0, 1024);
				}
			}

			if(Ui()->MouseButton(0))
			{
				m_Operation = EEnvelopeEditorOp::NONE;
				Map()->m_EnvOpTracker.Stop(false);
			}
			else if(Ui()->MouseButton(1) || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
			{
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					int SelectedIndex = Map()->m_vSelectedEnvelopePoints[k].first;
					pEnvelope->m_vPoints[SelectedIndex].m_Time = CFixedTime(std::round(m_vInitialPositionsX[k]));
				}
				for(size_t k = 0; k < Map()->m_vSelectedEnvelopePoints.size(); k++)
				{
					auto [SelectedIndex, SelectedChannel] = Map()->m_vSelectedEnvelopePoints[k];
					pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] = std::round(m_vInitialPositionsY[k]);
				}
				RemoveTimeOffsetEnvelope(pEnvelope);
				m_Operation = EEnvelopeEditorOp::NONE;
			}
		}

		// handle box selection
		if(m_Operation == EEnvelopeEditorOp::BOX_SELECT)
		{
			Ui()->ClipEnable(&View);
			CUIRect SelectionRect;
			SelectionRect.x = m_MouseStart.x;
			SelectionRect.y = m_MouseStart.y;
			SelectionRect.w = Ui()->MouseX() - m_MouseStart.x;
			SelectionRect.h = Ui()->MouseY() - m_MouseStart.y;
			SelectionRect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			Ui()->ClipDisable();

			if(!Ui()->MouseButton(0))
			{
				m_Operation = EEnvelopeEditorOp::NONE;
				Ui()->SetActiveItem(nullptr);

				float TimeStart = ScreenToEnvelopeX(View, m_MouseStart.x);
				float TimeEnd = ScreenToEnvelopeX(View, Ui()->MouseX());
				float ValueStart = ScreenToEnvelopeY(View, m_MouseStart.y);
				float ValueEnd = ScreenToEnvelopeY(View, Ui()->MouseY());

				float TimeMin = std::min(TimeStart, TimeEnd);
				float TimeMax = std::max(TimeStart, TimeEnd);
				float ValueMin = std::min(ValueStart, ValueEnd);
				float ValueMax = std::max(ValueStart, ValueEnd);

				if(!Input()->ShiftIsPressed())
					Map()->DeselectEnvPoints();

				for(int i = 0; i < (int)pEnvelope->m_vPoints.size(); i++)
				{
					for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
					{
						if(!(State.m_ActiveChannels & (1 << c)))
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

void CEnvelopeEditor::RenderColorBar(CUIRect ColorBar, const std::shared_ptr<CEnvelope> &pEnvelope)
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
	const float StartX = std::max(EnvelopeToScreenX(ColorBar, 0.0f), ColorBar.x);
	const float TotalWidth = std::min(EnvelopeToScreenX(ColorBar, pEnvelope->EndTime()) - StartX, ColorBar.x + ColorBar.w - StartX);

	Ui()->ClipEnable(&ColorBar);
	CUIRect ColorBarBackground = CUIRect{StartX, ColorBar.y, TotalWidth, ColorBar.h};
	Editor()->RenderBackground(ColorBarBackground, Editor()->m_CheckerTexture, ColorBarBackground.h, 1.0f);
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
			const float ClampedPointStartX = std::max(EnvelopeToScreenX(ColorBar, PointStartTime), ColorBar.x);
			const float ClampedPointEndX = std::min(EnvelopeToScreenX(ColorBar, PointEndTime), ColorBar.x + ColorBar.w);
			Steps = std::clamp((int)std::sqrt(5.0f * (ClampedPointEndX - ClampedPointStartX)), 1, 250);
		}
		const float OverallSectionStartTime = Steps == 1 ? PointStartTime : std::max(PointStartTime, ViewStartTime);
		const float OverallSectionEndTime = Steps == 1 ? PointEndTime : std::min(PointEndTime, ViewEndTime);
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

void CEnvelopeEditor::UpdateHotEnvelopePoint(const CUIRect &View, const CEnvelope *pEnvelope, int ActiveChannels)
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

void CEnvelopeEditor::ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View)
{
	float PosX = g_Config.m_EdZoomTarget ? (Ui()->MouseX() - View.x) / View.w : 0.5f;
	Map()->m_EnvelopeEditorState.m_Offset.x = PosX - (PosX - Map()->m_EnvelopeEditorState.m_Offset.x) * ZoomFactor;
}

void CEnvelopeEditor::UpdateZoomEnvelopeX(const CUIRect &View)
{
	float OldZoom = Map()->m_EnvelopeEditorState.m_ZoomX.GetValue();
	if(Map()->m_EnvelopeEditorState.m_ZoomX.UpdateValue())
		ZoomAdaptOffsetX(OldZoom / Map()->m_EnvelopeEditorState.m_ZoomX.GetValue(), View);
}

void CEnvelopeEditor::ZoomAdaptOffsetY(float ZoomFactor, const CUIRect &View)
{
	float PosY = g_Config.m_EdZoomTarget ? 1.0f - (Ui()->MouseY() - View.y) / View.h : 0.5f;
	Map()->m_EnvelopeEditorState.m_Offset.y = PosY - (PosY - Map()->m_EnvelopeEditorState.m_Offset.y) * ZoomFactor;
}

void CEnvelopeEditor::UpdateZoomEnvelopeY(const CUIRect &View)
{
	float OldZoom = Map()->m_EnvelopeEditorState.m_ZoomY.GetValue();
	if(Map()->m_EnvelopeEditorState.m_ZoomY.UpdateValue())
		ZoomAdaptOffsetY(OldZoom / Map()->m_EnvelopeEditorState.m_ZoomY.GetValue(), View);
}

void CEnvelopeEditor::ResetZoomEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels)
{
	auto [Bottom, Top] = pEnvelope->GetValueRange(ActiveChannels);
	float EndTime = pEnvelope->EndTime();
	float ValueRange = absolute(Top - Bottom);
	CState &State = Map()->m_EnvelopeEditorState;

	if(ValueRange < State.m_ZoomY.GetMinValue())
	{
		// Set view to some sane default if range is too small
		State.m_Offset.y = 0.5f - ValueRange / State.m_ZoomY.GetMinValue() / 2.0f - Bottom / State.m_ZoomY.GetMinValue();
		State.m_ZoomY.SetValueInstant(State.m_ZoomY.GetMinValue());
	}
	else if(ValueRange > State.m_ZoomY.GetMaxValue())
	{
		State.m_Offset.y = -Bottom / State.m_ZoomY.GetMaxValue();
		State.m_ZoomY.SetValueInstant(State.m_ZoomY.GetMaxValue());
	}
	else
	{
		// calculate biggest possible spacing
		float SpacingFactor = std::min(1.25f, State.m_ZoomY.GetMaxValue() / ValueRange);
		State.m_ZoomY.SetValueInstant(SpacingFactor * ValueRange);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;

		if(Top >= 0 && Bottom >= 0)
			State.m_Offset.y = Spacing - Bottom / State.m_ZoomY.GetValue();
		else if(Top <= 0 && Bottom <= 0)
			State.m_Offset.y = Spacing - Bottom / State.m_ZoomY.GetValue();
		else
			State.m_Offset.y = Spacing + Space * absolute(Bottom) / ValueRange;
	}

	if(EndTime < State.m_ZoomX.GetMinValue())
	{
		State.m_Offset.x = 0.5f - EndTime / State.m_ZoomX.GetMinValue();
		State.m_ZoomX.SetValueInstant(State.m_ZoomX.GetMinValue());
	}
	else if(EndTime > State.m_ZoomX.GetMaxValue())
	{
		State.m_Offset.x = 0.0f;
		State.m_ZoomX.SetValueInstant(State.m_ZoomX.GetMaxValue());
	}
	else
	{
		float SpacingFactor = std::min(1.25f, State.m_ZoomX.GetMaxValue() / EndTime);
		State.m_ZoomX.SetValueInstant(SpacingFactor * EndTime);
		float Space = 1.0f / SpacingFactor;
		float Spacing = (1.0f - Space) / 2.0f;
		State.m_Offset.x = Spacing;
	}
}

float CEnvelopeEditor::ScreenToEnvelopeX(const CUIRect &View, float x) const
{
	return (x - View.x - View.w * Map()->m_EnvelopeEditorState.m_Offset.x) / View.w * Map()->m_EnvelopeEditorState.m_ZoomX.GetValue();
}

float CEnvelopeEditor::EnvelopeToScreenX(const CUIRect &View, float x) const
{
	return View.x + View.w * Map()->m_EnvelopeEditorState.m_Offset.x + x / Map()->m_EnvelopeEditorState.m_ZoomX.GetValue() * View.w;
}

float CEnvelopeEditor::ScreenToEnvelopeY(const CUIRect &View, float y) const
{
	return (View.h - y + View.y) / View.h * Map()->m_EnvelopeEditorState.m_ZoomY.GetValue() - Map()->m_EnvelopeEditorState.m_Offset.y * Map()->m_EnvelopeEditorState.m_ZoomY.GetValue();
}

float CEnvelopeEditor::EnvelopeToScreenY(const CUIRect &View, float y) const
{
	return View.y + View.h - y / Map()->m_EnvelopeEditorState.m_ZoomY.GetValue() * View.h - Map()->m_EnvelopeEditorState.m_Offset.y * View.h;
}

float CEnvelopeEditor::ScreenToEnvelopeDeltaX(const CUIRect &View, float DeltaX)
{
	return DeltaX / Graphics()->ScreenWidth() * Ui()->Screen()->w / View.w * Map()->m_EnvelopeEditorState.m_ZoomX.GetValue();
}

float CEnvelopeEditor::ScreenToEnvelopeDeltaY(const CUIRect &View, float DeltaY)
{
	return DeltaY / Graphics()->ScreenHeight() * Ui()->Screen()->h / View.h * Map()->m_EnvelopeEditorState.m_ZoomY.GetValue();
}

void CEnvelopeEditor::RemoveTimeOffsetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope)
{
	CFixedTime TimeOffset = pEnvelope->m_vPoints[0].m_Time;
	for(auto &Point : pEnvelope->m_vPoints)
		Point.m_Time -= TimeOffset;

	Map()->m_EnvelopeEditorState.m_Offset.x += TimeOffset.AsSeconds() / Map()->m_EnvelopeEditorState.m_ZoomX.GetValue();
}

CUi::EPopupMenuFunctionResult CEnvelopeEditor::CPopupEnvelopePoint::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupEnvelopePoint *pPopupEnvelopePointContext = static_cast<CPopupEnvelopePoint *>(pContext);
	CEnvelopeEditor *pEnvelopeEditor = pPopupEnvelopePointContext->m_pEnvelopeEditor;
	CEditorMap *pMap = pEnvelopeEditor->Map();
	CEditor *pEditor = pEnvelopeEditor->Editor();

	if(pMap->m_SelectedEnvelope < 0 || pMap->m_SelectedEnvelope >= (int)pMap->m_vpEnvelopes.size())
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}

	const float RowHeight = 12.0f;
	CUIRect Row, Label, EditBox;

	pEditor->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::SELECTED;

	std::shared_ptr<CEnvelope> pEnvelope = pMap->m_vpEnvelopes[pMap->m_SelectedEnvelope];

	if(pEnvelope->GetChannels() == 4 && !pMap->IsTangentSelected())
	{
		View.HSplitTop(RowHeight, &Row, &View);
		View.HSplitTop(4.0f, nullptr, &View);
		Row.VSplitLeft(60.0f, &Label, &Row);
		Row.VSplitLeft(10.0f, nullptr, &EditBox);
		pEditor->Ui()->DoLabel(&Label, "Color:", RowHeight - 2.0f, TEXTALIGN_ML);

		const auto SelectedPoint = pMap->m_vSelectedEnvelopePoints.front();
		const int SelectedIndex = SelectedPoint.first;
		auto *pValues = pEnvelope->m_vPoints[SelectedIndex].m_aValues;
		const ColorRGBA Color = pEnvelope->m_vPoints[SelectedIndex].ColorValue();
		const auto &&SetColor = [&](ColorRGBA NewColor) {
			if(Color == NewColor && pEditor->m_ColorPickerPopupContext.m_State == EEditState::EDITING)
				return;

			if(pEditor->m_ColorPickerPopupContext.m_State == EEditState::START || pEditor->m_ColorPickerPopupContext.m_State == EEditState::ONE_GO)
			{
				for(int Channel = 0; Channel < 4; ++Channel)
					pPopupEnvelopePointContext->m_aValues[Channel] = pValues[Channel];
			}

			pEnvelope->m_vPoints[SelectedIndex].SetColorValue(NewColor);

			if(pEditor->m_ColorPickerPopupContext.m_State == EEditState::END || pEditor->m_ColorPickerPopupContext.m_State == EEditState::ONE_GO)
			{
				std::vector<std::shared_ptr<IEditorAction>> vpActions(4);

				for(int Channel = 0; Channel < 4; ++Channel)
				{
					vpActions[Channel] = std::make_shared<CEditorActionEnvelopeEditPoint>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, Channel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, pPopupEnvelopePointContext->m_aValues[Channel], f2fx(NewColor[Channel]));
				}

				char aDisplay[256];
				str_format(aDisplay, sizeof(aDisplay), "Edit color of point %d of envelope %d", SelectedIndex, pMap->m_SelectedEnvelope);
				pMap->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(pMap, vpActions, aDisplay));
			}

			pMap->m_UpdateEnvPointInfo = true;
			pMap->OnModify();
		};
		pEditor->DoColorPickerButton(&pPopupEnvelopePointContext->m_ColorPickerButtonId, &EditBox, Color, SetColor);
	}

	if(pMap->m_UpdateEnvPointInfo)
	{
		pMap->m_UpdateEnvPointInfo = false;

		const auto &[CurrentTime, CurrentValue] = pMap->SelectedEnvelopeTimeAndValue();

		// update displayed text
		pPopupEnvelopePointContext->m_ValueInput.SetFloat(fx2f(CurrentValue));
		pPopupEnvelopePointContext->m_TimeInput.SetFloat(CurrentTime.AsSeconds());

		pPopupEnvelopePointContext->m_CurrentTime = pPopupEnvelopePointContext->m_TimeInput.GetFloat();
		pPopupEnvelopePointContext->m_CurrentValue = pPopupEnvelopePointContext->m_ValueInput.GetFloat();
	}

	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->Ui()->DoLabel(&Label, "Value:", RowHeight - 2.0f, TEXTALIGN_ML);
	pEditor->DoEditBox(&pPopupEnvelopePointContext->m_ValueInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The value of the selected envelope point.");

	View.HSplitTop(4.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(60.0f, &Label, &Row);
	Row.VSplitLeft(10.0f, nullptr, &EditBox);
	pEditor->Ui()->DoLabel(&Label, "Time (in s):", RowHeight - 2.0f, TEXTALIGN_ML);
	pEditor->DoEditBox(&pPopupEnvelopePointContext->m_TimeInput, &EditBox, RowHeight - 2.0f, IGraphics::CORNER_ALL, "The time of the selected envelope point.");

	if(pEditor->Input()->KeyIsPressed(KEY_RETURN) || pEditor->Input()->KeyIsPressed(KEY_KP_ENTER))
	{
		float CurrentTime = pPopupEnvelopePointContext->m_TimeInput.GetFloat();
		float CurrentValue = pPopupEnvelopePointContext->m_ValueInput.GetFloat();
		if(!(absolute(CurrentTime - pPopupEnvelopePointContext->m_CurrentTime) < 0.0001f && absolute(CurrentValue - pPopupEnvelopePointContext->m_CurrentValue) < 0.0001f))
		{
			const auto &[OldTime, OldValue] = pMap->SelectedEnvelopeTimeAndValue();

			if(pMap->IsTangentInSelected())
			{
				auto [SelectedIndex, SelectedChannel] = pMap->m_SelectedTangentInPoint;

				pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::TANGENT_IN, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));
				CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel]).AsSeconds();
			}
			else if(pMap->IsTangentOutSelected())
			{
				auto [SelectedIndex, SelectedChannel] = pMap->m_SelectedTangentOutPoint;

				pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::TANGENT_OUT, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));
				CurrentTime = (pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel]).AsSeconds();
			}
			else
			{
				auto [SelectedIndex, SelectedChannel] = pMap->m_vSelectedEnvelopePoints.front();
				pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionEditEnvelopePointValue>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEditEnvelopePointValue::EType::POINT, OldTime, OldValue, CFixedTime::FromSeconds(CurrentTime), f2fx(CurrentValue)));

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

			pPopupEnvelopePointContext->m_TimeInput.SetFloat(CFixedTime::FromSeconds(CurrentTime).AsSeconds());
			pPopupEnvelopePointContext->m_ValueInput.SetFloat(fx2f(f2fx(CurrentValue)));

			pPopupEnvelopePointContext->m_CurrentTime = pPopupEnvelopePointContext->m_TimeInput.GetFloat();
			pPopupEnvelopePointContext->m_CurrentValue = pPopupEnvelopePointContext->m_ValueInput.GetFloat();
		}
	}

	View.HSplitTop(6.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	const char *pButtonText = pMap->IsTangentSelected() ? "Reset" : "Delete";
	const char *pTooltip = pMap->IsTangentSelected() ? "Reset tangent point to default value." : "Delete current envelope point in all channels.";
	if(pEditor->DoButton_Editor(&pPopupEnvelopePointContext->m_DeleteButtonId, pButtonText, 0, &Row, BUTTONFLAG_LEFT, pTooltip))
	{
		if(pMap->IsTangentInSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pMap->m_SelectedTangentInPoint;
			const auto &[OldTime, OldValue] = pMap->SelectedEnvelopeTimeAndValue();
			pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionResetEnvelopePointTangent>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, true, OldTime, OldValue));
		}
		else if(pMap->IsTangentOutSelected())
		{
			auto [SelectedIndex, SelectedChannel] = pMap->m_SelectedTangentOutPoint;
			const auto &[OldTime, OldValue] = pMap->SelectedEnvelopeTimeAndValue();
			pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionResetEnvelopePointTangent>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, false, OldTime, OldValue));
		}
		else
		{
			auto [SelectedIndex, SelectedChannel] = pMap->m_vSelectedEnvelopePoints.front();
			pMap->m_EnvelopeEditorHistory.Execute(std::make_shared<CEditorActionDeleteEnvelopePoint>(pMap, pMap->m_SelectedEnvelope, SelectedIndex));
		}

		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CEnvelopeEditor::CPopupEnvelopePointMulti::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupEnvelopePointMulti *pPopupEnvelopePointMultiContext = static_cast<CPopupEnvelopePointMulti *>(pContext);
	CEnvelopeEditor *pEnvelopeEditor = pPopupEnvelopePointMultiContext->m_pEnvelopeEditor;
	CEditor *pEditor = pEnvelopeEditor->Editor();

	CUIRect ProjectOntoButton;
	View.HSplitTop(14.0f, &ProjectOntoButton, &View);
	if(pEditor->DoButton_Editor(&pPopupEnvelopePointMultiContext->m_ProjectOntoButtonId, "Project onto", 0, &ProjectOntoButton, BUTTONFLAG_LEFT, "Project all selected envelopes onto the curve between the first and last selected envelope."))
	{
		pEnvelopeEditor->m_PopupEnvelopePointCurveType.m_pEnvelopeEditor = pEnvelopeEditor;
		pEditor->Ui()->DoPopupMenu(&pEnvelopeEditor->m_PopupEnvelopePointCurveType, pEditor->Ui()->MouseX(), pEditor->Ui()->MouseY(), 80, 80, &pEnvelopeEditor->m_PopupEnvelopePointCurveType, CEnvelopeEditor::CPopupEnvelopePointCurveType::Render);
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CEnvelopeEditor::CPopupEnvelopePointCurveType::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupEnvelopePointCurveType *pPopupEnvelopePointCurveTypeContext = static_cast<CPopupEnvelopePointCurveType *>(pContext);
	CEnvelopeEditor *pEnvelopeEditor = pPopupEnvelopePointCurveTypeContext->m_pEnvelopeEditor;
	CEditorMap *pMap = pEnvelopeEditor->Map();
	CEditor *pEditor = pEnvelopeEditor->Editor();

	int SelectedCurveType = -1;
	for(int Type = 0; Type <= CURVETYPE_SMOOTH; Type++)
	{
		CUIRect Button;
		View.HSplitTop(14.0f, &Button, &View);
		if(pEditor->DoButton_MenuItem(&pPopupEnvelopePointCurveTypeContext->m_aButtonIds[Type], CURVE_TYPE_NAMES[Type], 0, &Button))
		{
			SelectedCurveType = Type;
		}
	}
	if(SelectedCurveType < 0)
	{
		return CUi::POPUP_KEEP_OPEN;
	}

	std::shared_ptr<CEnvelope> pEnvelope = pMap->m_vpEnvelopes.at(pMap->m_SelectedEnvelope);
	std::vector<std::shared_ptr<IEditorAction>> vpActions;
	for(int Channel = 0; Channel < pEnvelope->GetChannels(); Channel++)
	{
		int FirstSelectedIndex = pEnvelope->m_vPoints.size();
		int LastSelectedIndex = -1;
		for(auto [SelectedIndex, SelectedChannel] : pMap->m_vSelectedEnvelopePoints)
		{
			if(SelectedChannel == Channel)
			{
				FirstSelectedIndex = std::min(FirstSelectedIndex, SelectedIndex);
				LastSelectedIndex = std::max(LastSelectedIndex, SelectedIndex);
			}
		}

		if(FirstSelectedIndex < (int)pEnvelope->m_vPoints.size() && LastSelectedIndex >= 0 && FirstSelectedIndex != LastSelectedIndex)
		{
			CEnvPoint FirstPoint = pEnvelope->m_vPoints[FirstSelectedIndex];
			CEnvPoint LastPoint = pEnvelope->m_vPoints[LastSelectedIndex];

			CEnvelope HelperEnvelope(1);
			HelperEnvelope.AddPoint(FirstPoint.m_Time, {FirstPoint.m_aValues[Channel], 0, 0, 0});
			HelperEnvelope.AddPoint(LastPoint.m_Time, {LastPoint.m_aValues[Channel], 0, 0, 0});
			HelperEnvelope.m_vPoints[0].m_Curvetype = SelectedCurveType;

			for(auto [SelectedIndex, SelectedChannel] : pMap->m_vSelectedEnvelopePoints)
			{
				if(SelectedChannel == Channel &&
					SelectedIndex != FirstSelectedIndex &&
					SelectedIndex != LastSelectedIndex)
				{
					CEnvPoint &CurrentPoint = pEnvelope->m_vPoints[SelectedIndex];
					ColorRGBA Channels = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
					HelperEnvelope.Eval(CurrentPoint.m_Time.AsSeconds(), Channels, 1);
					int PrevValue = CurrentPoint.m_aValues[Channel];
					CurrentPoint.m_aValues[Channel] = f2fx(Channels.r);
					vpActions.push_back(std::make_shared<CEditorActionEnvelopeEditPoint>(pMap, pMap->m_SelectedEnvelope, SelectedIndex, SelectedChannel, CEditorActionEnvelopeEditPoint::EEditType::VALUE, PrevValue, CurrentPoint.m_aValues[Channel]));
				}
			}
		}
	}

	if(!vpActions.empty())
	{
		pMap->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(pMap, vpActions, "Project points"));
	}

	pMap->OnModify();
	return CUi::POPUP_CLOSE_CURRENT;
}

CUi::EPopupMenuFunctionResult CEnvelopeEditor::CPopupEnvelopeCurveType::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupEnvelopeCurveType *pPopupEnvelopeCurveTypeContext = static_cast<CPopupEnvelopeCurveType *>(pContext);
	CEnvelopeEditor *pEnvelopeEditor = pPopupEnvelopeCurveTypeContext->m_pEnvelopeEditor;
	CEditorMap *pMap = pEnvelopeEditor->Map();
	CEditor *pEditor = pEnvelopeEditor->Editor();

	if(pMap->m_SelectedEnvelope < 0 || pMap->m_SelectedEnvelope >= (int)pMap->m_vpEnvelopes.size())
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}
	std::shared_ptr<CEnvelope> pEnvelope = pMap->m_vpEnvelopes[pMap->m_SelectedEnvelope];

	if(pPopupEnvelopeCurveTypeContext->m_SelectedPoint < 0 || pPopupEnvelopeCurveTypeContext->m_SelectedPoint >= (int)pEnvelope->m_vPoints.size())
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}
	CEnvPoint_runtime &SelectedPoint = pEnvelope->m_vPoints[pPopupEnvelopeCurveTypeContext->m_SelectedPoint];

	for(int Type = 0; Type < NUM_CURVETYPES; Type++)
	{
		CUIRect Button;
		View.HSplitTop(14.0f, &Button, &View);

		if(pEditor->DoButton_MenuItem(&pPopupEnvelopeCurveTypeContext->m_aButtonIds[Type], CURVE_TYPE_NAMES[Type], Type == SelectedPoint.m_Curvetype, &Button))
		{
			const int PrevCurve = SelectedPoint.m_Curvetype;
			if(PrevCurve != Type)
			{
				SelectedPoint.m_Curvetype = Type;
				pMap->m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeEditPoint>(
					pMap, pMap->m_SelectedEnvelope, pPopupEnvelopeCurveTypeContext->m_SelectedPoint, 0,
					CEditorActionEnvelopeEditPoint::EEditType::CURVE_TYPE, PrevCurve, SelectedPoint.m_Curvetype));
				pMap->OnModify();
				return CUi::POPUP_CLOSE_CURRENT;
			}
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}
