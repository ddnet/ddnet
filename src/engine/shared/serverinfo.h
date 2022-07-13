#ifndef ENGINE_SHARED_SERVERINFO_H
#define ENGINE_SHARED_SERVERINFO_H

#include "protocol.h"
#include <engine/map.h>

typedef struct _json_value json_value;
class CServerInfo;

class CServerInfo2
{
public:
	class CClient
	{
	public:
		char m_aName[MAX_NAME_LENGTH] = {0};
		char m_aClan[MAX_CLAN_LENGTH] = {0};
		int m_Country = 0;
		int m_Score = 0;
		bool m_IsPlayer = false;
	};

	CClient m_aClients[SERVERINFO_MAX_CLIENTS];
	int m_MaxClients = 0;
	int m_NumClients = 0; // Indirectly serialized.
	int m_MaxPlayers = 0;
	int m_NumPlayers = 0; // Not serialized.
	bool m_Passworded = false;
	char m_aGameType[16] = {0};
	char m_aName[64] = {0};
	char m_aMapName[MAX_MAP_LENGTH] = {0};
	char m_aVersion[32] = {0};

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
