/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*
	Title: Secure randomizer
*/

#ifndef BASE_RANDOM_H
#define BASE_RANDOM_H

#include "detect.h"
#include "types.h"

/**
 * Secure random number generation.
 *
 * @defgroup Secure-Random
 */

/**
 * Fills the buffer with the specified amount of random bytes.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the buffer.
 * @param length Length of the buffer.
 */
void secure_random_fill(void *bytes, unsigned length);

/**
 * Returns random `int`.
 *
 * @ingroup Secure-Random
 *
 * @return Random int.
 *
 * @remark Can be used as a replacement for the `rand` function.
 */
int secure_rand();

/**
 * Returns a random nonnegative integer below the given number,
 * with a uniform distribution.
 *
 * @ingroup Secure-Random
 *
 * @param below Upper limit (exclusive) of integers to return.
 *
 * @return Random nonnegative below the given number.
 */
int secure_rand_below(int below);

/**
 * Secure random number generation utility.
 *
 * This class provides static methods for secure random number generation.
 * All platform-specific initialization and cleanup is handled automatically.
 *
 * @ingroup Secure-Random
 */
class CSecureRandom
{
private:
	// Forward declaration for implementation details
	struct Impl;
	Impl *m_pImpl;

	CSecureRandom();
	~CSecureRandom();

	CSecureRandom(const CSecureRandom &) = delete;
	CSecureRandom &operator=(const CSecureRandom &) = delete;
	CSecureRandom(CSecureRandom &&) = delete;
	CSecureRandom &operator=(CSecureRandom &&) = delete;

	static CSecureRandom &Instance();

public:
	/**
	 * Fills the buffer with the specified amount of random bytes.
	 *
	 * @param bytes Pointer to the start of the buffer.
	 * @param length Length of the buffer.
	 */
	static void Fill(void *bytes, unsigned length);

	/**
	 * Returns random `int`.
	 *
	 * @return Random int.
	 *
	 * @remark Can be used as a replacement for the `rand` function.
	 */
	static int Rand();

	/**
	 * Returns a random nonnegative integer below the given number,
	 * with a uniform distribution.
	 *
	 * @param below Upper limit (exclusive) of integers to return.
	 *
	 * @return Random nonnegative below the given number.
	 */
	static int RandBelow(int below);

	/**
	 * Check if the secure random system was successfully initialized.
	 *
	 * @return true if initialized successfully, false otherwise.
	 */
	static bool Initialized();
};

#endif // BASE_RANDOM_H
