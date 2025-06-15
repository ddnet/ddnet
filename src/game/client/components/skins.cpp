/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "skins.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/engine.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

using namespace std::chrono_literals;

CSkins::CAbstractSkinLoadJob::CAbstractSkinLoadJob(CSkins *pSkins, const char *pName) :
	m_pSkins(pSkins)
{
	str_copy(m_aName, pName);
	Abortable(true);
}

CSkins::CAbstractSkinLoadJob::~CAbstractSkinLoadJob()
{
	m_Data.m_Info.Free();
	m_Data.m_InfoGrayscale.Free();
}

CSkins::CSkinLoadJob::CSkinLoadJob(CSkins *pSkins, const char *pName, int StorageType) :
	CAbstractSkinLoadJob(pSkins, pName),
	m_StorageType(StorageType)
{
}

CSkins::CSkinContainer::CSkinContainer(CSkins *pSkins, const char *pName, const char *pNormalizedName, EType Type, int StorageType) :
	m_pSkins(pSkins),
	m_Type(Type),
	m_StorageType(StorageType)
{
	str_copy(m_aName, pName);
	str_copy(m_aNormalizedName, pNormalizedName);
	m_Vanilla = IsVanillaSkinNormalized(m_aNormalizedName);
	m_Special = IsSpecialSkinNormalized(m_aNormalizedName);
	m_AlwaysLoaded = m_Vanilla; // Vanilla skins are loaded immediately and not unloaded
}

CSkins::CSkinContainer::~CSkinContainer()
{
	if(m_pLoadJob)
	{
		m_pLoadJob->Abort();
	}
}

bool CSkins::CSkinContainer::operator<(const CSkinContainer &Other) const
{
	return str_comp(m_aNormalizedName, Other.m_aNormalizedName) < 0;
}

static constexpr std::chrono::nanoseconds MIN_REQUESTED_TIME_FOR_PENDING = 250ms;
static constexpr std::chrono::nanoseconds MAX_REQUESTED_TIME_FOR_PENDING = 500ms;
static constexpr std::chrono::nanoseconds MIN_UNLOAD_TIME_PENDING = 1s;
static constexpr std::chrono::nanoseconds MIN_UNLOAD_TIME_LOADED = 2s;
static_assert(MIN_REQUESTED_TIME_FOR_PENDING < MAX_REQUESTED_TIME_FOR_PENDING);
static_assert(MIN_REQUESTED_TIME_FOR_PENDING < MIN_UNLOAD_TIME_PENDING, "Unloading pending skins must take longer than adding more pending skins");

void CSkins::CSkinContainer::RequestLoad()
{
	if(m_AlwaysLoaded)
	{
		return;
	}

	// Delay loading skins a bit after the load has been requested to avoid loading a lot of skins
	// when quickly scrolling through lists or if a player with a new skin quickly joins and leaves.
	if(m_State == EState::UNLOADED)
	{
		const std::chrono::nanoseconds Now = time_get_nanoseconds();
		if(!m_FirstLoadRequest.has_value() ||
			!m_LastLoadRequest.has_value() ||
			Now - m_LastLoadRequest.value() > MAX_REQUESTED_TIME_FOR_PENDING)
		{
			m_FirstLoadRequest = Now;
			m_LastLoadRequest = m_FirstLoadRequest;
		}
		else if(Now - m_FirstLoadRequest.value() > MIN_REQUESTED_TIME_FOR_PENDING)
		{
			m_State = EState::PENDING;
		}
	}
	else if(m_State == EState::PENDING ||
		m_State == EState::LOADING ||
		m_State == EState::LOADED)
	{
		m_LastLoadRequest = time_get_nanoseconds();
	}

	if(m_State == EState::PENDING ||
		m_State == EState::LOADED)
	{
		if(m_UsageEntryIterator.has_value())
		{
			m_pSkins->m_SkinsUsageList.erase(m_UsageEntryIterator.value());
		}
		m_pSkins->m_SkinsUsageList.emplace_front(NormalizedName());
		m_UsageEntryIterator = m_pSkins->m_SkinsUsageList.begin();
	}
}

CSkins::CSkinContainer::EState CSkins::CSkinContainer::DetermineInitialState() const
{
	if(m_AlwaysLoaded)
	{
		// Load immediately if it should always be loaded
		return EState::PENDING;
	}
	else if((g_Config.m_ClVanillaSkinsOnly && !m_Vanilla) ||
		(m_Type == EType::DOWNLOAD && !g_Config.m_ClDownloadSkins))
	{
		// Fail immediately if it shouldn't be loaded
		return EState::NOT_FOUND;
	}
	else
	{
		return EState::UNLOADED;
	}
}

void CSkins::CSkinContainer::SetState(EState State)
{
	m_State = State;

	if(m_State == EState::PENDING ||
		m_State == EState::LOADING ||
		m_State == EState::LOADED)
	{
		RequestLoad();
	}
	else
	{
		m_FirstLoadRequest = std::nullopt;
		m_LastLoadRequest = std::nullopt;
	}

	if(m_State != EState::PENDING &&
		m_State != EState::LOADED &&
		m_UsageEntryIterator.has_value())
	{
		m_pSkins->m_SkinsUsageList.erase(m_UsageEntryIterator.value());
		m_UsageEntryIterator = std::nullopt;
	}

	m_pSkins->m_SkinList.ForceRefresh();
}

bool CSkins::CSkinListEntry::operator<(const CSkins::CSkinListEntry &Other) const
{
	if(m_Favorite && !Other.m_Favorite)
	{
		return true;
	}
	if(!m_Favorite && Other.m_Favorite)
	{
		return false;
	}
	return str_comp(m_pSkinContainer->NormalizedName(), Other.m_pSkinContainer->NormalizedName()) < 0;
}

