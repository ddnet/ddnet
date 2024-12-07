/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include "menus.h"
#include "skins7.h"

const char *const CSkins7::ms_apSkinPartNames[protocol7::NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"};
const char *const CSkins7::ms_apSkinPartNamesLocalized[protocol7::NUM_SKINPARTS] = {Localizable("Body", "skins"), Localizable("Marking", "skins"), Localizable("Decoration", "skins"), Localizable("Hands", "skins"), Localizable("Feet", "skins"), Localizable("Eyes", "skins")};
const char *const CSkins7::ms_apColorComponents[NUM_COLOR_COMPONENTS] = {"hue", "sat", "lgt", "alp"};

char *CSkins7::ms_apSkinNameVariables[NUM_DUMMIES] = {0};
char *CSkins7::ms_apSkinVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};
int *CSkins7::ms_apUCCVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};
int unsigned *CSkins7::ms_apColorVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{0}};

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
		log_error("skins7", "Failed to load skin part '%s': name too long", pName);
		return 0;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), SKINS_DIR "/%s/%s", CSkins7::ms_apSkinPartNames[pSelf->m_ScanningPart], pName);
	CImageInfo Info;
	if(!pSelf->Graphics()->LoadPng(Info, aFilename, DirType))
	{
		log_error("skins7", "Failed to load skin part '%s'", pName);
		return 0;
	}
	if(!pSelf->Graphics()->IsImageFormatRgba(aFilename, Info))
	{
		log_error("skins7", "Failed to load skin part '%s': must be RGBA format", pName);
		Info.Free();
		return 0;
	}

	CSkinPart Part;
	str_copy(Part.m_aName, pName, minimum<int>(PartNameSize + 1, sizeof(Part.m_aName)));
	Part.m_OrgTexture = pSelf->Graphics()->LoadTextureRaw(Info, 0, aFilename);
	Part.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f);

	const int Step = 4;
	unsigned char *pData = (unsigned char *)Info.m_pData;

	// dig out blood color
	if(pSelf->m_ScanningPart == protocol7::SKINPART_BODY)
	{
		int Pitch = Info.m_Width * Step;
		int PartX = Info.m_Width / 2;
		int PartY = 0;
		int PartWidth = Info.m_Width / 2;
		int PartHeight = Info.m_Height / 2;

		int64_t aColors[3] = {0};
		for(int y = PartY; y < PartY + PartHeight; y++)
			for(int x = PartX; x < PartX + PartWidth; x++)
				if(pData[y * Pitch + x * Step + 3] > 128)
					for(int c = 0; c < 3; c++)
						aColors[c] += pData[y * Pitch + x * Step + c];

		Part.m_BloodColor = ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
	}

	ConvertToGrayscale(Info);

	Part.m_ColorTexture = pSelf->Graphics()->LoadTextureRawMove(Info, 0, aFilename);

	// set skin part data
	Part.m_Flags = 0;
	if(pName[0] == 'x' && pName[1] == '_')
		Part.m_Flags |= SKINFLAG_SPECIAL;
	if(DirType != IStorage::TYPE_SAVE)
		Part.m_Flags |= SKINFLAG_STANDARD;

	if(pSelf->Config()->m_Debug)
	{
		log_trace("skins7", "Loaded skin part '%s'", Part.m_aName);
	}
	pSelf->m_avSkinParts[pSelf->m_ScanningPart].emplace_back(Part);

	return 0;
}

