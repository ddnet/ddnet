#include "base/net.h"
#include <cstdint>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__
#endif // __GNUC__

extern "C" {
void cxxbridge1$194$net_addr_str(::NETADDR const *addr, char *string, ::std::int32_t max_length, bool add_port) noexcept {
  void (*net_addr_str$)(::NETADDR const *, char *, ::std::int32_t, bool) = ::net_addr_str;
  net_addr_str$(addr, string, max_length, add_port);
}

::std::int32_t cxxbridge1$194$net_addr_from_str(::NETADDR *addr, char const *string) noexcept {
  ::std::int32_t (*net_addr_from_str$)(::NETADDR *, char const *) = ::net_addr_from_str;
  return net_addr_from_str$(addr, string);
}
} // extern "C"
