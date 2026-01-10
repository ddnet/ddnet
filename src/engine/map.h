/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MAP_H
#define ENGINE_MAP_H

#include <base/hash.h>
#include <base/types.h>

#include <memory>

class IStorage;

enum
{
	MAX_MAP_LENGTH = 128
};

class IMap
{
public:
	virtual ~IMap() = default;

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

	[[nodiscard]] virtual bool Load(const char *pFullName, IStorage *pStorage, const char *pPath, int StorageType) = 0;
	[[nodiscard]] virtual bool Load(IStorage *pStorage, const char *pPath, int StorageType) = 0;
	virtual void Unload() = 0;
	virtual bool IsLoaded() const = 0;
	virtual IOHANDLE File() const = 0;

	/**
	 * Returns the full name of the currently loaded map.
	 *
	 * @return Full map name, e.g. `subfolder1/subfolder2/my_map`.
	 */
	virtual const char *FullName() const = 0;
	/**
	 * Returns the base name of the currently loaded map.
	 *
	 * @return Base map name, e.g. `my_map`.
	 */
	virtual const char *BaseName() const = 0;
	/**
	 * Returns the path of the currently loaded map.
	 *
	 * @return Map path, e.g. `maps/subfolder1/subfolder2/my_map.map`.
	 */
	virtual const char *Path() const = 0;
	virtual SHA256_DIGEST Sha256() const = 0;
	virtual unsigned Crc() const = 0;
	virtual int Size() const = 0;
};

extern std::unique_ptr<IMap> CreateMap();

#endif
