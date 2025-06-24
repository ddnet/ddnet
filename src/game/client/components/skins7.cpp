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

char *CSkins7::ms_apSkinNameVariables[NUM_DUMMIES] = {nullptr};
char *CSkins7::ms_apSkinVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{nullptr}};
int *CSkins7::ms_apUCCVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{nullptr}};
int unsigned *CSkins7::ms_apColorVariables[NUM_DUMMIES][protocol7::NUM_SKINPARTS] = {{nullptr}};

#define SKINS_DIR "skins7"

// TODO: uncomment
// const float MIN_EYE_BODY_COLOR_DIST = 80.f; // between body and eyes (LAB color space)

void CSkins7::CSkinPart::ApplyTo(CTeeRenderInfo::CSixup &SixupRenderInfo) const
{
	SixupRenderInfo.m_aOriginalTextures[m_Type] = m_OriginalTexture;
	SixupRenderInfo.m_aColorableTextures[m_Type] = m_ColorableTexture;
	if(m_Type == protocol7::SKINPART_BODY)
	{
		SixupRenderInfo.m_BloodColor = m_BloodColor;
	}
}

bool CSkins7::CSkinPart::operator<(const CSkinPart &Other) const
{
	return str_comp_nocase(m_aName, Other.m_aName) < 0;
}

bool CSkins7::CSkin::operator<(const CSkin &Other) const
{
	return str_comp_nocase(m_aName, Other.m_aName) < 0;
}

bool CSkins7::CSkin::operator==(const CSkin &Other) const
{
	return str_comp(m_aName, Other.m_aName) == 0;
}

bool CSkins7::IsSpecialSkin(const char *pName)
{
	return str_startswith(pName, "x_") != nullptr;
}

class CSkinPartScanData
{
public:
	CSkins7 *m_pThis;
	CSkins7::TSkinLoadedCallback m_SkinLoadedCallback;
	int m_Part;
};

int CSkins7::SkinPartScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	CSkinPartScanData *pScanData = static_cast<CSkinPartScanData *>(pUser);
	pScanData->m_pThis->LoadSkinPart(pScanData->m_Part, pName, DirType);
	pScanData->m_SkinLoadedCallback();
	return 0;
}

static ColorRGBA DetermineBloodColor(int PartType, const CImageInfo &Info)
{
	if(PartType != protocol7::SKINPART_BODY)
	{
		return ColorRGBA(1.0f, 1.0f, 1.0f);
	}

	const size_t Step = Info.PixelSize();
	const size_t Pitch = Info.m_Width * Step;
	const size_t PartX = Info.m_Width / 2;
	const size_t PartY = 0;
	const size_t PartWidth = Info.m_Width / 2;
	const size_t PartHeight = Info.m_Height / 2;

	int64_t aColors[3] = {0};
	for(size_t y = PartY; y < PartY + PartHeight; y++)
	{
		for(size_t x = PartX; x < PartX + PartWidth; x++)
		{
			const size_t Offset = y * Pitch + x * Step;
			if(Info.m_pData[Offset + 3] > 128)
			{
				for(size_t c = 0; c < 3; c++)
				{
					aColors[c] += Info.m_pData[Offset + c];
				}
			}
		}
	}

	return ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
}

bool CSkins7::LoadSkinPart(int PartType, const char *pName, int DirType)
{
	size_t PartNameSize, PartNameCount;
	str_utf8_stats(pName, str_length(pName) - str_length(".png") + 1, IO_MAX_PATH_LENGTH, &PartNameSize, &PartNameCount);
	if(PartNameSize >= protocol7::MAX_SKIN_ARRAY_SIZE || PartNameCount > protocol7::MAX_SKIN_LENGTH)
	{
		log_error("skins7", "Failed to load skin part '%s/%s': name too long", CSkins7::ms_apSkinPartNames[PartType], pName);
		return false;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), SKINS_DIR "/%s/%s", CSkins7::ms_apSkinPartNames[PartType], pName);
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, aFilename, DirType))
	{
		log_error("skins7", "Failed to load skin part '%s/%s': failed to load PNG file", CSkins7::ms_apSkinPartNames[PartType], pName);
		return false;
	}
	if(!Graphics()->IsImageFormatRgba(aFilename, Info))
	{
		log_error("skins7", "Failed to load skin part '%s/%s': must be RGBA format", CSkins7::ms_apSkinPartNames[PartType], pName);
		Info.Free();
		return false;
	}

	CSkinPart Part;
	Part.m_Type = PartType;
	Part.m_Flags = 0;
	if(IsSpecialSkin(pName))
	{
		Part.m_Flags |= SKINFLAG_SPECIAL;
	}
	if(DirType != IStorage::TYPE_SAVE)
	{
		Part.m_Flags |= SKINFLAG_STANDARD;
	}
	str_copy(Part.m_aName, pName, minimum<int>(PartNameSize + 1, sizeof(Part.m_aName)));
	Part.m_OriginalTexture = Graphics()->LoadTextureRaw(Info, 0, aFilename);
	Part.m_BloodColor = DetermineBloodColor(Part.m_Type, Info);
	ConvertToGrayscale(Info);
	Part.m_ColorableTexture = Graphics()->LoadTextureRawMove(Info, 0, aFilename);

	if(Config()->m_Debug)
	{
		log_trace("skins7", "Loaded skin part '%s/%s'", CSkins7::ms_apSkinPartNames[PartType], Part.m_aName);
	}
	m_avSkinParts[PartType].emplace_back(Part);
	return true;
}

