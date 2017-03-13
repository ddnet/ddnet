/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_ZONEDB_H
#define ENGINE_ZONEDB_H
#include "kernel.h"

class IZoneDB : public IInterface
{
	MACRO_INTERFACE("zonedb", 0)
	
public:
	virtual void ApplyZoneTypeTexture2D(const char* pZoneName) = 0;
	virtual void ApplyZoneTypeTexture3D(const char* pZoneName, int Index) = 0;
};

class IEngineZoneDB : public IZoneDB
{
	MACRO_INTERFACE("enginezonedb", 0)
public:
	virtual void Init() = 0;
};

extern IEngineZoneDB *CreateEngineZoneDB();

#endif
