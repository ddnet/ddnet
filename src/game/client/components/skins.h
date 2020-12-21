/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS_H
#define GAME_CLIENT_COMPONENTS_SKINS_H
#include <base/color.h>
#include <base/tl/sorted_array.h>
#include <base/vmath.h>
#include <engine/client/http.h>
#include <game/client/component.h>
#include <game/client/skin.h>

class CSkins : public CComponent
{
public:
	class CGetPngFile : public CGetFile
	{
		CSkins *m_pSkins;

	protected:
		virtual int OnCompletion(int State);

	public:
		CGetPngFile(CSkins *pSkins, IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType = -2, CTimeout Timeout = CTimeout{4000, 500, 5}, bool LogProgress = true);
		CImageInfo m_Info;
	};

	struct CDownloadSkin
	{
		std::shared_ptr<CSkins::CGetPngFile> m_pTask;
		char m_aPath[MAX_PATH_LENGTH];
		char m_aName[24];

		bool operator<(const CDownloadSkin &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<(const char *pOther) const { return str_comp(m_aName, pOther) < 0; }
		bool operator==(const char *pOther) const { return !str_comp(m_aName, pOther); }
	};

	void OnInit();

	void Refresh();
	int Num();
	const CSkin *Get(int Index);
	int Find(const char *pName);

private:
	sorted_array<CSkin> m_aSkins;
	sorted_array<CDownloadSkin> m_aDownloadSkins;
	char m_EventSkinPrefix[24];

	bool LoadSkinPNG(CImageInfo &Info, const char *pName, const char *pPath, int DirType);
	int LoadSkin(const char *pName, const char *pPath, int DirType);
	int LoadSkin(const char *pName, CImageInfo &Info);
	int FindImpl(const char *pName);
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);
};
#endif
