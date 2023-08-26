/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>
#include <ctime>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/localization.h>
#include <thread>

#include "engine/gfx/image_loader.h"
#include "skins.h"

bool CSkins::IsVanillaSkin(const char *pName)
{
	auto StringComp = [pName](const char *pSkinName) { return str_comp(pName, pSkinName) == 0; };
	return std::any_of(std::begin(VANILLA_SKINS), std::end(VANILLA_SKINS), StringComp) ||
	       std::any_of(std::begin(BASE_SKINS), std::end(BASE_SKINS), StringComp);
}

bool CSkins::IsDuplicateSkin(const char *pName)
{
	return m_Skins.find(pName) != m_Skins.end();
}

int CSkins::CGetPngFile::OnCompletion(int State)
{
	State = CHttpRequest::OnCompletion(State);

	std::lock_guard Lock(m_pSkins->m_Mutex);
	if(State != HTTP_ERROR && State != HTTP_ABORTED && !m_pSkins->LoadSkinPNG(m_Info, Dest(), Dest(), IStorage::TYPE_SAVE))
	{
		State = HTTP_ERROR;
	}
	return State;
}

CSkins::CGetPngFile::CGetPngFile(CSkins *pSkins, const char *pUrl, IStorage *pStorage, const char *pDest) :
	CHttpRequest(pUrl),
	m_pSkins(pSkins)
{
	WriteToFile(pStorage, pDest, IStorage::TYPE_SAVE);
	Timeout(CTimeout{0, 0, 0, 0});
	LogProgress(HTTPLOG::NONE);
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
	return LoadSkin(pName, Info, m_Skins);
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

const CSkin *CSkins::LoadSkin(const char *pName, CImageInfo &Info, SkinsContainer<CSkin> &Container)
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

	// find most common frequence
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
	const auto SkinInsertIt = Container.insert({pSkin->GetName(), std::move(pSkin)});

	return SkinInsertIt.first->second.get();
}

CSkins::~CSkins()
{
	ClearSkins();

	UnloadSkins(m_BaseSkins);
	m_BaseSkins.clear();

	delete m_FileLoader;
}

void CSkins::OnInit()
{
	m_aEventSkinPrefix[0] = '\0';

	if(g_Config.m_Events)
	{
		time_t RawTime;
		struct tm *pTimeInfo;
		std::time(&RawTime);
		pTimeInfo = localtime(&RawTime);
		if(pTimeInfo->tm_mon == 11 && pTimeInfo->tm_mday >= 24 && pTimeInfo->tm_mday <= 26)
		{ // Christmas
			str_copy(m_aEventSkinPrefix, "santa");
		}
	}

	Refresh();
}

void CSkins::OnRender()
{
	if(m_LoadingSkinsFromDisk)
	{
		std::lock_guard Lock(m_Mutex); // Not ideal but necessary. Maybe a lockfree method can be devised in the future
		if(!m_ReadySkinTextures.empty())
		{
			for(const auto &It : m_ReadySkinTextures)
			{
				LoadSkin(It.first.data(), *It.second, m_Skins); // Upload skins that have been loaded on the file loader thread since last frame
			}
			m_ReadySkinTextures.clear();
		}
	}
}

bool CSkins::SkinLoadErrorCallback(CMassFileLoader::ELoadError Error, const void *pData, void *pUser)
{
	static char aMessage[128];
	switch(Error)
	{
	case CMassFileLoader::LOAD_ERROR_INVALID_SEARCH_PATH:
		str_format(aMessage, sizeof(aMessage), "Invalid skins path: '%s'", reinterpret_cast<const char *>(pData));
		break;
	case CMassFileLoader::LOAD_ERROR_UNWANTED_SYMLINK:
		str_format(aMessage, sizeof(aMessage), "Unwanted symlink in skins folder ('%s')", reinterpret_cast<const char *>(pData));
		break;
	case CMassFileLoader::LOAD_ERROR_FILE_UNREADABLE:
		str_format(aMessage, sizeof(aMessage), "Unreadable file in skins folder ('%s')", reinterpret_cast<const char *>(pData));
		break;
	case CMassFileLoader::LOAD_ERROR_FILE_TOO_LARGE:
		str_format(aMessage, sizeof(aMessage), "Skin too large ('%s')", reinterpret_cast<const char *>(pData));
		break;
	case CMassFileLoader::LOAD_ERROR_INVALID_EXTENSION:
		break;
	case CMassFileLoader::LOAD_ERROR_UNKNOWN:
		[[fallthrough]];
	default:
		dbg_msg("gameclient", "Unknown error encountered while loading skins.");
		return true;
	}

	dbg_msg("gameclient", "%s", aMessage);
	return true;
}

