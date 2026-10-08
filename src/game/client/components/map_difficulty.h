/* (c) Mykyta Polishyk. See "licence.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_MAP_DIFFICULTY_H
#define GAME_CLIENT_MAP_DIFFICULTY_H

#include <game/client/component.h>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class IHttpRequest;

class CMapDifficulty : public CComponent
{
public:
	CMapDifficulty();

	int Get(const char *pMapName);
	void Request(const char *pMapName);

	virtual void OnUpdate() override;
	virtual int Sizeof() const override;
private:
	struct CHttpRequest
	{
		std::shared_ptr<IHttpRequest> m_pRequest;
		std::string m_MapName;
	};

	std::unordered_map<std::string, int> m_Cache;
	std::unordered_set<std::string> m_Pending;
	std::deque<std::string> m_Queue;
	std::vector<CHttpRequest> m_Requests;

	int m_RunningRequests;
};

#endif
