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

bool CSkins::IsVanillaSkin(const char *pName)
{
	return std::any_of(std::begin(VANILLA_SKINS), std::end(VANILLA_SKINS), [pName](const char *pVanillaSkin) { return str_comp(pName, pVanillaSkin) == 0; });
}

class CSkinScanUser
{
public:
	CSkins *m_pThis;
	CSkins::TSkinLoadedCallback m_SkinLoadedCallback;
};

int CSkins::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pUserReal = static_cast<CSkinScanUser *>(pUser);
	CSkins *pSelf = pUserReal->m_pThis;

	if(IsDir)
		return 0;

	const char *pSuffix = str_endswith(pName, ".png");
	if(pSuffix == nullptr)
		return 0;

	char aSkinName[IO_MAX_PATH_LENGTH];
	str_truncate(aSkinName, sizeof(aSkinName), pName, pSuffix - pName);
	if(!CSkin::IsValidName(aSkinName))
	{
		log_error("skins", "Skin name is not valid: %s", aSkinName);
		log_error("skins", "%s", CSkin::m_aSkinNameRestrictions);
		return 0;
	}

	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(aSkinName))
		return 0;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "skins/%s", pName);
	pSelf->LoadSkin(aSkinName, aPath, DirType);
	pUserReal->m_SkinLoadedCallback();
	return 0;
}

static void CheckMetrics(CSkin::SSkinMetricVariable &Metrics, const uint8_t *pImg, int ImgWidth, int ImgX, int ImgY, int CheckWidth, int CheckHeight)
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

	Metrics.m_Width = clamp((MaxX - MinX) + 1, 1, CheckWidth);
	Metrics.m_Height = clamp((MaxY - MinY) + 1, 1, CheckHeight);
	Metrics.m_OffsetX = clamp(MinX, 0, CheckWidth - 1);
	Metrics.m_OffsetY = clamp(MinY, 0, CheckHeight - 1);
	Metrics.m_MaxWidth = CheckWidth;
	Metrics.m_MaxHeight = CheckHeight;
}

const CSkin *CSkins::LoadSkin(const char *pName, const char *pPath, int DirType)
{
	CImageInfo Info;
	if(!Graphics()->LoadPng(Info, pPath, DirType))
	{
		log_error("skins", "Failed to load skin PNG: %s", pName);
		return nullptr;
	}
	return LoadSkin(pName, Info);
}

