#include "map_object.h"

#include <game/editor/editor.h>
#include <game/editor/mapitems/map.h>

CMapObject::CMapObject(CEditorMap *pMap)
{
	OnAttach(pMap);
}

CMapObject::CMapObject(const CMapObject &Other)
{
	m_pMap = Other.m_pMap;
}

void CMapObject::OnAttach(CEditorMap *pMap)
{
	m_pMap = pMap;
}

const CEditor *CMapObject::Editor() const
{
	return m_pMap->Editor();
}

CEditor *CMapObject::Editor()
{
	return m_pMap->Editor();
}

const CEditorMap *CMapObject::Map() const
{
	return m_pMap;
}

CEditorMap *CMapObject::Map()
{
	return m_pMap;
}

const IGraphics *CMapObject::Graphics() const
{
	return m_pMap->Editor()->Graphics();
}

IGraphics *CMapObject::Graphics()
{
	return m_pMap->Editor()->Graphics();
}

const ISound *CMapObject::Sound() const
{
	return m_pMap->Editor()->Sound();
}

ISound *CMapObject::Sound()
{
	return m_pMap->Editor()->Sound();
}

const IStorage *CMapObject::Storage() const
{
	return m_pMap->Editor()->Storage();
}

IStorage *CMapObject::Storage()
{
	return m_pMap->Editor()->Storage();
}

const ITextRender *CMapObject::TextRender() const
{
	return m_pMap->Editor()->TextRender();
}

ITextRender *CMapObject::TextRender()
{
	return m_pMap->Editor()->TextRender();
}