void CSkins::SkinLoadedCallback(const std::string_view
					ItemName,
	const unsigned char *pData, const unsigned int Size, void *pUser)
{
	auto *pThis = reinterpret_cast<CSkins *>(pUser);
	if(!pThis)
		return;

	std::lock_guard Lock(pThis->m_Mutex);

	// Name should have extension at this point, guaranteeing it is at least 4 bytes.
	const int idx = ItemName.find_last_of('/') + 1;
	std::string NameWithoutExtension = ItemName.substr(idx, ItemName.size() - idx - 4).data();

	if(g_Config.m_ClVanillaSkinsOnly && !CSkins::IsVanillaSkin(NameWithoutExtension.c_str()))
		return;

	for(const auto &It : pThis->BASE_SKINS) // If it's a base skin, it's already been loaded
		if(!str_comp(NameWithoutExtension.c_str(), It))
			return;

	// Don't add duplicate skins (one from user's config directory, other from
	// client itself)
	if(pThis->IsDuplicateSkin(NameWithoutExtension.c_str()))
	{
		dbg_msg("gameclient", "Duplicate skin '%s' will be ignored.", NameWithoutExtension.c_str());
		return;
	}

	TImageByteBuffer ByteBuffer;
	SImageByteBuffer ImageByteBuffer(&ByteBuffer);
	ByteBuffer.resize(Size);
	memcpy(&ByteBuffer.front(), pData, Size);

	std::unique_ptr<CImageInfo> Info = std::make_unique<CImageInfo>();
	uint8_t *pImgBuffer = NULL;
	EImageFormat ImageFormat;
	int PngliteIncompatible;

	if(!::LoadPNG(ImageByteBuffer, ItemName.data(), PngliteIncompatible, Info->m_Width, Info->m_Height, pImgBuffer, ImageFormat))
	{
		dbg_msg("gameclient", "Skin isn't valid PNG ('%s', %d bytes)", ItemName.data(), Size);
		return;
	}

	Info->m_pData = pImgBuffer;
	if(ImageFormat == IMAGE_FORMAT_RGB) // ignore_convention
		Info->m_Format = CImageInfo::FORMAT_RGB;
	else if(ImageFormat == IMAGE_FORMAT_RGBA) // ignore_convention
		Info->m_Format = CImageInfo::FORMAT_RGBA;
	else
	{
		free(pImgBuffer);
		dbg_msg("gameclient", "Skin format is not RGB or RGBA ('%s', %d bytes)", ItemName.data(), Size);
		return;
	}

	pThis->m_ReadySkinTextures.insert({NameWithoutExtension, std::move(Info)});

	dbg_msg("gameclient", "Skin loaded ('%s', %d bytes)", ItemName.data(), Size);
}

void CSkins::SkinLoadFinishedCallback(unsigned int Count, void *pUser)
{
	auto *pThis = reinterpret_cast<CSkins *>(pUser);
	if(!pThis)
		return;

	std::lock_guard Lock(pThis->m_Mutex);
	pThis->m_LoadingSkinsFromDisk = 0;
	dbg_msg("gameclient", "Skin loading finished (%d skins)", Count);
}

void CSkins::Refresh()
{
	m_Mutex.lock();

	ClearSkins();

	for(const auto &it : BASE_SKINS)
	{
		dbg_msg("gameclient", "Loading base skin %s", it);
		CImageInfo Info;
		if(LoadSkinPNG(Info, it, ("skins/" + std::string(it) + ".png").c_str(), IStorage::TYPE_ALL))
			LoadSkin(it, Info, m_BaseSkins);
	}
	m_Mutex.unlock();

	m_FileLoader = new CMassFileLoader(Storage(), CMassFileLoader::LOAD_FLAGS_ABSOLUTE_PATH | CMassFileLoader::LOAD_FLAGS_ASYNC | CMassFileLoader::LOAD_FLAGS_RECURSE_SUBDIRECTORIES);
	m_FileLoader->SetLoadFailedCallback(SkinLoadErrorCallback);
	m_FileLoader->SetFileLoadedCallback(SkinLoadedCallback);
	m_FileLoader->SetLoadFinishedCallback(SkinLoadFinishedCallback);
	m_FileLoader->SetPaths(":skins"); // Configurable?
	m_FileLoader->SetUserData(this);
	m_FileLoader->SetFileExtension(".png"); // Multiple in the future?
	m_FileLoader->Load();
}