const CSkin *CSkins::LoadSkin(const char *pName, CImageInfo &Info)
{
	if(!Graphics()->CheckImageDivisibility(pName, Info, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy, true))
	{
		log_error("skins", "Skin failed image divisibility: %s", pName);
		return nullptr;
	}
	if(!Graphics()->IsImageFormatRgba(pName, Info))
	{
		log_error("skins", "Skin format is not RGBA: %s", pName);
		return nullptr;
	}

	CSkin Skin{pName};
	Skin.m_OriginalSkin.m_Body = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_BODY]);
	Skin.m_OriginalSkin.m_BodyOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE]);
	Skin.m_OriginalSkin.m_Feet = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT]);
	Skin.m_OriginalSkin.m_FeetOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE]);
	Skin.m_OriginalSkin.m_Hands = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_HAND]);
	Skin.m_OriginalSkin.m_HandsOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_HAND_OUTLINE]);

	for(int i = 0; i < 6; ++i)
		Skin.m_OriginalSkin.m_aEyes[i] = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_EYE_NORMAL + i]);

	int FeetGridPixelsWidth = (Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_FOOT].m_pSet->m_Gridx);
	int FeetGridPixelsHeight = (Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_FOOT].m_pSet->m_Gridy);
	int FeetWidth = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_W * FeetGridPixelsWidth;
	int FeetHeight = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_H * FeetGridPixelsHeight;

	int FeetOffsetX = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_X * FeetGridPixelsWidth;
	int FeetOffsetY = g_pData->m_aSprites[SPRITE_TEE_FOOT].m_Y * FeetGridPixelsHeight;

	int FeetOutlineGridPixelsWidth = (Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_pSet->m_Gridx);
	int FeetOutlineGridPixelsHeight = (Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_pSet->m_Gridy);
	int FeetOutlineWidth = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_W * FeetOutlineGridPixelsWidth;
	int FeetOutlineHeight = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_H * FeetOutlineGridPixelsHeight;

	int FeetOutlineOffsetX = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_X * FeetOutlineGridPixelsWidth;
	int FeetOutlineOffsetY = g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE].m_Y * FeetOutlineGridPixelsHeight;

	int BodyOutlineGridPixelsWidth = (Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_pSet->m_Gridx);
	int BodyOutlineGridPixelsHeight = (Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_pSet->m_Gridy);
	int BodyOutlineWidth = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_W * BodyOutlineGridPixelsWidth;
	int BodyOutlineHeight = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_H * BodyOutlineGridPixelsHeight;

	int BodyOutlineOffsetX = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_X * BodyOutlineGridPixelsWidth;
	int BodyOutlineOffsetY = g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE].m_Y * BodyOutlineGridPixelsHeight;

	size_t BodyWidth = g_pData->m_aSprites[SPRITE_TEE_BODY].m_W * (Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx); // body width
	size_t BodyHeight = g_pData->m_aSprites[SPRITE_TEE_BODY].m_H * (Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy); // body height
	if(BodyWidth > Info.m_Width || BodyHeight > Info.m_Height)
		return nullptr;
	uint8_t *pData = Info.m_pData;
	const int PixelStep = 4;
	int Pitch = Info.m_Width * PixelStep;

	// dig out blood color
	{
		int64_t aColors[3] = {0};
		for(size_t y = 0; y < BodyHeight; y++)
		{
			for(size_t x = 0; x < BodyWidth; x++)
			{
				uint8_t AlphaValue = pData[y * Pitch + x * PixelStep + 3];
				if(AlphaValue > 128)
				{
					aColors[0] += pData[y * Pitch + x * PixelStep + 0];
					aColors[1] += pData[y * Pitch + x * PixelStep + 1];
					aColors[2] += pData[y * Pitch + x * PixelStep + 2];
				}
			}
		}

		Skin.m_BloodColor = ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
	}

	CheckMetrics(Skin.m_Metrics.m_Body, pData, Pitch, 0, 0, BodyWidth, BodyHeight);

	// body outline metrics
	CheckMetrics(Skin.m_Metrics.m_Body, pData, Pitch, BodyOutlineOffsetX, BodyOutlineOffsetY, BodyOutlineWidth, BodyOutlineHeight);

	// get feet size
	CheckMetrics(Skin.m_Metrics.m_Feet, pData, Pitch, FeetOffsetX, FeetOffsetY, FeetWidth, FeetHeight);

	// get feet outline size
	CheckMetrics(Skin.m_Metrics.m_Feet, pData, Pitch, FeetOutlineOffsetX, FeetOutlineOffsetY, FeetOutlineWidth, FeetOutlineHeight);

	ConvertToGrayscale(Info);

	int aFreq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequency
	for(size_t y = 0; y < BodyHeight; y++)
		for(size_t x = 0; x < BodyWidth; x++)
		{
			if(pData[y * Pitch + x * PixelStep + 3] > 128)
				aFreq[pData[y * Pitch + x * PixelStep]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(aFreq[OrgWeight] < aFreq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255 - OrgWeight;
	int InvNewWeight = 255 - NewWeight;
	for(size_t y = 0; y < BodyHeight; y++)
		for(size_t x = 0; x < BodyWidth; x++)
		{
			int v = pData[y * Pitch + x * PixelStep];
			if(v <= OrgWeight && OrgWeight == 0)
				v = 0;
			else if(v <= OrgWeight)
				v = (int)(((v / (float)OrgWeight) * NewWeight));
			else if(InvOrgWeight == 0)
				v = NewWeight;
			else
				v = (int)(((v - OrgWeight) / (float)InvOrgWeight) * InvNewWeight + NewWeight);
			pData[y * Pitch + x * PixelStep] = v;
			pData[y * Pitch + x * PixelStep + 1] = v;
			pData[y * Pitch + x * PixelStep + 2] = v;
		}

	Skin.m_ColorableSkin.m_Body = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_BODY]);
	Skin.m_ColorableSkin.m_BodyOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_BODY_OUTLINE]);
	Skin.m_ColorableSkin.m_Feet = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT]);
	Skin.m_ColorableSkin.m_FeetOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_FOOT_OUTLINE]);
	Skin.m_ColorableSkin.m_Hands = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_HAND]);
	Skin.m_ColorableSkin.m_HandsOutline = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_HAND_OUTLINE]);

	for(int i = 0; i < 6; ++i)
		Skin.m_ColorableSkin.m_aEyes[i] = Graphics()->LoadSpriteTexture(Info, &g_pData->m_aSprites[SPRITE_TEE_EYE_NORMAL + i]);

	Info.Free();

	if(g_Config.m_Debug)
	{
		log_trace("skins", "Loaded skin '%s'", Skin.GetName());
	}

	auto &&pSkin = std::make_unique<CSkin>(std::move(Skin));
	const auto SkinInsertIt = m_Skins.insert({pSkin->GetName(), std::move(pSkin)});

	m_LastRefreshTime = time_get_nanoseconds();

	return SkinInsertIt.first->second.get();
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
	m_LoadingSkins.clear();
}

