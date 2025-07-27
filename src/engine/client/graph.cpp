/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "graph.h"

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <limits>

CGraph::CGraph(int MaxEntries, int Precision, bool SummaryStats) :
	m_Entries(MaxEntries * (sizeof(CEntry) + 2 * CRingBufferBase::ITEM_SIZE), CRingBufferBase::FLAG_RECYCLE),
	m_Precision(Precision),
	m_SummaryStats(SummaryStats)
{
}

void CGraph::Init(float Min, float Max)
{
	m_Entries.Clear();
	m_pFirstScaled = nullptr;
	m_RenderedTotalTime = 0;
	m_Average = 0.0f;
	SetMin(Min);
	SetMax(Max);
}

void CGraph::SetMin(float Min)
{
	m_MinRange = m_MinValue = m_MinAxis = Min;
}

void CGraph::SetMax(float Max)
{
	m_MaxRange = m_MaxValue = m_MaxAxis = Max;
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
				CEntry *pPrev = m_Entries.Prev(m_pFirstScaled);
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
			CEntry *pNext = m_Entries.Next(m_pFirstScaled);
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
	m_Average = 0.0f;
	size_t NumValues = 0;
	m_MinAxis = m_MinRange;
	m_MaxAxis = m_MaxRange;
	m_MinValue = std::numeric_limits<float>::max();
	m_MaxValue = std::numeric_limits<float>::min();
	for(CEntry *pEntry = m_pFirstScaled; pEntry != nullptr; pEntry = m_Entries.Next(pEntry))
	{
		const float Value = pEntry->m_Value;
		m_Average += Value;
		++NumValues;

		if(Value > m_MaxAxis)
		{
			m_MaxAxis = Value;
		}
		else if(Value < m_MinAxis)
		{
			m_MinAxis = Value;
		}

		if(Value > m_MaxValue)
		{
			m_MaxValue = Value;
		}
		else if(Value < m_MinValue)
		{
			m_MinValue = Value;
		}
	}
	if(m_MaxValue < m_MinValue)
	{
		m_MinValue = m_MaxValue = 0.0f;
	}
	if(NumValues)
	{
		m_Average /= NumValues;
	}
}

void CGraph::Add(float Value, ColorRGBA Color)
{
	InsertAt(time_get(), Value, Color);
}

void CGraph::InsertAt(int64_t Time, float Value, ColorRGBA Color)
{
	CEntry *pEntry = m_Entries.Allocate(sizeof(CEntry));
	pEntry->m_Time = Time;
	pEntry->m_Value = Value;
	pEntry->m_Color = Color;

	// Determine whether the line (pPrev, pEntry) has different
	// vertex colors than the line (pPrevPrev, pPrev).
	CEntry *pPrev = m_Entries.Prev(pEntry);
	if(pPrev == nullptr)
	{
		pEntry->m_ApplyColor = true;
	}
	else
	{
		CEntry *pPrevPrev = m_Entries.Prev(pPrev);
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

void CGraph::RenderDataLines(IGraphics *pGraphics, float x, float y, float w, float h)
{
	if(m_pFirstScaled == nullptr)
	{
		return;
	}

	IGraphics::CLineItemBatch LineItemBatch;
	pGraphics->LinesBatchBegin(&LineItemBatch);

	const int64_t StartTime = m_pFirstScaled->m_Time;

	CEntry *pEntry0 = m_pFirstScaled;
	int a0 = round_to_int((pEntry0->m_Time - StartTime) * w / m_RenderedTotalTime);
	int v0 = round_to_int((pEntry0->m_Value - m_MinAxis) * h / (m_MaxAxis - m_MinAxis));
	while(pEntry0 != nullptr)
	{
		CEntry *pEntry1 = m_Entries.Next(pEntry0);
		if(pEntry1 == nullptr)
			break;

		const int a1 = round_to_int((pEntry1->m_Time - StartTime) * w / m_RenderedTotalTime);
		const int v1 = round_to_int((pEntry1->m_Value - m_MinAxis) * h / (m_MaxAxis - m_MinAxis));

		if(pEntry1->m_ApplyColor)
		{
			pGraphics->LinesBatchEnd(&LineItemBatch);
			pGraphics->LinesBatchBegin(&LineItemBatch);

			const IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, pEntry0->m_Color.r, pEntry0->m_Color.g, pEntry0->m_Color.b, pEntry0->m_Color.a),
				IGraphics::CColorVertex(1, pEntry1->m_Color.r, pEntry1->m_Color.g, pEntry1->m_Color.b, pEntry1->m_Color.a)};
			pGraphics->SetColorVertex(aColorVertices, std::size(aColorVertices));
		}
		const IGraphics::CLineItem Item = IGraphics::CLineItem(
			x + a0,
			y + h - v0,
			x + a1,
			y + h - v1);
		pGraphics->LinesBatchDraw(&LineItemBatch, &Item, 1);

		pEntry0 = pEntry1;
		a0 = a1;
		v0 = v1;
	}
	pGraphics->LinesBatchEnd(&LineItemBatch);
}

