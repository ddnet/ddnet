#ifndef ENGINE_CLIENT_BLOCKLIST_DRIVER_H
#define ENGINE_CLIENT_BLOCKLIST_DRIVER_H

const char *ParseBlocklistDriverVersions(const char *pVendorStr, const char *pVersionStr, int &BlocklistMajor, int &BlocklistMinor, int &BlocklistPatch, bool &RequiresWarning);

#endif // ENGINE_CLIENT_BLOCKLIST_DRIVER_H
