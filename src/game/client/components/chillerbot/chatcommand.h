#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATCOMMAND_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATCOMMAND_H

#include <game/client/component.h>

class CChatCommand : public CComponent
{
	virtual void OnMessage(int MsgType, void *pRawMsg) override;

	void OnServerMsg(const char *pMsg);

	bool ParseChatCmd(char Prefix, int ClientId, int Team, const char *pCmdWithArgs);

	/*
		Function: OnChatCmd

		define the same signature in your component
		and hook it inside (manually)

		to get parsed arguments

		only return true if a valid command was matched
	*/
	bool OnChatCmd(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine);

	/*
		Function: OnNoChatCommandMatches

		define the same signature in your component
		and hook it inside (manually)

		will be called if no component matched
		a valid chat command in this message
	*/
	void OnNoChatCommandMatches(int ClientId, int Team, const char *pMsg);

public:
	virtual int Sizeof() const override { return sizeof(*this); }

	bool OnChatMsg(int ClientId, int Team, const char *pMsg);
};

#endif
