/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS_H
#define GAME_CLIENT_COMPONENTS_SKINS_H

#include <base/system.h>
#include <engine/shared/file_loader.h>
#include <engine/shared/http.h>
#include <game/client/component.h>
#include <game/client/skin.h>
#include <mutex>
#include <string_view>
#include <unordered_map>

// Reads from or writes to any data inside this class will ideally be thread-safe. The best way to do this is by using the public mutex (m_Mutex)
class CSkins : public CComponent
{
public:
	CSkins() = default;
	//	virtual ~CSkins();

	class CGetPngFile : public CHttpRequest
	{
		CSkins *m_pSkins;

	protected:
		virtual int OnCompletion(int State) override;

	public:
		CGetPngFile(CSkins *pSkins, const char *pUrl, IStorage *pStorage, const char *pDest);
		CImageInfo m_Info;
	};

	struct CSkinDownloader // Load skin from the interwebs
	{
	private:
	public:
		std::shared_ptr<CSkins::CGetPngFile> m_pTask;
		char m_aPath[IO_MAX_PATH_LENGTH];
		char m_aName[24];

		CSkinDownloader(CSkinDownloader &&Other) = default;
		CSkinDownloader(const char *pName)
		{
			str_copy(m_aName, pName);
		}

		~CSkinDownloader()
		{
			if(m_pTask)
				m_pTask->Abort();
		}
		bool operator<(const CSkinDownloader &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<(const char *pOther) const { return str_comp(m_aName, pOther) < 0; }
		bool operator==(const char *pOther) const { return !str_comp(m_aName, pOther); }

		CSkinDownloader &operator=(CSkinDownloader &&Other) = default;

		const char *GetName() const { return m_aName; }
	};

	// This is the mutex that is used to only allow access to the skins data at the appropriate times.
	std::mutex m_Mutex;

	// Component-related
	virtual int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;

	// This triggers a skin refresh.
	void Refresh();

	// Every member after this point should be locked when using.

	// Is a skin refresh finished?
	bool AllLocalSkinsLoaded();

	// Returns number of non-base, non-downloaded skins.
	int LocalCount();

	// Returns total number of all skins.
	int TotalCount();

	// Remove all skins. Only used internally but may be helpful elsewhere
	void ClearSkins();

	// TODO: Make safe
	std::unordered_map<std::string, std::unique_ptr<CSkin>> &GetSkinsUnsafe() { return m_Skins; }

	// Attempts to find a skin. If not present, return nullptr
	const CSkin *FindOrNullptr(const char *pName);

	// Attempts to find a skin. If not present, return a pointer to the default skin
	const CSkin *Find(const char *pName);

	// Is a skin download currently in progress?
	bool IsDownloadingSkins() { return m_DownloadingSkins; }
	// Is the given skin a vanilla skin?
	static bool IsVanillaSkin(const char *pName);
	// Is there already a skin with the given name?
	bool IsDuplicateSkin(const char *pName);

	constexpr static const char *BASE_SKINS[] = {"default", "x_ninja", "x_spec"};
	constexpr static const char *VANILLA_SKINS[] = {"bluekitty", "bluestripe", "brownbear",
		"cammo", "cammostripes", "coala", /*"default", */ "limekitty",
		"pinky", "redbopp", "redstripe", "saddo", "toptri",
		"twinbop", "twintri", "warpaint",
		/*"x_ninja", "x_spec"*/};

private:
	template<typename T>
	using SkinsContainer = std::unordered_map<std::string, std::unique_ptr<T>>; // std::string was previously a std::string_view. However, introducing another thread means the lifetime of the original string isn't guaranteed when the string view is accessed, so the container must own it.

	char m_aEventSkinPrefix[24];
	SkinsContainer<CSkin> m_BaseSkins; // Any skin required for the game to work right (default, x_ninja, x_spec). Should not change throughout its lifetime
	CMassFileLoader *m_pFileLoader = nullptr; // This is initialized & only used in Refresh()

	static void SkinLoadedCallback(const std::string_view ItemName, const unsigned char *pData, unsigned int Size, void *pUser);
	static bool SkinLoadErrorCallback(CMassFileLoader::ELoadError Error, const void *pData, void *pUser);
	static void SkinLoadFinishedCallback(unsigned int Count, void *pUser);

	// Every member after this point should be locked when using.

	SkinsContainer<CSkin> m_Skins; // Any skin that is not downloaded or a base skin.
	SkinsContainer<CSkinDownloader> m_DownloadSkins; // Skins download via. HTTP
	SkinsContainer<CImageInfo> m_ReadySkinTextures; // Skins for m_Skins that have been loaded from disk on the file loader thread, but have not yet had their textures uploaded to the GPU by the main thread. This is managed in OnRender()

	template<typename T>
	void UnloadSkins(const SkinsContainer<T> &Skins)
	{
		for(const auto &It : Skins)
		{
			const auto &pSkin = It.second;
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
	}

	const CSkin *FindImpl(const char *pName);

	// why not bool? i'm leaving it
	size_t
		// Downloading skins via. HTTP
		m_DownloadingSkins = 0,
		// Loading skins from disk
		m_LoadingSkinsFromDisk = 0;

	// The following functions make use of IGraphics * and therefore can only be safely called from the main thread. Lock when using!
	bool LoadSkinPNG(CImageInfo &Info, const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, CImageInfo &Info, SkinsContainer<CSkin> &Container);
};
#endif
