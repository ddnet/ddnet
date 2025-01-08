#include "image_loader.h"

#include <base/log.h>
#include <base/system.h>

#include <csetjmp>
#include <cstdlib>

#include <png.h>

bool CByteBufferReader::Read(void *pData, size_t Size)
{
	if(m_Error)
		return false;

	if(m_ReadOffset + Size <= m_Size)
	{
		mem_copy(pData, &m_pData[m_ReadOffset], Size);
		m_ReadOffset += Size;
		return true;
	}
	else
	{
		m_Error = true;
		return false;
	}
}

void CByteBufferWriter::Write(const void *pData, size_t Size)
{
	if(!Size)
		return;

	const size_t WriteOffset = m_vBuffer.size();
	m_vBuffer.resize(WriteOffset + Size);
	mem_copy(&m_vBuffer[WriteOffset], pData, Size);
}

class CUserErrorStruct
{
public:
	CByteBufferReader *m_pReader;
	const char *m_pContextName;
	std::jmp_buf m_JmpBuf;
};

[[noreturn]] static void PngErrorCallback(png_structp pPngStruct, png_const_charp pErrorMessage)
{
	CUserErrorStruct *pUserStruct = static_cast<CUserErrorStruct *>(png_get_error_ptr(pPngStruct));
	log_error("png", "error for file \"%s\": %s", pUserStruct->m_pContextName, pErrorMessage);
	std::longjmp(pUserStruct->m_JmpBuf, 1);
}

static void PngWarningCallback(png_structp pPngStruct, png_const_charp pWarningMessage)
{
	CUserErrorStruct *pUserStruct = static_cast<CUserErrorStruct *>(png_get_error_ptr(pPngStruct));
	log_warn("png", "warning for file \"%s\": %s", pUserStruct->m_pContextName, pWarningMessage);
}

static void PngReadDataCallback(png_structp pPngStruct, png_bytep pOutBytes, png_size_t ByteCountToRead)
{
	CByteBufferReader *pReader = static_cast<CByteBufferReader *>(png_get_io_ptr(pPngStruct));
	if(!pReader->Read(pOutBytes, ByteCountToRead))
	{
		png_error(pPngStruct, "Could not read all bytes, file was too small");
	}
}

static CImageInfo::EImageFormat ImageFormatFromChannelCount(int ColorChannelCount)
{
	switch(ColorChannelCount)
	{
	case 1:
		return CImageInfo::FORMAT_R;
	case 2:
		return CImageInfo::FORMAT_RA;
	case 3:
		return CImageInfo::FORMAT_RGB;
	case 4:
		return CImageInfo::FORMAT_RGBA;
	default:
		dbg_assert(false, "ColorChannelCount invalid");
		dbg_break();
	}
}

static int PngliteIncompatibility(png_structp pPngStruct, png_infop pPngInfo)
{
	int Result = 0;

	const int ColorType = png_get_color_type(pPngStruct, pPngInfo);
	switch(ColorType)
	{
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_RGB:
	case PNG_COLOR_TYPE_RGB_ALPHA:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		break;
	default:
		log_debug("png", "color type %d unsupported by pnglite", ColorType);
		Result |= CImageLoader::PNGLITE_COLOR_TYPE;
	}

	const int BitDepth = png_get_bit_depth(pPngStruct, pPngInfo);
	switch(BitDepth)
	{
	case 8:
	case 16:
		break;
	default:
		log_debug("png", "bit depth %d unsupported by pnglite", BitDepth);
		Result |= CImageLoader::PNGLITE_BIT_DEPTH;
	}

	const int InterlaceType = png_get_interlace_type(pPngStruct, pPngInfo);
	if(InterlaceType != PNG_INTERLACE_NONE)
	{
		log_debug("png", "interlace type %d unsupported by pnglite", InterlaceType);
		Result |= CImageLoader::PNGLITE_INTERLACE_TYPE;
	}

	if(png_get_compression_type(pPngStruct, pPngInfo) != PNG_COMPRESSION_TYPE_BASE)
	{
		log_debug("png", "non-default compression type unsupported by pnglite");
		Result |= CImageLoader::PNGLITE_COMPRESSION_TYPE;
	}

	if(png_get_filter_type(pPngStruct, pPngInfo) != PNG_FILTER_TYPE_BASE)
	{
		log_debug("png", "non-default filter type unsupported by pnglite");
		Result |= CImageLoader::PNGLITE_FILTER_TYPE;
	}

	return Result;
}