void CSkins::CSkinListEntry::RequestLoad()
{
	m_pSkinContainer->RequestLoad();
}

CSkins::CSkins() :
	m_PlaceholderSkin("dummy")
{
	m_PlaceholderSkin.m_OriginalSkin.Reset();
	m_PlaceholderSkin.m_ColorableSkin.Reset();
	m_PlaceholderSkin.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	m_PlaceholderSkin.m_Metrics.m_Body.m_Width = 64;
	m_PlaceholderSkin.m_Metrics.m_Body.m_Height = 64;
	m_PlaceholderSkin.m_Metrics.m_Body.m_OffsetX = 16;
	m_PlaceholderSkin.m_Metrics.m_Body.m_OffsetY = 16;
	m_PlaceholderSkin.m_Metrics.m_Body.m_MaxWidth = 96;
	m_PlaceholderSkin.m_Metrics.m_Body.m_MaxHeight = 96;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_Width = 32;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_Height = 16;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_OffsetX = 16;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_OffsetY = 8;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_MaxWidth = 64;
	m_PlaceholderSkin.m_Metrics.m_Feet.m_MaxHeight = 32;
}

bool CSkins::IsSpecialSkin(const char *pName)
{
	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	return IsSpecialSkinNormalized(aNormalizedName);
}

bool CSkins::IsVanillaSkinNormalized(const char *pNormalizedName)
{
	return std::any_of(std::begin(VANILLA_SKINS), std::end(VANILLA_SKINS), [pNormalizedName](const char *pVanillaSkin) {
		return str_comp(pNormalizedName, pVanillaSkin) == 0;
	});
}

bool CSkins::IsSpecialSkinNormalized(const char *pNormalizedName)
{
	return str_startswith(pNormalizedName, "x_") != nullptr;
}

class CSkinScanUser
{
public:
	CSkins *m_pThis;
	CSkins::TSkinLoadedCallback m_SkinLoadedCallback;
};

int CSkins::SkinScan(const char *pName, int IsDir, int StorageType, void *pUser)
{
	auto *pUserReal = static_cast<CSkinScanUser *>(pUser);
	CSkins *pSelf = pUserReal->m_pThis;

	if(IsDir)
	{
		return 0;
	}

	const char *pSuffix = str_endswith(pName, ".png");
	if(pSuffix == nullptr)
	{
		return 0;
	}

	char aSkinName[IO_MAX_PATH_LENGTH];
	str_truncate(aSkinName, sizeof(aSkinName), pName, pSuffix - pName);
	if(!CSkin::IsValidName(aSkinName))
	{
		log_error("skins", "Skin name is not valid: %s", aSkinName);
		log_error("skins", "%s", CSkin::m_aSkinNameRestrictions);
		return 0;
	}

	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(aSkinName, aNormalizedName, sizeof(aNormalizedName));
	auto ExistingSkin = pSelf->m_Skins.find(aNormalizedName);
	if(ExistingSkin != pSelf->m_Skins.end())
	{
		return 0;
	}
	CSkinContainer SkinContainer(pSelf, aSkinName, aNormalizedName, CSkinContainer::EType::LOCAL, StorageType);
	auto &&pSkinContainer = std::make_unique<CSkinContainer>(std::move(SkinContainer));
	pSkinContainer->SetState(pSkinContainer->DetermineInitialState());
	pSelf->m_Skins.insert({pSkinContainer->NormalizedName(), std::move(pSkinContainer)});
	pUserReal->m_SkinLoadedCallback();
	return 0;
}

static void CheckMetrics(CSkin::CSkinMetricVariable &Metrics, const uint8_t *pImg, int ImgWidth, int ImgX, int ImgY, int CheckWidth, int CheckHeight)
{
	int MaxY = -1;
	int MinY = CheckHeight + 1;
	int MaxX = -1;
	int MinX = CheckWidth + 1;

	for(int y = 0; y < CheckHeight; y++)
	{
		for(int x = 0; x < CheckWidth; x++)
		{
			int OffsetAlpha = (y + ImgY) * ImgWidth + (x + ImgX) * 4 + 3;
			uint8_t AlphaValue = pImg[OffsetAlpha];
			if(AlphaValue > 0)
			{
				if(MaxY < y)
					MaxY = y;
				if(MinY > y)
					MinY = y;
				if(MaxX < x)
					MaxX = x;
				if(MinX > x)
					MinX = x;
			}
		}
	}

	Metrics.m_Width = std::clamp((MaxX - MinX) + 1, 1, CheckWidth);
	Metrics.m_Height = std::clamp((MaxY - MinY) + 1, 1, CheckHeight);
	Metrics.m_OffsetX = std::clamp(MinX, 0, CheckWidth - 1);
	Metrics.m_OffsetY = std::clamp(MinY, 0, CheckHeight - 1);
	Metrics.m_MaxWidth = CheckWidth;
	Metrics.m_MaxHeight = CheckHeight;
}

