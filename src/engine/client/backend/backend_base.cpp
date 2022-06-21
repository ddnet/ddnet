#include "backend_base.h"
#include <engine/gfx/image_manipulation.h>

size_t CCommandProcessorFragment_GLBase::TexFormatToImageColorChannelCount(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return 4;
	return 4;
}

void *CCommandProcessorFragment_GLBase::Resize(const unsigned char *pData, int Width, int Height, int NewWidth, int NewHeight, int BPP)
{
	return ResizeImage((const uint8_t *)pData, Width, Height, NewWidth, NewHeight, BPP);
}

bool CCommandProcessorFragment_GLBase::Texture2DTo3D(void *pImageBuffer, int ImageWidth, int ImageHeight, int ImageColorChannelCount, int SplitCountWidth, int SplitCountHeight, void *pTarget3DImageData, int &Target3DImageWidth, int &Target3DImageHeight)
{
	Target3DImageWidth = ImageWidth / SplitCountWidth;
	Target3DImageHeight = ImageHeight / SplitCountHeight;

	size_t FullImageWidth = (size_t)ImageWidth * ImageColorChannelCount;

	for(int Y = 0; Y < SplitCountHeight; ++Y)
	{
		for(int X = 0; X < SplitCountWidth; ++X)
		{
			for(int Y3D = 0; Y3D < Target3DImageHeight; ++Y3D)
			{
				int DepthIndex = X + Y * SplitCountWidth;

				size_t TargetImageFullWidth = (size_t)Target3DImageWidth * ImageColorChannelCount;
				size_t TargetImageFullSize = (size_t)TargetImageFullWidth * Target3DImageHeight;
				ptrdiff_t ImageOffset = (ptrdiff_t)(((size_t)Y * FullImageWidth * (size_t)Target3DImageHeight) + ((size_t)Y3D * FullImageWidth) + ((size_t)X * TargetImageFullWidth));
				ptrdiff_t TargetImageOffset = (ptrdiff_t)(TargetImageFullSize * (size_t)DepthIndex + ((size_t)Y3D * TargetImageFullWidth));
				mem_copy(((uint8_t *)pTarget3DImageData) + TargetImageOffset, ((uint8_t *)pImageBuffer) + (ptrdiff_t)(ImageOffset), TargetImageFullWidth);
			}
		}
	}

	return true;
}
