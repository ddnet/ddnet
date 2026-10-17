#ifndef GAME_EDITOR_HISTORY_DOCUMENT_HISTORY_H
#define GAME_EDITOR_HISTORY_DOCUMENT_HISTORY_H

#include "edit_coordinator.h"

#include <game/editor/map_object.h>
#include <game/editor/mapitems/document_graph.h>

enum class EEditState;

/**
 * Tool-facing adapter for one map's chronological history, including envelopes
 * and server settings.
 * Owner keys are opaque interaction identities (often UI/tool pointers), not
 * persistent document IDs. Keep the same key for every update of a gesture.
 */
class CEditorDocumentHistory : public CMapObject
{
public:
	using CCoordinator = editor_history::CEditCoordinator<CEditorDocumentValues>;
	using CHistory = CCoordinator::CHistory;
	using ECategory = editor_history::ECategory;
	using ECompletion = editor_history::EEditCompletion;
	using ECancellation = editor_history::EEditCancellation;

	explicit CEditorDocumentHistory(CEditorMap *pMap);
	~CEditorDocumentHistory() override;
	bool Initialize();
	bool Ready() const { return m_pCoordinator != nullptr; }
	const CHistory *History() const { return Ready() ? &m_pCoordinator->History() : nullptr; }
	bool Active() const { return Ready() && m_pCoordinator->HasActiveEdit(); }
	bool Pending() const { return Ready() && m_pCoordinator->HasPendingPublication(); }
	bool Publishing() const { return Ready() && m_pCoordinator->Publishing(); }
	bool Owns(const void *pOwner) const { return Active() && !Pending() && m_pOwnerKey == pOwner; }
	/** Begin before mutation; rejoins this owner or completes a different owner. */
	bool Begin(const void *pOwner, const char *pLabel, ECategory Category);
	/** Adapt widget state; call EndControl after applying the reported value. */
	bool BeginControl(const void *pOwner, const char *pLabel, EEditState State, ECategory Category = ECategory::MAP);
	void EndControl(const void *pOwner, EEditState State);
	/** Capture once at completion. Success includes a no-op with no new revision. */
	bool Complete(const void *pOwner, ECompletion Reason);
	bool CompleteActive(ECompletion Reason);
	/** Cancel/Undo/Redo request restoration; they do not replace runtime objects here. */
	bool Cancel(ECancellation Reason);
	bool Undo();
	bool Redo();
	bool CanUndo() const;
	bool CanRedo() const;
	bool GoTo(std::size_t Revision);
	bool Clear();
	/** May replace runtime objects. Call only after UI callbacks release their borrows. */
	bool PublishAtSafePoint();
	void RefreshPresentation();
	void KeyReleased(int Key);
	void StartFrame();
	void ApplyPreferences();
	struct SMemoryUsage
	{
		bool m_Sampled = false;
		bool m_Current = false;
		bool m_Updating = false;
		std::uint64_t m_Generation = 0;
		std::uint64_t m_SamplesStarted = 0;
		std::size_t m_LiveBytes = 0;
		std::size_t m_DraftExtraBytes = 0;
		std::size_t m_CacheBytes = 0;
	};
	/** Request coalesced worker accounting; figures can be stale while updating. */
	SMemoryUsage MemoryUsage() const;
	void InvalidateMemoryUsage() const { ++m_MemoryGeneration; }

	/** Rendered controls must touch their owner even on frames with no value change. */
	void TouchControl(const void *pOwner)
	{
		if(Owns(pOwner))
			m_ControlRendered = true;
	}
	void FinishFrame();
	/** Losing this UI capture cancels the gesture. Complete before releasing it. */
	void TrackPointer(const void *pOwner, const void *pCapture)
	{
		if(Owns(pOwner))
			m_pCaptureItem = pCapture;
	}

	/**
	 * Mutate live runtime values without capturing a revision. Helpers join the
	 * initiating owner explicitly. Callbacks return void; do not retain mutable
	 * views across restoration, which can replace objects and vector elements.
	 */
	template<typename F>
	bool Update(const void *pOwner, F &&Function)
	{
		if(!Owns(pOwner) || !m_Owner)
			return false;
		InvalidateMemoryUsage();
		return m_pCoordinator->Update(*m_Owner, std::forward<F>(Function));
	}

	/**
	 * Complete one command (Begin/Update/Complete). This is not a nesting helper:
	 * helpers inside an existing edit use Update with that edit's owner instead.
	 * True includes an unchanged result; it does not mean a revision was added.
	 */
	template<typename F>
	bool Edit(const void *pOwner, const char *pLabel, ECategory Category, F &&Function)
	{
		if(Active() && !CompleteActive(ECompletion::TOOL_SWITCH))
			return false;
		if(!Begin(pOwner, pLabel, Category))
			return false;
		if(!Update(pOwner, std::forward<F>(Function)))
			return false;
		return Complete(pOwner, ECompletion::ACCEPT);
	}

	/** Join key repeats until KeyReleased; a zero HeldKey completes immediately. */
	template<typename F>
	bool EditRepeated(const void *pOwner, const char *pLabel, ECategory Category, int HeldKey, F &&Function)
	{
		if(!Begin(pOwner, pLabel, Category))
			return false;
		if(!Update(pOwner, std::forward<F>(Function)))
			return false;
		if(HeldKey == 0)
			return Complete(pOwner, ECompletion::ACCEPT);
		m_HeldKey = HeldKey;
		return true;
	}

private:
	std::unique_ptr<CCoordinator> m_pCoordinator;
	std::optional<CCoordinator::COwner> m_Owner;
	const void *m_pOwnerKey = nullptr;
	int m_HeldKey = 0;
	const void *m_pCaptureItem = nullptr;
	bool m_ControlOwner = false;
	bool m_ControlRendered = false;
	mutable SMemoryUsage m_MemoryUsage;
	class CMemoryJob;
	mutable std::shared_ptr<CMemoryJob> m_pMemoryJob;
	mutable std::uint64_t m_MemoryGeneration = 1;
	mutable std::uint64_t m_FingerprintGeneration = 0;
	mutable bool m_MemoryRequested = false;
	void PruneSession();
};

#endif
