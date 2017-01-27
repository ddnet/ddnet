#include "tutorialtext.h"
#include "gamecontext.h"
#include <engine/storage.h>
#include <engine/console.h>
#include <engine/shared/linereader.h>

TutorialText::TutorialText(IStorage *pStorage)
{
	m_pStorage = pStorage;

	LoadLanguageList();
	BuildAvailableLangsList();
}

void TutorialText::LoadLanguageList()
{
	IOHANDLE File = m_pStorage->OpenFile("tutorial_langs/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return;
	CLineReader LineReader;
	LineReader.Init(File);

	char *Line;
	while((Line = LineReader.Get()))
	{
		CLanguage Lang;
		str_copy(Lang.m_Name, Line, sizeof(Lang.m_Name));
		Lang.m_Loaded = false;
		m_Languages.add(Lang);
	}

	io_close(File);
}

void TutorialText::BuildAvailableLangsList()
{
	char *CurrentLine = new char[240]();
	unsigned int CurrentLength = 0;
	for(int i = 0; i < m_Languages.size(); i++)
	{
		unsigned int Length = str_length(m_Languages[i].m_Name);
		if(CurrentLength + Length + 1 >= 240)
		{
			m_AvailableLangsList.add(CurrentLine);
			CurrentLine = new char[240]();
			CurrentLength = 0;
		}
		else
		{
			if(CurrentLength)
			{
				str_append(CurrentLine, ", ", 240);
				CurrentLength += 2;
			}
			str_append(CurrentLine, m_Languages[i].m_Name, 240);
			CurrentLength += Length;
		}
	}
	if(CurrentLength)
		m_AvailableLangsList.add(CurrentLine);
}

void TutorialText::LoadLanguage(CLanguage *Lang)
{
	char FileName[sizeof(CLanguage::m_Name) + 32];
	str_format(FileName, sizeof(FileName), "tutorial_langs/%s.txt", Lang->m_Name);
	IOHANDLE File = m_pStorage->OpenFile(FileName, IOFLAG_READ, IStorage::TYPE_ALL);

	if(File)
	{
		CLineReader LineReader;
		LineReader.Init(File);

		char *Line;
		while((Line = LineReader.Get()))
		{
			char *Text = new char[1024];
			str_copy(Text, Line, 1024);
			Lang->m_Texts.add(Text);
		}
	}
	Lang->m_Loaded = true;
}

array<char *> *TutorialText::GetLanguage(const char *LangName)
{
	CLanguage CompareLang;
	str_copy(CompareLang.m_Name, LangName, sizeof(CompareLang.m_Name));

	sorted_array<CLanguage>::range Result = ::find_binary(m_Languages.all(), CompareLang);

	if(Result.empty())
		return NULL;

	CLanguage *Lang = &Result.front();
	if(!Lang->m_Loaded)
		LoadLanguage(Lang);

	return &Lang->m_Texts;
}

const char *TutorialText::GetText(int Id, array<char *> *Texts)
{
	if(Texts && Texts->size() > Id)
	{
		const char *Text = (*Texts)[Id];
		if(str_length(Text))
			return Text;
	}
	return NULL;
}
