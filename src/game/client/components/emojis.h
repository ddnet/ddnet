/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EMOJIS_H
#define GAME_CLIENT_COMPONENTS_EMOJIS_H
#include <base/tl/array.h>
#include <game/client/component.h>

class CEmojis : public CComponent
{
public:
	struct CEmojiInfo {
		int m_ID;
		int index;
		int length;
		bool operator<(const CEmojiInfo &Other) {
			if (index < Other.index)
				return true;
			if (index == Other.index)
				return length >= Other.length;
			return false;
		}
	};
	struct CEmoji
	{
		int m_ID;
		char m_UTF[17];
		char m_UNICODE[64];
		char m_Alias[64];
	};
	void Render(int i, float x, float y, float w, float h);
	array<CEmoji> m_aEmojis;
private:
	void LoadEmojisIndexfile();
	void OnInit();
	virtual void OnMessage(int MsgType, void *pRawMsg);
};
#endif
