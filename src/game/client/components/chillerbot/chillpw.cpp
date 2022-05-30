// ChillerDragon 2020 - chillerbot ux

#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include <engine/shared/linereader.h>
#include <game/client/components/chat.h>

#include "chillpw.h"

void CChillPw::OnMapLoad()
{
	m_LoginOffset[0] = 0;
	m_LoginOffset[1] = 0;
	m_ChatDelay[0] = time_get() + time_freq() * 2;
	m_ChatDelay[1] = time_get() + time_freq() * 2;
	str_copy(m_aCurrentServerAddr, g_Config.m_UiServerAddress, sizeof(m_aCurrentServerAddr));
	str_copy(m_aCurrentServerAddrNoPort, g_Config.m_UiServerAddress, sizeof(m_aCurrentServerAddrNoPort));
	for(int i = 0; i < str_length(m_aCurrentServerAddr); i++)
	{
		if(m_aCurrentServerAddr[i] == ':')
		{
			m_aCurrentServerAddrNoPort[i] = '\0';
			break;
		}
	}
}

void CChillPw::OnRender()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(Client()->DummyConnecting())
	{
		m_LoginOffset[1] = 0;
		m_ChatDelay[1] = time_get() + time_freq() * 2;
	}
	for(int i = 0; i < NUM_DUMMIES; i++)
	{
		if(!m_ChatDelay[i])
			continue;
		if(m_ChatDelay[i] > time_get())
			continue;
		if(g_Config.m_ClDummy != i)
			continue;

		if(AuthChatAccount(i, m_LoginOffset[i] + 1))
			m_ChatDelay[i] = time_get() + time_freq() * 2;
		else
			m_ChatDelay[i] = 0;
		m_LoginOffset[i]++;
	}
}

void CChillPw::OnInit()
{
	IOHANDLE File = Storage()->OpenFile(g_Config.m_ClPasswordFile, IOFLAG_READ, IStorage::TYPE_ALL);
	char aBuf[128];
	int Line = 0;
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", g_Config.m_ClPasswordFile);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
		return;
	}
	char *pLine;
	CLineReader Reader;

	Reader.Init(File);

	while((pLine = Reader.Get()) && Line < MAX_PASSWORDS)
	{
		if(pLine[0] == '#' || pLine[0] == '\0')
			continue;
		char *pRow1 = pLine;
		CheckToken(pRow1, Line, pLine);
		char *pRow2 = (char *)str_find((const char *)pRow1 + 1, ",");
		CheckToken(pRow2, Line, pLine);
		char *pRow3 = (char *)str_find((const char *)pRow2 + 1, ",");
		CheckToken(pRow3, Line, pLine);
		str_copy(m_aaPasswords[Line], pRow3 + 1, sizeof(m_aaPasswords[Line]));
		pRow3[0] = '\0';
		m_aDummy[Line] = atoi(pRow2 + 1);
		pRow2[0] = '\0';
		str_copy(m_aaHostnames[Line], pRow1, sizeof(m_aaHostnames[Line]));
		// if the format is "hostname:*" cut of the globbed port to "hostname"
		for(int i = 0; i < str_length(pRow1); i++)
			if(pRow1[i] == ':' && pRow1[i + 1] == '*')
				m_aaHostnames[Line][i] = '\0';
		Line++;
	}
	str_format(aBuf, sizeof(aBuf), "loaded %d passwords from '%s'", Line, g_Config.m_ClPasswordFile);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);

	io_close(File);
}

void CChillPw::SavePassword(const char *pServer, const char *pPassword)
{
	IOHANDLE File = Storage()->OpenFile(g_Config.m_ClPasswordFile, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(File)
	{
		io_write(File, pServer, str_length(pServer));
		io_write(File, ";", 1);
		io_write(File, pPassword, str_length(pPassword));
		io_write_newline(File);
		io_close(File);
	}
}

bool CChillPw::AuthChatAccount(int Dummy, int Offset)
{
	int found = 0;
	for(int i = 0; i < MAX_PASSWORDS; i++)
	{
		if(m_aaHostnames[i][0] == '\0')
			continue;
		if(!str_find(m_aaHostnames[i], ":") || str_find(m_aaHostnames[i], ":*"))
		{
			if(str_comp(m_aCurrentServerAddrNoPort, m_aaHostnames[i]))
				continue;
		}
		else
		{
			if(str_comp(m_aCurrentServerAddr, m_aaHostnames[i]))
			{
				// if it has a port and it does not match skip to next entry
				if(str_find(m_aCurrentServerAddr, ":"))
					continue;

				// skip all non default port entrys
				if(!str_endswith(m_aaHostnames[i], ":8303"))
					continue;

				// if hostname without port does not match skip
				if(!str_startswith(m_aaHostnames[i], m_aCurrentServerAddr) || m_aaHostnames[i][str_length(m_aCurrentServerAddr)] != ':')
					continue;
			}
		}
		if(Dummy != m_aDummy[i])
			continue;
		if(Offset > ++found)
			continue;
		Console()->ExecuteLine(m_aaPasswords[i]);
		return true;
	}
	return false;
}
