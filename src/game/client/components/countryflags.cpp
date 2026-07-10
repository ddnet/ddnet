/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "countryflags.h"

#include <base/dbg.h>
#include <base/io.h>
#include <base/log.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

bool CCountryFlags::CCountryFlag::operator<(const CCountryFlag &Other) const
{
	return str_comp(m_aCountryCodeString, Other.m_aCountryCodeString) < 0;
}

bool CCountryFlags::ValidateCountryCodeString(const char *pString)
{
	const size_t Length = str_length(pString);
	if(Length >= sizeof(CCountryFlag{}.m_aCountryCodeString))
	{
		log_error("countryflags", "Country name '%s' is too long (max. %" PRIzu " characters)", pString, sizeof(CCountryFlag{}.m_aCountryCodeString) - 1);
		return false;
	}

	for(size_t Index = 0; Index < Length; ++Index)
	{
		if(pString[Index] != '-' &&
			!(pString[Index] >= 'a' && pString[Index] <= 'z') &&
			!(pString[Index] >= 'A' && pString[Index] <= 'Z'))
		{
			log_error("countryflags", "Country name '%s' must only contain letters and dash characters", pString);
			return false;
		}
	}

	return true;
}

void CCountryFlags::LoadCountryflagsIndexfile()
{
	m_vCountryFlags.clear();

	const char *pFilename = "countryflags/index.txt";
	CLineReader LineReader;
	if(LineReader.OpenFile(Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		while(const char *pLine = LineReader.Get())
		{
			if(pLine[0] == '\0' || pLine[0] == '#') // Skip empty lines and comments
			{
				continue;
			}

			if(!ValidateCountryCodeString(pLine))
			{
				continue;
			}
			CCountryFlag CountryFlag;
			str_copy(CountryFlag.m_aCountryCodeString, pLine);

			const char *pCountryCodeLine = LineReader.Get();
			if(!pCountryCodeLine)
			{
				log_error("countryflags", "Unexpected end of index file after country '%s'", CountryFlag.m_aCountryCodeString);
				break;
			}

			if(!str_startswith(pCountryCodeLine, "== "))
			{
				log_error("countryflags", "Malformed country code for country '%s'", CountryFlag.m_aCountryCodeString);
				continue;
			}
			pCountryCodeLine += str_length("== ");

			if(!str_toint(pCountryCodeLine, &CountryFlag.m_CountryCode))
			{
				log_error("countryflags", "Country code '%s' for country '%s' is not a number", pCountryCodeLine, CountryFlag.m_aCountryCodeString);
				continue;
			}
			if(!in_range(CountryFlag.m_CountryCode, CountryCode::MINIMUM, CountryCode::MAXIMUM))
			{
				log_error("countryflags", "Country code '%d' for country '%s' is not within valid code range [%d..%d]", CountryFlag.m_CountryCode, CountryFlag.m_aCountryCodeString, CountryCode::MINIMUM, CountryCode::MAXIMUM);
				continue;
			}
			if(CountryFlag.m_CountryCode == CountryCode::DEFAULT && str_comp(CountryFlag.m_aCountryCodeString, "default") != 0)
			{
				log_error("countryflags", "Country code '%d' for country '%s' is only allowed for the default country", CountryFlag.m_CountryCode, CountryFlag.m_aCountryCodeString);
				continue;
			}

			char aFlagPath[IO_MAX_PATH_LENGTH];
			CImageInfo ImageInfo;
			str_format(aFlagPath, sizeof(aFlagPath), "countryflags/%s.png", CountryFlag.m_aCountryCodeString);
			if(!Graphics()->LoadPng(ImageInfo, aFlagPath, IStorage::TYPE_ALL))
			{
				log_error("countryflags", "Failed to load country flag from '%s'", aFlagPath);
				continue;
			}

			CountryFlag.m_Texture = Graphics()->LoadTextureRawMove(ImageInfo, 0, aFlagPath);
			if(g_Config.m_Debug)
			{
				log_trace("countryflags", "Loaded country flag '%s'", CountryFlag.m_aCountryCodeString);
			}
			m_vCountryFlags.push_back(CountryFlag);
		}
	}
	else
	{
		log_error("countryflags", "Failed to open country flags index file '%s'", pFilename);
	}

	// Ensure a default flag exists
	auto ExistingDefaultFlag = std::find_if(m_vCountryFlags.begin(), m_vCountryFlags.end(), [](const CCountryFlag &Flag) {
		return Flag.m_CountryCode == CountryCode::DEFAULT;
	});
	if(ExistingDefaultFlag == m_vCountryFlags.end())
	{
		CCountryFlag DefaultFlag;
		DefaultFlag.m_CountryCode = CountryCode::DEFAULT;
		str_copy(DefaultFlag.m_aCountryCodeString, "default");
		m_vCountryFlags.push_back(DefaultFlag);
	}

	std::sort(m_vCountryFlags.begin(), m_vCountryFlags.end());

	size_t DefaultIndex = 0;
	for(size_t Index = 0; Index < m_vCountryFlags.size(); ++Index)
	{
		if(m_vCountryFlags[Index].m_CountryCode == CountryCode::DEFAULT)
		{
			DefaultIndex = Index;
			break;
		}
	}

	std::fill(std::begin(m_aCountryCodeToIndexTable), std::end(m_aCountryCodeToIndexTable), DefaultIndex);
	for(size_t i = 0; i < m_vCountryFlags.size(); ++i)
	{
		m_aCountryCodeToIndexTable[m_vCountryFlags[i].m_CountryCode - CountryCode::MINIMUM] = i;
	}

	log_debug("countryflags", "Loaded %" PRIzu " country flags", m_vCountryFlags.size());
}

void CCountryFlags::OnInit()
{
	LoadCountryflagsIndexfile();

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
	dbg_assert(CountryCode >= CountryCode::MINIMUM && CountryCode <= CountryCode::MAXIMUM, "Invalid CountryCode: %d", CountryCode);
	return GetByIndex(m_aCountryCodeToIndexTable[CountryCode - CountryCode::MINIMUM]);
}

const CCountryFlags::CCountryFlag &CCountryFlags::GetByIndex(size_t Index) const
{
	dbg_assert(Index < m_vCountryFlags.size(), "Invalid Index: %" PRIzu, Index);
	return m_vCountryFlags[Index];
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
