#ifndef ENGINE_CLIENT_CHECKSUM_H
#define ENGINE_CLIENT_CHECKSUM_H

#include <engine/shared/config.h>

struct CChecksumData
{
	int m_SizeofData = 0;
	char m_aVersionStr[128] = {0};
	int m_Version = 0;
	char m_aOsVersion[256] = {0};
	int64_t m_Start = 0;
	int m_Random = 0;
	int m_SizeofClient = 0;
	int m_SizeofGameClient = 0;
	float m_Zoom = 0;
	int m_SizeofConfig = 0;
	CConfig m_Config;
	int m_NumCommands = 0;
	int m_aCommandsChecksum[1024] = {0};
	int m_NumComponents = 0;
	int m_aComponentsChecksum[64] = {0};
	int m_NumFiles = 0;
	int m_NumExtra = 0;
	unsigned m_aFiles[1024] = {0};

	void InitFiles();
};

union CChecksum
{
	CChecksumData m_Data;
	char m_aBytes[sizeof(CChecksumData)] /* = {0} */;

    CChecksum()
    {
        mem_zero(m_aBytes, sizeof(CChecksum));
    }
};

#endif // ENGINE_CLIENT_CHECKSUM_H
