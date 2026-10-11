#include "document_export.h"

#include <base/detect.h>
#include <base/hash_ctxt.h>
#include <base/mem.h>

#include <engine/shared/datafile.h>

#include <game/gamecore.h>
#include <game/mapitems_ex.h>

#include <map>

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

// BCrypt requires the Windows type declarations above.
#include <bcrypt.h>
#endif

namespace
{
	template<typename TValue>
	struct CTileFormat;
	template<>
	struct CTileFormat<CTileValues>
	{
		using CFormat = CTile;
	};
	template<>
	struct CTileFormat<CTeleTileValues>
	{
		using CFormat = CTeleTile;
	};
	template<>
	struct CTileFormat<CSpeedupTileValues>
	{
		using CFormat = CSpeedupTile;
	};
	template<>
	struct CTileFormat<CSwitchTileValues>
	{
		using CFormat = CSwitchTile;
	};
	template<>
	struct CTileFormat<CTuneTileValues>
	{
		using CFormat = CTuneTile;
	};

	template<typename TValue>
	auto ExportTile(const TValue &Value, const std::array<unsigned char, 256> &aFlags)
	{
		using CFormat = typename CTileFormat<TValue>::CFormat;
		CFormat Tile = static_cast<CFormat>(Value);
		if constexpr(std::is_same_v<TValue, CTileValues>)
			Tile.m_Flags |= aFlags[Tile.m_Index];
		return Tile;
	}

	int EnvelopeChannels(CEnvelopeValues::EType Type)
	{
		switch(Type)
		{
		case CEnvelopeValues::EType::POSITION: return 3;
		case CEnvelopeValues::EType::COLOR: return 4;
		case CEnvelopeValues::EType::SOUND: return 1;
		}
		return 0;
	}

	/** Both writing and fingerprinting use this single native object projection. */
	template<typename F>
	int ExportLayerObjects(const CLayerDocumentValues &Layer, const CEditorDocumentValues &Document, F &&Output)
	{
		const auto EnvelopeIndex = [&](CDocumentReference Reference) { return Document.EnvelopeIndex(Reference); };
		if(const auto *pQuads = std::get_if<CLayerQuadsValues>(&Layer.m_Data))
		{
			if(pQuads->m_vQuads.size() > std::numeric_limits<int>::max() / sizeof(CQuad))
				return Output(0, nullptr);
			std::vector<CQuad> vQuads;
			vQuads.reserve(std::max<std::size_t>(1, pQuads->m_vQuads.size()));
			for(const auto &Quad : pQuads->m_vQuads)
				vQuads.push_back(Quad.Export(EnvelopeIndex));
			if(vQuads.empty())
				vQuads.emplace_back(); // Empty-layer compatibility payload.
			return Output(vQuads.size() * sizeof(CQuad), vQuads.data());
		}
		const auto &Sounds = std::get<CLayerSoundsValues>(Layer.m_Data);
		if(Sounds.m_vSources.size() > std::numeric_limits<int>::max() / sizeof(CSoundSource))
			return Output(0, nullptr);
		std::vector<CSoundSource> vSources;
		vSources.reserve(std::max<std::size_t>(1, Sounds.m_vSources.size()));
		for(const auto &Source : Sounds.m_vSources)
			vSources.push_back(Source.Export(EnvelopeIndex));
		if(vSources.empty())
			vSources.emplace_back();
		return Output(vSources.size() * sizeof(CSoundSource), vSources.data());
	}
}

int IEditorDocumentOutput::AddLayerObjects(const editor_history::CSharedValue<CLayerDocumentValues> &Layer, const CEditorDocumentValues &Document)
{
	return ExportLayerObjects(Layer.Read(), Document, [&](std::size_t Size, const void *pData) { return AddDataSwapped(Size, pData); });
}

int IEditorDocumentOutput::AddResourceBlob(const editor_history::CResourceBlob &Blob)
{
	return AddData(Blob.Bytes().size(), Blob.Bytes().data());
}

int IEditorDocumentOutput::AddTilePlane(CEditorTilePayload Plane, const std::array<unsigned char, 256> &aFlags)
{
	return std::visit([&](const auto *pPlane) {
		using TValue = std::remove_cvref_t<decltype((*pPlane)[0])>;
		using CFormat = typename CTileFormat<TValue>::CFormat;
		if(pPlane->Size() > std::numeric_limits<int>::max() / sizeof(CFormat))
			return AddData(0, nullptr);
		// Implicit empty cells can still acquire the image's tile-zero opacity.
		std::vector<CFormat> vTiles(pPlane->Size(), ExportTile(TValue{}, aFlags));
		const CEditorTilePlane<TValue> Empty(pPlane->Width(), pPlane->Height());
		pPlane->Plane().VisitChangedChunks(&Empty.Plane(), [&](std::size_t X, std::size_t Y, std::size_t Width, std::size_t Height, auto Cells) {
			for(std::size_t Row = 0; Row < Height; ++Row)
				for(std::size_t Column = 0; Column < Width; ++Column)
					vTiles[(Y + Row) * pPlane->Width() + X + Column] = ExportTile(Cells[Row * CEditorTilePlane<TValue>::CHUNK_EDGE + Column], aFlags);
		});
		return AddData(vTiles.size() * sizeof(CFormat), vTiles.data());
	},
		Plane);
}

