#include "base/system.h"
#include "editor.h"
#include "engine/graphics.h"
#include <game/client/ui_scrollregion.h>
#include <engine/keys.h>

int CEditor::PopupEnvelopeType(CEditor *pEditor, CUIRect View, void *pContext)
{
	const char* apEnvelopeTypes[3] = {"Position", "Color", "Sound"};

	for(int i = 0; i < 3; i++)
	{
		CUIRect Button;
		View.HSplitTop(14.0f, &Button, &View);

		if(pEditor->DoButton_MenuItem(&apEnvelopeTypes[i], apEnvelopeTypes[i], 0, &Button))
		{
			if(i == 0)
				pEditor->m_NewEnvelopeType = 3;
			else if(i == 1)
				pEditor->m_NewEnvelopeType = 4;
			else if(i == 2)
				pEditor->m_NewEnvelopeType = 1;

			return 1;
		}
	}
	return 0;
}

int CEditor::PopupEnvelope(CEditor *pEditor, CUIRect View, void *pContext)
{
    const float RowHeight = 12.0f;
    CUIRect Button;

    View.HSplitTop(12.0f, &Button, &View);
    pEditor->UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_LEFT);
    Button.VSplitLeft(40.0f, nullptr, &Button);
    static float s_NameEditBox = 0;
    char *pName = pEditor->m_Map.m_vpEnvelopes[pEditor->m_SelectedEnvelope]->m_aName;
    if(pEditor->DoEditBox(&s_NameEditBox, &Button, pName, sizeof(pName), 10.0f, &s_NameEditBox))
        pEditor->m_Map.m_Modified = true;

    View.HMargin(5.0f, &View);

    CProperty aProps[] = {
        {"Synchronised", pEditor->m_Map.m_vpEnvelopes[pEditor->m_SelectedEnvelope]->m_Synchronized, PROPTYPE_BOOL, 0, 1},
        {nullptr}
    };
    static int s_aPropIds[1] = {0};

    int NewVal = 0;
    int Prop = pEditor->DoProperties(&View, aProps, s_aPropIds, &NewVal);
    if(Prop == 0)
    {
        pEditor->m_Map.m_vpEnvelopes[pEditor->m_SelectedEnvelope]->m_Synchronized = NewVal;
        pEditor->m_Map.m_Modified = true;
    }

    View.HMargin(5.0f, &View);

    static int s_DeleteButton = 0;
	View.HSplitTop(RowHeight, &Button, &View);
    if(pEditor->DoButton_Editor(&s_DeleteButton, "Delete envelope", 0, &Button, 0, "Delete envelope"))
    {
        pEditor->m_Map.DeleteEnvelope(pEditor->m_SelectedEnvelope);
        pEditor->m_SelectedEnvelope = maximum(0, pEditor->m_SelectedEnvelope - 1);
        return 1;
    }

    return 0;
}

