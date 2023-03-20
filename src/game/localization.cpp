/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "localization.h"

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

void CLocalizationDatabase::LoadIndexfile(IStorage *pStorage, IConsole *pConsole)
{
	m_vLanguages.clear();
	m_vLanguages.emplace_back("English", "", 826);

	const char *pFilename = "languages/index.txt";
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
	if(!File)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "couldn't open index file '%s'", pFilename);
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
		return;
	}

	char aOrigin[128];
	char aReplacement[128];
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aOrigin, pLine);

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aReplacement, pLine + 3);

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		char aFileName[IO_MAX_PATH_LENGTH];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aOrigin);
		m_vLanguages.emplace_back(aReplacement, aFileName, str_toint(pLine + 3));
	}

	io_close(File);

	std::sort(m_vLanguages.begin(), m_vLanguages.end());
}

bool CLocalizationDatabase::Load(const char *pFilename, IStorage *pStorage, IConsole *pConsole)
{
	// empty string means unload
	if(pFilename[0] == 0)
	{
		m_vStrings.clear();
		m_StringsHeap.Reset();
		m_CurrentVersion = 0;
		return true;
	}

	IOHANDLE IoHandle = pStorage->OpenFile(pFilename, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
	if(!IoHandle)
		return false;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded '%s'", pFilename);
	pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
	m_vStrings.clear();
	m_StringsHeap.Reset();

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
	std::sort(m_vStrings.begin(), m_vStrings.end());

	m_CurrentVersion = ++m_VersionCounter;
	return true;
}

void CLocalizationDatabase::AddString(const char *pOrgStr, const char *pNewStr, const char *pContext)
{
	m_vStrings.emplace_back(str_quickhash(pOrgStr), str_quickhash(pContext), m_StringsHeap.StoreString(*pNewStr ? pNewStr : pOrgStr));
}

const char *CLocalizationDatabase::FindString(unsigned Hash, unsigned ContextHash) const
{
	CString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;
	String.m_pReplacement = 0x0;
	auto Range1 = std::equal_range(m_vStrings.begin(), m_vStrings.end(), String);
	if(std::distance(Range1.first, Range1.second) == 1)
		return Range1.first->m_pReplacement;

	const unsigned DefaultHash = str_quickhash("");
	if(ContextHash != DefaultHash)
	{
		// Do another lookup with the default context hash
		String.m_ContextHash = DefaultHash;
		auto Range2 = std::equal_range(m_vStrings.begin(), m_vStrings.end(), String);
		if(std::distance(Range2.first, Range2.second) == 1)
			return Range2.first->m_pReplacement;
	}

	return nullptr;
}

CLocalizationDatabase g_Localization;
