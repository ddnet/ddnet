#include "envelope_evaluator.h"

#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/map.h>

CMapEnvelopeEvaluator::CMapEnvelopeEvaluator(CEditorMap *pMap) :
	CMapObject(pMap)
{
}

void CMapEnvelopeEvaluator::EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels)
{
	if(EnvelopeIndex < 0 || EnvelopeIndex >= (int)Map()->m_vpEnvelopes.size())
		return;

	std::shared_ptr<CEnvelope> pEnvelope = Map()->m_vpEnvelopes[EnvelopeIndex];
	float Time = m_AnimateTime;
	Time *= m_AnimateSpeed;
	Time += (TimeOffsetMillis / 1000.0f);
	pEnvelope->Eval(Time, Result, Channels);
}