bool CSkins::LoadSkinData(const char *pName, CSkinLoadData &Data) const
{
	if(!Graphics()->CheckImageDivisibility(pName, Data.m_Info, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy, true))
	{
		log_error("skins", "Skin failed image divisibility: %s", pName);
		Data.m_Info.Free();
		return false;
	}
	if(!Graphics()->IsImageFormatRgba(pName, Data.m_Info))
	{
		log_error("skins", "Skin format is not RGBA: %s", pName);
		Data.m_Info.Free();
		return false;
	}
	const size_t BodyWidth = g_pData->m_aSprites[SPRITE_TEE_BODY].m_W * (Data.m_Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx);
	const size_t BodyHeight = g_pData->m_aSprites[SPRITE_TEE_BODY].m_H * (Data.m_Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy);
	if(BodyWidth > Data.m_Info.m_Width || BodyHeight > Data.m_Info.m_Height)
	{
		log_error("skins", "Skin size unsupported (w=%" PRIzu ", h=%" PRIzu "): %s", Data.m_Info.m_Width, Data.m_Info.m_Height, pName);
		Data.m_Info.Free();
		return false;
	}

	int FeetGridPixelsWidth = Data.m_Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_FOOT].m_pSet->m_Gridx;
	int FeetGridPixelsHeight = Data.m_Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_FOOT].m_pSet->m_Gridy;
	int FeetWidth = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_W * FeetGridPixelsWidth;
	int FeetHeight = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_H * FeetGridPixelsHeight;
	int FeetOffsetX = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_X * FeetGridPixelsWidth;
	int FeetOffsetY = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_Y * FeetGridPixelsHeight;

	int FeetOutlineGridPixelsWidth = Data.m_Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_pSet->m_Gridx;
	int FeetOutlineGridPixelsHeight = Data.m_Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_pSet->m_Gridy;
	int FeetOutlineWidth = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_W * FeetOutlineGridPixelsWidth;
	int FeetOutlineHeight = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_H * FeetOutlineGridPixelsHeight;
	int FeetOutlineOffsetX = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_X * FeetOutlineGridPixelsWidth;
	int FeetOutlineOffsetY = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_Y * FeetOutlineGridPixelsHeight;

	int BodyOutlineGridPixelsWidth = Data.m_Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_pSet->m_Gridx;
	int BodyOutlineGridPixelsHeight = Data.m_Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_pSet->m_Gridy;
	int BodyOutlineWidth = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_W * BodyOutlineGridPixelsWidth;
	int BodyOutlineHeight = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_H * BodyOutlineGridPixelsHeight;
	int BodyOutlineOffsetX = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_X * BodyOutlineGridPixelsWidth;
	int BodyOutlineOffsetY = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_Y * BodyOutlineGridPixelsHeight;

	const size_t PixelStep = Data.m_Info.PixelSize();
	const size_t Pitch = Data.m_Info.m_Width * PixelStep;

	// dig out blood color
	{
		int64_t aColors[3] = {0};
		for(size_t y = 0; y < BodyHeight; y++)
		{
			for(size_t x = 0; x < BodyWidth; x++)
			{
				const size_t Offset = y * Pitch + x * PixelStep;
				if(Data.m_Info.m_pData[Offset + 3] > 128)
				{
					for(size_t c = 0; c < 3; c++)
					{
						aColors[c] += Data.m_Info.m_pData[Offset + c];
					}
				}
			}
		}
		Data.m_BloodColor = ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
	}

	CheckMetrics(Data.m_Metrics.m_Body, Data.m_Info.m_pData, Pitch, 0, 0, BodyWidth, BodyHeight);
	CheckMetrics(Data.m_Metrics.m_Body, Data.m_Info.m_pData, Pitch, BodyOutlineOffsetX, BodyOutlineOffsetY, BodyOutlineWidth, BodyOutlineHeight);
	CheckMetrics(Data.m_Metrics.m_Feet, Data.m_Info.m_pData, Pitch, FeetOffsetX, FeetOffsetY, FeetWidth, FeetHeight);
	CheckMetrics(Data.m_Metrics.m_Feet, Data.m_Info.m_pData, Pitch, FeetOutlineOffsetX, FeetOutlineOffsetY, FeetOutlineWidth, FeetOutlineHeight);

	Data.m_InfoGrayscale = Data.m_Info.DeepCopy();
	ConvertToGrayscale(Data.m_InfoGrayscale);

	int aFreq[256] = {0};
	uint8_t OrgWeight = 1;
	uint8_t NewWeight = 192;

	// find most common non-zero frequency
	for(size_t y = 0; y < BodyHeight; y++)
	{
		for(size_t x = 0; x < BodyWidth; x++)
		{
			const size_t Offset = y * Pitch + x * PixelStep;
			if(Data.m_InfoGrayscale.m_pData[Offset + 3] > 128)
			{
				aFreq[Data.m_InfoGrayscale.m_pData[Offset]]++;
			}
		}
	}

	for(int i = 1; i < 256; i++)
	{
		if(aFreq[OrgWeight] < aFreq[i])
		{
			OrgWeight = i;
		}
	}

	// reorder
	for(size_t y = 0; y < BodyHeight; y++)
	{
		for(size_t x = 0; x < BodyWidth; x++)
		{
			const size_t Offset = y * Pitch + x * PixelStep;
			uint8_t v = Data.m_InfoGrayscale.m_pData[Offset];
			if(v <= OrgWeight)
			{
				v = (uint8_t)((v / (float)OrgWeight) * NewWeight);
			}
			else
			{
				v = (uint8_t)(((v - OrgWeight) / (float)(255 - OrgWeight)) * (255 - NewWeight) + NewWeight);
			}
			Data.m_InfoGrayscale.m_pData[Offset] = v;
			Data.m_InfoGrayscale.m_pData[Offset + 1] = v;
			Data.m_InfoGrayscale.m_pData[Offset + 2] = v;
		}
	}

	return true;
}

