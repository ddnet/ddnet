/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>
#include <engine/shared/dilate.h>
#include <pnglite.h>

int DilateFile(const char *pFileName)
{
	png_t Png;

	png_init(0, 0);
	int Error = png_open_file(&Png, pFileName);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("dilate", "failed to open image file. filename='%s', pnglite: %s", pFileName, png_error_string(Error));
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png);
		return 0;
	}

	if(Png.color_type != PNG_TRUECOLOR_ALPHA)
	{
		dbg_msg("dilate", "%s: not an RGBA image", pFileName);
		return 1;
	}

	unsigned char *pBuffer = (unsigned char *)malloc((size_t)Png.width * Png.height * sizeof(unsigned char) * 4);

	Error = png_get_data(&Png, pBuffer);
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("map_convert_07", "failed to read image. filename='%s', pnglite: %s", pFileName, png_error_string(Error));
		free(pBuffer);
		png_close_file(&Png);
		return 0;
	}
	png_close_file(&Png);

	int w = Png.width;
	int h = Png.height;

	DilateImage(pBuffer, w, h, 4);

	// save here
	png_open_file_write(&Png, pFileName);
	png_set_data(&Png, w, h, 8, PNG_TRUECOLOR_ALPHA, (unsigned char *)pBuffer);
	png_close_file(&Png);

	free(pBuffer);

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
