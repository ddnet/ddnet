/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MAP_H
#define ENGINE_MAP_H

#include "kernel.h"
#include <base/hash.h>
#include <base/types.h>

enum
{
	MAX_MAP_LENGTH = 128
};

class IMap : public IInterface
{
	MACRO_INTERFACE("map")
public:
	virtual int GetDataSize(int Index) const = 0;
	virtual void *GetData(int Index) = 0;
	virtual void *GetDataSwapped(int Index) = 0;
	virtual const char *GetDataString(int Index) = 0;
	virtual void UnloadData(int Index) = 0;
	virtual int NumData() const = 0;

	virtual int GetItemSize(int Index) = 0;
	virtual void *GetItem(int Index, int *pType = nullptr, int *pId = nullptr) = 0;
	virtual void GetType(int Type, int *pStart, int *pNum) = 0;
	virtual int FindItemIndex(int Type, int Id) = 0;
	virtual void *FindItem(int Type, int Id) = 0;
	virtual int NumItems() const = 0;
};

class IEngineMap : public IMap
{
	MACRO_INTERFACE("enginemap")
public:
	virtual bool Load(const char *pMapName) = 0;
	virtual void Unload() = 0;
	virtual bool IsLoaded() const = 0;
	virtual IOHANDLE File() const = 0;

	virtual SHA256_DIGEST Sha256() const = 0;
	virtual unsigned Crc() const = 0;
	virtual int MapSize() const = 0;
};

extern IEngineMap *CreateEngineMap();

#endif
