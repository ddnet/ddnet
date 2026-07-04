// Wii U stub: replaces Rust's rust_panic_use_dbg_assert().
// On Wii U there is no Rust cross-toolchain, so we provide a no-op C++
// implementation. Rust panics won't be hooked; the assert machinery used
// in normal debug builds is sufficient.

extern "C" void rust_panic_use_dbg_assert()
{
	// No-op on Wii U: Rust is not compiled for this target.
}

// POSIX functions missing from devkitPPC newlib that SQLite references.
// The Wii U has no concept of file ownership or user ids, so these are stubs.
#include <sys/types.h>
#include <cerrno>

extern "C" int fchown(int, uid_t, gid_t)
{
	errno = ENOSYS;
	return -1;
}

extern "C" uid_t geteuid(void)
{
	return 0;
}

// Named pipes (FIFOs) do not exist on the Wii U. CFifo::Init calls mkfifo;
// provide a failing stub so the FIFO subsystem simply stays disabled.
extern "C" int mkfifo(const char *, mode_t)
{
	errno = ENOSYS;
	return -1;
}

// The Rust engine bridge is not compiled on Wii U (no PowerPC Rust toolchain).
// RustVersionRegister normally registers a "rust_version" console command; on
// Wii U we provide an empty implementation.
class IConsole;
void RustVersionRegister(IConsole &) noexcept
{
}

// devkitPPC has no libatomic and PowerPC 750 (Espresso) lacks native 64-bit
// atomic instructions. Provide the __atomic_*_8 libcalls GCC emits, guarded by
// a global spinlock built from a 1-byte atomic_flag (which IS supported).
#include <atomic>

namespace {
std::atomic_flag g_atomic64_lock = ATOMIC_FLAG_INIT;
struct Atomic64Guard
{
	Atomic64Guard()
	{
		while(g_atomic64_lock.test_and_set(std::memory_order_acquire))
		{
		}
	}
	~Atomic64Guard()
	{
		g_atomic64_lock.clear(std::memory_order_release);
	}
};
} // namespace

extern "C" {
unsigned long long __atomic_load_8(const volatile void *ptr, int)
{
	Atomic64Guard guard;
	return *(const volatile unsigned long long *)ptr;
}

void __atomic_store_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	*(volatile unsigned long long *)ptr = val;
}

unsigned long long __atomic_exchange_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	unsigned long long old = *(volatile unsigned long long *)ptr;
	*(volatile unsigned long long *)ptr = val;
	return old;
}

bool __atomic_compare_exchange_8(volatile void *ptr, void *expected, unsigned long long desired, bool, int, int)
{
	Atomic64Guard guard;
	unsigned long long cur = *(volatile unsigned long long *)ptr;
	if(cur == *(unsigned long long *)expected)
	{
		*(volatile unsigned long long *)ptr = desired;
		return true;
	}
	*(unsigned long long *)expected = cur;
	return false;
}

unsigned long long __atomic_fetch_add_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	unsigned long long old = *(volatile unsigned long long *)ptr;
	*(volatile unsigned long long *)ptr = old + val;
	return old;
}

unsigned long long __atomic_fetch_sub_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	unsigned long long old = *(volatile unsigned long long *)ptr;
	*(volatile unsigned long long *)ptr = old - val;
	return old;
}

unsigned long long __atomic_fetch_and_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	unsigned long long old = *(volatile unsigned long long *)ptr;
	*(volatile unsigned long long *)ptr = old & val;
	return old;
}

unsigned long long __atomic_fetch_or_8(volatile void *ptr, unsigned long long val, int)
{
	Atomic64Guard guard;
	unsigned long long old = *(volatile unsigned long long *)ptr;
	*(volatile unsigned long long *)ptr = old | val;
	return old;
}
} // extern "C"