int IEditorDocumentOutput::AddEmptyTiles(std::size_t Count)
{
	if(Count > std::numeric_limits<int>::max() / sizeof(CTile))
		return AddData(0, nullptr);
	const std::vector<CTile> vTiles(Count);
	return AddData(Count * sizeof(CTile), vTiles.data());
}

std::array<unsigned char, 256> IEditorDocumentOutput::ImageTileFlags(const CEditorImageValues &Image)
{
	return EditorImageTileFlags(Image);
}

bool ExportEditorDocument(const CEditorDocumentValues &Document, IEditorDocumentOutput &Writer, std::string &Error)
{
	if(!Document.Validate(Error))
		return false;
	const auto ImageIndex = [&](CDocumentReference Reference) {
		const auto &vImages = Document.m_Images.Read();
		const auto It = std::find_if(vImages.begin(), vImages.end(), [&](const auto &Image) { return Image.m_Id == Reference.m_Id; });
		return It == vImages.end() ? -1 : static_cast<int>(It - vImages.begin());
	};
	const auto SoundIndex = [&](CDocumentReference Reference) {
		const auto &vSounds = Document.m_Sounds.Read();
		const auto It = std::find_if(vSounds.begin(), vSounds.end(), [&](const auto &Sound) { return Sound.m_Id == Reference.m_Id; });
		return It == vSounds.end() ? -1 : static_cast<int>(It - vSounds.begin());
	};
	std::vector<std::array<unsigned char, 256>> vImageFlags;
	for(const auto &Image : Document.m_Images.Read())
		vImageFlags.push_back(Writer.ImageTileFlags(Image));
	std::vector<const CEnvelopeValues *> vpEnvelopes;
	for(const auto &Envelope : Document.m_Envelopes.Read())
		vpEnvelopes.push_back(&Envelope.Read());
	// save version
	{
		CMapItemVersion Item{};
		Item.m_Version = 1;
		Writer.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Item), &Item);
	}

	// save map info
	{
		CMapItemInfoSettings Item{};
		Item.m_Version = 1;
		Item.m_Author = Writer.AddDataString(Document.m_MapInfo.m_Author.Buffer());
		Item.m_MapVersion = Writer.AddDataString(Document.m_MapInfo.m_Version.Buffer());
		Item.m_Credits = Writer.AddDataString(Document.m_MapInfo.m_Credits.Buffer());
		Item.m_License = Writer.AddDataString(Document.m_MapInfo.m_License.Buffer());

		Item.m_Settings = -1;
		if(!Document.m_vSettings.empty())
		{
			int Size = 0;
			for(const auto &Setting : Document.m_vSettings)
			{
				Size += str_length(Setting.m_Command.Buffer()) + 1;
			}

			std::vector<char> vSettings(std::max(Size, 1));
			char *pNext = vSettings.data();
			for(const auto &Setting : Document.m_vSettings)
			{
				int Length = str_length(Setting.m_Command.Buffer()) + 1;
				mem_copy(pNext, Setting.m_Command.Buffer(), Length);
				pNext += Length;
			}
			Item.m_Settings = Writer.AddData(Size, vSettings.data());
		}

		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
	}

	// save images
	for(size_t i = 0; i < Document.m_Images.Read().size(); i++)
	{
		const auto *pImg = &Document.m_Images.Read()[i];

		CMapItemImage Item{};
		Item.m_Version = 1;

		Item.m_Width = pImg->m_Width;
		Item.m_Height = pImg->m_Height;
		Item.m_External = pImg->m_External;
		Item.m_ImageName = Writer.AddDataString(pImg->m_aName);
		if(pImg->m_External || pImg->Data() == nullptr)
		{
			Item.m_ImageData = -1;
		}
		else
		{
			Item.m_ImageData = Writer.AddResourceBlob(*pImg->m_Content);
		}
		Writer.AddItem(MAPITEMTYPE_IMAGE, i, sizeof(Item), &Item);
	}

	// save sounds
	for(size_t i = 0; i < Document.m_Sounds.Read().size(); i++)
	{
		const auto *pSound = &Document.m_Sounds.Read()[i];

		CMapItemSound Item{};
		Item.m_Version = 1;

		Item.m_External = 0;
		Item.m_SoundName = Writer.AddDataString(pSound->m_aName);
		Item.m_SoundData = pSound->DataSize() == 0 ? -1 : Writer.AddResourceBlob(*pSound->m_Content);
		// Value is not read in new versions, but we still need to write it for compatibility with old versions.
		Item.m_SoundDataSize = pSound->DataSize();

		Writer.AddItem(MAPITEMTYPE_SOUND, i, sizeof(Item), &Item);
	}

	// save layers
	int LayerCount = 0, GroupCount = 0;
	int AutomapperCount = 0;
	for(const auto &GroupRecord : Document.m_Groups.Read())
	{
		const auto *pGroup = &GroupRecord.Read();

		CMapItemGroup GItem{};
		GItem.m_Version = 3;

		GItem.m_ParallaxX = pGroup->m_ParallaxX;
		GItem.m_ParallaxY = pGroup->m_ParallaxY;
		GItem.m_OffsetX = pGroup->m_OffsetX;
		GItem.m_OffsetY = pGroup->m_OffsetY;
		GItem.m_UseClipping = pGroup->m_UseClipping;
		GItem.m_ClipX = pGroup->m_ClipX;
		GItem.m_ClipY = pGroup->m_ClipY;
		GItem.m_ClipW = pGroup->m_ClipW;
		GItem.m_ClipH = pGroup->m_ClipH;
		GItem.m_StartLayer = LayerCount;
		GItem.m_NumLayers = 0;

		// save group name
		StrToInts(GItem.m_aName, std::size(GItem.m_aName), pGroup->m_aName);

		for(const auto &LayerRecord : pGroup->m_Layers.Read())
		{
			const auto *pLayer = &LayerRecord.Read();
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				const auto *pLayerTiles = &std::get<CTileLayerDocumentValues>(pLayer->m_Data);
				const auto aFlags = pLayerTiles->m_Color.a == 255 && ImageIndex(pLayerTiles->m_Image) >= 0 ? vImageFlags[ImageIndex(pLayerTiles->m_Image)] : std::array<unsigned char, 256>{};

				CMapItemLayerTilemap Item{};
				Item.m_Version = 3;

				Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = pLayer->m_Type;

				Item.m_Color = pLayerTiles->m_Color;
				Item.m_ColorEnv = Document.EnvelopeIndex(pLayerTiles->m_ColorEnv);
				Item.m_ColorEnvOffset = pLayerTiles->m_ColorEnvOffset;

				Item.m_Width = pLayerTiles->Width();
				Item.m_Height = pLayerTiles->Height();
				// Item.m_Flags = pLayerTiles->m_Game ? TILESLAYERFLAG_GAME : 0;

				if(pLayerTiles->m_HasTele)
					Item.m_Flags = TILESLAYERFLAG_TELE;
				else if(pLayerTiles->m_HasSpeedup)
					Item.m_Flags = TILESLAYERFLAG_SPEEDUP;
				else if(pLayerTiles->m_HasFront)
					Item.m_Flags = TILESLAYERFLAG_FRONT;
				else if(pLayerTiles->m_HasSwitch)
					Item.m_Flags = TILESLAYERFLAG_SWITCH;
				else if(pLayerTiles->m_HasTune)
					Item.m_Flags = TILESLAYERFLAG_TUNE;
				else
					Item.m_Flags = pLayerTiles->m_HasGame ? TILESLAYERFLAG_GAME : 0;

				Item.m_Image = ImageIndex(pLayerTiles->m_Image);

				// the following values were previously uninitialized, do not rely on them being -1 when unused
				Item.m_Tele = -1;
				Item.m_Speedup = -1;
				Item.m_Front = -1;
				Item.m_Switch = -1;
				Item.m_Tune = -1;

				if(Item.m_Flags && !(pLayerTiles->m_HasGame))
				{
					Item.m_Data = Writer.AddEmptyTiles(pLayerTiles->m_Tiles.Size());

					if(pLayerTiles->m_HasTele)
						Item.m_Tele = Writer.AddTilePlane(&std::get<CLayerTeleValues>(pLayerTiles->m_Auxiliary).m_TeleTiles);
					else if(pLayerTiles->m_HasSpeedup)
						Item.m_Speedup = Writer.AddTilePlane(&std::get<CLayerSpeedupValues>(pLayerTiles->m_Auxiliary).m_SpeedupTiles);
					else if(pLayerTiles->m_HasFront)
						Item.m_Front = Writer.AddTilePlane(&pLayerTiles->m_Tiles, aFlags);
					else if(pLayerTiles->m_HasSwitch)
						Item.m_Switch = Writer.AddTilePlane(&std::get<CLayerSwitchValues>(pLayerTiles->m_Auxiliary).m_SwitchTiles);
					else if(pLayerTiles->m_HasTune)
						Item.m_Tune = Writer.AddTilePlane(&std::get<CLayerTuneValues>(pLayerTiles->m_Auxiliary).m_TuneTiles);
				}
				else
					Item.m_Data = Writer.AddTilePlane(&pLayerTiles->m_Tiles, aFlags);

				// save layer name
				StrToInts(Item.m_aName, std::size(Item.m_aName), pLayer->m_aName);

				// save item
				Writer.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

				// save auto mapper of each tile layer (not physics layer)
				if(!Item.m_Flags)
				{
					CMapItemAutomapperConfig ItemAutomapper{};
					ItemAutomapper.m_Version = 1;
					ItemAutomapper.m_GroupId = GroupCount;
					ItemAutomapper.m_LayerId = GItem.m_NumLayers;
					ItemAutomapper.m_AutomapperConfig = pLayerTiles->m_AutomapperConfig;
					ItemAutomapper.m_AutomapperSeed = pLayerTiles->m_Seed;
					ItemAutomapper.m_Flags = 0;
					if(pLayerTiles->m_AutoAutomapper)
						ItemAutomapper.m_Flags |= CMapItemAutomapperConfig::FLAG_AUTOMATIC;

					Writer.AddItem(MAPITEMTYPE_AUTOMAPPER_CONFIG, AutomapperCount, sizeof(ItemAutomapper), &ItemAutomapper);
					AutomapperCount++;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				const auto *pLayerQuads = &std::get<CLayerQuadsValues>(pLayer->m_Data);
				CMapItemLayerQuads Item{};
				Item.m_Version = 2;
				Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = pLayer->m_Type;
				Item.m_Image = ImageIndex(pLayerQuads->m_Image);

				Item.m_NumQuads = pLayerQuads->m_vQuads.size();
				Item.m_Data = Writer.AddLayerObjects(LayerRecord, Document);

				// save layer name
				StrToInts(Item.m_aName, std::size(Item.m_aName), pLayer->m_aName);

				// save item
				Writer.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				const auto *pLayerSounds = &std::get<CLayerSoundsValues>(pLayer->m_Data);
				CMapItemLayerSounds Item{};
				Item.m_Version = 2;
				Item.m_Layer.m_Version = 0; // was previously uninitialized, do not rely on it being 0
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = pLayer->m_Type;
				Item.m_Sound = SoundIndex(pLayerSounds->m_Sound);

				Item.m_NumSources = pLayerSounds->m_vSources.size();
				Item.m_Data = Writer.AddLayerObjects(LayerRecord, Document);

				// save layer name
				StrToInts(Item.m_aName, std::size(Item.m_aName), pLayer->m_aName);

				// save item
				Writer.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);
			}

			GItem.m_NumLayers++;
			LayerCount++;
		}

		Writer.AddItem(MAPITEMTYPE_GROUP, GroupCount, sizeof(GItem), &GItem);
		GroupCount++;
	}

	// save envelopes

	int PointCount = 0;
	for(size_t e = 0; e < vpEnvelopes.size(); e++)
	{
		CMapItemEnvelope Item{};
		Item.m_Version = 2;
		Item.m_Channels = EnvelopeChannels(vpEnvelopes[e]->m_Type);
		Item.m_StartPoint = PointCount;
		Item.m_NumPoints = vpEnvelopes[e]->m_vPoints.size();
		Item.m_Synchronized = vpEnvelopes[e]->m_Synchronized;
		StrToInts(Item.m_aName, std::size(Item.m_aName), vpEnvelopes[e]->m_aName);

		Writer.AddItem(MAPITEMTYPE_ENVELOPE, e, sizeof(Item), &Item);
		PointCount += Item.m_NumPoints;
	}

	// save points

	bool BezierUsed = false;
	for(const auto &pEnvelope : vpEnvelopes)
	{
		for(const auto &Point : pEnvelope->m_vPoints)
		{
			if(Point.m_Curvetype == CURVETYPE_BEZIER)
			{
				BezierUsed = true;
				break;
			}
		}
		if(BezierUsed)
			break;
	}

	std::vector<CEnvPoint> vPoints(std::max(PointCount, 1));
	CEnvPoint *pPoints = vPoints.data();
	std::vector<CEnvPointBezier> vPointsBezier(BezierUsed ? std::max(PointCount, 1) : 0);
	CEnvPointBezier *pPointsBezier = vPointsBezier.empty() ? nullptr : vPointsBezier.data();
	PointCount = 0;

	for(const auto &pEnvelope : vpEnvelopes)
	{
		const CEnvPoint_runtime *pPrevPoint = nullptr;
		for(const auto &Point : pEnvelope->m_vPoints)
		{
			pPoints[PointCount] = static_cast<const CEnvPoint &>(Point);
			if(pPointsBezier != nullptr)
			{
				if(Point.m_Curvetype == CURVETYPE_BEZIER)
				{
					mem_copy(&pPointsBezier[PointCount].m_aOutTangentDeltaX, &Point.m_Bezier.m_aOutTangentDeltaX, sizeof(Point.m_Bezier.m_aOutTangentDeltaX));
					mem_copy(&pPointsBezier[PointCount].m_aOutTangentDeltaY, &Point.m_Bezier.m_aOutTangentDeltaY, sizeof(Point.m_Bezier.m_aOutTangentDeltaY));
				}
				if(pPrevPoint != nullptr && pPrevPoint->m_Curvetype == CURVETYPE_BEZIER)
				{
					mem_copy(&pPointsBezier[PointCount].m_aInTangentDeltaX, &Point.m_Bezier.m_aInTangentDeltaX, sizeof(Point.m_Bezier.m_aInTangentDeltaX));
					mem_copy(&pPointsBezier[PointCount].m_aInTangentDeltaY, &Point.m_Bezier.m_aInTangentDeltaY, sizeof(Point.m_Bezier.m_aInTangentDeltaY));
				}
			}
			PointCount++;
			pPrevPoint = &Point;
		}
	}

	Writer.AddItem(MAPITEMTYPE_ENVPOINTS, 0, sizeof(CEnvPoint) * PointCount, pPoints);

	if(pPointsBezier != nullptr)
	{
		Writer.AddItem(MAPITEMTYPE_ENVPOINTS_BEZIER, 0, sizeof(CEnvPointBezier) * PointCount, pPointsBezier);
	}

	return true;
}