class CSkinScanData
{
public:
	CSkins7 *m_pThis;
	CSkins7::TSkinLoadedCallback m_SkinLoadedCallback;
};

int CSkins7::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(IsDir || !str_endswith(pName, ".json"))
		return 0;

	CSkinScanData *pScanData = static_cast<CSkinScanData *>(pUser);
	pScanData->m_pThis->LoadSkin(pName, DirType);
	pScanData->m_SkinLoadedCallback();
	return 0;
}

bool CSkins7::LoadSkin(const char *pName, int DirType)
{
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), SKINS_DIR "/%s", pName);
	void *pFileData;
	unsigned JsonFileSize;
	if(!Storage()->ReadFile(aFilename, DirType, &pFileData, &JsonFileSize))
	{
		log_error("skins7", "Failed to read skin json file '%s'", aFilename);
		return false;
	}

	CSkin Skin;
	str_copy(Skin.m_aName, pName, 1 + str_length(pName) - str_length(".json"));
	const bool SpecialSkin = IsSpecialSkin(Skin.m_aName);
	Skin.m_Flags = 0;
	if(SpecialSkin)
	{
		Skin.m_Flags |= SKINFLAG_SPECIAL;
	}
	if(DirType != IStorage::TYPE_SAVE)
	{
		Skin.m_Flags |= SKINFLAG_STANDARD;
	}

	json_settings JsonSettings{};
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, static_cast<const json_char *>(pFileData), JsonFileSize, aError);
	free(pFileData);
	if(pJsonData == nullptr)
	{
		log_error("skins7", "Failed to parse skin json file '%s': %s", aFilename, aError);
		return false;
	}

	const json_value &Start = (*pJsonData)["skin"];
	if(Start.type != json_object)
	{
		log_error("skins7", "Failed to parse skin json file '%s': root must be an object", aFilename);
		json_value_free(pJsonData);
		return false;
	}

	for(int PartIndex = 0; PartIndex < protocol7::NUM_SKINPARTS; ++PartIndex)
	{
		Skin.m_aUseCustomColors[PartIndex] = 0;
		Skin.m_aPartColors[PartIndex] = (PartIndex == protocol7::SKINPART_MARKING ? 0xFF000000u : 0u) + 0x00FF80u;

		const json_value &Part = Start[(const char *)ms_apSkinPartNames[PartIndex]];
		if(Part.type == json_none)
		{
			Skin.m_apParts[PartIndex] = FindDefaultSkinPart(PartIndex);
			continue;
		}
		if(Part.type != json_object)
		{
			log_error("skins7", "Failed to parse skin json file '%s': attribute '%s' must specify an object", aFilename, ms_apSkinPartNames[PartIndex]);
			json_value_free(pJsonData);
			return false;
		}

		const json_value &Filename = Part["filename"];
		if(Filename.type == json_string)
		{
			Skin.m_apParts[PartIndex] = FindSkinPart(PartIndex, (const char *)Filename, SpecialSkin);
		}
		else
		{
			log_error("skins7", "Failed to parse skin json file '%s': part '%s' attribute 'filename' must specify a string", aFilename, ms_apSkinPartNames[PartIndex]);
			json_value_free(pJsonData);
			return false;
		}

		bool UseCustomColors = false;
		const json_value &Color = Part["custom_colors"];
		if(Color.type == json_string)
			UseCustomColors = str_comp((const char *)Color, "true") == 0;
		else if(Color.type == json_boolean)
			UseCustomColors = Color.u.boolean;
		Skin.m_aUseCustomColors[PartIndex] = UseCustomColors;

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

	if(Config()->m_Debug)
	{
		log_trace("skins7", "Loaded skin '%s'", Skin.m_aName);
	}
	m_vSkins.insert(std::lower_bound(m_vSkins.begin(), m_vSkins.end(), Skin), Skin);
	return true;
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

	Refresh([this]() {
		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
	});
}

