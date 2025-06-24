#include <base/system.h>

#include "translation_context.h"

void CTranslationContext::Reset()
{
	m_ServerSettings.Reset();

	std::fill(std::begin(m_apPlayerInfosRace), std::end(m_apPlayerInfosRace), nullptr);

	for(CClientData &Client : m_aClients)
		Client.Reset();

	std::fill(std::begin(m_aDamageTaken), std::end(m_aDamageTaken), 0);
	std::fill(std::begin(m_aDamageTakenTick), std::end(m_aDamageTakenTick), 0);
	std::fill(std::begin(m_aLocalClientId), std::end(m_aLocalClientId), -1);

	m_ShouldSendGameInfo = false;
	m_GameStateFlags7 = 0;

	m_GameFlags = 0;
	m_ScoreLimit = 0;
	m_TimeLimit = 0;
	m_MatchNum = 0;
	m_MatchCurrent = 0;

	m_MapdownloadTotalsize = -1;
	m_MapDownloadChunkSize = 0;
	m_MapDownloadChunksPerRequest = 0;

	m_FlagCarrierBlue = 0;
	m_FlagCarrierRed = 0;
	m_TeamscoreRed = 0;
	m_TeamscoreBlue = 0;

	m_GameStartTick7 = 0;
	m_GameStateEndTick7 = 0;
}
