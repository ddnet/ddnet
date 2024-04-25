/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_CLIENT_GRAPH_H
#define ENGINE_CLIENT_GRAPH_H

#include <base/color.h>

#include <engine/shared/ringbuffer.h>

#include <cstddef>

class IGraphics;
class ITextRender;

class CGraph
{
private:
	struct SEntry
	{
		int64_t m_Time;
		float m_Value;
		ColorRGBA m_Color;
		bool m_ApplyColor;
	};
	SEntry *m_pFirstScaled = nullptr;
	int64_t m_RenderedTotalTime = 0;
	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	CDynamicRingBuffer<SEntry> m_Entries;

public:
	CGraph(int MaxEntries);

	void Init(float Min, float Max);
	void SetMin(float Min);
	void SetMax(float Max);

	void Scale(int64_t WantedTotalTime);
	void Add(float Value, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f));
	void InsertAt(int64_t Time, float Value, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f));
	void Render(IGraphics *pGraphics, ITextRender *pTextRender, float x, float y, float w, float h, const char *pDescription);
};

#endif
