#include <base/log.h>

#include <engine/engine.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/storage.h>

#include "community_icons.h"

CCommunityIcons::CAbstractCommunityIconJob::CAbstractCommunityIconJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, int StorageType) :
	m_pCommunityIcons(pCommunityIcons),
	m_StorageType(StorageType)
{
	str_copy(m_aCommunityId, pCommunityId);
	str_format(m_aPath, sizeof(m_aPath), "communityicons/%s.png", pCommunityId);
}

CCommunityIcons::CCommunityIconDownloadJob::CCommunityIconDownloadJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, const char *pUrl, const SHA256_DIGEST &Sha256) :
	CHttpRequest(pUrl),
	CAbstractCommunityIconJob(pCommunityIcons, pCommunityId, IStorage::TYPE_SAVE)
{
	WriteToFile(pCommunityIcons->Storage(), m_aPath, IStorage::TYPE_SAVE);
	ExpectSha256(Sha256);
	Timeout(CTimeout{0, 0, 0, 0});
	LogProgress(HTTPLOG::FAILURE);
}

void CCommunityIcons::CCommunityIconLoadJob::Run()
{
	m_Success = m_pCommunityIcons->LoadFile(m_aPath, m_StorageType, m_ImageInfo, m_ImageInfoGrayscale, m_Sha256);
}

CCommunityIcons::CCommunityIconLoadJob::CCommunityIconLoadJob(CCommunityIcons *pCommunityIcons, const char *pCommunityId, int StorageType) :
	CAbstractCommunityIconJob(pCommunityIcons, pCommunityId, StorageType)
{
	Abortable(true);
}

CCommunityIcons::CCommunityIconLoadJob::~CCommunityIconLoadJob()
{
	m_ImageInfo.Free();
	m_ImageInfoGrayscale.Free();
}

int CCommunityIcons::FileScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	const char *pExtension = ".png";
	CCommunityIcons *pSelf = static_cast<CCommunityIcons *>(pUser);
	if(IsDir || !str_endswith(pName, pExtension) || str_length(pName) - str_length(pExtension) >= (int)CServerInfo::MAX_COMMUNITY_ID_LENGTH)
		return 0;

	char aCommunityId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	str_truncate(aCommunityId, sizeof(aCommunityId), pName, str_length(pName) - str_length(pExtension));

	std::shared_ptr<CCommunityIconLoadJob> pJob = std::make_shared<CCommunityIconLoadJob>(pSelf, aCommunityId, DirType);
	pSelf->Engine()->AddJob(pJob);
	pSelf->m_CommunityIconLoadJobs.push_back(pJob);
	return 0;
}

const CCommunityIcon *CCommunityIcons::Find(const char *pCommunityId)
{
	auto Icon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [pCommunityId](const CCommunityIcon &Element) {
		return str_comp(Element.m_aCommunityId, pCommunityId) == 0;
	});
	return Icon == m_vCommunityIcons.end() ? nullptr : &(*Icon);
}

bool CCommunityIcons::LoadFile(const char *pPath, int DirType, CImageInfo &Info, CImageInfo &InfoGrayscale, SHA256_DIGEST &Sha256)
{
	if(!Graphics()->LoadPng(Info, pPath, DirType))
	{
		log_error("menus/browser", "Failed to load community icon from '%s'", pPath);
		return false;
	}
	if(Info.m_Format != CImageInfo::FORMAT_RGBA)
	{
		Info.Free();
		log_error("menus/browser", "Failed to load community icon from '%s': must be an RGBA image", pPath);
		return false;
	}
	if(!Storage()->CalculateHashes(pPath, DirType, &Sha256))
	{
		Info.Free();
		log_error("menus/browser", "Failed to load community icon from '%s': could not calculate hash", pPath);
		return false;
	}
	InfoGrayscale = Info.DeepCopy();
	ConvertToGrayscale(InfoGrayscale);
	return true;
}

void CCommunityIcons::LoadFinish(const char *pCommunityId, CImageInfo &Info, CImageInfo &InfoGrayscale, const SHA256_DIGEST &Sha256)
{
	CCommunityIcon CommunityIcon;
	str_copy(CommunityIcon.m_aCommunityId, pCommunityId);
	CommunityIcon.m_Sha256 = Sha256;
	CommunityIcon.m_OrgTexture = Graphics()->LoadTextureRawMove(Info, 0, pCommunityId);
	CommunityIcon.m_GreyTexture = Graphics()->LoadTextureRawMove(InfoGrayscale, 0, pCommunityId);

	auto ExistingIcon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [pCommunityId](const CCommunityIcon &Element) {
		return str_comp(Element.m_aCommunityId, pCommunityId) == 0;
	});
	if(ExistingIcon == m_vCommunityIcons.end())
	{
		m_vCommunityIcons.push_back(CommunityIcon);
	}
	else
	{
		Graphics()->UnloadTexture(&ExistingIcon->m_OrgTexture);
		Graphics()->UnloadTexture(&ExistingIcon->m_GreyTexture);
		*ExistingIcon = CommunityIcon;
	}

	log_trace("menus/browser", "Loaded community icon '%s'", pCommunityId);
}

