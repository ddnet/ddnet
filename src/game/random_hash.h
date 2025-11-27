#ifndef GAME_RANDOM_HASH_H
#define GAME_RANDOM_HASH_H

#include <cstddef>
#include <cstdint>

namespace RandomHash
{
	uint32_t HashInts(const int32_t *Values, size_t Count);

	template<typename... Ts>
	inline uint32_t HashMany(Ts... Values)
	{
		int32_t Temp[] = {static_cast<int32_t>(Values)...};
		return HashInts(Temp, sizeof...(Ts));
	}

} // namespace RandomHash

#endif // GAME_RANDOM_HASH_H
