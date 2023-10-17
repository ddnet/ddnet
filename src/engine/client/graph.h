/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_CLIENT_GRAPH_H
#define ENGINE_CLIENT_GRAPH_H

#include <base/color.h>

#include <cstddef>

class IGraphics;
class ITextRender;

class CGraph
{
public:
	enum
	{
		MAX_VALUES = 128,
	};

private:
	struct SEntry
	{
		bool m_Initialized;
		float m_Value;
		ColorRGBA m_Color;
	};
	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	SEntry m_aEntries[MAX_VALUES];
	size_t m_Index;

public:
	void Init(float Min, float Max);
	void SetMin(float Min);
	void SetMax(float Max);

	void Scale();
	void Add(float Value, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f));
	void InsertAt(size_t Index, float Value, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f));
	void Render(IGraphics *pGraphics, ITextRender *pTextRender, float x, float y, float w, float h, const char *pDescription);
};

#endif
