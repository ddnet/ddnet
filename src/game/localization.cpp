/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "localization.h"

#include <base/log.h>

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

const char *Localize(const char *pStr, const char *pContext)
{
	const char *pNewStr = g_Localization.FindString(str_quickhash(pStr), str_quickhash(pContext));
	return pNewStr ? pNewStr : pStr;
}

void CLocalizationDatabase::LoadIndexfile(IStorage *pStorage, IConsole *pConsole)
{
	m_vLanguages.clear();

	const std::vector<std::string> vEnglishLanguageCodes = {"en"};
	m_vLanguages.emplace_back("English", "", 826, vEnglishLanguageCodes);

	const char *pFilename = "languages/index.txt";
	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		log_error("localization", "Couldn't open index file '%s'", pFilename);
		return;
	}

	while(const char *pLine = LineReader.Get())
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		char aEnglishName[128];
		str_copy(aEnglishName, pLine);

		pLine = LineReader.Get();
		if(!pLine)
		{
			log_error("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			log_error("localization", "Missing native name for language '%s'", aEnglishName);
			(void)LineReader.Get();
			(void)LineReader.Get();
			continue;
		}
		char aNativeName[128];
		str_copy(aNativeName, pLine + 3);

		pLine = LineReader.Get();
		if(!pLine)
		{
			log_error("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			log_error("localization", "Missing country code for language '%s'", aEnglishName);
			(void)LineReader.Get();
			continue;
		}
		char aCountryCode[128];
		str_copy(aCountryCode, pLine + 3);

		pLine = LineReader.Get();
		if(!pLine)
		{
			log_error("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			log_error("localization", "Missing language codes for language '%s'", aEnglishName);
			continue;
		}
		const char *pLanguageCodes = pLine + 3;
		char aLanguageCode[256];
		std::vector<std::string> vLanguageCodes;
		while((pLanguageCodes = str_next_token(pLanguageCodes, ";", aLanguageCode, sizeof(aLanguageCode))))
		{
			if(aLanguageCode[0])
			{
				vLanguageCodes.emplace_back(aLanguageCode);
			}
		}
		if(vLanguageCodes.empty())
		{
			log_error("localization", "At least one language code required for language '%s'", aEnglishName);
			continue;
		}

		char aFileName[IO_MAX_PATH_LENGTH];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aEnglishName);
		m_vLanguages.emplace_back(aNativeName, aFileName, str_toint(aCountryCode), vLanguageCodes);
	}

	std::sort(m_vLanguages.begin(), m_vLanguages.end());
}

void CLocalizationDatabase::SelectDefaultLanguage(IConsole *pConsole, char *pFilename, size_t Length) const
{
	if(Languages().empty())
		return;
	if(Languages().size() == 1)
	{
		str_copy(pFilename, Languages()[0].m_FileName.c_str(), Length);
		return;
	}

	char aLocaleStr[128];
	os_locale_str(aLocaleStr, sizeof(aLocaleStr));

	log_info("localization", "Choosing default language based on user locale '%s'", aLocaleStr);

	while(true)
	{
		const CLanguage *pPrefixMatch = nullptr;
		for(const auto &Language : Languages())
		{
			for(const auto &LanguageCode : Language.m_vLanguageCodes)
			{
				if(LanguageCode == aLocaleStr)
				{
					// Exact match found, use it immediately
					str_copy(pFilename, Language.m_FileName.c_str(), Length);
					return;
				}
				else if(LanguageCode.rfind(aLocaleStr, 0) == 0)
				{
					// Locale is prefix of language code, e.g. locale is "en" and current language is "en-US"
					pPrefixMatch = &Language;
				}
			}
		}
		// Use prefix match if no exact match was found
		if(pPrefixMatch)
		{
			str_copy(pFilename, pPrefixMatch->m_FileName.c_str(), Length);
			return;
		}

		// Remove last segment of locale string and try again with more generic locale, e.g. "en-US" -> "en"
		int i = str_length(aLocaleStr) - 1;
		for(; i >= 0; --i)
		{
			if(aLocaleStr[i] == '-')
			{
				aLocaleStr[i] = '\0';
				break;
			}
		}

		// Stop if no more locale segments are left
		if(i <= 0)
			break;
	}
}

bool CLocalizationDatabase::Load(const char *pFilename, IStorage *pStorage, IConsole *pConsole)
{
	// empty string means unload
	if(pFilename[0] == 0)
	{
		m_vStrings.clear();
		m_StringsHeap.Reset();
		return true;
	}

	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
		return false;

	log_info("localization", "loaded '%s'", pFilename);
	m_vStrings.clear();
	m_StringsHeap.Reset();

	char aContext[512];
	char aOrigin[512];
	int Line = 0;
	while(const char *pLine = LineReader.Get())
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
				log_error("localization", "malformed context '%s' on line %d", pLine, Line);
				continue;
			}
			str_truncate(aContext, sizeof(aContext), pLine + 1, Len - 2);
			pLine = LineReader.Get();
			if(!pLine)
			{
				log_error("localization", "unexpected end of file after context line '%s' on line %d", aContext, Line);
				break;
			}
			Line++;
		}
		else
		{
			aContext[0] = '\0';
		}

		str_copy(aOrigin, pLine);
		const char *pReplacement = LineReader.Get();
		if(!pReplacement)
		{
			log_error("localization", "unexpected end of file after original '%s' on line %d", aOrigin, Line);
			break;
		}
		Line++;

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			log_error("localization", "malformed replacement '%s' for original '%s' on line %d", pReplacement, aOrigin, Line);
			continue;
		}

		pReplacement += 3;
		AddString(aOrigin, pReplacement, aContext);
	}
	std::sort(m_vStrings.begin(), m_vStrings.end());
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
