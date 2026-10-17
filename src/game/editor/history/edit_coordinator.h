#ifndef GAME_EDITOR_HISTORY_EDIT_COORDINATOR_H
#define GAME_EDITOR_HISTORY_EDIT_COORDINATOR_H

#include "revision_history.h"

#include <base/dbg.h>

namespace editor_history
{

	enum class EEditCompletion
	{
		ACCEPT,
		POINTER_RELEASE,
		KEY_RELEASE,
		VALID_BLUR,
		TOOL_SWITCH,
		MANUAL_SAVE,
		DOCUMENT_CLOSE,
	};

	enum class EEditCancellation
	{
		ESCAPE,
		CAPTURE_LOST,
		FOCUS_LOST,
		OWNER_DESTROYED,
		INVALID_INPUT,
		FAILURE,
		UNDO,
	};

	/**
	 * Owns semantic edit lifetimes around a runtime binding of document values.
	 * Capture runs only when completing an edit. Restore runs only at an explicit
	 * safe point, after all callbacks borrowing the previous bindings returned.
	 * Update changes the live binding; the history draft receives captured values
	 * at Complete. Failed completion queues the immutable current root for rollback.
	 */
	template<typename T>
	class CEditCoordinator
	{
	public:
		using CHistory = CRevisionHistory<T>;
		using FCapture = std::function<std::optional<T>(const T &, std::string &)>;
		using FRestore = std::function<bool(const T &)>;
		using FBegin = std::function<void()>;
		using FError = std::function<void(const char *)>;

	private:
		class CState
		{
		public:
			std::unique_ptr<CHistory> m_pHistory;
			FCapture m_Capture;
			FRestore m_Restore;
			FBegin m_Begin;
			FError m_Error;
			std::optional<typename CHistory::CEditToken> m_Token;
			std::uint64_t m_NextOwner = 1;
			std::uint64_t m_Owner = 0;
			bool m_Publishing = false;
			bool m_InCallback = false;
			bool m_RestoreCurrent = false;
			std::optional<EEditCompletion> m_LastCompletion;
			std::optional<EEditCancellation> m_LastCancellation;

			bool Busy() const { return m_Publishing || m_InCallback; }
			void Report(const char *pMessage)
			{
				m_Error(pMessage);
			}
			bool Pending() const { return m_RestoreCurrent || m_pHistory->HasPendingPublication(); }
			bool Owns(std::uint64_t Owner) const
			{
				return Owner != 0 && m_Owner == Owner && m_Token && m_pHistory->HasActiveEdit();
			}
			bool Cancel(std::uint64_t Owner, EEditCancellation Reason)
			{
				if(m_Publishing || Pending() || !Owns(Owner))
					return false;
				m_LastCancellation = Reason;
				return m_pHistory->RequestCancel();
			}
			void RetireOwner()
			{
				m_Token.reset();
				m_Owner = 0;
			}
			void FailedCompletion(const char *pError)
			{
				// The core may already have cancelled its draft after a failed commit.
				// Its immutable current root remains the authoritative recovery target.
				RetireOwner();
				m_RestoreCurrent = true;
				m_LastCancellation = EEditCancellation::FAILURE;
				Report(pError);
			}
		};

		class CCallbackScope
		{
			bool &m_Flag;

		public:
			explicit CCallbackScope(bool &Flag) :
				m_Flag(Flag) { m_Flag = true; }
			~CCallbackScope() { m_Flag = false; }
		};

		std::shared_ptr<CState> m_pState;

	public:
		/** A document-bound owner. Its destruction requests cancellation, never commit. */
		class COwner
		{
			friend class CEditCoordinator;
			std::weak_ptr<CState> m_pState;
			std::uint64_t m_Id = 0;
			COwner(const std::shared_ptr<CState> &pState, std::uint64_t Id) :
				m_pState(pState), m_Id(Id) {}

