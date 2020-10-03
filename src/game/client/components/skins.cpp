/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <math.h>

#include <base/math.h>
#include <base/system.h>
#include <ctime>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "skins.h"

static const char *VANILLA_SKINS[] = {"bluekitty", "bluestripe", "brownbear",
	"cammo", "cammostripes", "coala", "default", "limekitty",
	"pinky", "redbopp", "redstripe", "saddo", "toptri",
	"twinbop", "twintri", "warpaint", "x_ninja", "x_spec"};

static bool IsVanillaSkin(const char *pName)
{
	for(unsigned int i = 0; i < sizeof(VANILLA_SKINS) / sizeof(VANILLA_SKINS[0]); i++)
	{
		if(str_comp(pName, VANILLA_SKINS[i]) == 0)
		{
			return true;
		}
	}
	return false;
}

int CSkins::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CSkins *pSelf = (CSkins *)pUser;

	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	char aNameWithoutPng[128];
	str_copy(aNameWithoutPng, pName, sizeof(aNameWithoutPng));
	aNameWithoutPng[str_length(aNameWithoutPng) - 4] = 0;

	// Don't add duplicate skins (one from user's config directory, other from
	// client itself)
	for(int i = 0; i < pSelf->Num(); i++)
	{
		const char *pExName = pSelf->Get(i)->m_aName;
		if(str_comp(pExName, aNameWithoutPng) == 0)
			return 0;
	}

	char aBuf[MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "skins/%s", pName);
	return pSelf->LoadSkin(aNameWithoutPng, aBuf, DirType);
}

int CSkins::LoadSkin(const char *pName, const char *pPath, int DirType)
{
	char aBuf[512];
	CImageInfo Info;
	if(!Graphics()->LoadPNG(&Info, pPath, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load skin from %s", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
		return 0;
	}

	CSkin Skin;
	Skin.m_IsVanilla = IsVanillaSkin(pName);
	Skin.m_OrgTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);

	int BodySize = 96; // body size
	if(BodySize > Info.m_Height)
		return 0;
	unsigned char *d = (unsigned char *)Info.m_pData;
	int Pitch = Info.m_Width * 4;

	// dig out blood color
	{
		int aColors[3] = {0};
		for(int y = 0; y < BodySize; y++)
			for(int x = 0; x < BodySize; x++)
			{
				if(d[y * Pitch + x * 4 + 3] > 128)
				{
					aColors[0] += d[y * Pitch + x * 4 + 0];
					aColors[1] += d[y * Pitch + x * 4 + 1];
					aColors[2] += d[y * Pitch + x * 4 + 2];
				}
			}

		Skin.m_BloodColor = ColorRGBA(normalize(vec3(aColors[0], aColors[1], aColors[2])));
	}

	// create colorless version
	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	// make the texture gray scale
	for(int i = 0; i < Info.m_Width * Info.m_Height; i++)
	{
		int v = (d[i * Step] + d[i * Step + 1] + d[i * Step + 2]) / 3;
		d[i * Step] = v;
		d[i * Step + 1] = v;
		d[i * Step + 2] = v;
	}

	int Freq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequence
	for(int y = 0; y < BodySize; y++)
		for(int x = 0; x < BodySize; x++)
		{
			if(d[y * Pitch + x * 4 + 3] > 128)
				Freq[d[y * Pitch + x * 4]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(Freq[OrgWeight] < Freq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255 - OrgWeight;
	int InvNewWeight = 255 - NewWeight;
	for(int y = 0; y < BodySize; y++)
		for(int x = 0; x < BodySize; x++)
		{
			int v = d[y * Pitch + x * 4];
			if(v <= OrgWeight)
				v = (int)(((v / (float)OrgWeight) * NewWeight));
			else
				v = (int)(((v - OrgWeight) / (float)InvOrgWeight) * InvNewWeight + NewWeight);
			d[y * Pitch + x * 4] = v;
			d[y * Pitch + x * 4 + 1] = v;
			d[y * Pitch + x * 4 + 2] = v;
		}

	Skin.m_ColorTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	free(Info.m_pData);

	// set skin data
	str_copy(Skin.m_aName, pName, sizeof(Skin.m_aName));
	if(g_Config.m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "load skin %s", Skin.m_aName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "game", aBuf);
	}
	m_aSkins.add(Skin);

	return 0;
}

void CSkins::OnInit()
{
	m_EventSkinPrefix[0] = '\0';

	if(g_Config.m_Events)
	{
		time_t rawtime;
		struct tm *timeinfo;
		std::time(&rawtime);
		timeinfo = localtime(&rawtime);
		if(timeinfo->tm_mon == 11 && timeinfo->tm_mday >= 24 && timeinfo->tm_mday <= 26)
		{ // Christmas
			str_copy(m_EventSkinPrefix, "santa", sizeof(m_EventSkinPrefix));
		}
	}

	// load skins
	m_aSkins.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "skins", SkinScan, this);
	if(!m_aSkins.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load skins. folder='skins/'");
		CSkin DummySkin;
		DummySkin.m_IsVanilla = true;
		str_copy(DummySkin.m_aName, "dummy", sizeof(DummySkin.m_aName));
		DummySkin.m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f);
		m_aSkins.add(DummySkin);
	}
}

