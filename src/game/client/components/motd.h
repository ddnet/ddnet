/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MOTD_H
#define GAME_CLIENT_COMPONENTS_MOTD_H
#include <game/client/component.h>

class CMotd : public CComponent
{
	// motd
	char m_aServerMotd[900];
	int64_t m_ServerMotdTime;

public:
	virtual int Sizeof() const override { return sizeof(*this); }

	const char *ServerMotd() const { return m_aServerMotd; }
	void Clear();
	bool IsActive();

	virtual void OnRender() override;
	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual bool OnInput(IInput::CEvent Event) override;
};

#endif
