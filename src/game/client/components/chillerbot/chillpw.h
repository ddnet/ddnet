#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLPW_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLPW_H

#include <game/client/component.h>

#define MAX_PASSWORDS 1024
#define MAX_PASSWORD_LENGTH 2048
#define MAX_HOSTNAME_LENGTH 128

class CChillPw : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }

private:
	virtual void OnMapLoad() override;
	virtual void OnRender() override;
	virtual void OnInit() override;

	bool AuthChatAccount(int Dummy, int Offset);
	void SavePassword(const char *pServer, const char *pPassword);

	void CheckToken(const char *p, int Line, const char *pLine)
	{
		if(p)
			return;
		char aBuf[2048];
		str_format(aBuf, sizeof(aBuf), "%s:%d '%s' invalid token", g_Config.m_ClPasswordFile, Line, pLine);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		exit(1);
	}

	int m_aDummy[MAX_PASSWORDS];
	char m_aaPasswords[MAX_PASSWORDS][MAX_PASSWORD_LENGTH];
	char m_aaHostnames[MAX_PASSWORDS][MAX_HOSTNAME_LENGTH];
	char m_aCurrentServerAddr[64];
	int64_t m_ChatDelay[NUM_DUMMIES];
	int m_LoginOffset[NUM_DUMMIES];
};

#endif
