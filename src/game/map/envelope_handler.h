#ifndef GAME_MAP_ENVELOPE_HANDLER_H
#define GAME_MAP_ENVELOPE_HANDLER_H

#include <engine/map.h>
#include <game/map/render_map.h>

#include <memory>

class CEnvelopeHandler : public IEnvelopeEval
{
public:
	CEnvelopeHandler(IEnvelopeEval *pEnvelopeEval, IMap *pMap);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels) override { return m_pEnvelopeEval->EnvelopeEval(TimeOffsetMillis, Env, Result, Channels); }

	class CEnvelopeExtrema
	{
	public:
		bool m_Available;
		ivec2 m_Minima;
		ivec2 m_Maxima;
	} m_EnvelopeExtramaNone;

	const CEnvelopeExtrema &GetExtrema(int Env) const;

private:
	void CalculateEnvelope(const CMapItemEnvelope *pEnvelopeItem, int EnvId);
	void CalculateExtrema();

	std::vector<CEnvelopeExtrema> m_vEnvelopeExtrema;

	IEnvelopeEval *m_pEnvelopeEval;
	CMapBasedEnvelopePointAccess m_EnvelopePoints;
	IMap *m_pMap;
};

#endif
