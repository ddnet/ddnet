/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include "compression.h"
#include "packer.h"

CAbstractPacker::CAbstractPacker(unsigned char *pBuffer, size_t Size) :
	m_pBuffer(pBuffer),
	m_BufferSize(Size)
{
}

void CAbstractPacker::Reset()
{
	m_Error = false;
	m_pCurrent = m_pBuffer;
	m_pEnd = m_pCurrent + m_BufferSize;
}

void CAbstractPacker::AddInt(int i)
{
	if(m_Error)
		return;

	unsigned char *pNext = CVariableInt::Pack(m_pCurrent, i, m_pEnd - m_pCurrent);
	if(!pNext)
	{
		m_Error = true;
		return;
	}
	m_pCurrent = pNext;
}

void CAbstractPacker::AddString(const char *pStr, int Limit, bool AllowTruncation)
{
	if(m_Error)
		return;

	unsigned char *const pPrevCurrent = m_pCurrent;
	if(Limit <= 0)
	{
		Limit = m_BufferSize;
	}
	while(*pStr)
	{
		int Codepoint = str_utf8_decode(&pStr);
		if(Codepoint == -1)
		{
			Codepoint = 0xfffd; // Unicode replacement character.
		}
		char aEncoded[4];
		const int Length = str_utf8_encode(aEncoded, Codepoint);
		// Limit must ensure space for null termination if desired.
		if(Limit < Length)
		{
			if(AllowTruncation)
			{
				break;
			}
			m_Error = true;
			m_pCurrent = pPrevCurrent;
			return;
		}
		// Ensure space for the null termination.
		if(m_pCurrent + Length + 1 > m_pEnd)
		{
			m_Error = true;
			m_pCurrent = pPrevCurrent;
			return;
		}
		mem_copy(m_pCurrent, aEncoded, Length);
		m_pCurrent += Length;
		Limit -= Length;
	}
	*m_pCurrent++ = '\0';
}

void CAbstractPacker::AddRaw(const void *pData, int Size)
{
	if(m_Error)
		return;

	if(m_pCurrent + Size > m_pEnd)
	{
		m_Error = true;
		return;
	}

	mem_copy(m_pCurrent, pData, Size);
	m_pCurrent += Size;
}

void CUnpacker::Reset(const void *pData, int Size)
{
	m_Error = false;
	m_pStart = (const unsigned char *)pData;
	m_pEnd = m_pStart + Size;
	m_pCurrent = m_pStart;
}

int CUnpacker::GetInt()
{
	if(m_Error)
		return 0;

	if(m_pCurrent >= m_pEnd)
	{
		m_Error = true;
		return 0;
	}

	int i;
	const unsigned char *pNext = CVariableInt::Unpack(m_pCurrent, &i, m_pEnd - m_pCurrent);
	if(!pNext)
	{
		m_Error = true;
		return 0;
	}
	m_pCurrent = pNext;
	return i;
}

int CUnpacker::GetIntOrDefault(int Default)
{
	if(m_Error)
	{
		return 0;
	}
	if(m_pCurrent == m_pEnd)
	{
		return Default;
	}
	return GetInt();
}

int CUnpacker::GetUncompressedInt()
{
	if(m_Error)
		return 0;

	if(m_pCurrent + sizeof(int) > m_pEnd)
	{
		m_Error = true;
		return 0;
	}

	int i;
	mem_copy(&i, m_pCurrent, sizeof(int));
	m_pCurrent += sizeof(int);
	return i;
}

int CUnpacker::GetUncompressedIntOrDefault(int Default)
{
	if(m_Error)
	{
		return 0;
	}
	if(m_pCurrent == m_pEnd)
	{
		return Default;
	}
	return GetUncompressedInt();
}

const char *CUnpacker::GetString(int SanitizeType)
{
	if(m_Error)
		return "";

	if(m_pCurrent >= m_pEnd)
	{
		m_Error = true;
		return "";
	}

	char *pPtr = (char *)m_pCurrent;
	while(*m_pCurrent) // skip the string
	{
		m_pCurrent++;
		if(m_pCurrent == m_pEnd)
		{
			m_Error = true;
			return "";
		}
	}
	m_pCurrent++;

	if(!str_utf8_check(pPtr))
	{
		m_Error = true;
		return "";
	}

	// sanitize all strings
	if(SanitizeType & SANITIZE)
		str_sanitize(pPtr);
	else if(SanitizeType & SANITIZE_CC)
		str_sanitize_cc(pPtr);
	return SanitizeType & SKIP_START_WHITESPACES ? str_utf8_skip_whitespaces(pPtr) : pPtr;
}

const unsigned char *CUnpacker::GetRaw(int Size)
{
	const unsigned char *pPtr = m_pCurrent;
	if(m_Error)
		return 0;

	// check for nasty sizes
	if(Size < 0 || m_pCurrent + Size > m_pEnd)
	{
		m_Error = true;
		return 0;
	}

	// "unpack" the data
	m_pCurrent += Size;
	return pPtr;
}
