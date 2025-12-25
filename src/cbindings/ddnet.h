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
	CPlayer *GetPlayer(uint32_t Id);
	const CPlayer *GetPlayer(uint32_t Id) const;

	int Init(const char *pDirectory);

	// Game trait
	void PlayerJoin(uint32_t Id);
	void PlayerReady(uint32_t Id);
	void PlayerInput(uint32_t Id, const CNetObj_PlayerInput *pInput);
	void PlayerLeave(uint32_t Id);

	void PlayerNet(uint32_t Id, const unsigned char *pNetMsg, uint32_t Len);
	void OnCommand(uint32_t Id, const char *pCommand);

	void SwapTees(uint32_t Id1, uint32_t Id2);

	void Tick(int32_t CurTime);

	// GameValidator trait
	uint32_t MaxTeeId() const;
	int32_t PlayerTeam(uint32_t Id) const;
	void SetPlayerTeam(uint32_t Id, int32_t Team);
	void TeePos(uint32_t Id, bool *pHasPos, int32_t *pX, int32_t *pY) const;
	void SetTeePos(uint32_t Id, bool HasPos, int32_t X, int32_t Y);

	// Snapper trait
	void Snap(const uint8_t **ppData, uint32_t *pLen); // TODO: snapper callback
};
