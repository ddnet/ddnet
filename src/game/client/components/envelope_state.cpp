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

void CEnvelopeState::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels)
{
	using namespace std::chrono;

	if(!m_pMap)
		return;

	int EnvStart, EnvNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)m_pMap->GetItem(EnvStart + Env);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	m_pEnvelopePoints->SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
		return;

	// online rendering
	if(m_OnlineOnly)
	{
		// we are doing time integration for smoother animations
		nanoseconds OnlineTime{0};
		static const nanoseconds s_NanosPerTick = nanoseconds(1s) / static_cast<int64_t>(Client()->GameTickSpeed());

		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const int CurTick = Client()->PredGameTick(g_Config.m_ClDummy);
			OnlineTime = std::chrono::nanoseconds((int64_t)(std::clamp(0.0, 1.0, (double)Client()->PredIntraGameTick(g_Config.m_ClDummy)) * s_NanosPerTick.count())) + CurTick * s_NanosPerTick;
		}
		CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), OnlineTime + milliseconds(TimeOffsetMillis), Result, Channels);
	}
	else // offline rendering (like menu background) relies on local time
	{
		// integrate over time deltas for smoother animations
		static auto s_LastLocalTime = time_get_nanoseconds();
		nanoseconds CurTime = time_get_nanoseconds();
		static nanoseconds s_Time{0};
		s_Time += CurTime - s_LastLocalTime;

		CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), s_Time + milliseconds(TimeOffsetMillis), Result, Channels);

		// update local timer
		s_LastLocalTime = CurTime;
	}
}
