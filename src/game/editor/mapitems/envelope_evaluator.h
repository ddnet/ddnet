#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_EVALUATOR_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_EVALUATOR_H

#include <base/color.h>

#include <game/editor/map_object.h>
#include <game/map/render_interfaces.h>

class CMapEnvelopeEvaluator : public CMapObject, public IEnvelopeEval
{
public:
	explicit CMapEnvelopeEvaluator(CEditorMap *pMap);

	void EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels) override;

	bool m_Animate = false;
	float m_AnimateStart = 0.0f;
	float m_AnimateTime = 0.0f;
	float m_AnimateSpeed = 1.0f;
	bool m_AnimateUpdatePopup = false;
};

#endif
