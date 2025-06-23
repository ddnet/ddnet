#ifndef GAME_CLIENT_COMPONENTS_COMMUNITY_ICONS_H
#define GAME_CLIENT_COMPONENTS_COMMUNITY_ICONS_H

#include <base/hash.h>

#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/http.h>
#include <engine/shared/jobs.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CCommunityIcon
{
	friend class CCommunityIcons;

private:
	char m_aCommunityId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	SHA256_DIGEST m_Sha256;
	IGraphics::CTextureHandle m_OrgTexture;
	IGraphics::CTextureHandle m_GreyTexture;
};

class CCommunityIcons : public CComponentInterfaces
{
public:
	const CCommunityIcon *Find(const char *pCommunityId);
	void Render(const CCommunityIcon *pIcon, CUIRect Rect, bool Active);
	void Load();
	void Update();
	void Shutdown();

private:
	class CAbstractCommunityIconJob
	{
	protected:
		CCommunityIcons *m_pCommunityIcons;
		char m_aCommunityId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
		char m_aPath[IO_MAX_PATH_LENGTH];
		int m_StorageType;
		bool m_Success = false;
		SHA256_DIGEST m_Sha256;

		CAbstractCommunityIconJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, int StorageType);

	public:
		const char *CommunityId() const { return m_aCommunityId; }
		bool Success() const { return m_Success; }
		const SHA256_DIGEST &Sha256() const { return m_Sha256; }
		virtual ~CAbstractCommunityIconJob() = default;
	};

	class CCommunityIconLoadJob : public IJob, public CAbstractCommunityIconJob
	{
		CImageInfo m_ImageInfo;
		CImageInfo m_ImageInfoGrayscale;

	protected:
		void Run() override;

	public:
		CCommunityIconLoadJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, int StorageType);
		~CCommunityIconLoadJob();

		CImageInfo &ImageInfo() { return m_ImageInfo; }
		CImageInfo &ImageInfoGrayscale() { return m_ImageInfoGrayscale; }
	};

	class CCommunityIconDownloadJob : public CHttpRequest, public CAbstractCommunityIconJob
	{
	public:
		CCommunityIconDownloadJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, const char *pUrl, const SHA256_DIGEST &Sha256);
	};

	std::vector<CCommunityIcon> m_vCommunityIcons;
	std::deque<std::shared_ptr<CCommunityIconLoadJob>> m_CommunityIconLoadJobs;
	std::deque<std::shared_ptr<CCommunityIconDownloadJob>> m_CommunityIconDownloadJobs;
	SHA256_DIGEST m_CommunityIconsInfoSha256 = SHA256_ZEROED;
	static int FileScan(const char *pName, int IsDir, int DirType, void *pUser);
	bool LoadFile(const char *pPath, int DirType, CImageInfo &Info, CImageInfo &InfoGrayscale, SHA256_DIGEST &Sha256);
	void LoadFinish(const char *pCommunityId, CImageInfo &Info, CImageInfo &InfoGrayscale, const SHA256_DIGEST &Sha256);
};

#endif
