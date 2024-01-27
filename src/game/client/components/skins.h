/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS_H
#define GAME_CLIENT_COMPONENTS_SKINS_H

#include <base/system.h>
#include <engine/shared/http.h>
#include <game/client/component.h>
#include <game/client/skin.h>
#include <string_view>
#include <unordered_map>

class CSkins : public CComponent
{
public:
	CSkins() = default;

	class CGetPngFile : public CHttpRequest
	{
		CSkins *m_pSkins;

	protected:
		virtual void OnCompletion(EHttpState State) override;

	public:
		CGetPngFile(CSkins *pSkins, const char *pUrl, IStorage *pStorage, const char *pDest);
		CImageInfo m_Info;
	};

	struct CDownloadSkin
	{
	private:
		char m_aName[24];

	public:
		std::shared_ptr<CSkins::CGetPngFile> m_pTask;
		char m_aPath[IO_MAX_PATH_LENGTH];

		CDownloadSkin(CDownloadSkin &&Other) = default;
		CDownloadSkin(const char *pName)
		{
			str_copy(m_aName, pName);
		}

		~CDownloadSkin()
		{
			if(m_pTask)
				m_pTask->Abort();
		}
		bool operator<(const CDownloadSkin &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<(const char *pOther) const { return str_comp(m_aName, pOther) < 0; }
		bool operator==(const char *pOther) const { return !str_comp(m_aName, pOther); }

		CDownloadSkin &operator=(CDownloadSkin &&Other) = default;

		const char *GetName() const { return m_aName; }
	};

	typedef std::function<void(int)> TSkinLoadedCBFunc;

	virtual int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;

	void Refresh(TSkinLoadedCBFunc &&SkinLoadedFunc);
	int Num();
	std::unordered_map<std::string_view, std::unique_ptr<CSkin>> &GetSkinsUnsafe() { return m_Skins; }
	const CSkin *FindOrNullptr(const char *pName, bool IgnorePrefix = false);
	const CSkin *Find(const char *pName);

	bool IsDownloadingSkins() { return m_DownloadingSkins; }

	static bool IsVanillaSkin(const char *pName);

	constexpr static const char *VANILLA_SKINS[] = {"bluekitty", "bluestripe", "brownbear",
		"cammo", "cammostripes", "coala", "default", "limekitty",
		"pinky", "redbopp", "redstripe", "saddo", "toptri",
		"twinbop", "twintri", "warpaint", "x_ninja", "x_spec"};

private:
	std::unordered_map<std::string_view, std::unique_ptr<CSkin>> m_Skins;
	std::unordered_map<std::string_view, std::unique_ptr<CDownloadSkin>> m_DownloadSkins;
	size_t m_DownloadingSkins = 0;
	char m_aEventSkinPrefix[24];

	bool LoadSkinPNG(CImageInfo &Info, const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, CImageInfo &Info);
	const CSkin *FindImpl(const char *pName);
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);
};
#endif
