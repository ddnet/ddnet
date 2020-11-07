#include "blocklist_driver.h"

#include <base/system.h>

#define VERSION_PARTS 4

struct SVersion
{
	int m_Parts[VERSION_PARTS];

	bool operator<=(const SVersion &Other) const
	{
		for(int i = 0; i < VERSION_PARTS; i++)
		{
			if(m_Parts[i] < Other.m_Parts[i])
				return true;
			if(m_Parts[i] > Other.m_Parts[i])
				return false;
		}
		return true;
	}
};

/* TODO: generalize it more for other drivers / vendors */
struct SBackEndDriverBlockList
{
	SVersion m_VersionMin;
	SVersion m_VersionMax;

	// the OpenGL version, that is supported
	int m_AllowedMajor;
	int m_AllowedMinor;
	int m_AllowedPatch;

	const char *m_pReason;
};

static SBackEndDriverBlockList gs_aBlockList[] = {
	{{26, 20, 100, 7800}, {26, 20, 100, 7999}, 2, 0, 0, "This Intel driver version can cause crashes, please update it to a newer version."}};

const char *ParseBlocklistDriverVersions(const char *pVendorStr, const char *pVersionStr, int &BlocklistMajor, int &BlocklistMinor, int &BlocklistPatch)
{
	if(str_find_nocase(pVendorStr, "Intel") == NULL)
		return NULL;

	const char *pVersionStrStart = str_find_nocase(pVersionStr, "Build ");
	if(pVersionStrStart == NULL)
		return NULL;

	// ignore "Build ", after that, it should directly start with the driver version
	pVersionStrStart += (ptrdiff_t)str_length("Build ");

	char aVersionStrHelper[512]; // the size is random, but shouldn't be too small probably

	SVersion Version;
	for(int &VersionPart : Version.m_Parts)
	{
		pVersionStrStart = str_next_token(pVersionStrStart, ".", aVersionStrHelper, sizeof(aVersionStrHelper));
		if(pVersionStrStart == NULL)
			return NULL;

		VersionPart = str_toint(aVersionStrHelper);
	}

	for(const auto &BlockListItem : gs_aBlockList)
	{
		if(BlockListItem.m_VersionMin <= Version && Version <= BlockListItem.m_VersionMax)
		{
			BlocklistMajor = BlockListItem.m_AllowedMajor;
			BlocklistMinor = BlockListItem.m_AllowedMinor;
			BlocklistPatch = BlockListItem.m_AllowedPatch;
			return BlockListItem.m_pReason;
		}
	}

	return NULL;
}
