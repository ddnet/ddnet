#ifndef GAME_CHAT_H
#define GAME_CHAT_H

// Author zero denotes the server.
// Sequence zero denotes a message without a server-assigned identity.
struct CChatMessageId
{
	int m_Author = 0;
	int m_Sequence = 0;

	bool IsValid() const { return m_Author >= 0 && m_Sequence > 0; }
	bool operator==(const CChatMessageId &Other) const = default;
};

struct CChatRedaction
{
	CChatMessageId m_Id;
	bool m_Through;

	bool Matches(CChatMessageId Id) const
	{
		return Id.IsValid() && Id.m_Author == m_Id.m_Author &&
		       (m_Through ? Id.m_Sequence <= m_Id.m_Sequence : Id.m_Sequence == m_Id.m_Sequence);
	}
};

#endif // GAME_CHAT_H
