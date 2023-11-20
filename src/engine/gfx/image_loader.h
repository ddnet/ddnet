#ifndef ENGINE_GFX_IMAGE_LOADER_H
#define ENGINE_GFX_IMAGE_LOADER_H

#include <cstddef>
#include <cstdint>
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
	SImageByteBuffer(std::vector<uint8_t> *pvBuff) :
		m_LoadOffset(0), m_pvLoadedImageBytes(pvBuff), m_Err(0) {}
	size_t m_LoadOffset;
	std::vector<uint8_t> *m_pvLoadedImageBytes;
	int m_Err;
};

bool LoadPNG(SImageByteBuffer &ByteLoader, const char *pFileName, int &Width, int &Height, uint8_t *&pImageBuff, EImageFormat &ImageFormat);
bool SavePNG(EImageFormat ImageFormat, const uint8_t *pRawBuffer, SImageByteBuffer &WrittenBytes, int Width, int Height);

#endif // ENGINE_GFX_IMAGE_LOADER_H
