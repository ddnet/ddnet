#ifndef GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H
#define GAME_CLIENT_COMPONENTS_ENVELOPE_STATE_H

#include <base/dbg.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/envelope_trigger.h>
#include <game/map/render_interfaces.h>
#include <game/map/render_map.h>

#include <chrono>
#include <memory>

class CEnvelopeTriggerState
{
public:
	CEnvelopeTriggerState() :
		m_Duration(0) {}
	CEnvelopeTriggerState(EEnvelopeTriggerType Type, CEnvelopeTriggerState *pOld, const std::chrono::nanoseconds &StartTime, const std::chrono::nanoseconds &Time);

	void InitTimes(const std::chrono::nanoseconds &Duration, const std::chrono::nanoseconds &Time);
	void Update(const std::chrono::nanoseconds &Time);

	bool IsLooping() const { return m_IsLooping; }
	bool IsDefault() const { return m_IsDefault; }
	bool IsPlaying() const { return m_IsPlaying; }
	const std::chrono::nanoseconds &EnvelopeTime() const { return m_CurrentTime; }
	const std::chrono::nanoseconds &Duration() const { return m_Duration; }
	void SetDuration(const std::chrono::nanoseconds &Duration) { m_Duration = Duration; }
	void SetEnvelopeTime(const std::chrono::nanoseconds &EnvelopeTime) { m_CurrentTime = EnvelopeTime; }
	bool Predicted() const { return m_Predicted; }
	void SetPredicted(bool Predicted) { m_Predicted = Predicted; }
	EEnvelopeTriggerType Type() const { return m_Type; }

	// Absolute trigger moment of this state in the same tick basis as the Time passed to
	// InitTimes/Update. Unlike EnvelopeTime it is a point in time, not a playback position,
	// and it is only used to reconcile triggers against each other.
	const std::chrono::nanoseconds &StartTime() const { return m_StartTime; }
	void SetStartTime(const std::chrono::nanoseconds &StartTime) { m_StartTime = StartTime; }

private:
	bool m_IsDefault = true;
	bool m_IsPlaying = false;
	bool m_IsLooping = false;
	bool m_Predicted = false;
	EEnvelopeTriggerType m_Type = EEnvelopeTriggerType::TRIGGER_TYPE_DEFAULT;
	std::chrono::nanoseconds m_LastGlobalTime = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_CurrentTime = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_StartTime = std::chrono::nanoseconds::zero();
	std::chrono::nanoseconds m_Duration = std::chrono::nanoseconds::zero();
};

class CEnvelopeState : public CComponent, public IEnvelopeEval
{
public:
	CEnvelopeState() :
		m_pEnvelopePoints(nullptr), m_pMap(nullptr) {}
	CEnvelopeState(IMap *pMap, bool OnlineOnly);
	void EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels, FEnvelopeEvalCallback Callback = {}) const override;

	int Sizeof() const override { return sizeof(*this); }
	static constexpr std::chrono::nanoseconds NanosPerTick()
	{
		using namespace std::chrono_literals;
		return std::chrono::nanoseconds(1s) / static_cast<int64_t>(SERVER_TICK_SPEED);
	}

private:
	std::chrono::milliseconds EnvelopeDuration() const;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	IMap *m_pMap;
	bool m_OnlineOnly;
};

#endif
