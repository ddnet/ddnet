/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/logger.h>
#include <base/system.h>

#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>

static bool DilateFile(const char *pFilename, bool DryRun)
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

	if(DryRun)
	{
		CImageInfo OldImage = Image.DeepCopy();
		DilateImage(Image);
		const bool EqualImages = Image.DataEquals(OldImage);
		Image.Free();
		OldImage.Free();
		log_info("dilate", "'%s' is %sdilated", pFilename, EqualImages ? "" : "NOT ");
		return EqualImages;
	}
	else
	{
		DilateImage(Image);
		const bool SaveResult = CImageLoader::SavePng(io_open(pFilename, IOFLAG_WRITE), pFilename, Image);
		Image.Free();
		return SaveResult;
	}
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc == 1)
	{
		log_error("dilate", "Usage: %s [--dry-run] <image1.png> [<image2.png> ...]", argv[0]);
		return -1;
	}

	const bool DryRun = str_comp(argv[1], "--dry-run") == 0;
	if(DryRun && argc < 3)
	{
		log_error("dilate", "Usage: %s [--dry-run] <image1.png> [<image2.png> ...]", argv[0]);
		return -1;
	}

	bool Success = true;
	for(int i = (DryRun ? 2 : 1); i < argc; i++)
	{
		Success &= DilateFile(argv[i], DryRun);
	}
	return Success ? 0 : -1;
}