void CSkins::LoadSkinFinish(CSkinContainer *pSkinContainer, const CSkinLoadData &Data)
{
	CSkin Skin{pSkinContainer->Name()};

	Skin.m_OriginalSkin.m_Body = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_BODY]);
	Skin.m_OriginalSkin.m_BodyOutline = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE]);
	Skin.m_OriginalSkin.m_Feet = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT]);
	Skin.m_OriginalSkin.m_FeetOutline = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE]);
	Skin.m_OriginalSkin.m_Hands = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_HAND]);
	Skin.m_OriginalSkin.m_HandsOutline = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_HAND_OUTLINE]);
	for(size_t i = 0; i < std::size(Skin.m_OriginalSkin.m_aEyes); ++i)
	{
		Skin.m_OriginalSkin.m_aEyes[i] = Graphics()->LoadSpriteTexture(Data.m_Info, &g_pData->m_aSprites[SPRITE_TEE_EYE_NORMAL + i]);
	}

	Skin.m_ColorableSkin.m_Body = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_BODY]);
	Skin.m_ColorableSkin.m_BodyOutline = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE]);
	Skin.m_ColorableSkin.m_Feet = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_FOOT]);
	Skin.m_ColorableSkin.m_FeetOutline = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE]);
	Skin.m_ColorableSkin.m_Hands = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_HAND]);
	Skin.m_ColorableSkin.m_HandsOutline = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_HAND_OUTLINE]);
	for(size_t i = 0; i < std::size(Skin.m_ColorableSkin.m_aEyes); ++i)
	{
		Skin.m_ColorableSkin.m_aEyes[i] = Graphics()->LoadSpriteTexture(Data.m_InfoGrayscale, &g_pData->m_aSprites[SPRITE_TEE_EYE_NORMAL + i]);
	}

	Skin.m_Metrics = Data.m_Metrics;
	Skin.m_BloodColor = Data.m_BloodColor;

	if(g_Config.m_Debug)
	{
		log_trace("skins", "Loaded skin '%s'", Skin.GetName());
	}

	auto SkinIt = m_Skins.find(pSkinContainer->NormalizedName());
	dbg_assert(SkinIt != m_Skins.end(), "LoadSkinFinish on skin '%s' which is not in m_Skins", pSkinContainer->NormalizedName());
	SkinIt->second->m_pSkin = std::make_unique<CSkin>(std::move(Skin));
	pSkinContainer->SetState(CSkinContainer::EState::LOADED);
}

void CSkins::LoadSkinDirect(const char *pName)
{
	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	auto ExistingSkin = m_Skins.find(aNormalizedName);
	if(ExistingSkin != m_Skins.end())
	{
		return;
	}
	CSkinContainer SkinContainer(this, pName, aNormalizedName, CSkinContainer::EType::LOCAL, IStorage::TYPE_ALL);
	auto &&pSkinContainer = std::make_unique<CSkinContainer>(std::move(SkinContainer));
	pSkinContainer->SetState(pSkinContainer->DetermineInitialState());
	const auto &[SkinIt, _] = m_Skins.insert({pSkinContainer->NormalizedName(), std::move(pSkinContainer)});

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "skins/%s.png", pName);
	CSkinLoadData DefaultSkinData;
	SkinIt->second->SetState(CSkinContainer::EState::LOADING);
	if(!Graphics()->LoadPng(DefaultSkinData.m_Info, aPath, SkinIt->second->StorageType()))
	{
		log_error("skins", "Failed to load PNG of skin '%s' from '%s'", pName, aPath);
		SkinIt->second->SetState(CSkinContainer::EState::ERROR);
	}
	else if(LoadSkinData(pName, DefaultSkinData))
	{
		LoadSkinFinish(SkinIt->second.get(), DefaultSkinData);
	}
	else
	{
		SkinIt->second->SetState(CSkinContainer::EState::ERROR);
	}
	DefaultSkinData.m_Info.Free();
	DefaultSkinData.m_InfoGrayscale.Free();
}

void CSkins::OnConsoleInit()
{
	ConfigManager()->RegisterCallback(CSkins::ConfigSaveCallback, this);
	Console()->Register("add_favorite_skin", "s[skin_name]", CFGFLAG_CLIENT, ConAddFavoriteSkin, this, "Add a skin as a favorite");
	Console()->Register("remove_favorite_skin", "s[skin_name]", CFGFLAG_CLIENT, ConRemFavoriteSkin, this, "Remove a skin from the favorites");

	Console()->Chain("player_skin", ConchainRefreshSkinList, this);
	Console()->Chain("dummy_skin", ConchainRefreshSkinList, this);
}

void CSkins::OnInit()
{
	m_aEventSkinPrefix[0] = '\0';

	if(g_Config.m_Events)
	{
		if(time_season() == SEASON_XMAS)
		{
			str_copy(m_aEventSkinPrefix, "santa");
		}
	}

	// load skins
	Refresh([this]() {
		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
	});
}

void CSkins::OnShutdown()
{
	for(auto &[_, pSkinContainer] : m_Skins)
	{
		if(pSkinContainer->m_pLoadJob)
		{
			pSkinContainer->m_pLoadJob->Abort();
		}
	}
	m_Skins.clear();
}

void CSkins::OnUpdate()
{
	// Only update skins periodically to reduce FPS impact
	const std::chrono::nanoseconds StartTime = time_get_nanoseconds();
	const std::chrono::nanoseconds MaxTime = std::chrono::microseconds(maximum(round_to_int(Client()->RenderFrameTime() / 8.0f), 25));
	if(m_ContainerUpdateTime.has_value() && StartTime - m_ContainerUpdateTime.value() < MaxTime)
	{
		return;
	}
	m_ContainerUpdateTime = StartTime;

	// Update loaded state of managed skins which are not retrieved with the FindImpl function
	GameClient()->CollectManagedTeeRenderInfos([&](const char *pSkinName) {
		// This will update the loaded state of the container
		dbg_assert(FindContainerOrNullptr(pSkinName) != nullptr, "No skin container found for managed tee render info: %s", pSkinName);
	});
	// Keep player and dummy skin loaded
	FindContainerOrNullptr(g_Config.m_ClPlayerSkin);
	FindContainerOrNullptr(g_Config.m_ClDummySkin);

	CSkinLoadingStats Stats = LoadingStats();
	UpdateUnloadSkins(Stats);
	UpdateStartLoading(Stats);
	UpdateFinishLoading(Stats, StartTime, MaxTime);
}

