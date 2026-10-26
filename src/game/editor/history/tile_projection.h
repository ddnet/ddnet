#ifndef GAME_EDITOR_HISTORY_TILE_PROJECTION_H
#define GAME_EDITOR_HISTORY_TILE_PROJECTION_H

#include "tile_plane.h"

#include <optional>
#include <span>
#include <vector>

namespace editor_history
{

	/**
	 * Derived contiguous ABI data for renderers and exporters. Only changed chunks
	 * are projected. The previous plane pins immutable content identities; writes
	 * to a live draft therefore cannot modify the cached baseline through aliases.
	 */
	template<typename TSource, typename TTarget, std::size_t ChunkEdge>
	class CTileProjection
	{
		using CPlane = CTilePlane<TSource, ChunkEdge>;
		std::optional<CPlane> m_Previous;
		std::vector<TTarget> m_vCells;

	public:
		/** Invalidate after external projection inputs, e.g. image opacity, change. */
		void Invalidate() { m_Previous.reset(); }
		std::span<const TTarget> Cells() const { return m_vCells; }
		void Account(CStorageUsage &Usage) const
		{
			Usage.Add(m_vCells.data(), m_vCells.capacity() * sizeof(TTarget));
			if(m_Previous)
				m_Previous->Account(Usage);
		}

		template<typename F>
		std::size_t Refresh(const CPlane &Plane, F &&Project)
		{
			// Resize stages any allocation before the pinned previous state changes.
			m_vCells.resize(Plane.Width() * Plane.Height());
			std::size_t ChangedChunks = 0;
			Plane.VisitChangedChunks(m_Previous ? &*m_Previous : nullptr,
				[&](std::size_t X, std::size_t Y, std::size_t Width, std::size_t Height, const auto &aTiles) {
					for(std::size_t OffsetY = 0; OffsetY < Height; ++OffsetY)
						for(std::size_t OffsetX = 0; OffsetX < Width; ++OffsetX)
							m_vCells[(Y + OffsetY) * Plane.Width() + X + OffsetX] = std::invoke(Project, aTiles[OffsetY * ChunkEdge + OffsetX]);
					++ChangedChunks;
				});
			m_Previous = Plane;
			return ChangedChunks;
		}
	};

} // namespace editor_history

#endif
