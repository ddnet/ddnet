/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/zonedb.h>

#include <engine/graphics.h>
#include <engine/storage.h>

#include <base/tl/array.h>

/*
 * Description:
 *    This engine store a list of zone type
 *    Each zone type contains a texture used to render the overlay
 * 
 *    This engine is designed to be independent of DDNet-related entities
 *    Please respect this while you are editing this file
 * 
 * TODO:
 *    - use a hashtable to find quickly a zone type
 *    - take care of too long zone type names
 */

enum
{
	ZONETYPENAME_SIZE=16,
};

struct SZoneType
{
	char m_aName[ZONETYPENAME_SIZE];
	int m_Texture2DId;
	int m_Texture3DId;
};

class CZoneDB : public IEngineZoneDB
{
private:
	IStorage *m_pStorage;
	IGraphics *m_pGraphics;
	array<SZoneType> m_ZoneTypes;
	int m_GenericTexture2D;
	int m_GenericTexture3D;
	
private:
	int GetZoneTypeIndex(const char* pZoneName)
	{
		for(int i=0; i<m_ZoneTypes.size(); i++)
		{
			if(str_comp(m_ZoneTypes[i].m_aName, pZoneName) == 0)
				return i;
		}
		
		SZoneType ZoneType;
		
		//copy name of the zone type
		str_copy(ZoneType.m_aName, pZoneName, sizeof(ZoneType.m_aName));
		
		//find the texture for the zone type
		char aBuffer[1024];
		str_format(aBuffer, sizeof(aBuffer), "zones/%s.png", ZoneType.m_aName);
		
		ZoneType.m_Texture2DId = m_pGraphics->LoadTexture(aBuffer, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		ZoneType.m_Texture3DId = m_pGraphics->LoadTexture(aBuffer, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_TEXTURE3D);
		
		m_ZoneTypes.add(ZoneType);
		
		return m_ZoneTypes.size()-1;
	}
	
public:
	CZoneDB()
	{
		m_pGraphics = 0;
		m_pStorage = 0;
	}

	virtual void Init()
	{
		m_pStorage = Kernel()->RequestInterface<IStorage>();
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		
		m_GenericTexture2D = m_pGraphics->LoadTexture("zones/generic.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		m_GenericTexture3D = m_pGraphics->LoadTexture("zones/generic.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_TEXTURE3D);
	}
	
	virtual void ApplyZoneTypeTexture2D(const char* pZoneTypeName)
	{
		int ZoneTypeIndex = GetZoneTypeIndex(pZoneTypeName);
		if(ZoneTypeIndex < 0 || !m_ZoneTypes[ZoneTypeIndex].m_Texture2DId)
			m_pGraphics->TextureSet(m_GenericTexture2D);
		else
			m_pGraphics->TextureSet(m_ZoneTypes[ZoneTypeIndex].m_Texture2DId);
	}
	
	virtual void ApplyZoneTypeTexture3D(const char* pZoneTypeName, int Index)
	{
		int ZoneTypeIndex = GetZoneTypeIndex(pZoneTypeName);
		if(ZoneTypeIndex < 0 || !m_ZoneTypes[ZoneTypeIndex].m_Texture3DId)
			m_pGraphics->TextureSet3D(m_GenericTexture3D, Index);
		else
			m_pGraphics->TextureSet3D(m_ZoneTypes[ZoneTypeIndex].m_Texture3DId, Index);
	}
};

IEngineZoneDB *CreateEngineZoneDB() { return new CZoneDB; }
