#ifndef ENGINE_SHARED_CHILLERBOT_LANGPARSER_H
#define ENGINE_SHARED_CHILLERBOT_LANGPARSER_H

class CLangParser
{
public:
	bool IsGreeting(const char *pMsg);
	bool IsBye(const char *pMsg);
	bool IsInsult(int ClientID, const char *pMsg, int MsgLen, int NameLen);
	bool IsQuestionWhy(const char *pMsg);
};

#endif
