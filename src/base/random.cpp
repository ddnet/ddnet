/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "random.h"
#include "logger.h"
#include "system.h"

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

#include <wincrypt.h>
#endif

struct CSecureRandom::Impl
{
	bool m_Initialized;
#if defined(CONF_FAMILY_WINDOWS)
	HCRYPTPROV m_Provider;
#else
	IOHANDLE m_Urandom;
#endif

	Impl()
	{
#if defined(CONF_FAMILY_WINDOWS)
		m_Initialized = (CryptAcquireContext(&m_Provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) != FALSE);
		if(!m_Initialized)
		{
			const DWORD LastError = GetLastError();
			const std::string ErrorMsg = windows_format_system_message(LastError);
			log_error("random", "CryptAcquireContext failed: %ld %s", LastError, ErrorMsg.c_str());
		}
#else
		m_Urandom = io_open("/dev/urandom", IOFLAG_READ);
		m_Initialized = (m_Urandom != nullptr);
		if(!m_Initialized)
			log_error("random", "Failed to open /dev/urandom");
#endif
	}

	~Impl()
	{
		if(m_Initialized)
		{
#if defined(CONF_FAMILY_WINDOWS)
			if(!CryptReleaseContext(m_Provider, 0))
				log_error("random", "CryptReleaseContext failed: %ld", GetLastError());
#else
			if(!io_close(m_Urandom))
				log_error("random", "io_close failed");
#endif
		}
	}
};

CSecureRandom::CSecureRandom() :
	m_pImpl(new Impl())
{
}

CSecureRandom::~CSecureRandom()
{
	delete m_pImpl;
}

CSecureRandom &CSecureRandom::Instance()
{
	static CSecureRandom instance;
	return instance;
}

bool CSecureRandom::Initialized()
{
	return Instance().m_pImpl->m_Initialized;
}

void CSecureRandom::Fill(void *bytes, unsigned length)
{
	CSecureRandom &instance = Instance();
	dbg_assert(instance.m_pImpl->m_Initialized, "secure random not initialized");
#if defined(CONF_FAMILY_WINDOWS)
	if(!CryptGenRandom(instance.m_pImpl->m_Provider, length, (unsigned char *)bytes))
	{
		const DWORD LastError = GetLastError();
		const std::string ErrorMsg = windows_format_system_message(LastError);
		dbg_assert(false, "CryptGenRandom failed: %ld %s", LastError, ErrorMsg.c_str());
	}
#else
	dbg_assert(length == io_read(instance.m_pImpl->m_Urandom, bytes, length), "io_read returned with a short read");
#endif
}

int CSecureRandom::Rand()
{
	unsigned int i;
	Fill(&i, sizeof(i));
	return (int)(i % RAND_MAX);
}

// From https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2.
unsigned int find_next_power_of_two_minus_one(unsigned int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 4;
	n |= n >> 16;
	return n;
}

int CSecureRandom::RandBelow(int below)
{
	unsigned int mask = find_next_power_of_two_minus_one(below);
	dbg_assert(below > 0, "below must be positive");
	while(true)
	{
		unsigned int n;
		Fill(&n, sizeof(n));
		n &= mask;
		if((int)n < below)
		{
			return n;
		}
	}
}

void secure_random_fill(void *bytes, unsigned length)
{
	CSecureRandom::Fill(bytes, length);
}

int secure_rand()
{
	return CSecureRandom::Rand();
}

int secure_rand_below(int below)
{
	return CSecureRandom::RandBelow(below);
}
