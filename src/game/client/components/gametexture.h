/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_GSKINS_H
#define GAME_CLIENT_COMPONENTS_GSKINS_H
#include <queue>
#include <base/vmath.h>
#include <base/tl/sorted_array.h>
#include <game/client/component.h>

class CGameTextureManager : public CComponent
{
	struct CLoadHelper
	{
		CLoadHelper(CGameTextureManager *pSelf, int ScanType) : m_pSelf(pSelf), m_ScanType(ScanType) { }
		CGameTextureManager *m_pSelf;
		int m_ScanType;
	};

	int MapImageToGroup(int Image) const;
	int MapGroupToImage(int Group) const;

public:
	enum
	{
		TEXTURE_GROUP_GAME=0,
		TEXTURE_GROUP_CURSOR,
		TEXTURE_GROUP_EMOTE,
		TEXTURE_GROUP_PARTICLES,
		//TEXTURE_GROUP_ENTITIES,
		NUM_TEXTURE_GROUPS
	};

	class CGameSkin
	{
		friend class CGameTextureManager;
		int m_Texture;

	public:
		CGameSkin() {}
		CGameSkin(const class CDataImage& Image);
		char m_aName[128];
		int Texture() const { return m_Texture; }

		bool operator<(const CGameSkin& Other) { return str_comp(m_aName, Other.m_aName) < 0; }

	};

	void OnInit();
	void OnReset();

	int SetTexture(int Image, const char *pName);
	int FindTexture(int Group, const char *pName);
	const sorted_array<CGameSkin>& GetGroup(int Group) { return m_aSkins[Group]; }

private:
	sorted_array<CGameSkin> m_aSkins[NUM_TEXTURE_GROUPS];

	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);
};

#endif
