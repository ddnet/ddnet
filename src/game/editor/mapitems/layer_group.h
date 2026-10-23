#ifndef GAME_EDITOR_MAPITEMS_LAYER_GROUP_H
#define GAME_EDITOR_MAPITEMS_LAYER_GROUP_H

#include "layer.h"

#include <game/editor/map_object.h>

#include <memory>
#include <vector>

class CLayerGroup : public CMapObject, public CLayerGroupValues
{
public:
	std::vector<std::shared_ptr<CLayer>> m_vpLayers;

	bool m_Visible;
	bool m_Collapse;

	explicit CLayerGroup(CEditorMap *pMap, std::uint64_t RetainedId = 0);
	void OnAttach(CEditorMap *pMap) override;

	void Convert(CUIRect *pRect) const;
	void Render(const CEditorMap *pRenderMap);
	void MapScreen();
	CScreenRect Mapping() const;

	void GetSize(float *pWidth, float *pHeight) const;

	void AddLayer(const std::shared_ptr<CLayer> &pLayer);
	void DeleteLayer(int Index);
	void DuplicateLayer(int Index);
	int MoveLayer(int IndexFrom, int IndexTo);

	bool IsEmpty() const;
	void Clear();

	void VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction);
	void VisitEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction);
	void VisitSoundReferences(const FDocumentReferenceFunction &ReferenceFunction);
};

#endif