namespace
{
	class CWriterOutput : public IEditorDocumentOutput
	{
		CDataFileWriter &m_Writer;
		bool m_Success = true;

	public:
		explicit CWriterOutput(CDataFileWriter &Writer) :
			m_Writer(Writer) {}
		bool Success() const { return m_Success; }

		int AddItem(int Type, int Id, std::size_t Size, const void *pData) override
		{
			if(Id < 0 || Id > 65535 || Size % sizeof(int) != 0 || Size > std::numeric_limits<int>::max() || (Size != 0 && pData == nullptr))
			{
				m_Success = false;
				return -1;
			}
			return m_Writer.AddItem(Type, Id, Size, pData);
		}
		int AddData(std::size_t Size, const void *pData) override
		{
			if(Size == 0 || Size > std::numeric_limits<int>::max() || pData == nullptr)
			{
				m_Success = false;
				return -1;
			}
			return m_Writer.AddData(Size, pData);
		}
		int AddDataSwapped(std::size_t Size, const void *pData) override
		{
			if(Size == 0 || Size % sizeof(int) != 0 || Size > std::numeric_limits<int>::max() || pData == nullptr)
			{
				m_Success = false;
				return -1;
			}
			return m_Writer.AddDataSwapped(Size, pData);
		}
	};

	class CKeyOutput : public IEditorDocumentOutput
	{
	protected:
		SHA256_CTX m_Context;
		int m_DataCount = 0;
		int m_ItemCount = 0;

