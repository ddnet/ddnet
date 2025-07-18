#ifndef GAME_MAP_IMAGE_CONTAINER
#define GAME_MAP_IMAGE_CONTAINER

#include <engine/graphics.h>

class IImageContainer
{
public:
	enum EMapImageEntityLayerType
	{
		MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH = 0,
		MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH,

		MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT,
	};

	virtual ~IImageContainer() = default;

	virtual IGraphics::CTextureHandle Get(int Index) const = 0;
	virtual int Num() const = 0;

	virtual IGraphics::CTextureHandle GetEntities(EMapImageEntityLayerType EntityLayerType) = 0;
	virtual IGraphics::CTextureHandle GetSpeedupArrow() = 0;

	virtual IGraphics::CTextureHandle GetOverlayBottom() = 0;
	virtual IGraphics::CTextureHandle GetOverlayTop() = 0;
	virtual IGraphics::CTextureHandle GetOverlayCenter() = 0;
};

#endif
