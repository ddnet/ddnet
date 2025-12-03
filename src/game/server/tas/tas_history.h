/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TAS_TAS_HISTORY_H
#define GAME_SERVER_TAS_TAS_HISTORY_H

#include "tas_state.h"

#include <deque>
#include <map>
#include <memory>
#include <vector>

class CGameContext;

class CTasHistory
{
public:
	CTasHistory();
	~CTasHistory();

	// Configuration
	void SetMaxHistoryTicks(int Ticks);    // Default: 90000 (30 min at 50 ticks/sec)
	void SetKeyframeInterval(int Ticks);   // Default: 50 (1 second)
	void SetMaxMemoryBytes(size_t Bytes);  // Memory limit

	int GetMaxHistoryTicks() const { return m_MaxHistoryTicks; }
	int GetKeyframeInterval() const { return m_KeyframeInterval; }
	size_t GetMaxMemoryBytes() const { return m_MaxMemoryBytes; }

	// State management
	void SaveState(CGameContext *pGameServer, int Tick);
	bool LoadState(CGameContext *pGameServer, int Tick);
	bool HasState(int Tick) const;

	// Get snapshot without restoring
	const CTasSnapshot *GetSnapshot(int Tick) const;

	// Navigation
	int GetOldestTick() const;
	int GetNewestTick() const;
	int GetNearestKeyframe(int Tick) const;
	int GetNearestKeyframeBefore(int Tick) const;

	// Memory optimization
	bool IsIdle(const CTasSnapshot *pCurrent, const CTasSnapshot *pPrevious) const;
	void CompressOldStates();
	size_t GetMemoryUsage() const;
	void TrimToMemoryLimit();
	void TrimToTickLimit();

	// Clear all history
	void Clear();

	// Statistics
	int GetStoredStateCount() const;
	int GetKeyframeCount() const;

	// For status display
	struct SHistoryStats
	{
		int m_StoredStates;
		int m_Keyframes;
		int m_OldestTick;
		int m_NewestTick;
		size_t m_MemoryUsage;
		size_t m_MemoryLimit;
	};
	SHistoryStats GetStats() const;

private:
	// Storage: map from tick to snapshot
	std::map<int, std::unique_ptr<CTasSnapshot>> m_mSnapshots;

	// Quick access to keyframe ticks (sorted)
	std::vector<int> m_vKeyframeTicks;

	// Idle tick ranges (stores start tick, references the last non-idle snapshot)
	// When idle, we don't store snapshots, just mark the range
	struct SIdleRange
	{
		int m_StartTick;
		int m_EndTick;
		int m_ReferenceTick; // The tick of the snapshot this idle range references
	};
	std::vector<SIdleRange> m_vIdleRanges;

	// Configuration
	int m_MaxHistoryTicks;
	int m_KeyframeInterval;
	size_t m_MaxMemoryBytes;

	// Current memory usage estimate
	size_t m_CurrentMemoryUsage;

	// Idle detection state
	int m_LastNonIdleTick;
	bool m_CurrentlyIdle;
	int m_IdleStartTick;

	// Internal helpers
	bool ShouldStoreKeyframe(int Tick) const;
	void AddSnapshot(int Tick, std::unique_ptr<CTasSnapshot> pSnapshot);
	void RemoveOldestState();
	void RebuildKeyframeIndex();

	// Find which snapshot to use for a given tick (handles idle ranges)
	int FindEffectiveTick(int Tick) const;
};

#endif // GAME_SERVER_TAS_TAS_HISTORY_H
