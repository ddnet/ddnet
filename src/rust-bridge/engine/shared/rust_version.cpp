#include "base/rust.h"
#include "engine/console.h"

extern "C" {
void cxxbridge1$RustVersionPrint(::IConsole const &console) noexcept;

void cxxbridge1$RustVersionRegister(::IConsole &console) noexcept;
} // extern "C"

void RustVersionPrint(::IConsole const &console) noexcept {
  cxxbridge1$RustVersionPrint(console);
}

void RustVersionRegister(::IConsole &console) noexcept {
  cxxbridge1$RustVersionRegister(console);
}
