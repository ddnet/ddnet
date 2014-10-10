
#pragma once

#include <base/tl/array.h>

#include <game/client/component.h>

class CMapSounds : public CComponent
{
	int m_aSounds[64];
	int m_Count;

	array<CSoundSource *> m_SourceQueue;

public:
	CMapSounds();

	virtual void OnMapLoad();
	virtual void OnRender();

	int Get(int Index) const { return m_aSounds[Index]; }
	int Num() const { return m_Count; }
};