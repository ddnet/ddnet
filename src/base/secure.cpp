/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "secure.h"

#include "dbg.h"
#include "types.h"
#include "windows.h"

#include <cstddef> // size_t
#include <mutex> // std::call_once

#if defined(CONF_FAMILY_WINDOWS)
#include <wtypes.h>
// Need to include wtypes.h before wincrypt.h because the latter is missing the former include
#include <wincrypt.h>
#else
#include "io.h"
#if defined(__WIIU__)
#include <coreinit/time.h>
#include <cstdlib>
#endif
#endif

struct SECURE_RANDOM_DATA
{
	std::once_flag initialized_once_flag;
#if defined(CONF_FAMILY_WINDOWS)
	HCRYPTPROV provider;
#else
	IOHANDLE urandom;
#endif
};

static struct SECURE_RANDOM_DATA secure_random_data = {};

static void ensure_secure_random_init()
{
	std::call_once(secure_random_data.initialized_once_flag, []() {
#if defined(CONF_FAMILY_WINDOWS)
		if(!CryptAcquireContext(&secure_random_data.provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		{
			const DWORD LastError = GetLastError();
			dbg_assert_failed("Failed to initialize secure random: CryptAcquireContext failure (%ld '%s')", LastError, windows_format_system_message(LastError).c_str());
		}
#else
		secure_random_data.urandom = io_open("/dev/urandom", IOFLAG_READ);
		dbg_assert(secure_random_data.urandom != nullptr, "Failed to initialize secure random: failed to open /dev/urandom");
#endif
	});
}

void generate_password(char *buffer, unsigned length, const unsigned short *random, unsigned random_length)
{
	static const char VALUES[] = "ABCDEFGHKLMNPRSTUVWXYZabcdefghjkmnopqt23456789";
	static const size_t NUM_VALUES = sizeof(VALUES) - 1; // Disregard the '\0'.
	unsigned i;
	dbg_assert(length >= random_length * 2 + 1, "too small buffer");
	dbg_assert(NUM_VALUES * NUM_VALUES >= 2048, "need at least 2048 possibilities for 2-character sequences");

	buffer[random_length * 2] = 0;

	for(i = 0; i < random_length; i++)
	{
		unsigned short random_number = random[i] % 2048;
		buffer[2 * i + 0] = VALUES[random_number / NUM_VALUES];
		buffer[2 * i + 1] = VALUES[random_number % NUM_VALUES];
	}
}

static constexpr unsigned MAX_PASSWORD_LENGTH = 128;

void secure_random_password(char *buffer, unsigned length, unsigned pw_length)
{
	unsigned short random[MAX_PASSWORD_LENGTH / 2];
	// With 6 characters, we get a password entropy of log(2048) * 6/2 = 33bit.
	dbg_assert(length >= pw_length + 1, "too small buffer");
	dbg_assert(pw_length >= 6, "too small password length");
	dbg_assert(pw_length % 2 == 0, "need an even password length");
	dbg_assert(pw_length <= MAX_PASSWORD_LENGTH, "too large password length");

	secure_random_fill(random, pw_length);

	generate_password(buffer, length, random, pw_length / 2);
}

#if defined(__WIIU__)
static unsigned long long wiiu_x = 123456789, wiiu_y = 362436069, wiiu_z = 521288629, wiiu_w = 88675123;

static void wiiu_secure_random_init()
{
	unsigned long long seed = OSGetSystemTime();
	wiiu_x ^= seed;
	wiiu_y ^= (seed >> 8);
	wiiu_z ^= (seed >> 16);
	wiiu_w ^= (seed >> 24);
}

static void wiiu_secure_random_fill(void *bytes, unsigned length)
{
	static std::once_flag wiiu_random_once;
	std::call_once(wiiu_random_once, wiiu_secure_random_init);

	unsigned char *p = (unsigned char *)bytes;
	for(unsigned i = 0; i < length; i++)
	{
		unsigned long long t = wiiu_x ^ (wiiu_x << 11);
		wiiu_x = wiiu_y; wiiu_y = wiiu_z; wiiu_z = wiiu_w;
		wiiu_w = wiiu_w ^ (wiiu_w >> 19) ^ (t ^ (t >> 8));
		p[i] = wiiu_w & 0xFF;
	}
}
#endif

void secure_random_fill(void *bytes, unsigned length)
{
#if defined(__WIIU__)
	wiiu_secure_random_fill(bytes, length);
#else
	ensure_secure_random_init();
#if defined(CONF_FAMILY_WINDOWS)
	if(!CryptGenRandom(secure_random_data.provider, length, (unsigned char *)bytes))
	{
		const DWORD LastError = GetLastError();
		dbg_assert_failed("CryptGenRandom failure (%ld '%s')", LastError, windows_format_system_message(LastError).c_str());
	}
#else
	dbg_assert(length == io_read(secure_random_data.urandom, bytes, length), "io_read returned with a short read");
#endif
#endif
}

// From https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2.
static unsigned int find_next_power_of_two_minus_one(unsigned int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n;
}

int secure_rand_below(int below)
{
	unsigned int mask = find_next_power_of_two_minus_one(below);
	dbg_assert(below > 0, "below must be positive");
	while(true)
	{
		unsigned int n;
		secure_random_fill(&n, sizeof(n));
		n &= mask;
		if((int)n < below)
		{
			return n;
		}
	}
}
