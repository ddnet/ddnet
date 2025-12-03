/* (c) DDNet developers. See licence.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TAS_TAS_COMMANDS_H
#define GAME_SERVER_TAS_TAS_COMMANDS_H

#include <engine/console.h>

class CGameContext;

// Registration function called from CGameContext::OnConsoleInit
void RegisterTasCommands(CGameContext *pGameServer, IConsole *pConsole);

// Tick control commands
void ConTasPause(IConsole::IResult *pResult, void *pUserData);
void ConTasResume(IConsole::IResult *pResult, void *pUserData);
void ConTasToggle(IConsole::IResult *pResult, void *pUserData);
void ConTasStep(IConsole::IResult *pResult, void *pUserData);
void ConTasStepN(IConsole::IResult *pResult, void *pUserData);
void ConTasRewind(IConsole::IResult *pResult, void *pUserData);
void ConTasGoto(IConsole::IResult *pResult, void *pUserData);
void ConTasFastForward(IConsole::IResult *pResult, void *pUserData);
void ConTasSpeed(IConsole::IResult *pResult, void *pUserData);
void ConTasStatus(IConsole::IResult *pResult, void *pUserData);

// Recording/Playback commands
void ConTasRecord(IConsole::IResult *pResult, void *pUserData);
void ConTasPlay(IConsole::IResult *pResult, void *pUserData);
void ConTasStop(IConsole::IResult *pResult, void *pUserData);
void ConTasSave(IConsole::IResult *pResult, void *pUserData);
void ConTasLoad(IConsole::IResult *pResult, void *pUserData);

// Input injection commands
void ConTasInput(IConsole::IResult *pResult, void *pUserData);
void ConTasInputClear(IConsole::IResult *pResult, void *pUserData);

// Multi-client commands
void ConTasControl(IConsole::IResult *pResult, void *pUserData);
void ConTasCollabAdd(IConsole::IResult *pResult, void *pUserData);
void ConTasCollabRemove(IConsole::IResult *pResult, void *pUserData);

// History management
void ConTasHistoryClear(IConsole::IResult *pResult, void *pUserData);

#endif // GAME_SERVER_TAS_TAS_COMMANDS_H
