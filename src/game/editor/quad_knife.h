#ifndef GAME_EDITOR_QUAD_KNIFE_H
#define GAME_EDITOR_QUAD_KNIFE_H

#include "component.h"

class CQuadKnife : public CEditorComponent
{
public:
	class CState
	{
	public:
		bool m_Active;
		int m_SelectedQuadIndex;
		int m_Count;
		vec2 m_aPoints[4];

		void Reset();
	};

	bool IsActive() const;
	void Activate(int SelectedQuad);
	void Deactivate();
	void DoSlice();
};

#endif
