#ifndef ENGINE_SHARED_SNAP_THREADED_H
#define ENGINE_SHARED_SNAP_THREADED_H

#include <engine/shared/protocol.h>
#include <engine/shared/snap_tasks.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/task_processor.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class CServer;
class CSnapshot;

// NOLINTBEGIN(portability-template-virtual-member-function)

// Threaded snapshot delta/compress worker.
class CSnapThreaded final : public ITaskProcessor<CSnapTask, CSnapResult>
{
public:
	CSnapThreaded();
	~CSnapThreaded() override;

	// Called once early (e.g. in CServer ctor) so we can query TickSpeed(), logging, etc.
	void SetServer(CServer *pServer);

	// Optional: start/stop worker thread (automatically stops in dtor).
	void Start();
	void Stop();

	// --- Task API (enqueued onto a single queue to preserve order) ---

	// Enqueue a raw snapshot for a specific client.
	// Ownership of pSnapshotBytes is transferred.
	// - ClientId: 0..MaxClients-1
	// - Tick: current game tick
	// - Sixup: whether 0.7 packing rules apply for that client
	// - LastAckedTick: last acked snapshot tick reported by that client (or -1)
	// - pSnapshotBytes + SnapshotSize: bytes returned by CSnapshotBuilder::Finish(...)
	void EnqueueRawSnapshot(int ClientId, int Tick, bool Sixup, int LastAckedTick, std::unique_ptr<uint8_t[]> pSnapshotBytes, int SnapshotSize);

	// Request to purge in-memory snapshot storage for a client (queued; order-safe).
	void ResetStorage(int ClientId);

	// The network thread (single consumer) should call this frequently and send the packets.
	// Returns true if a result was popped and filled into out.
	bool TryPopResult(SSnapSendResult &Out);

	// Optional tweak: static sizes for forced statics in delta
	void SetStaticsize(int ItemType, int Size);

	// For latency calculation on the main thread when both net and snap are threaded.
	bool TryGetTagTime(int ClientId, int Tick, int64_t &OutTagTime) const;

	// ITaskProcessor implementation
	std::unique_ptr<CSnapResult> ProcessTask(CSnapTask &Task) override;

private:
	// Tiny lock-free ring with seqlock slots to expose (tick -> tag time) to readers.
	struct STagRing
	{
		static constexpr int K_CAP = 64;
		struct SSlot
		{
			std::atomic<uint32_t> m_Seq{0}; // seqlock
			int m_Tick{-1};
			int64_t m_TagTime{0};
		};
		SSlot m_Slots[K_CAP];
		std::atomic<uint32_t> m_WriteIdx{0};

		void Push(int Tick, int64_t TagTime);

		// lock-free consistent read; returns true if Tick is found
		bool TryGet(int Tick, int64_t &OutTagTime) const;
	};

	struct SClientState
	{
		CSnapshotStorage m_Storage;
		STagRing m_TagTimes;
	};

	// We keep a single delta helper in the worker thread (not shared with main).
	CSnapshotDelta m_Delta;

	std::vector<uint8_t> m_DeltaBuf;
	std::vector<uint8_t> m_CompBuf;

	// Task processors
	std::unique_ptr<CSnapResult> ProcessSnapshotTask(CSnapshotTask *pTask);
	void ProcessResetStorageTask(CResetStorageTask *pTask);

	// Emitters into result queue
	std::unique_ptr<CSnapResult> EmitEmpty(int ClientId, int Tick, int DeltaTick, bool SixUp);
	std::unique_ptr<CSnapResult> EmitCompressed(int ClientId, int Tick, int DeltaTick, int Crc, const uint8_t *pComp, int CompSize, bool SixUp);

	// Environment
	CServer *m_pServer{nullptr};

	// Task processor
	CTaskProcessor<CSnapTask, CSnapResult> m_TaskProcessor;

	// Per-client state.
	std::array<SClientState, MAX_CLIENTS> m_Clients{};
};

// NOLINTEND(portability-template-virtual-member-function)

#endif