int CSkins7::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(IsDir || !str_endswith(pName, ".json"))
		return 0;

	CSkins7 *pSelf = (CSkins7 *)pUser;

	// read file data into buffer
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), SKINS_DIR "/%s", pName);
	void *pFileData;
	unsigned JsonFileSize;
	if(!pSelf->Storage()->ReadFile(aFilename, IStorage::TYPE_ALL, &pFileData, &JsonFileSize))
	{
		return 0;
	}

	// init
	CSkin Skin;
	str_copy(Skin.m_aName, pName, 1 + str_length(pName) - str_length(".json"));
	const bool SpecialSkin = pName[0] == 'x' && pName[1] == '_';
	Skin.m_Flags = SpecialSkin ? SKINFLAG_SPECIAL : 0;
	if(DirType != IStorage::TYPE_SAVE)
		Skin.m_Flags |= SKINFLAG_STANDARD;

	// parse json data
	json_settings JsonSettings{};
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, static_cast<const json_char *>(pFileData), JsonFileSize, aError);
	free(pFileData);

	if(pJsonData == nullptr)
	{
		log_error("skins7", "Failed to parse skin json file '%s': %s", aFilename, aError);
		return 0;
	}

	// extract data
	const json_value &Start = (*pJsonData)["skin"];
	if(Start.type != json_object)
	{
		log_error("skins7", "Failed to parse skin json file '%s': root must be an object", aFilename);
		json_value_free(pJsonData);
		return 0;
	}

	for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; ++PartIndex)
	{
		Skin.m_aUseCustomColors[PartIndex] = 0;
		Skin.m_aPartColors[PartIndex] = (PartIndex == protocol7::SKINPART_MARKING ? 0xFF000000u : 0u) + 0x00FF80u;

		const json_value &Part = Start[(const char *)ms_apSkinPartNames[PartIndex]];
		if(Part.type == json_none)
		{
			Skin.m_apParts[PartIndex] = pSelf->FindDefaultSkinPart(PartIndex);
			continue;
		}
		if(Part.type != json_object)
		{
			log_error("skins7", "Failed to parse skin json file '%s': attribute '%s' must specify an object", aFilename, ms_apSkinPartNames[PartIndex]);
			json_value_free(pJsonData);
			return 0;
		}

		// filename
		const json_value &Filename = Part["filename"];
		if(Filename.type == json_string)
		{
			Skin.m_apParts[PartIndex] = pSelf->FindSkinPart(PartIndex, (const char *)Filename, SpecialSkin);
		}
		else
		{
			log_error("skins7", "Failed to parse skin json file '%s': part '%s' attribute 'filename' must specify a string", aFilename, ms_apSkinPartNames[PartIndex]);
			json_value_free(pJsonData);
			return 0;
		}

		// use custom colors
		bool UseCustomColors = false;
		const json_value &Color = Part["custom_colors"];
		if(Color.type == json_string)
			UseCustomColors = str_comp((const char *)Color, "true") == 0;
		else if(Color.type == json_boolean)
			UseCustomColors = Color.u.boolean;
		Skin.m_aUseCustomColors[PartIndex] = UseCustomColors;

		// color components
		if(!UseCustomColors)
			continue;

		for(int i = 0; i < NUM_COLOR_COMPONENTS; i++)
		{
			if(PartIndex != protocol7::SKINPART_MARKING && i == 3)
				continue;

			const json_value &Component = Part[(const char *)ms_apColorComponents[i]];
			if(Component.type == json_integer)
			{
				switch(i)
				{
				case 0: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFF00FFFFu) | (Component.u.integer << 16); break;
				case 1: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFFFF00FFu) | (Component.u.integer << 8); break;
				case 2: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0xFFFFFF00u) | Component.u.integer; break;
				case 3: Skin.m_aPartColors[PartIndex] = (Skin.m_aPartColors[PartIndex] & 0x00FFFFFFu) | (Component.u.integer << 24); break;
				}
			}
		}
	}

	json_value_free(pJsonData);

	if(pSelf->Config()->m_Debug)
	{
		log_trace("skins7", "Loaded skin '%s'", Skin.m_aName);
	}
	pSelf->m_vSkins.insert(std::lower_bound(pSelf->m_vSkins.begin(), pSelf->m_vSkins.end(), Skin), Skin);

	return 0;
}

void CSkins7::OnInit()
{
	int Dummy = 0;
	ms_apSkinNameVariables[Dummy] = Config()->m_ClPlayer7Skin;
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
	ms_apColorVariables[Dummy][protocol7::SKINPART_BODY] = &Config()->m_ClPlayer7ColorBody;
	ms_apColorVariables[Dummy][protocol7::SKINPART_MARKING] = &Config()->m_ClPlayer7ColorMarking;
	ms_apColorVariables[Dummy][protocol7::SKINPART_DECORATION] = &Config()->m_ClPlayer7ColorDecoration;
	ms_apColorVariables[Dummy][protocol7::SKINPART_HANDS] = &Config()->m_ClPlayer7ColorHands;
	ms_apColorVariables[Dummy][protocol7::SKINPART_FEET] = &Config()->m_ClPlayer7ColorFeet;
	ms_apColorVariables[Dummy][protocol7::SKINPART_EYES] = &Config()->m_ClPlayer7ColorEyes;

	Dummy = 1;
	ms_apSkinNameVariables[Dummy] = Config()->m_ClDummy7Skin;
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
	ms_apColorVariables[Dummy][protocol7::SKINPART_BODY] = &Config()->m_ClDummy7ColorBody;
	ms_apColorVariables[Dummy][protocol7::SKINPART_MARKING] = &Config()->m_ClDummy7ColorMarking;
	ms_apColorVariables[Dummy][protocol7::SKINPART_DECORATION] = &Config()->m_ClDummy7ColorDecoration;
	ms_apColorVariables[Dummy][protocol7::SKINPART_HANDS] = &Config()->m_ClDummy7ColorHands;
	ms_apColorVariables[Dummy][protocol7::SKINPART_FEET] = &Config()->m_ClDummy7ColorFeet;
	ms_apColorVariables[Dummy][protocol7::SKINPART_EYES] = &Config()->m_ClDummy7ColorEyes;

	InitPlaceholderSkinParts();

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		m_avSkinParts[Part].clear();

		// add none part
		if(Part == protocol7::SKINPART_MARKING || Part == protocol7::SKINPART_DECORATION)
		{
			CSkinPart NoneSkinPart;
			NoneSkinPart.m_Flags = SKINFLAG_STANDARD;
			NoneSkinPart.m_aName[0] = '\0';
			NoneSkinPart.m_BloodColor = vec3(1.0f, 1.0f, 1.0f);
			m_avSkinParts[Part].emplace_back(NoneSkinPart);
		}

		// load skin parts
		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s", ms_apSkinPartNames[Part]);
		m_ScanningPart = Part;
		Storage()->ListDirectory(IStorage::TYPE_ALL, aBuf, SkinPartScan, this);

		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
	}

	// load skins
	m_vSkins.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, SKINS_DIR, SkinScan, this);
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);

	LoadXmasHat();
	LoadBotDecoration();
	GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);

	m_LastRefreshTime = time_get_nanoseconds();
}

