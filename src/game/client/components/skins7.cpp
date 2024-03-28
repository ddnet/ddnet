/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include "menus.h"
#include "skins7.h"

const char *const CSkins7::ms_apSkinPartNames[protocol7::NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"}; /* Localize("body","skins");Localize("marking","skins");Localize("decoration","skins");Localize("hands","skins");Localize("feet","skins");Localize("eyes","skins"); */
const char *const CSkins7::ms_apColorComponents[NUM_COLOR_COMPONENTS] = {"hue", "sat", "lgt", "alp"};

char *CSkins7::ms_apSkinVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};
int *CSkins7::ms_apUCCVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};
int *CSkins7::ms_apColorVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};

#define SKINS_DIR "skins7"

// TODO: uncomment
// const float MIN_EYE_BODY_COLOR_DIST = 80.f; // between body and eyes (LAB color space)

int CSkins7::SkinPartScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CSkins7 *pSelf = (CSkins7 *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	size_t PartNameSize, PartNameCount;
	str_utf8_stats(pName, str_length(pName) - str_length(".png") + 1, IO_MAX_PATH_LENGTH, &PartNameSize, &PartNameCount);
	if(PartNameSize >= protocol7::MAX_SKIN_ARRAY_SIZE || PartNameCount > protocol7::MAX_SKIN_LENGTH)
	{
		char aBuf[IO_MAX_PATH_LENGTH + 64];
		str_format(aBuf, sizeof(aBuf), "failed to load skin part '%s': name too long", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skins", aBuf);
		return 0;
	}

	CSkinPart Part;
	str_copy(Part.m_aName, pName, minimum<int>(PartNameSize + 1, sizeof(Part.m_aName)));
	if(pSelf->FindSkinPart(pSelf->m_ScanningPart, Part.m_aName, true) != -1)
		return 0;

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s/%s", CSkins7::ms_apSkinPartNames[pSelf->m_ScanningPart], pName);
	CImageInfo Info;
	if(!pSelf->Graphics()->LoadPNG(&Info, aBuf, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load skin part '%s'", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skins", aBuf);
		return 0;
	}
	if(Info.m_Format != CImageInfo::FORMAT_RGBA)
	{
		str_format(aBuf, sizeof(aBuf), "failed to load skin part '%s': must be RGBA format", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skins", aBuf);
		return 0;
	}

	Part.m_OrgTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	// Part.m_OrgTexture = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_BODY]);
	// &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL_LEFT]
	Part.m_BloodColor = vec3(1.0f, 1.0f, 1.0f);

	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;
	unsigned char *pData = (unsigned char *)Info.m_pData;

	// dig out blood color
	if(pSelf->m_ScanningPart == protocol7::SKINPART_BODY)
	{
		int Pitch = Info.m_Width * Step;
		int PartX = Info.m_Width / 2;
		int PartY = 0;
		int PartWidth = Info.m_Width / 2;
		int PartHeight = Info.m_Height / 2;

		int aColors[3] = {0};
		for(int y = PartY; y < PartY + PartHeight; y++)
			for(int x = PartX; x < PartX + PartWidth; x++)
				if(pData[y * Pitch + x * Step + 3] > 128)
					for(int c = 0; c < 3; c++)
						aColors[c] += pData[y * Pitch + x * Step + c];

		Part.m_BloodColor = normalize(vec3(aColors[0], aColors[1], aColors[2]));
	}

	// create colorless version
	for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
	{
		const int Average = (pData[i * Step] + pData[i * Step + 1] + pData[i * Step + 2]) / 3;
		pData[i * Step] = Average;
		pData[i * Step + 1] = Average;
		pData[i * Step + 2] = Average;
	}

	Part.m_ColorTexture = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	free(Info.m_pData);

	// set skin part data
	Part.m_Flags = 0;
	if(pName[0] == 'x' && pName[1] == '_')
		Part.m_Flags |= SKINFLAG_SPECIAL;
	if(DirType != IStorage::TYPE_SAVE)
		Part.m_Flags |= SKINFLAG_STANDARD;
	if(pSelf->Config()->m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "load skin part %s", Part.m_aName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skins", aBuf);
	}
	pSelf->m_aaSkinParts[pSelf->m_ScanningPart].emplace_back(Part);

	return 0;
}

int CSkins7::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(IsDir || !str_endswith(pName, ".json"))
		return 0;

	CSkins7 *pSelf = (CSkins7 *)pUser;

	// read file data into buffer
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s", pName);
	IOHANDLE File = pSelf->Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return 0;
	int FileSize = (int)io_length(File);
	char *pFileData = (char *)malloc(FileSize);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// init
	CSkin Skin = pSelf->m_DummySkin;
	int NameLength = str_length(pName);
	str_copy(Skin.m_aName, pName, 1 + NameLength - str_length(".json"));
	if(pSelf->Find(Skin.m_aName, true) != -1)
		return 0;
	bool SpecialSkin = pName[0] == 'x' && pName[1] == '_';

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	free(pFileData);

	if(pJsonData == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, aBuf, aError);
		return 0;
	}

	// extract data
	const json_value &rStart = (*pJsonData)["skin"];
	if(rStart.type == json_object)
	{
		for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; ++PartIndex)
		{
			const json_value &rPart = rStart[(const char *)ms_apSkinPartNames[PartIndex]];
			if(rPart.type != json_object)
				continue;

			// filename
			const json_value &rFilename = rPart["filename"];
			if(rFilename.type == json_string)
			{
				int SkinPart = pSelf->FindSkinPart(PartIndex, (const char *)rFilename, SpecialSkin);
				if(SkinPart > -1)
					Skin.m_apParts[PartIndex] = pSelf->GetSkinPart(PartIndex, SkinPart);
			}

			// use custom colors
			bool UseCustomColors = false;
			const json_value &rColor = rPart["custom_colors"];
			if(rColor.type == json_string)
				UseCustomColors = str_comp((const char *)rColor, "true") == 0;
			else if(rColor.type == json_boolean)
				UseCustomColors = rColor.u.boolean;
			Skin.m_aUseCustomColors[PartIndex] = UseCustomColors;

			// color components
			if(!UseCustomColors)
				continue;

			for(int i = 0; i < NUM_COLOR_COMPONENTS; i++)
			{
				if(PartIndex != protocol7::SKINPART_MARKING && i == 3)
					continue;

				const json_value &rComponent = rPart[(const char *)ms_apColorComponents[i]];
				if(rComponent.type == json_integer)
				{
					switch(i)
					{
					case 0: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFF00FFFF) | (rComponent.u.integer << 16); break;
					case 1: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFFFF00FF) | (rComponent.u.integer << 8); break;
					case 2: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFFFFFF00) | rComponent.u.integer; break;
					case 3: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0x00FFFFFF) | (rComponent.u.integer << 24); break;
					}
				}
			}
		}
	}

	// clean up
	json_value_free(pJsonData);

	// set skin data
	Skin.m_Flags = SpecialSkin ? SKINFLAG_SPECIAL : 0;
	if(DirType != IStorage::TYPE_SAVE)
		Skin.m_Flags |= SKINFLAG_STANDARD;
	if(pSelf->Config()->m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "load skin %s", Skin.m_aName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skins", aBuf);
	}
	pSelf->m_aSkins.insert(std::lower_bound(pSelf->m_aSkins.begin(), pSelf->m_aSkins.end(), Skin), Skin);

	return 0;
}

