/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_LINEREADER_H
#define ENGINE_SHARED_LINEREADER_H
#include <base/system.h>

// buffered stream for reading lines, should perhaps be something smaller
class CLineReader
{
	char m_aBuffer[4 * 8192 + 1] = {0}; // 1 additional byte for null termination
	unsigned m_BufferPos = 0;
	unsigned m_BufferSize = 0;
	unsigned m_BufferMaxSize = 0;
	IOHANDLE m_File;

public:
	void Init(IOHANDLE File);
	char *Get();
};
#endif
