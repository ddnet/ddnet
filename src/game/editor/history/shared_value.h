#ifndef GAME_EDITOR_HISTORY_SHARED_VALUE_H
#define GAME_EDITOR_HISTORY_SHARED_VALUE_H

#include <base/dbg.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace editor_history
{

	/** Counts the union of owned allocations, not the sum of revision sizes. */
	class CStorageUsage
	{
		class CAllocation
		{
		public:
			std::size_t m_Bytes = 0;
			std::size_t m_References = 0;
		};
		class CChange
		{
		public:
			const void *m_pAllocation;
			CAllocation m_Previous;
			bool m_Existed;
		};
		std::unordered_map<const void *, CAllocation> m_Allocations;
		std::vector<CChange> m_vChanges;
		std::size_t m_Bytes = 0;
		std::size_t m_Visits = 0;
		bool m_Transaction = false;
		bool m_Removing = false;

	public:
		/** Incremental DAG accounting, journaled so failed commits restore the union. */
		class CTransaction
		{
			CStorageUsage &m_Usage;
			std::size_t m_PreviousBytes;
			bool m_Committed = false;

		public:
			explicit CTransaction(CStorageUsage &Usage) :
				m_Usage(Usage), m_PreviousBytes(Usage.m_Bytes)
			{
				Usage.m_Transaction = true;
				Usage.m_Visits = 0;
			}
			CTransaction(const CTransaction &) = delete;
			~CTransaction()
			{
				if(!m_Committed)
				{
					for(const auto &Change : m_Usage.m_vChanges | std::views::reverse)
						if(Change.m_Existed)
							m_Usage.m_Allocations.find(Change.m_pAllocation)->second = Change.m_Previous;
						else
							m_Usage.m_Allocations.erase(Change.m_pAllocation);
					m_Usage.m_Bytes = m_PreviousBytes;
				}
				m_Usage.m_vChanges.clear();
				m_Usage.m_Transaction = m_Usage.m_Removing = false;
			}
			void Commit()
			{
				for(const auto &Change : m_Usage.m_vChanges)
				{
					const auto It = m_Usage.m_Allocations.find(Change.m_pAllocation);
					if(It != m_Usage.m_Allocations.end() && It->second.m_References == 0)
						m_Usage.m_Allocations.erase(It);
				}
				m_Committed = true;
			}
		};
		void Removing(bool Removing) { m_Removing = Removing; }
		/**
		 * Count a reference to an allocation (or remove it in removal mode). True
		 * means first insertion/last removal: only then traverse owned children.
		 * Unconditional recursion would miscount shared subtrees during eviction.
		 */
		bool Add(const void *pAllocation, std::size_t Bytes)
		{
			if(pAllocation == nullptr)
				return false;
			++m_Visits;
			auto It = m_Allocations.find(pAllocation);
			if(m_Transaction)
				m_vChanges.push_back({pAllocation, It == m_Allocations.end() ? CAllocation{} : It->second, It != m_Allocations.end()});
			if(m_Removing)
			{
				dbg_assert(It != m_Allocations.end() && It->second.m_References != 0, "Removing an unaccounted storage allocation");
				if(--It->second.m_References != 0)
					return false;
				m_Bytes -= It->second.m_Bytes;
				return true;
			}
			if(It == m_Allocations.end())
				It = m_Allocations.emplace(pAllocation, CAllocation{Bytes, 0}).first;
			if(It->second.m_References++ != 0)
				return false;
			It->second.m_Bytes = Bytes;
			m_Bytes += Bytes;
			return true;
		}
		std::size_t Bytes() const { return m_Bytes; }
		std::size_t Allocations() const { return m_Allocations.size(); }
		std::size_t Visits() const { return m_Visits; }
	};

	/** Main-thread capture of allocation identities and immutable storage owners.
	 * Workers may compare the numeric identities, but never dereference them. */
	class CStorageObservation
	{
		std::vector<std::pair<std::uintptr_t, std::size_t>> m_vAllocations;
		std::vector<std::function<void(CStorageUsage &)>> m_vOwners;

	public:
		void Add(const void *pAllocation, std::size_t Bytes)
		{
			if(pAllocation && Bytes != 0)
				m_vAllocations.emplace_back(reinterpret_cast<std::uintptr_t>(pAllocation), Bytes);
		}
		/** Only pass immutable/COW values, never runtime objects or mutable caches. */
		template<typename T>
		void Observe(const T &Value)
		{
			if constexpr(requires { Value.ObserveStorage(*this); })
				Value.ObserveStorage(*this);
			else
				m_vOwners.emplace_back([Value](CStorageUsage &Usage) { Value.Account(Usage); });
		}
		void ObserveOwned(std::function<void(CStorageUsage &)> Owner) { m_vOwners.push_back(std::move(Owner)); }
		void Account(CStorageUsage &Usage) const
		{
			for(const auto &[Identity, Bytes] : m_vAllocations)
				Usage.Add(reinterpret_cast<const void *>(Identity), Bytes);
			for(const auto &Owner : m_vOwners)
				Owner(Usage);
		}
	};

	/**
	 * A value shared by immutable revisions and detached on its first draft write.
	 * Read references and mutation callbacks are borrowed views: they must not
	 * outlive the operation using them. Update deliberately cannot return a view.
	 */
	template<typename T>
	class CSharedValue
	{
		std::shared_ptr<T> m_pValue;
		std::uint64_t m_Generation = 0;

	public:
		CSharedValue() :
			m_pValue(std::make_shared<T>())
		{
		}

		explicit CSharedValue(T Value) :
			m_pValue(std::make_shared<T>(std::move(Value)))
		{
		}

		const T &Read() const { return *m_pValue; }
		std::shared_ptr<const T> Pin() const { return m_pValue; }
		std::uint64_t Generation() const { return m_Generation; }

		template<typename F>
		void Update(F &&Function)
		{
			static_assert(std::is_void_v<std::invoke_result_t<F, T &>>, "A mutable document view must not escape its callback");
			if(m_pValue.use_count() != 1 || m_Generation == std::numeric_limits<std::uint64_t>::max())
			{
				m_pValue = std::make_shared<T>(*m_pValue);
				m_Generation = 0;
			}
			++m_Generation;
			std::invoke(std::forward<F>(Function), *m_pValue);
		}

		bool operator==(const CSharedValue &Other) const
		{
			return m_pValue == Other.m_pValue || *m_pValue == *Other.m_pValue;
		}

		bool SharesStorageWith(const CSharedValue &Other) const { return m_pValue == Other.m_pValue; }

		// Control-block/allocator overhead is implementation-dependent; report owned
		// payload and container capacity consistently, separately from process RSS.
		bool Account(CStorageUsage &Usage) const { return Usage.Add(m_pValue.get(), sizeof(T)); }
	};

	/** Detaches the whole vector on a shared write; elements can share their own records. */
	template<typename T>
	class CSharedVector
	{
		CSharedValue<std::vector<T>> m_vValues;

	public:
		CSharedVector() = default;
		explicit CSharedVector(std::vector<T> vValues) :
			m_vValues(std::move(vValues))
		{
		}

		const std::vector<T> &Read() const { return m_vValues.Read(); }
		std::shared_ptr<const std::vector<T>> Pin() const { return m_vValues.Pin(); }

		template<typename F>
		void Update(F &&Function)
		{
			m_vValues.Update(std::forward<F>(Function));
		}

		bool operator==(const CSharedVector &) const = default;
		bool SharesStorageWith(const CSharedVector &Other) const { return m_vValues.SharesStorageWith(Other.m_vValues); }

		bool Account(CStorageUsage &Usage) const
		{
			if(!m_vValues.Account(Usage))
				return false;
			const auto &vValues = Read();
			if(vValues.capacity() != 0)
				Usage.Add(vValues.data(), vValues.capacity() * sizeof(T));
			return true;
		}
	};

} // namespace editor_history

#endif