void CEditor::RenderEnvelopeSelector(CUIRect View)
{
    enum {
        OP_NONE,
        OP_CLICK_HEADER,
        OP_CLICK_ENVELOPE,
        OP_DRAG_HEADER,
        OP_DRAG_ENVELOPE
    };
    static int s_Operation = OP_NONE;
    static const void *s_pDraggedButton = 0;
    static int s_InitialMouseY = 0.0f;
    static float s_InitialMouseOffsetY = 0.0f;
    static float s_InitialCutHeight = 0.0f;
    bool StartDragEnvelope = false;
    bool MoveEnvelope = false;
    bool DraggedPositionFound = false;
    int EnvelopeAfterDragPosition = 0;

    if(!UI()->CheckActiveItem(s_pDraggedButton))
		s_Operation = OP_NONE;

    const float StatusBarHeight = 16.0f;
    const float RowHeight = 12.0f;
    const float MarginLayersHeader = 10.0f;
    View.HMargin(MarginLayersHeader, &View);

    CUIRect HeaderButton;
    View.HSplitTop(RowHeight, &HeaderButton, &View);

    static int s_HeaderButton;
    bool Clicked;
    bool Abrupted;
    if (int Result = DoButton_DraggableEx(&s_HeaderButton, "Envelopes", 0, &HeaderButton, &Clicked, &Abrupted, 0, "some tooltip", IGraphics::CORNER_T))
    {
        if(s_Operation == OP_NONE && Result == 1)
        {
            s_InitialMouseY = UI()->MouseY();
            s_InitialMouseOffsetY = UI()->MouseY() - HeaderButton.y;
            s_Operation = OP_CLICK_HEADER;
        }

        if(Abrupted)
            s_Operation = OP_NONE;

        if(s_Operation == OP_CLICK_HEADER)
        {
            if(absolute(UI()->MouseY() - s_InitialMouseY) > 5)
                s_Operation = OP_DRAG_HEADER;
            
            s_pDraggedButton = &s_HeaderButton;
        }

        if(s_Operation == OP_CLICK_HEADER && Clicked)
        {
            s_Operation = OP_NONE;
            if(m_EnvelopeSelectorSplit > 38.0f)
            {
                m_LastEnvelopeSelectorSplit = m_EnvelopeSelectorSplit;
                m_EnvelopeSelectorSplit = 38.0f;
            }
            else
                m_EnvelopeSelectorSplit = m_LastEnvelopeSelectorSplit;
        }

        if(s_Operation == OP_DRAG_HEADER)
        {
            if(Clicked)
                s_Operation = OP_NONE;

            // TODO: why +8!?
            m_EnvelopeSelectorSplit = s_InitialMouseOffsetY + View.y + View.h + HeaderButton.h + 8.0f - UI()->MouseY();
            m_EnvelopeSelectorSplit = clamp(m_EnvelopeSelectorSplit, StatusBarHeight + HeaderButton.h + MarginLayersHeader, 400.0f);
        }
    }

    const float MarginHeaderEnvelopes = 4.0f;
    View.HMargin(MarginHeaderEnvelopes, &View);

    if(m_EnvelopeSelectorSplit < StatusBarHeight + HeaderButton.h + MarginLayersHeader + RowHeight)
        return;

    CUIRect UnscrolledView = View;

    static CScrollRegion s_ScrollRegion;
    vec2 ScrollOffset(0.0f, 0.0f);

    CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5.0f;
    ScrollParams.m_SliderMinHeight = 5.0f;
	s_ScrollRegion.Begin(&View, &ScrollOffset, &ScrollParams);
	View.y += ScrollOffset.y;

    if(s_Operation == OP_DRAG_ENVELOPE)
	{
		float MinDraggableValue = UnscrolledView.y;
		float MaxDraggableValue = MinDraggableValue + (m_Map.m_vpEnvelopes.size() - 1) * (RowHeight + 2.0f) + ScrollOffset.y;

		UnscrolledView.HSplitTop(s_InitialCutHeight, nullptr, &UnscrolledView);
		UnscrolledView.y -= s_InitialMouseY - UI()->MouseY();

		UnscrolledView.y = clamp(UnscrolledView.y, MinDraggableValue, MaxDraggableValue);

		UnscrolledView.w = View.w;
	}

    for(int i = 0; i < (int)m_Map.m_vpEnvelopes.size(); i++)
    {

        CUIRect Slot;
        if(s_Operation == OP_DRAG_ENVELOPE)
        {
            if(i == m_SelectedEnvelope)
            {
                UnscrolledView.HSplitTop(RowHeight, &Slot, nullptr);
            }
            else if(!DraggedPositionFound && UI()->MouseY() < View.y + RowHeight / 2)
            {
                DraggedPositionFound = true;
                EnvelopeAfterDragPosition = i;

                View.HSplitTop(RowHeight + 2.0f, &Slot, &View);
                s_ScrollRegion.AddRect(Slot);
            }
        }
        if(s_Operation != OP_DRAG_ENVELOPE || i != m_SelectedEnvelope)
		{
			View.HSplitTop(RowHeight, &Slot, &View);

			CUIRect TmpRect;
			View.HSplitTop(2.0f, &TmpRect, &View);
			s_ScrollRegion.AddRect(TmpRect);
		}

        if(s_ScrollRegion.AddRect(Slot))
        {
            CEnvelope *pEnvelope = m_Map.m_vpEnvelopes[i];
            char aButtonLabel[64];
            str_format(aButtonLabel, sizeof(aButtonLabel), "#%d %s", i + 1, pEnvelope->m_aName);
            
            int Checked = i == m_SelectedEnvelope ? 7 : 6;
            if(int Result = DoButton_DraggableEx(&m_Map.m_vpEnvelopes[i], aButtonLabel, Checked, &Slot, &Clicked, &Abrupted, 1, "some tooltip"))
            {
                if(s_Operation == OP_CLICK_HEADER || s_Operation == OP_DRAG_HEADER)
                    continue;

                if(s_Operation == OP_NONE)
				{
                    s_InitialMouseY = UI()->MouseY();
                    s_InitialCutHeight = s_InitialMouseY - UnscrolledView.y;
                    s_Operation = OP_CLICK_ENVELOPE;

                    m_SelectedEnvelope = i;
                    m_ShowEnvelopeEditor = true;
                }

                if(Abrupted)
                    s_Operation = OP_NONE;

                if(s_Operation == OP_CLICK_ENVELOPE)
                {
                    if(absolute(UI()->MouseY() - s_InitialMouseY) > 5)
                        StartDragEnvelope = true;

                    s_pDraggedButton = &m_Map.m_vpEnvelopes[i];
                }

                if(s_Operation == OP_CLICK_ENVELOPE && Clicked)
                {
                    static int s_EnvelopePopupId = 0;
                    if(Result == 2)
                        UiInvokePopupMenu(&s_EnvelopePopupId, 0, UI()->MouseX(), UI()->MouseY(), 145, 15.0f + 14.0f * 3, PopupEnvelope);
                }

                if(s_Operation == OP_DRAG_ENVELOPE && Clicked)
                {
                    MoveEnvelope = true;
                }
            }
        }
    }

    if(!DraggedPositionFound && s_Operation == OP_DRAG_ENVELOPE)
	{
        EnvelopeAfterDragPosition = m_Map.m_vpEnvelopes.size();

		CUIRect TmpSlot;
		View.HSplitTop(RowHeight + 2.0f, &TmpSlot, &View);
		s_ScrollRegion.AddRect(TmpSlot);
	}

    if(MoveEnvelope && 0 <= EnvelopeAfterDragPosition && EnvelopeAfterDragPosition <= (int)m_Map.m_vpEnvelopes.size())
	{
        dbg_msg("editor", "Moving to %d", EnvelopeAfterDragPosition);
		CEnvelope *pSelectedEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
		CEnvelope *pNextEnvelope = nullptr;
		if(EnvelopeAfterDragPosition < (int)m_Map.m_vpEnvelopes.size())
			pNextEnvelope = m_Map.m_vpEnvelopes[EnvelopeAfterDragPosition];

		m_Map.m_vpEnvelopes.erase(m_Map.m_vpEnvelopes.begin() + m_SelectedEnvelope);

		auto InsertPosition = std::find(m_Map.m_vpEnvelopes.begin(), m_Map.m_vpEnvelopes.end(), pNextEnvelope);
		m_Map.m_vpEnvelopes.insert(InsertPosition, pSelectedEnvelope);

		m_SelectedEnvelope = InsertPosition - m_Map.m_vpEnvelopes.begin();
		m_Map.m_Modified = true;
	}

    if(MoveEnvelope)
        s_Operation = OP_NONE;
    if(StartDragEnvelope)
        s_Operation = OP_DRAG_ENVELOPE;

    if(s_Operation == OP_DRAG_ENVELOPE)
    {
        s_ScrollRegion.DoEdgeScrolling();
        UI()->SetActiveItem(s_pDraggedButton);
    }

    // TODO: maybe add up/down keys

    CUIRect AddEnvelopeButton;
	View.HSplitTop(RowHeight + 1.0f, &AddEnvelopeButton, &View);
	if(s_ScrollRegion.AddRect(AddEnvelopeButton))
	{
		AddEnvelopeButton.HSplitTop(RowHeight, &AddEnvelopeButton, 0);
		
        static int s_AddEnvelopeButton = 0;
		if(DoButton_Editor(&s_AddEnvelopeButton, "Add envelope", 0, &AddEnvelopeButton, IGraphics::CORNER_R, "Adds a new envelope"))
		{
            static int s_PopupSelectEnvelopeType = 0;
            UiInvokePopupMenu(&s_PopupSelectEnvelopeType, 0, AddEnvelopeButton.x, AddEnvelopeButton.y + AddEnvelopeButton.h, View.w, 10.0f + 14.0f * 3, PopupEnvelopeType);
		}

        if(m_NewEnvelopeType > 0)
        {
            CEnvelope *pNewEnv = m_Map.NewEnvelope(m_NewEnvelopeType);
            m_SelectedEnvelope = m_Map.m_vpEnvelopes.size() - 1;
            m_Map.m_Modified = true;
            m_NewEnvelopeType = -1;
            m_ShowEnvelopeEditor = true;

            if(pNewEnv->m_Channels == 4)
            {
                pNewEnv->AddPoint(0, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
				pNewEnv->AddPoint(1000, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
            }
            else
            {
                pNewEnv->AddPoint(0, 0);
				pNewEnv->AddPoint(1000, 0);
            }
        }
	}


    s_ScrollRegion.End();
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	CEnvelope *pEnvelope = nullptr;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < (int)m_Map.m_vpEnvelopes.size())
		pEnvelope = m_Map.m_vpEnvelopes[m_SelectedEnvelope];
	dbg_assert(pEnvelope != nullptr, "");

	CUIRect DragBar = {
		View.x,
		View.y - 6.0f,
		View.w,
		22.0f
	};
	CUIRect ToolBar, ColorBar;
    View.HSplitTop(15.0f, &ToolBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->m_Channels == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
		RenderBackground(ColorBar, m_CheckerTexture, 16.0f, 1.0f);
	}

	enum {
		OP_NONE,
		OP_DRAG_HEADER,
		OP_CLICK_HEADER
	};
	static int s_Operation = 0;
	static float s_InitialMouseY = 0.0f;
	static float s_InitialMouseOffsetY = 0.0f;

	static int s_DragBar = 0;
	bool Clicked;
	bool Abrupted;
	if (int Result = DoButton_DraggableEx(&s_DragBar, "", 8, &DragBar, &Clicked, &Abrupted, 0, "Change the size of the editor by dragging."))
    {
        if(s_Operation == OP_NONE && Result == 1)
        {
            s_InitialMouseY = UI()->MouseY();
            s_InitialMouseOffsetY = UI()->MouseY() - DragBar.y;
            s_Operation = OP_CLICK_HEADER;
        }

        if(Abrupted)
            s_Operation = OP_NONE;

        if(s_Operation == OP_CLICK_HEADER && absolute(UI()->MouseY() - s_InitialMouseY) > 5)
			s_Operation = OP_DRAG_HEADER;

        if(s_Operation == OP_DRAG_HEADER)
        {
            if(Clicked)
                s_Operation = OP_NONE;

            // TODO: why +8!?
            m_EnvelopeEditorSplit = s_InitialMouseOffsetY + View.y + View.h - UI()->MouseY();
            m_EnvelopeEditorSplit = clamp(m_EnvelopeEditorSplit, 100.0f, 400.0f);
        }
    }

	// close button
	CUIRect Button;
	ToolBar.VSplitRight(15.0f, &ToolBar, &Button);
	static int s_CloseButton = 0;
	if(DoButton_Editor(&s_CloseButton, "✗", 0, &Button, 0, "Close envelope editor"))
	{
		m_ShowEnvelopeEditor = false;
		m_SelectedEnvelope = -1;
	}

	static std::vector<int> s_vSelection;
	static int s_EnvelopeEditorID = 0;
	static int s_ActiveChannels = 0xf;

	ColorRGBA aColors[] = {ColorRGBA(1, 0.2f, 0.2f), ColorRGBA(0.2f, 1, 0.2f), ColorRGBA(0.2f, 0.2f, 1), ColorRGBA(1, 1, 0.2f)};

	ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

	static const char *s_aapNames[4][CEnvPoint::MAX_CHANNELS] = {
		{"V", "", "", ""},
		{"", "", "", ""},
		{"X", "Y", "R", ""},
		{"R", "G", "B", "A"},
	};

	static const char *s_aapDescriptions[4][CEnvPoint::MAX_CHANNELS] = {
		{"Volume of the envelope", "", "", ""},
		{"", "", "", ""},
		{"X-axis of the envelope", "Y-axis of the envelope", "Rotation of the envelope", ""},
		{"Red value of the envelope", "Green value of the envelope", "Blue value of the envelope", "Alpha value of the envelope"},
	};

	static int s_aChannelButtons[CEnvPoint::MAX_CHANNELS] = {0};
	int Bit = 1;

	for(int i = 0; i < CEnvPoint::MAX_CHANNELS; i++, Bit <<= 1)
	{
		ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
		if(i < pEnvelope->m_Channels)
		{
			int Corners = IGraphics::CORNER_NONE;
			if(i == 0)
				Corners = IGraphics::CORNER_L;
			else if(i == pEnvelope->m_Channels - 1)
				Corners = IGraphics::CORNER_R;
			if(pEnvelope->m_Channels == 1)
				Corners = IGraphics::CORNER_ALL;

			if(DoButton_Env(&s_aChannelButtons[i], s_aapNames[pEnvelope->m_Channels - 1][i], s_ActiveChannels & Bit, &Button, s_aapDescriptions[pEnvelope->m_Channels - 1][i], aColors[i], Corners))
				s_ActiveChannels ^= Bit;
		}
	}

	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	float EndTime = pEnvelope->EndTime();
	if(EndTime < 1)
		EndTime = 1;

	pEnvelope->FindTopBottom(s_ActiveChannels);
	float Top = pEnvelope->m_Top;
	float Bottom = pEnvelope->m_Bottom;

	if(Top < 1)
		Top = 1;
	if(Bottom >= 0)
		Bottom = 0;

	float TimeScale = EndTime / View.w;
	float ValueScale = (Top - Bottom) / View.h;

	if(UI()->MouseInside(&View))
		UI()->SetHotItem(&s_EnvelopeEditorID);

	if(UI()->HotItem() == &s_EnvelopeEditorID)
	{
		// do stuff
		if(UI()->MouseButtonClicked(1))
		{
			// add point
			int Time = (int)(((UI()->MouseX() - View.x) * TimeScale) * 1000.0f);
			ColorRGBA Channels;
			pEnvelope->Eval(Time / 1000.0f, Channels);
			pEnvelope->AddPoint(Time,
				f2fx(Channels.r), f2fx(Channels.g),
				f2fx(Channels.b), f2fx(Channels.a));
			m_Map.m_Modified = true;
		}

		m_ShowEnvelopePreview = SHOWENV_SELECTED;
		m_pTooltip = "Press right mouse button to create a new point";
	}

	// render lines
	{
		UI()->ClipEnable(&View);
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		for(int c = 0; c < pEnvelope->m_Channels; c++)
		{
			if(s_ActiveChannels & (1 << c))
				Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
			else
				Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

			float PrevX = 0;
			ColorRGBA Channels;
			pEnvelope->Eval(0.000001f, Channels);
			float PrevValue = Channels[c];

			int Steps = (int)((View.w / UI()->Screen()->w) * Graphics()->ScreenWidth());
			for(int i = 1; i <= Steps; i++)
			{
				float a = i / (float)Steps;
				pEnvelope->Eval(a * EndTime, Channels);
				float v = Channels[c];
				v = (v - Bottom) / (Top - Bottom);

				IGraphics::CLineItem LineItem(View.x + PrevX * View.w, View.y + View.h - PrevValue * View.h, View.x + a * View.w, View.y + View.h - v * View.h);
				Graphics()->LinesDraw(&LineItem, 1);
				PrevX = a;
				PrevValue = v;
			}
		}
		Graphics()->LinesEnd();
		UI()->ClipDisable();
	}

	// // render curve options
	// {
	// 	for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
	// 	{
	// 		float t0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
	// 		float t1 = pEnvelope->m_vPoints[i + 1].m_Time / 1000.0f / EndTime;

	// 		CUIRect v;
	// 		v.x = CurveBar.x + (t0 + (t1 - t0) * 0.5f) * CurveBar.w;
	// 		v.y = CurveBar.y;
	// 		v.h = CurveBar.h;
	// 		v.w = CurveBar.h;
	// 		v.x -= v.w / 2;
	// 		void *pID = &pEnvelope->m_vPoints[i].m_Curvetype;
	// 		const char *apTypeName[] = {
	// 			"N", "L", "S", "F", "M"};
	// 		const char *pTypeName = "Invalid";
	// 		if(0 <= pEnvelope->m_vPoints[i].m_Curvetype && pEnvelope->m_vPoints[i].m_Curvetype < (int)std::size(apTypeName))
	// 			pTypeName = apTypeName[pEnvelope->m_vPoints[i].m_Curvetype];
	// 		if(DoButton_Editor(pID, pTypeName, 0, &v, 0, "Switch curve type"))
	// 			pEnvelope->m_vPoints[i].m_Curvetype = (pEnvelope->m_vPoints[i].m_Curvetype + 1) % NUM_CURVETYPES;
	// 	}
	// }

	// render colorbar
	if(ShowColorBar)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int i = 0; i < (int)pEnvelope->m_vPoints.size() - 1; i++)
		{
			float r0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[0]);
			float g0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[1]);
			float b0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[2]);
			float a0 = fx2f(pEnvelope->m_vPoints[i].m_aValues[3]);
			float r1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[0]);
			float g1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[1]);
			float b1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[2]);
			float a1 = fx2f(pEnvelope->m_vPoints[i + 1].m_aValues[3]);

			IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, r0, g0, b0, a0),
				IGraphics::CColorVertex(1, r1, g1, b1, a1),
				IGraphics::CColorVertex(2, r1, g1, b1, a1),
				IGraphics::CColorVertex(3, r0, g0, b0, a0)};
			Graphics()->SetColorVertex(Array, 4);

			float x0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
			float x1 = pEnvelope->m_vPoints[i + 1].m_Time / 1000.0f / EndTime;

			IGraphics::CQuadItem QuadItem(ColorBar.x + x0 * ColorBar.w, ColorBar.y, (x1 - x0) * ColorBar.w, ColorBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
		}
		Graphics()->QuadsEnd();
	}

	// render handles

	// keep track of last Env
	static void *s_pID = nullptr;

	// chars for textinput
	static char s_aStrCurTime[32] = "0.000";
	static char s_aStrCurValue[32] = "0.000";

	// bool CurrentEnvelopeSwitched = false;
	// if(CurrentEnvelopeSwitched)
	// {
	// 	s_pID = nullptr;

	// 	// update displayed text
	// 	str_copy(s_aStrCurTime, "0.000");
	// 	str_copy(s_aStrCurValue, "0.000");
	// }

	{
		int CurrentValue = 0, CurrentTime = 0;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int c = 0; c < pEnvelope->m_Channels; c++)
		{
			if(!(s_ActiveChannels & (1 << c)))
				continue;

			for(size_t i = 0; i < pEnvelope->m_vPoints.size(); i++)
			{
				float x0 = pEnvelope->m_vPoints[i].m_Time / 1000.0f / EndTime;
				float y0 = (fx2f(pEnvelope->m_vPoints[i].m_aValues[c]) - Bottom) / (Top - Bottom);
				CUIRect Final;
				Final.x = View.x + x0 * View.w;
				Final.y = View.y + View.h - y0 * View.h;
				Final.x -= 2.0f;
				Final.y -= 2.0f;
				Final.w = 4.0f;
				Final.h = 4.0f;

				void *pID = &pEnvelope->m_vPoints[i].m_aValues[c];

				if(UI()->MouseInside(&Final))
					UI()->SetHotItem(pID);

				float ColorMod = 1.0f;

				if(UI()->CheckActiveItem(pID))
				{
					if(!UI()->MouseButton(0))
					{
						m_SelectedQuadEnvelope = -1;
						m_SelectedEnvelopePoint = -1;

						UI()->SetActiveItem(nullptr);
					}
					else
					{
						if(Input()->ShiftIsPressed())
						{
							if(i != 0)
							{
								if(Input()->ModifierIsPressed())
									pEnvelope->m_vPoints[i].m_Time += (int)((m_MouseDeltaX));
								else
									pEnvelope->m_vPoints[i].m_Time += (int)((m_MouseDeltaX * TimeScale) * 1000.0f);
								if(pEnvelope->m_vPoints[i].m_Time < pEnvelope->m_vPoints[i - 1].m_Time)
									pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i - 1].m_Time + 1;
								if(i + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[i].m_Time > pEnvelope->m_vPoints[i + 1].m_Time)
									pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i + 1].m_Time - 1;
							}
						}
						else
						{
							if(Input()->ModifierIsPressed())
								pEnvelope->m_vPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * 0.001f);
							else
								pEnvelope->m_vPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * ValueScale);
						}

						m_SelectedQuadEnvelope = m_SelectedEnvelope;
						m_ShowEnvelopePreview = SHOWENV_SELECTED;
						m_SelectedEnvelopePoint = i;
						m_Map.m_Modified = true;
					}

					ColorMod = 100.0f;
					Graphics()->SetColor(1, 1, 1, 1);
				}
				else if(UI()->HotItem() == pID)
				{
					if(UI()->MouseButton(0))
					{
						s_vSelection.clear();
						s_vSelection.push_back(i);
						UI()->SetActiveItem(pID);
						// track it
						s_pID = pID;
					}

					// remove point
					if(UI()->MouseButtonClicked(1))
					{
						if(s_pID == pID)
						{
							s_pID = nullptr;

							// update displayed text
							str_copy(s_aStrCurTime, "0.000");
							str_copy(s_aStrCurValue, "0.000");
						}

						pEnvelope->m_vPoints.erase(pEnvelope->m_vPoints.begin() + i);
						m_Map.m_Modified = true;
					}

					m_ShowEnvelopePreview = SHOWENV_SELECTED;
					ColorMod = 100.0f;
					Graphics()->SetColor(1, 0.75f, 0.75f, 1);
					m_pTooltip = "Left mouse to drag. Hold ctrl to be more precise. Hold shift to alter time point as well. Right click to delete.";
				}

				if(pID == s_pID && (Input()->KeyIsPressed(KEY_RETURN) || Input()->KeyIsPressed(KEY_KP_ENTER)))
				{
					if(i != 0)
					{
						pEnvelope->m_vPoints[i].m_Time = str_tofloat(s_aStrCurTime) * 1000.0f;

						if(pEnvelope->m_vPoints[i].m_Time < pEnvelope->m_vPoints[i - 1].m_Time)
							pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i - 1].m_Time + 1;
						if(i + 1 != pEnvelope->m_vPoints.size() && pEnvelope->m_vPoints[i].m_Time > pEnvelope->m_vPoints[i + 1].m_Time)
							pEnvelope->m_vPoints[i].m_Time = pEnvelope->m_vPoints[i + 1].m_Time - 1;
					}
					else
						pEnvelope->m_vPoints[i].m_Time = 0.0f;

					str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", pEnvelope->m_vPoints[i].m_Time / 1000.0f);

					pEnvelope->m_vPoints[i].m_aValues[c] = f2fx(str_tofloat(s_aStrCurValue));
				}

				if(UI()->CheckActiveItem(pID))
				{
					CurrentTime = pEnvelope->m_vPoints[i].m_Time;
					CurrentValue = pEnvelope->m_vPoints[i].m_aValues[c];

					// update displayed text
					str_format(s_aStrCurTime, sizeof(s_aStrCurTime), "%.3f", CurrentTime / 1000.0f);
					str_format(s_aStrCurValue, sizeof(s_aStrCurValue), "%.3f", fx2f(CurrentValue));
				}

				if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == (int)i)
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				else
					Graphics()->SetColor(aColors[c].r * ColorMod, aColors[c].g * ColorMod, aColors[c].b * ColorMod, 1.0f);
				IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
		}
		Graphics()->QuadsEnd();

		CUIRect ToolBar1;
		CUIRect ToolBar2;
		ToolBar.VSplitMid(&ToolBar1, &ToolBar2);
		ToolBar1.VSplitRight(3.0f, &ToolBar1, nullptr);
		ToolBar2.VSplitRight(3.0f, &ToolBar2, nullptr);
		if(ToolBar.w > ToolBar.h * 21)
		{
			CUIRect Label1;
			CUIRect Label2;

			ToolBar1.VSplitMid(&Label1, &ToolBar1);
			ToolBar2.VSplitMid(&Label2, &ToolBar2);
			Label1.VSplitRight(3.0f, &Label1, nullptr);
			Label2.VSplitRight(3.0f, &Label2, nullptr);

			UI()->DoLabel(&Label1, "Value:", 10.0f, TEXTALIGN_RIGHT);
			UI()->DoLabel(&Label2, "Time (in s):", 10.0f, TEXTALIGN_RIGHT);
		}

		static float s_ValNumber = 0;
		DoEditBox(&s_ValNumber, &ToolBar1, s_aStrCurValue, sizeof(s_aStrCurValue), 10.0f, &s_ValNumber, false, IGraphics::CORNER_ALL, "The value of the selected envelope point");
		static float s_TimeNumber = 0;
		DoEditBox(&s_TimeNumber, &ToolBar2, s_aStrCurTime, sizeof(s_aStrCurTime), 10.0f, &s_TimeNumber, false, IGraphics::CORNER_ALL, "The time of the selected envelope point");
	}
}
