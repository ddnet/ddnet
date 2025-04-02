/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS_H
#define GAME_CLIENT_COMPONENTS_SKINS_H

#include <base/lock.h>

#include <engine/shared/config.h>
#include <engine/shared/jobs.h>

#include <game/client/component.h>
#include <game/client/skin.h>

#include <chrono>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>

class CHttpRequest;

class CSkins : public CComponent
{
public:
	class CSkinListEntry
	{
	public:
		const CSkin *m_pSkin;
		bool m_Favorite;

		CSkinListEntry() :
			m_pSkin(nullptr),
			m_Favorite(false) {}
		CSkinListEntry(const CSkin *pSkin, bool Favorite) :
			m_pSkin(pSkin),
			m_Favorite(Favorite) {}

		bool operator<(const CSkinListEntry &Other) const;

		const void *ListItemId() const { return &m_pSkin; }
		const void *FavoriteButtonId() const { return &m_Favorite; }
	};

	CSkins();

	typedef std::function<void()> TSkinLoadedCallback;

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;

	void Refresh(TSkinLoadedCallback &&SkinLoadedCallback);
	std::chrono::nanoseconds LastRefreshTime() const { return m_LastRefreshTime; }

	const std::vector<CSkinListEntry> &SkinList();
	void ForceRefreshSkinList();

	const CSkin *FindOrNullptr(const char *pName, bool IgnorePrefix = false);
	const CSkin *Find(const char *pName);

	void AddFavorite(const char *pName);
	void RemoveFavorite(const char *pName);
	bool IsFavorite(const char *pName) const;

	void RandomizeSkin(int Dummy);

	static bool IsVanillaSkin(const char *pName);
	static bool IsSpecialSkin(const char *pName);

private:
	constexpr static const char *VANILLA_SKINS[] = {"bluekitty", "bluestripe", "brownbear",
		"cammo", "cammostripes", "coala", "default", "limekitty",
		"pinky", "redbopp", "redstripe", "saddo", "toptri",
		"twinbop", "twintri", "warpaint", "x_ninja", "x_spec"};

	class CSkinDownloadJob : public IJob
	{
	public:
		CSkinDownloadJob(CSkins *pSkins, const char *pName);
		~CSkinDownloadJob();

		bool Abort() override REQUIRES(!m_Lock);

		CImageInfo &ImageInfo() { return m_ImageInfo; }

	protected:
		void Run() override REQUIRES(!m_Lock);

	private:
		CSkins *m_pSkins;
		char m_aName[MAX_SKIN_LENGTH];
		CLock m_Lock;
		std::shared_ptr<CHttpRequest> m_pGetRequest;
		CImageInfo m_ImageInfo;
	};

	class CLoadingSkin
	{
	private:
		char m_aName[MAX_SKIN_LENGTH];

	public:
		std::shared_ptr<CSkinDownloadJob> m_pDownloadJob = nullptr;

		CLoadingSkin(CLoadingSkin &&Other) = default;
		CLoadingSkin(const char *pName);
		~CLoadingSkin();

		bool operator<(const CLoadingSkin &Other) const;
		bool operator<(const char *pOther) const;
		bool operator==(const char *pOther) const;

		CLoadingSkin &operator=(CLoadingSkin &&Other) = default;

		const char *Name() const { return m_aName; }
	};

	std::unordered_map<std::string_view, std::unique_ptr<CSkin>> m_Skins;

	std::unordered_map<std::string_view, std::unique_ptr<CLoadingSkin>> m_LoadingSkins;
	std::chrono::nanoseconds m_LastRefreshTime;

	std::vector<CSkinListEntry> m_vSkinList;
	std::optional<std::chrono::nanoseconds> m_SkinListLastRefreshTime;

	std::set<std::string> m_Favorites;

	CSkin m_PlaceholderSkin;
	char m_aEventSkinPrefix[MAX_SKIN_LENGTH];

	const CSkin *LoadSkin(const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, CImageInfo &Info);
	const CSkin *FindImpl(const char *pName);
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);

	static void ConAddFavoriteSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConRemFavoriteSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
	void OnConfigSave(IConfigManager *pConfigManager);
};
#endif
