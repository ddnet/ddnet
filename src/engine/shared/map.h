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

	virtual void *GetData(int Index);
	virtual int GetDataSize(int Index);
	virtual void *GetDataSwapped(int Index);
	virtual void UnloadData(int Index);
	virtual void *GetItem(int Index, int *pType, int *pID);
	virtual int GetItemSize(int Index);
	virtual void GetType(int Type, int *pStart, int *pNum);
	virtual void *FindItem(int Type, int ID);
	virtual int NumItems();

	virtual void Unload();

	virtual bool Load(const char *pMapName);

	virtual bool IsLoaded();

	virtual SHA256_DIGEST Sha256();

	virtual unsigned Crc();

	virtual int MapSize();

	virtual IOHANDLE File();
};

#endif
