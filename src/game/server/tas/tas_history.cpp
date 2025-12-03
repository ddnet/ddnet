/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#include "tas_history.h"

#include <game/server/gamecontext.h>

#include <algorithm>

CTasHistory::CTasHistory() :
	m_MaxHistoryTicks(90000), // 30 minutes at 50 ticks/sec
	m_KeyframeInterval(50),   // 1 second
	m_MaxMemoryBytes(2ULL * 1024 * 1024 * 1024), // 2 GB default
	m_CurrentMemoryUsage(0),
	m_LastNonIdleTick(-1),
	m_CurrentlyIdle(false),
	m_IdleStartTick(-1)
{
}

CTasHistory::~CTasHistory()
{
	Clear();
}

void CTasHistory::SetMaxHistoryTicks(int Ticks)
{
	m_MaxHistoryTicks = Ticks;
	TrimToTickLimit();
}

void CTasHistory::SetKeyframeInterval(int Ticks)
{
	m_KeyframeInterval = maximum(1, Ticks);
}

void CTasHistory::SetMaxMemoryBytes(size_t Bytes)
{
	m_MaxMemoryBytes = Bytes;
	TrimToMemoryLimit();
}

void CTasHistory::SaveState(CGameContext *pGameServer, int Tick)
{
	auto pSnapshot = std::make_unique<CTasSnapshot>();
	pSnapshot->Capture(pGameServer);

	// Check if this should be a keyframe
	pSnapshot->m_IsKeyframe = ShouldStoreKeyframe(Tick);

	// Check for idle state (compare with previous snapshot)
	const CTasSnapshot *pPrevSnapshot = nullptr;
	if(!m_mSnapshots.empty())
	{
		auto It = m_mSnapshots.rbegin();
		pPrevSnapshot = It->second.get();
	}

	bool bIsIdle = pPrevSnapshot && this->IsIdle(pSnapshot.get(), pPrevSnapshot);

	if(bIsIdle)
	{
		// If already idle, extend the idle range
		if(m_CurrentlyIdle)
		{
			// Update the last idle range's end tick
			if(!m_vIdleRanges.empty())
			{
				m_vIdleRanges.back().m_EndTick = Tick;
			}
			// Don't store the snapshot for idle ticks (memory optimization)
			return;
		}
		else
		{
			// Start a new idle range
			m_CurrentlyIdle = true;
			m_IdleStartTick = Tick;

			SIdleRange Range;
			Range.m_StartTick = Tick;
			Range.m_EndTick = Tick;
			Range.m_ReferenceTick = m_LastNonIdleTick;
			m_vIdleRanges.push_back(Range);

			// Don't store the snapshot for idle ticks
			return;
		}
	}
	else
	{
		// Not idle - end any current idle range
		m_CurrentlyIdle = false;
		m_LastNonIdleTick = Tick;
	}

	// Store the snapshot
	AddSnapshot(Tick, std::move(pSnapshot));

	// Trim if needed
	TrimToTickLimit();
	TrimToMemoryLimit();
}

bool CTasHistory::LoadState(CGameContext *pGameServer, int Tick)
{
	const CTasSnapshot *pSnapshot = GetSnapshot(Tick);
	if(!pSnapshot)
		return false;

	// Create a mutable copy to restore
	CTasSnapshot Snapshot = *pSnapshot;
	return Snapshot.Restore(pGameServer);
}

bool CTasHistory::HasState(int Tick) const
{
	// Check direct storage
	if(m_mSnapshots.find(Tick) != m_mSnapshots.end())
		return true;

	// Check idle ranges
	for(const auto &Range : m_vIdleRanges)
	{
		if(Tick >= Range.m_StartTick && Tick <= Range.m_EndTick)
			return HasState(Range.m_ReferenceTick);
	}

	return false;
}

const CTasSnapshot *CTasHistory::GetSnapshot(int Tick) const
{
	// Check direct storage first
	auto It = m_mSnapshots.find(Tick);
	if(It != m_mSnapshots.end())
		return It->second.get();

	// Check idle ranges
	int EffectiveTick = FindEffectiveTick(Tick);
	if(EffectiveTick != Tick)
	{
		It = m_mSnapshots.find(EffectiveTick);
		if(It != m_mSnapshots.end())
			return It->second.get();
	}

	// If not found, try to find nearest keyframe and extrapolate
	// (for ticks in between keyframes that weren't stored)
	return nullptr;
}

