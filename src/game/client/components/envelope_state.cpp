#include "envelope_state.h"

#include <game/client/gameclient.h>
#include <game/localization.h>

CEnvelopeState::CEnvelopeState(IMap *pMap, bool OnlineOnly) :
	m_pMap(pMap)
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pMap);
	m_OnlineOnly = OnlineOnly;
}

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels, EnvelopeEvalCallback Callback)
{
	using namespace std::chrono;

	if(!m_pMap)
		return;

	int EnvelopeStart, EnvelopeNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeNum);
	if(EnvelopeIndex < 0 || EnvelopeIndex >= EnvelopeNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)m_pMap->GetItem(EnvelopeStart + EnvelopeIndex);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	m_pEnvelopePoints->SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
		return;

	nanoseconds Time;

	// offline rendering (like menu background) relies on local time
	if(!m_OnlineOnly)
	{
		Time = time_get_nanoseconds();
	}
	else
	{
		// online rendering
		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const int MinTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;

			double TickRatio = mix<double>(0, CurTick - MinTick, (double)Client()->IntraGameTick(g_Config.m_ClDummy));
			Time = duration_cast<nanoseconds>(TickRatio * NanosPerTick()) + MinTick * NanosPerTick();

			// handle envelope triggers
			auto EnvelopeState = GameClient()->m_GameWorld.EnvTriggerState().find(EnvelopeIndex);
			if(EnvelopeState != GameClient()->m_GameWorld.EnvTriggerState().end())
			{
				CEnvelopeTriggerState &TriggerState = EnvelopeState->second;

				/* Initialize if not already done, we can't do this earlier,
				 * because we are missing the envelope points and the time */
				if(TriggerState.Duration() == nanoseconds::zero())
				{
					TriggerState.InitTimes(EnvelopeDuration(), Time);
				}

				TriggerState.Update(Time);

				// default mode still needs updates, but just runs on global timer
				if(!EnvelopeState->second.IsDefault())
				{
					Time = TriggerState.EnvelopeTime();
				}

				// callback for sounds
				if(Callback)
				{
					Callback(TriggerState.IsDefault(), TriggerState.IsLooping(), TriggerState.IsPlaying(), TriggerState.EnvelopeTime() == nanoseconds::zero());
				}
			}
		}
		else
		{
			Time = nanoseconds::zero();
		}
	}

	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), Time + milliseconds(TimeOffsetMillis), Result, Channels);
}

std::chrono::milliseconds CEnvelopeState::EnvelopeDuration() const
{
	const CEnvPoint *pLastEnvPoint = m_pEnvelopePoints->GetPoint(m_pEnvelopePoints->NumPoints() - 1);
	const CFixedTime &LastEnvPointTime = pLastEnvPoint->m_Time;

	return std::chrono::milliseconds(LastEnvPointTime.GetInternal());
}

static constexpr const char *s_apEnvelopeTriggerNames[NUM_ENV_TRIGGERS] = {
	"default",
	"pause",
	"start_once",
	"start_loop",
	"stop",
	"reset_start_once",
	"reset_start_loop",
};

const char *CEnvelopeTrigger::ConsoleName(EEnvelopeTriggerType Trigger)
{
	dbg_assert(Trigger >= 0 && Trigger < NUM_ENV_TRIGGERS, "unknown envelope trigger type");
	return s_apEnvelopeTriggerNames[static_cast<int>(Trigger)];
}

EEnvelopeTriggerType CEnvelopeTrigger::FromName(const char *pTriggerName)
{
	for(int i = 0; i < NUM_ENV_TRIGGERS; i++)
	{
		if(str_comp(pTriggerName, s_apEnvelopeTriggerNames[i]) == 0)
			return static_cast<EEnvelopeTriggerType>(i);
	}
	return EEnvelopeTriggerType::DEFAULT;
}

CEnvelopeTriggerState::CEnvelopeTriggerState(EEnvelopeTriggerType Type, CEnvelopeTriggerState *pOld)
{
	if(pOld != nullptr)
	{
		m_Duration = pOld->m_Duration;
		m_LastGlobalTime = pOld->m_LastGlobalTime;
	}
	else
	{
		m_Duration = std::chrono::nanoseconds::zero();
		m_LastGlobalTime = std::chrono::nanoseconds::zero();
	}

	// Envelope Time
	if(pOld != nullptr && (Type == EEnvelopeTriggerType::PAUSE || Type == EEnvelopeTriggerType::START_ONCE || Type == EEnvelopeTriggerType::START_LOOP))
	{
		m_CurrentTime = pOld->m_CurrentTime;
	}
	else
	{
		m_CurrentTime = std::chrono::nanoseconds::zero();
	}

	m_IsPlaying = !(Type == EEnvelopeTriggerType::PAUSE || Type == EEnvelopeTriggerType::STOP);
	m_IsLooping = (Type == EEnvelopeTriggerType::START_LOOP || Type == EEnvelopeTriggerType::RESET_START_LOOP);
	m_IsDefault = (Type == EEnvelopeTriggerType::DEFAULT);
}

void CEnvelopeTriggerState::InitTimes(const std::chrono::nanoseconds &Duration, const std::chrono::nanoseconds &Time)
{
	m_Duration = Duration;
	m_LastGlobalTime = Time;
}

void CEnvelopeTriggerState::Update(std::chrono::nanoseconds &Time)
{
	// default mode just keeps running in current time
	if(m_IsDefault)
	{
		m_LastGlobalTime = Time;
		return;
	}

	// we're not playing so we don't update the time
	if(!m_IsPlaying)
	{
		// keep updating, so the starting delta is correct
		m_LastGlobalTime = Time;
		return;
	}

	// update with delta T, current Time can be behind due to PAUSEping
	m_CurrentTime += Time - m_LastGlobalTime;

	if(m_CurrentTime > m_Duration)
	{
		if(m_IsLooping)
		{
			m_CurrentTime -= m_Duration;
		}
		else
		{
			m_CurrentTime = std::chrono::nanoseconds::zero();
			m_IsPlaying = false;
		}
	}

	m_LastGlobalTime = Time;
}
