#include "envelope_state.h"

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

CEnvelopeState::CEnvelopeState(IMap *pMap, bool OnlineOnly) :
	m_pMap(pMap)
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pMap);
	m_OnlineOnly = OnlineOnly;
}

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels)
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
			static const nanoseconds s_NanosPerTick = nanoseconds(1s) / static_cast<int64_t>(Client()->GameTickSpeed());

			// get the lerp of the current tick and prev
			const int MinTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;

			double TickRatio = mix<double>(0, CurTick - MinTick, (double)Client()->IntraGameTick(g_Config.m_ClDummy));
			Time = duration_cast<nanoseconds>(TickRatio * s_NanosPerTick) + MinTick * s_NanosPerTick;
		}
		else
		{
			Time = nanoseconds::zero();
		}
	}

	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), Time + milliseconds(TimeOffsetMillis), Result, Channels);
}

const char *CEnvelopeTrigger::ConsoleName(EEnvelopeTriggerType Trigger)
{
	switch(Trigger)
	{
	case EEnvelopeTriggerType::DEFAULT:
		return "default";
	case EEnvelopeTriggerType::STOP:
		return "stop";
	case EEnvelopeTriggerType::START_ONCE:
		return "start_once";
	case EEnvelopeTriggerType::START_LOOP:
		return "start_loop";
	case EEnvelopeTriggerType::RESET_STOP:
		return "reset_stop";
	case EEnvelopeTriggerType::RESET_START_ONCE:
		return "reset_start_once";
	case EEnvelopeTriggerType::RESET_START_LOOP:
		return "reset_start_loop";
	default:
		dbg_assert_failed("unknown envelope trigger type");
	}
}

EEnvelopeTriggerType CEnvelopeTrigger::FromName(const char *pTriggerName)
{
	if(str_comp(pTriggerName, "stop") == 0)
		return EEnvelopeTriggerType::STOP;
	if(str_comp(pTriggerName, "start_once") == 0)
		return EEnvelopeTriggerType::START_ONCE;
	if(str_comp(pTriggerName, "start_loop") == 0)
		return EEnvelopeTriggerType::START_LOOP;
	if(str_comp(pTriggerName, "reset_stop") == 0)
		return EEnvelopeTriggerType::RESET_STOP;
	if(str_comp(pTriggerName, "reset_start_once") == 0)
		return EEnvelopeTriggerType::RESET_START_ONCE;
	if(str_comp(pTriggerName, "reset_start_loop") == 0)
		return EEnvelopeTriggerType::RESET_START_LOOP;

	return EEnvelopeTriggerType::DEFAULT;
}
