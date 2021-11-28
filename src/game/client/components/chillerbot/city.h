#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CITY_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CITY_H

#include <game/client/component.h>

class CCityHelper : public CComponent
{
private:
	virtual void OnRender();
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnMessage(int MsgType, void *pRawMsg);

	static void ConchainShowWallet(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConAutoDropMoney(IConsole::IResult *pResult, void *pUserData);

	void SetAutoDrop(bool Drop, int Delay, int ClientID);
	void OnServerMsg(const char *pMsg);

	bool m_AutoDropMoney[NUM_DUMMIES];

	int m_WalletMoney[NUM_DUMMIES];
	int64_t m_NextWalletDrop[NUM_DUMMIES];
	int m_WalletDropDelay[NUM_DUMMIES];

	void SetWalletMoney(int Money, int ClientID = -1);
	void AddWalletMoney(int Money, int ClientID = -1);

public:
	void DropAllMoney(int ClientID);
	int WalletMoney(int ClientID = -1);
};

#endif
