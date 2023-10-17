/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/graphics.h>
#include <engine/textrender.h>

#include "graph.h"

void CGraph::Init(float Min, float Max)
{
	SetMin(Min);
	SetMax(Max);
	m_Index = 0;
}

void CGraph::SetMin(float Min)
{
	m_MinRange = m_Min = Min;
}

void CGraph::SetMax(float Max)
{
	m_MaxRange = m_Max = Max;
}

void CGraph::Scale()
{
	m_Min = m_MinRange;
	m_Max = m_MaxRange;
	for(auto &Entry : m_aEntries)
	{
		if(Entry.m_Value > m_Max)
			m_Max = Entry.m_Value;
		else if(Entry.m_Value < m_Min)
			m_Min = Entry.m_Value;
	}
}

void CGraph::Add(float Value, ColorRGBA Color)
{
	InsertAt(m_Index, Value, Color);
	m_Index = (m_Index + 1) % MAX_VALUES;
}

void CGraph::InsertAt(size_t Index, float Value, ColorRGBA Color)
{
	dbg_assert(Index < MAX_VALUES, "Index out of bounds");
	m_aEntries[Index].m_Value = Value;
	m_aEntries[Index].m_Color = Color;
}

void CGraph::Render(IGraphics *pGraphics, ITextRender *pTextRender, float x, float y, float w, float h, const char *pDescription)
{
	pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(0.0f, 0.0f, 0.0f, 0.75f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();

	pGraphics->LinesBegin();
	pGraphics->SetColor(0.95f, 0.95f, 0.95f, 1.0f);
	IGraphics::CLineItem LineItem(x, y + h / 2, x + w, y + h / 2);
	pGraphics->LinesDraw(&LineItem, 1);
	pGraphics->SetColor(0.5f, 0.5f, 0.5f, 0.75f);
	IGraphics::CLineItem aLineItems[2] = {
		IGraphics::CLineItem(x, y + (h * 3) / 4, x + w, y + (h * 3) / 4),
		IGraphics::CLineItem(x, y + h / 4, x + w, y + h / 4)};
	pGraphics->LinesDraw(aLineItems, std::size(aLineItems));
	for(int i = 1; i < MAX_VALUES; i++)
	{
		float a0 = (i - 1) / (float)MAX_VALUES;
		float a1 = i / (float)MAX_VALUES;
		int i0 = (m_Index + i - 1) % MAX_VALUES;
		int i1 = (m_Index + i) % MAX_VALUES;

		float v0 = (m_aEntries[i0].m_Value - m_Min) / (m_Max - m_Min);
		float v1 = (m_aEntries[i1].m_Value - m_Min) / (m_Max - m_Min);

		IGraphics::CColorVertex aColorVertices[2] = {
			IGraphics::CColorVertex(0, m_aEntries[i0].m_Color.r, m_aEntries[i0].m_Color.g, m_aEntries[i0].m_Color.b, m_aEntries[i0].m_Color.a),
			IGraphics::CColorVertex(1, m_aEntries[i1].m_Color.r, m_aEntries[i1].m_Color.g, m_aEntries[i1].m_Color.b, m_aEntries[i1].m_Color.a)};
		pGraphics->SetColorVertex(aColorVertices, std::size(aColorVertices));
		IGraphics::CLineItem LineItem2(x + a0 * w, y + h - v0 * h, x + a1 * w, y + h - v1 * h);
		pGraphics->LinesDraw(&LineItem2, 1);
	}
	pGraphics->LinesEnd();

	const float FontSize = 12.0f;
	const float Spacing = 2.0f;

	pTextRender->Text(x + Spacing, y + h - FontSize - Spacing, FontSize, pDescription);

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.2f", m_Max);
	pTextRender->Text(x + w - pTextRender->TextWidth(FontSize, aBuf) - Spacing, y + Spacing, FontSize, aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_Min);
	pTextRender->Text(x + w - pTextRender->TextWidth(FontSize, aBuf) - Spacing, y + h - FontSize - Spacing, FontSize, aBuf);
}