		void Number(std::uint64_t Value)
		{
			unsigned char aBytes[8];
			for(int Index = 0; Index < 8; ++Index)
				aBytes[Index] = static_cast<unsigned char>(Value >> (Index * 8));
			sha256_update(&m_Context, aBytes, sizeof(aBytes));
		}

		void Bytes(std::size_t Size, const void *pData, bool Integers)
		{
			Number(Size);
#if defined(CONF_ARCH_ENDIAN_BIG)
			if(Integers)
			{
				const auto *pBytes = static_cast<const unsigned char *>(pData);
				for(std::size_t Index = 0; Index < Size; Index += 4)
				{
					const unsigned char aLittleEndian[4]{pBytes[Index + 3], pBytes[Index + 2], pBytes[Index + 1], pBytes[Index]};
					sha256_update(&m_Context, aLittleEndian, sizeof(aLittleEndian));
				}
				return;
			}
#else
			(void)Integers;
#endif
			if(Size != 0)
				sha256_update(&m_Context, pData, Size);
		}

	public:
		CKeyOutput() { sha256_init(&m_Context); }
		int AddItem(int Type, int Id, std::size_t Size, const void *pData) override
		{
			Number(0);
			Number(Type);
			Number(Id);
			Bytes(Size, pData, true);
			return m_ItemCount++;
		}
		int AddData(std::size_t Size, const void *pData) override
		{
			Number(1);
			Bytes(Size, pData, false);
			return m_DataCount++;
		}
		int AddDataSwapped(std::size_t Size, const void *pData) override
		{
			Number(1);
			Bytes(Size, pData, true);
			return m_DataCount++;
		}
		SHA256_DIGEST FinishDigest() { return sha256_finish(&m_Context); }
		std::string Finish()
		{
			const auto Digest = FinishDigest();
			char aText[SHA256_MAXSTRSIZE];
			sha256_str(Digest, aText, sizeof(aText));
			return aText;
		}
	};
}

