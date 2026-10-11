#ifndef GAME_EDITOR_MAPITEMS_DOCUMENT_VALUES_H
#define GAME_EDITOR_MAPITEMS_DOCUMENT_VALUES_H

#include "element_values.h"
#include "tile_values.h"

#include <base/str.h>
#include <base/types.h>

#include <engine/image.h>

#include <game/editor/history/resource_blob.h>
#include <game/mapitems.h>

#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>

/** Fixed-capacity document text compares its text, never unused buffer bytes. */
template<std::size_t Capacity>
class CNamedDocumentValues
{
public:
	char m_aName[Capacity]{};

	bool operator==(const CNamedDocumentValues &Other) const
	{
		return std::strncmp(m_aName, Other.m_aName, Capacity) == 0;
	}
};

/** Fixed text storage with default-copyable semantic equality. */
template<std::size_t Capacity>
class CDocumentText
{
	char m_aBuffer[Capacity]{};

public:
	char (&Buffer())[Capacity] { return m_aBuffer; }
	const char (&Buffer() const)[Capacity] { return m_aBuffer; }
	bool operator==(const CDocumentText &Other) const
	{
		return std::strncmp(m_aBuffer, Other.m_aBuffer, Capacity) == 0;
	}
};

class CMapInfoValues
{
public:
	CDocumentText<32> m_Author;
	CDocumentText<16> m_Version;
	CDocumentText<128> m_Credits;
	CDocumentText<32> m_License;
	bool operator==(const CMapInfoValues &) const = default;
	void Reset() { *this = {}; }
	void Copy(const CMapInfoValues &Other) { *this = Other; }
};

class CEditorMapSetting
{
public:
	CDocumentText<256> m_Command;
	CEditorMapSetting(const char *pCommand) { str_copy(m_Command.Buffer(), pCommand); }
	bool operator==(const CEditorMapSetting &) const = default;
};

/**
 * Authoritative records below are copied as whole values during capture/restore.
 * Initialized scalar fields with default equality need no history field mapping.
 * Heap ownership, references and file persistence have extra integration points.
 * Keep session state and derived handles outside these records.
 */
class CMapValues
{
public:
	CMapInfoValues m_MapInfo;
	std::vector<CEditorMapSetting> m_vSettings;
	bool operator==(const CMapValues &) const = default;
};

class CLayerQuadsValues
{
public:
	CDocumentReference m_Image;
	std::vector<CQuadValues> m_vQuads;
	bool operator==(const CLayerQuadsValues &) const = default;
};

class CLayerSoundsValues
{
public:
	CDocumentReference m_Sound;
	std::vector<CSoundSourceValues> m_vSources;
	bool operator==(const CLayerSoundsValues &) const = default;
};

class CEnvelopeValues : public CDocumentIdentity, public CNamedDocumentValues<32>
{
public:
	enum class EType
	{
		POSITION,
		COLOR,
		SOUND,
	};

	EType m_Type = EType::POSITION;
	bool m_Synchronized = true;
	std::vector<CEnvelopePointValues> m_vPoints;
	bool operator==(const CEnvelopeValues &) const = default;
};

/** Authoritative common layer values; visibility and locking stay in CLayer. */
class CLayerValues : public CDocumentIdentity, public CNamedDocumentValues<12>
{
public:
	int m_Type = 0;
	int m_Flags = 0;
	bool operator==(const CLayerValues &) const = default;
};

/** Authoritative group properties, independent of runtime layer bindings. */
class CLayerGroupValues : public CDocumentIdentity, public CNamedDocumentValues<12>
{
public:
	int m_OffsetX = 0;
	int m_OffsetY = 0;
	int m_ParallaxX = 100;
	int m_ParallaxY = 100;
	int m_UseClipping = 0;
	int m_ClipX = 0;
	int m_ClipY = 0;
	int m_ClipW = 0;
	int m_ClipH = 0;
	bool m_GameGroup = false;
	bool operator==(const CLayerGroupValues &) const = default;
};

