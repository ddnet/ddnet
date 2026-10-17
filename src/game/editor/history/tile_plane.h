#ifndef GAME_EDITOR_HISTORY_TILE_PLANE_H
#define GAME_EDITOR_HISTORY_TILE_PLANE_H

#include "shared_value.h"

#include <base/dbg.h>

#include <game/map/tile_cursor.h>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>

namespace editor_history
{
	/**
	 * Sparse persistent radix tree: copies share their root; writes detach shared
	 * nodes along the affected path and the affected chunk. Empty subtrees allocate
	 * nothing. The editor wrapper selects 32x32 chunks; this template stays generic.
	 * Read/cursor views borrow storage and must not survive mutation of the plane.
	 */
	template<typename T, std::size_t ChunkEdge>
	class CTilePlane
	{
		static_assert(ChunkEdge > 0);
		static constexpr std::size_t FANOUT = 16;
		using CCells = std::array<T, ChunkEdge * ChunkEdge>;
		class CChunk
		{
		public:
			CCells m_aCells{};
			std::size_t m_Nonzero = 0;
		};
		class CNode
		{
		public:
			std::array<std::shared_ptr<CNode>, FANOUT> m_apChildren{};
			std::shared_ptr<CChunk> m_pChunk;
		};
		std::size_t m_Width = 0, m_Height = 0;
		unsigned m_Depth = 0;
		std::shared_ptr<CNode> m_pRoot;
		inline static const CCells ms_aEmpty{};
		std::size_t Columns() const { return (m_Width + ChunkEdge - 1) / ChunkEdge; }
		std::size_t Rows() const { return (m_Height + ChunkEdge - 1) / ChunkEdge; }
		static std::size_t Span(unsigned Depth) { return std::size_t{1} << (Depth * 4); }
		const CChunk *ChunkAt(std::size_t Index) const
		{
			const CNode *pNode = m_pRoot.get();
			for(unsigned Depth = m_Depth; pNode && Depth != 0; --Depth)
				pNode = pNode->m_apChildren[(Index >> ((Depth - 1) * 4)) & (FANOUT - 1)].get();
			return pNode ? pNode->m_pChunk.get() : nullptr;
		}
		template<typename F>
		static void Mutate(std::shared_ptr<CNode> &pNode, unsigned Depth, std::size_t Index, F &&Function)
		{
			std::shared_ptr<CNode> pStaged;
			if(!pNode)
				pStaged = std::make_shared<CNode>();
			else if(pNode.use_count() != 1)
				pStaged = std::make_shared<CNode>(*pNode);
			auto *pTarget = pStaged ? pStaged.get() : pNode.get();
			bool Empty;
			if(Depth == 0)
			{
				Function(pTarget->m_pChunk);
				Empty = !pTarget->m_pChunk;
			}
			else
			{
				Mutate(pTarget->m_apChildren[(Index >> ((Depth - 1) * 4)) & (FANOUT - 1)], Depth - 1, Index, Function);
				Empty = std::all_of(pTarget->m_apChildren.begin(), pTarget->m_apChildren.end(), [](const auto &pChild) { return !pChild; });
			}
			// Publish a new path only after its leaf allocation/mutation succeeds.
			if(Empty)
				pNode.reset();
			else if(pStaged)
				pNode = std::move(pStaged);
		}
		static bool Equal(const CNode *pLeft, const CNode *pRight, unsigned Depth)
		{
			if(pLeft == pRight)
				return true;
			if(!pLeft || !pRight)
				return false;
			if(Depth == 0)
				return pLeft->m_pChunk == pRight->m_pChunk || pLeft->m_pChunk->m_aCells == pRight->m_pChunk->m_aCells;
			for(std::size_t Child = 0; Child < FANOUT; ++Child)
				if(!Equal(pLeft->m_apChildren[Child].get(), pRight->m_apChildren[Child].get(), Depth - 1))
					return false;
			return true;
		}
		template<typename F>
		std::size_t Visit(const CNode *pNode, const CNode *pPrevious, unsigned Depth, std::size_t First, bool All, F &Visitor) const
		{
			if(First >= ChunkCount() || (!All && pNode == pPrevious))
				return 1;
			if(Depth == 0)
			{
				const auto X = First % Columns() * ChunkEdge, Y = First / Columns() * ChunkEdge;
				std::invoke(Visitor, X, Y, std::min(ChunkEdge, m_Width - X), std::min(ChunkEdge, m_Height - Y), std::span<const T, ChunkEdge * ChunkEdge>(pNode ? pNode->m_pChunk->m_aCells : ms_aEmpty));
				return 1;
			}
			std::size_t Visits = 1;
			for(std::size_t Child = 0; Child < FANOUT; ++Child)
				Visits += Visit(pNode ? pNode->m_apChildren[Child].get() : nullptr, pPrevious ? pPrevious->m_apChildren[Child].get() : nullptr, Depth - 1, First + Child * Span(Depth - 1), All, Visitor);
			return Visits;
		}