bool CImageLoader::LoadPng(CByteBufferReader &Reader, const char *pContextName, CImageInfo &Image, int &PngliteIncompatible)
{
	CUserErrorStruct UserErrorStruct = {&Reader, pContextName, {}};

	png_structp pPngStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(pPngStruct == nullptr)
	{
		log_error("png", "libpng internal failure: png_create_read_struct failed.");
		return false;
	}

	png_infop pPngInfo = nullptr;
	png_bytepp pRowPointers = nullptr;
	int Height = 0; // ensure this is not undefined for the Cleanup function
	const auto &&Cleanup = [&]() {
		if(pRowPointers != nullptr)
		{
			for(int y = 0; y < Height; ++y)
			{
				delete[] pRowPointers[y];
			}
		}
		delete[] pRowPointers;
		if(pPngInfo != nullptr)
		{
			png_destroy_info_struct(pPngStruct, &pPngInfo);
		}
		png_destroy_read_struct(&pPngStruct, nullptr, nullptr);
	};
	if(setjmp(UserErrorStruct.m_JmpBuf))
	{
		Cleanup();
		return false;
	}
	png_set_error_fn(pPngStruct, &UserErrorStruct, PngErrorCallback, PngWarningCallback);

	pPngInfo = png_create_info_struct(pPngStruct);
	if(pPngInfo == nullptr)
	{
		Cleanup();
		log_error("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	png_byte aSignature[8];
	if(!Reader.Read(aSignature, sizeof(aSignature)) || png_sig_cmp(aSignature, 0, sizeof(aSignature)) != 0)
	{
		Cleanup();
		log_error("png", "file is not a valid PNG file (signature mismatch).");
		return false;
	}

	png_set_read_fn(pPngStruct, (png_bytep)&Reader, PngReadDataCallback);
	png_set_sig_bytes(pPngStruct, sizeof(aSignature));

	png_read_info(pPngStruct, pPngInfo);

	if(Reader.Error())
	{
		// error already logged
		Cleanup();
		return false;
	}

	const int Width = png_get_image_width(pPngStruct, pPngInfo);
	Height = png_get_image_height(pPngStruct, pPngInfo);
	const png_byte BitDepth = png_get_bit_depth(pPngStruct, pPngInfo);
	const int ColorType = png_get_color_type(pPngStruct, pPngInfo);

	if(Width == 0 || Height == 0)
	{
		log_error("png", "image has width (%d) or height (%d) of 0.", Width, Height);
		Cleanup();
		return false;
	}

	if(BitDepth == 16)
	{
		png_set_strip_16(pPngStruct);
	}
	else if(BitDepth > 8 || BitDepth == 0)
	{
		log_error("png", "bit depth %d not supported.", BitDepth);
		Cleanup();
		return false;
	}

	if(ColorType == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb(pPngStruct);
	}

	if(ColorType == PNG_COLOR_TYPE_GRAY && BitDepth < 8)
	{
		png_set_expand_gray_1_2_4_to_8(pPngStruct);
	}

	if(png_get_valid(pPngStruct, pPngInfo, PNG_INFO_tRNS))
	{
		png_set_tRNS_to_alpha(pPngStruct);
	}

	png_read_update_info(pPngStruct, pPngInfo);

	const int ColorChannelCount = png_get_channels(pPngStruct, pPngInfo);
	const int BytesInRow = png_get_rowbytes(pPngStruct, pPngInfo);
	dbg_assert(BytesInRow == Width * ColorChannelCount, "bytes in row incorrect.");

	pRowPointers = new png_bytep[Height];
	for(int y = 0; y < Height; ++y)
	{
		pRowPointers[y] = new png_byte[BytesInRow];
	}

	png_read_image(pPngStruct, pRowPointers);

	if(!Reader.Error())
	{
		Image.m_Width = Width;
		Image.m_Height = Height;
		Image.m_Format = ImageFormatFromChannelCount(ColorChannelCount);
		Image.m_pData = static_cast<uint8_t *>(malloc(Image.DataSize()));
		for(int y = 0; y < Height; ++y)
		{
			mem_copy(&Image.m_pData[y * BytesInRow], pRowPointers[y], BytesInRow);
		}
		PngliteIncompatible = PngliteIncompatibility(pPngStruct, pPngInfo);
	}

	Cleanup();

	return !Reader.Error();
}

bool CImageLoader::LoadPng(IOHANDLE File, const char *pFilename, CImageInfo &Image, int &PngliteIncompatible)
{
	if(!File)
	{
		log_error("png", "failed to open file for reading. filename='%s'", pFilename);
		return false;
	}

	void *pFileData;
	unsigned FileDataSize;
	const bool ReadSuccess = io_read_all(File, &pFileData, &FileDataSize);
	io_close(File);
	if(!ReadSuccess)
	{
		log_error("png", "failed to read file. filename='%s'", pFilename);
		return false;
	}

	CByteBufferReader ImageReader(static_cast<const uint8_t *>(pFileData), FileDataSize);

	const bool LoadResult = CImageLoader::LoadPng(ImageReader, pFilename, Image, PngliteIncompatible);
	free(pFileData);
	if(!LoadResult)
	{
		log_error("png", "failed to load image from file. filename='%s'", pFilename);
		return false;
	}

	if(Image.m_Format != CImageInfo::FORMAT_RGB && Image.m_Format != CImageInfo::FORMAT_RGBA)
	{
		log_error("png", "image has unsupported format. filename='%s' format='%s'", pFilename, Image.FormatName());
		Image.Free();
		return false;
	}

	return true;
}

static void PngWriteDataCallback(png_structp pPngStruct, png_bytep pOutBytes, png_size_t ByteCountToWrite)
{
	CByteBufferWriter *pWriter = static_cast<CByteBufferWriter *>(png_get_io_ptr(pPngStruct));
	pWriter->Write(pOutBytes, ByteCountToWrite);
}

static void PngOutputFlushCallback(png_structp pPngStruct)
{
	// no need to flush memory buffer
}

static int PngColorTypeFromFormat(CImageInfo::EImageFormat Format)
{
	switch(Format)
	{
	case CImageInfo::FORMAT_R:
		return PNG_COLOR_TYPE_GRAY;
	case CImageInfo::FORMAT_RA:
		return PNG_COLOR_TYPE_GRAY_ALPHA;
	case CImageInfo::FORMAT_RGB:
		return PNG_COLOR_TYPE_RGB;
	case CImageInfo::FORMAT_RGBA:
		return PNG_COLOR_TYPE_RGBA;
	default:
		dbg_assert(false, "Format invalid");
		dbg_break();
	}
}

bool CImageLoader::SavePng(CByteBufferWriter &Writer, const CImageInfo &Image)
{
	png_structp pPngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(pPngStruct == nullptr)
	{
		log_error("png", "libpng internal failure: png_create_write_struct failed.");
		return false;
	}

	png_infop pPngInfo = png_create_info_struct(pPngStruct);
	if(pPngInfo == nullptr)
	{
		png_destroy_read_struct(&pPngStruct, nullptr, nullptr);
		log_error("png", "libpng internal failure: png_create_info_struct failed.");
		return false;
	}

	png_set_write_fn(pPngStruct, (png_bytep)&Writer, PngWriteDataCallback, PngOutputFlushCallback);

	png_set_IHDR(pPngStruct, pPngInfo, Image.m_Width, Image.m_Height, 8, PngColorTypeFromFormat(Image.m_Format), PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(pPngStruct, pPngInfo);

	png_bytepp pRowPointers = new png_bytep[Image.m_Height];
	const int WidthBytes = Image.m_Width * Image.PixelSize();
	ptrdiff_t BufferOffset = 0;
	for(size_t y = 0; y < Image.m_Height; ++y)
	{
		pRowPointers[y] = new png_byte[WidthBytes];
		mem_copy(pRowPointers[y], Image.m_pData + BufferOffset, WidthBytes);
		BufferOffset += (ptrdiff_t)WidthBytes;
	}
	png_write_image(pPngStruct, pRowPointers);
	png_write_end(pPngStruct, pPngInfo);

	for(size_t y = 0; y < Image.m_Height; ++y)
	{
		delete[] pRowPointers[y];
	}
	delete[] pRowPointers;

	png_destroy_info_struct(pPngStruct, &pPngInfo);
	png_destroy_write_struct(&pPngStruct, nullptr);

	return true;
}

bool CImageLoader::SavePng(IOHANDLE File, const char *pFilename, const CImageInfo &Image)
{
	if(!File)
	{
		log_error("png", "failed to open file for writing. filename='%s'", pFilename);
		return false;
	}

	CByteBufferWriter Writer;
	if(!CImageLoader::SavePng(Writer, Image))
	{
		// error already logged
		io_close(File);
		return false;
	}

	const bool WriteSuccess = io_write(File, Writer.Data(), Writer.Size()) == Writer.Size();
	if(!WriteSuccess)
	{
		log_error("png", "failed to write PNG data to file. filename='%s'", pFilename);
	}
	io_close(File);
	return WriteSuccess;
}
