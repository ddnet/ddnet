#ifndef ENGINE_SHARED_IMAGE_LOADER_H
#define ENGINE_SHARED_IMAGE_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

enum EImageFormat
{
	IMAGE_FORMAT_R = 0,
	IMAGE_FORMAT_RGB,
	IMAGE_FORMAT_RGBA,
};

typedef std::vector<uint8_t> TImageByteBuffer;
struct SImageByteBuffer
{
	SImageByteBuffer(TImageByteBuffer *pBuff) :
		m_LoadOffset(0), m_pLoadedImageBytes(pBuff), m_Err(0) {}
	size_t m_LoadOffset;
	TImageByteBuffer *m_pLoadedImageBytes;
	int m_Err;
};

bool LoadPNG(SImageByteBuffer &ByteLoader, const char *pFileName, int &Width, int &Height, uint8_t *&pImageBuff, EImageFormat &ImageFormat);
bool SavePNG(EImageFormat ImageFormat, const uint8_t *pRawBuffer, SImageByteBuffer &WrittenBytes, int Width, int Height);

#endif
