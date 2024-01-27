/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include "skins.h"

bool CSkins::IsVanillaSkin(const char *pName)
{
	return std::any_of(std::begin(VANILLA_SKINS), std::end(VANILLA_SKINS), [pName](const char *pVanillaSkin) { return str_comp(pName, pVanillaSkin) == 0; });
}

void CSkins::CGetPngFile::OnCompletion(EHttpState State)
{
	// Maybe this should start another thread to load the png in instead of stalling the curl thread
	if(State == EHttpState::DONE)
	{
		m_pSkins->LoadSkinPNG(m_Info, Dest(), Dest(), IStorage::TYPE_SAVE);
	}
}

CSkins::CGetPngFile::CGetPngFile(CSkins *pSkins, const char *pUrl, IStorage *pStorage, const char *pDest) :
	CHttpRequest(pUrl),
	m_pSkins(pSkins)
{
	WriteToFile(pStorage, pDest, IStorage::TYPE_SAVE);
	Timeout(CTimeout{0, 0, 0, 0});
	LogProgress(HTTPLOG::NONE);
}

struct SSkinScanUser
{
	CSkins *m_pThis;
	CSkins::TSkinLoadedCBFunc m_SkinLoadedFunc;
};

int CSkins::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pUserReal = (SSkinScanUser *)pUser;
	CSkins *pSelf = pUserReal->m_pThis;

	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	char aNameWithoutPng[128];
	str_copy(aNameWithoutPng, pName);
	aNameWithoutPng[str_length(aNameWithoutPng) - 4] = 0;

	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(aNameWithoutPng))
		return 0;

	// Don't add duplicate skins (one from user's config directory, other from
	// client itself)
	if(pSelf->m_Skins.find(aNameWithoutPng) != pSelf->m_Skins.end())
		return 0;

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "skins/%s", pName);
	pSelf->LoadSkin(aNameWithoutPng, aBuf, DirType);
	pUserReal->m_SkinLoadedFunc((int)pSelf->m_Skins.size());
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
	if(!LoadSkinPNG(Info, pName, pPath, DirType))
		return 0;
	return LoadSkin(pName, Info);
}

bool CSkins::LoadSkinPNG(CImageInfo &Info, const char *pName, const char *pPath, int DirType)
{
	char aBuf[512];
	if(!Graphics()->LoadPNG(&Info, pPath, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load skin from %s", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		return false;
	}
	return true;
}

const CSkin *CSkins::LoadSkin(const char *pName, CImageInfo &Info)
{
	char aBuf[512];

	if(!Graphics()->CheckImageDivisibility(pName, Info, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy, true))
	{
		str_format(aBuf, sizeof(aBuf), "skin failed image divisibility: %s", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		return nullptr;
	}
	if(!Graphics()->IsImageFormatRGBA(pName, Info))
	{
		str_format(aBuf, sizeof(aBuf), "skin format is not RGBA: %s", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
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

	int BodyWidth = g_pData->m_aSprites[SPRITE_TEE_BODY].m_W * (Info.m_Width / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridx); // body width
	int BodyHeight = g_pData->m_aSprites[SPRITE_TEE_BODY].m_H * (Info.m_Height / g_pData->m_aSprites[SPRITE_TEE_BODY].m_pSet->m_Gridy); // body height
	if(BodyWidth > Info.m_Width || BodyHeight > Info.m_Height)
		return nullptr;
	unsigned char *pData = (unsigned char *)Info.m_pData;
	const int PixelStep = 4;
	int Pitch = Info.m_Width * PixelStep;

	// dig out blood color
	{
		int aColors[3] = {0};
		for(int y = 0; y < BodyHeight; y++)
			for(int x = 0; x < BodyWidth; x++)
			{
				uint8_t AlphaValue = pData[y * Pitch + x * PixelStep + 3];
				if(AlphaValue > 128)
				{
					aColors[0] += pData[y * Pitch + x * PixelStep + 0];
					aColors[1] += pData[y * Pitch + x * PixelStep + 1];
					aColors[2] += pData[y * Pitch + x * PixelStep + 2];
				}
			}
		if(aColors[0] != 0 && aColors[1] != 0 && aColors[2] != 0)
			Skin.m_BloodColor = ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
		else
			Skin.m_BloodColor = ColorRGBA(0, 0, 0, 1);
	}

	CheckMetrics(Skin.m_Metrics.m_Body, pData, Pitch, 0, 0, BodyWidth, BodyHeight);

	// body outline metrics
	CheckMetrics(Skin.m_Metrics.m_Body, pData, Pitch, BodyOutlineOffsetX, BodyOutlineOffsetY, BodyOutlineWidth, BodyOutlineHeight);

	// get feet size
	CheckMetrics(Skin.m_Metrics.m_Feet, pData, Pitch, FeetOffsetX, FeetOffsetY, FeetWidth, FeetHeight);

	// get feet outline size
	CheckMetrics(Skin.m_Metrics.m_Feet, pData, Pitch, FeetOutlineOffsetX, FeetOutlineOffsetY, FeetOutlineWidth, FeetOutlineHeight);

	// make the texture gray scale
	for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
	{
		int v = (pData[i * PixelStep] + pData[i * PixelStep + 1] + pData[i * PixelStep + 2]) / 3;
		pData[i * PixelStep] = v;
		pData[i * PixelStep + 1] = v;
		pData[i * PixelStep + 2] = v;
	}

	int aFreq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequency
	for(int y = 0; y < BodyHeight; y++)
		for(int x = 0; x < BodyWidth; x++)
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
	for(int y = 0; y < BodyHeight; y++)
		for(int x = 0; x < BodyWidth; x++)
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

	Graphics()->FreePNG(&Info);

	// set skin data
	if(g_Config.m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "load skin %s", Skin.GetName());
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
	}

	auto &&pSkin = std::make_unique<CSkin>(std::move(Skin));
	const auto SkinInsertIt = m_Skins.insert({pSkin->GetName(), std::move(pSkin)});

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

	// load skins;
	Refresh([this](int SkinCounter) {
		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading skin files"), 0);
	});
}

void CSkins::Refresh(TSkinLoadedCBFunc &&SkinLoadedFunc)
{
	for(const auto &SkinIt : m_Skins)
	{
		const auto &pSkin = SkinIt.second;
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_Body);
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_BodyOutline);
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_Feet);
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_FeetOutline);
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_Hands);
		Graphics()->UnloadTexture(&pSkin->m_OriginalSkin.m_HandsOutline);
		for(auto &Eye : pSkin->m_OriginalSkin.m_aEyes)
			Graphics()->UnloadTexture(&Eye);

		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_Body);
		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_BodyOutline);
		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_Feet);
		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_FeetOutline);
		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_Hands);
		Graphics()->UnloadTexture(&pSkin->m_ColorableSkin.m_HandsOutline);
		for(auto &Eye : pSkin->m_ColorableSkin.m_aEyes)
			Graphics()->UnloadTexture(&Eye);
	}

	m_Skins.clear();
	m_DownloadSkins.clear();
	m_DownloadingSkins = 0;
	SSkinScanUser SkinScanUser;
	SkinScanUser.m_pThis = this;
	SkinScanUser.m_SkinLoadedFunc = SkinLoadedFunc;
	Storage()->ListDirectory(IStorage::TYPE_ALL, "skins", SkinScan, &SkinScanUser);
	if(m_Skins.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load skins. folder='skins/'");
		CSkin DummySkin{"dummy"};
		DummySkin.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f);
		auto &&pDummySkin = std::make_unique<CSkin>(std::move(DummySkin));
		m_Skins.insert({pDummySkin->GetName(), std::move(pDummySkin)});
	}
}

