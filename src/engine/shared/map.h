/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_MAP_H
#define ENGINE_SHARED_MAP_H

#include <base/system.h>

#include "datafile.h"
#include <engine/map.h>

class CMap : public IEngineMap
{
	CDataFileReader m_DataFile;

public:
	CMap();

	void *GetData(int Index) override;
	int GetDataSize(int Index) override;
	void *GetDataSwapped(int Index) override;
	void UnloadData(int Index) override;
	void *GetItem(int Index, int *pType, int *pID) override;
	int GetItemSize(int Index) override;
	void GetType(int Type, int *pStart, int *pNum) override;
	void *FindItem(int Type, int ID) override;
	int NumItems() override;

	void Unload() override;

	bool Load(const char *pMapName) override;

	bool IsLoaded() override;

	SHA256_DIGEST Sha256() override;

	unsigned Crc() override;

	int MapSize() override;

	IOHANDLE File() override;
};

#endif
