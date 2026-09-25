#ifndef ENGINE_CLIENT_MAP_THUMBNAIL_H
#define ENGINE_CLIENT_MAP_THUMBNAIL_H

#include <base/process.h>

#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <optional>

class CMapThumbnailJob : public IJob
{
public:
	CMapThumbnailJob(const char *pMapPath, const char *pOutputPath, IStorage *pStorage);
	void Run() override;
	bool Finished() const;
	const char *ThumbnailPath() { return m_pOutputPath; }

private:
	char m_pMapPath[IO_MAX_PATH_LENGTH];
	char m_pOutputPath[IO_MAX_PATH_LENGTH];
	IStorage *m_pStorage = nullptr;
	IStorage *Storage() { return m_pStorage; }
	PROCESS m_Process;
};

#endif
