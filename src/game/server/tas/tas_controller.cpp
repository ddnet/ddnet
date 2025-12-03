/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#include "tas_controller.h"

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

CTasController::CTasController(CGameContext *pGameServer) :
	m_pGameServer(pGameServer),
	m_Mode(ETasMode::DISABLED),
	m_PlaybackState(ETasPlaybackState::STOPPED),
	m_Paused(false),
	m_Speed(1.0f),
	m_TargetTick(-1),
	m_StepRemaining(0),
	m_Rewinding(false),
	m_FastForwarding(false),
	m_RecordingStartTick(-1),
	m_ControlClientId(-1),
	m_SpeedAccumulator(0.0f)
{
}

CTasController::~CTasController() = default;

void CTasController::SetMode(ETasMode Mode)
{
	m_Mode = Mode;
	if(Mode == ETasMode::DISABLED)
	{
		m_Paused = false;
		m_PlaybackState = ETasPlaybackState::STOPPED;
		m_History.Clear();
		m_vInjectedInputs.clear();
		m_vRecordedInputs.clear();
	}
}

bool CTasController::ShouldAdvanceTick() const
{
	if(!IsEnabled())
		return true;

	// If paused and no steps remaining, don't advance
	if(m_Paused && m_StepRemaining <= 0)
		return false;

	return true;
}

void CTasController::OnPreTick()
{
	if(!IsEnabled())
		return;

	// Handle speed control
	if(m_Speed != 1.0f && !m_Paused)
	{
		m_SpeedAccumulator += m_Speed;
		if(m_SpeedAccumulator < 1.0f)
		{
			// Skip this tick (slow motion)
			return;
		}
		m_SpeedAccumulator -= 1.0f;
	}
}

void CTasController::OnPostTick()
{
	if(!IsEnabled())
		return;

	// Decrement step counter if stepping
	if(m_StepRemaining > 0)
	{
		m_StepRemaining--;
		if(m_StepRemaining == 0 && m_Paused)
		{
			// Stay paused after stepping
		}
	}

	// Save state for history
	SaveCurrentState();

	// Record inputs if recording
	if(m_PlaybackState == ETasPlaybackState::RECORDING)
	{
		RecordCurrentInputs();
	}

	// Check if we've reached target tick (for fast forward/rewind)
	if(m_TargetTick >= 0)
	{
		int CurrentTick = GetCurrentTick();
		if(CurrentTick >= m_TargetTick)
		{
			m_TargetTick = -1;
			m_FastForwarding = false;
			// Optionally pause after reaching target
		}
	}
}

void CTasController::Pause()
{
	m_Paused = true;
	m_pGameServer->SendBroadcast("TAS: Paused", -1, false);
}

void CTasController::Resume()
{
	m_Paused = false;
	m_StepRemaining = 0;
	m_pGameServer->SendBroadcast("TAS: Resumed", -1, false);
}

void CTasController::TogglePause()
{
	if(m_Paused)
		Resume();
	else
		Pause();
}

void CTasController::StepForward(int NumTicks)
{
	if(NumTicks <= 0)
		NumTicks = 1;

	m_Paused = true;
	m_StepRemaining = NumTicks;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "TAS: Stepping %d tick%s", NumTicks, NumTicks > 1 ? "s" : "");
	m_pGameServer->SendBroadcast(aBuf, -1, false);
}

void CTasController::FastForward(int TargetTick)
{
	int CurrentTick = GetCurrentTick();
	if(TargetTick <= CurrentTick)
		return;

	m_TargetTick = TargetTick;
	m_FastForwarding = true;
	m_Paused = false;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "TAS: Fast forwarding to tick %d", TargetTick);
	m_pGameServer->SendBroadcast(aBuf, -1, false);
}

