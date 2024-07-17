/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "linereader.h"

#include <base/system.h>

CLineReader::CLineReader()
{
	m_pBuffer = nullptr;
}

CLineReader::~CLineReader()
{
	free(m_pBuffer);
}

bool CLineReader::OpenFile(IOHANDLE File)
{
	if(!File)
	{
		return false;
	}
	char *pBuffer = io_read_all_str(File);
	io_close(File);
	if(pBuffer == nullptr)
	{
		return false;
	}
	OpenBuffer(pBuffer);
	return true;
}

void CLineReader::OpenBuffer(char *pBuffer)
{
	dbg_assert(pBuffer != nullptr, "Line reader initialized without valid buffer");

	m_pBuffer = pBuffer;
	m_BufferPos = 0;
	m_ReadLastLine = false;

	// Skip UTF-8 BOM
	if(m_pBuffer[0] == '\xEF' && m_pBuffer[1] == '\xBB' && m_pBuffer[2] == '\xBF')
	{
		m_BufferPos += 3;
	}
}

const char *CLineReader::Get()
{
	dbg_assert(m_pBuffer != nullptr, "Line reader not initialized");
	if(m_ReadLastLine)
	{
		return nullptr;
	}

	unsigned LineStart = m_BufferPos;
	while(true)
	{
		if(m_pBuffer[m_BufferPos] == '\0' || m_pBuffer[m_BufferPos] == '\n' || (m_pBuffer[m_BufferPos] == '\r' && m_pBuffer[m_BufferPos + 1] == '\n'))
		{
			if(m_pBuffer[m_BufferPos] == '\0')
			{
				m_ReadLastLine = true;
			}
			else
			{
				if(m_pBuffer[m_BufferPos] == '\r')
				{
					m_pBuffer[m_BufferPos] = '\0';
					++m_BufferPos;
				}
				m_pBuffer[m_BufferPos] = '\0';
				++m_BufferPos;
			}

			if(!str_utf8_check(&m_pBuffer[LineStart]))
			{
				// Skip lines containing invalid UTF-8
				if(m_ReadLastLine)
				{
					return nullptr;
				}
				LineStart = m_BufferPos;
				continue;
			}
			// Skip trailing empty line
			if(m_ReadLastLine && m_pBuffer[LineStart] == '\0')
			{
				return nullptr;
			}
			return &m_pBuffer[LineStart];
		}
		++m_BufferPos;
	}
}
