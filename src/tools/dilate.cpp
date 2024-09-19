/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/logger.h>
#include <base/system.h>

#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>

static bool DilateFile(const char *pFilename)
{
	CImageInfo Image;
	int PngliteIncompatible;
	if(!CImageLoader::LoadPng(io_open(pFilename, IOFLAG_READ), pFilename, Image, PngliteIncompatible))
		return false;

	if(Image.m_Format != CImageInfo::FORMAT_RGBA)
	{
		log_error("dilate", "ERROR: only RGBA PNG images are supported");
		Image.Free();
		return false;
	}

	DilateImage(Image);

	const bool SaveResult = CImageLoader::SavePng(io_open(pFilename, IOFLAG_WRITE), pFilename, Image);
	Image.Free();

	return SaveResult;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc == 1)
	{
		log_error("dilate", "Usage: %s <image1.png> [<image2.png> ...]", argv[0]);
		return -1;
	}

	bool Success = true;
	for(int i = 1; i < argc; i++)
	{
		Success &= DilateFile(argv[i]);
	}
	return Success ? 0 : -1;
}
