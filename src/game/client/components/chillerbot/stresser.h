#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_STRESSER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_STRESSER_H

class CStresser : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;

	CNetObj_PlayerInput m_InputData[NUM_DUMMIES];
	bool m_SendData[NUM_DUMMIES];

private:
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnMapLoad() override;
	virtual void OnConsoleInit() override;

	static void ConchainPenTest(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void OnChatMessage(int ClientID, int Team, const char *pMsg);

	const char *GetPentestCommand(const char *pFilename);
	const char *GetRandomChatCommand();
	void RandomMovements();

	std::vector<char *> m_vChatCmds;
	int64_t m_RequestCmdlist;
	int m_PenDelay;
};
#endif
