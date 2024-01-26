#include <base/color.h>
#include <base/system.h>

#include "teeinfo.h"

struct StdSkin
{
	char m_aSkinName[64];
	// body, marking, decoration, hands, feet, eyes
	char m_apSkinPartNames[6][24];
	bool m_aUseCustomColors[6];
	int m_aSkinPartColors[6];
};

static StdSkin g_aStdSkins[] = {
	{"default", {"standard", "", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {1798004, 0, 0, 1799582, 1869630, 0}},
	{"bluekitty", {"kitty", "whisker", "", "standard", "standard", "negative"}, {true, true, false, true, true, true}, {8681144, -8229413, 0, 7885547, 8868585, 9043712}},
	{"bluestripe", {"standard", "stripes", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {10187898, 0, 0, 750848, 1944919, 0}},
	{"brownbear", {"bear", "bear", "hair", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {1082745, -15634776, 0, 1082745, 1147174, 0}},
	{"cammo", {"standard", "cammo2", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {5334342, -11771603, 0, 750848, 1944919, 0}},
	{"cammostripes", {"standard", "cammostripes", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {5334342, -14840320, 0, 750848, 1944919, 0}},
	{"coala", {"koala", "twinbelly", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {184, -15397662, 0, 184, 9765959, 0}},
	{"limekitty", {"kitty", "whisker", "", "standard", "standard", "negative"}, {true, true, false, true, true, true}, {4612803, -12229920, 0, 3827951, 3827951, 8256000}},
	{"pinky", {"standard", "whisker", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {15911355, -801066, 0, 15043034, 15043034, 0}},
	{"redbopp", {"standard", "donny", "unibop", "standard", "standard", "standard"}, {true, true, true, true, true, false}, {16177260, -16590390, 16177260, 16177260, 7624169, 0}},
	{"redstripe", {"standard", "stripe", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {16307835, 0, 0, 184, 9765959, 0}},
	{"saddo", {"standard", "saddo", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {7171455, -9685436, 0, 3640746, 5792119, 0}},
	{"toptri", {"standard", "toptri", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {6119331, 0, 0, 3640746, 5792119, 0}},
	{"twinbop", {"standard", "duodonny", "twinbopp", "standard", "standard", "standard"}, {true, true, true, true, true, false}, {15310519, -1600806, 15310519, 15310519, 37600, 0}},
	{"twintri", {"standard", "twintri", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {3447932, -14098717, 0, 185, 9634888, 0}},
	{"warpaint", {"standard", "warpaint", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {1944919, 0, 0, 750337, 1944919, 0}}};

CTeeInfo::CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	str_copy(m_aSkinName, pSkinName, sizeof(m_aSkinName));
	m_UseCustomColor = UseCustomColor;
	m_ColorBody = ColorBody;
	m_ColorFeet = ColorFeet;
}

CTeeInfo::CTeeInfo(const char *apSkinPartNames[6], const int *pUseCustomColors, const int *pSkinPartColors)
{
	for(int i = 0; i < 6; i++)
	{
		str_copy(m_apSkinPartNames[i], apSkinPartNames[i], sizeof(m_apSkinPartNames[i]));
		m_aUseCustomColors[i] = pUseCustomColors[i];
		m_aSkinPartColors[i] = pSkinPartColors[i];
	}
}

void CTeeInfo::ToSixup()
{
	// reset to default skin
	for(int p = 0; p < 6; p++)
	{
		str_copy(m_apSkinPartNames[p], g_aStdSkins[0].m_apSkinPartNames[p], 24);
		m_aUseCustomColors[p] = g_aStdSkins[0].m_aUseCustomColors[p];
		m_aSkinPartColors[p] = g_aStdSkins[0].m_aSkinPartColors[p];
	}

	// check for std skin
	for(auto &StdSkin : g_aStdSkins)
	{
		if(!str_comp(m_aSkinName, StdSkin.m_aSkinName))
		{
			for(int p = 0; p < 6; p++)
			{
				str_copy(m_apSkinPartNames[p], StdSkin.m_apSkinPartNames[p], 24);
				m_aUseCustomColors[p] = StdSkin.m_aUseCustomColors[p];
				m_aSkinPartColors[p] = StdSkin.m_aSkinPartColors[p];
			}
			break;
		}
	}

	if(m_UseCustomColor)
	{
		int ColorBody = ColorHSLA(m_ColorBody).UnclampLighting().Pack(ms_DarkestLGT7);
		int ColorFeet = ColorHSLA(m_ColorFeet).UnclampLighting().Pack(ms_DarkestLGT7);
		m_aUseCustomColors[0] = true;
		m_aUseCustomColors[1] = true;
		m_aUseCustomColors[2] = true;
		m_aUseCustomColors[3] = true;
		m_aUseCustomColors[4] = true;
		m_aSkinPartColors[0] = ColorBody;
		m_aSkinPartColors[1] = 0x22FFFFFF;
		m_aSkinPartColors[2] = ColorBody;
		m_aSkinPartColors[3] = ColorBody;
		m_aSkinPartColors[4] = ColorFeet;
	}
}

void CTeeInfo::FromSixup()
{
	// reset to default skin
	str_copy(m_aSkinName, "default", sizeof(m_aSkinName));
	m_UseCustomColor = false;
	m_ColorBody = 0;
	m_ColorFeet = 0;

	// check for std skin
	for(auto &StdSkin : g_aStdSkins)
	{
		bool match = true;
		for(int p = 0; p < 6; p++)
		{
			if(str_comp(m_apSkinPartNames[p], StdSkin.m_apSkinPartNames[p]) || m_aUseCustomColors[p] != StdSkin.m_aUseCustomColors[p] || (m_aUseCustomColors[p] && m_aSkinPartColors[p] != StdSkin.m_aSkinPartColors[p]))
			{
				match = false;
				break;
			}
		}
		if(match)
		{
			str_copy(m_aSkinName, StdSkin.m_aSkinName, sizeof(m_aSkinName));
			return;
		}
	}

	// find closest match
	int best_skin = 0;
	int best_matches = -1;
	for(int s = 0; s < 16; s++)
	{
		int matches = 0;
		for(int p = 0; p < 3; p++)
			if(str_comp(m_apSkinPartNames[p], g_aStdSkins[s].m_apSkinPartNames[p]) == 0)
				matches++;

		if(matches > best_matches)
		{
			best_matches = matches;
			best_skin = s;
		}
	}

	str_copy(m_aSkinName, g_aStdSkins[best_skin].m_aSkinName, sizeof(m_aSkinName));
	m_UseCustomColor = true;
	m_ColorBody = ColorHSLA(m_aUseCustomColors[0] ? m_aSkinPartColors[0] : 255).UnclampLighting(ms_DarkestLGT7).Pack(ColorHSLA::DARKEST_LGT);
	m_ColorFeet = ColorHSLA(m_aUseCustomColors[4] ? m_aSkinPartColors[4] : 255).UnclampLighting(ms_DarkestLGT7).Pack(ColorHSLA::DARKEST_LGT);
}
