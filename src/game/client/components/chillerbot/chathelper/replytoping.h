#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_REPLYTOPING_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_REPLYTOPING_H

/*
	CReplyToPing

	Description:
		One instance per message and reply.
		Trys to respond to common teeworlds chat messages.

	Example:
		char aResponse[1024];
		CReplyToPing ReplyToPing = CReplyToPing(this, aName, m_pClient->m_aClients[ClientID].m_aClan, pLine, aResponse, sizeof(aResponse));
		if(ReplyToPing.Reply())
			m_pClient->m_Chat.Say(0, aResponse);

	Public methods:
		Reply()
*/
class CReplyToPing
{
	class CChatHelper *m_pChatHelper;
	class CChatHelper *ChatHelper() { return m_pChatHelper; }
	class CLangParser &LangParser();

	const char *m_pMessageAuthor;
	const char *m_pMessageAuthorClan;
	const char *m_pMessage;
	char *m_pResponse;
	long unsigned int m_SizeOfResponse;

	bool WhyWar(const char *pVictim, bool IsCheck = false);
	/*
		GetSuffixLen

		if pStr ends with pSuffix return length of pSuffix
	*/
	int GetSuffixLen(const char *pStr, const char *pSuffix);
	/*
		StripSpacesAndPunctuationAndOwnName

		If the input pStr "yourname: hello world  ???"
		is given the pStripped will point to "hello world"
	*/
	void StripSpacesAndPunctuationAndOwnName(const char *pStr, char *pStripped, int SizeOfStripped);
	/*
		IsEmptyStr

		returns true if pStr is null "" or " " or "			 "
	*/
	bool IsEmptyStr(const char *pStr);

public:
	CReplyToPing(class CChatHelper *pChatHelper, const char *pMessageAuthor, const char *pMessageAuthorClan, const char *pMessage, char *pResponse, long unsigned int SizeOfResponse);

	/*
		Reply

		Return:
			true - if known message found and got a reply
			false - if no known message found

		Side effect:
			Fills pResponse buffer with the response message
	*/
	bool Reply();
};

#endif
