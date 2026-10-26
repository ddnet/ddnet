#ifndef GAME_EDITOR_HISTORY_SAVE_STATE_H
#define GAME_EDITOR_HISTORY_SAVE_STATE_H

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace editor_history
{

	enum class ESaveKind
	{
		MANUAL,
		AUTOMATIC,
		COPY,
	};

	/** Independent saved-content markers for one open-document lifetime. */
	template<typename T>
	class CSaveState
	{
		class CState
		{
		public:
			std::uint64_t m_NextRequest = 1;
			std::uint64_t m_LastManual = 0;
			std::uint64_t m_LastAutomatic = 0;
			std::shared_ptr<const std::string> m_Manual;
			std::shared_ptr<const std::string> m_Automatic;
		};
		std::shared_ptr<CState> m_pState = std::make_shared<CState>();

	public:
		class CTicket
		{
			friend class CSaveState;
			std::weak_ptr<CState> m_pState;
			std::uint64_t m_Request = 0;
			ESaveKind m_Kind = ESaveKind::MANUAL;
			std::shared_ptr<const std::string> m_pPersistedKey;
			CTicket() = default;

		public:
			std::shared_ptr<const T> m_pRoot;
			std::string m_Destination;
			std::uint64_t Request() const { return m_Request; }
			ESaveKind Kind() const { return m_Kind; }
		};

		CSaveState() = default;
		CSaveState(const CSaveState &) = delete;
		CSaveState &operator=(const CSaveState &) = delete;

		/** Loaded contents are the only baseline allowed to establish both markers. */
		bool InitializeLoaded(std::string Key)
		{
			if(m_pState->m_NextRequest != 1)
				return false;
			auto pKey = std::make_shared<const std::string>(std::move(Key));
			m_pState->m_Manual = pKey;
			m_pState->m_Automatic = std::move(pKey);
			return true;
		}

		std::optional<CTicket> Capture(std::shared_ptr<const T> pRoot, std::string Key, std::string Destination, ESaveKind Kind)
		{
			if(!pRoot || m_pState->m_NextRequest == std::numeric_limits<std::uint64_t>::max())
				return std::nullopt;
			CTicket Ticket;
			Ticket.m_pState = m_pState;
			// Prepare marker ownership before the job is queued. Completion only
			// publishes this immutable owner and cannot allocate after file replace.
			Ticket.m_pPersistedKey = std::make_shared<const std::string>(std::move(Key));
			Ticket.m_Request = m_pState->m_NextRequest++;
			Ticket.m_Kind = Kind;
			Ticket.m_pRoot = std::move(pRoot);
			Ticket.m_Destination = std::move(Destination);
			return Ticket;
		}

		bool Owns(const CTicket &Ticket) const noexcept { return Ticket.m_pState.lock() == m_pState; }
		/** Call only after the final replacement succeeded. Never clears another lifetime. */
		bool Complete(const CTicket &Ticket, bool Success) noexcept
		{
			if(!Success || !Owns(Ticket) || Ticket.m_Kind == ESaveKind::COPY)
				return false;
			auto &Last = Ticket.m_Kind == ESaveKind::MANUAL ? m_pState->m_LastManual : m_pState->m_LastAutomatic;
			if(Ticket.m_Request <= Last)
				return false;
			auto &Marker = Ticket.m_Kind == ESaveKind::MANUAL ? m_pState->m_Manual : m_pState->m_Automatic;
			Marker = Ticket.m_pPersistedKey;
			Last = Ticket.m_Request;
			return true;
		}

		bool Dirty(const std::string &CurrentKey, ESaveKind Kind) const
		{
			const auto &Marker = Kind == ESaveKind::MANUAL ? m_pState->m_Manual : m_pState->m_Automatic;
			return !Marker || *Marker != CurrentKey;
		}
	};

} // namespace editor_history

#endif