int CSkins::Num()
{
	return m_aSkins.size();
}

const CSkins::CSkin *CSkins::Get(int Index)
{
	if(Index < 0)
	{
		Index = Find("default");

		if(Index < 0)
			Index = 0;
	}
	return &m_aSkins[Index % m_aSkins.size()];
}

int CSkins::Find(const char *pName)
{
	const char *pSkinPrefix = m_EventSkinPrefix[0] ? m_EventSkinPrefix : g_Config.m_ClSkinPrefix;
	if(g_Config.m_ClVanillaSkinsOnly && !IsVanillaSkin(pName))
	{
		return -1;
	}
	else if(pSkinPrefix && pSkinPrefix[0])
	{
		char aBuf[24];
		str_format(aBuf, sizeof(aBuf), "%s_%s", pSkinPrefix, pName);
		// If we find something, use it, otherwise fall back to normal skins.
		int Result = FindImpl(aBuf);
		if(Result != -1)
		{
			return Result;
		}
	}
	return FindImpl(pName);
}

int CSkins::FindImpl(const char *pName)
{
	auto r = ::find_binary(m_aSkins.all(), pName);
	if(!r.empty())
		return &r.front() - m_aSkins.base_ptr();

	if(str_comp(pName, "default") == 0)
		return -1;

	if(!g_Config.m_ClDownloadSkins)
		return -1;

	if(str_find(pName, "/") != 0)
		return -1;

	auto d = ::find_binary(m_aDownloadSkins.all(), pName);
	if(!d.empty())
	{
		if(d.front().m_pTask && d.front().m_pTask->State() == HTTP_DONE)
		{
			char aPath[MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "downloadedskins/%s.png", d.front().m_aName);
			Storage()->RenameFile(d.front().m_aPath, aPath, IStorage::TYPE_SAVE);
			LoadSkin(d.front().m_aName, aPath, IStorage::TYPE_SAVE);
			d.front().m_pTask = nullptr;
		}
		if(d.front().m_pTask && (d.front().m_pTask->State() == HTTP_ERROR || d.front().m_pTask->State() == HTTP_ABORTED))
		{
			d.front().m_pTask = nullptr;
		}
		return -1;
	}

	CDownloadSkin Skin;
	str_copy(Skin.m_aName, pName, sizeof(Skin.m_aName));

	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%s%s.png", g_Config.m_ClSkinDownloadUrl, pName);
	str_format(Skin.m_aPath, sizeof(Skin.m_aPath), "downloadedskins/%s.%d.tmp", pName, pid());
	Skin.m_pTask = std::make_shared<CGetFile>(Storage(), aUrl, Skin.m_aPath, IStorage::TYPE_SAVE, CTimeout{0, 0, 0}, false);
	m_pClient->Engine()->AddJob(Skin.m_pTask);
	m_aDownloadSkins.add(Skin);
	return -1;
}
