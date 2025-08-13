#include <engine/graphics.h>
#include <engine/textrender.h>

#include "render_component.h"

void CRenderComponent::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	m_pRenderMap = pRenderMap;
}

void CRenderComponent::OnInit(CRenderComponent *pRenderComponent)
{
	OnInit(pRenderComponent->Graphics(), pRenderComponent->TextRender(), pRenderComponent->RenderMap());
}
