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
		virtual int OnCompletion(int State) override;

	public:
		CGetPngFile(CSkins *pSkins, const char *pUrl, IStorage *pStorage, const char *pDest);
		CImageInfo m_Info;
	};

	class ISkinLoader
	{ // IGetSkin defines the interface of an object that will eventually return a skin PNG
	public:
		virtual ISkinLoader *Copy(ISkinLoader &&Other) = 0; // We cannot declare a constructor as virtual so we have to employ virtual constructor idioms

		virtual bool operator<(const ISkinLoader &Other) const = 0;
		virtual bool operator<(const char *pOther) const = 0;
		virtual bool operator==(const char *pOther) const = 0;
		//		virtual ISkinLoader &operator=(ISkinLoader &&Other);
		virtual const char *GetName() const = 0;
		char m_aName[24];

	protected:
	};

	//	struct CLocalSkinLoader : ISkinLoader // Load skin from disk
	//	{
	//	public:
	//		CLocalSkinLoader(CLocalSkinLoader &&Other) = delete;
	//		CLocalSkinLoader(const char *pName)
	//		{
	//			str_copy(m_aName, pName);
	//		}

	//		~CLocalSkinLoader()
	//		{
	//			if(m_pTask)
	//				m_pTask->Abort();
	//		}
	//		bool operator<(const CLocalSkinLoader &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
	//		bool operator<(const char *pOther) const { return str_comp(m_aName, pOther) < 0; }
	//		bool operator==(const char *pOther) const { return !str_comp(m_aName, pOther); }

	//		CLocalSkinLoader &operator=(CLocalSkinLoader &&Other) = default;

	//		const char *GetName() const { return m_aName; }
	//	};

	struct CSkinDownloader : ISkinLoader // Load skin from the interwebs
	{
	private:
	public:
		std::shared_ptr<CSkins::CGetPngFile> m_pTask;
		char m_aPath[IO_MAX_PATH_LENGTH];

		CSkinDownloader(const char *pName)
		{
			str_copy(m_aName, pName);
		}

		CSkinDownloader(CSkinDownloader &&Other) = default;
		ISkinLoader *Copy(ISkinLoader &&Other) override
		{
			return new CSkinDownloader(reinterpret_cast<CSkinDownloader &&>(Other));
			// CSkinDownloader(CSkinDownloader &&Other) = default;
		}

		~CSkinDownloader()
		{
			if(m_pTask)
				m_pTask->Abort();
		}
		bool operator<(const ISkinLoader &Other) const override { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<(const char *pOther) const override { return str_comp(m_aName, pOther) < 0; }
		bool operator==(const char *pOther) const override { return !str_comp(m_aName, pOther); }

		CSkinDownloader &operator=(CSkinDownloader &&Other) = default;

		const char *GetName() const override { return m_aName; }
	};

	typedef std::function<void(int)> TSkinLoadedCBFunc;

	virtual int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;

	void Refresh(TSkinLoadedCBFunc &&SkinLoadedFunc);
	int Num();
	std::unordered_map<std::string_view, std::unique_ptr<CSkin>> &GetSkinsUnsafe() { return m_Skins; }
	const CSkin *FindOrNullptr(const char *pName);
	const CSkin *Find(const char *pName);

	bool IsDownloadingSkins() { return m_DownloadingSkins; }

	static bool IsVanillaSkin(const char *pName);

	constexpr static const char *VANILLA_SKINS[] = {"bluekitty", "bluestripe", "brownbear",
		"cammo", "cammostripes", "coala", "default", "limekitty",
		"pinky", "redbopp", "redstripe", "saddo", "toptri",
		"twinbop", "twintri", "warpaint", "x_ninja", "x_spec"};

private:
	std::unordered_map<std::string_view, std::unique_ptr<CSkin>> m_Skins;
	std::unordered_map<std::string_view, std::unique_ptr<CSkinDownloader>> m_DownloadSkins;
	size_t m_DownloadingSkins = 0;
	char m_aEventSkinPrefix[24];

	bool LoadSkinPNG(CImageInfo &Info, const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, const char *pPath, int DirType);
	const CSkin *LoadSkin(const char *pName, CImageInfo &Info);
	const CSkin *FindImpl(const char *pName);
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);
};
#endif
