#ifndef ENGINE_SHARED_TRANSLATION_CONTEXT_H
#define ENGINE_SHARED_TRANSLATION_CONTEXT_H

#include <engine/shared/protocol.h>
#include <game/generated/protocol7.h>

#include <engine/client/enums.h>

class CTranslationContext
{
public:
	CTranslationContext()
	{
		Reset();
	}

	void Reset();

	// this class is not used
	// it could be used in the in game menu
	// to grey out buttons and similar
	//
	// but that can not be done without mixing it
	// into the 0.6 code so it is out of scope for ddnet
	class CServerSettings
	{
	public:
		bool m_KickVote;
		int m_KickMin;
		bool m_SpecVote;
		bool m_TeamLock;
		bool m_TeamBalance;
		int m_PlayerSlots;

		CServerSettings()
		{
			Reset();
		}

		void Reset()
		{
			m_KickVote = false;
			m_KickMin = 0;
			m_SpecVote = false;
			m_TeamLock = false;
			m_TeamBalance = false;
			m_PlayerSlots = 0;
		}

	} m_ServerSettings;

	class CClientData
	{
	public:
		CClientData()
		{
			Reset();
		}

		void Reset()
		{
			m_Active = false;

			m_UseCustomColor = 0;
			m_ColorBody = 0;
			m_ColorFeet = 0;

			m_aName[0] = '\0';
			m_aClan[0] = '\0';
			m_Country = 0;
			m_aSkinName[0] = '\0';
			m_SkinColor = 0;
			m_Team = 0;
			m_PlayerFlags7 = 0;
		}

		bool m_Active;

		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		char m_aSkinName[protocol7::MAX_SKIN_LENGTH];
		int m_SkinColor;
		int m_Team;
		int m_PlayerFlags7;
	};

	const protocol7::CNetObj_PlayerInfoRace *m_apPlayerInfosRace[MAX_CLIENTS];
	CClientData m_aClients[MAX_CLIENTS];
	int m_aDamageTaken[MAX_CLIENTS];
	float m_aDamageTakenTick[MAX_CLIENTS];

	int m_aLocalClientId[NUM_DUMMIES];

	bool m_ShouldSendGameInfo;
	int m_GameStateFlags7;
	// 0.7 game flags
	// use in combination with protocol7::GAMEFLAG_*
	// for example protocol7::GAMEFLAG_TEAMS
	int m_GameFlags;
	int m_ScoreLimit;
	int m_TimeLimit;
	int m_MatchNum;
	int m_MatchCurrent;

	int m_MapdownloadTotalsize;
	int m_MapDownloadChunkSize;
	int m_MapDownloadChunksPerRequest;

	int m_FlagCarrierBlue;
	int m_FlagCarrierRed;
	int m_TeamscoreRed;
	int m_TeamscoreBlue;

	int m_GameStartTick7;
	int m_GameStateEndTick7;
};

#endif
