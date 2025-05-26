/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include "countryflags.h"

#include <game/client/render.h>

void CCountryFlags::LoadCountryflagsIndexfile()
{
	const char *pFilename = "countryflags/index.txt";
	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "couldn't open index file '%s'", pFilename);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", aBuf);
		return;
	}

	char aOrigin[128];
	while(const char *pLine = LineReader.Get())
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aOrigin, pLine);
		const char *pReplacement = LineReader.Get();
		if(!pReplacement)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", "unexpected end of index file");
			break;
		}

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", aBuf);
			continue;
		}

		int CountryCode;
		if(!str_toint(pReplacement + 3, &CountryCode))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "country code '%s' not a valid int", pReplacement + 3);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", aBuf);
			continue;
		}

		// load the graphic file
		char aBuf[128];
		CImageInfo Info;
		str_format(aBuf, sizeof(aBuf), "countryflags/%s.png", aOrigin);
		if(!Graphics()->LoadPng(Info, aBuf, IStorage::TYPE_ALL))
		{
			char aMsg[128];
			str_format(aMsg, sizeof(aMsg), "failed to load '%s'", aBuf);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", aMsg);
			continue;
		}

		// add entry
		CCountryFlag CountryFlag;
		CountryFlag.m_CountryCode = CountryCode;
		str_copy(CountryFlag.m_aCountryCodeString, aOrigin);
		CountryFlag.m_Texture = Graphics()->LoadTextureRawMove(Info, 0, aBuf);

		if(g_Config.m_Debug)
		{
			str_format(aBuf, sizeof(aBuf), "loaded country flag '%s'", aOrigin);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "countryflags", aBuf);
		}
		m_CountryFlags[CountryCode] = CountryFlag;
	}
}

void CCountryFlags::OnInit()
{
	// load country flags
	m_CountryFlags.clear();
	LoadCountryflagsIndexfile();
	if(m_CountryFlags.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "countryflags", "failed to load country flags. folder='countryflags/'");
		CCountryFlag DummyEntry;
		DummyEntry.m_CountryCode = -1;
		mem_zero(DummyEntry.m_aCountryCodeString, sizeof(DummyEntry.m_aCountryCodeString));
		m_CountryFlags[DummyEntry.m_CountryCode] = DummyEntry;
	}

	m_FlagsQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_FlagsQuadContainerIndex, 0, 0, 1, 1);
	Graphics()->QuadContainerUpload(m_FlagsQuadContainerIndex);
}

const std::unordered_map<int, CCountryFlags::CCountryFlag> &CCountryFlags::CountryFlags() const
{
	return m_CountryFlags;
}

void CCountryFlags::Render(const CCountryFlag *pFlag, ColorRGBA Color, float x, float y, float w, float h)
{
	if(pFlag->m_Texture.IsValid())
	{
		Graphics()->TextureSet(pFlag->m_Texture);
		Graphics()->SetColor(Color);
		Graphics()->RenderQuadContainerEx(m_FlagsQuadContainerIndex, 0, -1, x, y, w, h);
	}
}

void CCountryFlags::Render(int CountryCode, ColorRGBA Color, float x, float y, float w, float h)
{
	auto It = m_CountryFlags.find(CountryCode);
	if(It == m_CountryFlags.end())
		It = m_CountryFlags.find(-1);
	Render(&It->second, Color, x, y, w, h);
}
