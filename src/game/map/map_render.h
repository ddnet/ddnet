#ifndef GAME_CLIENT_MAP_RENDER
#define GAME_CLIENT_MAP_RENDER

#include <engine/map.h>
#include <game/client/component.h>
#include <game/client/render.h>
#include <cstdint>
#include <memory>
#include <vector>

class CGameClient;
class ColorRGBA;

namespace MapRender
{
	enum
	{
		TYPE_BACKGROUND = 0,
		TYPE_BACKGROUND_FORCE,
		TYPE_FOREGROUND,
		TYPE_FULL_DESIGN,
		TYPE_ALL = -1,
	};

  void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly);
}

#endif /* GAME_CLIENT_MAP_RENDER */