int CTasHistory::GetOldestTick() const
{
	if(m_mSnapshots.empty())
		return -1;
	return m_mSnapshots.begin()->first;
}

int CTasHistory::GetNewestTick() const
{
	if(m_mSnapshots.empty())
		return -1;

	int NewestStored = m_mSnapshots.rbegin()->first;

	// Check idle ranges for later ticks
	for(const auto &Range : m_vIdleRanges)
	{
		if(Range.m_EndTick > NewestStored)
			NewestStored = Range.m_EndTick;
	}

	return NewestStored;
}

int CTasHistory::GetNearestKeyframe(int Tick) const
{
	if(m_vKeyframeTicks.empty())
		return -1;

	// Binary search for nearest keyframe <= Tick
	auto It = std::upper_bound(m_vKeyframeTicks.begin(), m_vKeyframeTicks.end(), Tick);
	if(It == m_vKeyframeTicks.begin())
		return m_vKeyframeTicks.front();

	--It;
	return *It;
}

int CTasHistory::GetNearestKeyframeBefore(int Tick) const
{
	if(m_vKeyframeTicks.empty())
		return -1;

	// Binary search for nearest keyframe < Tick
	auto It = std::lower_bound(m_vKeyframeTicks.begin(), m_vKeyframeTicks.end(), Tick);
	if(It == m_vKeyframeTicks.begin())
		return -1;

	--It;
	return *It;
}

bool CTasHistory::IsIdle(const CTasSnapshot *pCurrent, const CTasSnapshot *pPrevious) const
{
	if(!pCurrent || !pPrevious)
		return false;

	return pCurrent->IsEquivalent(*pPrevious);
}

void CTasHistory::CompressOldStates()
{
	// Future: implement compression using zstd
	// For now, just trim old states
}

size_t CTasHistory::GetMemoryUsage() const
{
	return m_CurrentMemoryUsage;
}

void CTasHistory::TrimToMemoryLimit()
{
	// Keep removing oldest non-keyframe states until under memory limit
	// Always keep at least the 5 most recent seconds (250 ticks)
	int MinimumTicks = 250;
	int NewestTick = GetNewestTick();

	while(m_CurrentMemoryUsage > m_MaxMemoryBytes && m_mSnapshots.size() > 1)
	{
		auto It = m_mSnapshots.begin();

		// Don't remove if it's within the minimum recent window
		if(NewestTick - It->first < MinimumTicks)
			break;

		// Prefer removing non-keyframes first
		bool RemovedNonKeyframe = false;
		for(auto CheckIt = m_mSnapshots.begin(); CheckIt != m_mSnapshots.end(); ++CheckIt)
		{
			if(NewestTick - CheckIt->first < MinimumTicks)
				break;

			if(!CheckIt->second->m_IsKeyframe)
			{
				m_CurrentMemoryUsage -= CheckIt->second->GetApproximateSize();
				m_mSnapshots.erase(CheckIt);
				RemovedNonKeyframe = true;
				break;
			}
		}

		// If no non-keyframe found, remove oldest keyframe
		if(!RemovedNonKeyframe)
		{
			m_CurrentMemoryUsage -= It->second->GetApproximateSize();

			// Remove from keyframe index
			auto KeyframeIt = std::find(m_vKeyframeTicks.begin(), m_vKeyframeTicks.end(), It->first);
			if(KeyframeIt != m_vKeyframeTicks.end())
				m_vKeyframeTicks.erase(KeyframeIt);

			m_mSnapshots.erase(It);
		}
	}

	// Also trim idle ranges
	int OldestTick = GetOldestTick();
	m_vIdleRanges.erase(
		std::remove_if(m_vIdleRanges.begin(), m_vIdleRanges.end(),
			[OldestTick](const SIdleRange &Range) {
				return Range.m_EndTick < OldestTick;
			}),
		m_vIdleRanges.end());
}