bool WriteEditorDocument(const CEditorDocumentValues &Document, CDataFileWriter &Writer, std::string &Error)
{
	for(const auto &Image : Document.m_Images.Read())
		if(!Image.m_External && (!Image.m_Content || Image.m_Format != CImageInfo::FORMAT_RGBA))
		{
			Error = "An embedded image is unavailable or is not in RGBA format";
			return false;
		}
	for(const auto &Sound : Document.m_Sounds.Read())
		if(Sound.DataSize() == 0)
		{
			Error = "An embedded sound is unavailable";
			return false;
		}
	CWriterOutput Output(Writer);
	if(!ExportEditorDocument(Document, Output, Error))
		return false;
	if(!Output.Success())
	{
		Error = "The document exceeds the native map format limits";
		return false;
	}
	return true;
}

namespace
{
	/** Reusable native SHA-256 where available; byte-identical portable fallback. */
	class CContentHash
	{
#if defined(CONF_FAMILY_WINDOWS)
		BCRYPT_ALG_HANDLE m_Algorithm = nullptr;
		BCRYPT_HASH_HANDLE m_Hash = nullptr;
#endif
	public:
		explicit CContentHash(CEditorDocumentFingerprint::EHashBackend Backend)
		{
#if defined(CONF_FAMILY_WINDOWS)
			if(Backend == CEditorDocumentFingerprint::EHashBackend::PLATFORM)
			{
				if(BCryptOpenAlgorithmProvider(&m_Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
					m_Algorithm = nullptr;
				else if(BCryptCreateHash(m_Algorithm, &m_Hash, nullptr, 0, nullptr, 0, BCRYPT_HASH_REUSABLE_FLAG) < 0)
					m_Hash = nullptr;
			}
#else
			(void)Backend;
#endif
		}
		~CContentHash()
		{
#if defined(CONF_FAMILY_WINDOWS)
			if(m_Hash)
				BCryptDestroyHash(m_Hash);
			if(m_Algorithm)
				BCryptCloseAlgorithmProvider(m_Algorithm, 0);
#endif
		}
		CContentHash(const CContentHash &) = delete;
		CContentHash &operator=(const CContentHash &) = delete;
		SHA256_DIGEST Digest(const void *pData, std::size_t Size)
		{
#if defined(CONF_FAMILY_WINDOWS)
			if(m_Hash)
			{
				auto *pBytes = static_cast<const unsigned char *>(pData);
				std::size_t Remaining = Size;
				bool Success = true;
				while(Remaining != 0 && Success)
				{
					const auto Count = static_cast<ULONG>(std::min<std::size_t>(Remaining, std::numeric_limits<ULONG>::max()));
					Success = BCryptHashData(m_Hash, const_cast<unsigned char *>(pBytes), Count, 0) >= 0;
					pBytes += Count;
					Remaining -= Count;
				}
				SHA256_DIGEST Result;
				if(Success && BCryptFinishHash(m_Hash, Result.data, sizeof(Result.data), 0) >= 0)
					return Result;
				// A failed reusable operation must not contaminate any subsequent key.
				BCryptDestroyHash(m_Hash);
				m_Hash = nullptr;
			}
#endif
			return sha256(pData, Size);
		}
	};

	template<typename TValue>
	class CPlaneFingerprint
	{
		using CPlane = typename CEditorTilePlane<TValue>::CPlane;
		static constexpr std::size_t EDGE = CEditorTilePlane<TValue>::CHUNK_EDGE;
		using CFormat = typename CTileFormat<TValue>::CFormat;
		using CExportChunk = std::array<CFormat, EDGE * EDGE>;
		class CSubtreeDigest
		{
		public:
			SHA256_DIGEST m_Hash{};
			std::array<unsigned char, 32> m_aUsedIndices{};
			std::size_t m_NonzeroIndices = 0;
		};
		std::optional<CEditorTilePlane<TValue>> m_Previous;
		std::array<unsigned char, 256> m_aFlags{};
		typename CPlane::template CDigestCache<CSubtreeDigest> m_Cache;
		std::vector<CSubtreeDigest> m_vEmptyDigests;

	public:
		SHA256_DIGEST Refresh(const CEditorTilePlane<TValue> &Plane, const std::array<unsigned char, 256> &aFlags, CEditorDocumentFingerprint::CStatistics &Statistics, CContentHash &Hasher)
		{
			const bool Reset = !m_Previous || Plane.Width() != m_Previous->Width() || Plane.Height() != m_Previous->Height();
			if(Reset)
			{
				m_Cache = {};
				m_vEmptyDigests.clear();
				Statistics.m_TileChunks += Plane.Plane().ChunkCount();
			}
			else if(aFlags != m_aFlags)
				Statistics.m_TileChunks += Plane.Plane().ChunkCount();
			else
				Plane.Plane().VisitChangedChunks(&m_Previous->Plane(), [&](auto...) { ++Statistics.m_TileChunks; });
			const auto HashChunk = [&](std::span<const TValue, EDGE * EDGE> Cells) {
				CExportChunk aTiles{};
				CSubtreeDigest Result;
				const auto *pCells = Cells.data();
				auto *pTiles = aTiles.data();
				auto *pUsedIndices = Result.m_aUsedIndices.data();
				const auto Count = aTiles.size();
				for(std::size_t Index = 0; Index < Count; ++Index)
				{
					pTiles[Index] = static_cast<CFormat>(pCells[Index]);
					if constexpr(std::is_same_v<TValue, CTileValues>)
						if(pCells[Index].m_Index != 0)
						{
							const auto Tile = pCells[Index].m_Index;
							pUsedIndices[Tile / 8] |= 1 << (Tile % 8);
							++Result.m_NonzeroIndices;
						}
				}
				++Statistics.m_TileLeafHashes;
				Result.m_Hash = Hasher.Digest(aTiles.data(), sizeof(aTiles));
				return Result;
			};
			const auto HashBranch = [&](const std::array<CSubtreeDigest, 16> &aDigests) {
				CSubtreeDigest Result;
				std::array<SHA256_DIGEST, 16> aHashes;
				for(std::size_t Child = 0; Child < aDigests.size(); ++Child)
				{
					aHashes[Child] = aDigests[Child].m_Hash;
					if constexpr(std::is_same_v<TValue, CTileValues>)
					{
						Result.m_NonzeroIndices += aDigests[Child].m_NonzeroIndices;
						for(std::size_t Index = 0; Index < Result.m_aUsedIndices.size(); ++Index)
							Result.m_aUsedIndices[Index] |= aDigests[Child].m_aUsedIndices[Index];
					}
				}
				++Statistics.m_TileBranchHashes;
				Result.m_Hash = Hasher.Digest(aHashes.data(), sizeof(aHashes));
				return Result;
			};
			if(m_vEmptyDigests.empty())
				m_vEmptyDigests.push_back(HashChunk(std::array<TValue, EDGE * EDGE>{}));
			while(m_vEmptyDigests.size() <= Plane.Plane().TreeDepth())
			{
				std::array<CSubtreeDigest, 16> aDigests;
				aDigests.fill(m_vEmptyDigests.back());
				m_vEmptyDigests.push_back(HashBranch(aDigests));
			}
			auto Digest = Plane.Plane().Digest(m_Cache, std::span<const CSubtreeDigest>(m_vEmptyDigests), HashChunk, HashBranch);
			m_Previous = Plane;
			m_aFlags = aFlags;
			if constexpr(std::is_same_v<TValue, CTileValues>)
			{
				// Opacity is independent of transform flags. Include it only for
				// indices used by valid cells: normalized chunk/tree padding must
				// never make an unused external image tile affect file dirtiness.
				if(Digest.m_NonzeroIndices < Plane.Size())
					Digest.m_aUsedIndices[0] |= 1;
				std::array<unsigned char, sizeof(SHA256_DIGEST) + 256> aProjection{};
				std::copy(std::begin(Digest.m_Hash.data), std::end(Digest.m_Hash.data), aProjection.begin());
				for(std::size_t Index = 0; Index < aFlags.size(); ++Index)
					if(Digest.m_aUsedIndices[Index / 8] & (1 << (Index % 8)))
						aProjection[sizeof(SHA256_DIGEST) + Index] = aFlags[Index];
				return Hasher.Digest(aProjection.data(), aProjection.size());
			}
			return Digest.m_Hash;
		}
		void Account(editor_history::CStorageUsage &Usage) const
		{
			if(m_Previous)
				m_Previous->Account(Usage);
			m_Cache.Account(Usage);
			Usage.Add(m_vEmptyDigests.data(), m_vEmptyDigests.capacity() * sizeof(CSubtreeDigest));
		}
		void Observe(editor_history::CStorageObservation &Observation) const
		{
			if(m_Previous)
				Observation.Observe(*m_Previous);
			Observation.Add(&m_Cache, m_Cache.ChildrenBytes());
			Observation.Add(m_vEmptyDigests.data(), m_vEmptyDigests.capacity() * sizeof(CSubtreeDigest));
		}
	};
}

class CEditorDocumentFingerprint::CCache : public CKeyOutput
{
	using CPlaneSlot = std::variant<CPlaneFingerprint<CTileValues>, CPlaneFingerprint<CTeleTileValues>, CPlaneFingerprint<CSpeedupTileValues>, CPlaneFingerprint<CSwitchTileValues>, CPlaneFingerprint<CTuneTileValues>>;
	struct CImageSlot
	{
		CEditorImageValues m_Previous;
		std::array<unsigned char, 256> m_aFlags{};
	};
	struct CObjectSlot
	{
		std::weak_ptr<const CLayerDocumentValues> m_Previous;
		std::uint64_t m_Generation = 0;
		std::vector<std::uint64_t> m_vEnvelopeIds;
		SHA256_DIGEST m_Digest{};
	};
	CContentHash m_Hasher;
	const EHashBackend m_Backend;
	std::vector<CPlaneSlot> m_vPlanes;
	std::vector<CImageSlot> m_vImages;
	std::vector<CObjectSlot> m_vObjects;
	std::vector<std::uint64_t> m_vEnvelopeIds;
	std::map<std::weak_ptr<const void>, SHA256_DIGEST, std::owner_less<std::weak_ptr<const void>>> m_BlobDigests;
	std::size_t m_PlaneIndex = 0;
	std::size_t m_ImageIndex = 0;
	std::size_t m_ObjectIndex = 0;

public:
	explicit CCache(EHashBackend Backend) :
		m_Hasher(Backend), m_Backend(Backend) {}
	EHashBackend Backend() const { return m_Backend; }
	CStatistics m_Statistics;
	void Discard()
	{
		m_vPlanes.clear();
		m_vImages.clear();
		m_vObjects.clear();
		m_vEnvelopeIds.clear();
		m_BlobDigests.clear();
	}

