#include "serverbrowser.h"

#include <base/color.h>
#include <base/types.h>

#include <engine/friends.h>
#include <engine/shared/masterserver.h>

void CServerInfo::Reset()
{
	m_ServerIndex = 0;

	m_Type = SERVERINFO_VANILLA;
	m_ReceivedPackets = 0;

	m_NumAddresses = 0;

	m_QuickSearchHit = 0;
	m_FriendState = IFriends::FRIEND_NO;
	m_FriendNum = 0;

	m_MaxClients = 0;
	m_NumClients = 0;
	m_MaxPlayers = 0;
	m_NumPlayers = 0;
	m_Flags = 0;
	m_ClientScoreKind = CLIENT_SCORE_KIND_UNSPECIFIED;
	m_Favorite = TRISTATE::NONE;
	m_FavoriteAllowPing = TRISTATE::NONE;
	m_aCommunityId[0] = '\0';
	m_aCommunityCountry[0] = '\0';
	m_aCommunityType[0] = '\0';
	m_Location = LOC_UNKNOWN;
	m_LatencyIsEstimated = false;
	m_Latency = 0;
	m_HasRank = RANK_UNAVAILABLE;
	m_aGameType[0] = '\0';
	m_GametypeColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	m_aName[0] = '\0';
	m_aMap[0] = '\0';
	m_MapCrc = 0;
	m_MapSize = 0;
	m_aVersion[0] = '\0';
	m_aAddress[0] = '\0';
	m_vClients.clear();
	m_NumFilteredPlayers = 0;
	m_RequiresLogin = false;
}
