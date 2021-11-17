#ifndef ENGINE_SHARED_IMAGE_MANIPULATION_H
#define ENGINE_SHARED_IMAGE_MANIPULATION_H

#include <stddef.h>
#include <stdint.h>

#define TW_DILATE_ALPHA_THRESHOLD 10

void DilateImage(unsigned char *pImageBuff, int w, int h, int BPP);
void DilateImageSub(unsigned char *pImageBuff, int w, int h, int BPP, int x, int y, int sw, int sh);

// returned pointer is allocated with malloc
uint8_t *ResizeImage(const uint8_t *pImgData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

int HighestBit(int OfVar);

#endif