int CSkins7::GetInitAmount() const
{
	return protocol7::NUM_SKINPARTS * 5 + 8;
}

void CSkins7::OnInit()
{
	int Dummy = 0;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_BODY] = Config()->m_ClPlayer7SkinBody;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_MARKING] = Config()->m_ClPlayer7SkinMarking;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_DECORATION] = Config()->m_ClPlayer7SkinDecoration;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_HANDS] = Config()->m_ClPlayer7SkinHands;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_FEET] = Config()->m_ClPlayer7SkinFeet;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_EYES] = Config()->m_ClPlayer7SkinEyes;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_BODY] = &Config()->m_ClPlayer7UseCustomColorBody;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_MARKING] = &Config()->m_ClPlayer7UseCustomColorMarking;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_DECORATION] = &Config()->m_ClPlayer7UseCustomColorDecoration;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_HANDS] = &Config()->m_ClPlayer7UseCustomColorHands;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_FEET] = &Config()->m_ClPlayer7UseCustomColorFeet;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_EYES] = &Config()->m_ClPlayer7UseCustomColorEyes;
	ms_apColorVariables[Dummy][protocol7::SKINPART_BODY] = (int *)&Config()->m_ClPlayer7ColorBody;
	ms_apColorVariables[Dummy][protocol7::SKINPART_MARKING] = (int *)&Config()->m_ClPlayer7ColorMarking;
	ms_apColorVariables[Dummy][protocol7::SKINPART_DECORATION] = (int *)&Config()->m_ClPlayer7ColorDecoration;
	ms_apColorVariables[Dummy][protocol7::SKINPART_HANDS] = (int *)&Config()->m_ClPlayer7ColorHands;
	ms_apColorVariables[Dummy][protocol7::SKINPART_FEET] = (int *)&Config()->m_ClPlayer7ColorFeet;
	ms_apColorVariables[Dummy][protocol7::SKINPART_EYES] = (int *)&Config()->m_ClPlayer7ColorEyes;

	Dummy = 1;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_BODY] = Config()->m_ClDummy7SkinBody;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_MARKING] = Config()->m_ClDummy7SkinMarking;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_DECORATION] = Config()->m_ClDummy7SkinDecoration;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_HANDS] = Config()->m_ClDummy7SkinHands;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_FEET] = Config()->m_ClDummy7SkinFeet;
	ms_apSkinVariables[Dummy][protocol7::SKINPART_EYES] = Config()->m_ClDummy7SkinEyes;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_BODY] = &Config()->m_ClDummy7UseCustomColorBody;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_MARKING] = &Config()->m_ClDummy7UseCustomColorMarking;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_DECORATION] = &Config()->m_ClDummy7UseCustomColorDecoration;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_HANDS] = &Config()->m_ClDummy7UseCustomColorHands;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_FEET] = &Config()->m_ClDummy7UseCustomColorFeet;
	ms_apUCCVariables[Dummy][protocol7::SKINPART_EYES] = &Config()->m_ClDummy7UseCustomColorEyes;
	ms_apColorVariables[Dummy][protocol7::SKINPART_BODY] = (int *)&Config()->m_ClDummy7ColorBody;
	ms_apColorVariables[Dummy][protocol7::SKINPART_MARKING] = (int *)&Config()->m_ClDummy7ColorMarking;
	ms_apColorVariables[Dummy][protocol7::SKINPART_DECORATION] = (int *)&Config()->m_ClDummy7ColorDecoration;
	ms_apColorVariables[Dummy][protocol7::SKINPART_HANDS] = (int *)&Config()->m_ClDummy7ColorHands;
	ms_apColorVariables[Dummy][protocol7::SKINPART_FEET] = (int *)&Config()->m_ClDummy7ColorFeet;
	ms_apColorVariables[Dummy][protocol7::SKINPART_EYES] = (int *)&Config()->m_ClDummy7ColorEyes;

	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		m_aaSkinParts[p].clear();

		// add none part
		if(p == protocol7::SKINPART_MARKING || p == protocol7::SKINPART_DECORATION)
		{
			CSkinPart NoneSkinPart;
			NoneSkinPart.m_Flags = SKINFLAG_STANDARD;
			NoneSkinPart.m_aName[0] = '\0';
			NoneSkinPart.m_BloodColor = vec3(1.0f, 1.0f, 1.0f);
			m_aaSkinParts[p].emplace_back(NoneSkinPart);
		}

		// load skin parts
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s", ms_apSkinPartNames[p]);
		m_ScanningPart = p;
		Storage()->ListDirectory(IStorage::TYPE_ALL, aBuf, SkinPartScan, this);

		// add dummy skin part
		if(m_aaSkinParts[p].empty())
		{
			CSkinPart DummySkinPart;
			DummySkinPart.m_Flags = SKINFLAG_STANDARD;
			str_copy(DummySkinPart.m_aName, "dummy");
			DummySkinPart.m_BloodColor = vec3(1.0f, 1.0f, 1.0f);
			m_aaSkinParts[p].emplace_back(DummySkinPart);
		}

		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
	}

	// create dummy skin
	m_DummySkin.m_Flags = SKINFLAG_STANDARD;
	str_copy(m_DummySkin.m_aName, "dummy");
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		int Default;
		if(p == protocol7::SKINPART_MARKING || p == protocol7::SKINPART_DECORATION)
			Default = FindSkinPart(p, "", false);
		else
			Default = FindSkinPart(p, "standard", false);
		if(Default < 0)
			Default = 0;
		m_DummySkin.m_apParts[p] = GetSkinPart(p, Default);
		m_DummySkin.m_aPartColors[p] = p == protocol7::SKINPART_MARKING ? (255 << 24) + 65408 : 65408;
		m_DummySkin.m_aUseCustomColors[p] = 0;
	}
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);

	// load skins
	m_aSkins.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, SKINS_DIR, SkinScan, this);
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);

	// add dummy skin
	if(m_aSkins.empty())
		m_aSkins.emplace_back(m_DummySkin);

	{
		// add xmas hat
		const char *pFileName = SKINS_DIR "/xmas_hat.png";
		CImageInfo Info;
		if(!Graphics()->LoadPNG(&Info, pFileName, IStorage::TYPE_ALL) || Info.m_Width != 128 || Info.m_Height != 512)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "failed to load xmas hat '%s'", pFileName);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "loaded xmas hat '%s'", pFileName);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
			m_XmasHatTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
			free(Info.m_pData);
		}
	}
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);

	{
		// add bot decoration
		const char *pFileName = SKINS_DIR "/bot.png";
		CImageInfo Info;
		if(!Graphics()->LoadPNG(&Info, pFileName, IStorage::TYPE_ALL) || Info.m_Width != 384 || Info.m_Height != 160)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "failed to load bot '%s'", pFileName);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "loaded bot '%s'", pFileName);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
			m_BotTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
			free(Info.m_pData);
		}
	}
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
}

