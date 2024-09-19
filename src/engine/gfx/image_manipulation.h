#ifndef ENGINE_GFX_IMAGE_MANIPULATION_H
#define ENGINE_GFX_IMAGE_MANIPULATION_H

#include <engine/image.h>

#include <cstdint>

// Destination must have appropriate size for RGBA data
bool ConvertToRgba(uint8_t *pDest, const CImageInfo &SourceImage);
// Allocates appropriate buffer with malloc, must be freed by caller
bool ConvertToRgbaAlloc(uint8_t *&pDest, const CImageInfo &SourceImage);
// Replaces existing image data with RGBA data (unless already RGBA)
bool ConvertToRgba(CImageInfo &Image);

// Changes the image data (not the format)
void ConvertToGrayscale(const CImageInfo &Image);

// These functions assume that the image data is 4 bytes per pixel RGBA
void DilateImage(uint8_t *pImageBuff, int w, int h);
void DilateImage(const CImageInfo &Image);
void DilateImageSub(uint8_t *pImageBuff, int w, int h, int x, int y, int SubWidth, int SubHeight);

// Returned buffer is allocated with malloc, must be freed by caller
uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP);
// Replaces existing image data with resized buffer
void ResizeImage(CImageInfo &Image, int NewWidth, int NewHeight);

int HighestBit(int OfVar);

#endif // ENGINE_GFX_IMAGE_MANIPULATION_H
