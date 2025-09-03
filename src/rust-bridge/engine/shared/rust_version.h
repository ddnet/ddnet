#pragma once
#include "base/rust.h"
#include "engine/console.h"

void RustVersionPrint(::IConsole const &console) noexcept;

void RustVersionRegister(::IConsole &console) noexcept;
