#include "envelope_state.h"

#include <base/time.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <chrono>

CEnvelopeState::CEnvelopeState(IMap *pMap, bool OnlineOnly) :
	m_pMap(pMap)
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pMap);
	m_OnlineOnly = OnlineOnly;
}

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels, FEnvelopeEvalCallback Callback) const
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
	Channels = std::min({Channels, (size_t)pItem->m_Channels, (size_t)CEnvPoint::MAX_CHANNELS});

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
			int EnvelopeTick;
			double TickRatio;
			if(Client()->State() == IClient::STATE_DEMOPLAYBACK || !g_Config.m_ClPredict ||
				(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW))
			{
				EnvelopeTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				TickRatio = mix<double>(0, CurTick - EnvelopeTick, (double)Client()->IntraGameTick(g_Config.m_ClDummy));
			}
			else
			{
				EnvelopeTick = Client()->PredGameTick(g_Config.m_ClDummy) - 1 - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
				TickRatio = (double)Client()->PredIntraGameTick(g_Config.m_ClDummy);
			}
			Time = duration_cast<nanoseconds>(TickRatio * NanosPerTick()) + EnvelopeTick * NanosPerTick();

			// handle envelope triggers
			bool IsDummy = g_Config.m_ClDummy;
			auto EnvelopeState = GameClient()->m_GameWorld.EnvelopeTriggerState(IsDummy).find(EnvelopeIndex);
			if(EnvelopeState != GameClient()->m_GameWorld.EnvelopeTriggerState(IsDummy).end())
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
	if(m_pEnvelopePoints->NumPoints() == 0)
		return std::chrono::milliseconds::zero();
	const CEnvPoint *pLastEnvPoint = m_pEnvelopePoints->GetPoint(m_pEnvelopePoints->NumPoints() - 1);
	const CFixedTime &LastEnvPointTime = pLastEnvPoint->m_Time;

	return std::chrono::milliseconds(LastEnvPointTime.GetInternal());
}

CEnvelopeTriggerState::CEnvelopeTriggerState(EEnvelopeTriggerType Type, CEnvelopeTriggerState *pOld, const std::chrono::nanoseconds &StartTime, const std::chrono::nanoseconds &Time)
{
	m_Type = Type;
	m_StartTime = StartTime;

	if(pOld != nullptr)
	{
		m_Duration = pOld->m_Duration;
		m_LastGlobalTime = pOld->m_LastGlobalTime;
	}

	// Envelope time semantics:
	// - stop resets the animation to 0, default keeps it unused
	// - pause freezes the animation at the current frame
	// - start_once/start_loop continue from the current frame (e.g. resume after a pause)
	// - the remaining (re)starts begin at the elapsed time since the trigger
	if(Type == EEnvelopeTriggerType::TRIGGER_TYPE_STOP || Type == EEnvelopeTriggerType::TRIGGER_TYPE_DEFAULT)
	{
		m_CurrentTime = std::chrono::nanoseconds::zero();
	}
	else if(Type == EEnvelopeTriggerType::TRIGGER_TYPE_PAUSE || Type == EEnvelopeTriggerType::TRIGGER_TYPE_START_ONCE || Type == EEnvelopeTriggerType::TRIGGER_TYPE_START_LOOP)
	{
		m_CurrentTime = Time - StartTime;
		if(pOld != nullptr)
		{
			m_CurrentTime = pOld->m_CurrentTime;
		}
	}
	else
	{
		m_CurrentTime = Time - StartTime;
	}

	m_IsPlaying = !(Type == EEnvelopeTriggerType::TRIGGER_TYPE_PAUSE || Type == EEnvelopeTriggerType::TRIGGER_TYPE_STOP);
	m_IsLooping = (Type == EEnvelopeTriggerType::TRIGGER_TYPE_START_LOOP || Type == EEnvelopeTriggerType::TRIGGER_TYPE_RESET_START_LOOP);
	m_IsDefault = (Type == EEnvelopeTriggerType::TRIGGER_TYPE_DEFAULT);
}

void CEnvelopeTriggerState::InitTimes(const std::chrono::nanoseconds &Duration, const std::chrono::nanoseconds &Time)
{
	m_Duration = Duration;
	m_LastGlobalTime = Time;
}

void CEnvelopeTriggerState::Update(const std::chrono::nanoseconds &Time)
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

	// update with delta T, current Time can be behind due to PAUSEing
	m_CurrentTime += Time - m_LastGlobalTime;

	if(m_CurrentTime >= m_Duration - std::chrono::nanoseconds(1) && !m_IsLooping)
	{
		m_CurrentTime = m_Duration - std::chrono::nanoseconds(1); // stop at last frame
		m_IsPlaying = false;
	}
	else if(m_CurrentTime >= m_Duration && m_IsLooping)
	{
		m_CurrentTime -= m_Duration;
	}

	m_LastGlobalTime = Time;
}
