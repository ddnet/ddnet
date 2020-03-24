#ifndef GAME_SERVER_ANTIBOT_H
#define GAME_SERVER_ANTIBOT_H

class CGameContext;

class CAntibot
{
	CGameContext *m_pGameContext;
	void Update();
public:
	CAntibot();
	~CAntibot();
	void Init(CGameContext *pGameContext);
	void Dump();

	void OnPlayerInit(int ClientID);
	void OnPlayerDestroy(int ClientID);
	void OnSpawn(int ClientID);
	void OnHammerFireReloading(int ClientID);
	void OnHammerFire(int ClientID);
	void OnHammerHit(int ClientID);
	void OnDirectInput(int ClientID);
	void OnTick(int ClientID);
	void OnHookAttach(int ClientID, bool Player);
};

#endif // GAME_SERVER_ANTIBOT_H