void CSkins7::AddSkin(const char *pSkinName, int Dummy)
{
	CSkin Skin = m_DummySkin;
	Skin.m_Flags = 0;
	str_copy(Skin.m_aName, pSkinName, sizeof(Skin.m_aName));
	for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; ++PartIndex)
	{
		int SkinPart = FindSkinPart(PartIndex, ms_apSkinVariables[Dummy][PartIndex], false);
		if(SkinPart > -1)
			Skin.m_apParts[PartIndex] = GetSkinPart(PartIndex, SkinPart);
		Skin.m_aUseCustomColors[PartIndex] = *ms_apUCCVariables[Dummy][PartIndex];
		Skin.m_aPartColors[PartIndex] = *ms_apColorVariables[Dummy][PartIndex];
	}
	int SkinIndex = Find(Skin.m_aName, false);
	if(SkinIndex != -1)
		m_aSkins[SkinIndex] = Skin;
	else
		m_aSkins.emplace_back(Skin);
}

void CSkins7::RemoveSkin(const CSkin *pSkin)
{
	auto Position = std::find(m_aSkins.begin(), m_aSkins.end(), *pSkin);
	m_aSkins.erase(Position);
}

int CSkins7::Num()
{
	return m_aSkins.size();
}

int CSkins7::NumSkinPart(int Part)
{
	return m_aaSkinParts[Part].size();
}

