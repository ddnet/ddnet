#include "snap_threaded.h"

#include <engine/server/server.h>
#include <engine/shared/compression.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>

#include <algorithm>

CSnapThreaded::CSnapThreaded() :
	m_TaskProcessor(*this)
{
}

CSnapThreaded::~CSnapThreaded()
{
	Stop();
}

void CSnapThreaded::SetServer(CServer *pServer)
{
	m_pServer = pServer;
}

void CSnapThreaded::Start()
{
	m_DeltaBuf.resize(CSnapshot::MAX_SIZE);
	m_CompBuf.resize(CSnapshot::MAX_SIZE);
	m_TaskProcessor.Start("snap thread");
}

void CSnapThreaded::Stop()
{
	m_TaskProcessor.Stop();
}

void CSnapThreaded::EnqueueRawSnapshot(int ClientId, int Tick, bool Sixup, int LastAckedTick, std::unique_ptr<uint8_t[]> pSnapshotBytes, int SnapshotSize)
{
	auto pTask = std::make_unique<CSnapshotTask>();
	pTask->m_ClientId = ClientId;
	pTask->m_Tick = Tick;
	pTask->m_Sixup = Sixup;
	pTask->m_LastAckedTick = LastAckedTick;
	pTask->m_Data = std::move(pSnapshotBytes);
	pTask->m_Size = SnapshotSize;
	m_TaskProcessor.EnqueueTask(std::move(pTask));
}

void CSnapThreaded::ResetStorage(int ClientId)
{
	auto pTask = std::make_unique<CResetStorageTask>();
	pTask->m_ClientId = ClientId;
	m_TaskProcessor.EnqueueTask(std::move(pTask));
}

bool CSnapThreaded::TryPopResult(SSnapSendResult &Out)
{
	std::unique_ptr<CSnapResult> pResult;
	if(m_TaskProcessor.TryDequeueResult(pResult))
	{
		Out = std::move(pResult->m_Result);
		return true;
	}
	return false;
}

void CSnapThreaded::SetStaticsize(int ItemType, int Size)
{
	m_Delta.SetStaticsize(ItemType, Size);
}

bool CSnapThreaded::TryGetTagTime(int ClientId, int Tick, int64_t &OutTagTime) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	return m_Clients[ClientId].m_TagTimes.TryGet(Tick, OutTagTime);
}

std::unique_ptr<CSnapResult> CSnapThreaded::ProcessTask(CSnapTask &Task)
{
	switch(Task.m_Type)
	{
	case ESnapTaskType::SNAPSHOT:
		return ProcessSnapshotTask(dynamic_cast<CSnapshotTask *>(&Task));
	case ESnapTaskType::RESET_STORAGE:
		ProcessResetStorageTask(dynamic_cast<CResetStorageTask *>(&Task));
		return nullptr;
	default:
		return nullptr;
	}
}

void CSnapThreaded::ProcessResetStorageTask(CResetStorageTask *pTask)
{
	const int ClientId = pTask->m_ClientId;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	SClientState &State = m_Clients[ClientId];
	State.m_Storage.PurgeAll();
}

std::unique_ptr<CSnapResult> CSnapThreaded::EmitEmpty(int ClientId, int Tick, int DeltaTick, bool SixUp)
{
	auto pResult = std::make_unique<CSnapResult>();
	pResult->m_Result.m_ClientId = ClientId;
	pResult->m_Result.m_SixUp = SixUp;
	pResult->m_Result.m_Tick = Tick;
	pResult->m_Result.m_DeltaTick = DeltaTick;
	pResult->m_Result.m_Crc = 0;
	pResult->m_Result.m_Empty = true;
	return pResult;
}

std::unique_ptr<CSnapResult> CSnapThreaded::EmitCompressed(int ClientId, int Tick, int DeltaTick, int Crc, const uint8_t *pComp, int CompSize, bool SixUp)
{
	// Split into packets of size MAX_SNAPSHOT_PACKSIZE.
	const int MaxSize = MAX_SNAPSHOT_PACKSIZE;
	const int NumPackets = (CompSize + MaxSize - 1) / MaxSize;

	auto Buf = std::make_shared<std::vector<uint8_t>>();
	Buf->resize(CompSize);
	mem_copy(Buf->data(), pComp, CompSize);

	auto pResult = std::make_unique<CSnapResult>();
	pResult->m_Result.m_ClientId = ClientId;
	pResult->m_Result.m_SixUp = SixUp;
	pResult->m_Result.m_Tick = Tick;
	pResult->m_Result.m_DeltaTick = DeltaTick;
	pResult->m_Result.m_Crc = Crc;
	pResult->m_Result.m_Empty = false;
	pResult->m_Result.m_Buffer = std::move(Buf);
	pResult->m_Result.m_Slices.reserve(NumPackets);

	int Left = CompSize;
	int Offset = 0;
	while(Left > 0)
	{
		const int Chunk = Left < MaxSize ? Left : MaxSize;
		pResult->m_Result.m_Slices.push_back({Offset, Chunk});
		Offset += Chunk;
		Left -= Chunk;
	}

	return pResult;
}