	std::string Key(const CEditorDocumentValues &Document)
	{
		sha256_init(&m_Context);
		m_DataCount = m_ItemCount = 0;
		m_PlaneIndex = m_ImageIndex = m_ObjectIndex = 0;
		m_vEnvelopeIds.clear();
		for(const auto &Envelope : Document.m_Envelopes.Read())
			m_vEnvelopeIds.push_back(Envelope.Read().m_Id);
		m_Statistics = {};
		std::string Error;
		const bool Success = ExportEditorDocument(Document, *this, Error);
		m_vPlanes.resize(m_PlaneIndex);
		m_vImages.resize(m_ImageIndex);
		m_vObjects.resize(m_ObjectIndex);
		std::erase_if(m_BlobDigests, [](const auto &Entry) { return Entry.first.expired(); });
		return Success ? Finish() : std::string{};
	}

	int AddResourceBlob(const editor_history::CResourceBlob &Blob) override
	{
		const auto Identity = Blob.StorageIdentity();
		auto It = m_BlobDigests.find(Identity);
		if(It == m_BlobDigests.end())
		{
			It = m_BlobDigests.emplace(Identity, m_Hasher.Digest(Blob.Bytes().data(), Blob.Bytes().size())).first;
			++m_Statistics.m_BlobScans;
		}
		Number(2);
		Number(Blob.Bytes().size());
		Bytes(sizeof(It->second), &It->second, false);
		return m_DataCount++;
	}