		public:
			COwner(const COwner &) = delete;
			COwner &operator=(const COwner &) = delete;
			COwner(COwner &&Other) noexcept :
				m_pState(std::move(Other.m_pState)), m_Id(std::exchange(Other.m_Id, 0)) {}
			COwner &operator=(COwner &&Other) noexcept
			{
				if(this != &Other)
				{
					Cancel(EEditCancellation::OWNER_DESTROYED);
					m_pState = std::move(Other.m_pState);
					m_Id = std::exchange(Other.m_Id, 0);
				}
				return *this;
			}
			~COwner() { Cancel(EEditCancellation::OWNER_DESTROYED); }
			bool Cancel(EEditCancellation Reason)
			{
				const auto pState = m_pState.lock();
				return pState && pState->Cancel(m_Id, Reason);
			}
			bool Active() const
			{
				const auto pState = m_pState.lock();
				return pState && pState->Owns(m_Id) && !pState->Pending();
			}
		};

		CEditCoordinator(std::unique_ptr<CHistory> pHistory, FCapture Capture, FRestore Restore, FBegin Begin, FError Error) :
			m_pState(std::make_shared<CState>())
		{
			dbg_assert(pHistory != nullptr, "Editor edit coordinator requires a history");
			m_pState->m_pHistory = std::move(pHistory);
			m_pState->m_Capture = std::move(Capture);
			m_pState->m_Restore = std::move(Restore);
			m_pState->m_Begin = std::move(Begin);
			m_pState->m_Error = std::move(Error);
		}
		CEditCoordinator(const CEditCoordinator &) = delete;
		CEditCoordinator &operator=(const CEditCoordinator &) = delete;

		const CHistory &History() const { return *m_pState->m_pHistory; }
		bool HasActiveEdit() const { return History().HasActiveEdit(); }
		bool HasPendingPublication() const { return m_pState->Pending(); }
		bool Publishing() const { return m_pState->m_Publishing; }
		bool Dirty(ESaveKind Kind) const { return m_pState->m_RestoreCurrent || History().Dirty(Kind); }
		std::optional<EEditCompletion> LastCompletion() const { return m_pState->m_LastCompletion; }
		std::optional<EEditCancellation> LastCancellation() const { return m_pState->m_LastCancellation; }

		std::optional<COwner> BeginEdit(std::string Label, ECategory Category)
		{
			auto &State = *m_pState;
			if(State.Busy() || State.Pending() || HasActiveEdit() || State.m_NextOwner == std::numeric_limits<std::uint64_t>::max())
				return std::nullopt;
			State.m_Token = State.m_pHistory->BeginEdit(std::move(Label), Category);
			if(!State.m_Token)
				return std::nullopt;
			State.m_Owner = State.m_NextOwner++;
			CCallbackScope Scope(State.m_InCallback);
			State.m_Begin();
			return COwner(m_pState, State.m_Owner);
		}

		/** Helpers must receive and join the initiating owner explicitly. */
		template<typename F>
		bool Update(COwner &Owner, F &&Function)
		{
			static_assert(std::is_void_v<std::invoke_result_t<F>>, "Mutable runtime views may not escape the edit callback");
			auto &State = *m_pState;
			if(State.m_Publishing || State.Pending() || Owner.m_pState.lock() != m_pState || !State.Owns(Owner.m_Id))
				return false;
			// Joining is explicit, so nested domain helpers may use the same owner.
			const bool WasInCallback = State.m_InCallback;
			State.m_InCallback = true;
			std::invoke(std::forward<F>(Function));
			State.m_InCallback = WasInCallback;
			return !State.Pending();
		}