void CSkins::UpdateUnloadSkins(CSkinLoadingStats &Stats)
{
	if(Stats.m_NumPending + Stats.m_NumLoaded + Stats.m_NumLoading <= (size_t)g_Config.m_ClSkinsLoadedMax)
	{
		return;
	}

	const std::chrono::nanoseconds UnloadStart = time_get_nanoseconds();
	size_t NumToUnload = std::min<size_t>(Stats.m_NumPending + Stats.m_NumLoaded + Stats.m_NumLoading - (size_t)g_Config.m_ClSkinsLoadedMax, 16);
	const size_t MaxSkipped = m_SkinsUsageList.size() / 8;
	size_t NumSkipped = 0;
	for(auto It = m_SkinsUsageList.rbegin(); It != m_SkinsUsageList.rend() && NumToUnload != 0 && NumSkipped < MaxSkipped; ++It)
	{
		auto SkinIt = m_Skins.find(*It);
		dbg_assert(SkinIt != m_Skins.end(), "m_SkinsUsageList contains skin not in m_Skins");
		auto &pSkinContainer = SkinIt->second;
		dbg_assert(!pSkinContainer->m_AlwaysLoaded, "m_SkinsUsageList contains skins with m_AlwaysLoaded");
		if(pSkinContainer->m_State != CSkinContainer::EState::PENDING &&
			pSkinContainer->m_State != CSkinContainer::EState::LOADED)
		{
			dbg_assert(pSkinContainer->m_State == CSkinContainer::EState::LOADING, "m_SkinsUsageList contains skin which is not PENDING, LOADING or LOADED");
			NumSkipped++;
			continue;
		}
		const std::chrono::nanoseconds TimeUnused = UnloadStart - pSkinContainer->m_LastLoadRequest.value();
		if(TimeUnused < (pSkinContainer->m_State == CSkinContainer::EState::LOADED ? MIN_UNLOAD_TIME_LOADED : MIN_UNLOAD_TIME_PENDING))
		{
			NumSkipped++;
			continue;
		}
		if(pSkinContainer->m_State == CSkinContainer::EState::LOADED)
		{
			pSkinContainer->m_pSkin->m_OriginalSkin.Unload(Graphics());
			pSkinContainer->m_pSkin->m_ColorableSkin.Unload(Graphics());
			pSkinContainer->m_pSkin = nullptr;
			Stats.m_NumLoaded--;
		}
		else
		{
			Stats.m_NumPending--;
		}
		Stats.m_NumUnloaded++;
		pSkinContainer->SetState(CSkinContainer::EState::UNLOADED);
		NumToUnload--;
	}
}

void CSkins::UpdateStartLoading(CSkinLoadingStats &Stats)
{
	for(auto &[_, pSkinContainer] : m_Skins)
	{
		if(Stats.m_NumPending == 0 || Stats.m_NumLoading + Stats.m_NumLoaded >= (size_t)g_Config.m_ClSkinsLoadedMax)
		{
			break;
		}
		if(pSkinContainer->m_State != CSkinContainer::EState::PENDING)
		{
			continue;
		}
		switch(pSkinContainer->Type())
		{
		case CSkinContainer::EType::LOCAL:
			pSkinContainer->m_pLoadJob = std::make_shared<CSkinLoadJob>(this, pSkinContainer->Name(), pSkinContainer->StorageType());
			break;
		case CSkinContainer::EType::DOWNLOAD:
			pSkinContainer->m_pLoadJob = std::make_shared<CSkinDownloadJob>(this, pSkinContainer->Name());
			break;
		default:
			dbg_assert(false, "pSkinContainer->Type() invalid");
			dbg_break();
		}
		Engine()->AddJob(pSkinContainer->m_pLoadJob);
		pSkinContainer->SetState(CSkinContainer::EState::LOADING);
		Stats.m_NumPending--;
		Stats.m_NumLoading++;
	}
}

void CSkins::UpdateFinishLoading(CSkinLoadingStats &Stats, std::chrono::nanoseconds StartTime, std::chrono::nanoseconds MaxTime)
{
	for(auto &[_, pSkinContainer] : m_Skins)
	{
		if(Stats.m_NumLoading == 0)
		{
			break;
		}
		if(pSkinContainer->m_State != CSkinContainer::EState::LOADING)
		{
			continue;
		}
		dbg_assert(pSkinContainer->m_pLoadJob != nullptr, "Skin container in loading state must have a load job");
		if(!pSkinContainer->m_pLoadJob->Done())
		{
			continue;
		}
		Stats.m_NumLoading--;
		if(pSkinContainer->m_pLoadJob->State() == IJob::STATE_DONE && pSkinContainer->m_pLoadJob->m_Data.m_Info.m_pData)
		{
			LoadSkinFinish(pSkinContainer.get(), pSkinContainer->m_pLoadJob->m_Data);
			GameClient()->OnSkinUpdate(pSkinContainer->Name());
			pSkinContainer->m_pLoadJob = nullptr;
			Stats.m_NumLoaded++;
			if(time_get_nanoseconds() - StartTime >= MaxTime)
			{
				// Avoid using too much frame time for loading skins
				break;
			}
		}
		else
		{
			if(pSkinContainer->m_pLoadJob->State() == IJob::STATE_DONE && pSkinContainer->m_pLoadJob->m_NotFound)
			{
				pSkinContainer->SetState(CSkinContainer::EState::NOT_FOUND);
				Stats.m_NumNotFound++;
			}
			else
			{
				pSkinContainer->SetState(CSkinContainer::EState::ERROR);
				Stats.m_NumError++;
			}
			pSkinContainer->m_pLoadJob = nullptr;
		}
	}
}

