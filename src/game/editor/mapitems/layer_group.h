#ifndef GAME_EDITOR_MAPITEMS_LAYER_GROUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_GROUP_H

#include <game/editor/component.h>

class CLayer;

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CLayerGroup : public CEditorComponent
{
public:
	std::vector<std::shared_ptr<CLayer>> m_vpLayers = {};

	int m_OffsetX = 0;
	int m_OffsetY = 0;

	int m_ParallaxX = 100;
	int m_ParallaxY = 100;
	int m_CustomParallaxZoom = 0;
	int m_ParallaxZoom = 100;

	int m_UseClipping = 0;
	int m_ClipX = 0;
	int m_ClipY = 0;
	int m_ClipW = 0;
	int m_ClipH = 0;

	char m_aName[12] = "";
	bool m_GameGroup = false;
	bool m_Visible = true;
	bool m_Collapse = false;

	void Convert(CUIRect *pRect);
	void Render();
	void MapScreen();
	void Mapping(float *pPoints);

	void GetSize(float *pWidth, float *pHeight) const;

	void DeleteLayer(int Index);
	void DuplicateLayer(int Index);
	int SwapLayers(int Index0, int Index1);

	bool IsEmpty() const;
	void OnEdited();
	void Clear();

	void AddLayer(const std::shared_ptr<CLayer> &pLayer);

	void ModifyImageIndex(const FIndexModifyFunction &Func);
	void ModifyEnvelopeIndex(const FIndexModifyFunction &Func);
	void ModifySoundIndex(const FIndexModifyFunction &Func);
};

#endif
