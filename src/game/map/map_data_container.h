#ifndef GAME_MAP_DATA_CONTAINER
#define GAME_MAP_DATA_CONTAINER

#include <base/color.h>
#include <game/client/component.h>
#include <game/mapitems.h>
#include "map_render.h"

class IMapData
{
public:
  virtual ~IMapData() = default;
  virtual void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly) = 0;
  virtual CQuad *GetQuads(int DataIndex) = 0;
  virtual void *GetTileDataRaw(int DataIndex) = 0;
  virtual size_t GetTileDataSize(int DataIndex) = 0;
};

class IMapDataComponent : public CComponentInterfaces, public IMapData
{
public:
  IMapDataComponent(IMap *pMap, CGameClient *pGameClient) : m_pMap(pMap), m_EnvelopePoints(m_pMap) { OnInterfacesInit(pGameClient); }
  virtual ~IMapDataComponent() = default;
  
	CQuad *GetQuads(int DataIndex) override { return (CQuad*)m_pMap->GetDataSwapped(DataIndex); }
  void *GetTileDataRaw(int DataIndex) override { return m_pMap->GetData(DataIndex); }
  size_t GetTileDataSize(int DataIndex) override { return m_pMap->GetDataSize(DataIndex); }
  virtual void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly) override { return MapRender::EnvelopeEval(TimeOffsetMillis, Env, Result, Channels, m_pMap, &m_EnvelopePoints, Client(), GameClient(), OnlineOnly); }

private:
  IMap *m_pMap;
  CMapBasedEnvelopePointAccess m_EnvelopePoints;
};

#endif