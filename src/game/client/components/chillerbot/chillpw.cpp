// ChillerDragon 2020 - chillerbot ux

#include <game/client/components/chat.h>
#include <engine/shared/linereader.h>

#include "chillpw.h"

void CChillPw::OnMapLoad()
{
    m_ChatDelay[0] = time_get() + time_freq() * 2;
    m_ChatDelay[1] = time_get() + time_freq() * 2;
}

void CChillPw::OnRender()
{
    for (int i = 0; i < NUM_DUMMIES; i++)
    {
        if (!m_ChatDelay[i])
            continue;
        if (m_ChatDelay[i] > time_get())
            continue;
        if (g_Config.m_ClDummy != i)
            continue;

        if (AuthChatAccount(i))
            m_ChatDelay[i] = 0;
    }
}

void CChillPw::OnInit()
{
    IOHANDLE File = Storage()->OpenFile(PASSWORD_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	char aBuf[128];
    int Line = 0;
	if(!File)
    {
        str_format(aBuf, sizeof(aBuf), "failed to open '%s'", PASSWORD_FILE);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);
        return;
    }
    char *pLine;
    CLineReader Reader;

    Reader.Init(File);

    while((pLine = Reader.Get()) && Line < MAX_PASSWORDS)
    {
        if (pLine[0] == '#' || pLine[0] == '\0')
            continue;
        const char *p = strtok(pLine, ",");CheckToken(p, Line, pLine);
        str_copy(m_aaHostnames[Line], p, sizeof(m_aaHostnames[Line]));
        p = strtok(NULL, ",");CheckToken(p, Line, pLine);
        m_aDummy[Line] = atoi(p);
        p = strtok(NULL, ",");CheckToken(p, Line, pLine);
        str_copy(m_aaPasswords[Line], p, sizeof(m_aaPasswords[Line]));
        Line++;
    }
    str_format(aBuf, sizeof(aBuf), "loaded %d passwords from '%s'", Line, PASSWORD_FILE);
    Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chillerbot", aBuf);

    io_close(File);
}

void CChillPw::SavePassword(const char *pServer, const char *pPassword)
{
    IOHANDLE File = Storage()->OpenFile(PASSWORD_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
    if(File)
    {
        io_write(File, pServer, str_length(pServer));
        io_write(File, ";", 1);
        io_write(File, pPassword, str_length(pPassword));
        io_write_newline(File);
        io_close(File);
    }
}

bool CChillPw::AuthChatAccount(int Dummy)
{
    for (int i = 0; i < MAX_PASSWORDS; i++)
    {
        if(str_comp(g_Config.m_UiServerAddress, m_aaHostnames[i]))
            continue;
        if(Dummy != m_aDummy[i])
            continue;
        Console()->ExecuteLine(m_aaPasswords[i]);
        return true;
    }
    return false;
}
