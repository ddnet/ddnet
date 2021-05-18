/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "engine/graphics.h"
#include <base/math.h>
#include <base/system.h>
#include <engine/shared/image_loader.h>
#include <engine/shared/image_manipulation.h>

int DilateFile(const char *pFileName)
{
	IOHANDLE File = io_open(pFileName, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		unsigned int FileSize = io_tell(File);
		io_seek(File, 0, IOSEEK_START);
		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		CImageInfo Img;

		uint8_t *pImgBuffer = NULL;
		EImageFormat ImageFormat;
		if(LoadPNG(ImageByteBuffer, pFileName, Img.m_Width, Img.m_Height, pImgBuffer, ImageFormat))
		{
			if(ImageFormat != IMAGE_FORMAT_RGBA)
			{
				free(pImgBuffer);
				dbg_msg("dilate", "%s: not an RGBA image", pFileName);
				return -1;
			}

			Img.m_pData = pImgBuffer;

			unsigned char *pBuffer = (unsigned char *)Img.m_pData;

			int w = Img.m_Width;
			int h = Img.m_Height;

			DilateImage(pBuffer, w, h, 4);

			// save here
			IOHANDLE SaveFile = io_open(pFileName, IOFLAG_WRITE);
			if(SaveFile)
			{
				TImageByteBuffer ByteBuffer;
				SImageByteBuffer ImageByteBuffer(&ByteBuffer);

				if(SavePNG(IMAGE_FORMAT_RGBA, (const uint8_t *)pBuffer, ImageByteBuffer, w, h))
					io_write(SaveFile, &ByteBuffer.front(), ByteBuffer.size());
				io_close(SaveFile);

				free(pBuffer);
			}
		}
		else
		{
			dbg_msg("dilate", "failed unknown image format: %s", pFileName);
			return -1;
		}
	}
	else
	{
		dbg_msg("dilate", "failed to open image file. filename='%s'", pFileName);
		return -1;
	}

	return 0;
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	if(argc == 1)
	{
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", argv[0]);
		return -1;
	}

	for(int i = 1; i < argc; i++)
		DilateFile(argv[i]);
	return 0;
}