void CSkins::Refresh(TSkinLoadedCallback &&SkinLoadedCallback)
{
	for(auto &[_, pSkinContainer] : m_Skins)
	{
		if(pSkinContainer->m_pLoadJob)
		{
			pSkinContainer->m_pLoadJob->Abort();
		}
		if(pSkinContainer->m_pSkin)
		{
			pSkinContainer->m_pSkin->m_OriginalSkin.Unload(Graphics());
			pSkinContainer->m_pSkin->m_ColorableSkin.Unload(Graphics());
		}
	}
	m_Skins.clear();
	m_SkinsUsageList.clear();

	LoadSkinDirect("default");
	SkinLoadedCallback();

	CSkinScanUser SkinScanUser;
	SkinScanUser.m_pThis = this;
	SkinScanUser.m_SkinLoadedCallback = SkinLoadedCallback;
	Storage()->ListDirectory(IStorage::TYPE_ALL, "skins", SkinScan, &SkinScanUser);
}

CSkins::CSkinLoadingStats CSkins::LoadingStats() const
{
	CSkinLoadingStats Stats;
	for(const auto &[_, pSkinContainer] : m_Skins)
	{
		switch(pSkinContainer->m_State)
		{
		case CSkinContainer::EState::UNLOADED:
			Stats.m_NumUnloaded++;
			break;
		case CSkinContainer::EState::PENDING:
			Stats.m_NumPending++;
			break;
		case CSkinContainer::EState::LOADING:
			Stats.m_NumLoading++;
			break;
		case CSkinContainer::EState::LOADED:
			Stats.m_NumLoaded++;
			break;
		case CSkinContainer::EState::ERROR:
			Stats.m_NumError++;
			break;
		case CSkinContainer::EState::NOT_FOUND:
			Stats.m_NumNotFound++;
			break;
		}
	}
	return Stats;
}

