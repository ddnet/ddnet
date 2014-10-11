
#pragma once

#include <base/tl/array.h>

#include <game/client/component.h>

class CMapSounds : public CComponent
{
	int m_aSounds[64];
	int m_Count;

	struct CSource
	{
		int m_Sound;
		CSoundSource *m_pSource;

		bool operator ==(const CSource &Other) const { return (m_Sound == Other.m_Sound) && (m_pSource == Other.m_pSource); }
	};

	array<CSource> m_SourceQueue;

public:
	CMapSounds();

	virtual void OnMapLoad();
	virtual void OnRender();
};