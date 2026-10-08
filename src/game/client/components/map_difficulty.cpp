/* (c) Mykyta Polishyk. See "licence.txt" and the readme.txt in the root of the distribution for more information. */

#include "map_difficulty.h"

#include <engine/http.h>
#include <engine/kernel.h>
#include <engine/shared/json.h>
#include <base/str.h>

#include <memory>
#include <string>


CMapDifficulty::CMapDifficulty(): m_RunningRequests(0){
}

int CMapDifficulty::Sizeof() const
{
	return sizeof(*this);
}

int CMapDifficulty::Get(const char *pMapName)
{
	if(!pMapName || !pMapName[0])
		return 0;

	const auto It = m_Cache.find(pMapName);

	if(It != m_Cache.end())
		return It->second;

	return 0;
}

void CMapDifficulty::Request(const char *pMapName)
{
	if(!pMapName || !pMapName[0])
		return;

	const std::string MapName(pMapName);

	if(m_Cache.find(MapName) != m_Cache.end())
		return;

	if(m_Pending.find(MapName) != m_Pending.end())
		return;

	m_Pending.insert(MapName);
	m_Queue.push_back(MapName);
}

void CMapDifficulty::OnUpdate()
{
	IEngineHttp *pHttp = Kernel()->RequestInterface<IEngineHttp>();

	if(!pHttp)
		return;

	// Check completed requests.
	for(size_t i = 0; i < m_Requests.size();)
	{
		CHttpRequest &Request = m_Requests[i];

		if(!Request.m_pRequest->Done())
		{
			++i;
			continue;
		}

		const std::string MapName = Request.m_MapName;
		std::shared_ptr<IHttpRequest> pRequest = Request.m_pRequest;

		if(pRequest->State() == EHttpState::DONE)
		{
			const int StatusCode = pRequest->StatusCode();
			int FoundDifficulty = -1;

			if(StatusCode >= 200 && StatusCode < 300)
			{
				json_value *pJson = pRequest->ResultJson();

				if(pJson)
				{
					if(pJson->type == json_object)
					{
						const json_value *pDifficulty = json_object_get(pJson, "difficulty");

						if(pDifficulty && pDifficulty->type == json_integer)
						{
							const int Difficulty = json_int_get(pDifficulty);

							if(Difficulty > 0)
                                FoundDifficulty = Difficulty;
						}
					}

					json_value_free(pJson);
				}
			}
            m_Cache[MapName] = FoundDifficulty;
		}
		else{
            m_Cache[MapName] = -1;
		}

		m_Pending.erase(MapName);

		if(m_RunningRequests > 0)
			--m_RunningRequests;

		m_Requests.erase(m_Requests.begin() + i);
	}

	// Start queued requests.
	while(m_RunningRequests < 512 && !m_Queue.empty())
	{
		const std::string MapName = m_Queue.front();
		m_Queue.pop_front();

		if(m_Cache.find(MapName) != m_Cache.end())
		{
			m_Pending.erase(MapName);
			continue;
		}

		char aEscapedMapName[256];
		EscapeUrl(aEscapedMapName, sizeof(aEscapedMapName), MapName.c_str());

		char aUrl[512];
		str_format(aUrl, sizeof(aUrl), "https://ddnet.org/maps/?json=%s", aEscapedMapName);

		std::unique_ptr<IHttpRequest> pRequest = HttpGet(aUrl);

		if(!pRequest)
		{
			m_Pending.erase(MapName);
			continue;
		}

		pRequest->WriteToMemory();
		pRequest->Timeout(CTimeout{4000, 15000, 500, 5});

		std::shared_ptr<IHttpRequest> pSharedRequest = std::move(pRequest);

		CHttpRequest Request;
		Request.m_pRequest = pSharedRequest;
		Request.m_MapName = MapName;

		m_Requests.push_back(std::move(Request));

		++m_RunningRequests;

		pHttp->Run(pSharedRequest);
	}
}
