#ifndef ENGINE_SHARED_SERVERINFO_H
#define ENGINE_SHARED_SERVERINFO_H

#include "protocol.h"

#include <engine/map.h>
#include <engine/serverbrowser.h>

typedef struct _json_value json_value;
class CServerInfo;

class CServerInfo2
{
public:
	class CClient
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		bool m_IsPlayer;
		bool m_IsAfk;
		char m_aSkin[MAX_SKIN_LENGTH];
		bool m_CustomSkinColors;
		int m_CustomSkinColorBody;
		int m_CustomSkinColorFeet;
	};

	CClient m_aClients[SERVERINFO_MAX_CLIENTS];
	int m_MaxClients;
	int m_NumClients; // Indirectly serialized.
	int m_MaxPlayers;
	int m_NumPlayers; // Not serialized.
	CServerInfo::EClientScoreKind m_ClientScoreKind;
	bool m_Passworded;
	char m_aGameType[16];
	char m_aName[64];
	char m_aMapName[MAX_MAP_LENGTH];
	char m_aVersion[32];
	bool m_RequiresLogin;

	bool operator==(const CServerInfo2 &Other) const;
	bool operator!=(const CServerInfo2 &Other) const { return !(*this == Other); }
	static bool FromJson(CServerInfo2 *pOut, const json_value *pJson);
	static bool FromJsonRaw(CServerInfo2 *pOut, const json_value *pJson);
	bool Validate() const;
	void ToJson(char *pBuffer, int BufferSize) const;

	operator CServerInfo() const;
};

bool ParseCrc(unsigned int *pResult, const char *pString);

#endif // ENGINE_SHARED_SERVERINFO_H
