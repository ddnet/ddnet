#ifndef ENGINE_GFX_IMAGE_LOADER_H
#define ENGINE_GFX_IMAGE_LOADER_H

#include <base/types.h>

#include <engine/image.h>

#include <vector>

class CByteBufferReader
{
	const uint8_t *m_pData;
	size_t m_Size;
	size_t m_ReadOffset = 0;
	bool m_Error = false;

public:
	CByteBufferReader(const uint8_t *pData, size_t Size) :
		m_pData(pData),
		m_Size(Size) {}

	bool Read(void *pData, size_t Size);
	bool Error() const { return m_Error; }
};

class CByteBufferWriter
{
	std::vector<uint8_t> m_vBuffer;

public:
	void Write(const void *pData, size_t Size);
	const uint8_t *Data() const { return m_vBuffer.data(); }
	size_t Size() const { return m_vBuffer.size(); }
};

class CImageLoader
{
	CImageLoader() = delete;

public:
	enum
	{
		PNGLITE_COLOR_TYPE = 1 << 0,
		PNGLITE_BIT_DEPTH = 1 << 1,
		PNGLITE_INTERLACE_TYPE = 1 << 2,
		PNGLITE_COMPRESSION_TYPE = 1 << 3,
		PNGLITE_FILTER_TYPE = 1 << 4,
	};

	static bool LoadPng(CByteBufferReader &Reader, const char *pContextName, CImageInfo &Image, int &PngliteIncompatible);
	static bool LoadPng(IOHANDLE File, const char *pFilename, CImageInfo &Image, int &PngliteIncompatible);

	static bool SavePng(CByteBufferWriter &Writer, const CImageInfo &Image);
	static bool SavePng(IOHANDLE File, const char *pFilename, const CImageInfo &Image);
};

#endif // ENGINE_GFX_IMAGE_LOADER_H
