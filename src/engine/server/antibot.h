#ifndef ENGINE_SERVER_ANTIBOT_H
#define ENGINE_SERVER_ANTIBOT_H

#include <antibot/antibot_data.h>
#include <engine/antibot.h>

class CAntibot : public IEngineAntibot
{
	class IServer *m_pServer;
	class IConsole *m_pConsole;
	class IGameServer *m_pGameServer;

	class IServer *Server() const { return m_pServer; }
	class IConsole *Console() const { return m_pConsole; }
	class IGameServer *GameServer() const { return m_pGameServer; }

	CAntibotData m_Data;
	CAntibotRoundData m_RoundData;
	bool m_Initialized;

	void Update();
	static void Send(int ClientID, const void *pData, int Size, int Flags, void *pUser);
	static void Log(const char *pMessage, void *pUser);
	static void Report(int ClientID, const char *pMessage, void *pUser);

public:
	CAntibot();
	virtual ~CAntibot();

	// Engine
	void Init() override;

	void OnEngineTick() override;
	void OnEngineClientJoin(int ClientID, bool Sixup) override;
	void OnEngineClientDrop(int ClientID, const char *pReason) override;
	bool OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags) override;
	bool OnEngineServerMessage(int ClientID, const void *pData, int Size, int Flags) override;
	bool OnEngineSimulateClientMessage(int *pClientID, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags) override;

	// Game
	void RoundStart(class IGameServer *pGameServer) override;
	void RoundEnd() override;

	void OnPlayerInit(int ClientID) override;
	void OnPlayerDestroy(int ClientID) override;
	void OnSpawn(int ClientID) override;
	void OnHammerFireReloading(int ClientID) override;
	void OnHammerFire(int ClientID) override;
	void OnHammerHit(int ClientID, int TargetID) override;
	void OnDirectInput(int ClientID) override;
	void OnCharacterTick(int ClientID) override;
	void OnHookAttach(int ClientID, bool Player) override;

	void Dump() override;
};

extern IEngineAntibot *CreateEngineAntibot();

#endif // ENGINE_SERVER_ANTIBOT_H
