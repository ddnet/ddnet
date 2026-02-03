#ifndef ENGINE_SHARED_SNAP_TASKS_H
#define ENGINE_SHARED_SNAP_TASKS_H

#include <cstdint>
#include <memory>
#include <vector>

enum class ESnapTaskType : uint8_t
{
	SNAPSHOT,
	RESET_STORAGE,
};

struct CSnapTask
{
	virtual ~CSnapTask() = default;
	ESnapTaskType m_Type;
};

struct CSnapshotTask : CSnapTask
{
	CSnapshotTask()
	{
		m_Type = ESnapTaskType::SNAPSHOT;
	}

	int m_ClientId{-1};
	int m_Tick{-1};
	bool m_Sixup{false};
	int m_LastAckedTick{-1};
	std::unique_ptr<uint8_t[]> m_Data;
	int m_Size{0};
};

struct CResetStorageTask : CSnapTask
{
	CResetStorageTask()
	{
		m_Type = ESnapTaskType::RESET_STORAGE;
	}

	int m_ClientId{-1};
};

// Result structure
struct SSnapSendResult
{
	int m_ClientId{-1};
	int m_SixUp{false};
	int m_Tick{-1};
	int m_DeltaTick{-1}; // -1 if base wasn't found
	int m_Crc{0};
	bool m_Empty{false}; // true => send NETMSG_SNAPEMPTY (ignore buffer/slices)

	// Single owned buffer with concatenated compressed payload for all packets.
	// Slices point into this buffer.
	std::shared_ptr<std::vector<uint8_t>> m_Buffer;

	struct Slice
	{
		int m_Offset{0}; // offset into m_Buffer
		int m_Size{0}; // bytes in this packet
	};
	std::vector<Slice> m_Slices; // size == number of packets

	// Convenience:
	int NumPackets() const { return m_Empty ? 0 : (int)m_Slices.size(); }
	bool IsValid() const { return m_ClientId >= 0 && m_Tick >= 0; }
};

// Wrapper for result to match CTaskProcessor interface
struct CSnapResult
{
	virtual ~CSnapResult() = default;
	SSnapSendResult m_Result;
};

#endif
