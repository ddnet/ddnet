#include <engine/storage.h>

#include <engine/shared/linereader.h>

#include "hmasterserver.h"

int CMasterInfo::GetEndpoint(char *aBuf, int BufSize, const char *pEndpoint) const
{
    return str_format(aBuf, BufSize, "%s/" HTTP_MASTER_VERSION "/%s", m_aUrl, pEndpoint);
}

void CHMasterServer::Init(IStorage *pStorage)
{
    dbg_msg("hmasterserver", "initializing");
    m_pStorage = pStorage;
}

bool CHMasterServer::SetDefaults()
{
    for(m_Count = 0; m_Count < MAX_MASTERSERVERS; m_Count++)
        str_format(m_aMasterServers[m_Count].m_aUrl, sizeof(m_aMasterServers[m_Count].m_aUrl), "https://master%d.ddnet.tw", m_Count + 1);

    return true;
}

bool CHMasterServer::Load()
{
    if(!m_pStorage)
        return false;

    IOHANDLE File = m_pStorage->OpenFile("hmasters.cfg", IOFLAG_READ, IStorage::TYPE_SAVE);
    if(!File)
        return SetDefaults();

    CLineReader Reader;
    Reader.Init(File);

    char *pLine = nullptr;
    for(m_Count = 0; m_Count < MAX_MASTERSERVERS && (pLine = Reader.Get()); m_Count++)
        str_copy(m_aMasterServers[m_Count].m_aUrl, pLine, sizeof(m_aMasterServers[m_Count].m_aUrl));

    io_close(File);
    return true;
}

bool CHMasterServer::Save()
{
    if(!m_pStorage)
        return false;

    IOHANDLE File = m_pStorage->OpenFile("hmasters.cfg", IOFLAG_WRITE, IStorage::TYPE_SAVE);
    if(!File)
        return false;

    for(int i = 0; i < m_Count; i++)
    {
        io_write(File, m_aMasterServers[i].m_aUrl, str_length(m_aMasterServers[i].m_aUrl));
        io_write_newline(File);
    }

    return true;
}

IHMasterServer *CreateHMasterServer() { return new CHMasterServer; }