CSkins::CSkinList &CSkins::SkinList()
{
	if(!m_SkinList.m_NeedsUpdate)
	{
		return m_SkinList;
	}

	m_SkinList.m_vSkins.clear();
	m_SkinList.m_UnfilteredCount = 0;

	// Ensure all favorite skins are present as skin containers so they are included in the next loop.
	for(const auto &FavoriteSkin : m_Favorites)
	{
		FindContainerOrNullptr(FavoriteSkin.c_str());
	}

	char aPlayerSkin[NORMALIZED_SKIN_NAME_LENGTH];
	char aDummySkin[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(g_Config.m_ClPlayerSkin, aPlayerSkin, sizeof(aPlayerSkin));
	str_utf8_tolower(g_Config.m_ClDummySkin, aDummySkin, sizeof(aDummySkin));
	for(const auto &[_, pSkinContainer] : m_Skins)
	{
		if(pSkinContainer->IsSpecial())
		{
			continue;
		}

		const bool SelectedMain = str_comp(pSkinContainer->NormalizedName(), aPlayerSkin) == 0;
		const bool SelectedDummy = str_comp(pSkinContainer->NormalizedName(), aDummySkin) == 0;

		// Don't include skins in the list that couldn't be found in the database except the current player
		// and dummy skins to avoid showing a lot of not-found entries while the user is typing a skin name.
		if(pSkinContainer->m_State == CSkinContainer::EState::NOT_FOUND &&
			!pSkinContainer->IsSpecial() &&
			!SelectedMain &&
			!SelectedDummy)
		{
			continue;
		}
		m_SkinList.m_UnfilteredCount++;

		std::optional<std::pair<int, int>> NameMatch;
		if(g_Config.m_ClSkinFilterString[0] != '\0')
		{
			const char *pNameMatchEnd;
			const char *pNameMatchStart = str_utf8_find_nocase(pSkinContainer->Name(), g_Config.m_ClSkinFilterString, &pNameMatchEnd);
			if(pNameMatchStart == nullptr)
			{
				continue;
			}
			NameMatch = std::make_pair<int, int>(pNameMatchStart - pSkinContainer->Name(), pNameMatchEnd - pNameMatchStart);
		}
		m_SkinList.m_vSkins.emplace_back(pSkinContainer.get(), m_Favorites.find(pSkinContainer->NormalizedName()) != m_Favorites.end(), SelectedMain, SelectedDummy, NameMatch);
	}

	std::sort(m_SkinList.m_vSkins.begin(), m_SkinList.m_vSkins.end());
	m_SkinList.m_NeedsUpdate = false;
	return m_SkinList;
}

const CSkin *CSkins::Find(const char *pName)
{
	const auto *pSkin = FindOrNullptr(pName);
	if(pSkin == nullptr)
	{
		pSkin = FindOrNullptr("default");
	}
	if(pSkin == nullptr)
	{
		pSkin = &m_PlaceholderSkin;
	}
	return pSkin;
}

const CSkins::CSkinContainer *CSkins::FindContainerOrNullptr(const char *pName)
{
	if(!CSkin::IsValidName(pName))
	{
		return nullptr;
	}

	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	auto ExistingSkin = m_Skins.find(aNormalizedName);
	if(ExistingSkin == m_Skins.end())
	{
		CSkinContainer SkinContainer(this, pName, aNormalizedName, CSkinContainer::EType::DOWNLOAD, IStorage::TYPE_SAVE);
		auto &&pSkinContainer = std::make_unique<CSkinContainer>(std::move(SkinContainer));
		pSkinContainer->SetState(pSkinContainer->DetermineInitialState());
		ExistingSkin = m_Skins.insert({pSkinContainer->NormalizedName(), std::move(pSkinContainer)}).first;
	}
	ExistingSkin->second->RequestLoad();
	return ExistingSkin->second.get();
}

const CSkin *CSkins::FindOrNullptr(const char *pName, bool IgnorePrefix)
{
	const char *pSkinPrefix = m_aEventSkinPrefix[0] != '\0' ? m_aEventSkinPrefix : g_Config.m_ClSkinPrefix;
	if(!g_Config.m_ClVanillaSkinsOnly && !IgnorePrefix && pSkinPrefix[0] != '\0')
	{
		char aNameWithPrefix[2 * MAX_SKIN_LENGTH + 2]; // Larger than skin name length to allow IsValidName to check if it's too long
		str_format(aNameWithPrefix, sizeof(aNameWithPrefix), "%s_%s", pSkinPrefix, pName);
		// If we find something, use it, otherwise fall back to normal skins.
		const auto *pResult = FindImpl(aNameWithPrefix);
		if(pResult != nullptr)
		{
			return pResult;
		}
	}

	return FindImpl(pName);
}

const CSkin *CSkins::FindImpl(const char *pName)
{
	const CSkinContainer *pSkinContainer = FindContainerOrNullptr(pName);
	if(pSkinContainer == nullptr || pSkinContainer->m_State != CSkinContainer::EState::LOADED)
	{
		return nullptr;
	}
	return pSkinContainer->m_pSkin.get();
}

void CSkins::AddFavorite(const char *pName)
{
	if(!CSkin::IsValidName(pName))
	{
		log_error("skins", "Favorite skin name '%s' is not valid", pName);
		log_error("skins", "%s", CSkin::m_aSkinNameRestrictions);
		return;
	}

	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	const auto &[_, Inserted] = m_Favorites.emplace(aNormalizedName);
	if(Inserted)
	{
		m_SkinList.ForceRefresh();
	}
}

void CSkins::RemoveFavorite(const char *pName)
{
	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	const auto FavoriteIt = m_Favorites.find(aNormalizedName);
	if(FavoriteIt != m_Favorites.end())
	{
		m_Favorites.erase(FavoriteIt);
		m_SkinList.ForceRefresh();
	}
}

bool CSkins::IsFavorite(const char *pName) const
{
	char aNormalizedName[NORMALIZED_SKIN_NAME_LENGTH];
	str_utf8_tolower(pName, aNormalizedName, sizeof(aNormalizedName));
	return m_Favorites.find(aNormalizedName) != m_Favorites.end();
}

void CSkins::RandomizeSkin(int Dummy)
{
	static const float s_aSchemes[] = {1.0f / 2.0f, 1.0f / 3.0f, 1.0f / -3.0f, 1.0f / 12.0f, 1.0f / -12.0f}; // complementary, triadic, analogous
	const bool UseCustomColor = Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor;
	if(UseCustomColor)
	{
		float GoalSat = random_float(0.3f, 1.0f);
		float MaxBodyLht = 1.0f - GoalSat * GoalSat; // max allowed lightness before we start losing saturation

		ColorHSLA Body;
		Body.h = random_float();
		Body.l = random_float(0.0f, MaxBodyLht);
		Body.s = std::clamp(GoalSat * GoalSat / (1.0f - Body.l), 0.0f, 1.0f);

		ColorHSLA Feet;
		Feet.h = std::fmod(Body.h + s_aSchemes[rand() % std::size(s_aSchemes)], 1.0f);
		Feet.l = random_float();
		Feet.s = std::clamp(GoalSat * GoalSat / (1.0f - Feet.l), 0.0f, 1.0f);

		unsigned *pColorBody = Dummy ? &g_Config.m_ClDummyColorBody : &g_Config.m_ClPlayerColorBody;
		unsigned *pColorFeet = Dummy ? &g_Config.m_ClDummyColorFeet : &g_Config.m_ClPlayerColorFeet;

		*pColorBody = Body.Pack(false);
		*pColorFeet = Feet.Pack(false);
	}

	std::vector<const CSkinContainer *> vpConsideredSkins;
	for(const auto &[_, pSkinContainer] : m_Skins)
	{
		if(pSkinContainer->m_State == CSkinContainer::EState::ERROR ||
			pSkinContainer->m_State == CSkinContainer::EState::NOT_FOUND ||
			pSkinContainer->IsSpecial())
		{
			continue;
		}
		vpConsideredSkins.push_back(pSkinContainer.get());
	}
	const char *pRandomSkin;
	if(vpConsideredSkins.empty())
	{
		pRandomSkin = "default";
	}
	else
	{
		pRandomSkin = vpConsideredSkins[rand() % vpConsideredSkins.size()]->Name();
	}

	char *pSkinName = Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
	const size_t SkinNameSize = Dummy ? sizeof(g_Config.m_ClDummySkin) : sizeof(g_Config.m_ClPlayerSkin);
	str_copy(pSkinName, pRandomSkin, SkinNameSize);
	m_SkinList.ForceRefresh();
}

void CSkins::CSkinLoadJob::Run()
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "skins/%s.png", m_aName);
	if(m_pSkins->Graphics()->LoadPng(m_Data.m_Info, aPath, m_StorageType))
	{
		if(State() == IJob::STATE_ABORTED)
		{
			return;
		}
		m_pSkins->LoadSkinData(m_aName, m_Data);
	}
	else
	{
		log_error("skins", "Failed to load PNG of skin '%s' from '%s'", m_aName, aPath);
	}
}

CSkins::CSkinDownloadJob::CSkinDownloadJob(CSkins *pSkins, const char *pName) :
	CAbstractSkinLoadJob(pSkins, pName)
{
}

