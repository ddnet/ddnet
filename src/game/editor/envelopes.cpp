#include "editor.h"
#include <game/client/ui_scrollregion.h>

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
