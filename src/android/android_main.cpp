#include <base/detect.h>

#ifdef CONF_PLATFORM_ANDROID
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

#include <base/hash.h>
#include <base/system.h>
#include <engine/shared/linereader.h>
#include <string>
#include <vector>

extern "C" __attribute__((visibility("default"))) void InitAndroid();

static int gs_AndroidStarted = false;

void InitAndroid()
{
	if(gs_AndroidStarted)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DDNet", "The app was started, but not closed properly, this causes bugs. Please restart or manually delete this task.", SDL_GL_GetCurrentWindow());
		std::exit(0);
	}

	gs_AndroidStarted = true;

	// change current path to a writable directory
	const char *pPath = SDL_AndroidGetExternalStoragePath();
	chdir(pPath);
	dbg_msg("client", "changed path to %s", pPath);

	// copy integrity files
	{
		SDL_RWops *pF = SDL_RWFromFile("asset_integrity_files/integrity.txt", "rb");
		if(!pF)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DDNet", "integrity.txt not found, consider reinstalling", SDL_GL_GetCurrentWindow());
			std::exit(0);
		}

		long int length;
		SDL_RWseek(pF, 0, RW_SEEK_END);
		length = SDL_RWtell(pF);
		SDL_RWseek(pF, 0, RW_SEEK_SET);

		char *pAl = (char *)malloc(length);
		SDL_RWread(pF, pAl, 1, length);

		SDL_RWclose(pF);

		mkdir("data", 0755);

		dbg_msg("integrity", "copying integrity.txt with size: %ld", length);

		IOHANDLE pIO = io_open("integrity.txt", IOFLAG_WRITE);
		io_write(pIO, pAl, length);
		io_close(pIO);

		free(pAl);
	}

	IOHANDLE pIO = io_open("integrity.txt", IOFLAG_READ);
	CLineReader LineReader;
	LineReader.Init(pIO);
	const char *pReadLine = NULL;
	std::vector<std::string> vLines;
	while((pReadLine = LineReader.Get()))
	{
		vLines.push_back(pReadLine);
	}
	io_close(pIO);

	// first line is the whole hash
	std::string AllAsOne;
	for(size_t i = 1; i < vLines.size(); ++i)
	{
		AllAsOne.append(vLines[i]);
		AllAsOne.append("\n");
	}
	SHA256_DIGEST ShaAll;
	bool GotSHA = false;
	{
		IOHANDLE pIOR = io_open("integrity_save.txt", IOFLAG_READ);
		if(pIOR != NULL)
		{
			CLineReader LineReader;
			LineReader.Init(pIOR);
			const char *pLine = LineReader.Get();
			if(pLine != NULL)
			{
				sha256_from_str(&ShaAll, pLine);
				GotSHA = true;
			}
		}
	}

	SHA256_DIGEST ShaAllFile;
	sha256_from_str(&ShaAllFile, vLines[0].c_str());

	// TODO: check files individually
	if(!GotSHA || sha256_comp(ShaAllFile, ShaAll) != 0)
	{
		// then the files
		for(size_t i = 1; i < vLines.size(); ++i)
		{
			std::string FileName, Hash;
			std::string::size_type n = 0;
			std::string::size_type c = 0;
			while((c = vLines[i].find(' ', n)) != std::string::npos)
				n = c + 1;
			FileName = vLines[i].substr(0, n - 1);
			Hash = vLines[i].substr(n + 1);

			std::string AssetFileName = std::string("asset_integrity_files/") + FileName;
			SDL_RWops *pF = SDL_RWFromFile(AssetFileName.c_str(), "rb");

			dbg_msg("Integrity", "Copying from assets: %s", FileName.c_str());

			std::string FileNamePath = FileName;
			std::string FileNamePathSub;
			c = 0;
			while((c = FileNamePath.find('/', c)) != std::string::npos)
			{
				FileNamePathSub = FileNamePath.substr(0, c);
				fs_makedir(FileNamePathSub.c_str());
				++c;
			}

			long int length;
			SDL_RWseek(pF, 0, RW_SEEK_END);
			length = SDL_RWtell(pF);
			SDL_RWseek(pF, 0, RW_SEEK_SET);

			char *pAl = (char *)malloc(length);
			SDL_RWread(pF, pAl, 1, length);

			SDL_RWclose(pF);

			IOHANDLE pIO = io_open(FileName.c_str(), IOFLAG_WRITE);
			io_write(pIO, pAl, length);
			io_close(pIO);

			free(pAl);
		}

		IOHANDLE pIOR = io_open("integrity_save.txt", IOFLAG_WRITE);
		if(pIOR != NULL)
		{
			char aFileSHA[SHA256_MAXSTRSIZE];
			sha256_str(ShaAllFile, aFileSHA, sizeof(aFileSHA));
			io_write(pIOR, aFileSHA, str_length(aFileSHA));
			io_close(pIOR);
		}
	}
}

#endif
