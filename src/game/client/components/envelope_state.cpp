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
