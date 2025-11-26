#ifndef GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H
#define GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H

#include <game/client/component.h>
#include <game/map/render_interfaces.h>
#include <game/map/render_map.h>

#include <memory>

enum EEnvelopeTriggerType
{
	DEFAULT = 0,
	STOP,
	START_ONCE,
	START_LOOP,
	RESET_STOP,
	RESET_START_ONCE,
	RESET_START_LOOP,
	NUM_ENV_TRIGGERS,
};

class CEnvelopeTrigger {
public:
	int m_EnvId;
	EEnvelopeTriggerType m_State;
	static const char *ConsoleName(EEnvelopeTriggerType Trigger);
	static EEnvelopeTriggerType FromName(const char* pTriggerName);
};

class CEnvelopeTriggerZone {
public:
	std::vector<CEnvelopeTrigger> m_EnvTriggers;
};

class CEnvelopeState : public CComponent, public IEnvelopeEval
{
public:
	CEnvelopeState() :
		m_pEnvelopePoints(nullptr), m_pMap(nullptr) {}
	CEnvelopeState(IMap *pMap, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels) override;

	int Sizeof() const override { return sizeof(*this); }

private:
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	IMap *m_pMap;
	bool m_OnlineOnly;
};

#endif
