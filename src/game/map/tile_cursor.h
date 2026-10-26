#ifndef GAME_MAP_TILE_CURSOR_H
#define GAME_MAP_TILE_CURSOR_H

#include <cstddef>
#include <limits>

/** Bounded borrowed chunk addresses for one render invocation. The plane must
 * not change while the cursor exists; never retain this across a frame/edit.
 */
class CTileChunkCursor
{
public:
	class CEntry
	{
	public:
		std::size_t m_Index = std::numeric_limits<std::size_t>::max();
		std::size_t m_EmptyEnd = 0;
		const void *m_pCells = nullptr;
	};
	static constexpr std::size_t CAPACITY = 64;
	CEntry m_aEntries[CAPACITY];
	std::size_t m_TreeLookups = 0;
};

#endif
