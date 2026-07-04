/* Wii U networking extensions stub.
 * The Wii U BSD socket stack does not support IPv6.
 * This header provides minimal stub types so IPv6-guarded code can still
 * reference the types without compile errors when __WIIU__ guards are used.
 *
 * Included via the WUT socket include path for Wii U builds.
 */
#ifndef _WIIU_NET_IPV6_STUB_H
#define _WIIU_NET_IPV6_STUB_H

#ifdef __WIIU__

/* AF_INET6 — not actually usable, but needed so #ifdef'd code compiles */
#ifndef AF_INET6
#define AF_INET6 28
#endif

/* Stub IPv6 structures */
struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    unsigned short  sin6_family;
    unsigned short  sin6_port;
    unsigned int    sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int    sin6_scope_id;
};

#endif /* __WIIU__ */
#endif /* _WIIU_NET_IPV6_STUB_H */