		static void AccountNode(const CNode *pNode, CStorageUsage &Usage)
		{
			if(!pNode || !Usage.Add(pNode, sizeof(CNode)))
				return;
			if(pNode->m_pChunk)
				Usage.Add(pNode->m_pChunk.get(), sizeof(CChunk));
			else
				for(const auto &pChild : pNode->m_apChildren)
					AccountNode(pChild.get(), Usage);
		}

	public:
		class CRowSpan
		{
		public:
			// An empty span represents m_Length default cells without storage.
			std::span<const T> m_Cells;
			std::size_t m_Length;
		};

		CTilePlane() = default;
		static std::optional<CTilePlane> Create(std::size_t Width, std::size_t Height)
		{
			if(Width > std::numeric_limits<std::size_t>::max() - ChunkEdge + 1 || Height > std::numeric_limits<std::size_t>::max() - ChunkEdge + 1 || (Width != 0 && Height > std::vector<T>().max_size() / Width))
				return std::nullopt;
			CTilePlane Result;
			Result.m_Width = Width;
			Result.m_Height = Height;
			for(auto Last = Result.ChunkCount() == 0 ? 0 : Result.ChunkCount() - 1; Last != 0; Last /= FANOUT)
				++Result.m_Depth;
			return Result;
		}
		std::size_t Width() const { return m_Width; }
		std::size_t Height() const { return m_Height; }
		std::size_t ChunkCount() const { return Columns() * Rows(); }
		unsigned TreeDepth() const { return m_Depth; }
		const T &At(std::size_t X, std::size_t Y) const
		{
			dbg_assert(X < m_Width && Y < m_Height, "Tile coordinates out of bounds");
			const auto *pChunk = ChunkAt((Y / ChunkEdge) * Columns() + X / ChunkEdge);
			return (pChunk ? pChunk->m_aCells : ms_aEmpty)[(Y % ChunkEdge) * ChunkEdge + X % ChunkEdge];
		}
		std::span<const T, ChunkEdge * ChunkEdge> ChunkCells(std::size_t X, std::size_t Y) const
		{
			const auto *pChunk = ChunkAt(Y / ChunkEdge * Columns() + X / ChunkEdge);
			return std::span<const T, ChunkEdge * ChunkEdge>(pChunk ? pChunk->m_aCells : ms_aEmpty);
		}

