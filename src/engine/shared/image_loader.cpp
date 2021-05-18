#include "image_loader.h"
#include <base/system.h>

#include <png.h>

struct SLibPNGWarningItem
{
	SImageByteBuffer *m_pByteLoader;
	const char *pFileName;
};

static void LibPNGError(png_structp png_ptr, png_const_charp error_msg)
{
	SLibPNGWarningItem *pUserStruct = (SLibPNGWarningItem *)png_get_error_ptr(png_ptr);
	pUserStruct->m_pByteLoader->m_Err = -1;
	dbg_msg("libpng", "error for file \"%s\": %s", pUserStruct->pFileName, error_msg);
}

static void LibPNGWarning(png_structp png_ptr, png_const_charp warning_msg)
{
	SLibPNGWarningItem *pUserStruct = (SLibPNGWarningItem *)png_get_error_ptr(png_ptr);
	dbg_msg("libpng", "warning for file \"%s\": %s", pUserStruct->pFileName, warning_msg);
}

static bool FileMatchesImageType(SImageByteBuffer &ByteLoader)
{
	if(ByteLoader.m_pLoadedImageBytes->size() >= 8)
		return png_sig_cmp((png_bytep) & (*ByteLoader.m_pLoadedImageBytes)[0], 0, 8) == 0;
	return false;
}

static void ReadDataFromLoadedBytes(png_structp pPNGStruct, png_bytep pOutBytes, png_size_t ByteCountToRead)
{
	png_voidp pIO_Ptr = png_get_io_ptr(pPNGStruct);

	SImageByteBuffer *pByteLoader = (SImageByteBuffer *)pIO_Ptr;

	if(pByteLoader->m_pLoadedImageBytes->size() >= pByteLoader->m_LoadOffset + (size_t)ByteCountToRead)
	{
		mem_copy(pOutBytes, &(*pByteLoader->m_pLoadedImageBytes)[pByteLoader->m_LoadOffset], (size_t)ByteCountToRead);

		pByteLoader->m_LoadOffset += (size_t)ByteCountToRead;
	}
	else
	{
		pByteLoader->m_Err = -1;
		dbg_msg("png", "could not read bytes, file was too small.");
	}
}

static int LibPNGGetColorChannelCount(int LibPNGColorType)
{
	if(LibPNGColorType == PNG_COLOR_TYPE_GRAY)
		return 1;
	else if(LibPNGColorType == PNG_COLOR_TYPE_PALETTE || LibPNGColorType == PNG_COLOR_TYPE_RGB)
		return 3;
	else if(LibPNGColorType == PNG_COLOR_TYPE_RGBA)
		return 4;

	return 4;
}

static void LibPNGSetImageFormat(EImageFormat &ImageFormat, int LibPNGColorType)
{
	ImageFormat = IMAGE_FORMAT_RGBA;
	if(LibPNGColorType == PNG_COLOR_TYPE_GRAY)
		ImageFormat = IMAGE_FORMAT_R;
	else if(LibPNGColorType == PNG_COLOR_TYPE_PALETTE || LibPNGColorType == PNG_COLOR_TYPE_RGB)
		ImageFormat = IMAGE_FORMAT_RGB;
	else if(LibPNGColorType == PNG_COLOR_TYPE_RGBA)
		ImageFormat = IMAGE_FORMAT_RGBA;
}

static void LibPNGDeleteReadStruct(png_structp pPNGStruct, png_infop pPNGInfo)
{
	png_destroy_info_struct(pPNGStruct, &pPNGInfo);
	png_destroy_read_struct(&pPNGStruct, NULL, NULL);
}

