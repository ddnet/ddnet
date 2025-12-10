/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_MEM_H
#define BASE_MEM_H

#include <cstdint>
#include <cstring>
#include <type_traits>

/**
 * Memory management utilities.
 *
 * @defgroup Memory Memory
 */

/**
 * Copies a memory block.
 *
 * @ingroup Memory
 *
 * @param dest Destination.
 * @param source Source to copy.
 * @param size Size of the block to copy.
 *
 * @remark This functions DOES NOT handle cases where the source and destination overlap.
 *
 * @see mem_move
 */
void mem_copy(void *dest, const void *source, size_t size);

/**
 * Copies a memory block.
 *
 * @ingroup Memory
 *
 * @param dest Destination.
 * @param source Source to copy.
 * @param size Size of the block to copy.
 *
 * @remark This functions handles the cases where the source and destination overlap.
 *
 * @see mem_copy
 */
void mem_move(void *dest, const void *source, size_t size);

/**
 * Sets a complete memory block to 0.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to zero out.
 * @param size Size of the block.
 */
template<typename T>
inline void mem_zero(T *block, size_t size)
{
	static_assert((std::is_trivially_constructible<T>::value && std::is_trivially_destructible<T>::value) || std::is_fundamental<T>::value);
	memset(block, 0, size);
}

/**
 * Compares two blocks of memory of the same size, lexicographically.
 *
 * @ingroup Memory
 *
 * @param a First block of data.
 * @param b Second block of data.
 * @param size Size of the data to compare.
 *
 * @return `< 0` if block `a` is less than block `b`.
 * @return `0` if block `a` is equal to block `b`.
 * @return `> 0` if block `a` is greater than block `b`.
 */
int mem_comp(const void *a, const void *b, size_t size);

/**
 * Checks whether a block of memory contains null bytes.
 *
 * @ingroup Memory
 *
 * @param block Pointer to the block to check for nulls.
 * @param size Size of the block.
 *
 * @return `true` if the block has a null byte, `false` otherwise.
 */
bool mem_has_null(const void *block, size_t size);

#endif