	private:
		CRowSpan ReadRowImpl(std::size_t X, std::size_t Y, std::size_t EndX, std::size_t Index, CTileChunkCursor::CEntry &Entry) const
		{
			if(Entry.m_Index != Index)
			{
				const CNode *pNode = m_pRoot.get();
				unsigned Depth = m_Depth;
				while(pNode && Depth != 0)
				{
					--Depth;
					pNode = pNode->m_apChildren.data()[(Index >> (Depth * 4)) & (FANOUT - 1)].get();
				}
				Entry.m_Index = Index;
				Entry.m_pCells = pNode ? pNode->m_pChunk->m_aCells.data() : nullptr;
				Entry.m_EmptyEnd = Index + Span(Depth) - Index % Span(Depth);
			}
			if(!Entry.m_pCells)
			{
				const auto RemainingChunks = Entry.m_EmptyEnd - Index;
				return {{}, std::min(EndX - X, RemainingChunks * ChunkEdge - X % ChunkEdge)};
			}
			const auto Length = std::min(EndX - X, ChunkEdge - X % ChunkEdge);
			return {std::span<const T>(static_cast<const T *>(Entry.m_pCells) + Y % ChunkEdge * ChunkEdge + X % ChunkEdge, Length), Length};
		}

	public:
		/** Borrow one chunk row or skip a missing subtree, bounded by EndX.
		 * The view expires at the next plane mutation; renderers never retain it.
		 */
		CRowSpan ReadRow(std::size_t X, std::size_t Y, std::size_t EndX) const
		{
			dbg_assert(X < EndX && EndX <= m_Width && Y < m_Height, "Invalid tile row range");
			CTileChunkCursor::CEntry Entry;
			return ReadRowImpl(X, Y, EndX, Y / ChunkEdge * Columns() + X / ChunkEdge, Entry);
		}
		CRowSpan ReadRowCached(std::size_t X, std::size_t Y, std::size_t EndX, CTileChunkCursor &Cursor) const
		{
			dbg_assert(X < EndX && EndX <= m_Width && Y < m_Height, "Invalid tile row range");
			const auto Index = Y / ChunkEdge * Columns() + X / ChunkEdge;
			auto &Entry = Cursor.m_aEntries[Index % CTileChunkCursor::CAPACITY];
			Cursor.m_TreeLookups += Entry.m_Index != Index;
			return ReadRowImpl(X, Y, EndX, Index, Entry);
		}

