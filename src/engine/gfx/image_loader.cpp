#include "image_loader.h"
#include <base/log.h>
#include <base/system.h>
#include <csetjmp>
#include <cstdlib>

#include <png.h>

struct SLibPngWarningItem
{
	SImageByteBuffer *m_pByteLoader;
	const char *m_pFileName;
	std::jmp_buf m_Buf;
};

[[noreturn]] static void LibPngError(png_structp png_ptr, png_const_charp error_msg)
{
	SLibPngWarningItem *pUserStruct = (SLibPngWarningItem *)png_get_error_ptr(png_ptr);
	pUserStruct->m_pByteLoader->m_Err = -1;
	dbg_msg("png", "error for file \"%s\": %s", pUserStruct->m_pFileName, error_msg);
	std::longjmp(pUserStruct->m_Buf, 1);
}

static void LibPngWarning(png_structp png_ptr, png_const_charp warning_msg)
{
	SLibPngWarningItem *pUserStruct = (SLibPngWarningItem *)png_get_error_ptr(png_ptr);
	dbg_msg("png", "warning for file \"%s\": %s", pUserStruct->m_pFileName, warning_msg);
}

static bool FileMatchesImageType(SImageByteBuffer &ByteLoader)
{
	if(ByteLoader.m_pvLoadedImageBytes->size() >= 8)
		return png_sig_cmp((png_bytep)ByteLoader.m_pvLoadedImageBytes->data(), 0, 8) == 0;
	return false;
}

static void ReadDataFromLoadedBytes(png_structp pPngStruct, png_bytep pOutBytes, png_size_t ByteCountToRead)
{
	png_voidp pIO_Ptr = png_get_io_ptr(pPngStruct);

	SImageByteBuffer *pByteLoader = (SImageByteBuffer *)pIO_Ptr;

	if(pByteLoader->m_pvLoadedImageBytes->size() >= pByteLoader->m_LoadOffset + (size_t)ByteCountToRead)
	{
		mem_copy(pOutBytes, &(*pByteLoader->m_pvLoadedImageBytes)[pByteLoader->m_LoadOffset], (size_t)ByteCountToRead);

		pByteLoader->m_LoadOffset += (size_t)ByteCountToRead;
	}
	else
	{
		pByteLoader->m_Err = -1;
		dbg_msg("png", "could not read bytes, file was too small.");
	}
}

static EImageFormat LibPngGetImageFormat(int ColorChannelCount)
{
	switch(ColorChannelCount)
	{
	case 1:
		return IMAGE_FORMAT_R;
	case 2:
		return IMAGE_FORMAT_RA;
	case 3:
		return IMAGE_FORMAT_RGB;
	case 4:
		return IMAGE_FORMAT_RGBA;
	default:
		dbg_assert(false, "ColorChannelCount invalid");
		dbg_break();
	}
}

static void LibPngDeleteReadStruct(png_structp pPngStruct, png_infop pPngInfo)
{
	if(pPngInfo != nullptr)
		png_destroy_info_struct(pPngStruct, &pPngInfo);
	png_destroy_read_struct(&pPngStruct, nullptr, nullptr);
}

static int PngliteIncompatibility(png_structp pPngStruct, png_infop pPngInfo)
{
	int ColorType = png_get_color_type(pPngStruct, pPngInfo);
	int BitDepth = png_get_bit_depth(pPngStruct, pPngInfo);
	int InterlaceType = png_get_interlace_type(pPngStruct, pPngInfo);
	int Result = 0;
	switch(ColorType)
	{
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_RGB:
	case PNG_COLOR_TYPE_RGB_ALPHA:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		break;
	default:
		log_debug("png", "color type %d unsupported by pnglite", ColorType);
		Result |= PNGLITE_COLOR_TYPE;
	}

	switch(BitDepth)
	{
	case 8:
	case 16:
		break;
	default:
		log_debug("png", "bit depth %d unsupported by pnglite", BitDepth);
		Result |= PNGLITE_BIT_DEPTH;
	}

	if(InterlaceType != PNG_INTERLACE_NONE)
	{
		log_debug("png", "interlace type %d unsupported by pnglite", InterlaceType);
		Result |= PNGLITE_INTERLACE_TYPE;
	}
	if(png_get_compression_type(pPngStruct, pPngInfo) != PNG_COMPRESSION_TYPE_BASE)
	{
		log_debug("png", "non-default compression type unsupported by pnglite");
		Result |= PNGLITE_COMPRESSION_TYPE;
	}
	if(png_get_filter_type(pPngStruct, pPngInfo) != PNG_FILTER_TYPE_BASE)
	{
		log_debug("png", "non-default filter type unsupported by pnglite");
		Result |= PNGLITE_FILTER_TYPE;
	}
	return Result;
}