std::unique_ptr<CSnapResult> CSnapThreaded::ProcessSnapshotTask(CSnapshotTask *pTask)
{
	const int ClientId = pTask->m_ClientId;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || pTask->m_Size <= 0 || pTask->m_Data == nullptr)
		return nullptr;

	// Prepare delta tables per 0.7/0.6 flavor.
	m_Delta.SetStaticsize(protocol7::NETEVENTTYPE_SOUNDWORLD, pTask->m_Sixup);
	m_Delta.SetStaticsize(protocol7::NETEVENTTYPE_DAMAGE, pTask->m_Sixup);

	// Interpret bytes as CSnapshot
	CSnapshot *pSnap = reinterpret_cast<CSnapshot *>(pTask->m_Data.get());

	if(pTask->m_Size > CSnapshot::MAX_SIZE || !pSnap->IsValid(pTask->m_Size))
	{
		return EmitEmpty(ClientId, pTask->m_Tick, -1, pTask->m_Sixup);
	}

	SClientState &State = m_Clients[ClientId];

	// Add to storage, prune old (keep ~3 seconds), resolve base
	{
		const int64_t Now = time_get();
		const int KeepTicks = (m_pServer ? m_pServer->TickSpeed() : 50) * 3;
		State.m_Storage.PurgeUntil(pTask->m_Tick - KeepTicks);
		State.m_Storage.Add(pTask->m_Tick, Now, pTask->m_Size, pSnap, 0, nullptr);
		State.m_TagTimes.Push(pTask->m_Tick, Now);
	}

	int DeltaTick = -1;
	const CSnapshot *pBase = CSnapshot::EmptySnapshot();
	{
		int64_t DummyTagTime = 0;
		const CSnapshot *pFound = nullptr;
		const int BaseSize = State.m_Storage.Get(pTask->m_LastAckedTick, &DummyTagTime, &pFound, nullptr);
		if(BaseSize >= 0 && pFound && pFound->IsValid(BaseSize))
		{
			pBase = pFound;
			DeltaTick = pTask->m_LastAckedTick;
		}
	}

	// Compute CRC of full snapshot (for headers).
	const int Crc = pSnap->Crc();

	// Create delta
	const int DeltaSize = m_Delta.CreateDelta(pBase, pSnap, (char *)m_DeltaBuf.data());

	if(DeltaSize == 0)
	{
		// Nothing to send this tick to that client -> NETMSG_SNAPEMPTY
		return EmitEmpty(ClientId, pTask->m_Tick, DeltaTick, pTask->m_Sixup);
	}

	// Compress
	const int CompSize = CVariableInt::Compress((char *)m_DeltaBuf.data(), DeltaSize, (char *)m_CompBuf.data(), (int)m_CompBuf.size());

	return EmitCompressed(ClientId, pTask->m_Tick, DeltaTick, Crc, m_CompBuf.data(), CompSize, pTask->m_Sixup);
}

void CSnapThreaded::STagRing::Push(int Tick, int64_t TagTime)
{
	uint32_t WriteIdx = m_WriteIdx.fetch_add(1, std::memory_order_relaxed);
	SSlot &Slot = m_Slots[WriteIdx % K_CAP];
	uint32_t Seq0 = Slot.m_Seq.load(std::memory_order_relaxed);
	Slot.m_Seq.store(Seq0 + 1, std::memory_order_release); // odd => write begin
	Slot.m_Tick = Tick;
	Slot.m_TagTime = TagTime;
	Slot.m_Seq.store(Seq0 + 2, std::memory_order_release); // even => write end
}

bool CSnapThreaded::STagRing::TryGet(int Tick, int64_t &OutTagTime) const
{
	for(const auto &Slot : m_Slots)
	{
		uint32_t Seq1 = Slot.m_Seq.load(std::memory_order_acquire);
		if((Seq1 & 1u) != 0) // in write
			continue;
		int t = Slot.m_Tick;
		int64_t TagTime = Slot.m_TagTime;
		uint32_t Seq2 = Slot.m_Seq.load(std::memory_order_acquire);
		if(Seq1 == Seq2 && (Seq2 & 1u) == 0)
		{
			if(t == Tick)
			{
				OutTagTime = TagTime;
				return true;
			}
		}
	}
	return false;
}