		/** Capture the resulting values, then commit or preserve a no-op's redo branch. */
		ECommitResult Complete(COwner &Owner, EEditCompletion Reason)
		{
			auto &State = *m_pState;
			if(State.Busy() || State.Pending() || Owner.m_pState.lock() != m_pState || !State.Owns(Owner.m_Id))
				return ECommitResult::INVALID_OWNER;
			CCallbackScope Scope(State.m_InCallback);
			std::string Error;
			auto Candidate = State.m_Capture(History().Current(), Error);
			if(!Candidate)
			{
				State.FailedCompletion(Error.empty() ? "Could not capture the completed document edit" : Error.c_str());
				return ECommitResult::FAILED;
			}
			State.m_pHistory->Update(*State.m_Token, [&](T &Draft) { Draft = std::move(*Candidate); });
			const auto Result = State.m_pHistory->Commit(*State.m_Token);
			if(Result == ECommitResult::FAILED || Result == ECommitResult::INVALID_OWNER)
				State.FailedCompletion("Could not retain the completed document edit; its original contents will be restored");
			else
			{
				State.RetireOwner();
				State.m_LastCompletion = Reason;
			}
			return Result;
		}

		template<typename F>
		ECommitResult Edit(std::string Label, ECategory Category, F &&Function)
		{
			auto Owner = BeginEdit(std::move(Label), Category);
			if(!Owner)
				return ECommitResult::INVALID_OWNER;
			if(!Update(*Owner, std::forward<F>(Function)))
				return ECommitResult::FAILED;
			return Complete(*Owner, EEditCompletion::ACCEPT);
		}

		bool CancelActive(EEditCancellation Reason)
		{
			return !m_pState->Busy() && m_pState->Cancel(m_pState->m_Owner, Reason);
		}
		ECommitResult CompleteActive(EEditCompletion Reason)
		{
			if(m_pState->Busy() || m_pState->Pending() || !HasActiveEdit())
				return ECommitResult::INVALID_OWNER;
			COwner Owner(m_pState, m_pState->m_Owner);
			return Complete(Owner, Reason);
		}

		bool RequestUndo()
		{
			if(m_pState->Busy() || m_pState->m_RestoreCurrent)
				return false;
			if(HasActiveEdit())
				m_pState->m_LastCancellation = EEditCancellation::UNDO;
			return m_pState->m_pHistory->RequestUndo();
		}
		bool RequestRedo()
		{
			return !m_pState->Busy() && !m_pState->m_RestoreCurrent && m_pState->m_pHistory->RequestRedo();
		}
		bool RequestRevision(std::size_t Index)
		{
			return !m_pState->Busy() && !m_pState->m_RestoreCurrent && m_pState->m_pHistory->RequestRevision(Index);
		}
		bool AbandonPendingTraversal()
		{
			return !m_pState->Busy() && !m_pState->m_RestoreCurrent && m_pState->m_pHistory->AbandonPendingTraversal();
		}

		bool PublishAtSafePoint()
		{
			auto &State = *m_pState;
			if(State.Busy() || !State.Pending())
				return false;
			CCallbackScope Scope(State.m_Publishing);
			bool Published = false;
			if(State.m_RestoreCurrent)
			{
				Published = State.m_Restore(History().Current());
				if(Published)
					State.m_RestoreCurrent = false;
			}
			else
				Published = State.m_pHistory->PublishPendingWith(State.m_Restore);
			if(Published && !HasActiveEdit())
				State.RetireOwner();
			return Published;
		}

		bool SetRetentionLimits(CRetentionLimits Limits)
		{
			return !m_pState->Busy() && !m_pState->Pending() && m_pState->m_pHistory->SetRetentionLimits(Limits);
		}

		bool Clear()
		{
			return !m_pState->Busy() && !m_pState->Pending() && m_pState->m_pHistory->Clear();
		}
		std::optional<typename CHistory::CSaveTicket> CaptureSave(std::string Destination, ESaveKind Kind)
		{
			if(m_pState->Busy() || m_pState->Pending())
				return std::nullopt;
			return m_pState->m_pHistory->CaptureSave(std::move(Destination), Kind);
		}
		bool CompleteSave(const typename CHistory::CSaveTicket &Ticket, bool Success)
		{
			return !m_pState->Busy() && m_pState->m_pHistory->CompleteSave(Ticket, Success);
		}
	};

} // namespace editor_history

#endif
