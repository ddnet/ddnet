#pragma once
#include "base/rust.h"
#include "engine/console.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__

void RustVersionPrint(::IConsole const &console) noexcept;

void RustVersionRegister(::IConsole &console) noexcept;

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