bool LoadPNG(SImageByteBuffer &ByteLoader, const char *pFileName, int &Width, int &Height, uint8_t *&pImageBuff, EImageFormat &ImageFormat)
{
	png_structp pPNGStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(pPNGStruct == NULL)
	{
		dbg_msg("png", "libpng internal failure: png_create_read_struct failed.");
		return false;
	}

	png_infop pPNGInfo = png_create_info_struct(pPNGStruct);

	if(pPNGInfo == NULL)
	{
		png_destroy_read_struct(&pPNGStruct, NULL, NULL);
		dbg_msg("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	SLibPNGWarningItem UserErrorStruct = {&ByteLoader, pFileName};
	png_set_error_fn(pPNGStruct, &UserErrorStruct, LibPNGError, LibPNGWarning);

	if(!FileMatchesImageType(ByteLoader))
	{
		LibPNGDeleteReadStruct(pPNGStruct, pPNGInfo);
		return false;
	}

	ByteLoader.m_LoadOffset = 8;

	png_set_read_fn(pPNGStruct, (png_bytep)&ByteLoader, ReadDataFromLoadedBytes);

	png_set_sig_bytes(pPNGStruct, 8);

	png_read_info(pPNGStruct, pPNGInfo);

	if(ByteLoader.m_Err != 0)
	{
		LibPNGDeleteReadStruct(pPNGStruct, pPNGInfo);
		return false;
	}

	Width = png_get_image_width(pPNGStruct, pPNGInfo);
	Height = png_get_image_height(pPNGStruct, pPNGInfo);
	int ColorType = png_get_color_type(pPNGStruct, pPNGInfo);
	png_byte BitDepth = png_get_bit_depth(pPNGStruct, pPNGInfo);

	bool PNGErr = false;

	if(BitDepth == 16)
		png_set_strip_16(pPNGStruct);
	else if(BitDepth > 8)
	{
		dbg_msg("png", "non supported bit depth.");
		PNGErr = true;
	}

	if(Width == 0 || Height == 0 || BitDepth == 0)
	{
		dbg_msg("png", "image had width or height of 0.");
		PNGErr = true;
	}

	if(!PNGErr)
	{
		if(ColorType == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(pPNGStruct);

		if(ColorType == PNG_COLOR_TYPE_GRAY && BitDepth < 8)
			png_set_expand_gray_1_2_4_to_8(pPNGStruct);

		if(png_get_valid(pPNGStruct, pPNGInfo, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(pPNGStruct);

		png_read_update_info(pPNGStruct, pPNGInfo);

		int ColorChannelCount = LibPNGGetColorChannelCount(ColorType);
		int BytesInRow = png_get_rowbytes(pPNGStruct, pPNGInfo);

		if(BytesInRow == Width * ColorChannelCount)
		{
			png_bytepp pRowPointers = new png_bytep[Height];
			for(int y = 0; y < Height; ++y)
			{
				pRowPointers[y] = new png_byte[BytesInRow];
			}

			png_read_image(pPNGStruct, pRowPointers);

			if(ByteLoader.m_Err == 0)
				pImageBuff = (uint8_t *)malloc(Height * Width * ColorChannelCount * sizeof(uint8_t));
			else
				PNGErr = true;

			for(int i = 0; i < Height; ++i)
			{
				if(ByteLoader.m_Err == 0)
					mem_copy(&pImageBuff[i * BytesInRow], pRowPointers[i], BytesInRow);
				delete[] pRowPointers[i];
			}
			delete[] pRowPointers;

			LibPNGSetImageFormat(ImageFormat, ColorType);
		}
		else
			PNGErr = true;
	}

	png_destroy_info_struct(pPNGStruct, &pPNGInfo);
	png_destroy_read_struct(&pPNGStruct, NULL, NULL);

	return !PNGErr;
}

static void WriteDataFromLoadedBytes(png_structp pPNGStruct, png_bytep pOutBytes, png_size_t ByteCountToWrite)
{
	if(ByteCountToWrite > 0)
	{
		png_voidp pIO_Ptr = png_get_io_ptr(pPNGStruct);

		SImageByteBuffer *pByteLoader = (SImageByteBuffer *)pIO_Ptr;

		size_t NewSize = pByteLoader->m_LoadOffset + (size_t)ByteCountToWrite;
		pByteLoader->m_pLoadedImageBytes->resize(NewSize);

		mem_copy(&(*pByteLoader->m_pLoadedImageBytes)[pByteLoader->m_LoadOffset], pOutBytes, (size_t)ByteCountToWrite);
		pByteLoader->m_LoadOffset = NewSize;
	}
}

static void FlushPNGWrite(png_structp png_ptr) {}

static int ImageLoaderHelperFormatToColorChannel(EImageFormat Format)
{
	if(Format == IMAGE_FORMAT_R)
		return 1;
	else if(Format == IMAGE_FORMAT_RGB)
		return 3;
	else if(Format == IMAGE_FORMAT_RGBA)
		return 4;

	return 4;
}

bool SavePNG(EImageFormat ImageFormat, const uint8_t *pRawBuffer, SImageByteBuffer &WrittenBytes, int Width, int Height)
{
	png_structp pPNGStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(pPNGStruct == NULL)
	{
		dbg_msg("png", "libpng internal failure: png_create_write_struct failed.");
		return false;
	}

	png_infop pPNGInfo = png_create_info_struct(pPNGStruct);

	if(pPNGInfo == NULL)
	{
		png_destroy_read_struct(&pPNGStruct, NULL, NULL);
		dbg_msg("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	WrittenBytes.m_LoadOffset = 0;
	WrittenBytes.m_pLoadedImageBytes->clear();

	png_set_write_fn(pPNGStruct, (png_bytep)&WrittenBytes, WriteDataFromLoadedBytes, FlushPNGWrite);

	int ColorType = PNG_COLOR_TYPE_RGB;
	int WriteBytesPerPixel = ImageLoaderHelperFormatToColorChannel(ImageFormat);
	if(ImageFormat == IMAGE_FORMAT_R)
	{
		ColorType = PNG_COLOR_TYPE_GRAY;
	}
	else if(ImageFormat == IMAGE_FORMAT_RGBA)
	{
		ColorType = PNG_COLOR_TYPE_RGBA;
	}

	png_set_IHDR(pPNGStruct, pPNGInfo, Width, Height, 8, ColorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(pPNGStruct, pPNGInfo);

	png_bytepp pRowPointers = new png_bytep[Height];
	int WidthBytes = Width * WriteBytesPerPixel;
	ptrdiff_t BufferOffset = 0;
	for(int y = 0; y < Height; ++y)
	{
		pRowPointers[y] = new png_byte[WidthBytes];
		mem_copy(pRowPointers[y], pRawBuffer + BufferOffset, WidthBytes);
		BufferOffset += (ptrdiff_t)WidthBytes;
	}
	png_write_image(pPNGStruct, pRowPointers);

	png_write_end(pPNGStruct, pPNGInfo);

	for(int y = 0; y < Height; ++y)
	{
		delete[](pRowPointers[y]);
	}
	delete[](pRowPointers);

	png_destroy_info_struct(pPNGStruct, &pPNGInfo);
	png_destroy_write_struct(&pPNGStruct, NULL);

	return true;
}