bool LoadPng(SImageByteBuffer &ByteLoader, const char *pFileName, int &PngliteIncompatible, size_t &Width, size_t &Height, uint8_t *&pImageBuff, EImageFormat &ImageFormat)
{
	SLibPngWarningItem UserErrorStruct = {&ByteLoader, pFileName, {}};

	png_structp pPngStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if(pPngStruct == nullptr)
	{
		dbg_msg("png", "libpng internal failure: png_create_read_struct failed.");
		return false;
	}

	png_infop pPngInfo = nullptr;
	png_bytepp pRowPointers = nullptr;
	Height = 0; // ensure this is not undefined for the error handler
	if(setjmp(UserErrorStruct.m_Buf))
	{
		if(pRowPointers != nullptr)
		{
			for(size_t i = 0; i < Height; ++i)
			{
				delete[] pRowPointers[i];
			}
		}
		delete[] pRowPointers;
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		return false;
	}
	png_set_error_fn(pPngStruct, &UserErrorStruct, LibPngError, LibPngWarning);

	pPngInfo = png_create_info_struct(pPngStruct);

	if(pPngInfo == nullptr)
	{
		png_destroy_read_struct(&pPngStruct, nullptr, nullptr);
		dbg_msg("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	if(!FileMatchesImageType(ByteLoader))
	{
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		dbg_msg("png", "file does not match image type.");
		return false;
	}

	ByteLoader.m_LoadOffset = 8;

	png_set_read_fn(pPngStruct, (png_bytep)&ByteLoader, ReadDataFromLoadedBytes);

	png_set_sig_bytes(pPngStruct, 8);

	png_read_info(pPngStruct, pPngInfo);

	if(ByteLoader.m_Err != 0)
	{
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		dbg_msg("png", "byte loader error.");
		return false;
	}

	Width = png_get_image_width(pPngStruct, pPngInfo);
	Height = png_get_image_height(pPngStruct, pPngInfo);
	const int ColorType = png_get_color_type(pPngStruct, pPngInfo);
	const png_byte BitDepth = png_get_bit_depth(pPngStruct, pPngInfo);
	PngliteIncompatible = PngliteIncompatibility(pPngStruct, pPngInfo);

	if(BitDepth == 16)
	{
		png_set_strip_16(pPngStruct);
	}
	else if(BitDepth > 8)
	{
		dbg_msg("png", "non supported bit depth.");
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		return false;
	}

	if(Width == 0 || Height == 0 || BitDepth == 0)
	{
		dbg_msg("png", "image had width, height or bit depth of 0.");
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		return false;
	}

	if(ColorType == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(pPngStruct);

	if(ColorType == PNG_COLOR_TYPE_GRAY && BitDepth < 8)
		png_set_expand_gray_1_2_4_to_8(pPngStruct);

	if(png_get_valid(pPngStruct, pPngInfo, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(pPngStruct);

	png_read_update_info(pPngStruct, pPngInfo);

	const size_t ColorChannelCount = png_get_channels(pPngStruct, pPngInfo);
	const size_t BytesInRow = png_get_rowbytes(pPngStruct, pPngInfo);
	dbg_assert(BytesInRow == Width * ColorChannelCount, "bytes in row incorrect.");

	pRowPointers = new png_bytep[Height];
	for(size_t y = 0; y < Height; ++y)
	{
		pRowPointers[y] = new png_byte[BytesInRow];
	}

	png_read_image(pPngStruct, pRowPointers);

	if(ByteLoader.m_Err == 0)
		pImageBuff = (uint8_t *)malloc(Height * Width * ColorChannelCount * sizeof(uint8_t));

	for(size_t i = 0; i < Height; ++i)
	{
		if(ByteLoader.m_Err == 0)
			mem_copy(&pImageBuff[i * BytesInRow], pRowPointers[i], BytesInRow);
		delete[] pRowPointers[i];
	}
	delete[] pRowPointers;
	pRowPointers = nullptr;

	if(ByteLoader.m_Err != 0)
	{
		LibPngDeleteReadStruct(pPngStruct, pPngInfo);
		dbg_msg("png", "byte loader error.");
		return false;
	}

	ImageFormat = LibPngGetImageFormat(ColorChannelCount);

	png_destroy_info_struct(pPngStruct, &pPngInfo);
	png_destroy_read_struct(&pPngStruct, nullptr, nullptr);

	return true;
}

static void WriteDataFromLoadedBytes(png_structp pPngStruct, png_bytep pOutBytes, png_size_t ByteCountToWrite)
{
	if(ByteCountToWrite > 0)
	{
		png_voidp pIO_Ptr = png_get_io_ptr(pPngStruct);

		SImageByteBuffer *pByteLoader = (SImageByteBuffer *)pIO_Ptr;

		size_t NewSize = pByteLoader->m_LoadOffset + (size_t)ByteCountToWrite;
		pByteLoader->m_pvLoadedImageBytes->resize(NewSize);

		mem_copy(&(*pByteLoader->m_pvLoadedImageBytes)[pByteLoader->m_LoadOffset], pOutBytes, (size_t)ByteCountToWrite);
		pByteLoader->m_LoadOffset = NewSize;
	}
}

static void FlushPngWrite(png_structp png_ptr) {}

static size_t ImageLoaderHelperFormatToColorChannel(EImageFormat Format)
{
	switch(Format)
	{
	case IMAGE_FORMAT_R:
		return 1;
	case IMAGE_FORMAT_RA:
		return 2;
	case IMAGE_FORMAT_RGB:
		return 3;
	case IMAGE_FORMAT_RGBA:
		return 4;
	default:
		dbg_assert(false, "Format invalid");
		dbg_break();
	}
}

bool SavePng(EImageFormat ImageFormat, const uint8_t *pRawBuffer, SImageByteBuffer &WrittenBytes, size_t Width, size_t Height)
{
	png_structp pPngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if(pPngStruct == nullptr)
	{
		dbg_msg("png", "libpng internal failure: png_create_write_struct failed.");
		return false;
	}

	png_infop pPngInfo = png_create_info_struct(pPngStruct);

	if(pPngInfo == nullptr)
	{
		png_destroy_read_struct(&pPngStruct, nullptr, nullptr);
		dbg_msg("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	WrittenBytes.m_LoadOffset = 0;
	WrittenBytes.m_pvLoadedImageBytes->clear();

	png_set_write_fn(pPngStruct, (png_bytep)&WrittenBytes, WriteDataFromLoadedBytes, FlushPngWrite);

	int ColorType = PNG_COLOR_TYPE_RGB;
	size_t WriteBytesPerPixel = ImageLoaderHelperFormatToColorChannel(ImageFormat);
	if(ImageFormat == IMAGE_FORMAT_R)
	{
		ColorType = PNG_COLOR_TYPE_GRAY;
	}
	else if(ImageFormat == IMAGE_FORMAT_RGBA)
	{
		ColorType = PNG_COLOR_TYPE_RGBA;
	}

	png_set_IHDR(pPngStruct, pPngInfo, Width, Height, 8, ColorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(pPngStruct, pPngInfo);

	png_bytepp pRowPointers = new png_bytep[Height];
	size_t WidthBytes = Width * WriteBytesPerPixel;
	ptrdiff_t BufferOffset = 0;
	for(size_t y = 0; y < Height; ++y)
	{
		pRowPointers[y] = new png_byte[WidthBytes];
		mem_copy(pRowPointers[y], pRawBuffer + BufferOffset, WidthBytes);
		BufferOffset += (ptrdiff_t)WidthBytes;
	}
	png_write_image(pPngStruct, pRowPointers);

	png_write_end(pPngStruct, pPngInfo);

	for(size_t y = 0; y < Height; ++y)
	{
		delete[](pRowPointers[y]);
	}
	delete[](pRowPointers);

	png_destroy_info_struct(pPngStruct, &pPngInfo);
	png_destroy_write_struct(&pPngStruct, nullptr);

	return true;
}
