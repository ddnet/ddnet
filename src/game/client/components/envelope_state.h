#ifndef GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H
#define GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H

#include <game/client/component.h>
#include <game/map/render_interfaces.h>
#include <game/map/render_map.h>

#include <chrono>
#include <memory>

class CEnvelopeState : public CComponent, public IEnvelopeEval
{
public:
	CEnvelopeState() :
		m_pEnvelopePoints(nullptr) {}
	CEnvelopeState(IMap *pMap, bool OnlineOnly);

	void Update();
	void EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels) const override;

	int Sizeof() const override { return sizeof(*this); }

private:
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	bool m_OnlineOnly;
	std::chrono::nanoseconds m_CachedTime;
};

#endif
