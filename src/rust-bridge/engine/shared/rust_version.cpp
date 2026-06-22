#include "base/rust.h"
#include "engine/console.h"

extern "C" {
void cxxbridge1$RustVersionPrint(const ::IConsole &console) noexcept;

void cxxbridge1$RustVersionRegister(::IConsole &console) noexcept;
} // extern "C"

void RustVersionPrint(const ::IConsole &console) noexcept {
  cxxbridge1$RustVersionPrint(console);
}

void RustVersionRegister(::IConsole &console) noexcept {
  cxxbridge1$RustVersionRegister(console);
}
