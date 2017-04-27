/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>
#include <string.h>

#include <game/generated/client_data.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include "emojis.h"

void CEmojis::LoadEmojisIndexfile()
{
	IOHANDLE File = Storage()->OpenFile("emojis/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if (!File)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "emojis", "couldn't open index file");
		return;
	}

	CLineReader LineReader;
	LineReader.Init(File);

	char aUTF[17];
	char aUNICODE[64];
	char *pLine;

	int curr = 0;
	bool failed = false;

	while ((pLine = LineReader.Get()) && !failed)
	{
		if (!str_length(pLine)) continue;
		str_copy(aUTF, pLine, sizeof(aUTF));

		pLine = LineReader.Get();
		if (!pLine)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "emojis", "unexpected end of index file");
			failed = true;
			break;
		}
		str_copy(aUNICODE, pLine, sizeof(aUNICODE));

		pLine = LineReader.Get();
		if (!pLine)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "emojis", "unexpected end of index file");
			failed = true;
			break;
		}

		int Count = str_toint(pLine); //TODO: add verification if valid number
		
		while (Count-- > 0) {
			char *pAlias = LineReader.Get();
			if (!pAlias)
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "emojis", "unexpected end of index file");
				failed = true;
				break;
			}

			CEmoji Emoji;
			Emoji.m_ID = curr;
			str_copy(Emoji.m_UTF, aUTF, sizeof(Emoji.m_UTF));
			str_copy(Emoji.m_UNICODE, aUNICODE, sizeof(Emoji.m_UNICODE));
			str_copy(Emoji.m_Alias, pAlias, sizeof(Emoji.m_Alias));

			char aBuf[128];

			if (g_Config.m_Debug)
			{
				str_format(aBuf, sizeof(aBuf), "loaded emoji '%s': %s", aUNICODE, pAlias);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "emojis", aBuf);
			}

			m_aEmojis.add(Emoji);

		}
		curr++;
	}
	io_close(File);
}

void CEmojis::OnInit()
{
	m_aEmojis.clear();
	LoadEmojisIndexfile();
	if (!m_aEmojis.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "emojis", "failed to load emojis. folder='emojis/'");
	}
}

void CEmojis::Render(int i, float x, float y, float w, float h)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOJIS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_0023_20E3 + i);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

// Taken from http://stackoverflow.com/a/779960
// You must free the result if result is non-NULL.
char *replace(char *orig, char *rep, char *with) {
	char *result; // the return string
	char *ins;    // the next insert point
	char *tmp;    // varies
	int len_rep;  // length of rep (the string to remove)
	int len_with; // length of with (the string to replace rep with)
	int len_front; // distance between rep and end of last rep
	int count;    // number of replacements

				  // sanity checks and initialization
	if (!orig || !rep)
		return NULL;
	len_rep = strlen(rep);
	if (len_rep == 0)
		return NULL; // empty rep causes infinite loop during count
	if (!with)
		with = "";
	len_with = strlen(with);

	// count the number of replacements needed
	ins = orig;
	for (count = 0; tmp = strstr(ins, rep); ++count) {
		ins = tmp + len_rep;
	}

	tmp = result = (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

	if (!result)
		return NULL;

	// first time through the loop, all the variable are set correctly
	// from here on,
	//    tmp points to the end of the result string
	//    ins points to the next occurrence of rep in orig
	//    orig points to the remainder of orig after "end of rep"
	while (count--) {
		ins = strstr(orig, rep);
		len_front = ins - orig;
		tmp = strncpy(tmp, orig, len_front) + len_front;
		tmp = strcpy(tmp, with) + len_with;
		orig += len_front + len_rep; // move to next "end of rep"
	}
	strcpy(tmp, orig);
	return result;
}

void CEmojis::OnMessage(int MsgType, void *pRawMsg)
{
	if (MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if (m_aEmojis.size() == 0) return;

		pMsg->m_pMessage = replace((char *)pMsg->m_pMessage, m_aEmojis[0].m_Alias, m_aEmojis[0].m_UTF);

		for (int i = 1; i < m_aEmojis.size(); i++) {
			char* tmp = replace((char*)pMsg->m_pMessage, m_aEmojis[i].m_Alias, m_aEmojis[i].m_UTF);
			// if (i != 0)
			// free previous
			free((void *)pMsg->m_pMessage);
			pMsg->m_pMessage = tmp;
		}
	}
}
