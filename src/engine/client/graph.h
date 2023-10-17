/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_CLIENT_GRAPH_H
#define ENGINE_CLIENT_GRAPH_H

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
	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	float m_aValues[MAX_VALUES];
	float m_aColors[MAX_VALUES][3];
	size_t m_Index;

public:
	void Init(float Min, float Max);
	void SetMin(float Min);
	void SetMax(float Max);

	void Scale();
	void Add(float v, float r, float g, float b);
	void InsertAt(size_t Index, float v, float r, float g, float b);
	void Render(IGraphics *pGraphics, ITextRender *pTextRender, float x, float y, float w, float h, const char *pDescription);
};

#endif
