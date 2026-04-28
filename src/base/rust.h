#ifndef BASE_RUST_H
#define BASE_RUST_H
typedef const char *StrRef;
typedef void *UserPtr;

extern "C" void rust_panic_use_dbg_assert();
#endif // BASE_RUST_H
