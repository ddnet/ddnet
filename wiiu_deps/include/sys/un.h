/* Wii U stub for sys/un.h (Unix domain sockets)
 * devkitPPC newlib does not support AF_UNIX sockets.
 * This stub provides the minimum required types so the code compiles.
 * The net_unix_* functions will be stubbed out in net.cpp via __WIIU__ guards.
 */
#ifndef _SYS_UN_H
#define _SYS_UN_H

#include <sys/socket.h>

struct sockaddr_un {
    unsigned short sun_family;  /* AF_UNIX */
    char           sun_path[108]; /* pathname */
};

#endif /* _SYS_UN_H */
