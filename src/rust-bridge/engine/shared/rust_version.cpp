#include "base/rust.h"
#include "engine/console.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

extern "C" {
void cxxbridge1$194$RustVersionPrint(::IConsole const &console) noexcept;

void cxxbridge1$194$RustVersionRegister(::IConsole &console) noexcept;
} // extern "C"

void RustVersionPrint(::IConsole const &console) noexcept {
  cxxbridge1$194$RustVersionPrint(console);
}

void RustVersionRegister(::IConsole &console) noexcept {
  cxxbridge1$194$RustVersionRegister(console);
}
