/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_SECURE_H
#define BASE_SECURE_H

/**
 * Secure random number generation.
 *
 * @defgroup Secure-Random Secure Random
 */

/**
 * Generates a null-terminated password of length `2 * random_length`.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the output buffer.
 * @param length Length of the buffer.
 * @param random Pointer to a randomly-initialized array of shorts.
 * @param random_length Length of the short array.
 */
void generate_password(char *buffer, unsigned length, const unsigned short *random, unsigned random_length);

/**
 * Fills the buffer with the specified amount of random password characters.
 *
 * @ingroup Secure-Random
 *
 * @param buffer Pointer to the start of the buffer.
 * @param length Length of the buffer.
 * @param pw_length Length of the desired password.
 *
 * @remark The desired password length must be greater or equal to 6,
 *         even and smaller or equal to 128.
 */
void secure_random_password(char *buffer, unsigned length, unsigned pw_length);

/**
 * Fills the buffer with the specified amount of random bytes.
 *
 * @ingroup Secure-Random
 *
 * @param bytes Pointer to the start of the buffer.
 * @param length Length of the buffer.
 */
void secure_random_fill(void *bytes, unsigned length);

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

#endif
