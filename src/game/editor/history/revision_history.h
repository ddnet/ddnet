#ifndef GAME_EDITOR_HISTORY_REVISION_HISTORY_H
#define GAME_EDITOR_HISTORY_REVISION_HISTORY_H

#include "save_state.h"
#include "shared_value.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace editor_history
{

	enum class ECategory
	{
		MAP,
		ENVELOPE,
		SETTINGS,
	};

	enum class ECommitResult
	{
		CHANGED,
		UNCHANGED,
		FAILED,
		INVALID_OWNER,
	};

	class CRetentionLimits
	{
	public:
		std::size_t m_Entries;
		std::size_t m_Bytes;
	};

	/**
	 * Headless, per-document transactional timeline. T is the authoritative value
	 * record, with value equality and owned/COW storage only. Runtime bindings
	 * publish pending traversal after callbacks have released their borrowed views.
	 * All operations run on the document's owning thread.
	 * Every revision owns a complete logical root, sharing unchanged allocations;
	 * there is no action replay or inverse-command dispatch. FValidate checks graph
	 * invariants, FPersistedKey describes file contents, and FAccount visits owned
	 * storage. Equality can change without changing FPersistedKey (authoring state).
	 */
	template<typename T>
	class CRevisionHistory
	{
	public:
		using FValidate = std::function<bool(const T &)>;
		using FPersistedKey = std::function<std::string(const T &)>;
		using FAccount = std::function<void(const T &, CStorageUsage &)>;

		class CRevision
		{
		public:
			std::shared_ptr<const T> m_pRoot;
			std::string m_Label;
			ECategory m_Category = ECategory::MAP;
			std::string m_PersistedKey;
		};

	private:
		class CActiveEdit
		{
		public:
			std::uint64_t m_Owner;
			std::unique_ptr<T> m_pDraft;
			std::string m_Label;
			ECategory m_Category;
		};

		class CState
		{
		public:
			std::vector<CRevision> m_vRevisions;
			std::size_t m_Cursor = 0;
			std::optional<CActiveEdit> m_Active;
			std::optional<std::size_t> m_PendingCursor;
			bool m_PendingCancel = false;
			std::uint64_t m_NextOwner = 1;
			std::uint64_t m_NextObjectId = 1;
			std::shared_ptr<CSaveState<T>> m_pSaves = std::make_shared<CSaveState<T>>();
			CRetentionLimits m_Limits;
			std::size_t m_RetainedBytes = 0;
			bool m_Oversized = false;
			FValidate m_Validate;
			FPersistedKey m_PersistedKey;
			FAccount m_Account;
			CStorageUsage m_Usage;

			void Cancel(std::uint64_t Owner)
			{
				if(m_Active && m_Active->m_Owner == Owner)
				{
					m_Active.reset();
					m_PendingCancel = false;
				}
			}

			void AccountRoot(const CRevision &Revision, bool Remove)
			{
				m_Usage.Removing(Remove);
				if(m_Usage.Add(Revision.m_pRoot.get(), sizeof(T)))
					m_Account(*Revision.m_pRoot, m_Usage);
			}

			std::size_t AccountMetadata(const std::vector<CRevision> &vRevisions, std::size_t First) const
			{
				CStorageUsage Usage;
				Usage.Add(vRevisions.data(), (First == 0 ? vRevisions.capacity() : vRevisions.size() - First) * sizeof(CRevision));
				for(std::size_t Index = First; Index < vRevisions.size(); ++Index)
				{
					const auto &Revision = vRevisions[Index];
					const auto AccountString = [&](const std::string &String) {
						const auto Data = reinterpret_cast<std::uintptr_t>(String.data());
						const auto Object = reinterpret_cast<std::uintptr_t>(&String);
						if(Data < Object || Data >= Object + sizeof(String)) // NOLINT(bugprone-sizeof-container): Detect inline storage within the string object.
							Usage.Add(String.data(), String.capacity() + 1);
					};
					AccountString(Revision.m_Label);
					AccountString(Revision.m_PersistedKey);
				}
				return Usage.Bytes();
			}
		};

		std::shared_ptr<CState> m_pState;

	public:
		/** An abandoned or invalidated owner cancels its draft without branching. */
		class CEditToken
		{
			friend class CRevisionHistory;
			std::weak_ptr<CState> m_pState;
			std::uint64_t m_Owner = 0;

			CEditToken(const std::shared_ptr<CState> &pState, std::uint64_t Owner) :
				m_pState(pState), m_Owner(Owner)
			{
			}

		public:
			CEditToken(const CEditToken &) = delete;
			CEditToken &operator=(const CEditToken &) = delete;
			CEditToken(CEditToken &&Other) noexcept :
				m_pState(std::move(Other.m_pState)),
				m_Owner(std::exchange(Other.m_Owner, 0))
			{
			}

			CEditToken &operator=(CEditToken &&Other) noexcept
			{
				if(this != &Other)
				{
					Cancel();
					m_pState = std::move(Other.m_pState);
					m_Owner = std::exchange(Other.m_Owner, 0);
				}
				return *this;
			}

			~CEditToken() { Cancel(); }

			void Cancel()
			{
				if(auto pState = m_pState.lock())
					pState->Cancel(m_Owner);
				m_Owner = 0;
			}
		};

		/** Retains exact export input; completion is bound to this document lifetime. */
		using CSaveTicket = typename CSaveState<T>::CTicket;

	private:
		CRevisionHistory(T Initial, CRetentionLimits Limits, FValidate Validate, FPersistedKey PersistedKey, FAccount Account) :
			m_pState(std::make_shared<CState>())
		{
			auto &State = *m_pState;
			State.m_Limits = Limits;
			State.m_Validate = std::move(Validate);
			State.m_PersistedKey = std::move(PersistedKey);
			State.m_Account = std::move(Account);
			State.m_vRevisions.push_back({std::make_shared<const T>(std::move(Initial)), {}, ECategory::MAP, {}});
			State.m_vRevisions.front().m_PersistedKey = State.m_PersistedKey(Current());
			State.AccountRoot(State.m_vRevisions.front(), false);
			State.m_RetainedBytes = State.m_Usage.Bytes() + State.AccountMetadata(State.m_vRevisions, 0);
			State.m_Oversized = State.m_RetainedBytes > Limits.m_Bytes;
		}

	public:
		static std::unique_ptr<CRevisionHistory> Create(T Initial, CRetentionLimits Limits, FValidate Validate, FPersistedKey PersistedKey, FAccount Account)
		{
			if(!Validate(Initial))
				return nullptr;
			return std::unique_ptr<CRevisionHistory>(new CRevisionHistory(std::move(Initial), Limits, std::move(Validate), std::move(PersistedKey), std::move(Account)));
		}

		~CRevisionHistory() = default;
		CRevisionHistory(const CRevisionHistory &) = delete;
		CRevisionHistory &operator=(const CRevisionHistory &) = delete;
		CRevisionHistory(CRevisionHistory &&) = delete;
		CRevisionHistory &operator=(CRevisionHistory &&) = delete;

		const T &Current() const { return *m_pState->m_vRevisions[m_pState->m_Cursor].m_pRoot; }
		const T &Preview() const { return m_pState->m_Active ? *m_pState->m_Active->m_pDraft : Current(); }
		std::shared_ptr<const T> PinCurrent() const { return m_pState->m_vRevisions[m_pState->m_Cursor].m_pRoot; }
		const std::vector<CRevision> &Revisions() const { return m_pState->m_vRevisions; }
		std::size_t Cursor() const { return m_pState->m_Cursor; }
		bool HasActiveEdit() const { return m_pState->m_Active.has_value(); }
		bool CanUndo() const { return HasActiveEdit() || Cursor() > 0; }
		bool CanRedo() const { return !HasActiveEdit() && Cursor() + 1 < Revisions().size(); }
		std::size_t RetainedBytes() const { return m_pState->m_RetainedBytes; }
		std::size_t AccountingVisits() const { return m_pState->m_Usage.Visits(); }
		std::size_t RetainedPayloadBytes() const { return m_pState->m_Usage.Bytes(); }
		bool Oversized() const { return m_pState->m_Oversized; }
		std::shared_ptr<CSaveState<T>> Saves() const { return m_pState->m_pSaves; }
		/** Bind the map's existing lifetime/markers before the first user edit. */
		bool UseSaveState(std::shared_ptr<CSaveState<T>> pSaves)
		{
			if(!pSaves || m_pState->m_NextOwner != 1 || HasActiveEdit() || HasPendingPublication())
				return false;
			m_pState->m_pSaves = std::move(pSaves);
			return true;
		}

		CRetentionLimits Limits() const { return m_pState->m_Limits; }
		/** Preferences never discard a redo branch. Trim its oldest prefix while
		 * retaining the current state and its immediately preceding state. */
		bool SetRetentionLimits(CRetentionLimits Limits)
		{
			if(HasActiveEdit() || HasPendingPublication())
				return false;
			auto &State = *m_pState;
			CStorageUsage::CTransaction Accounting(State.m_Usage);
			std::size_t First = 0;
			const std::size_t MaxFirst = State.m_Cursor > 0 ? State.m_Cursor - 1 : 0;
			const std::size_t Entries = std::max<std::size_t>(1, Limits.m_Entries);
			while(First < MaxFirst && (State.m_vRevisions.size() - First - 1 > Entries || State.m_Usage.Bytes() + State.AccountMetadata(State.m_vRevisions, First) > Limits.m_Bytes))
				State.AccountRoot(State.m_vRevisions[First++], true);
			std::vector<CRevision> vRetained(State.m_vRevisions.begin() + First, State.m_vRevisions.end());
			vRetained.front().m_Label.clear();
			const auto Bytes = State.m_Usage.Bytes() + State.AccountMetadata(vRetained, 0);
			Accounting.Commit();
			State.m_vRevisions.swap(vRetained);
			State.m_Cursor -= First;
			State.m_Limits = Limits;
			State.m_RetainedBytes = Bytes;
			State.m_Oversized = Bytes > Limits.m_Bytes;
			return true;
		}

		void AccountRetainedPayload(CStorageUsage &Usage) const
		{
			for(const auto &Revision : Revisions())
				if(Usage.Add(Revision.m_pRoot.get(), sizeof(T)))
					m_pState->m_Account(*Revision.m_pRoot, Usage);
		}
		void AccountDraft(CStorageUsage &Usage) const
		{
			if(m_pState->m_Active && Usage.Add(m_pState->m_Active->m_pDraft.get(), sizeof(T)))
				m_pState->m_Account(*m_pState->m_Active->m_pDraft, Usage);
		}
		void ObserveDraft(CStorageObservation &Observation) const
		{
			if(m_pState->m_Active)
			{
				Observation.Add(m_pState->m_Active->m_pDraft.get(), sizeof(T));
				Observation.Observe(*m_pState->m_Active->m_pDraft);
			}
		}

		/** Import has validated this baseline and supplied the highest existing ID. */
		bool InitializeLoadedBaseline(std::uint64_t HighestObjectId)
		{
			if(m_pState->m_NextOwner != 1 ||
				HasActiveEdit() || m_pState->m_PendingCursor ||
				HighestObjectId == std::numeric_limits<std::uint64_t>::max())
				return false;
			const auto &Key = Revisions().front().m_PersistedKey;
			if(!m_pState->m_pSaves->InitializeLoaded(Key))
				return false;
			m_pState->m_NextObjectId = std::max(m_pState->m_NextObjectId, HighestObjectId + 1);
			return true;
		}

		std::optional<std::uint64_t> AllocateObjectId()
		{
			if(m_pState->m_NextObjectId == std::numeric_limits<std::uint64_t>::max())
				return std::nullopt;
			return m_pState->m_NextObjectId++;
		}

		std::optional<CEditToken> BeginEdit(std::string Label, ECategory Category)
		{
			auto &State = *m_pState;
			if(HasActiveEdit() || State.m_PendingCursor || State.m_PendingCancel)
				return std::nullopt;
			if(State.m_NextOwner == std::numeric_limits<std::uint64_t>::max())
				return std::nullopt;
			const auto Owner = State.m_NextOwner++;
			State.m_Active.emplace(Owner, std::make_unique<T>(Current()), std::move(Label), Category);
			return CEditToken(m_pState, Owner);
		}

		/** Helpers explicitly join the initiating token before updating its draft. */
		template<typename F>
		bool Update(CEditToken &Token, F &&Function)
		{
			static_assert(std::is_void_v<std::invoke_result_t<F, T &>>, "A document draft must not escape its callback");
			if(!Owns(Token) || m_pState->m_PendingCancel)
				return false;
			std::invoke(std::forward<F>(Function), *m_pState->m_Active->m_pDraft);
			return true;
		}

		ECommitResult Commit(CEditToken &Token)
		{
			if(!Owns(Token) || m_pState->m_PendingCancel)
				return ECommitResult::INVALID_OWNER;
			auto &State = *m_pState;
			const auto &Edit = *State.m_Active;
			if(!State.m_Validate(*Edit.m_pDraft))
			{
				Token.Cancel();
				return ECommitResult::FAILED;
			}
			if(*Edit.m_pDraft == Current())
			{
				Token.Cancel();
				return ECommitResult::UNCHANGED;
			}

			// Stage every allocation, export key, and eviction decision before
			// changing the published cursor or discarding a redo branch.
			std::vector<CRevision> vCandidate(State.m_vRevisions.begin(), State.m_vRevisions.begin() + State.m_Cursor + 1);
			vCandidate.push_back({std::make_shared<const T>(*Edit.m_pDraft), Edit.m_Label, Edit.m_Category, State.m_PersistedKey(*Edit.m_pDraft)});
			const std::size_t MaxStates = std::max<std::size_t>(1, State.m_Limits.m_Entries) == std::numeric_limits<std::size_t>::max() ?
							      std::numeric_limits<std::size_t>::max() :
							      std::max<std::size_t>(1, State.m_Limits.m_Entries) + 1;
			CStorageUsage::CTransaction Accounting(State.m_Usage);
			State.AccountRoot(vCandidate.back(), false);
			for(std::size_t Index = State.m_Cursor + 1; Index < State.m_vRevisions.size(); ++Index)
				State.AccountRoot(State.m_vRevisions[Index], true);
			std::size_t First = 0;
			while(First + 2 < vCandidate.size() && (vCandidate.size() - First > MaxStates || State.m_Usage.Bytes() + State.AccountMetadata(vCandidate, First) > State.m_Limits.m_Bytes))
				State.AccountRoot(vCandidate[First++], true);
			std::vector<CRevision> vRetained(std::make_move_iterator(vCandidate.begin() + First), std::make_move_iterator(vCandidate.end()));
			vRetained.front().m_Label.clear();
			const auto Bytes = State.m_Usage.Bytes() + State.AccountMetadata(vRetained, 0);
			Accounting.Commit();
			State.m_vRevisions.swap(vRetained);
			State.m_Cursor = State.m_vRevisions.size() - 1;
			State.m_RetainedBytes = Bytes;
			State.m_Oversized = Bytes > State.m_Limits.m_Bytes;
			Token.Cancel();
			return ECommitResult::CHANGED;
		}

		bool RequestUndo()
		{
			if(HasActiveEdit())
			{
				m_pState->m_PendingCancel = true;
				return true;
			}
			const auto Cursor = m_pState->m_PendingCursor.value_or(m_pState->m_Cursor);
			if(Cursor == 0)
				return false;
			m_pState->m_PendingCursor = Cursor - 1;
			return true;
		}

		/** Escape/interruption queues cancellation without traversing older history. */
		bool RequestCancel()
		{
			if(!HasActiveEdit())
				return false;
			m_pState->m_PendingCancel = true;
			return true;
		}

		bool RequestRedo()
		{
			if(HasActiveEdit())
				return false;
			const auto Cursor = m_pState->m_PendingCursor.value_or(m_pState->m_Cursor);
			if(Cursor + 1 >= Revisions().size())
				return false;
			m_pState->m_PendingCursor = Cursor + 1;
			return true;
		}

		bool RequestRevision(std::size_t Index)
		{
			if(HasActiveEdit() || Index >= Revisions().size())
				return false;
			m_pState->m_PendingCursor = Index;
			return true;
		}

		/** A failed traversal can be dismissed so a recoverable error does not lock editing. */
		bool AbandonPendingTraversal()
		{
			if(!m_pState->m_PendingCursor || m_pState->m_PendingCancel)
				return false;
			m_pState->m_PendingCursor.reset();
			return true;
		}

		bool HasPendingPublication() const { return m_pState->m_PendingCursor.has_value() || m_pState->m_PendingCancel; }

		/**
		 * Stage and publish runtime bindings before changing the history cursor.
		 * A failed binding leaves both the pending request and current state intact.
		 * Call only once prior-state runtime/UI borrowers have returned.
		 */
		template<typename F>
		bool PublishPendingWith(F &&Publish)
		{
			auto &State = *m_pState;
			if(State.m_PendingCancel)
			{
				if(!std::invoke(Publish, Current()))
					return false;
				State.m_Active.reset();
				State.m_PendingCancel = false;
				return true;
			}
			if(!State.m_PendingCursor)
				return false;
			const bool Changed = State.m_Cursor != *State.m_PendingCursor;
			if(Changed && !std::invoke(Publish, *State.m_vRevisions[*State.m_PendingCursor].m_pRoot))
				return false;
			State.m_Cursor = *State.m_PendingCursor;
			State.m_PendingCursor.reset();
			return Changed;
		}

		bool PublishPending()
		{
			return PublishPendingWith([](const T &) { return true; });
		}

		/** Clearing history never resets save keys or the object identity allocator. */
		bool Clear()
		{
			if(HasActiveEdit() || m_pState->m_PendingCursor)
				return false;
			std::vector<CRevision> vCandidate{Revisions()[Cursor()]};
			vCandidate.front().m_Label.clear();
			CStorageUsage::CTransaction Accounting(m_pState->m_Usage);
			for(std::size_t Index = 0; Index < Revisions().size(); ++Index)
				if(Index != Cursor())
					m_pState->AccountRoot(Revisions()[Index], true);
			const auto Bytes = m_pState->m_Usage.Bytes() + m_pState->AccountMetadata(vCandidate, 0);
			Accounting.Commit();
			m_pState->m_vRevisions.swap(vCandidate);
			m_pState->m_Cursor = 0;
			m_pState->m_RetainedBytes = Bytes;
			m_pState->m_Oversized = Bytes > m_pState->m_Limits.m_Bytes;
			return true;
		}

		bool Dirty(ESaveKind Kind) const
		{
			if(HasActiveEdit())
				return true;
			return m_pState->m_pSaves->Dirty(Revisions()[Cursor()].m_PersistedKey, Kind);
		}

		/** The caller settles manual edits; autosave simply defers an active draft. */
		std::optional<CSaveTicket> CaptureSave(std::string Destination, ESaveKind Kind)
		{
			if(HasActiveEdit() || m_pState->m_PendingCursor || m_pState->m_PendingCancel)
				return std::nullopt;
			return m_pState->m_pSaves->Capture(PinCurrent(), Revisions()[Cursor()].m_PersistedKey, std::move(Destination), Kind);
		}

		/** Call after successful file replacement, never on enqueue. */
		bool CompleteSave(const CSaveTicket &Ticket, bool Success)
		{
			return m_pState->m_pSaves->Complete(Ticket, Success);
		}

	private:
		bool Owns(const CEditToken &Token) const
		{
			return Token.m_pState.lock() == m_pState && m_pState->m_Active && m_pState->m_Active->m_Owner == Token.m_Owner;
		}
	};

} // namespace editor_history

#endif