		void Set(std::size_t X, std::size_t Y, const T &Value)
		{
			if(At(X, Y) == Value)
				return;
			Mutate(m_pRoot, m_Depth, (Y / ChunkEdge) * Columns() + X / ChunkEdge, [&](auto &pChunk) {
				if(!pChunk)
					pChunk = std::make_shared<CChunk>();
				else if(pChunk.use_count() != 1)
					pChunk = std::make_shared<CChunk>(*pChunk);
				auto &Cell = pChunk->m_aCells[(Y % ChunkEdge) * ChunkEdge + X % ChunkEdge];
				pChunk->m_Nonzero -= !(Cell == T{});
				Cell = Value;
				pChunk->m_Nonzero += !(Cell == T{});
				if(pChunk->m_Nonzero == 0)
					pChunk.reset();
			});
		}
		/** Bulk import builds each normalized chunk once, without edit-style writes. */
		template<typename F>
		void AssignRows(F &&FillRow)
		{
			if(ChunkCount() == 0)
				return;
			auto Candidate = *Create(m_Width, m_Height);
			CChunk Chunk;
			for(std::size_t ChunkY = 0; ChunkY < Rows(); ++ChunkY)
				for(std::size_t ChunkX = 0; ChunkX < Columns(); ++ChunkX)
				{
					Chunk = {};
					const auto Width = std::min(ChunkEdge, m_Width - ChunkX * ChunkEdge);
					const auto Height = std::min(ChunkEdge, m_Height - ChunkY * ChunkEdge);
					for(std::size_t Y = 0; Y < Height; ++Y)
						Chunk.m_Nonzero += FillRow(ChunkX * ChunkEdge, ChunkY * ChunkEdge + Y, std::span<T>(Chunk.m_aCells.data() + Y * ChunkEdge, Width));
					if(Chunk.m_Nonzero != 0)
						Mutate(Candidate.m_pRoot, m_Depth, ChunkY * Columns() + ChunkX, [&](auto &pChunk) { pChunk = std::make_shared<CChunk>(Chunk); });
				}
			*this = std::move(Candidate);
		}
		template<typename F>
		void Assign(F &&CellAt)
		{
			AssignRows([&](std::size_t X, std::size_t Y, std::span<T> Row) {
				std::size_t Nonzero = 0;
				for(std::size_t Index = 0; Index < Row.size(); ++Index)
				{
					Row[Index] = CellAt(X + Index, Y);
					Nonzero += !(Row[Index] == T{});
				}
				return Nonzero;
			});
		}
		/** Bulk sparse transform. Function must map an empty source cell to T{}. */
		template<typename U, typename F>
		void AssignSparse(const CTilePlane<U, ChunkEdge> &Source, F &&Function)
		{
			dbg_assert(m_Width == Source.Width() && m_Height == Source.Height(), "Sparse tile plane dimensions do not match");
			auto Candidate = *Create(m_Width, m_Height);
			const auto Empty = *CTilePlane<U, ChunkEdge>::Create(m_Width, m_Height);
			Source.VisitChangedChunks(&Empty, [&](std::size_t X, std::size_t Y, std::size_t Width, std::size_t Height, std::span<const U, ChunkEdge * ChunkEdge> Cells) {
				CChunk Chunk;
				for(std::size_t Row = 0; Row < Height; ++Row)
					for(std::size_t Column = 0; Column < Width; ++Column)
					{
						const auto Index = Row * ChunkEdge + Column;
						Chunk.m_aCells[Index] = Function(Cells[Index]);
						Chunk.m_Nonzero += !(Chunk.m_aCells[Index] == T{});
					}
				if(Chunk.m_Nonzero != 0)
					Mutate(Candidate.m_pRoot, m_Depth, Y / ChunkEdge * Columns() + X / ChunkEdge, [&](auto &pChunk) { pChunk = std::make_shared<CChunk>(std::move(Chunk)); });
			});
			*this = std::move(Candidate);
		}
		/** Preserve coordinates, share interior chunks, and normalize clipped edges. */
		bool Resize(std::size_t Width, std::size_t Height)
		{
			if(Width == m_Width && Height == m_Height)
				return true;
			auto Candidate = Create(Width, Height);
			if(!Candidate)
				return false;
			const auto Copy = [&](auto &&Self, const CNode *pNode, unsigned Depth, std::size_t First) -> void {
				if(!pNode)
					return;
				if(Depth != 0)
				{
					for(std::size_t Child = 0; Child < FANOUT; ++Child)
						Self(Self, pNode->m_apChildren[Child].get(), Depth - 1, First + Child * Span(Depth - 1));
					return;
				}
				const auto X = First % Columns() * ChunkEdge, Y = First / Columns() * ChunkEdge;
				if(X >= Width || Y >= Height)
					return;
				auto pChunk = pNode->m_pChunk;
				if(std::min(ChunkEdge, Width - X) < std::min(ChunkEdge, m_Width - X) || std::min(ChunkEdge, Height - Y) < std::min(ChunkEdge, m_Height - Y))
				{
					pChunk = std::make_shared<CChunk>(*pChunk);
					for(std::size_t TileY = 0; TileY < ChunkEdge; ++TileY)
						for(std::size_t TileX = 0; TileX < ChunkEdge; ++TileX)
							if(X + TileX >= Width || Y + TileY >= Height)
							{
								auto &Cell = pChunk->m_aCells[TileY * ChunkEdge + TileX];
								pChunk->m_Nonzero -= !(Cell == T{});
								Cell = {};
							}
				}
				if(pChunk->m_Nonzero != 0)
					Mutate(Candidate->m_pRoot, Candidate->m_Depth, Y / ChunkEdge * Candidate->Columns() + X / ChunkEdge, [&](auto &pTarget) { pTarget = std::move(pChunk); });
			};
			Copy(Copy, m_pRoot.get(), m_Depth, 0);
			*this = std::move(*Candidate);
			return true;
		}
		bool operator==(const CTilePlane &Other) const { return m_Width == Other.m_Width && m_Height == Other.m_Height && Equal(m_pRoot.get(), Other.m_pRoot.get(), m_Depth); }
		template<typename F>
		std::size_t VisitChangedChunks(const CTilePlane *pPrevious, F &&Visitor) const
		{
			const bool Same = pPrevious && m_Width == pPrevious->m_Width && m_Height == pPrevious->m_Height;
			return Visit(m_pRoot.get(), Same ? pPrevious->m_pRoot.get() : nullptr, m_Depth, 0, !Same, Visitor);
		}
		void Account(CStorageUsage &Usage) const { AccountNode(m_pRoot.get(), Usage); }
		std::size_t SharedChunks(const CTilePlane &Other) const
		{
			std::size_t Count = 0;
			for(std::size_t Y = 0; Y < std::min(Rows(), Other.Rows()); ++Y)
				for(std::size_t X = 0; X < std::min(Columns(), Other.Columns()); ++X)
					Count += ChunkAt(Y * Columns() + X) == Other.ChunkAt(Y * Other.Columns() + X);
			return Count;
		}
		/** Pins source nodes so subsequent writes detach even without a retained revision. */
		template<typename D>
		class CDigestCache
		{
			friend class CTilePlane;
			std::shared_ptr<const CNode> m_pSource;
			std::array<std::unique_ptr<CDigestCache>, FANOUT> m_apChildren{};
			D m_Digest{};
			std::size_t m_ChildrenBytes = 0;

