#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_REPLYTOPING_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_REPLYTOPING_H

class CReplyToPing
{
    class CChatHelper *m_pChatHelper;
    class CChatHelper *ChatHelper() { return m_pChatHelper; }
    class CLangParser &LangParser();

public:
    CReplyToPing(class CChatHelper *pChatHelper);

    bool ReplyToLastPing(const char *pMessageAuthor, const char *pMessageAuthorClan, const char *pMessage, char *pResponse, int SizeOfResponse);
};

#endif
