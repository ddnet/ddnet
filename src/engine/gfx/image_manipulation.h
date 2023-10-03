#ifndef ENGINE_GFX_IMAGE_MANIPULATION_H
#define ENGINE_GFX_IMAGE_MANIPULATION_H

#include <cstdint>

// These functions assume that the image data is 4 bytes per pixel RGBA
void DilateImage(unsigned char *pImageBuff, int w, int h);
void DilateImageSub(unsigned char *pImageBuff, int w, int h, int x, int y, int sw, int sh);

// returned pointer is allocated with malloc
uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

int HighestBit(int OfVar);

#endif // ENGINE_GFX_IMAGE_MANIPULATION_H
