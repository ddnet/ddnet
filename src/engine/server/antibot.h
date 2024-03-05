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
	static void Kick(int ClientId, const char *pMessage, void *pUser);
	static void Log(const char *pMessage, void *pUser);
	static void Report(int ClientId, const char *pMessage, void *pUser);
	static void Send(int ClientId, const void *pData, int Size, int Flags, void *pUser);
	static void Teehistorian(const void *pData, int Size, void *pUser);

public:
	CAntibot();
	virtual ~CAntibot();

	// Engine
	void Init() override;

	void OnEngineTick() override;
	void OnEngineClientJoin(int ClientId, bool Sixup) override;
	void OnEngineClientDrop(int ClientId, const char *pReason) override;
	bool OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags) override;
	bool OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags) override;
	bool OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags) override;

	// Game
	void RoundStart(class IGameServer *pGameServer) override;
	void RoundEnd() override;

	void OnPlayerInit(int ClientId) override;
	void OnPlayerDestroy(int ClientId) override;
	void OnSpawn(int ClientId) override;
	void OnHammerFireReloading(int ClientId) override;
	void OnHammerFire(int ClientId) override;
	void OnHammerHit(int ClientId, int TargetId) override;
	void OnDirectInput(int ClientId) override;
	void OnCharacterTick(int ClientId) override;
	void OnHookAttach(int ClientId, bool Player) override;

	void ConsoleCommand(const char *pCommand) override;
};

extern IEngineAntibot *CreateEngineAntibot();

#endif // ENGINE_SERVER_ANTIBOT_H
