/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TAS_TAS_CONTROLLER_H
#define GAME_SERVER_TAS_TAS_CONTROLLER_H

#include "tas_history.h"
#include "tas_state.h"

#include <algorithm>
#include <set>
#include <vector>

class CGameContext;

// TAS server modes
enum class ETasMode
{
	DISABLED = 0,
	SINGLE_CONTROL,  // First client controls everything
	COLLABORATIVE,   // Multiple clients can participate
};

// TAS playback state
enum class ETasPlaybackState
{
	STOPPED = 0,
	PLAYING,
	RECORDING,
};

class CTasController
{
public:
	CTasController(CGameContext *pGameServer);
	~CTasController();

	// Mode management
	void SetMode(ETasMode Mode);
	ETasMode GetMode() const { return m_Mode; }
	bool IsEnabled() const { return m_Mode != ETasMode::DISABLED; }

	// Called from the game tick loop
	bool ShouldAdvanceTick() const;
	void OnPreTick();
	void OnPostTick();

	// Tick manipulation commands
	void Pause();
	void Resume();
	void TogglePause();
	void StepForward(int NumTicks = 1);
	void FastForward(int TargetTick);
	void Rewind(int TargetTick);
	void GotoTick(int TargetTick); // Handles both forward and backward
	void SetSpeed(float Speed);    // 0.1 to 10.0

	// State queries
	bool IsPaused() const { return m_Paused; }
	int GetCurrentTick() const;
	int GetTargetTick() const { return m_TargetTick; }
	float GetSpeed() const { return m_Speed; }
	bool IsRewinding() const { return m_Rewinding; }
	bool IsFastForwarding() const { return m_FastForwarding; }

	// Input injection
	void InjectInput(const STasInput &Input);
	void QueueInputSequence(const std::vector<STasInput> &vInputs);
	bool HasInjectedInput(int ClientId, int Tick) const;
	const STasInput *GetInjectedInput(int ClientId, int Tick) const;
	void ClearInjectedInputs();
	void ClearInjectedInputsAfterTick(int Tick);

	// Recording/Playback
	void StartRecording();
	void StopRecording();
	void StartPlayback();
	void StopPlayback();
	ETasPlaybackState GetPlaybackState() const { return m_PlaybackState; }

	// History access
	CTasHistory *History() { return &m_History; }
	const CTasHistory *History() const { return &m_History; }

	// Client management (for collaborative mode)
	void SetControlClient(int ClientId);
	int GetControlClient() const { return m_ControlClientId; }
	void AddCollaborator(int ClientId);
	void RemoveCollaborator(int ClientId);
	bool IsCollaborator(int ClientId) const;
	bool CanControl(int ClientId) const;

	// Save/Load TAS files
	bool SaveToFile(const char *pFilename);
	bool LoadFromFile(const char *pFilename);

	// Status display
	struct STasStatus
	{
		ETasMode m_Mode;
		ETasPlaybackState m_PlaybackState;
		bool m_Paused;
		float m_Speed;
		int m_CurrentTick;
		int m_TargetTick;
		int m_ControlClientId;
		int m_CollaboratorCount;
		int m_InjectedInputCount;
		int m_RecordedInputCount;
		CTasHistory::SHistoryStats m_HistoryStats;
	};
	STasStatus GetStatus() const;

	// Format status as string for console output
	void FormatStatus(char *pBuffer, int BufferSize) const;

private:
	CGameContext *m_pGameServer;

	// Mode and state
	ETasMode m_Mode;
	ETasPlaybackState m_PlaybackState;
	bool m_Paused;
	float m_Speed;
	int m_TargetTick;       // For fast forward/rewind
	int m_StepRemaining;    // Ticks to step through
	bool m_Rewinding;       // Currently in rewind operation
	bool m_FastForwarding;  // Currently in fast forward operation

	// History
	CTasHistory m_History;

	// Input injection (sorted by tick, then client id)
	std::vector<STasInput> m_vInjectedInputs;

	// Recorded inputs during recording session
	std::vector<STasInput> m_vRecordedInputs;
	int m_RecordingStartTick;

	// Client management
	int m_ControlClientId;
	std::set<int> m_CollaboratorClientIds;

	// Speed control timing
	float m_SpeedAccumulator;

	// Internal helpers
	void SaveCurrentState();
	bool RestoreState(int Tick);
	void RecordCurrentInputs();
	void DestroyAllDynamicEntities();
	void ForceFullSnapshots();

	// Binary helpers for file I/O
	void WriteHeader(std::vector<uint8_t> &vBuffer) const;
	bool ReadHeader(const uint8_t *pData, size_t Size, size_t &Offset);
	void WriteInputs(std::vector<uint8_t> &vBuffer) const;
	bool ReadInputs(const uint8_t *pData, size_t Size, size_t &Offset);
};

#endif // GAME_SERVER_TAS_TAS_CONTROLLER_H
