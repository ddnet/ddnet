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


void CPlayerPics::LoadCountryflagsIndexfile()
{
	// data
	char aOrigin[128];
	int CountryCode = 1;
	str_format(aOrigin, sizeof(aOrigin), "%d", CountryCode);

	// load the graphic files

	// chiller
	{
		char aBuf[128];
		CImageInfo Info;
		if(g_Config.m_ClLoadCountryFlags)
		{
			str_format(aBuf, sizeof(aBuf), "playerpics/ChillerDragon.png");
			if(!Graphics()->LoadPNG(&Info, aBuf, IStorage::TYPE_ALL))
			{
				char aMsg[128];
				str_format(aMsg, sizeof(aMsg), "failed to load '%s'", aBuf);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
				return;
			}
			else
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", "SUCCESSFULLY LOADED ChillerDragon pic");
		}

		// add entry
		CPlayerPic CountryFlag;
		CountryFlag.m_CountryCode = 1;
		str_copy(CountryFlag.m_aCountryCodeString, aOrigin, sizeof(CountryFlag.m_aCountryCodeString));
		if(g_Config.m_ClLoadCountryFlags)
		{
			CountryFlag.m_Texture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
			free(Info.m_pData);
		}
		else
			CountryFlag.m_Texture = -1;
		if(g_Config.m_Debug)
		{
			str_format(aBuf, sizeof(aBuf), "loaded country flag '%s'", aOrigin);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aBuf);
		}
		m_aCountryFlags.add_unsorted(CountryFlag);
	}

	// jao
	{
		char aBuf[128];
		CImageInfo Info;
		if(g_Config.m_ClLoadCountryFlags)
		{
			str_format(aBuf, sizeof(aBuf), "playerpics/jao.png");
			if(!Graphics()->LoadPNG(&Info, aBuf, IStorage::TYPE_ALL))
			{
				char aMsg[128];
				str_format(aMsg, sizeof(aMsg), "failed to load '%s'", aBuf);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aMsg);
				return;
			}
			else
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", "SUCCESSFULLY LOADED jao pic");
		}

		// add entry
		CPlayerPic CountryFlag;
		CountryFlag.m_CountryCode = 2;
		str_copy(CountryFlag.m_aCountryCodeString, aOrigin, sizeof(CountryFlag.m_aCountryCodeString));
		if(g_Config.m_ClLoadCountryFlags)
		{
			CountryFlag.m_Texture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
			free(Info.m_pData);
		}
		else
			CountryFlag.m_Texture = -1;
		if(g_Config.m_Debug)
		{
			str_format(aBuf, sizeof(aBuf), "loaded country flag '%s'", aOrigin);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "playerpics", aBuf);
		}
		m_aCountryFlags.add_unsorted(CountryFlag);
	}


	m_aCountryFlags.sort_range();

	// find index of default item
	int DefaultIndex = 0, Index = 0;
	for(sorted_array<CPlayerPic>::range r = m_aCountryFlags.all(); !r.empty(); r.pop_front(), ++Index)
		if(r.front().m_CountryCode == -1)
		{
			DefaultIndex = Index;
			return;
		}

	// init LUT
	if(DefaultIndex != 0)
		for(int i = 0; i < CODE_RANGE; ++i)
			m_CodeIndexLUT[i] = DefaultIndex;
	else
		mem_zero(m_CodeIndexLUT, sizeof(m_CodeIndexLUT));
	for(int i = 0; i < m_aCountryFlags.size(); ++i)
		m_CodeIndexLUT[max(0, (m_aCountryFlags[i].m_CountryCode-CODE_LB)%CODE_RANGE)] = i;
}

void CPlayerPics::OnInit()
{
	// load country flags
	m_aCountryFlags.clear();
	LoadCountryflagsIndexfile();
	if(!m_aCountryFlags.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "playerpics", "failed to load country flags. folder='playerpics/'");
		CPlayerPic DummyEntry;
		DummyEntry.m_CountryCode = -1;
		DummyEntry.m_Texture = -1;
		mem_zero(DummyEntry.m_aCountryCodeString, sizeof(DummyEntry.m_aCountryCodeString));
		m_aCountryFlags.add(DummyEntry);
	}
}

int CPlayerPics::Num() const
{
	return m_aCountryFlags.size();
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByCountryCode(int CountryCode) const
{
	return GetByIndex(m_CodeIndexLUT[max(0, (CountryCode-CODE_LB)%CODE_RANGE)]);
}

const CPlayerPics::CPlayerPic *CPlayerPics::GetByIndex(int Index) const
{
	return &m_aCountryFlags[max(0, Index%m_aCountryFlags.size())];
}

void CPlayerPics::Render(int CountryCode, const vec4 *pColor, float x, float y, float w, float h)
{
	const CPlayerPic *pFlag = GetByCountryCode(CountryCode);
	if(pFlag->m_Texture != -1)
	{
		Graphics()->TextureSet(pFlag->m_Texture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(pColor->r, pColor->g, pColor->b, pColor->a);
		IGraphics::CQuadItem QuadItem(x, y, w, h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	else
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x, y, 10.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = w;
		TextRender()->TextEx(&Cursor, pFlag->m_aCountryCodeString, -1);
	}
}