int CSkins::Num()
{
	return m_Skins.size();
}

const CSkin *CSkins::Find(const char *pName)
{
	const auto *pSkin = FindOrNullptr(pName);
	if(pSkin == nullptr)
	{
		pSkin = FindOrNullptr("default");
		if(pSkin == nullptr)
			return m_Skins.begin()->second.get();
		else
			return pSkin;
	}
	else
	{
		return pSkin;
	}
}

const CSkin *CSkins::FindOrNullptr(const char *pName, bool IgnorePrefix)
{
	const char *pSkinPrefix = m_aEventSkinPrefix[0] ? m_aEventSkinPrefix : g_Config.m_ClSkinPrefix;
	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(pName))
	{
		return nullptr;
	}
	else if(pSkinPrefix && pSkinPrefix[0] && !IgnorePrefix)
	{
		char aBuf[24];
		str_format(aBuf, sizeof(aBuf), "%s_%s", pSkinPrefix, pName);
		// If we find something, use it, otherwise fall back to normal skins.
		const auto *pResult = FindImpl(aBuf);
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

	if(str_find(pName, "/") != 0)
		return nullptr;

	const auto SkinDownloadIt = m_DownloadSkins.find(pName);
	if(SkinDownloadIt != m_DownloadSkins.end())
	{
		if(SkinDownloadIt->second->m_pTask && SkinDownloadIt->second->m_pTask->State() == EHttpState::DONE && SkinDownloadIt->second->m_pTask->m_Info.m_pData)
		{
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "downloadedskins/%s.png", SkinDownloadIt->second->GetName());
			Storage()->RenameFile(SkinDownloadIt->second->m_aPath, aPath, IStorage::TYPE_SAVE);
			const auto *pSkin = LoadSkin(SkinDownloadIt->second->GetName(), SkinDownloadIt->second->m_pTask->m_Info);
			SkinDownloadIt->second->m_pTask = nullptr;
			--m_DownloadingSkins;
			return pSkin;
		}
		if(SkinDownloadIt->second->m_pTask && (SkinDownloadIt->second->m_pTask->State() == EHttpState::ERROR || SkinDownloadIt->second->m_pTask->State() == EHttpState::ABORTED))
		{
			SkinDownloadIt->second->m_pTask = nullptr;
			--m_DownloadingSkins;
		}
		return nullptr;
	}

	CDownloadSkin Skin{pName};

	char aUrl[IO_MAX_PATH_LENGTH];
	char aEscapedName[256];
	EscapeUrl(aEscapedName, sizeof(aEscapedName), pName);
	str_format(aUrl, sizeof(aUrl), "%s%s.png", g_Config.m_ClDownloadCommunitySkins != 0 ? g_Config.m_ClSkinCommunityDownloadUrl : g_Config.m_ClSkinDownloadUrl, aEscapedName);

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(Skin.m_aPath, sizeof(Skin.m_aPath), "downloadedskins/%s", IStorage::FormatTmpPath(aBuf, sizeof(aBuf), pName));

	Skin.m_pTask = std::make_shared<CGetPngFile>(this, aUrl, Storage(), Skin.m_aPath);
	Http()->Run(Skin.m_pTask);

	auto &&pDownloadSkin = std::make_unique<CDownloadSkin>(std::move(Skin));
	m_DownloadSkins.insert({pDownloadSkin->GetName(), std::move(pDownloadSkin)});
	++m_DownloadingSkins;

	return nullptr;
}