	int AddLayerObjects(const editor_history::CSharedValue<CLayerDocumentValues> &Layer, const CEditorDocumentValues &Document) override
	{
		if(m_vObjects.size() == m_ObjectIndex)
			m_vObjects.emplace_back();
		auto &Slot = m_vObjects[m_ObjectIndex++];
		const auto Owner = Layer.Pin();
		if(Slot.m_Previous.lock() != Owner || Slot.m_Generation != Layer.Generation() || Slot.m_vEnvelopeIds != m_vEnvelopeIds)
		{
			CKeyOutput Output;
			ExportLayerObjects(Layer.Read(), Document, [&](std::size_t Size, const void *pData) { return Output.AddDataSwapped(Size, pData); });
			const auto Digest = Output.FinishDigest();
			// Publish the cache identity after preparing the complete projection.
			Slot.m_vEnvelopeIds = m_vEnvelopeIds;
			Slot.m_Digest = Digest;
			Slot.m_Previous = Owner;
			Slot.m_Generation = Layer.Generation();
			++m_Statistics.m_ObjectBlocks;
		}
		Number(5);
		Bytes(sizeof(Slot.m_Digest), &Slot.m_Digest, false);
		return m_DataCount++;
	}

	int AddTilePlane(CEditorTilePayload Plane, const std::array<unsigned char, 256> &aFlags) override
	{
		if(m_vPlanes.size() == m_PlaneIndex)
			m_vPlanes.emplace_back();
		auto &Slot = m_vPlanes[m_PlaneIndex++];
		return std::visit([&](const auto *pPlane) {
			using TValue = std::remove_cvref_t<decltype((*pPlane)[0])>;
			using CSlot = CPlaneFingerprint<TValue>;
			if(!std::holds_alternative<CSlot>(Slot))
				Slot.emplace<CSlot>();
			const auto Digest = std::get<CSlot>(Slot).Refresh(*pPlane, aFlags, m_Statistics, m_Hasher);
			Number(3);
			Number(pPlane->Width());
			Number(pPlane->Height());
			Number(sizeof(typename CTileFormat<TValue>::CFormat));
			Bytes(sizeof(Digest), &Digest, false);
			return m_DataCount++;
		},
			Plane);
	}