void CGraph::Render(IGraphics *pGraphics, ITextRender *pTextRender, float x, float y, float w, float h, const char *pDescription)
{
	const float FontSize = 12.0f;
	const float Spacing = 2.0f;
	const float HeaderHeight = FontSize + 2.0f * Spacing;

	pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.7f));
	const IGraphics::CQuadItem ItemHeader(x, y, w, HeaderHeight);
	pGraphics->QuadsDrawTL(&ItemHeader, 1);
	const IGraphics::CQuadItem ItemFooter(x, y + h - HeaderHeight, w, HeaderHeight);
	pGraphics->QuadsDrawTL(&ItemFooter, 1);
	pGraphics->SetColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.6f));
	const IGraphics::CQuadItem ItemGraph(x, y + HeaderHeight, w, h - 2.0f * HeaderHeight);
	pGraphics->QuadsDrawTL(&ItemGraph, 1);
	pGraphics->QuadsEnd();

	pGraphics->LinesBegin();
	pGraphics->SetColor(ColorRGBA(0.5f, 0.5f, 0.5f, 0.75f));
	const IGraphics::CLineItem aLineItems[] = {
		IGraphics::CLineItem(ItemGraph.m_X, ItemGraph.m_Y + (ItemGraph.m_Height * 3) / 4, ItemGraph.m_X + ItemGraph.m_Width, ItemGraph.m_Y + (ItemGraph.m_Height * 3) / 4),
		IGraphics::CLineItem(ItemGraph.m_X, ItemGraph.m_Y + ItemGraph.m_Height / 2, ItemGraph.m_X + ItemGraph.m_Width, ItemGraph.m_Y + ItemGraph.m_Height / 2),
		IGraphics::CLineItem(ItemGraph.m_X, ItemGraph.m_Y + ItemGraph.m_Height / 4, ItemGraph.m_X + ItemGraph.m_Width, ItemGraph.m_Y + ItemGraph.m_Height / 4)};
	pGraphics->LinesDraw(aLineItems, std::size(aLineItems));
	pGraphics->LinesEnd();

	RenderDataLines(pGraphics, ItemGraph.m_X, ItemGraph.m_Y + 1.0f, ItemGraph.m_Width, ItemGraph.m_Height + 2.0f);

	char aBuf[128];

	pTextRender->Text(ItemHeader.m_X + Spacing, ItemHeader.m_Y + Spacing, FontSize, pDescription);

	if(m_SummaryStats)
	{
		str_format(aBuf, sizeof(aBuf), "Avg. %.*f   ↓ %.*f   ↑ %.*f", m_Precision, m_Average, m_Precision, m_MinValue, m_Precision, m_MaxValue);
		pTextRender->Text(ItemFooter.m_X + Spacing, ItemFooter.m_Y + ItemFooter.m_Height - FontSize - Spacing, FontSize, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "%.*f", m_Precision, m_MaxAxis);
	pTextRender->Text(ItemHeader.m_X + ItemHeader.m_Width - pTextRender->TextWidth(FontSize, aBuf) - Spacing, ItemHeader.m_Y + Spacing, FontSize, aBuf);

	str_format(aBuf, sizeof(aBuf), "%.*f", m_Precision, m_MinAxis);
	pTextRender->Text(ItemFooter.m_X + ItemFooter.m_Width - pTextRender->TextWidth(FontSize, aBuf) - Spacing, ItemFooter.m_Y + ItemFooter.m_Height - FontSize - Spacing, FontSize, aBuf);
}
