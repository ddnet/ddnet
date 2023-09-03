#ifndef ENGINE_GFX_IMAGE_MANIPULATION_H
#define ENGINE_GFX_IMAGE_MANIPULATION_H

#include <stdint.h>

void DilateImage(unsigned char *pImageBuff, int w, int h, int BPP);
void DilateImageSub(unsigned char *pImageBuff, int w, int h, int BPP, int x, int y, int sw, int sh);

// returned pointer is allocated with malloc
uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

int HighestBit(int OfVar);

#endif // ENGINE_GFX_IMAGE_MANIPULATION_H