void CSkins::Refresh(TSkinLoadedCallback &&SkinLoadedCallback)
{
	m_LoadingSkins.clear();

	for(const auto &[_, pSkin] : m_Skins)
	{
		pSkin->m_OriginalSkin.Unload(Graphics());
		pSkin->m_ColorableSkin.Unload(Graphics());
	}
	m_Skins.clear();

	CSkinScanUser SkinScanUser;
	SkinScanUser.m_pThis = this;
	SkinScanUser.m_SkinLoadedCallback = SkinLoadedCallback;
	Storage()->ListDirectory(IStorage::TYPE_ALL, "skins", SkinScan, &SkinScanUser);

	m_LastRefreshTime = time_get_nanoseconds();
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

const CSkin *CSkins::FindOrNullptr(const char *pName, bool IgnorePrefix)
{
	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(pName))
	{
		return nullptr;
	}

	const char *pSkinPrefix = m_aEventSkinPrefix[0] != '\0' ? m_aEventSkinPrefix : g_Config.m_ClSkinPrefix;
	if(!IgnorePrefix && pSkinPrefix[0] != '\0')
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
	auto SkinIt = m_Skins.find(pName);
	if(SkinIt != m_Skins.end())
		return SkinIt->second.get();

	if(str_comp(pName, "default") == 0)
		return nullptr;

	if(!g_Config.m_ClDownloadSkins)
		return nullptr;

	if(!CSkin::IsValidName(pName))
		return nullptr;

	auto ExistingLoadingSkin = m_LoadingSkins.find(pName);
	if(ExistingLoadingSkin != m_LoadingSkins.end())
	{
		std::unique_ptr<CLoadingSkin> &pLoadingSkin = ExistingLoadingSkin->second;
		if(!pLoadingSkin->m_pDownloadJob || !pLoadingSkin->m_pDownloadJob->Done())
			return nullptr;

		if(pLoadingSkin->m_pDownloadJob->State() == IJob::STATE_DONE && pLoadingSkin->m_pDownloadJob->ImageInfo().m_pData)
		{
			LoadSkin(pLoadingSkin->Name(), pLoadingSkin->m_pDownloadJob->ImageInfo());
		}
		pLoadingSkin->m_pDownloadJob = nullptr;
		return nullptr;
	}

	CLoadingSkin LoadingSkin(pName);
	LoadingSkin.m_pDownloadJob = std::make_shared<CSkinDownloadJob>(this, pName);
	Engine()->AddJob(LoadingSkin.m_pDownloadJob);
	auto &&pLoadingSkin = std::make_unique<CLoadingSkin>(std::move(LoadingSkin));
	m_LoadingSkins.insert({pLoadingSkin->Name(), std::move(pLoadingSkin)});
	return nullptr;
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
		Body.s = clamp(GoalSat * GoalSat / (1.0f - Body.l), 0.0f, 1.0f);

		ColorHSLA Feet;
		Feet.h = std::fmod(Body.h + s_aSchemes[rand() % std::size(s_aSchemes)], 1.0f);
		Feet.l = random_float();
		Feet.s = clamp(GoalSat * GoalSat / (1.0f - Feet.l), 0.0f, 1.0f);

		unsigned *pColorBody = Dummy ? &g_Config.m_ClDummyColorBody : &g_Config.m_ClPlayerColorBody;
		unsigned *pColorFeet = Dummy ? &g_Config.m_ClDummyColorFeet : &g_Config.m_ClPlayerColorFeet;

		*pColorBody = Body.Pack(false);
		*pColorFeet = Feet.Pack(false);
	}

	const size_t SkinNameSize = Dummy ? sizeof(g_Config.m_ClDummySkin) : sizeof(g_Config.m_ClPlayerSkin);
	char aRandomSkinName[MAX_SKIN_LENGTH];
	str_copy(aRandomSkinName, "default", SkinNameSize);
	if(!m_Skins.empty())
	{
		do
		{
			auto it = m_Skins.begin();
			std::advance(it, rand() % m_Skins.size());
			str_copy(aRandomSkinName, (*it).second->GetName(), SkinNameSize);
		} while(!str_comp(aRandomSkinName, "x_ninja") || !str_comp(aRandomSkinName, "x_spec"));
	}
	char *pSkinName = Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
	str_copy(pSkinName, aRandomSkinName, SkinNameSize);
}