bool CSkins::AllLocalSkinsLoaded()
{
	return !m_LoadingSkinsFromDisk && !m_Skins.empty();
}

int CSkins::LocalCount()
{
	return m_Skins.size();
}

int CSkins::TotalCount()
{
	return m_Skins.size() + m_DownloadSkins.size() + m_BaseSkins.size();
}

void CSkins::ClearSkins()
{
	UnloadSkins(m_Skins);
	m_Skins.clear();
	m_DownloadSkins.clear();
	m_ReadySkinTextures.clear();

	m_LoadingSkinsFromDisk = 1;
	m_DownloadingSkins = 0;
}

// TODO: fix finds
const CSkin *CSkins::Find(const char *pName)
{
	const auto *pSkin = FindOrNullptr(pName);
	if(pSkin == nullptr)
	{
		const auto it = m_BaseSkins.find(pName);
		if(it != m_BaseSkins.end())
		{
			pSkin = it->second.get(); // is a base skin; default, x_ninja, x_spec
		}
		else
		{
			pSkin = m_BaseSkins.find("default")->second.get();
			dbg_assert(bool(pSkin), "Base skins do not contain default skin.");
		}
	}
	return pSkin;
}

const CSkin *CSkins::FindOrNullptr(const char *pName)
{
	const char *pSkinPrefix = m_aEventSkinPrefix[0] ? m_aEventSkinPrefix : g_Config.m_ClSkinPrefix;
	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(pName))
	{
		return nullptr;
	}
	else if(pSkinPrefix && pSkinPrefix[0])
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
	auto SkinIt = m_BaseSkins.find(pName);
	if(SkinIt != m_BaseSkins.end())
		return SkinIt->second.get();

	SkinIt = m_Skins.find(pName);
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
		if(SkinDownloadIt->second->m_pTask && SkinDownloadIt->second->m_pTask->State() == HTTP_DONE)
		{
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "downloadedskins/%s.png", SkinDownloadIt->second->GetName());
			Storage()->RenameFile(SkinDownloadIt->second->m_aPath, aPath, IStorage::TYPE_SAVE);
			const auto *pSkin = LoadSkin(SkinDownloadIt->second->GetName(), SkinDownloadIt->second->m_pTask->m_Info, m_Skins);
			SkinDownloadIt->second->m_pTask = nullptr;
			--m_DownloadingSkins;
			return pSkin;
		}
		if(SkinDownloadIt->second->m_pTask && (SkinDownloadIt->second->m_pTask->State() == HTTP_ERROR || SkinDownloadIt->second->m_pTask->State() == HTTP_ABORTED))
		{
			SkinDownloadIt->second->m_pTask = nullptr;
			--m_DownloadingSkins;
		}
		return nullptr;
	}

	CSkinDownloader Skin{pName};

	char aUrl[IO_MAX_PATH_LENGTH];
	char aEscapedName[256];
	EscapeUrl(aEscapedName, sizeof(aEscapedName), pName);
	str_format(aUrl, sizeof(aUrl), "%s%s.png", g_Config.m_ClDownloadCommunitySkins != 0 ? g_Config.m_ClSkinCommunityDownloadUrl : g_Config.m_ClSkinDownloadUrl, aEscapedName);
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(Skin.m_aPath, sizeof(Skin.m_aPath), "downloadedskins/%s", IStorage::FormatTmpPath(aBuf, sizeof(aBuf), pName));
	Skin.m_pTask = std::make_shared<CGetPngFile>(this, aUrl, Storage(), Skin.m_aPath);
	m_pClient->Engine()->AddJob(Skin.m_pTask);
	auto &&pDownloadSkin = std::make_unique<CSkinDownloader>(std::move(Skin));
	m_DownloadSkins.insert({pDownloadSkin->GetName(), std::move(pDownloadSkin)});
	++m_DownloadingSkins;
	return nullptr;
}
