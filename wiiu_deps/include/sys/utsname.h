/* Wii U stub for sys/utsname.h
 * devkitPPC newlib does not have uname().
 * This stub provides the minimum required types and a no-op uname().
 */
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static inline int uname(struct utsname *u)
{
    if(!u) return -1;
    __builtin_memcpy(u->sysname,  "WiiU",    5);
    __builtin_memcpy(u->nodename, "wiiu",    5);
    __builtin_memcpy(u->release,  "1.0.0",   6);
    __builtin_memcpy(u->version,  "CafeOS",  7);
    __builtin_memcpy(u->machine,  "powerpc", 8);
    return 0;
}

#endif /* _SYS_UTSNAME_H */
