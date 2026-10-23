#ifndef GAME_EDITOR_HISTORY_RESOURCE_BLOB_H
#define GAME_EDITOR_HISTORY_RESOURCE_BLOB_H

#include "shared_value.h"

#include <cstdint>
#include <span>
#include <vector>

namespace editor_history
{

	/** Retained source contents. No API exposes a mutable alias to these bytes. */
	class CResourceBlob
	{
		CSharedVector<std::uint8_t> m_Bytes;

	public:
		CResourceBlob() = default;
		explicit CResourceBlob(std::span<const std::uint8_t> Bytes) :
			m_Bytes(Bytes.empty() ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(Bytes.begin(), Bytes.end()))
		{
		}

		std::span<const std::uint8_t> Bytes() const { return m_Bytes.Read(); }
		// The ownership token cannot be confused with a recycled allocation address.
		std::weak_ptr<const void> StorageIdentity() const { return m_Bytes.Pin(); }
		bool operator==(const CResourceBlob &) const = default;
		void Account(CStorageUsage &Usage) const { m_Bytes.Account(Usage); }
		bool SharesStorageWith(const CResourceBlob &Other) const { return m_Bytes.SharesStorageWith(Other.m_Bytes); }
	};

} // namespace editor_history

#endif
