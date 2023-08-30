/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LOCALIZATION_H
#define GAME_LOCALIZATION_H

#include <base/system.h> // GNUC_ATTRIBUTE

#include <engine/shared/memheap.h>

#include <string>
#include <vector>

class CLanguage
{
public:
	CLanguage() = default;
	CLanguage(const char *pName, const char *pFileName, int Code, const std::vector<std::string> &vLanguageCodes) :
		m_Name(pName), m_FileName(pFileName), m_CountryCode(Code), m_vLanguageCodes(vLanguageCodes) {}

	std::string m_Name;
	std::string m_FileName;
	int m_CountryCode;
	std::vector<std::string> m_vLanguageCodes;

	bool operator<(const CLanguage &Other) const { return m_Name < Other.m_Name; }
};

class CLocalizationDatabase
{
	class CString
	{
	public:
		unsigned m_Hash;
		unsigned m_ContextHash;
		const char *m_pReplacement;

		CString() {}
		CString(unsigned Hash, unsigned ContextHash, const char *pReplacement) :
			m_Hash(Hash), m_ContextHash(ContextHash), m_pReplacement(pReplacement)
		{
		}

		bool operator<(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash < Other.m_ContextHash); }
		bool operator<=(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash <= Other.m_ContextHash); }
		bool operator==(const CString &Other) const { return m_Hash == Other.m_Hash && m_ContextHash == Other.m_ContextHash; }
	};

	std::vector<CLanguage> m_vLanguages;
	std::vector<CString> m_vStrings;
	CHeap m_StringsHeap;

public:
	void LoadIndexfile(class IStorage *pStorage, class IConsole *pConsole);
	const std::vector<CLanguage> &Languages() const { return m_vLanguages; }
	void SelectDefaultLanguage(class IConsole *pConsole, char *pFilename, size_t Length) const;

	bool Load(const char *pFilename, class IStorage *pStorage, class IConsole *pConsole);

	void AddString(const char *pOrgStr, const char *pNewStr, const char *pContext);
	const char *FindString(unsigned Hash, unsigned ContextHash) const;
};

extern CLocalizationDatabase g_Localization;

extern const char *Localize(const char *pStr, const char *pContext = "")
	GNUC_ATTRIBUTE((format_arg(1)));
#endif