void CCommunityIcons::Render(const CCommunityIcon *pIcon, CUIRect Rect, bool Active)
{
	Rect.VMargin(Rect.w / 2.0f - Rect.h, &Rect);

	Graphics()->TextureSet(Active ? pIcon->m_OrgTexture : pIcon->m_GreyTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Active ? 1.0f : 0.5f);
	IGraphics::CQuadItem QuadItem(Rect.x, Rect.y, Rect.w, Rect.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CCommunityIcons::Load()
{
	m_vCommunityIcons.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, "communityicons", FileScan, this);
}

void CCommunityIcons::Shutdown()
{
	m_CommunityIconLoadJobs.clear();
	m_CommunityIconDownloadJobs.clear();
}

void CCommunityIcons::Update()
{
	// Update load jobs (icon is loaded from existing file)
	if(!m_CommunityIconLoadJobs.empty())
	{
		std::shared_ptr<CCommunityIconLoadJob> pJob = m_CommunityIconLoadJobs.front();
		if(pJob->Done())
		{
			if(pJob->Success())
			{
				LoadFinish(pJob->CommunityId(), pJob->ImageInfo(), pJob->ImageInfoGrayscale(), pJob->Sha256());
			}
			m_CommunityIconLoadJobs.pop_front();
		}

		// Don't start download jobs until all load jobs are done
		if(!m_CommunityIconLoadJobs.empty())
			return;
	}

	// Update download jobs (icon is downloaded and loaded from new file)
	if(!m_CommunityIconDownloadJobs.empty())
	{
		std::shared_ptr<CCommunityIconDownloadJob> pJob = m_CommunityIconDownloadJobs.front();
		if(pJob->Done())
		{
			if(pJob->State() == EHttpState::DONE)
			{
				std::shared_ptr<CCommunityIconLoadJob> pLoadJob = std::make_shared<CCommunityIconLoadJob>(this, pJob->CommunityId(), IStorage::TYPE_SAVE);
				Engine()->AddJob(pLoadJob);
				m_CommunityIconLoadJobs.push_back(pLoadJob);
			}
			m_CommunityIconDownloadJobs.pop_front();
		}
	}

	// Rescan for changed communities only when necessary
	if(!ServerBrowser()->DDNetInfoAvailable() || (m_CommunityIconsInfoSha256 != SHA256_ZEROED && m_CommunityIconsInfoSha256 == ServerBrowser()->DDNetInfoSha256()))
		return;
	m_CommunityIconsInfoSha256 = ServerBrowser()->DDNetInfoSha256();

	// Remove icons for removed communities
	auto RemovalIterator = m_vCommunityIcons.begin();
	while(RemovalIterator != m_vCommunityIcons.end())
	{
		if(ServerBrowser()->Community(RemovalIterator->m_aCommunityId) == nullptr)
		{
			Graphics()->UnloadTexture(&RemovalIterator->m_OrgTexture);
			Graphics()->UnloadTexture(&RemovalIterator->m_GreyTexture);
			RemovalIterator = m_vCommunityIcons.erase(RemovalIterator);
		}
		else
		{
			++RemovalIterator;
		}
	}

	// Find added and updated community icons
	for(const auto &Community : ServerBrowser()->Communities())
	{
		if(str_comp(Community.Id(), IServerBrowser::COMMUNITY_NONE) == 0)
			continue;
		auto ExistingIcon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [Community](const auto &Element) {
			return str_comp(Element.m_aCommunityId, Community.Id()) == 0;
		});
		auto pExistingDownload = std::find_if(m_CommunityIconDownloadJobs.begin(), m_CommunityIconDownloadJobs.end(), [Community](const auto &Element) {
			return str_comp(Element->CommunityId(), Community.Id()) == 0;
		});
		if(pExistingDownload == m_CommunityIconDownloadJobs.end() && (ExistingIcon == m_vCommunityIcons.end() || ExistingIcon->m_Sha256 != Community.IconSha256()))
		{
			std::shared_ptr<CCommunityIconDownloadJob> pJob = std::make_shared<CCommunityIconDownloadJob>(this, Community.Id(), Community.IconUrl(), Community.IconSha256());
			Http()->Run(pJob);
			m_CommunityIconDownloadJobs.push_back(pJob);
		}
	}
}
