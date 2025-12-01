#include "layer.h"

#include <base/str.h>

#include <game/mapitems.h>

CLayer::CLayer(CEditorMap *pMap, int Type) :
	CMapObject(pMap),
	m_Type(Type)
{
}

CLayer::CLayer(const CLayer &Other) :
	CMapObject(Other)
{
	m_Type = Other.m_Type;
	str_copy(m_aName, Other.m_aName);
	m_Flags = Other.m_Flags;
	m_Readonly = false;
	m_Visible = true;
}