const CSkins7::CSkin *CSkins7::Get(int Index)
{
	return &m_aSkins[maximum((std::size_t)0, Index % m_aSkins.size())];
}

int CSkins7::Find(const char *pName, bool AllowSpecialSkin)
{
	for(unsigned int i = 0; i < m_aSkins.size(); i++)
	{
		if(str_comp(m_aSkins[i].m_aName, pName) == 0 && ((m_aSkins[i].m_Flags & SKINFLAG_SPECIAL) == 0 || AllowSpecialSkin))
			return i;
	}
	return -1;
}

const CSkins7::CSkinPart *CSkins7::GetSkinPart(int Part, int Index)
{
	int Size = m_aaSkinParts[Part].size();
	return &m_aaSkinParts[Part][maximum(0, Index % Size)];
}

int CSkins7::FindSkinPart(int Part, const char *pName, bool AllowSpecialPart)
{
	for(unsigned int i = 0; i < m_aaSkinParts[Part].size(); i++)
	{
		if(str_comp(m_aaSkinParts[Part][i].m_aName, pName) == 0 && ((m_aaSkinParts[Part][i].m_Flags & SKINFLAG_SPECIAL) == 0 || AllowSpecialPart))
			return i;
	}
	return -1;
}

void CSkins7::RandomizeSkin(int Dummy)
{
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		int Hue = rand() % 255;
		int Sat = rand() % 255;
		int Lgt = rand() % 255;
		int Alp = 0;
		if(p == protocol7::SKINPART_MARKING)
			Alp = rand() % 255;
		int ColorVariable = (Alp << 24) | (Hue << 16) | (Sat << 8) | Lgt;
		*CSkins7::ms_apUCCVariables[Dummy][p] = true;
		*CSkins7::ms_apColorVariables[Dummy][p] = ColorVariable;
	}

	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		const CSkins7::CSkinPart *s = GetSkinPart(p, rand() % NumSkinPart(p));
		while(s->m_Flags & CSkins7::SKINFLAG_SPECIAL)
			s = GetSkinPart(p, rand() % NumSkinPart(p));
		mem_copy(CSkins7::ms_apSkinVariables[Dummy][p], s->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
	}
}