CSkins::CSkinDownloadJob::CSkinDownloadJob(CSkins *pSkins, const char *pName) :
	m_pSkins(pSkins)
{
	str_copy(m_aName, pName);
	Abortable(true);
}

CSkins::CSkinDownloadJob::~CSkinDownloadJob()
{
	m_ImageInfo.Free();
}

bool CSkins::CSkinDownloadJob::Abort()
{
	if(!IJob::Abort())
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

	// We assume the file does not exist if we could not get the times
	time_t FileCreatedTime, FileModifiedTime;
	const bool GotFileTimes = m_pSkins->Storage()->RetrieveTimes(aPathReal, IStorage::TYPE_SAVE, &FileCreatedTime, &FileModifiedTime);

	std::shared_ptr<CHttpRequest> pGet = HttpGet(aUrl);
	pGet->Timeout(Timeout);
	pGet->MaxResponseSize(MaxResponseSize);
	if(GotFileTimes)
	{
		pGet->IfModifiedSince(FileModifiedTime);
		pGet->FailOnErrorStatus(false);
	}
	pGet->LogProgress(HTTPLOG::NONE);
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = pGet;
	}
	m_pSkins->Http()->Run(pGet);

	// Load existing file while waiting for the HTTP request
	if(GotFileTimes)
	{
		m_pSkins->Graphics()->LoadPng(m_ImageInfo, aPathReal, IStorage::TYPE_SAVE);
	}

	pGet->Wait();
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = nullptr;
	}
	if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED || pGet->StatusCode() >= 400)
	{
		return;
	}
	if(pGet->StatusCode() == 304) // 304 Not Modified
	{
		if(m_ImageInfo.m_pData != nullptr)
		{
			return; // Local skin is up-to-date and was loaded successfully
		}

		log_error("skins", "Failed to load PNG of existing downloaded skin '%s' from '%s', downloading it again", m_aName, aPathReal);
		pGet = HttpGet(aUrl);
		pGet->Timeout(Timeout);
		pGet->MaxResponseSize(MaxResponseSize);
		pGet->LogProgress(HTTPLOG::NONE);
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
		if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED)
		{
			return;
		}
	}

	unsigned char *pResult;
	size_t ResultSize;
	pGet->Result(&pResult, &ResultSize);

	if(!m_pSkins->Graphics()->LoadPng(m_ImageInfo, pResult, ResultSize, aUrl))
	{
		log_error("skins", "Failed to load PNG of skin '%s' downloaded from '%s'", m_aName, aUrl);
		return;
	}

	if(State() == IJob::STATE_ABORTED)
	{
		return;
	}

	char aBuf[IO_MAX_PATH_LENGTH];
	char aPathTemp[IO_MAX_PATH_LENGTH];
	str_format(aPathTemp, sizeof(aPathTemp), "downloadedskins/%s", IStorage::FormatTmpPath(aBuf, sizeof(aBuf), m_aName));

	IOHANDLE TempFile = m_pSkins->Storage()->OpenFile(aPathTemp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!TempFile)
	{
		log_error("skins", "Failed to open temporary skin file '%s' for writing", aPathTemp);
		return;
	}
	if(io_write(TempFile, pResult, ResultSize) != ResultSize)
	{
		log_error("skins", "Failed to write downloaded skin data to '%s'", aPathTemp);
		io_close(TempFile);
		m_pSkins->Storage()->RemoveFile(aPathTemp, IStorage::TYPE_SAVE);
		return;
	}
	io_close(TempFile);

	if(!m_pSkins->Storage()->RenameFile(aPathTemp, aPathReal, IStorage::TYPE_SAVE))
	{
		log_error("skins", "Failed to rename temporary skin file '%s' to '%s'", aPathTemp, aPathReal);
		m_pSkins->Storage()->RemoveFile(aPathTemp, IStorage::TYPE_SAVE);
		return;
	}
}

CSkins::CLoadingSkin::CLoadingSkin(const char *pName)
{
	str_copy(m_aName, pName);
}

CSkins::CLoadingSkin::~CLoadingSkin()
{
	if(m_pDownloadJob)
	{
		m_pDownloadJob->Abort();
	}
}

bool CSkins::CLoadingSkin::operator<(const CLoadingSkin &Other) const
{
	return str_comp(m_aName, Other.m_aName) < 0;
}

bool CSkins::CLoadingSkin::operator<(const char *pOther) const
{
	return str_comp(m_aName, pOther) < 0;
}

bool CSkins::CLoadingSkin::operator==(const char *pOther) const
{
	return !str_comp(m_aName, pOther);
}
