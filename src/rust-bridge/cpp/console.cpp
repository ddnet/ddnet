#include "base/rust.h"
#include "engine/console.h"
#include "engine/rust.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace rust {
inline namespace cxxbridge1 {
// #include "rust/cxx.h"

#ifndef CXXBRIDGE1_IS_COMPLETE
#define CXXBRIDGE1_IS_COMPLETE
namespace detail {
namespace {
template <typename T, typename = std::size_t>
struct is_complete : std::false_type {};
template <typename T>
struct is_complete<T, decltype(sizeof(T))> : std::true_type {};
} // namespace
} // namespace detail
#endif // CXXBRIDGE1_IS_COMPLETE

#ifndef CXXBRIDGE1_RELOCATABLE
#define CXXBRIDGE1_RELOCATABLE
namespace detail {
template <typename... Ts>
struct make_void {
  using type = void;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

template <typename Void, template <typename...> class, typename...>
struct detect : std::false_type {};
template <template <typename...> class T, typename... A>
struct detect<void_t<T<A...>>, T, A...> : std::true_type {};

template <template <typename...> class T, typename... A>
using is_detected = detect<void, T, A...>;

template <typename T>
using detect_IsRelocatable = typename T::IsRelocatable;

template <typename T>
struct get_IsRelocatable
    : std::is_same<typename T::IsRelocatable, std::true_type> {};
} // namespace detail

template <typename T>
struct IsRelocatable
    : std::conditional<
          detail::is_detected<detail::detect_IsRelocatable, T>::value,
          detail::get_IsRelocatable<T>,
          std::integral_constant<
              bool, std::is_trivially_move_constructible<T>::value &&
                        std::is_trivially_destructible<T>::value>>::type {};
#endif // CXXBRIDGE1_RELOCATABLE

namespace {
template <bool> struct deleter_if {
  template <typename T> void operator()(T *) {}
};

template <> struct deleter_if<true> {
  template <typename T> void operator()(T *ptr) { ptr->~T(); }
};
} // namespace
} // namespace cxxbridge1
} // namespace rust

using IConsole_IResult = ::IConsole_IResult;
using IConsole = ::IConsole;

static_assert(
    ::rust::IsRelocatable<::ColorRGBA>::value,
    "type ColorRGBA should be trivially move constructible and trivially destructible in C++ to be used as an argument of `Print` in Rust");
static_assert(
    ::rust::IsRelocatable<::ColorHSLA>::value,
    "type ColorHSLA should be trivially move constructible and trivially destructible in C++ to be used as a return value of `GetColor` in Rust");
static_assert(
    ::rust::IsRelocatable<::StrRef>::value,
    "type StrRef should be trivially move constructible and trivially destructible in C++ to be used as an argument of `ExecuteLine`, `Print`, `Register` or return value of `GetString` in Rust");
static_assert(
    ::rust::IsRelocatable<::UserPtr>::value,
    "type UserPtr should be trivially move constructible and trivially destructible in C++ to be used as an argument of `Register` in Rust");
static_assert(
    ::rust::IsRelocatable<::IConsole_FCommandCallback>::value,
    "type IConsole_FCommandCallback should be trivially move constructible and trivially destructible in C++ to be used as an argument of `Register` in Rust");