void CTasHistory::TrimToTickLimit()
{
	int NewestTick = GetNewestTick();
	if(NewestTick < 0)
		return;

	int OldestAllowedTick = NewestTick - m_MaxHistoryTicks;

	// Remove snapshots older than the limit
	while(!m_mSnapshots.empty())
	{
		auto It = m_mSnapshots.begin();
		if(It->first >= OldestAllowedTick)
			break;

		m_CurrentMemoryUsage -= It->second->GetApproximateSize();

		// Remove from keyframe index
		if(It->second->m_IsKeyframe)
		{
			auto KeyframeIt = std::find(m_vKeyframeTicks.begin(), m_vKeyframeTicks.end(), It->first);
			if(KeyframeIt != m_vKeyframeTicks.end())
				m_vKeyframeTicks.erase(KeyframeIt);
		}

		m_mSnapshots.erase(It);
	}

	// Trim idle ranges
	m_vIdleRanges.erase(
		std::remove_if(m_vIdleRanges.begin(), m_vIdleRanges.end(),
			[OldestAllowedTick](const SIdleRange &Range) {
				return Range.m_EndTick < OldestAllowedTick;
			}),
		m_vIdleRanges.end());
}

void CTasHistory::Clear()
{
	m_mSnapshots.clear();
	m_vKeyframeTicks.clear();
	m_vIdleRanges.clear();
	m_CurrentMemoryUsage = 0;
	m_LastNonIdleTick = -1;
	m_CurrentlyIdle = false;
	m_IdleStartTick = -1;
}

int CTasHistory::GetStoredStateCount() const
{
	return static_cast<int>(m_mSnapshots.size());
}

int CTasHistory::GetKeyframeCount() const
{
	return static_cast<int>(m_vKeyframeTicks.size());
}

CTasHistory::SHistoryStats CTasHistory::GetStats() const
{
	SHistoryStats Stats;
	Stats.m_StoredStates = GetStoredStateCount();
	Stats.m_Keyframes = GetKeyframeCount();
	Stats.m_OldestTick = GetOldestTick();
	Stats.m_NewestTick = GetNewestTick();
	Stats.m_MemoryUsage = m_CurrentMemoryUsage;
	Stats.m_MemoryLimit = m_MaxMemoryBytes;
	return Stats;
}

bool CTasHistory::ShouldStoreKeyframe(int Tick) const
{
	if(m_vKeyframeTicks.empty())
		return true;

	// Store keyframe if enough ticks have passed since the last one
	int LastKeyframe = m_vKeyframeTicks.back();
	return (Tick - LastKeyframe) >= m_KeyframeInterval;
}

void CTasHistory::AddSnapshot(int Tick, std::unique_ptr<CTasSnapshot> pSnapshot)
{
	// Update memory usage
	size_t SnapshotSize = pSnapshot->GetApproximateSize();

	// Add to keyframe index if it's a keyframe
	if(pSnapshot->m_IsKeyframe)
	{
		m_vKeyframeTicks.push_back(Tick);
	}

	// Insert into map
	m_mSnapshots[Tick] = std::move(pSnapshot);
	m_CurrentMemoryUsage += SnapshotSize;
}

void CTasHistory::RemoveOldestState()
{
	if(m_mSnapshots.empty())
		return;

	auto It = m_mSnapshots.begin();
	m_CurrentMemoryUsage -= It->second->GetApproximateSize();

	// Remove from keyframe index if needed
	if(It->second->m_IsKeyframe)
	{
		auto KeyframeIt = std::find(m_vKeyframeTicks.begin(), m_vKeyframeTicks.end(), It->first);
		if(KeyframeIt != m_vKeyframeTicks.end())
			m_vKeyframeTicks.erase(KeyframeIt);
	}

	m_mSnapshots.erase(It);
}

void CTasHistory::RebuildKeyframeIndex()
{
	m_vKeyframeTicks.clear();
	for(const auto &[Tick, pSnapshot] : m_mSnapshots)
	{
		if(pSnapshot->m_IsKeyframe)
			m_vKeyframeTicks.push_back(Tick);
	}
}

int CTasHistory::FindEffectiveTick(int Tick) const
{
	// Check if this tick falls within an idle range
	for(const auto &Range : m_vIdleRanges)
	{
		if(Tick >= Range.m_StartTick && Tick <= Range.m_EndTick)
		{
			return Range.m_ReferenceTick;
		}
	}

	return Tick;
}