		public:
			std::size_t ChildrenBytes() const { return m_ChildrenBytes; }
			void Account(CStorageUsage &Usage) const
			{
				AccountNode(m_pSource.get(), Usage);
				for(const auto &pChild : m_apChildren)
					if(pChild && Usage.Add(pChild.get(), sizeof(CDigestCache)))
						pChild->Account(Usage);
			}
		};
		template<typename D, typename FChunk, typename FBranch>
		D Digest(CDigestCache<D> &Cache, std::span<const D> EmptyDigests, FChunk &&HashChunk, FBranch &&HashBranch) const
		{
			const auto Fold = [&](auto &&Self, const std::shared_ptr<CNode> &pNode, unsigned Depth, CDigestCache<D> &Entry) -> D {
				if(!pNode)
				{
					Entry = {};
					return EmptyDigests[Depth];
				}
				if(Entry.m_pSource == pNode)
					return Entry.m_Digest;
				if(Depth == 0)
					Entry.m_Digest = HashChunk(std::span<const T, ChunkEdge * ChunkEdge>(pNode->m_pChunk->m_aCells));
				else
				{
					Entry.m_ChildrenBytes = 0;
					std::array<D, FANOUT> aDigests;
					for(std::size_t Child = 0; Child < FANOUT; ++Child)
					{
						if(!pNode->m_apChildren[Child])
						{
							Entry.m_apChildren[Child].reset();
							aDigests[Child] = EmptyDigests[Depth - 1];
							continue;
						}
						if(!Entry.m_apChildren[Child])
							Entry.m_apChildren[Child] = std::make_unique<CDigestCache<D>>();
						aDigests[Child] = Self(Self, pNode->m_apChildren[Child], Depth - 1, *Entry.m_apChildren[Child]);
						Entry.m_ChildrenBytes += sizeof(CDigestCache<D>) + Entry.m_apChildren[Child]->m_ChildrenBytes;
					}
					Entry.m_Digest = HashBranch(aDigests);
				}
				Entry.m_pSource = pNode;
				return Entry.m_Digest;
			};
			return Fold(Fold, m_pRoot, m_Depth, Cache);
		}
	};
} // namespace editor_history
#endif
