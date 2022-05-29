/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/image_manipulation.h>
#include <pnglite.h>

int DilateFile(const char *pFilename)
{
	png_t Png;

	png_init(0, 0);

	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(!File)
	{
		dbg_msg("dilate", "failed to open file. filename='%s'", pFilename);
		return 0;
	}
	int Error = png_open_read(&Png, 0, File);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("dilate", "failed to open image file. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		io_close(File);
		return 0;
	}

	if(Png.color_type != PNG_TRUECOLOR_ALPHA)
	{
		dbg_msg("dilate", "%s: not an RGBA image", pFilename);
		return 1;
	}

	unsigned char *pBuffer = (unsigned char *)malloc((size_t)Png.width * Png.height * sizeof(unsigned char) * 4);

	Error = png_get_data(&Png, pBuffer);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to read image. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		free(pBuffer);
		io_close(File);
		return 0;
	}
	io_close(File);

	int w = Png.width;
	int h = Png.height;

	DilateImage(pBuffer, w, h, 4);

	// save here
	File = io_open(pFilename, IOFLAG_WRITE);
	if(!File)
	{
		dbg_msg("dilate", "failed to open file. filename='%s'", pFilename);
		free(pBuffer);
		return 0;
	}
	Error = png_open_write(&Png, 0, File);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("dilate", "failed to open image file. filename='%s', pnglite: %s", pFilename, png_error_string(Error));
		io_close(File);
		return 0;
	}
	png_set_data(&Png, w, h, 8, PNG_TRUECOLOR_ALPHA, (unsigned char *)pBuffer);
	io_close(File);

	free(pBuffer);

	return 0;
}

int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);
	log_set_global_logger_default();
	if(argc == 1)
	{
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", argv[0]);
		return -1;
	}

	for(int i = 1; i < argc; i++)
		DilateFile(argv[i]);
	cmdline_free(argc, argv);
	return 0;
}
