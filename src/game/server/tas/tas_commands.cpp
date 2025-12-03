/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#include "tas_commands.h"

#include "tas_controller.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

// Helper to get TAS controller from user data
static CTasController *GetTasController(void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	return pGameServer->TasController();
}

// Helper to check if TAS mode is enabled
static bool CheckTasEnabled(CGameContext *pGameServer)
{
	CTasController *pTas = pGameServer->TasController();
	if(!pTas || !pTas->IsEnabled())
	{
		pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"TAS mode is not enabled. Set sv_tas_server 1 to enable.");
		return false;
	}
	return true;
}

void RegisterTasCommands(CGameContext *pGameServer, IConsole *pConsole)
{
	// Tick control commands
	pConsole->Register("tas_pause", "", CFGFLAG_SERVER, ConTasPause, pGameServer,
		"Pause the game (TAS mode)");
	pConsole->Register("tas_resume", "", CFGFLAG_SERVER, ConTasResume, pGameServer,
		"Resume the game (TAS mode)");
	pConsole->Register("tas_toggle", "", CFGFLAG_SERVER, ConTasToggle, pGameServer,
		"Toggle pause state (TAS mode)");
	pConsole->Register("tas_step", "", CFGFLAG_SERVER, ConTasStep, pGameServer,
		"Step forward 1 tick (TAS mode)");
	pConsole->Register("tas_step_n", "i[ticks]", CFGFLAG_SERVER, ConTasStepN, pGameServer,
		"Step forward N ticks (TAS mode)");
	pConsole->Register("tas_rewind", "i[tick]", CFGFLAG_SERVER, ConTasRewind, pGameServer,
		"Rewind to specific tick (TAS mode)");
	pConsole->Register("tas_goto", "i[tick]", CFGFLAG_SERVER, ConTasGoto, pGameServer,
		"Go to specific tick (forward or backward) (TAS mode)");
	pConsole->Register("tas_ff", "i[tick]", CFGFLAG_SERVER, ConTasFastForward, pGameServer,
		"Fast forward to specific tick (TAS mode)");
	pConsole->Register("tas_speed", "f[multiplier]", CFGFLAG_SERVER, ConTasSpeed, pGameServer,
		"Set playback speed (0.1 to 10.0) (TAS mode)");
	pConsole->Register("tas_status", "", CFGFLAG_SERVER, ConTasStatus, pGameServer,
		"Show TAS status information");

	// Recording/Playback commands
	pConsole->Register("tas_record", "", CFGFLAG_SERVER, ConTasRecord, pGameServer,
		"Start recording inputs (TAS mode)");
	pConsole->Register("tas_play", "", CFGFLAG_SERVER, ConTasPlay, pGameServer,
		"Start playback of injected inputs (TAS mode)");
	pConsole->Register("tas_stop", "", CFGFLAG_SERVER, ConTasStop, pGameServer,
		"Stop recording or playback (TAS mode)");
	pConsole->Register("tas_save", "s[filename]", CFGFLAG_SERVER, ConTasSave, pGameServer,
		"Save TAS to file (TAS mode)");
	pConsole->Register("tas_load", "s[filename]", CFGFLAG_SERVER, ConTasLoad, pGameServer,
		"Load TAS from file (TAS mode)");

	// Input injection commands
	pConsole->Register("tas_input", "i[tick] i[dir] i[aim_x] i[aim_y] i[jump] i[fire] i[hook]", CFGFLAG_SERVER, ConTasInput, pGameServer,
		"Inject input at specific tick (TAS mode)");
	pConsole->Register("tas_input_clear", "", CFGFLAG_SERVER, ConTasInputClear, pGameServer,
		"Clear all injected inputs (TAS mode)");

	// Multi-client commands
	pConsole->Register("tas_control", "i[client_id]", CFGFLAG_SERVER, ConTasControl, pGameServer,
		"Set controlling client (TAS mode)");
	pConsole->Register("tas_collab_add", "i[client_id]", CFGFLAG_SERVER, ConTasCollabAdd, pGameServer,
		"Add collaborator (TAS mode)");
	pConsole->Register("tas_collab_remove", "i[client_id]", CFGFLAG_SERVER, ConTasCollabRemove, pGameServer,
		"Remove collaborator (TAS mode)");

	// History management
	pConsole->Register("tas_history_clear", "", CFGFLAG_SERVER, ConTasHistoryClear, pGameServer,
		"Clear TAS history (TAS mode)");
}

void ConTasPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->Pause();
}

void ConTasResume(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->Resume();
}

void ConTasToggle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->TogglePause();
}

void ConTasStep(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->StepForward(1);
}

void ConTasStepN(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int Ticks = pResult->GetInteger(0);
	if(Ticks <= 0)
	{
		pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"Number of ticks must be positive");
		return;
	}

	pGameServer->TasController()->StepForward(Ticks);
}

void ConTasRewind(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int TargetTick = pResult->GetInteger(0);
	pGameServer->TasController()->Rewind(TargetTick);
}

void ConTasGoto(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int TargetTick = pResult->GetInteger(0);
	pGameServer->TasController()->GotoTick(TargetTick);
}

void ConTasFastForward(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int TargetTick = pResult->GetInteger(0);
	pGameServer->TasController()->FastForward(TargetTick);
}

void ConTasSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	float Speed = pResult->GetFloat(0);
	pGameServer->TasController()->SetSpeed(Speed);
}

void ConTasStatus(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	CTasController *pTas = pGameServer->TasController();

	if(!pTas)
	{
		pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
			"TAS mode is not available");
		return;
	}

	char aStatus[1024];
	pTas->FormatStatus(aStatus, sizeof(aStatus));
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aStatus);
}

void ConTasRecord(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->StartRecording();
}

void ConTasPlay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->StartPlayback();
}

void ConTasStop(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->StopPlayback();
}

void ConTasSave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	const char *pFilename = pResult->GetString(0);
	pGameServer->TasController()->SaveToFile(pFilename);
}

void ConTasLoad(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	const char *pFilename = pResult->GetString(0);
	pGameServer->TasController()->LoadFromFile(pFilename);
}

void ConTasInput(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	STasInput Input;
	Input.m_Tick = pResult->GetInteger(0);
	Input.m_ClientId = 0; // Default to first client
	Input.m_Direction = pResult->GetInteger(1);
	Input.m_TargetX = pResult->GetInteger(2);
	Input.m_TargetY = pResult->GetInteger(3);
	Input.m_Jump = pResult->GetInteger(4);
	Input.m_Fire = pResult->GetInteger(5);
	Input.m_Hook = pResult->GetInteger(6);
	Input.m_Weapon = 0;

	pGameServer->TasController()->InjectInput(Input);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Input injected at tick %d", Input.m_Tick);
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void ConTasInputClear(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->ClearInjectedInputs();
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
		"All injected inputs cleared");
}

void ConTasControl(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int ClientId = pResult->GetInteger(0);
	pGameServer->TasController()->SetControlClient(ClientId);
}

void ConTasCollabAdd(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int ClientId = pResult->GetInteger(0);
	pGameServer->TasController()->AddCollaborator(ClientId);

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Client %d added as collaborator", ClientId);
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void ConTasCollabRemove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	int ClientId = pResult->GetInteger(0);
	pGameServer->TasController()->RemoveCollaborator(ClientId);

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Client %d removed as collaborator", ClientId);
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void ConTasHistoryClear(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pGameServer = static_cast<CGameContext *>(pUserData);
	if(!CheckTasEnabled(pGameServer))
		return;

	pGameServer->TasController()->History()->Clear();
	pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas",
		"TAS history cleared");
}