vec3 CSkins7::GetColorV3(int v) const
{
	float Dark = DARKEST_COLOR_LGT / 255.0f;
	ColorRGBA color = color_cast<ColorRGBA>(ColorHSLA(v).UnclampLighting(Dark));
	return vec3(color.r, color.g, color.b);
}

vec4 CSkins7::GetColorV4(int v, bool UseAlpha) const
{
	vec3 r = GetColorV3(v);
	float Alpha = UseAlpha ? ((v >> 24) & 0xff) / 255.0f : 1.0f;
	return vec4(r.r, r.g, r.b, Alpha);
}

int CSkins7::GetTeamColor(int UseCustomColors, int PartColor, int Team, int Part) const
{
	static const int s_aTeamColors[3] = {0xC4C34E, 0x00FF6B, 0x9BFF6B};

	int TeamHue = (s_aTeamColors[Team + 1] >> 16) & 0xff;
	int TeamSat = (s_aTeamColors[Team + 1] >> 8) & 0xff;
	int TeamLgt = s_aTeamColors[Team + 1] & 0xff;
	int PartSat = (PartColor >> 8) & 0xff;
	int PartLgt = PartColor & 0xff;

	if(!UseCustomColors)
	{
		PartSat = 255;
		PartLgt = 255;
	}

	int MinSat = 160;
	int MaxSat = 255;

	int h = TeamHue;
	int s = clamp(mix(TeamSat, PartSat, 0.2), MinSat, MaxSat);
	int l = clamp(mix(TeamLgt, PartLgt, 0.2), (int)DARKEST_COLOR_LGT, 200);

	int ColorVal = (h << 16) + (s << 8) + l;
	if(Part == protocol7::SKINPART_MARKING) // keep alpha
		ColorVal += PartColor & 0xff000000;

	return ColorVal;
}

bool CSkins7::ValidateSkinParts(char *apPartNames[protocol7::NUM_SKINPARTS], int *pUseCustomColors, int *pPartColors, int GameFlags) const
{
	// force standard (black) eyes on team skins
	if(GameFlags & GAMEFLAG_TEAMS)
	{
		// TODO: adjust eye color here as well?
		if(str_comp(apPartNames[protocol7::SKINPART_EYES], "colorable") == 0 || str_comp(apPartNames[protocol7::SKINPART_EYES], "negative") == 0)
		{
			str_copy(apPartNames[protocol7::SKINPART_EYES], "standard", protocol7::MAX_SKIN_ARRAY_SIZE);
			return false;
		}
	}
	return true;
}

bool CSkins7::SaveSkinfile(const char *pSaveSkinName, int Dummy)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "skins/%s.json", pSaveSkinName);
	IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	CJsonFileWriter Writer(File);

	Writer.BeginObject();
	Writer.WriteAttribute("skin");
	Writer.BeginObject();
	for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; PartIndex++)
	{
		if(!ms_apSkinVariables[Dummy][PartIndex][0])
			continue;

		// part start
		Writer.WriteAttribute(ms_apSkinPartNames[PartIndex]);
		Writer.BeginObject();
		{
			Writer.WriteAttribute("filename");
			Writer.WriteStrValue(ms_apSkinVariables[Dummy][PartIndex]);

			const bool CustomColors = *ms_apUCCVariables[PartIndex];
			Writer.WriteAttribute("custom_colors");
			Writer.WriteBoolValue(CustomColors);

			if(CustomColors)
			{
				for(int c = 0; c < NUM_COLOR_COMPONENTS - 1; c++)
				{
					int Val = (*ms_apColorVariables[Dummy][PartIndex] >> (2 - c) * 8) & 0xff;
					Writer.WriteAttribute(ms_apColorComponents[c]);
					Writer.WriteIntValue(Val);
				}
				if(PartIndex == protocol7::SKINPART_MARKING)
				{
					int Val = (*ms_apColorVariables[Dummy][PartIndex] >> 24) & 0xff;
					Writer.WriteAttribute(ms_apColorComponents[3]);
					Writer.WriteIntValue(Val);
				}
			}
		}
		Writer.EndObject();
	}
	Writer.EndObject();
	Writer.EndObject();

	// add new skin to the skin list
	AddSkin(pSaveSkinName, Dummy);
	return true;
}