extern "C" {
::std::int32_t cxxbridge1$IConsole_IResult$GetInteger(const ::IConsole_IResult &self, ::std::uint32_t Index) noexcept {
  ::std::int32_t (::IConsole_IResult::*GetInteger$)(::std::uint32_t) const = &::IConsole_IResult::GetInteger;
  return (self.*GetInteger$)(Index);
}

float cxxbridge1$IConsole_IResult$GetFloat(const ::IConsole_IResult &self, ::std::uint32_t Index) noexcept {
  float (::IConsole_IResult::*GetFloat$)(::std::uint32_t) const = &::IConsole_IResult::GetFloat;
  return (self.*GetFloat$)(Index);
}

void cxxbridge1$IConsole_IResult$GetString(const ::IConsole_IResult &self, ::std::uint32_t Index, ::StrRef *return$) noexcept {
  ::StrRef (::IConsole_IResult::*GetString$)(::std::uint32_t) const = &::IConsole_IResult::GetString;
  new (return$) ::StrRef((self.*GetString$)(Index));
}

void cxxbridge1$IConsole_IResult$GetColor(const ::IConsole_IResult &self, ::std::uint32_t Index, float DarkestLighting, ::ColorHSLA *return$) noexcept {
  std::optional<::ColorHSLA> (::IConsole_IResult::*GetColor$)(::std::uint32_t, float) const = &::IConsole_IResult::GetColor;
  new(return$)::ColorHSLA((self.*GetColor$)(Index, DarkestLighting).value_or(::ColorHSLA(0, 0, 0)));
}

::std::int32_t cxxbridge1$IConsole_IResult$NumArguments(const ::IConsole_IResult &self) noexcept {
  ::std::int32_t (::IConsole_IResult::*NumArguments$)() const = &::IConsole_IResult::NumArguments;
  return (self.*NumArguments$)();
}

::std::int32_t cxxbridge1$IConsole_IResult$GetVictim(const ::IConsole_IResult &self) noexcept {
  ::std::int32_t (::IConsole_IResult::*GetVictim$)() const = &::IConsole_IResult::GetVictim;
  return (self.*GetVictim$)();
}

void cxxbridge1$IConsole$ExecuteLine(::IConsole &self, ::StrRef *pStr, ::std::int32_t ClientId, bool InterpretSemicolons) noexcept {
  void (::IConsole::*ExecuteLine$)(::StrRef, ::std::int32_t, bool) = &::IConsole::ExecuteLine;
  (self.*ExecuteLine$)(::std::move(*pStr), ClientId, InterpretSemicolons);
}

void cxxbridge1$IConsole$Print(const ::IConsole &self, ::std::int32_t Level, ::StrRef *pFrom, ::StrRef *pStr, ::ColorRGBA *PrintColor) noexcept {
  void (::IConsole::*Print$)(::std::int32_t, ::StrRef, ::StrRef, ::ColorRGBA) const = &::IConsole::Print;
  (self.*Print$)(Level, ::std::move(*pFrom), ::std::move(*pStr), ::std::move(*PrintColor));
}

void cxxbridge1$IConsole$Register(::IConsole &self, ::StrRef *pName, ::StrRef *pParams, ::std::int32_t Flags, ::IConsole_FCommandCallback *pfnFunc, ::UserPtr *pUser, ::StrRef *pHelp) noexcept {
  void (::IConsole::*Register$)(::StrRef, ::StrRef, ::std::int32_t, ::IConsole_FCommandCallback, ::UserPtr, ::StrRef) = &::IConsole::Register;
  (self.*Register$)(::std::move(*pName), ::std::move(*pParams), Flags, ::std::move(*pfnFunc), ::std::move(*pUser), ::std::move(*pHelp));
}

::IConsole *cxxbridge1$CreateConsole(::std::int32_t FlagMask) noexcept {
  ::std::unique_ptr<::IConsole> (*CreateConsole$)(::std::int32_t) = ::CreateConsole;
  return CreateConsole$(FlagMask).release();
}

static_assert(::rust::detail::is_complete<::IConsole>::value, "definition of IConsole is required");
static_assert(sizeof(::std::unique_ptr<::IConsole>) == sizeof(void *), "");
static_assert(alignof(::std::unique_ptr<::IConsole>) == alignof(void *), "");
void cxxbridge1$unique_ptr$IConsole$null(::std::unique_ptr<::IConsole> *ptr) noexcept {
  ::new (ptr) ::std::unique_ptr<::IConsole>();
}
void cxxbridge1$unique_ptr$IConsole$raw(::std::unique_ptr<::IConsole> *ptr, ::IConsole *raw) noexcept {
  ::new (ptr) ::std::unique_ptr<::IConsole>(raw);
}
const ::IConsole *cxxbridge1$unique_ptr$IConsole$get(const ::std::unique_ptr<::IConsole>& ptr) noexcept {
  return ptr.get();
}
::IConsole *cxxbridge1$unique_ptr$IConsole$release(::std::unique_ptr<::IConsole>& ptr) noexcept {
  return ptr.release();
}
void cxxbridge1$unique_ptr$IConsole$drop(::std::unique_ptr<::IConsole> *ptr) noexcept {
  ::rust::deleter_if<::rust::detail::is_complete<::IConsole>::value>{}(ptr);
}
} // extern "C"
