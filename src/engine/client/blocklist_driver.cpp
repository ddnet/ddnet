#include "blocklist_driver.h"

#include <base/system.h>

#include <cstddef>

#define VERSION_PARTS 4

struct SVersion
{
	int m_aParts[VERSION_PARTS] = {0};

	bool operator<=(const SVersion &Other) const
	{
		for(int i = 0; i < VERSION_PARTS; i++)
		{
			if(m_aParts[i] < Other.m_aParts[i])
				return true;
			if(m_aParts[i] > Other.m_aParts[i])
				return false;
		}
		return true;
	}
};

enum EBackendDriverBlockListType
{
	BACKEND_DRIVER_BLOCKLIST_TYPE_VERSION = 0,
	BACKEND_DRIVER_BLOCKLIST_TYPE_VENDOR,
};

/* TODO: generalize it more for other drivers / vendors */
struct SBackEndDriverBlockList
{
	EBackendDriverBlockListType m_BlockListType = BACKEND_DRIVER_BLOCKLIST_TYPE_VERSION;

	SVersion m_VersionMin;
	SVersion m_VersionMax;

	const char *m_pVendorName = nullptr;

	// the OpenGL version, that is supported
	int m_AllowedMajor = 0;
	int m_AllowedMinor = 0;
	int m_AllowedPatch = 0;

	const char *m_pReason = nullptr;

	bool m_DisplayReason = false;
	const char *m_pOSName = nullptr;
};

static SBackEndDriverBlockList gs_aBlockList[] = {
	{BACKEND_DRIVER_BLOCKLIST_TYPE_VENDOR, {26, 20, 100, 7800}, {27, 20, 100, 8853}, "Intel", 2, 0, 0, "This Intel driver version can cause crashes, please update it to a newer version.", false, "windows"}};

const char *ParseBlocklistDriverVersions(const char *pVendorStr, const char *pVersionStr, int &BlocklistMajor, int &BlocklistMinor, int &BlocklistPatch, bool &RequiresWarning)
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
	for(int &VersionPart : Version.m_aParts)
	{
		pVersionStrStart = str_next_token(pVersionStrStart, ".", aVersionStrHelper, sizeof(aVersionStrHelper));
		if(pVersionStrStart == NULL)
			return NULL;

		VersionPart = str_toint(aVersionStrHelper);
	}

	for(const auto &BlockListItem : gs_aBlockList)
	{
		if(str_comp(BlockListItem.m_pOSName, CONF_FAMILY_STRING) == 0)
		{
			bool DriverBlocked = false;
			if(BlockListItem.m_BlockListType == BACKEND_DRIVER_BLOCKLIST_TYPE_VENDOR)
			{
				if(str_find_nocase(pVendorStr, BlockListItem.m_pVendorName) != NULL)
				{
					DriverBlocked = true;
				}
			}
			else if(BlockListItem.m_BlockListType == BACKEND_DRIVER_BLOCKLIST_TYPE_VERSION)
			{
				if(BlockListItem.m_VersionMin <= Version && Version <= BlockListItem.m_VersionMax)
				{
					DriverBlocked = true;
				}
			}

			if(DriverBlocked)
			{
				RequiresWarning = BlockListItem.m_DisplayReason;
				BlocklistMajor = BlockListItem.m_AllowedMajor;
				BlocklistMinor = BlockListItem.m_AllowedMinor;
				BlocklistPatch = BlockListItem.m_AllowedPatch;
				return BlockListItem.m_pReason;
			}
		}
	}

	return NULL;
}
