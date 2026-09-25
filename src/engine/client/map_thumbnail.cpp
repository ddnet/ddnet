#include "map_thumbnail.h"

#include <base/fs.h>
#include <base/log.h>
#include <base/str.h>

#define MAPRENDER_EXEC "map_render"

CMapThumbnailJob::CMapThumbnailJob(const char *pMapPath, const char *pOutputPath, IStorage *pStorage)
{
	str_copy(m_pMapPath, pMapPath, sizeof(m_pMapPath));
	str_copy(m_pOutputPath, pOutputPath, sizeof(m_pOutputPath));
	m_pStorage = pStorage;
}

void CMapThumbnailJob::Run()
{
#ifndef CONF_PLATFORM_ANDROID
	char aBinaryPath[IO_MAX_PATH_LENGTH];
	char aMapPath[IO_MAX_PATH_LENGTH];
	char aDownloadedMapsPath[IO_MAX_PATH_LENGTH];

	Storage()->GetBinaryPath(MAPRENDER_EXEC, aBinaryPath, sizeof(aBinaryPath));
	if(!fs_is_file(aBinaryPath))
	{
		log_warn("thumbnail", "%s binary not found", MAPRENDER_EXEC);
		return;
	}
	if(!Storage()->FindFile(m_pMapPath, "downloadedmaps", IStorage::TYPE_SAVE, aMapPath, sizeof(aMapPath)))
	{
		log_error("thumbnail", "Did not find map '%s'", aMapPath);
		return;
	}

	Storage()->GetCompletePath(IStorage::TYPE_SAVE, "downloadedmaps", aDownloadedMapsPath, sizeof(aDownloadedMapsPath));
	str_format(aMapPath, sizeof(aMapPath), "%s/%s", aDownloadedMapsPath, m_pMapPath);
	std::vector<const char *> vpArguments = {"-o", m_pOutputPath, "-w", "640", "-h", "480", aMapPath};
	m_Process = process_execute(aBinaryPath, EShellExecuteWindowState::FOREGROUND, vpArguments.data(), vpArguments.size());
	if(m_Process == INVALID_PROCESS)
		log_error("thumbnail", "process crashed");
#endif
}

bool CMapThumbnailJob::Finished() const
{
#ifdef CONF_PLATFORM_ANDROID
	return true;
#else
	return !process_is_alive(m_Process);
#endif
}