void CSkins7::InitPlaceholderSkinParts()
{
	for(CSkinPart &SkinPart : m_aPlaceholderSkinParts)
	{
		SkinPart.m_Flags = SKINFLAG_STANDARD;
		str_copy(SkinPart.m_aName, "dummy");
		SkinPart.m_OrgTexture.Invalidate();
		SkinPart.m_ColorTexture.Invalidate();
		SkinPart.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CSkins7::LoadXmasHat()
{
	const char *pFilename = SKINS_DIR "/xmas_hat.png";
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, pFilename, IStorage::TYPE_ALL) ||
		!Graphics()->IsImageFormatRgba(pFilename, Info) ||
		!Graphics()->CheckImageDivisibility(pFilename, Info, 1, 4, false))
	{
		log_error("skins7", "Failed to xmas hat '%s'", pFilename);
		Info.Free();
	}
	else
	{
		if(Config()->m_Debug)
		{
			log_trace("skins7", "Loaded xmas hat '%s'", pFilename);
		}
		m_XmasHatTexture = Graphics()->LoadTextureRawMove(Info, 0, pFilename);
	}
}

void CSkins7::LoadBotDecoration()
{
	const char *pFilename = SKINS_DIR "/bot.png";
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, pFilename, IStorage::TYPE_ALL) ||
		!Graphics()->IsImageFormatRgba(pFilename, Info) ||
		!Graphics()->CheckImageDivisibility(pFilename, Info, 12, 5, false))
	{
		log_error("skins7", "Failed to load bot '%s'", pFilename);
		Info.Free();
	}
	else
	{
		if(Config()->m_Debug)
		{
			log_trace("skins7", "Loaded bot '%s'", pFilename);
		}
		m_BotTexture = Graphics()->LoadTextureRawMove(Info, 0, pFilename);
	}
}

void CSkins7::AddSkinFromConfigVariables(const char *pName, int Dummy)
{
	auto OldSkin = std::find_if(m_vSkins.begin(), m_vSkins.end(), [pName](const CSkin &Skin) {
		return str_comp(Skin.m_aName, pName) == 0;
	});
	if(OldSkin != m_vSkins.end())
	{
		m_vSkins.erase(OldSkin);
	}

	CSkin NewSkin;
	NewSkin.m_Flags = 0;
	str_copy(NewSkin.m_aName, pName);
	for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; ++PartIndex)
	{
		NewSkin.m_apParts[PartIndex] = FindSkinPart(PartIndex, ms_apSkinVariables[Dummy][PartIndex], false);
		NewSkin.m_aUseCustomColors[PartIndex] = *ms_apUCCVariables[Dummy][PartIndex];
		NewSkin.m_aPartColors[PartIndex] = *ms_apColorVariables[Dummy][PartIndex];
	}
	m_vSkins.insert(std::lower_bound(m_vSkins.begin(), m_vSkins.end(), NewSkin), NewSkin);
}

bool CSkins7::RemoveSkin(const CSkin *pSkin)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s.json", pSkin->m_aName);
	if(!Storage()->RemoveFile(aBuf, IStorage::TYPE_SAVE))
	{
		return false;
	}

	auto FoundSkin = std::find(m_vSkins.begin(), m_vSkins.end(), *pSkin);
	dbg_assert(FoundSkin != m_vSkins.end(), "Skin not found");
	m_vSkins.erase(FoundSkin);
	return true;
}

const std::vector<CSkins7::CSkin> &CSkins7::GetSkins() const
{
	return m_vSkins;
}