	int AddEmptyTiles(std::size_t Count) override
	{
		Number(4);
		Number(Count * sizeof(CTile));
		return m_DataCount++;
	}

	std::array<unsigned char, 256> ImageTileFlags(const CEditorImageValues &Image) override
	{
		if(m_vImages.size() == m_ImageIndex)
			m_vImages.emplace_back();
		auto &Slot = m_vImages[m_ImageIndex++];
		const auto &Previous = Slot.m_Previous;
		// Names, IDs, and external/embedded choice do not affect pixel opacity.
		if(Image.m_Width != Previous.m_Width || Image.m_Height != Previous.m_Height ||
			Image.m_Format != Previous.m_Format || Image.m_Content != Previous.m_Content)
		{
			Slot.m_aFlags = EditorImageTileFlags(Image);
			++m_Statistics.m_ImagesAnalyzed;
		}
		Slot.m_Previous = Image;
		return Slot.m_aFlags;
	}

	void Account(editor_history::CStorageUsage &Usage) const
	{
		Usage.Add(this, sizeof(*this));
		Usage.Add(m_vPlanes.data(), m_vPlanes.capacity() * sizeof(CPlaneSlot));
		for(const auto &Slot : m_vPlanes)
			std::visit([&](const auto &Plane) { Plane.Account(Usage); }, Slot);
		Usage.Add(m_vImages.data(), m_vImages.capacity() * sizeof(CImageSlot));
		Usage.Add(m_vObjects.data(), m_vObjects.capacity() * sizeof(CObjectSlot));
		Usage.Add(m_vEnvelopeIds.data(), m_vEnvelopeIds.capacity() * sizeof(std::uint64_t));
		for(const auto &Slot : m_vObjects)
		{
			Usage.Add(Slot.m_vEnvelopeIds.data(), Slot.m_vEnvelopeIds.capacity() * sizeof(std::uint64_t));
		}
		for(const auto &Slot : m_vImages)
			if(Slot.m_Previous.m_Content)
				Slot.m_Previous.m_Content->Account(Usage);
		// Tree node/allocator overhead is excluded consistently with document accounting.
		for(const auto &Entry : m_BlobDigests)
			Usage.Add(&Entry, sizeof(Entry));
	}
	void Observe(editor_history::CStorageObservation &Observation) const
	{
		Observation.Add(this, sizeof(*this));
		Observation.Add(m_vPlanes.data(), m_vPlanes.capacity() * sizeof(CPlaneSlot));
		for(const auto &Slot : m_vPlanes)
			std::visit([&](const auto &Plane) { Plane.Observe(Observation); }, Slot);
		Observation.Add(m_vImages.data(), m_vImages.capacity() * sizeof(CImageSlot));
		Observation.Add(m_vObjects.data(), m_vObjects.capacity() * sizeof(CObjectSlot));
		Observation.Add(m_vEnvelopeIds.data(), m_vEnvelopeIds.capacity() * sizeof(std::uint64_t));
		for(const auto &Slot : m_vObjects)
			Observation.Add(Slot.m_vEnvelopeIds.data(), Slot.m_vEnvelopeIds.capacity() * sizeof(std::uint64_t));
		for(const auto &Slot : m_vImages)
			if(Slot.m_Previous.m_Content)
				Observation.Observe(*Slot.m_Previous.m_Content);
		Observation.Add(&m_BlobDigests, m_BlobDigests.size() * sizeof(decltype(m_BlobDigests)::value_type));
	}
};

CEditorDocumentFingerprint::CEditorDocumentFingerprint() :
	CEditorDocumentFingerprint(EHashBackend::PLATFORM) {}
CEditorDocumentFingerprint::CEditorDocumentFingerprint(EHashBackend Backend) :
	m_pCache(std::make_unique<CCache>(Backend)) {}
CEditorDocumentFingerprint::~CEditorDocumentFingerprint() = default;
std::string CEditorDocumentFingerprint::Key(const CEditorDocumentValues &Document)
{
	++m_Generation;
	auto Key = m_pCache->Key(Document);
	if(Key.empty())
		m_pCache->Discard();
	return Key;
}
const CEditorDocumentFingerprint::CStatistics &CEditorDocumentFingerprint::Statistics() const { return m_pCache->m_Statistics; }
void CEditorDocumentFingerprint::Account(editor_history::CStorageUsage &Usage) const { m_pCache->Account(Usage); }
void CEditorDocumentFingerprint::Observe(editor_history::CStorageObservation &Observation) const { m_pCache->Observe(Observation); }
void CEditorDocumentFingerprint::Clear()
{
	++m_Generation;
	m_pCache = std::make_unique<CCache>(m_pCache->Backend());
}

std::string EditorDocumentPersistedKey(const CEditorDocumentValues &Document)
{
	CEditorDocumentFingerprint Fingerprint;
	return Fingerprint.Key(Document);
}
