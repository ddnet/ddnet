#include <base/hash.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/map.h>
#include <engine/shared/uuid_manager.h>
#include <engine/storage.h>

static const char *TOOL_NAME = "map_test";

static int TestMap(const char *pMapPath, bool CalcHashes, IStorage *pStorage)
{
	log_info(TOOL_NAME, "Testing map '%s'...", pMapPath);

	std::unique_ptr<IMap> pMap = CreateMap();
	if(!pMap->Load(pStorage, pMapPath, IStorage::TYPE_ABSOLUTE))
	{
		log_error(TOOL_NAME, "Failed to open map '%s' for reading", pMapPath);
		return -1;
	}

	char aSha256Str[SHA256_MAXSTRSIZE];
	sha256_str(pMap->Sha256(), aSha256Str, sizeof(aSha256Str));
	log_info(TOOL_NAME, "File size: %d", pMap->Size());
	log_info(TOOL_NAME, "File SHA256: %s", aSha256Str);
	log_info(TOOL_NAME, "File CRC32: %08x", pMap->Crc());
	log_info(TOOL_NAME, "Num items: %d", pMap->NumItems());
	log_info(TOOL_NAME, "Num data: %d", pMap->NumData());

	for(int Index = 0; Index < pMap->NumItems(); Index++)
	{
		log_info(TOOL_NAME, "Item %d:", Index);

		int Type, Id;
		CUuid Uuid;
		const void *pItem = pMap->GetItem(Index, &Type, &Id, &Uuid);

		log_info(TOOL_NAME, "  Type: %d", Type);
		log_info(TOOL_NAME, "  ID: %d", Id);
		char aUuidStr[UUID_MAXSTRSIZE];
		FormatUuid(Uuid, aUuidStr, sizeof(aUuidStr));
		log_info(TOOL_NAME, "  UUID: %s", aUuidStr);

		const int Size = pMap->GetItemSize(Index);
		log_info(TOOL_NAME, "  Size: %d bytes", Size);

		if(CalcHashes)
		{
			const SHA256_DIGEST ItemDataSha256 = sha256(pItem, Size);
			sha256_str(ItemDataSha256, aSha256Str, sizeof(aSha256Str));
			log_info(TOOL_NAME, "  Data (SHA256): %s", aSha256Str);
		}
	}

	for(int Index = 0; Index < pMap->NumData(); Index++)
	{
		log_info(TOOL_NAME, "Data %d:", Index);

		const int Size = pMap->GetDataSize(Index);
		log_info(TOOL_NAME, "  Size: %d bytes", Size);

		const void *pData = pMap->GetData(Index);
		if(pData == nullptr)
		{
			log_info(TOOL_NAME, "  Data erroneous");
		}
		else if(CalcHashes)
		{
			const SHA256_DIGEST ItemDataSha256 = sha256(pData, Size);
			sha256_str(ItemDataSha256, aSha256Str, sizeof(aSha256Str));
			log_info(TOOL_NAME, "  Data (SHA256): %s", aSha256Str);
		}

		pMap->UnloadData(Index);
	}

	pMap->Unload();
	log_info(TOOL_NAME, "Tested map '%s' successfully", pMapPath);
	return 0;
}

int main(int argc, const char **argv)
{
	const CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	const char *pMapPath;
	bool CalcHashes;
	if(argc == 2)
	{
		pMapPath = argv[1];
		CalcHashes = false;
	}
	else if(argc == 3 && str_comp(argv[1], "--calc-hashes") == 0)
	{
		pMapPath = argv[2];
		CalcHashes = true;
	}
	else
	{
		log_error(TOOL_NAME, "Usage: %s [--calc-hashes] <map>", TOOL_NAME);
		return -1;
	}

	std::unique_ptr<IStorage> pStorage = std::unique_ptr<IStorage>(CreateStorage(IStorage::EInitializationType::BASIC, argc, argv));
	if(!pStorage)
	{
		log_error(TOOL_NAME, "Error creating basic storage");
		return -1;
	}

	return TestMap(pMapPath, CalcHashes, pStorage.get());
}