const std::vector<CSkins7::CSkinPart> &CSkins7::GetSkinParts(int Part) const
{
	return m_avSkinParts[Part];
}

const CSkins7::CSkinPart *CSkins7::FindSkinPartOrNullptr(int Part, const char *pName, bool AllowSpecialPart) const
{
	auto FoundPart = std::find_if(m_avSkinParts[Part].begin(), m_avSkinParts[Part].end(), [pName](const CSkinPart &SkinPart) {
		return str_comp(SkinPart.m_aName, pName) == 0;
	});
	if(FoundPart == m_avSkinParts[Part].end())
	{
		return nullptr;
	}
	if((FoundPart->m_Flags & SKINFLAG_SPECIAL) != 0 && !AllowSpecialPart)
	{
		return nullptr;
	}
	return &*FoundPart;
}

const CSkins7::CSkinPart *CSkins7::FindDefaultSkinPart(int Part) const
{
	const char *pDefaultPartName = Part == protocol7::SKINPART_MARKING || Part == protocol7::SKINPART_DECORATION ? "" : "default";
	const CSkinPart *pDefault = FindSkinPartOrNullptr(Part, pDefaultPartName, false);
	if(pDefault != nullptr)
	{
		return pDefault;
	}
	return &m_aPlaceholderSkinParts[Part];
}

const CSkins7::CSkinPart *CSkins7::FindSkinPart(int Part, const char *pName, bool AllowSpecialPart) const
{
	const CSkinPart *pSkinPart = FindSkinPartOrNullptr(Part, pName, AllowSpecialPart);
	if(pSkinPart != nullptr)
	{
		return pSkinPart;
	}
	return FindDefaultSkinPart(Part);
}

void CSkins7::RandomizeSkin(int Dummy) const
{
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		int Hue = rand() % 255;
		int Sat = rand() % 255;
		int Lgt = rand() % 255;
		int Alp = 0;
		if(Part == protocol7::SKINPART_MARKING)
			Alp = rand() % 255;
		int ColorVariable = (Alp << 24) | (Hue << 16) | (Sat << 8) | Lgt;
		*ms_apUCCVariables[Dummy][Part] = true;
		*ms_apColorVariables[Dummy][Part] = ColorVariable;
	}

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		std::vector<const CSkins7::CSkinPart *> vConsideredSkinParts;
		for(const CSkinPart &SkinPart : GetSkinParts(Part))
		{
			if((SkinPart.m_Flags & CSkins7::SKINFLAG_SPECIAL) != 0)
				continue;
			vConsideredSkinParts.push_back(&SkinPart);
		}
		const CSkins7::CSkinPart *pRandomPart;
		if(vConsideredSkinParts.empty())
		{
			pRandomPart = FindDefaultSkinPart(Part);
		}
		else
		{
			pRandomPart = vConsideredSkinParts[rand() % vConsideredSkinParts.size()];
		}
		str_copy(CSkins7::ms_apSkinVariables[Dummy][Part], pRandomPart->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
	}

	ms_apSkinNameVariables[Dummy][0] = '\0';
}

ColorRGBA CSkins7::GetColor(int Value, bool UseAlpha) const
{
	return color_cast<ColorRGBA>(ColorHSLA(Value, UseAlpha).UnclampLighting(ColorHSLA::DARKEST_LGT7));
}

ColorRGBA CSkins7::GetTeamColor(int UseCustomColors, int PartColor, int Team, int Part) const
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
	int l = clamp(mix(TeamLgt, PartLgt, 0.2), (int)ColorHSLA::DARKEST_LGT7, 200);

	int ColorVal = (h << 16) + (s << 8) + l;

	return GetColor(ColorVal, Part == protocol7::SKINPART_MARKING);
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

bool CSkins7::SaveSkinfile(const char *pName, int Dummy)
{
	const bool SpecialSkin = pName[0] == 'x' && pName[1] == '_';
	dbg_assert(!SpecialSkin, "Cannot save special skins");

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), SKINS_DIR "/%s.json", pName);
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

			const bool CustomColors = *ms_apUCCVariables[Dummy][PartIndex];
			Writer.WriteAttribute("custom_colors");
			Writer.WriteBoolValue(CustomColors);

			if(CustomColors)
			{
				for(int ColorComponent = 0; ColorComponent < NUM_COLOR_COMPONENTS - 1; ColorComponent++)
				{
					int Val = (*ms_apColorVariables[Dummy][PartIndex] >> (2 - ColorComponent) * 8) & 0xff;
					Writer.WriteAttribute(ms_apColorComponents[ColorComponent]);
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

	AddSkinFromConfigVariables(pName, Dummy);
	return true;
}