void CTasController::Rewind(int TargetTick)
{
	if(!m_History.HasState(TargetTick))
	{
		// Find nearest available tick
		int NearestKeyframe = m_History.GetNearestKeyframeBefore(TargetTick);
		if(NearestKeyframe < 0)
		{
			m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
				"Cannot rewind: no state available for that tick");
			return;
		}
		TargetTick = NearestKeyframe;
	}

	m_Rewinding = true;

	if(RestoreState(TargetTick))
	{
		// Clear any injected inputs after the target tick
		ClearInjectedInputsAfterTick(TargetTick);

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "TAS: Rewound to tick %d", TargetTick);
		m_pGameServer->SendBroadcast(aBuf, -1, false);

		// Pause after rewind
		m_Paused = true;
	}
	else
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Failed to restore state");
	}

	m_Rewinding = false;
}

void CTasController::GotoTick(int TargetTick)
{
	int CurrentTick = GetCurrentTick();

	if(TargetTick < CurrentTick)
	{
		Rewind(TargetTick);
	}
	else if(TargetTick > CurrentTick)
	{
		FastForward(TargetTick);
	}
}

void CTasController::SetSpeed(float Speed)
{
	m_Speed = clamp(Speed, 0.1f, 10.0f);

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "TAS: Speed set to %.1fx", m_Speed);
	m_pGameServer->SendBroadcast(aBuf, -1, false);
}

int CTasController::GetCurrentTick() const
{
	return m_pGameServer->Server()->Tick();
}

void CTasController::InjectInput(const STasInput &Input)
{
	// Insert in sorted order
	auto It = std::lower_bound(m_vInjectedInputs.begin(), m_vInjectedInputs.end(), Input);
	m_vInjectedInputs.insert(It, Input);
}

void CTasController::QueueInputSequence(const std::vector<STasInput> &vInputs)
{
	for(const auto &Input : vInputs)
	{
		InjectInput(Input);
	}
}

bool CTasController::HasInjectedInput(int ClientId, int Tick) const
{
	return GetInjectedInput(ClientId, Tick) != nullptr;
}

const STasInput *CTasController::GetInjectedInput(int ClientId, int Tick) const
{
	// Binary search for the input
	STasInput SearchKey;
	SearchKey.m_Tick = Tick;
	SearchKey.m_ClientId = ClientId;

	auto It = std::lower_bound(m_vInjectedInputs.begin(), m_vInjectedInputs.end(), SearchKey);
	if(It != m_vInjectedInputs.end() && It->m_Tick == Tick && It->m_ClientId == ClientId)
	{
		return &(*It);
	}

	return nullptr;
}

void CTasController::ClearInjectedInputs()
{
	m_vInjectedInputs.clear();
}

void CTasController::ClearInjectedInputsAfterTick(int Tick)
{
	m_vInjectedInputs.erase(
		std::remove_if(m_vInjectedInputs.begin(), m_vInjectedInputs.end(),
			[Tick](const STasInput &Input) {
				return Input.m_Tick > Tick;
			}),
		m_vInjectedInputs.end());
}

void CTasController::StartRecording()
{
	m_PlaybackState = ETasPlaybackState::RECORDING;
	m_RecordingStartTick = GetCurrentTick();
	m_vRecordedInputs.clear();
	m_pGameServer->SendBroadcast("TAS: Recording started", -1, false);
}

void CTasController::StopRecording()
{
	if(m_PlaybackState == ETasPlaybackState::RECORDING)
	{
		m_PlaybackState = ETasPlaybackState::STOPPED;
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "TAS: Recording stopped (%d inputs recorded)",
			(int)m_vRecordedInputs.size());
		m_pGameServer->SendBroadcast(aBuf, -1, false);
	}
}

void CTasController::StartPlayback()
{
	if(m_vInjectedInputs.empty())
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"No inputs to play back");
		return;
	}

	m_PlaybackState = ETasPlaybackState::PLAYING;
	m_Paused = false;
	m_pGameServer->SendBroadcast("TAS: Playback started", -1, false);
}

