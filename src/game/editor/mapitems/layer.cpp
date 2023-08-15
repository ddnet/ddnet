#include "layer.h"

#include <game/mapitems.h>

CLayer::CLayer()
{
	m_Type = LAYERTYPE_INVALID;
	str_copy(m_aName, "(invalid)");
	m_Visible = true;
	m_Readonly = false;
	m_Flags = 0;
}

CLayer::CLayer(const CLayer &Other) :
	CEditorComponent(Other)
{
	str_copy(m_aName, Other.m_aName);
	m_Flags = Other.m_Flags;
	m_Type = Other.m_Type;
	m_Visible = true;
	m_Readonly = false;
}

void CLayer::GetSize(float *pWidth, float *pHeight)
{
	*pWidth = 0;
	*pHeight = 0;
}
