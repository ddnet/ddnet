#ifndef GAME_EDITOR_MAPITEMS_LAYER_GROUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_GROUP_H

#include "layer.h"

#include <memory>
#include <vector>

class CLayerGroup
{
public:
	class CEditorMap *m_pMap;

	std::vector<std::shared_ptr<CLayer>> m_vpLayers;

	int m_OffsetX;
	int m_OffsetY;

	int m_ParallaxX;
	int m_ParallaxY;

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	char m_aName[12];
	bool m_GameGroup;
	bool m_Visible;
	bool m_Collapse;

	CLayerGroup();
	~CLayerGroup();

	void Convert(CUIRect *pRect) const;
	void Render();
	void MapScreen() const;
	void Mapping(float *pPoints) const;

	void GetSize(float *pWidth, float *pHeight) const;

	void DeleteLayer(int Index);
	void DuplicateLayer(int Index);
	int SwapLayers(int Index0, int Index1);

	bool IsEmpty() const
	{
		return m_vpLayers.empty();
	}

	void Clear()
	{
		m_vpLayers.clear();
	}

	void AddLayer(const std::shared_ptr<CLayer> &pLayer);

	void ModifyImageIndex(FIndexModifyFunction Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyImageIndex(Func);
	}

	void ModifyEnvelopeIndex(FIndexModifyFunction Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifyEnvelopeIndex(Func);
	}

	void ModifySoundIndex(FIndexModifyFunction Func)
	{
		for(auto &pLayer : m_vpLayers)
			pLayer->ModifySoundIndex(Func);
	}
};

#endif
