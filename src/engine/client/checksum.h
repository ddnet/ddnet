#ifndef ENGINE_CLIENT_CHECKSUM_H
#define ENGINE_CLIENT_CHECKSUM_H

#include <engine/shared/config.h>

struct CChecksumData
{
	int m_SizeofData;
	char m_aVersionStr[128];
	int m_Version;
	char m_aOsVersion[256];
	int64_t m_Start;
	int m_Random;
	int m_SizeofClient;
	int m_SizeofGameClient;
	float m_Zoom;
	int m_SizeofConfig;
	CConfig m_Config;
	int m_NumCommands;
	int m_aCommandsChecksum[1024];
	int m_NumComponents;
	int m_aComponentsChecksum[64];
	int m_NumFiles;
	int m_NumExtra;
	unsigned m_aFiles[1024];

	void InitFiles();
};

union CChecksum
{
	char m_aBytes[sizeof(CChecksumData)];
	CChecksumData m_Data;
};

#endif // ENGINE_CLIENT_CHECKSUM_H
