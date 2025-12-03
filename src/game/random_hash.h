#ifndef GAME_RANDOM_HASH_H
#define GAME_RANDOM_HASH_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace RandomHash
{
	uint32_t HashInts(std::initializer_list<int> Values);

	int SeededRandomIntBelow(int Below, std::initializer_list<int> Values);

} // namespace RandomHash

#endif // GAME_RANDOM_HASH_H
