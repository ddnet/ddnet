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
	virtual void Init();

	virtual void OnEngineTick();
	virtual void OnEngineClientJoin(int ClientID, bool Sixup);
	virtual void OnEngineClientDrop(int ClientID, const char *pReason);
	virtual void OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags);

	// Game
	virtual void RoundStart(class IGameServer *pGameServer);
	virtual void RoundEnd();

	virtual void OnPlayerInit(int ClientID);
	virtual void OnPlayerDestroy(int ClientID);
	virtual void OnSpawn(int ClientID);
	virtual void OnHammerFireReloading(int ClientID);
	virtual void OnHammerFire(int ClientID);
	virtual void OnHammerHit(int ClientID);
	virtual void OnDirectInput(int ClientID);
	virtual void OnCharacterTick(int ClientID);
	virtual void OnHookAttach(int ClientID, bool Player);

	virtual void Dump();
};

extern IEngineAntibot *CreateEngineAntibot();

#endif // ENGINE_SERVER_ANTIBOT_H
