/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "localization.h"
#include <base/tl/algorithm.h>

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

const char *Localize(const char *pStr, const char *pContext)
{
	const char *pNewStr = g_Localization.FindString(str_quickhash(pStr), str_quickhash(pContext));
	return pNewStr ? pNewStr : pStr;
}

CLocConstString::CLocConstString(const char *pStr, const char *pContext)
{
	m_pDefaultStr = pStr;
	m_Hash = str_quickhash(m_pDefaultStr);
	m_Version = -1;
}

void CLocConstString::Reload()
{
	m_Version = g_Localization.Version();
	const char *pNewStr = g_Localization.FindString(m_Hash, m_ContextHash);
	m_pCurrentStr = pNewStr;
	if(!m_pCurrentStr)
		m_pCurrentStr = m_pDefaultStr;
}

CLocalizationDatabase::CLocalizationDatabase()
{
	m_VersionCounter = 0;
	m_CurrentVersion = 0;
}

void CLocalizationDatabase::AddString(const char *pOrgStr, const char *pNewStr, const char *pContext)
{
	CString s;
	s.m_Hash = str_quickhash(pOrgStr);
	s.m_ContextHash = str_quickhash(pContext);
	s.m_Replacement = *pNewStr ? pNewStr : pOrgStr;
	m_Strings.add(s);
}

bool CLocalizationDatabase::Load(const char *pFilename, IStorage *pStorage, IConsole *pConsole)
{
	// empty string means unload
	if(pFilename[0] == 0)
	{
		m_Strings.clear();
		m_CurrentVersion = 0;
		return true;
	}

	IOHANDLE IoHandle = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!IoHandle)
		return false;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded '%s'", pFilename);
	pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
	m_Strings.clear();

	char aContext[512];
	char aOrigin[512];
	CLineReader LineReader;
	LineReader.Init(IoHandle);
	char *pLine;
	int Line = 0;
	while((pLine = LineReader.Get()))
	{
		Line++;
		if(!str_length(pLine))
			continue;

		if(pLine[0] == '#') // skip comments
			continue;

		if(pLine[0] == '[') // context
		{
			size_t Len = str_length(pLine);
			if(Len < 1 || pLine[Len - 1] != ']')
			{
				str_format(aBuf, sizeof(aBuf), "malform context line (%d): %s", Line, pLine);
				pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
				continue;
			}
			str_copy(aContext, pLine + 1, Len - 1);
			pLine = LineReader.Get();
		}
		else
		{
			aContext[0] = '\0';
		}

		str_copy(aOrigin, pLine, sizeof(aOrigin));
		char *pReplacement = LineReader.Get();
		Line++;
		if(!pReplacement)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of file");
			break;
		}

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			str_format(aBuf, sizeof(aBuf), "malform replacement line (%d) for '%s'", Line, aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		pReplacement += 3;
		AddString(aOrigin, pReplacement, aContext);
	}
	io_close(IoHandle);

	m_CurrentVersion = ++m_VersionCounter;
	return true;
}

const char *CLocalizationDatabase::FindString(unsigned Hash, unsigned ContextHash)
{
	CString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;
	sorted_array<CString>::range r = ::find_binary(m_Strings.all(), String);
	if(r.empty())
		return 0;

	unsigned DefaultHash = str_quickhash("");
	unsigned DefaultIndex = 0;
	for(unsigned i = 0; i < r.size() && r.index(i).m_Hash == Hash; ++i)
	{
		const CString &rStr = r.index(i);
		if(rStr.m_ContextHash == ContextHash)
			return rStr.m_Replacement;
		else if(rStr.m_ContextHash == DefaultHash)
			DefaultIndex = i;
	}

	return r.index(DefaultIndex).m_Replacement;
}

CLocalizationDatabase g_Localization;
