#ifndef GAME_MAP_ENVELOPE_MANAGER_H
#define GAME_MAP_ENVELOPE_MANAGER_H

#include <engine/map.h>

#include <game/map/envelope_extrema.h>
#include <game/map/render_interfaces.h>

#include <memory>

class CEnvelopeManager
{
public:
	CEnvelopeManager(const IEnvelopeEval *pEnvelopeEval, IMap *pMap) :
		m_pEnvelopeEval(pEnvelopeEval), m_EnvelopeExtrema(pMap) {}

	const IEnvelopeEval *EnvelopeEval() const { return m_pEnvelopeEval; }
	const CEnvelopeExtrema *EnvelopeExtrema() const { return &m_EnvelopeExtrema; }

private:
	const IEnvelopeEval *m_pEnvelopeEval;
	CEnvelopeExtrema m_EnvelopeExtrema;
};

#endif