void CTasController::StopPlayback()
{
	if(m_PlaybackState == ETasPlaybackState::PLAYING)
	{
		m_PlaybackState = ETasPlaybackState::STOPPED;
		m_pGameServer->SendBroadcast("TAS: Playback stopped", -1, false);
	}
	else if(m_PlaybackState == ETasPlaybackState::RECORDING)
	{
		StopRecording();
	}
}

void CTasController::SetControlClient(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		m_ControlClientId = -1;
		return;
	}

	m_ControlClientId = ClientId;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "TAS: Control given to client %d", ClientId);
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void CTasController::AddCollaborator(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
	{
		m_CollaboratorClientIds.insert(ClientId);
	}
}

void CTasController::RemoveCollaborator(int ClientId)
{
	m_CollaboratorClientIds.erase(ClientId);
}

bool CTasController::IsCollaborator(int ClientId) const
{
	return m_CollaboratorClientIds.find(ClientId) != m_CollaboratorClientIds.end();
}

bool CTasController::CanControl(int ClientId) const
{
	if(m_Mode == ETasMode::DISABLED)
		return false;

	if(m_Mode == ETasMode::COLLABORATIVE)
		return IsCollaborator(ClientId) || ClientId == m_ControlClientId;

	return ClientId == m_ControlClientId;
}

bool CTasController::SaveToFile(const char *pFilename)
{
	// Construct full path
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "tas/%s.tas", pFilename);

	// Create directory if needed
	m_pGameServer->Storage()->CreateFolder("tas", IStorage::TYPE_SAVE);

	// Open file for writing
	IOHANDLE File = m_pGameServer->Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Failed to open file for writing");
		return false;
	}

	// Write header
	std::vector<uint8_t> vBuffer;
	WriteHeader(vBuffer);

	// Write inputs
	WriteInputs(vBuffer);

	// Write to file
	io_write(File, vBuffer.data(), vBuffer.size());
	io_close(File);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "TAS: Saved to %s (%d inputs)", aPath, (int)m_vInjectedInputs.size());
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);

	return true;
}

bool CTasController::LoadFromFile(const char *pFilename)
{
	// Construct full path
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "tas/%s.tas", pFilename);

	// Open file for reading
	IOHANDLE File = m_pGameServer->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Failed to open file for reading");
		return false;
	}

	// Read entire file
	long FileSize = io_length(File);
	if(FileSize <= 0)
	{
		io_close(File);
		return false;
	}

	std::vector<uint8_t> vBuffer(FileSize);
	io_read(File, vBuffer.data(), FileSize);
	io_close(File);

	// Parse header and inputs
	size_t Offset = 0;
	if(!ReadHeader(vBuffer.data(), vBuffer.size(), Offset))
		return false;

	if(!ReadInputs(vBuffer.data(), vBuffer.size(), Offset))
		return false;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "TAS: Loaded from %s (%d inputs)", aPath, (int)m_vInjectedInputs.size());
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);

	return true;
}

CTasController::STasStatus CTasController::GetStatus() const
{
	STasStatus Status;
	Status.m_Mode = m_Mode;
	Status.m_PlaybackState = m_PlaybackState;
	Status.m_Paused = m_Paused;
	Status.m_Speed = m_Speed;
	Status.m_CurrentTick = GetCurrentTick();
	Status.m_TargetTick = m_TargetTick;
	Status.m_ControlClientId = m_ControlClientId;
	Status.m_CollaboratorCount = static_cast<int>(m_CollaboratorClientIds.size());
	Status.m_InjectedInputCount = static_cast<int>(m_vInjectedInputs.size());
	Status.m_RecordedInputCount = static_cast<int>(m_vRecordedInputs.size());
	Status.m_HistoryStats = m_History.GetStats();
	return Status;
}

