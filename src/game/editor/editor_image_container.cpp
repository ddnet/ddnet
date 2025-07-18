#include <game/editor/mapitems/image.h>
#include <game/map/image_container.h>

#include "editor.h"

IGraphics::CTextureHandle CEditor::Get(int Index) const
{
	return m_Map.m_vpImages[Index]->m_Texture;
}

int CEditor::Num() const
{
	return (int)m_Map.m_vpImages.size();
}

IGraphics::CTextureHandle CEditor::GetEntities(IImageContainer::EMapImageEntityLayerType EntityLayerType)
{
	switch(EntityLayerType)
	{
	case IImageContainer::EMapImageEntityLayerType::MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH:
		return this->GetEntitiesTexture();
	case IImageContainer::EMapImageEntityLayerType::MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT:
		return this->GetFrontTexture();
	default:
		return this->GetFrontTexture(); // TODO No idea
	}
}

IGraphics::CTextureHandle CEditor::GetSpeedupArrow()
{
	return GetSpeedupTexture();
}

IGraphics::CTextureHandle CEditor::GetOverlayBottom()
{
	return IGraphics::CTextureHandle();
}

IGraphics::CTextureHandle CEditor::GetOverlayTop()
{
	return IGraphics::CTextureHandle();
}

IGraphics::CTextureHandle CEditor::GetOverlayCenter()
{
	return IGraphics::CTextureHandle();
}

CGameClient *CEditor::GameClient()
{
	// omg is this ugly
	return (CGameClient *)Kernel()->RequestInterface<IGameClient>();
}
