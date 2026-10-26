#include "layer.h"

#include "map.h"

#include <base/str.h>

#include <game/mapitems.h>

CLayer::CLayer(CEditorMap *pMap, int Type, std::uint64_t RetainedId) :
	CMapObject(pMap)
{
	m_Id = RetainedId ? RetainedId : Map()->AllocateObjectId();
	m_Type = Type;
}

CLayer::CLayer(const CLayer &Other) :
	CMapObject(Other),
	CLayerValues(Other)
{
	m_Id = Map()->AllocateObjectId();
	m_Readonly = false;
	m_Visible = true;
}

void CLayer::OnAttach(CEditorMap *pMap)
{
	if(Map() != pMap)
	{
		m_Id = pMap->AllocateObjectId();
	}
	CMapObject::OnAttach(pMap);
}