void CTasController::FormatStatus(char *pBuffer, int BufferSize) const
{
	STasStatus Status = GetStatus();

	const char *pModeStr = "Disabled";
	if(Status.m_Mode == ETasMode::SINGLE_CONTROL)
		pModeStr = "Single Control";
	else if(Status.m_Mode == ETasMode::COLLABORATIVE)
		pModeStr = "Collaborative";

	const char *pStateStr = "Stopped";
	if(Status.m_PlaybackState == ETasPlaybackState::PLAYING)
		pStateStr = "Playing";
	else if(Status.m_PlaybackState == ETasPlaybackState::RECORDING)
		pStateStr = "Recording";

	str_format(pBuffer, BufferSize,
		"TAS Status:\n"
		"  Mode: %s\n"
		"  State: %s\n"
		"  Paused: %s\n"
		"  Speed: %.1fx\n"
		"  Current Tick: %d\n"
		"  History: %d states, %d keyframes\n"
		"  Memory: %.1f MB / %.1f MB\n"
		"  Injected Inputs: %d\n"
		"  Control Client: %d",
		pModeStr,
		pStateStr,
		Status.m_Paused ? "Yes" : "No",
		Status.m_Speed,
		Status.m_CurrentTick,
		Status.m_HistoryStats.m_StoredStates,
		Status.m_HistoryStats.m_Keyframes,
		Status.m_HistoryStats.m_MemoryUsage / (1024.0 * 1024.0),
		Status.m_HistoryStats.m_MemoryLimit / (1024.0 * 1024.0),
		Status.m_InjectedInputCount,
		Status.m_ControlClientId);
}

void CTasController::SaveCurrentState()
{
	m_History.SaveState(m_pGameServer, GetCurrentTick());
}

bool CTasController::RestoreState(int Tick)
{
	if(!m_History.LoadState(m_pGameServer, Tick))
		return false;

	// Set the server tick to the restored tick
	m_pGameServer->Server()->SetTick(Tick);

	// Force full snapshots to all clients for synchronization
	ForceFullSnapshots();

	return true;
}

void CTasController::RecordCurrentInputs()
{
	int CurrentTick = GetCurrentTick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = m_pGameServer->m_apPlayers[i];
		if(!pPlayer)
			continue;

		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr)
			continue;

		// Get current input
		CNetObj_PlayerInput Input = m_pGameServer->GetLastPlayerInput(i);

		STasInput TasInput;
		TasInput.m_Tick = CurrentTick;
		TasInput.m_ClientId = i;
		TasInput.m_Direction = Input.m_Direction;
		TasInput.m_TargetX = Input.m_TargetX;
		TasInput.m_TargetY = Input.m_TargetY;
		TasInput.m_Jump = Input.m_Jump;
		TasInput.m_Fire = Input.m_Fire;
		TasInput.m_Hook = Input.m_Hook;
		TasInput.m_Weapon = Input.m_WantedWeapon - 1;

		m_vRecordedInputs.push_back(TasInput);
	}
}

void CTasController::DestroyAllDynamicEntities()
{
	// Destroy projectiles
	CEntity *pEnt = m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE);
	while(pEnt)
	{
		CEntity *pNext = pEnt->TypeNext();
		pEnt->Destroy();
		pEnt = pNext;
	}

	// Destroy lasers
	pEnt = m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_LASER);
	while(pEnt)
	{
		CEntity *pNext = pEnt->TypeNext();
		pEnt->Destroy();
		pEnt = pNext;
	}

	m_pGameServer->m_World.RemoveEntities();
}

void CTasController::ForceFullSnapshots()
{
	m_pGameServer->Server()->ForceFullSnapshots();
}

