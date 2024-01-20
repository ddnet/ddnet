/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/graphics.h>
#include <engine/textrender.h>

#include "graph.h"

CGraph::CGraph(int MaxEntries) :
	m_Entries(MaxEntries * (sizeof(SEntry) + 2 * CRingBufferBase::ITEM_SIZE), CRingBufferBase::FLAG_RECYCLE)
{
}

void CGraph::Init(float Min, float Max)
{
	SetMin(Min);
	SetMax(Max);
}

void CGraph::SetMin(float Min)
{
	m_MinRange = m_Min = Min;
}

void CGraph::SetMax(float Max)
{
	m_MaxRange = m_Max = Max;
}

void CGraph::Scale(int64_t WantedTotalTime)
{
	// Scale X axis for wanted total time
	if(m_Entries.First() != nullptr)
	{
		const int64_t EndTime = m_Entries.Last()->m_Time;
		bool ScaleTotalTime = false;
		m_pFirstScaled = nullptr;

		if(m_Entries.First()->m_Time >= EndTime - WantedTotalTime)
		{
			m_pFirstScaled = m_Entries.First();
		}
		else
		{
			m_pFirstScaled = m_Entries.Last();
			while(m_pFirstScaled)
			{
				SEntry *pPrev = m_Entries.Prev(m_pFirstScaled);
				if(pPrev == nullptr)
					break;
				if(pPrev->m_Time < EndTime - WantedTotalTime)
				{
					// Scale based on actual total time instead of based on wanted total time,
					// to avoid flickering last segment due to rounding errors.
					ScaleTotalTime = true;
					break;
				}
				m_pFirstScaled = pPrev;
			}
		}

		m_RenderedTotalTime = ScaleTotalTime ? (EndTime - m_pFirstScaled->m_Time) : WantedTotalTime;

		// Ensure that color is applied to first line segment
		if(m_pFirstScaled)
		{
			m_pFirstScaled->m_ApplyColor = true;
			SEntry *pNext = m_Entries.Next(m_pFirstScaled);
			if(pNext != nullptr)
			{
				pNext->m_ApplyColor = true;
			}
		}
	}
	else
	{
		m_pFirstScaled = nullptr;
		m_RenderedTotalTime = 0;
	}

	// Scale Y axis
	m_Min = m_MinRange;
	m_Max = m_MaxRange;
	for(SEntry *pEntry = m_pFirstScaled; pEntry != nullptr; pEntry = m_Entries.Next(pEntry))
	{
		if(pEntry->m_Value > m_Max)
			m_Max = pEntry->m_Value;
		else if(pEntry->m_Value < m_Min)
			m_Min = pEntry->m_Value;
	}
}

void CGraph::Add(float Value, ColorRGBA Color)
{
	InsertAt(time_get(), Value, Color);
}

void CGraph::InsertAt(int64_t Time, float Value, ColorRGBA Color)
{
	SEntry *pEntry = m_Entries.Allocate(sizeof(SEntry));
	pEntry->m_Time = Time;
	pEntry->m_Value = Value;
	pEntry->m_Color = Color;

	// Determine whether the line (pPrev, pEntry) has different
	// vertex colors than the line (pPrevPrev, pPrev).
	SEntry *pPrev = m_Entries.Prev(pEntry);
	if(pPrev == nullptr)
	{
		pEntry->m_ApplyColor = true;
	}
	else
	{
		SEntry *pPrevPrev = m_Entries.Prev(pPrev);
		if(pPrevPrev == nullptr)
		{
			pEntry->m_ApplyColor = true;
		}
		else
		{
			pEntry->m_ApplyColor = Color != pPrev->m_Color || pPrev->m_Color != pPrevPrev->m_Color;
		}
	}
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

	if(m_pFirstScaled != nullptr)
	{
		IGraphics::CLineItem aValueLineItems[128];
		size_t NumValueLineItems = 0;

		const int64_t StartTime = m_pFirstScaled->m_Time;

		SEntry *pEntry0 = m_pFirstScaled;
		int a0 = round_to_int((pEntry0->m_Time - StartTime) * w / m_RenderedTotalTime);
		int v0 = round_to_int((pEntry0->m_Value - m_Min) * h / (m_Max - m_Min));
		while(pEntry0 != nullptr)
		{
			SEntry *pEntry1 = m_Entries.Next(pEntry0);
			if(pEntry1 == nullptr)
				break;

			const int a1 = round_to_int((pEntry1->m_Time - StartTime) * w / m_RenderedTotalTime);
			const int v1 = round_to_int((pEntry1->m_Value - m_Min) * h / (m_Max - m_Min));

			if(pEntry1->m_ApplyColor)
			{
				if(NumValueLineItems)
				{
					pGraphics->LinesDraw(aValueLineItems, NumValueLineItems);
					NumValueLineItems = 0;
				}

				IGraphics::CColorVertex aColorVertices[2] = {
					IGraphics::CColorVertex(0, pEntry0->m_Color.r, pEntry0->m_Color.g, pEntry0->m_Color.b, pEntry0->m_Color.a),
					IGraphics::CColorVertex(1, pEntry1->m_Color.r, pEntry1->m_Color.g, pEntry1->m_Color.b, pEntry1->m_Color.a)};
				pGraphics->SetColorVertex(aColorVertices, std::size(aColorVertices));
			}
			if(NumValueLineItems == std::size(aValueLineItems))
			{
				pGraphics->LinesDraw(aValueLineItems, NumValueLineItems);
				NumValueLineItems = 0;
			}
			aValueLineItems[NumValueLineItems] = IGraphics::CLineItem(x + a0, y + h - v0, x + a1, y + h - v1);
			++NumValueLineItems;

			pEntry0 = pEntry1;
			a0 = a1;
			v0 = v1;
		}
		if(NumValueLineItems)
		{
			pGraphics->LinesDraw(aValueLineItems, NumValueLineItems);
		}
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
