#ifndef GAME_SERVER_TUTORIALTEXT_H
#define GAME_SERVER_TUTORIALTEXT_H

#include <base/tl/sorted_array.h>

class TutorialText
{
	class CLanguage
	{
	public:
		char m_Name[64];
		bool m_Loaded;
		array<char *> m_Texts;

		bool operator <(const CLanguage &Other) const { return str_comp(m_Name, Other.m_Name) < 0; }
		bool operator ==(const CLanguage &Other) const { return str_comp(m_Name, Other.m_Name) == 0; }
	};

	class IStorage *m_pStorage;
	sorted_array<CLanguage> m_Languages;
	array<char *> m_AvailableLangsList;

	void LoadLanguageList();
	void BuildAvailableLangsList();
	void LoadLanguage(CLanguage *Lang);

public:
	TutorialText(class IStorage *pStorage);
	array<char *> *GetAvailableLangsList() { return &m_AvailableLangsList; }
	array<char *> *GetLanguage(const char *LangName);
	const char *GetText(int Id, array<char *> *Texts);
};

#endif