bool CSkins::CSkinDownloadJob::Abort()
{
	if(!CAbstractSkinLoadJob::Abort())
	{
		return false;
	}

	const CLockScope LockScope(m_Lock);
	if(m_pGetRequest)
	{
		m_pGetRequest->Abort();
		m_pGetRequest = nullptr;
	}
	return true;
}

void CSkins::CSkinDownloadJob::Run()
{
	const char *pBaseUrl = g_Config.m_ClDownloadCommunitySkins != 0 ? g_Config.m_ClSkinCommunityDownloadUrl : g_Config.m_ClSkinDownloadUrl;

	char aEscapedName[256];
	EscapeUrl(aEscapedName, m_aName);

	char aUrl[IO_MAX_PATH_LENGTH];
	str_format(aUrl, sizeof(aUrl), "%s%s.png", pBaseUrl, aEscapedName);

	char aPathReal[IO_MAX_PATH_LENGTH];
	str_format(aPathReal, sizeof(aPathReal), "downloadedskins/%s.png", m_aName);

	const CTimeout Timeout{10000, 0, 8192, 10};
	const size_t MaxResponseSize = 10 * 1024 * 1024; // 10 MiB

	std::shared_ptr<CHttpRequest> pGet = HttpGetBoth(aUrl, m_pSkins->Storage(), aPathReal, IStorage::TYPE_SAVE);
	pGet->Timeout(Timeout);
	pGet->MaxResponseSize(MaxResponseSize);
	pGet->ValidateBeforeOverwrite(true);
	pGet->LogProgress(HTTPLOG::NONE);
	pGet->FailOnErrorStatus(false);
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = pGet;
	}
	m_pSkins->Http()->Run(pGet);

	// Load existing file while waiting for the HTTP request
	{
		void *pPngData;
		unsigned PngSize;
		if(m_pSkins->Storage()->ReadFile(aPathReal, IStorage::TYPE_SAVE, &pPngData, &PngSize))
		{
			if(m_pSkins->Graphics()->LoadPng(m_Data.m_Info, static_cast<uint8_t *>(pPngData), PngSize, aPathReal))
			{
				if(State() == IJob::STATE_ABORTED)
				{
					return;
				}
				m_pSkins->LoadSkinData(m_aName, m_Data);
			}
			free(pPngData);
		}
	}

	pGet->Wait();
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = nullptr;
	}
	if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED || pGet->StatusCode() >= 400)
	{
		m_NotFound = pGet->State() == EHttpState::DONE && pGet->StatusCode() == 404; // 404 Not Found
		return;
	}
	if(pGet->StatusCode() == 304) // 304 Not Modified
	{
		bool Success = m_Data.m_Info.m_pData != nullptr;
		pGet->OnValidation(Success);
		if(Success)
		{
			return; // Local skin is up-to-date and was loaded successfully
		}

		log_error("skins", "Failed to load PNG of existing downloaded skin '%s' from '%s', downloading it again", m_aName, aPathReal);
		pGet = HttpGetBoth(aUrl, m_pSkins->Storage(), aPathReal, IStorage::TYPE_SAVE);
		pGet->Timeout(Timeout);
		pGet->MaxResponseSize(MaxResponseSize);
		pGet->ValidateBeforeOverwrite(true);
		pGet->SkipByFileTime(false);
		pGet->LogProgress(HTTPLOG::NONE);
		pGet->FailOnErrorStatus(false);
		{
			const CLockScope LockScope(m_Lock);
			m_pGetRequest = pGet;
		}
		m_pSkins->Http()->Run(pGet);
		pGet->Wait();
		{
			const CLockScope LockScope(m_Lock);
			m_pGetRequest = nullptr;
		}
		if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED || pGet->StatusCode() >= 400)
		{
			m_NotFound = pGet->State() == EHttpState::DONE && pGet->StatusCode() == 404; // 404 Not Found
			return;
		}
	}

	unsigned char *pResult;
	size_t ResultSize;
	pGet->Result(&pResult, &ResultSize);

	m_Data.m_Info.Free();
	m_Data.m_InfoGrayscale.Free();
	const bool Success = m_pSkins->Graphics()->LoadPng(m_Data.m_Info, pResult, ResultSize, aUrl);
	if(Success)
	{
		if(State() == IJob::STATE_ABORTED)
		{
			return;
		}
		m_pSkins->LoadSkinData(m_aName, m_Data);
	}
	else
	{
		log_error("skins", "Failed to load PNG of skin '%s' downloaded from '%s' (size %" PRIzu ")", m_aName, aUrl, ResultSize);
	}
	pGet->OnValidation(Success);
}

void CSkins::ConAddFavoriteSkin(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CSkins *>(pUserData);
	pSelf->AddFavorite(pResult->GetString(0));
}

void CSkins::ConRemFavoriteSkin(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CSkins *>(pUserData);
	pSelf->RemoveFavorite(pResult->GetString(0));
}

void CSkins::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	auto *pSelf = static_cast<CSkins *>(pUserData);
	pSelf->OnConfigSave(pConfigManager);
}

void CSkins::OnConfigSave(IConfigManager *pConfigManager)
{
	for(const auto &Favorite : m_Favorites)
	{
		char aBuffer[32 + MAX_SKIN_LENGTH];
		str_format(aBuffer, sizeof(aBuffer), "add_favorite_skin \"%s\"", Favorite.c_str());
		pConfigManager->WriteLine(aBuffer);
	}
}

void CSkins::ConchainRefreshSkinList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CSkins *pThis = static_cast<CSkins *>(pUserData);
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		pThis->m_SkinList.ForceRefresh();
	}
}
