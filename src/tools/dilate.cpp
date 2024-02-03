/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>

int DilateFile(const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		long int FileSize = io_tell(File);
		if(FileSize <= 0)
		{
			io_close(File);
			dbg_msg("dilate", "failed to get file size (%ld). filename='%s'", FileSize, pFilename);
			return false;
		}
		io_seek(File, 0, IOSEEK_START);
		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		CImageInfo Img;

		uint8_t *pImgBuffer = NULL;
		EImageFormat ImageFormat;
		int PngliteIncompatible;
		if(LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, Img.m_Width, Img.m_Height, pImgBuffer, ImageFormat))
		{
			if(ImageFormat != IMAGE_FORMAT_RGBA)
			{
				free(pImgBuffer);
				dbg_msg("dilate", "%s: not an RGBA image", pFilename);
				return -1;
			}

			Img.m_pData = pImgBuffer;

			unsigned char *pBuffer = (unsigned char *)Img.m_pData;

			int w = Img.m_Width;
			int h = Img.m_Height;

			DilateImage(pBuffer, w, h);

			// save here
			IOHANDLE SaveFile = io_open(pFilename, IOFLAG_WRITE);
			if(SaveFile)
			{
				TImageByteBuffer ByteBuffer2;
				SImageByteBuffer ImageByteBuffer2(&ByteBuffer2);

				if(SavePNG(IMAGE_FORMAT_RGBA, (const uint8_t *)pBuffer, ImageByteBuffer2, w, h))
					io_write(SaveFile, &ByteBuffer2.front(), ByteBuffer2.size());
				io_close(SaveFile);

				free(pBuffer);
			}
		}
		else
		{
			dbg_msg("dilate", "failed unknown image format: %s", pFilename);
			return -1;
		}
	}
	else
	{
		dbg_msg("dilate", "failed to open image file. filename='%s'", pFilename);
		return -1;
	}

	return 0;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();
	if(argc == 1)
	{
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", argv[0]);
		return -1;
	}

	for(int i = 1; i < argc; i++)
		DilateFile(argv[i]);

	return 0;
}
