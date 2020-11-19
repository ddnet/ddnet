#ifndef ENGINE_SHARED_IMAGE_MANIPULATION_H
#define ENGINE_SHARED_IMAGE_MANIPULATION_H

#include <stddef.h>
#include <stdint.h>

void DilateImage(unsigned char *pImageBuff, int w, int h, int BPP);

// returned pointer is allocated with malloc
uint8_t *ResizeImage(const uint8_t *pImgData, int Width, int Height, int NewWidth, int NewHeight, int BPP);

int HighestBit(int OfVar);

#endif
