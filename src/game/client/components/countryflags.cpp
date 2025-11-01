/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "countryflags.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

void CCountryFlags::LoadCountryflagsIndexfile()
{
	const char *pFilename = "countryflags/index.txt";
	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		log_error("countryflags", "couldn't open index file '%s'", pFilename);
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
			log_error("countryflags", "unexpected end of index file");
			break;
		}

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			log_error("countryflags", "malformed replacement for index '%s'", aOrigin);
			continue;
		}

		int CountryCode = str_toint(pReplacement + 3);
		if(CountryCode < CODE_LB || CountryCode > CODE_UB)
		{
			log_error("countryflags", "country code '%i' not within valid code range [%i..%i]", CountryCode, CODE_LB, CODE_UB);
			continue;
		}

		// load the graphic file
		char aBuf[128];
		CImageInfo Info;
		str_format(aBuf, sizeof(aBuf), "countryflags/%s.png", aOrigin);
		if(!Graphics()->LoadPng(Info, aBuf, IStorage::TYPE_ALL))
		{
			log_error("countryflags", "failed to load '%s'", aBuf);
			continue;
		}

		// add entry
		CCountryFlag CountryFlag;
		CountryFlag.m_CountryCode = CountryCode;
		str_copy(CountryFlag.m_aCountryCodeString, aOrigin);
		CountryFlag.m_Texture = Graphics()->LoadTextureRawMove(Info, 0, aBuf);

		if(g_Config.m_Debug)
		{
			log_debug("countryflags", "loaded country flag '%s'", aOrigin);
		}
		m_vCountryFlags.push_back(CountryFlag);
	}

	std::sort(m_vCountryFlags.begin(), m_vCountryFlags.end());

	// find index of default item
	size_t DefaultIndex = 0;
	for(size_t Index = 0; Index < m_vCountryFlags.size(); ++Index)
		if(m_vCountryFlags[Index].m_CountryCode == -1)
		{
			DefaultIndex = Index;
			break;
		}

	// init LUT
	std::fill(std::begin(m_aCodeIndexLUT), std::end(m_aCodeIndexLUT), DefaultIndex);
	for(size_t i = 0; i < m_vCountryFlags.size(); ++i)
		m_aCodeIndexLUT[maximum(0, (m_vCountryFlags[i].m_CountryCode - CODE_LB) % CODE_RANGE)] = i;
}

void CCountryFlags::OnInit()
{
	// load country flags
	m_vCountryFlags.clear();
	LoadCountryflagsIndexfile();
	if(m_vCountryFlags.empty())
	{
		log_error("countryflags", "failed to load country flags. folder='countryflags/'");
		CCountryFlag DummyEntry;
		DummyEntry.m_CountryCode = -1;
		DummyEntry.m_aCountryCodeString[0] = '\0';
		m_vCountryFlags.push_back(DummyEntry);
	}

	m_FlagsQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->QuadContainerAddSprite(m_FlagsQuadContainerIndex, 0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->QuadContainerUpload(m_FlagsQuadContainerIndex);
}

size_t CCountryFlags::Num() const
{
	return m_vCountryFlags.size();
}

const CCountryFlags::CCountryFlag &CCountryFlags::GetByCountryCode(int CountryCode) const
{
	return GetByIndex(m_aCodeIndexLUT[maximum(0, (CountryCode - CODE_LB) % CODE_RANGE)]);
}

const CCountryFlags::CCountryFlag &CCountryFlags::GetByIndex(size_t Index) const
{
	return m_vCountryFlags[Index % m_vCountryFlags.size()];
}

void CCountryFlags::Render(const CCountryFlag &Flag, ColorRGBA Color, float x, float y, float w, float h)
{
	if(Flag.m_Texture.IsValid())
	{
		Graphics()->TextureSet(Flag.m_Texture);
		Graphics()->SetColor(Color);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->RenderQuadContainerEx(m_FlagsQuadContainerIndex, 0, -1, x, y, w, h);
	}
}

void CCountryFlags::Render(int CountryCode, ColorRGBA Color, float x, float y, float w, float h)
{
	Render(GetByCountryCode(CountryCode), Color, x, y, w, h);
}