/** Authoritative tile-layer properties and spatial cell storage. */
class CLayerTilesValues
{
public:
	bool m_HasGame = false;
	CDocumentReference m_Image;
	CEditorTilePlane<CTileValues> m_Tiles;
	int Width() const { return m_Tiles.Width(); }
	int Height() const { return m_Tiles.Height(); }
	CColor m_Color{255, 255, 255, 255};
	CDocumentReference m_ColorEnv;
	int m_ColorEnvOffset = 0;
	int m_FillGameTile = -1;
	bool m_LiveGameTiles = false;
	int m_AutomapperConfig = -1;
	int m_AutomapperReference = -1;
	int m_Seed = 0;
	bool m_AutoAutomapper = false;
	bool m_HasTele = false;
	bool m_HasSpeedup = false;
	bool m_HasFront = false;
	bool m_HasSwitch = false;
	bool m_HasTune = false;
	bool operator==(const CLayerTilesValues &) const = default;
};

class CLayerTeleValues
{
public:
	CEditorTilePlane<CTeleTileValues> m_TeleTiles;
	bool operator==(const CLayerTeleValues &) const = default;
};

class CLayerSpeedupValues
{
public:
	CEditorTilePlane<CSpeedupTileValues> m_SpeedupTiles;
	bool operator==(const CLayerSpeedupValues &) const = default;
};

class CLayerSwitchValues
{
public:
	CEditorTilePlane<CSwitchTileValues> m_SwitchTiles;
	bool operator==(const CLayerSwitchValues &) const = default;
};

class CLayerTuneValues
{
public:
	CEditorTilePlane<CTuneTileValues> m_TuneTiles;
	bool operator==(const CLayerTuneValues &) const = default;
};

/** Encoded sound bytes and name; the playback handle belongs to the wrapper. */
class CEditorSoundValues : public CDocumentIdentity, public CNamedDocumentValues<IO_MAX_PATH_LENGTH>
{
public:
	std::optional<editor_history::CResourceBlob> m_Content;
	bool operator==(const CEditorSoundValues &) const = default;

	const std::uint8_t *Data() const { return m_Content ? m_Content->Bytes().data() : nullptr; }
	std::size_t DataSize() const { return m_Content ? m_Content->Bytes().size() : 0; }
};

/** Image authoring values retain source pixels independently of GPU handles. */
class CEditorImageValues : public CDocumentIdentity, public CNamedDocumentValues<IO_MAX_PATH_LENGTH>
{
public:
	int m_External = 0;
	std::size_t m_Width = 0;
	std::size_t m_Height = 0;
	CImageInfo::EImageFormat m_Format = CImageInfo::FORMAT_UNDEFINED;
	std::optional<editor_history::CResourceBlob> m_Content;
	bool operator==(const CEditorImageValues &) const = default;

	const std::uint8_t *Data() const { return m_Content ? m_Content->Bytes().data() : nullptr; }
	std::size_t DataSize() const { return m_Width == 0 || m_Height == 0 ? 0 : m_Width * m_Height * CImageInfo::PixelSize(m_Format); }
};

/** Derived opacity flags, computed from retained pixels without changing values. */
inline std::array<unsigned char, 256> EditorImageTileFlags(const CEditorImageValues &Image)
{
	std::array<unsigned char, 256> aFlags{};
	if(!Image.m_Content || Image.m_Format != CImageInfo::FORMAT_RGBA)
		return aFlags;
	const auto TileWidth = Image.m_Width / 16;
	const auto TileHeight = Image.m_Height / 16;
	if(TileWidth == 0 || TileWidth != TileHeight || Image.m_Width > Image.m_Content->Bytes().size() / 4 / Image.m_Height)
		return aFlags;
	for(std::size_t TileY = 0; TileY < 16; ++TileY)
		for(std::size_t TileX = 0; TileX < 16; ++TileX)
		{
			bool Opaque = true;
			for(std::size_t Y = 0; Y < TileHeight && Opaque; ++Y)
				for(std::size_t X = 0; X < TileWidth; ++X)
					if(Image.Data()[((TileY * TileHeight + Y) * Image.m_Width + TileX * TileWidth + X) * 4 + 3] < 250)
					{
						Opaque = false;
						break;
					}
			if(Opaque)
				aFlags[TileY * 16 + TileX] = TILEFLAG_OPAQUE;
		}
	return aFlags;
}

#endif
