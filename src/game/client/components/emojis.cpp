/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

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

int CEmojis::Num() const
{
	return m_aEmojis.size();
}

const CEmojis::CEmoji *CEmojis::GetByAlias(const char *alias) const
{
	for (int i = 0; i < m_aEmojis.size(); i++)
	{
		if (str_comp(alias, m_aEmojis[i].m_Alias) == 0) return GetByIndex(i);
	}
	return NULL;
}

const CEmojis::CEmoji *CEmojis::GetByIndex(int index) const
{
	return &m_aEmojis[max(0, index%m_aEmojis.size())];
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
