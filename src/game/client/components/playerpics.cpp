/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

// nobo copy of countryflags.cpp
#include "playerpics.h"

void CPlayerPics::LoadImageByName(const char * pImgName)
{
	char aBuf[128];
	CImageInfo Info;
	if(g_Config.m_ClLoadCountryFlags)
	{
		str_format(aBuf, sizeof(aBuf), "playerpics/%s.png", pImgName);
		if(!Graphics()->LoadPNG(&Info, aBuf, IStorage::TYPE_ALL))
		{
			char aMsg[128];
			str_format(aMsg, sizeof(aMsg), "failed to load '%s'", aBuf);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
			return;
		}
		else
		{
			char aMsg[128];
			str_format(aMsg, sizeof(aMsg), "SUCCESS loading '%s'", aBuf);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
		}
	}

	// add entry
	CPlayerPic CountryFlag;
	str_copy(CountryFlag.m_aPlayerName, pImgName, sizeof(CountryFlag.m_aPlayerName));
	if(g_Config.m_ClLoadCountryFlags)
	{
		CountryFlag.m_Texture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
		free(Info.m_pData);
	}
	else
		CountryFlag.m_Texture = -1;
	if(g_Config.m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "loaded player pic '%s'", pImgName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aBuf);
	}
	m_aPlayerPics.add_unsorted(CountryFlag);
}

void CPlayerPics::LoadCountryflagsIndexfile()
{
	// load the graphic files
	LoadImageByName("ChillerDragon");
	LoadImageByName("jao");

	m_aPlayerPics.sort_range();
}

void CPlayerPics::OnInit()
{
	// load country flags
	m_aPlayerPics.clear();
	LoadCountryflagsIndexfile();
	if(!m_aPlayerPics.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "playerpics", "failed to load country flags. folder='playerpics/'");
		CPlayerPic DummyEntry;
		DummyEntry.m_Texture = -1;
		mem_zero(DummyEntry.m_aPlayerName, sizeof(DummyEntry.m_aPlayerName));
		m_aPlayerPics.add(DummyEntry);
	}
}

int CPlayerPics::Num() const
{
	return m_aPlayerPics.size();
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByName(const char * pName) const
{
	for(int i = 0; i < m_aPlayerPics.size(); i++)
	{
		if(str_comp(m_aPlayerPics[i].m_aPlayerName, pName) == 0)
		{
			return GetByIndex(i);
		}
	}
	return NULL;
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByIndex(int Index) const
{
	return &m_aPlayerPics[max(0, Index%m_aPlayerPics.size())];
}

void CPlayerPics::Render(const char * pName, const vec4 *pColor, float x, float y, float w, float h)
{
	const CPlayerPic *pFlag = GetByName(pName);
	if (!pFlag)
		return;

	if(pFlag->m_Texture != -1)
	{
		Graphics()->TextureSet(pFlag->m_Texture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(pColor->r, pColor->g, pColor->b, pColor->a);
		IGraphics::CQuadItem QuadItem(x, y, w, h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
}
