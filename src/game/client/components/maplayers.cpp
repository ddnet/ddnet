/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/client/gameclient.h>
#include <game/localization.h>

#include "maplayers.h"

#include <chrono>

using namespace std::chrono_literals;

void CMapLayers::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly)
{
	using namespace std::chrono;
	int EnvStart, EnvNum;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)pMap->GetItem(EnvStart + Env);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	pEnvelopePoints->SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(pEnvelopePoints->NumPoints() == 0)
		return;

	// online rendering
	if(OnlineOnly)
	{
		// we are doing time integration for smoother animations
		static nanoseconds s_OnlineTime{0};
		static int s_SynchronizationTick = 0;

		static const nanoseconds s_NanosPerTick = nanoseconds(1s) / static_cast<int64_t>(pClient->GameTickSpeed());
		static int s_LastPredTick = 0;

		double IntraGameTick = pClient->PredIntraGameTick(g_Config.m_ClDummy);
		nanoseconds IntraTick = duration_cast<nanoseconds>(s_NanosPerTick * IntraGameTick);

		const int WarmupTimer = pGameClient->m_Snap.m_pGameInfoObj && pGameClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME ? pGameClient->m_Snap.m_pGameInfoObj->m_WarmupTimer : 0;
		bool IsSynchronized = pItem->m_Synchronized && pItem->m_Version >= 2;

		const int CurTick = pClient->GameTick(g_Config.m_ClDummy);
		const int PredTick = pClient->PredGameTick(g_Config.m_ClDummy);

		// we may have missed the sync tick
		if(WarmupTimer < 0 && WarmupTimer + s_SynchronizationTick != 0)
			s_SynchronizationTick = -WarmupTimer;

		// Reset if times desync too much (for example map reload or server switch)
		if(s_LastPredTick != PredTick && s_LastPredTick != PredTick - 1)
		{
			s_OnlineTime = nanoseconds::zero();
			s_LastPredTick = 0;
		}

		// detect if we hit the start line
		bool DoResync = IsSynchronized && WarmupTimer + CurTick == 0;

		// Reset synchronization
		if(DoResync)
		{
			// save pred tick on startline
			s_SynchronizationTick = CurTick;
		}

		// resync to server time on each tick
		if(s_LastPredTick < PredTick)
		{
			// resync time
			s_OnlineTime = s_NanosPerTick * PredTick;
			s_LastPredTick = PredTick;
		}

		if(IsSynchronized)
		{
			nanoseconds SynchronizationOffset = s_NanosPerTick * s_SynchronizationTick;

			// prevent flickering on the start line, minimal flicker due to missing intratick offset, almost unnoticable
			if(DoResync)
			{
				// don't use online time on resync because this will cause flickering
				CRenderMap::RenderEvalEnvelope(pEnvelopePoints, milliseconds(TimeOffsetMillis), Result, Channels);
			}
			else
				CRenderMap::RenderEvalEnvelope(pEnvelopePoints, s_OnlineTime - SynchronizationOffset + IntraTick + milliseconds(TimeOffsetMillis), Result, Channels);
		}
		else
			CRenderMap::RenderEvalEnvelope(pEnvelopePoints, s_OnlineTime + IntraTick + milliseconds(TimeOffsetMillis), Result, Channels);
	}
	else // offline rendering (like menu background) relies on local time
	{
		// integrate over time deltas for smoother animations
		static auto s_LastLocalTime = time_get_nanoseconds();
		nanoseconds CurTime = time_get_nanoseconds();
		static nanoseconds s_Time{0};
		s_Time += CurTime - s_LastLocalTime;

		CRenderMap::RenderEvalEnvelope(pEnvelopePoints, s_Time + milliseconds(TimeOffsetMillis), Result, Channels);

		// update local timer
		s_LastLocalTime = CurTime;
	}
}

void CMapLayers::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels)
{
	EnvelopeEval(TimeOffsetMillis, Env, Result, Channels, this->m_pLayers->Map(), this->m_pEnvelopePoints.get(), this->Client(), this->GameClient(), this->m_OnlineOnly);
}

CMapLayers::CMapLayers(ERenderType Type, bool OnlineOnly)
{
	m_Type = Type;
	m_OnlineOnly = OnlineOnly;

	// static parameters for ingame rendering
	m_Params.m_RenderType = m_Type;
	m_Params.m_RenderInvalidTiles = false;
	m_Params.m_TileAndQuadBuffering = true;
	m_Params.m_RenderTileBorder = true;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = &GameClient()->m_MapImages;
	m_MapRenderer.Clear();
}

CCamera *CMapLayers::GetCurCamera()
{
	return &GameClient()->m_Camera;
}

void CMapLayers::OnMapLoad()
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pLayers->Map());
	FRenderUploadCallback FRenderCallback = [&](const char *pTitle, const char *pMessage, int IncreaseCounter) { GameClient()->m_Menus.RenderLoading(pTitle, pMessage, IncreaseCounter); };
	auto FRenderCallbackOptional = std::make_optional<FRenderUploadCallback>(FRenderCallback);

	const char *pLoadingTitle = Localize("Loading map");
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	GameClient()->m_Menus.RenderLoading(pLoadingTitle, pLoadingMessage, 0);

	// can't do that in CMapLayers::OnInit, because some of this interfaces are not available yet
	m_MapRenderer.OnInit(Graphics(), TextRender(), RenderMap());

	m_MapRenderer.Load(m_Type, m_pLayers, m_pImages, this, FRenderCallbackOptional);
}

void CMapLayers::OnRender()
{
	if(m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// dynamic parameters for ingame rendering
	m_Params.m_EntityOverlayVal = m_Type == RENDERTYPE_FULL_DESIGN ? 0 : g_Config.m_ClOverlayEntities;
	m_Params.m_Center = GetCurCamera()->m_Center;
	m_Params.m_Zoom = GetCurCamera()->m_Zoom;
	m_Params.m_RenderText = g_Config.m_ClTextEntities;
	m_Params.m_DebugRenderOptions = g_Config.m_DbgRenderLayers;

	m_MapRenderer.Render(m_Params);
}
