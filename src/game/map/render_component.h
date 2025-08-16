#ifndef GAME_MAP_RENDER_COMPONENT_H
#define GAME_MAP_RENDER_COMPONENT_H

#include "render_map.h"

class IGraphics;
class ITextRender;

class CRenderComponent
{
public:
	virtual ~CRenderComponent() = default;
	IGraphics *Graphics() { return m_pGraphics; }
	const IGraphics *Graphics() const { return m_pGraphics; }
	ITextRender *TextRender() { return m_pTextRender; }
	CRenderMap *RenderMap() { return m_pRenderMap; }

	void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap);
	void OnInit(CRenderComponent *pRenderComponent);

private:
	IGraphics *m_pGraphics;
	ITextRender *m_pTextRender;
	CRenderMap *m_pRenderMap;
};

#endif
