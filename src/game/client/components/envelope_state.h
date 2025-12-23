#ifndef GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H
#define GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H

#include <game/client/component.h>
#include <game/map/render_interfaces.h>
#include <game/map/render_map.h>

#include <chrono>
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

class CEnvelopeTrigger
{
public:
	int m_EnvId;
	EEnvelopeTriggerType m_State;
	static const char *ConsoleName(EEnvelopeTriggerType Trigger);
	static EEnvelopeTriggerType FromName(const char *pTriggerName);
};

class CEnvelopeTriggerZone
{
public:
	std::vector<CEnvelopeTrigger> m_EnvTriggers;
};

class CEnvelopeTriggerState
{
public:
	CEnvelopeTriggerState() = default;
	CEnvelopeTriggerState(EEnvelopeTriggerType Type, CEnvelopeTriggerState *pOld = nullptr);

	void InitTimes(const std::chrono::nanoseconds &Duration, const std::chrono::nanoseconds &Time);
	void Update(std::chrono::nanoseconds &Time);

	bool IsLooping() const { return m_IsLooping; }
	bool IsDefault() const { return m_IsDefault; }
	const std::chrono::nanoseconds &EnvelopeTime() const { return m_CurrentTime; }
	const std::chrono::nanoseconds &Duration() const { return m_Duration; }
	void SetDuration(std::chrono::nanoseconds &Duration) { m_Duration = Duration; }
	void SetEnvelopeTime(std::chrono::nanoseconds &EnvelopeTime) { m_CurrentTime = EnvelopeTime; }

private:
	bool m_IsDefault;
	bool m_IsPlaying;
	bool m_IsLooping;
	std::chrono::nanoseconds m_LastGlobalTime;
	std::chrono::nanoseconds m_CurrentTime;
	std::chrono::nanoseconds m_Duration;
};

class CEnvelopeState : public CComponent, public IEnvelopeEval
{
public:
	CEnvelopeState() :
		m_pEnvelopePoints(nullptr), m_pMap(nullptr) {}
	CEnvelopeState(IMap *pMap, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels) override;
	static constexpr std::chrono::nanoseconds NanosPerTick()
	{
		using namespace std::chrono_literals;
		// I can't use Client()->GameTickSpeed() here
		return std::chrono::nanoseconds(1s) / static_cast<int64_t>(50);
	}

	int Sizeof() const override { return sizeof(*this); }

private:
	std::chrono::milliseconds EnvelopeDuration() const;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	IMap *m_pMap;
	bool m_OnlineOnly;
};

#endif