void CTasController::WriteHeader(std::vector<uint8_t> &vBuffer) const
{
	// Magic number "DDTAS"
	vBuffer.push_back('D');
	vBuffer.push_back('D');
	vBuffer.push_back('T');
	vBuffer.push_back('A');
	vBuffer.push_back('S');

	// Version (uint16)
	vBuffer.push_back(1);
	vBuffer.push_back(0);

	// Map name (64 bytes, padded with zeros)
	const char *pMapName = m_pGameServer->Server()->GetMapName();
	for(int i = 0; i < 64; i++)
	{
		if(pMapName && i < (int)str_length(pMapName))
			vBuffer.push_back(pMapName[i]);
		else
			vBuffer.push_back(0);
	}

	// Number of inputs (uint32)
	uint32_t NumInputs = static_cast<uint32_t>(m_vInjectedInputs.size());
	vBuffer.push_back((NumInputs >> 0) & 0xFF);
	vBuffer.push_back((NumInputs >> 8) & 0xFF);
	vBuffer.push_back((NumInputs >> 16) & 0xFF);
	vBuffer.push_back((NumInputs >> 24) & 0xFF);
}

bool CTasController::ReadHeader(const uint8_t *pData, size_t Size, size_t &Offset)
{
	if(Size < 5 + 2 + 64 + 4)
		return false;

	// Check magic
	if(pData[0] != 'D' || pData[1] != 'D' || pData[2] != 'T' ||
		pData[3] != 'A' || pData[4] != 'S')
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Invalid TAS file format");
		return false;
	}

	Offset = 5;

	// Version
	uint16_t Version = pData[Offset] | (pData[Offset + 1] << 8);
	Offset += 2;

	if(Version != 1)
	{
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Unsupported TAS file version");
		return false;
	}

	// Map name (skip for now)
	Offset += 64;

	// Number of inputs (for validation later)
	Offset += 4;

	return true;
}

void CTasController::WriteInputs(std::vector<uint8_t> &vBuffer) const
{
	for(const auto &Input : m_vInjectedInputs)
	{
		// Each input: tick(4) + client(1) + dir(1) + targetX(4) + targetY(4) + jump(1) + fire(1) + hook(1) + weapon(1)
		// Total: 18 bytes per input

		auto WriteInt32 = [&vBuffer](int Value) {
			vBuffer.push_back((Value >> 0) & 0xFF);
			vBuffer.push_back((Value >> 8) & 0xFF);
			vBuffer.push_back((Value >> 16) & 0xFF);
			vBuffer.push_back((Value >> 24) & 0xFF);
		};

		WriteInt32(Input.m_Tick);
		vBuffer.push_back(static_cast<uint8_t>(Input.m_ClientId));
		vBuffer.push_back(static_cast<uint8_t>(Input.m_Direction + 128)); // Offset to make unsigned
		WriteInt32(Input.m_TargetX);
		WriteInt32(Input.m_TargetY);
		vBuffer.push_back(static_cast<uint8_t>(Input.m_Jump));
		vBuffer.push_back(static_cast<uint8_t>(Input.m_Fire));
		vBuffer.push_back(static_cast<uint8_t>(Input.m_Hook));
		vBuffer.push_back(static_cast<uint8_t>(Input.m_Weapon));
	}
}

bool CTasController::ReadInputs(const uint8_t *pData, size_t Size, size_t &Offset)
{
	m_vInjectedInputs.clear();

	auto ReadInt32 = [pData, Size, &Offset]() -> int {
		if(Offset + 4 > Size)
			return 0;
		int Value = pData[Offset] |
			    (pData[Offset + 1] << 8) |
			    (pData[Offset + 2] << 16) |
			    (pData[Offset + 3] << 24);
		Offset += 4;
		return Value;
	};

	while(Offset + 18 <= Size)
	{
		STasInput Input;
		Input.m_Tick = ReadInt32();
		Input.m_ClientId = pData[Offset++];
		Input.m_Direction = static_cast<int>(pData[Offset++]) - 128;
		Input.m_TargetX = ReadInt32();
		Input.m_TargetY = ReadInt32();
		Input.m_Jump = pData[Offset++];
		Input.m_Fire = pData[Offset++];
		Input.m_Hook = pData[Offset++];
		Input.m_Weapon = pData[Offset++];

		m_vInjectedInputs.push_back(Input);
	}

	return true;
}
