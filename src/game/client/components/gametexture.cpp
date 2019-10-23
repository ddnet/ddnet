#include <base/system.h>
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>

#include "gametexture.h"

static const char *ms_pTextureDirs[] = {
		"game",
		"cursor",
		"emotes",
		"particles",
		//"entities"
};


CGameTextureManager::CGameSkin::CGameSkin(const CDataImage &Image)
{
	m_Texture = Image.m_Id;
	const char *pName = str_find_rev(Image.m_pFilename, "/");
	if(!pName)
		pName = Image.m_pFilename;
	str_copy(m_aName, pName, max(0, min((int)sizeof(m_aName), str_length(pName)-3)));
}


int CGameTextureManager::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	int l = str_length(pName);
	if(pName[0] == '!' || l < 4 || IsDir || str_comp(pName+l-4, ".png") != 0)
		return 0;

	CLoadHelper *pData = (CLoadHelper *)pUser;
	CGameTextureManager *pSelf = pData->m_pSelf;

	char aPath[512];
	str_format(aPath, sizeof(aPath), "data/textures/%s/%s", ms_pTextureDirs[pData->m_ScanType], pName);

	bool IsUsed = ((pData->m_ScanType == TEXTURE_GROUP_GAME && str_comp(pName, g_Config.m_TexGame) == 0) ||
		(pData->m_ScanType == TEXTURE_GROUP_CURSOR && str_comp(pName, g_Config.m_TexCursor) == 0) ||
		(pData->m_ScanType == TEXTURE_GROUP_EMOTE && str_comp(pName, g_Config.m_TexEmoticons) == 0) ||
		(pData->m_ScanType == TEXTURE_GROUP_PARTICLES && str_comp(pName, g_Config.m_TexParticles) == 0));
				//(pData->m_ScanType == TEXTURE_GROUP_ENTITIES && str_comp(pName, g_Config.m_TexEntities) == 0));


	// set skin data
	CGameSkin Skin;
	if(g_Config.m_TexLazyLoading && !IsUsed)
		Skin.m_Texture = 0;
	else
		Skin.m_Texture = pSelf->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	str_copy(Skin.m_aName, pName, min((int)sizeof(Skin.m_aName),l-3));
	pSelf->m_aSkins[pData->m_ScanType].add(Skin);


	if(g_Config.m_Debug)
		pSelf->Console()->Printf(IConsole::OUTPUT_LEVEL_ADDINFO, "game", "loading %s-texture %s", ms_pTextureDirs[pData->m_ScanType], pName);

	return 0;
}

void CGameTextureManager::OnInit()
{
	// load skins
	for(int i = 0; i < NUM_TEXTURE_GROUPS; i++)
	{
		m_aSkins[i].clear();

		// set default skin data
		CGameSkin DefaultSkin(g_pData->m_aImages[MapGroupToImage(i)]);
		m_aSkins[i].add(DefaultSkin);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "data/textures/%s", ms_pTextureDirs[i]);
		CLoadHelper LoadHelper(this, i);
		Storage()->ListDirectory(IStorage::TYPE_ALL, aBuf, SkinScan, &LoadHelper);
		if(m_aSkins[i].empty())
		{
			Console()->Printf(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load textures. folder='textures/%s'", ms_pTextureDirs[i]);
			CGameSkin DummySkin;
			DummySkin.m_Texture = -1;
			str_copy(DummySkin.m_aName, "dummy", sizeof(DummySkin.m_aName));
			m_aSkins[i].add(DummySkin);
		}
	}
}

void CGameTextureManager::OnReset()
{
	SetTexture(IMAGE_GAME, g_Config.m_TexGame);
	SetTexture(IMAGE_CURSOR, g_Config.m_TexCursor);
	SetTexture(IMAGE_EMOTICONS, g_Config.m_TexEmoticons);
	SetTexture(IMAGE_PARTICLES, g_Config.m_TexParticles);
}

int CGameTextureManager::SetTexture(int Image, const char *pName)
{
	int Group = MapImageToGroup(Image);
	dbg_assert(Group >= 0 && Group < NUM_TEXTURE_GROUPS, "CGameTextureManager::SetTexture invalid group");

	int result = g_pData->m_aImages[Image].m_Id = FindTexture(Group, pName);

	if(g_Config.m_Debug)
		dbg_msg("gametexture/debug", "settings texture %i <- %i(%s) : %i('%s')", Image, Group, ms_pTextureDirs[Group], result, pName);

	return result;
}

int CGameTextureManager::FindTexture(int Group, const char* pName)
{
	for (int i = 0; i < m_aSkins[Group].size(); i++)
	{
		if (str_comp(m_aSkins[Group][i].m_aName, pName) == 0)
		{
			if (m_aSkins[Group][i].m_Texture <= 0)
			{
				char aPath[512];
				str_format(aPath, sizeof(aPath), "textures/%s/%s.png", ms_pTextureDirs[Group], pName);
				if ((m_aSkins[Group][i].m_Texture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0)) <= 0)
					dbg_msg("gametexture/error", "failed to load '%s'", aPath);
				else if (g_Config.m_Debug)
					dbg_msg("gametexture/debug", "loaded texture '%s'", aPath);

			}
			return m_aSkins[Group][i].m_Texture;
		}
	}
	return 0;
}

int CGameTextureManager::MapImageToGroup(int Image) const
{
	int Group = Image == IMAGE_GAME ? TEXTURE_GROUP_GAME :
		Image == IMAGE_CURSOR ? TEXTURE_GROUP_CURSOR :
		Image == IMAGE_EMOTICONS ? TEXTURE_GROUP_EMOTE :
		Image == IMAGE_PARTICLES ? TEXTURE_GROUP_PARTICLES : -1;
				//Image == IMAGE_ENTITIES ? TEXTURE_GROUP_ENTITIES : -1;
	return Group;
}

int CGameTextureManager::MapGroupToImage(int Group) const
{
	int Image = Group == TEXTURE_GROUP_GAME ? IMAGE_GAME :
		Group == TEXTURE_GROUP_CURSOR ? IMAGE_CURSOR :
		Group == TEXTURE_GROUP_EMOTE ? IMAGE_EMOTICONS :
		Group == TEXTURE_GROUP_PARTICLES ? IMAGE_PARTICLES : IMAGE_NULL;
				//Group == TEXTURE_GROUP_ENTITIES ? IMAGE_ENTITIES : IMAGE_NULL;
	return Image;
}