void CSkins7::InitPlaceholderSkinParts()
{
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		CSkinPart &SkinPart = m_aPlaceholderSkinParts[Part];
		SkinPart.m_Type = Part;
		SkinPart.m_Flags = SKINFLAG_STANDARD;
		str_copy(SkinPart.m_aName, "dummy");
		SkinPart.m_OriginalTexture.Invalidate();
		SkinPart.m_ColorableTexture.Invalidate();
		SkinPart.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CSkins7::Refresh(TSkinLoadedCallback &&SkinLoadedCallback)
{
	m_vSkins.clear();

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		for(CSkinPart &SkinPart : m_avSkinParts[Part])
		{
			Graphics()->UnloadTexture(&SkinPart.m_OriginalTexture);
			Graphics()->UnloadTexture(&SkinPart.m_ColorableTexture);
		}
		m_avSkinParts[Part].clear();

		if(Part == protocol7::SKINPART_MARKING || Part == protocol7::SKINPART_DECORATION)
		{
			CSkinPart NoneSkinPart;
			NoneSkinPart.m_Type = Part;
			NoneSkinPart.m_Flags = SKINFLAG_STANDARD;
			NoneSkinPart.m_aName[0] = '\0';
			NoneSkinPart.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			m_avSkinParts[Part].emplace_back(NoneSkinPart);
		}

		CSkinPartScanData SkinPartScanData;
		SkinPartScanData.m_pThis = this;
		SkinPartScanData.m_SkinLoadedCallback = SkinLoadedCallback;
		SkinPartScanData.m_Part = Part;
		char aPartsDirectory[IO_MAX_PATH_LENGTH];
		str_format(aPartsDirectory, sizeof(aPartsDirectory), SKINS_DIR "/%s", ms_apSkinPartNames[Part]);
		Storage()->ListDirectory(IStorage::TYPE_ALL, aPartsDirectory, SkinPartScan, &SkinPartScanData);
	}

	CSkinScanData SkinScanData;
	SkinScanData.m_pThis = this;
	SkinScanData.m_SkinLoadedCallback = SkinLoadedCallback;
	Storage()->ListDirectory(IStorage::TYPE_ALL, SKINS_DIR, SkinScan, &SkinScanData);

	LoadXmasHat();
	LoadBotDecoration();
	SkinLoadedCallback();

	m_LastRefreshTime = time_get_nanoseconds();
}

void CSkins7::LoadXmasHat()
{
	Graphics()->UnloadTexture(&m_XmasHatTexture);

	const char *pFilename = SKINS_DIR "/xmas_hat.png";
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, pFilename, IStorage::TYPE_ALL) ||
		!Graphics()->IsImageFormatRgba(pFilename, Info) ||
		!Graphics()->CheckImageDivisibility(pFilename, Info, 1, 4, false))
	{
		log_error("skins7", "Failed to load xmas hat '%s'", pFilename);
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
	Graphics()->UnloadTexture(&m_BotTexture);

	const char *pFilename = SKINS_DIR "/bot.png";
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, pFilename, IStorage::TYPE_ALL) ||
		!Graphics()->IsImageFormatRgba(pFilename, Info) ||
		!Graphics()->CheckImageDivisibility(pFilename, Info, 12, 5, false))
	{
		log_error("skins7", "Failed to load bot decoration '%s'", pFilename);
		Info.Free();
	}
	else
	{
		if(Config()->m_Debug)
		{
			log_trace("skins7", "Loaded bot decoration '%s'", pFilename);
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
	m_LastRefreshTime = time_get_nanoseconds();
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
	m_LastRefreshTime = time_get_nanoseconds();
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
	const char *pDefaultPartName = Part == protocol7::SKINPART_MARKING || Part == protocol7::SKINPART_DECORATION ? "" : "standard";
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
		std::vector<const CSkins7::CSkinPart *> vpConsideredSkinParts;
		for(const CSkinPart &SkinPart : GetSkinParts(Part))
		{
			if((SkinPart.m_Flags & CSkins7::SKINFLAG_SPECIAL) != 0)
				continue;
			vpConsideredSkinParts.push_back(&SkinPart);
		}
		const CSkins7::CSkinPart *pRandomPart;
		if(vpConsideredSkinParts.empty())
		{
			pRandomPart = FindDefaultSkinPart(Part);
		}
		else
		{
			pRandomPart = vpConsideredSkinParts[rand() % vpConsideredSkinParts.size()];
		}
		str_copy(CSkins7::ms_apSkinVariables[Dummy][Part], pRandomPart->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
	}

	ms_apSkinNameVariables[Dummy][0] = '\0';
}

ColorRGBA CSkins7::GetColor(int Value, bool UseAlpha) const
{
	return color_cast<ColorRGBA>(ColorHSLA(Value, UseAlpha).UnclampLighting(ColorHSLA::DARKEST_LGT7));
}

void CSkins7::ApplyColorTo(CTeeRenderInfo::CSixup &SixupRenderInfo, bool UseCustomColors, int Value, int Part) const
{
	SixupRenderInfo.m_aUseCustomColors[Part] = UseCustomColors;
	if(UseCustomColors)
	{
		SixupRenderInfo.m_aColors[Part] = GetColor(Value, Part == protocol7::SKINPART_MARKING);
	}
	else
	{
		SixupRenderInfo.m_aColors[Part] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
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
	int s = std::clamp(mix(TeamSat, PartSat, 0.2), MinSat, MaxSat);
	int l = std::clamp(mix(TeamLgt, PartLgt, 0.2), (int)ColorHSLA::DARKEST_LGT7, 200);

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
	dbg_assert(!IsSpecialSkin(pName), "Cannot save special skins");

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
