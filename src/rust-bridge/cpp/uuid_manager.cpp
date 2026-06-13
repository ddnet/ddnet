#include "base/rust.h"
#include "engine/shared/uuid_manager.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif // __clang__
#endif // __GNUC__

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

using CUuidManager = ::CUuidManager;

static_assert(
    ::rust::IsRelocatable<::StrRef>::value,
    "type StrRef should be trivially move constructible and trivially destructible in C++ to be used as an argument of `CalculateUuid`, `RegisterName` or return value of `GetName` in Rust");
static_assert(
    ::rust::IsRelocatable<::CUuid>::value,
    "type CUuid should be trivially move constructible and trivially destructible in C++ to be used as an argument of `LookupUuid` or return value of `RandomUuid`, `CalculateUuid`, `GetUuid` in Rust");

extern "C" {
void cxxbridge1$194$RandomUuid(::CUuid *return$) noexcept {
  ::CUuid (*RandomUuid$)() = ::RandomUuid;
  new (return$) ::CUuid(RandomUuid$());
}

void cxxbridge1$194$CalculateUuid(::StrRef *name, ::CUuid *return$) noexcept {
  ::CUuid (*CalculateUuid$)(::StrRef) = ::CalculateUuid;
  new (return$) ::CUuid(CalculateUuid$(::std::move(*name)));
}

void cxxbridge1$194$CUuidManager$RegisterName(::CUuidManager &self, ::std::int32_t id, ::StrRef *name) noexcept {
  void (::CUuidManager::*RegisterName$)(::std::int32_t, ::StrRef) = &::CUuidManager::RegisterName;
  (self.*RegisterName$)(id, ::std::move(*name));
}

void cxxbridge1$194$CUuidManager$GetUuid(::CUuidManager const &self, ::std::int32_t id, ::CUuid *return$) noexcept {
  ::CUuid (::CUuidManager::*GetUuid$)(::std::int32_t) const = &::CUuidManager::GetUuid;
  new (return$) ::CUuid((self.*GetUuid$)(id));
}

void cxxbridge1$194$CUuidManager$GetName(::CUuidManager const &self, ::std::int32_t id, ::StrRef *return$) noexcept {
  ::StrRef (::CUuidManager::*GetName$)(::std::int32_t) const = &::CUuidManager::GetName;
  new (return$) ::StrRef((self.*GetName$)(id));
}

::std::int32_t cxxbridge1$194$CUuidManager$LookupUuid(::CUuidManager const &self, ::CUuid *uuid) noexcept {
  ::std::int32_t (::CUuidManager::*LookupUuid$)(::CUuid) const = &::CUuidManager::LookupUuid;
  return (self.*LookupUuid$)(::std::move(*uuid));
}

::std::int32_t cxxbridge1$194$CUuidManager$NumUuids(::CUuidManager const &self) noexcept {
  ::std::int32_t (::CUuidManager::*NumUuids$)() const = &::CUuidManager::NumUuids;
  return (self.*NumUuids$)();
}

void cxxbridge1$194$CUuidManager$DebugDump(::CUuidManager const &self) noexcept {
  void (::CUuidManager::*DebugDump$)() const = &::CUuidManager::DebugDump;
  (self.*DebugDump$)();
}

::CUuidManager *cxxbridge1$194$CUuidManager$New() noexcept {
  ::std::unique_ptr<::CUuidManager> (*New$)() = &::CUuidManager::New;
  return New$().release();
}

::CUuidManager const *cxxbridge1$194$CUuidManager$Global() noexcept {
  ::CUuidManager const &(*Global$)() = &::CUuidManager::Global;
  return &Global$();
}

static_assert(::rust::detail::is_complete<::std::remove_extent<::CUuidManager>::type>::value, "definition of `::CUuidManager` is required");
static_assert(sizeof(::std::unique_ptr<::CUuidManager>) == sizeof(void *), "");
static_assert(alignof(::std::unique_ptr<::CUuidManager>) == alignof(void *), "");
void cxxbridge1$unique_ptr$CUuidManager$null(::std::unique_ptr<::CUuidManager> *ptr) noexcept {
  ::new (ptr) ::std::unique_ptr<::CUuidManager>();
}
void cxxbridge1$unique_ptr$CUuidManager$raw(::std::unique_ptr<::CUuidManager> *ptr, ::std::unique_ptr<::CUuidManager>::pointer raw) noexcept {
  ::new (ptr) ::std::unique_ptr<::CUuidManager>(raw);
}
::std::unique_ptr<::CUuidManager>::element_type const *cxxbridge1$unique_ptr$CUuidManager$get(::std::unique_ptr<::CUuidManager> const &ptr) noexcept {
  return ptr.get();
}
::std::unique_ptr<::CUuidManager>::pointer cxxbridge1$unique_ptr$CUuidManager$release(::std::unique_ptr<::CUuidManager> &ptr) noexcept {
  return ptr.release();
}
void cxxbridge1$unique_ptr$CUuidManager$drop(::std::unique_ptr<::CUuidManager> *ptr) noexcept {
  ::rust::deleter_if<::rust::detail::is_complete<::CUuidManager>::value>{}(ptr);
}
} // extern "C"
