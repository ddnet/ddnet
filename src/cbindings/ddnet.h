#pragma once

#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <cstdint>

class CGameContext;
class CServer;
class CPlayer;
class IEngine;
class IKernel;
struct CNetObj_PlayerInput;

class CServerLib {
	CServer *m_pServer = nullptr;
	IKernel *m_pKernel = nullptr;
	IEngine *m_pEngine = nullptr;
	CGameContext *m_GameContext;

	bool m_aHasInput[MAX_CLIENTS];
	CNetObj_PlayerInput m_aInputs[MAX_CLIENTS];
	uint8_t m_aSnapBuffer[CSnapshot::MAX_SIZE];

	void SwapInputs(int Client1, int Client2)
	{
		std::swap(m_aInputs[Client1], m_aInputs[Client2]);
		std::swap(m_aHasInput[Client1], m_aHasInput[Client2]);
	}

public:
	CServerLib();
	~CServerLib();

	CServer *Server() { return m_pServer; }
	CGameContext *GameContext() { return m_GameContext; }
	const CGameContext *GameContext() const { return m_GameContext; }
	CPlayer *GetPlayer(int32_t Id);
	const CPlayer *GetPlayer(int32_t Id) const;

	int Init(const char *pDirectory);

	// Game trait
	void PlayerJoin(int32_t Id);
	void PlayerReady(int32_t Id);
	void PlayerInput(int32_t Id, const CNetObj_PlayerInput *pInput);
	void PlayerLeave(int32_t Id);

	void PlayerNet(int32_t Id, const unsigned char *pNetMsg, uint32_t Len);
	void OnCommand(int32_t Id, const char *pCommand);

	void SwapTees(int32_t Id1, int32_t Id2);

	void Tick(int32_t CurTime);

	// GameValidator trait
	int32_t MaxTeeId() const;
	int32_t PlayerTeam(int32_t Id) const;
	void SetPlayerTeam(int32_t Id, int32_t Team);
	void TeePos(int32_t Id, bool *pHasPos, int32_t *pX, int32_t *pY) const;
	void SetTeePos(int32_t Id, bool HasPos, int32_t X, int32_t Y);

	// Snapper trait
	void Snap(const uint8_t **ppData, uint32_t *pLen); // TODO: snapper callback
};
