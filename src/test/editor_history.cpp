#include "test.h"

#include <base/hash.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/editor/history/tile_plane.h>
#include <game/editor/history/tile_projection.h>
#include <game/editor/mapitems/document_export.h>
#include <game/editor/mapitems/document_graph.h>
#include <game/editor/mapitems/document_session.h>
#include <game/editor/mapitems/document_values.h>
#include <game/editor/mapitems/envelope.h>
#include <game/map/render_map.h>
#include <game/mapitems_ex.h>

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace editor_history;

namespace
{

	class CTestTile
	{
	public:
		int m_Index = 0;
		int m_Flags = 0;
		bool operator==(const CTestTile &) const = default;
	};

	using CTestPlane = CTilePlane<CTestTile, 16>;

} // namespace

TEST(EditorHistoryStorage, SharedValuesCompareContentsAndDetach)
{
	CSharedVector<int> Original(std::vector<int>{1, 2, 3});
	auto Draft = Original;
	EXPECT_TRUE(Draft.SharesStorageWith(Original));
	Draft.Update([](auto &vValues) { vValues[1] = 7; });
	EXPECT_FALSE(Draft.SharesStorageWith(Original));
	EXPECT_EQ(Original.Read()[1], 2);
	EXPECT_EQ(Draft.Read()[1], 7);
	Draft.Update([](auto &vValues) { vValues[1] = 2; });
	EXPECT_EQ(Draft, Original);
	CStorageUsage Usage;
	EXPECT_TRUE(Original.Account(Usage));
	const auto Bytes = Usage.Bytes();
	auto Shared = Original; // NOLINT(performance-unnecessary-copy-initialization): Test owning copies and detachment.
	EXPECT_FALSE(Shared.Account(Usage));
	EXPECT_EQ(Usage.Bytes(), Bytes);
}

TEST(EditorHistoryStorage, SparseEditCopiesOnlyTouchedChunks)
{
	auto Baseline = *CTestPlane::Create(1024, 1024);
	auto Draft = Baseline;
	Draft.Set(17, 19, {3, 4});
	Draft.Set(18, 19, {5, 6});
	EXPECT_EQ(Draft.SharedChunks(Baseline), Baseline.ChunkCount() - 1);
	EXPECT_EQ(Baseline.At(17, 19), CTestTile{});
	EXPECT_EQ(Draft.At(17, 19), (CTestTile{3, 4}));
	auto Frozen = Draft;
	Draft.Set(17, 19, {7, 8});
	EXPECT_EQ(Frozen.At(17, 19), (CTestTile{3, 4}));
}

TEST(EditorHistoryStorage, ResizePreservesCoordinatesAndClearsRemovedCells)
{
	auto Plane = *CTestPlane::Create(35, 21);
	Plane.Set(4, 5, {11, 2});
	Plane.Set(34, 20, {27, 3});
	const auto Baseline = Plane;
	ASSERT_TRUE(Plane.Resize(65, 40));
	EXPECT_EQ(Plane.At(34, 20), (CTestTile{27, 3}));
	EXPECT_EQ(Plane.At(35, 20), CTestTile{});
	EXPECT_EQ(Plane.SharedChunks(Baseline), Baseline.ChunkCount());
	ASSERT_TRUE(Plane.Resize(20, 18));
	ASSERT_TRUE(Plane.Resize(65, 40));
	EXPECT_EQ(Plane.At(4, 5), (CTestTile{11, 2}));
	EXPECT_EQ(Plane.At(34, 20), CTestTile{});
	EXPECT_EQ(Baseline.At(34, 20), (CTestTile{27, 3}));
}

TEST(EditorHistoryStorage, OverflowDoesNotModifyPlane)
{
	auto Plane = *CTestPlane::Create(32, 32);
	Plane.Set(1, 2, {5, 6});
	const auto Baseline = Plane;
	EXPECT_FALSE(CTestPlane::Create(std::numeric_limits<std::size_t>::max(), 1));
	EXPECT_FALSE(Plane.Resize(1, std::numeric_limits<std::size_t>::max()));
	EXPECT_EQ(Plane, Baseline);
}

TEST(EditorHistorySchema, TileFlagRotationHasExpectedOrientationForEveryCombination)
{
	const std::array<unsigned char, 8> aBefore{0, 1, 2, 3, 8, 9, 10, 11};
	const std::array<unsigned char, 8> aAfter{8, 9, 10, 11, 3, 2, 1, 0};
	for(std::size_t Index = 0; Index < aBefore.size(); ++Index)
	{
		CTileValues Tile(CTile{37, aBefore[Index]});
		RotateTileFlagsClockwise(Tile);
		EXPECT_EQ(Tile.m_Index, 37);
		EXPECT_EQ(Tile.m_Flags, aAfter[Index]);
	}
}

namespace
{
	CEditorDocumentValues MakeCompleteDocument()
	{
		CDocumentIdentityAllocator Ids;
		const auto NextId = [&] { return *Ids.Allocate(); };
		CEditorDocumentValues Document;
		str_copy(Document.m_MapInfo.m_Author.Buffer(), "mapper");
		str_copy(Document.m_MapInfo.m_Version.Buffer(), "2.3");
		str_copy(Document.m_MapInfo.m_Credits.Buffer(), "contributors");
		str_copy(Document.m_MapInfo.m_License.Buffer(), "CC0");
		Document.m_vSettings.emplace_back("sv_test_cmd 1");
		Document.m_vSettings.emplace_back("sv_test_cmd 2");

		CEditorImageValues Image;
		Image.m_Id = NextId();
		str_copy(Image.m_aName, "retained image");
		Image.m_External = 1;
		Image.m_Width = 2;
		Image.m_Height = 2;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.m_Content.emplace(std::vector<std::uint8_t>(16, 31));
		Document.m_Images.Update([&](auto &vImages) { vImages.push_back(Image); });
		CEditorSoundValues Sound;
		Sound.m_Id = NextId();
		str_copy(Sound.m_aName, "retained sound");
		Sound.m_Content.emplace(std::vector<std::uint8_t>{1, 7, 9, 3});
		Document.m_Sounds.Update([&](auto &vSounds) { vSounds.push_back(Sound); });
		for(const auto Type : {CEnvelopeValues::EType::POSITION, CEnvelopeValues::EType::COLOR, CEnvelopeValues::EType::SOUND})
		{
			CEnvelopeValues Envelope;
			Envelope.m_Id = NextId();
			Envelope.m_Type = Type;
			Envelope.m_Synchronized = false;
			str_copy(Envelope.m_aName, "envelope");
			for(int Index = 0; Index < 2; ++Index)
			{
				CEnvelopePointValues Point;
				Point.m_Id = NextId();
				Point.m_Time = CFixedTime(Index * 2000);
				Point.m_Curvetype = Index == 0 ? CURVETYPE_BEZIER : CURVETYPE_SMOOTH;
				for(int Channel = 0; Channel < 4; ++Channel)
				{
					Point.m_aValues[Channel] = 100 + Index * 10 + Channel;
					Point.m_Bezier.m_aInTangentDeltaX[Channel] = CFixedTime(200 + Channel);
					Point.m_Bezier.m_aInTangentDeltaY[Channel] = 300 + Channel;
					Point.m_Bezier.m_aOutTangentDeltaX[Channel] = CFixedTime(400 + Channel);
					Point.m_Bezier.m_aOutTangentDeltaY[Channel] = 500 + Channel;
				}
				Envelope.m_vPoints.push_back(Point);
			}
			Document.m_Envelopes.Update([&](auto &vEnvelopes) { vEnvelopes.emplace_back(Envelope); });
		}
		CGroupDocumentValues Group;
		Group.m_Id = NextId();
		Group.m_GameGroup = true;
		str_copy(Group.m_aName, "Game");
		Group.m_OffsetX = 47;
		Group.m_OffsetY = -13;
		Group.m_ParallaxX = 87;
		Group.m_ParallaxY = 91;
		Group.m_UseClipping = 1;
		Group.m_ClipX = 5;
		Group.m_ClipY = 9;
		Group.m_ClipW = 400;
		Group.m_ClipH = 600;
		for(int Role = 0; Role < 7; ++Role)
		{
			CLayerDocumentValues Layer;
			Layer.m_Id = NextId();
			Layer.m_Type = LAYERTYPE_TILES;
			Layer.m_Flags = LAYERFLAG_DETAIL;
			str_copy(Layer.m_aName, "tiles");
			CTileLayerDocumentValues Tiles;
			Tiles.m_Tiles.Resize(2, 2);
			Tiles.m_Tiles.Set(3, CTileValues(CTile{17, TILEFLAG_XFLIP, 0, 0}));
			Tiles.m_HasGame = Role == 0;
			Tiles.m_HasFront = Role == 1;
			Tiles.m_HasTele = Role == 2;
			Tiles.m_HasSpeedup = Role == 3;
			Tiles.m_HasSwitch = Role == 4;
			Tiles.m_HasTune = Role == 5;
			Tiles.m_Image = {Image.m_Id};
			Tiles.m_Color = CColor{11, 22, 33, 44};
			Tiles.m_ColorEnv = Document.EnvelopeReference(1);
			Tiles.m_ColorEnvOffset = 321;
			Tiles.m_AutomapperConfig = 3;
			Tiles.m_AutomapperReference = 4;
			Tiles.m_AutoAutomapper = true;
			Tiles.m_Seed = 73;
			Tiles.m_LiveGameTiles = true;
			Tiles.m_FillGameTile = TILE_DEATH;
			if(Role == 2)
			{
				CLayerTeleValues Auxiliary;
				Auxiliary.m_TeleTiles.Resize(2, 2);
				Auxiliary.m_TeleTiles.Set(3, CTeleTileValues(CTeleTile{9, TILE_TELEIN}));
				Tiles.m_Auxiliary = Auxiliary;
			}
			else if(Role == 3)
			{
				CLayerSpeedupValues Auxiliary;
				Auxiliary.m_SpeedupTiles.Resize(2, 2);
				Auxiliary.m_SpeedupTiles.Set(3, CSpeedupTileValues(CSpeedupTile{12, 34, TILE_SPEED_BOOST, 0, 267}));
				Tiles.m_Auxiliary = Auxiliary;
			}
			else if(Role == 4)
			{
				CLayerSwitchValues Auxiliary;
				Auxiliary.m_SwitchTiles.Resize(2, 2);
				Auxiliary.m_SwitchTiles.Set(3, CSwitchTileValues(CSwitchTile{8, TILE_SWITCHOPEN, TILEFLAG_ROTATE, 15}));
				Tiles.m_Auxiliary = Auxiliary;
			}
			else if(Role == 5)
			{
				CLayerTuneValues Auxiliary;
				Auxiliary.m_TuneTiles.Resize(2, 2);
				Auxiliary.m_TuneTiles.Set(3, CTuneTileValues(CTuneTile{10, TILE_TUNE}));
				Tiles.m_Auxiliary = Auxiliary;
			}
			Layer.m_Data = std::move(Tiles);
			Group.m_Layers.Update([&](auto &vLayers) { vLayers.emplace_back(Layer); });
		}
		CLayerDocumentValues QuadLayer;
		QuadLayer.m_Id = NextId();
		QuadLayer.m_Type = LAYERTYPE_QUADS;
		CLayerQuadsValues Quads;
		Quads.m_Image = {Image.m_Id};
		CQuadValues Quad;
		Quad.m_Id = NextId();
		for(int Index = 0; Index < 5; ++Index)
			Quad.m_aPoints[Index] = CPoint{1000 + Index, 2000 + Index};
		for(int Index = 0; Index < 4; ++Index)
		{
			Quad.m_aColors[Index] = CColor{10 + Index, 20 + Index, 30 + Index, 40 + Index};
			Quad.m_aTexcoords[Index] = CPoint{3000 + Index, 4000 + Index};
		}
		Quad.m_PosEnv = Document.EnvelopeReference(0);
		Quad.m_PosEnvOffset = 125;
		Quad.m_ColorEnv = Document.EnvelopeReference(1);
		Quad.m_ColorEnvOffset = -75;
		Quads.m_vQuads.push_back(Quad);
		QuadLayer.m_Data = Quads;
		Group.m_Layers.Update([&](auto &vLayers) { vLayers.emplace_back(QuadLayer); });
		CLayerDocumentValues SoundLayer;
		SoundLayer.m_Id = NextId();
		SoundLayer.m_Type = LAYERTYPE_SOUNDS;
		CLayerSoundsValues Sources;
		Sources.m_Sound = {Sound.m_Id};
		for(int Shape = 0; Shape < CSoundShape::NUM_SHAPES; ++Shape)
		{
			CSoundSourceValues Source;
			Source.m_Id = NextId();
			Source.m_Position = CPoint{81, -23};
			Source.m_Loop = 1;
			Source.m_Pan = 1;
			Source.m_TimeDelay = 12;
			Source.m_Falloff = 56;
			Source.m_PosEnv = Document.EnvelopeReference(0);
			Source.m_PosEnvOffset = 234;
			Source.m_SoundEnv = Document.EnvelopeReference(2);
			Source.m_SoundEnvOffset = 345;
			Source.m_Shape.m_Type = Shape;
			if(Shape == CSoundShape::SHAPE_RECTANGLE)
				Source.m_Shape.m_Rectangle = {456, 567};
			else
				Source.m_Shape.m_Circle.m_Radius = 678;
			Sources.m_vSources.push_back(Source);
		}
		SoundLayer.m_Data = Sources;
		Group.m_Layers.Update([&](auto &vLayers) { vLayers.emplace_back(SoundLayer); });
		Document.m_Groups.Update([&](auto &vGroups) { vGroups.emplace_back(Group); });
		return Document;
	}

	template<typename F>
	void UpdateDocumentLayer(CEditorDocumentValues &Document, std::size_t Index, F Function)
	{
		Document.m_Groups.Update([&](auto &vGroups) {
			vGroups[0].Update([&](auto &Group) {
				Group.m_Layers.Update([&](auto &vLayers) { vLayers[Index].Update(Function); });
			});
		});
	}

	const CLayerDocumentValues &DocumentLayer(const CEditorDocumentValues &Document, std::size_t Index)
	{
		return Document.m_Groups.Read()[0].Read().m_Layers.Read()[Index].Read();
	}
}

TEST(EditorDocumentExport, PersistedContentIgnoresInternalIdsAndHistoryOnlySettings)
{
	const auto Baseline = MakeCompleteDocument();
	const auto Key = EditorDocumentPersistedKey(Baseline);
	ASSERT_EQ(Key.size(), 64u);
	auto Changed = Baseline;
	const auto Remap = [](CDocumentReference &Reference) { if(Reference.IsSet()) Reference.m_Id += 1000; };
	Changed.m_Images.Update([](auto &vImages) { for(auto &Image : vImages) Image.m_Id += 1000; });
	Changed.m_Sounds.Update([](auto &vSounds) { for(auto &Sound : vSounds) Sound.m_Id += 1000; });
	Changed.m_Envelopes.Update([](auto &vEnvelopes) {
		for(auto &Record : vEnvelopes)
			Record.Update([](auto &Envelope) { Envelope.m_Id += 1000; for(auto &Point : Envelope.m_vPoints) Point.m_Id += 1000; });
	});
	Changed.m_Groups.Update([&](auto &vGroups) {
		for(auto &GroupRecord : vGroups)
			GroupRecord.Update([&](auto &Group) {
				Group.m_Id += 1000;
				Group.m_Layers.Update([&](auto &vLayers) {
					for(auto &LayerRecord : vLayers)
						LayerRecord.Update([&](auto &Layer) {
							Layer.m_Id += 1000;
							std::visit([&](auto &Data) {
								using T = std::decay_t<decltype(Data)>;
								if constexpr(std::is_same_v<T, CTileLayerDocumentValues>)
								{
									Remap(Data.m_Image);
									Remap(Data.m_ColorEnv);
									Data.m_LiveGameTiles = !Data.m_LiveGameTiles;
									Data.m_FillGameTile = TILE_SOLID;
									Data.m_AutomapperReference++;
								}
								else if constexpr(std::is_same_v<T, CLayerQuadsValues>)
								{
									Remap(Data.m_Image);
									for(auto &Quad : Data.m_vQuads)
									{
										Quad.m_Id += 1000;
										Remap(Quad.m_PosEnv);
										Remap(Quad.m_ColorEnv);
									}
								}
								else
								{
									Remap(Data.m_Sound);
									for(auto &Source : Data.m_vSources)
									{
										Source.m_Id += 1000;
										Remap(Source.m_PosEnv);
										Remap(Source.m_SoundEnv);
									}
								}
							},
								Layer.m_Data);
						});
				});
			});
	});
	EXPECT_NE(Changed, Baseline);
	EXPECT_EQ(EditorDocumentPersistedKey(Changed), Key);
	str_copy(Changed.m_MapInfo.m_Credits.Buffer(), "new credits");
	EXPECT_NE(EditorDocumentPersistedKey(Changed), Key);
	EXPECT_EQ(EditorDocumentPersistedKey(Baseline), Key);
}

TEST(EditorDocumentExport, FingerprintIncludesOnlyExportedBezierTangents)
{
	const auto Baseline = MakeCompleteDocument();
	const auto Key = EditorDocumentPersistedKey(Baseline);
	auto Changed = Baseline;
	Changed.m_Envelopes.Update([](auto &vEnvelopes) {
		vEnvelopes[0].Update([](auto &Envelope) {
			Envelope.m_vPoints[0].m_Bezier.m_aInTangentDeltaY[1] += 29;
			Envelope.m_vPoints[1].m_Bezier.m_aOutTangentDeltaY[1] += 41;
		});
	});
	EXPECT_NE(Changed, Baseline);
	EXPECT_EQ(EditorDocumentPersistedKey(Changed), Key);
	Changed.m_Envelopes.Update([](auto &vEnvelopes) {
		vEnvelopes[0].Update([](auto &Envelope) { Envelope.m_vPoints[0].m_Bezier.m_aOutTangentDeltaY[1] += 2; });
	});
	EXPECT_NE(EditorDocumentPersistedKey(Changed), Key);
	Changed = Baseline;
	Changed.m_Envelopes.Update([](auto &vEnvelopes) {
		vEnvelopes[0].Update([](auto &Envelope) { Envelope.m_vPoints[1].m_Bezier.m_aInTangentDeltaY[1] += 3; });
	});
	EXPECT_NE(EditorDocumentPersistedKey(Changed), Key);
}

TEST(EditorDocumentExport, PureNativeMapProjectionPreservesAllLayerVariants)
{
	const auto Document = MakeCompleteDocument();
	const auto Baseline = Document; // NOLINT(performance-unnecessary-copy-initialization): Retain an owning roundtrip baseline.
	const auto KeyBefore = EditorDocumentPersistedKey(Document);
	CTestInfo Info;
	const auto pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	std::string Error;
	{
		CDataFileWriter Writer;
		ASSERT_TRUE(Writer.Open(pStorage.get(), "roundtrip.map"));
		ASSERT_TRUE(WriteEditorDocument(Document, Writer, Error)) << Error;
		Writer.Finish();
	}
	CDataFileReader Reader;
	ASSERT_TRUE(Reader.Open(pStorage.get(), "roundtrip.map", IStorage::TYPE_SAVE));
	const auto *pInfo = static_cast<const CMapItemInfoSettings *>(Reader.FindItem(MAPITEMTYPE_INFO, 0));
	ASSERT_NE(pInfo, nullptr);
	EXPECT_STREQ(Reader.GetDataString(pInfo->m_Author), "mapper");
	EXPECT_STREQ(Reader.GetDataString(pInfo->m_Credits), "contributors");
	const auto *pSettings = static_cast<const char *>(Reader.GetData(pInfo->m_Settings));
	ASSERT_NE(pSettings, nullptr);
	EXPECT_STREQ(pSettings, "sv_test_cmd 1");
	EXPECT_STREQ(pSettings + str_length(pSettings) + 1, "sv_test_cmd 2");
	const std::array<int, 7> aRoles{TILESLAYERFLAG_GAME, TILESLAYERFLAG_FRONT, TILESLAYERFLAG_TELE, TILESLAYERFLAG_SPEEDUP, TILESLAYERFLAG_SWITCH, TILESLAYERFLAG_TUNE, 0};
	for(int Index = 0; Index < 7; ++Index)
	{
		const auto *pTiles = static_cast<const CMapItemLayerTilemap *>(Reader.FindItem(MAPITEMTYPE_LAYER, Index));
		ASSERT_NE(pTiles, nullptr);
		EXPECT_EQ(pTiles->m_Width, 2);
		EXPECT_EQ(pTiles->m_Height, 2);
		EXPECT_EQ(pTiles->m_Image, 0);
		EXPECT_EQ(pTiles->m_ColorEnv, 1);
		EXPECT_EQ(pTiles->m_ColorEnvOffset, 321);
		EXPECT_EQ(pTiles->m_Flags, aRoles[Index]);
		if(Index == 2)
		{
			const auto *pTele = static_cast<const CTeleTile *>(Reader.GetData(pTiles->m_Tele));
			ASSERT_NE(pTele, nullptr);
			EXPECT_EQ(pTele[3].m_Number, 9);
			EXPECT_EQ(pTele[3].m_Type, TILE_TELEIN);
		}
		else if(Index == 3)
		{
			const auto *pSpeedup = static_cast<const CSpeedupTile *>(Reader.GetData(pTiles->m_Speedup));
			ASSERT_NE(pSpeedup, nullptr);
			EXPECT_EQ(pSpeedup[3].m_Force, 12);
			EXPECT_EQ(pSpeedup[3].m_MaxSpeed, 34);
			EXPECT_EQ(pSpeedup[3].m_Angle, 267);
		}
		else if(Index == 4)
		{
			const auto *pSwitch = static_cast<const CSwitchTile *>(Reader.GetData(pTiles->m_Switch));
			ASSERT_NE(pSwitch, nullptr);
			EXPECT_EQ(pSwitch[3].m_Number, 8);
			EXPECT_EQ(pSwitch[3].m_Type, TILE_SWITCHOPEN);
			EXPECT_EQ(pSwitch[3].m_Delay, 15);
			EXPECT_EQ(pSwitch[3].m_Flags, TILEFLAG_ROTATE);
		}
		else if(Index == 5)
		{
			const auto *pTune = static_cast<const CTuneTile *>(Reader.GetData(pTiles->m_Tune));
			ASSERT_NE(pTune, nullptr);
			EXPECT_EQ(pTune[3].m_Number, 10);
			EXPECT_EQ(pTune[3].m_Type, TILE_TUNE);
		}
		else
		{
			const auto *pData = static_cast<const CTile *>(Reader.GetData(Index == 1 ? pTiles->m_Front : pTiles->m_Data));
			ASSERT_NE(pData, nullptr);
			EXPECT_EQ(pData[3].m_Index, 17);
			EXPECT_EQ(pData[3].m_Flags, TILEFLAG_XFLIP);
			EXPECT_EQ(pData[3].m_Skip, 0);
		}
	}
	const auto *pQuadLayer = static_cast<const CMapItemLayerQuads *>(Reader.FindItem(MAPITEMTYPE_LAYER, 7));
	ASSERT_NE(pQuadLayer, nullptr);
	const auto *pQuads = static_cast<const CQuad *>(Reader.GetDataSwapped(pQuadLayer->m_Data));
	ASSERT_NE(pQuads, nullptr);
	EXPECT_EQ(pQuads[0].m_aPoints[4], (CPoint{1004, 2004}));
	EXPECT_EQ(pQuads[0].m_PosEnv, 0);
	EXPECT_EQ(pQuads[0].m_ColorEnv, 1);
	const auto *pSoundLayer = static_cast<const CMapItemLayerSounds *>(Reader.FindItem(MAPITEMTYPE_LAYER, 8));
	ASSERT_NE(pSoundLayer, nullptr);
	const auto *pSources = static_cast<const CSoundSource *>(Reader.GetDataSwapped(pSoundLayer->m_Data));
	ASSERT_NE(pSources, nullptr);
	EXPECT_EQ(pSources[0].m_Shape.m_Rectangle.m_Width, 456);
	EXPECT_EQ(pSources[1].m_Shape.m_Circle.m_Radius, 678);
	EXPECT_EQ(pSources[1].m_SoundEnv, 2);
	const auto *pBezier = static_cast<const CEnvPointBezier *>(Reader.FindItem(MAPITEMTYPE_ENVPOINTS_BEZIER, 0));
	ASSERT_NE(pBezier, nullptr);
	EXPECT_EQ(pBezier[0].m_aInTangentDeltaY[0], 0);
	EXPECT_EQ(pBezier[0].m_aOutTangentDeltaY[0], 500);
	EXPECT_EQ(pBezier[1].m_aInTangentDeltaY[0], 300);
	EXPECT_EQ(pBezier[1].m_aOutTangentDeltaY[0], 0);
	EXPECT_EQ(Document, Baseline);
	EXPECT_EQ(EditorDocumentPersistedKey(Document), KeyBefore);
	Reader.Close();
	Info.m_DeleteTestStorageFilesOnSuccess = true;
}

TEST(EditorDocumentExport, MissingResourceRejectsSaveWithoutChangingEditableDocument)
{
	auto Document = MakeCompleteDocument();
	Document.m_Sounds.Update([](auto &vSounds) { vSounds[0].m_Content.reset(); });
	const auto Baseline = Document;
	CDataFileWriter Writer;
	std::string Error;
	EXPECT_FALSE(WriteEditorDocument(Document, Writer, Error));
	EXPECT_FALSE(Error.empty());
	EXPECT_EQ(Document, Baseline);
	EXPECT_EQ(EditorDocumentPersistedKey(Document).size(), 64u);
}

TEST(EditorDocumentSchema, CompleteGraphValidatesAndRejectsBrokenIdentityReferencesAndPayloads)
{
	const auto Baseline = MakeCompleteDocument();
	std::string Error;
	ASSERT_TRUE(Baseline.Validate(Error)) << Error;
	auto Changed = Baseline;
	UpdateDocumentLayer(Changed, 1, [&](auto &Layer) { Layer.m_Id = DocumentLayer(Baseline, 0).m_Id; });
	EXPECT_FALSE(Changed.Validate(Error));
	EXPECT_NE(Error.find("identity"), std::string::npos);
	Changed = Baseline;
	UpdateDocumentLayer(Changed, 7, [](auto &Layer) { std::get<CLayerQuadsValues>(Layer.m_Data).m_vQuads[0].m_PosEnv = {999999}; });
	EXPECT_FALSE(Changed.Validate(Error));
	Changed = Baseline;
	UpdateDocumentLayer(Changed, 2, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Auxiliary = std::monostate{}; });
	EXPECT_FALSE(Changed.Validate(Error));
	Changed = Baseline;
	UpdateDocumentLayer(Changed, 3, [](auto &Layer) { std::get<CLayerSpeedupValues>(std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Auxiliary).m_SpeedupTiles.Resize(3, 2); });
	EXPECT_FALSE(Changed.Validate(Error));
	Changed = Baseline;
	UpdateDocumentLayer(Changed, 1, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Resize(3, 2); });
	EXPECT_FALSE(Changed.Validate(Error));
	Changed = Baseline;
	UpdateDocumentLayer(Changed, 8, [](auto &Layer) { std::get<CLayerSoundsValues>(Layer.m_Data).m_vSources[0].m_Shape.m_Type = 19; });
	EXPECT_FALSE(Changed.Validate(Error));
	Changed = Baseline;
	Changed.m_Envelopes.Update([](auto &vEnvelopes) { vEnvelopes[0].Update([](auto &Envelope) { std::swap(Envelope.m_vPoints[0], Envelope.m_vPoints[1]); }); });
	EXPECT_FALSE(Changed.Validate(Error));
	EXPECT_TRUE(Baseline.Validate(Error));
	EXPECT_TRUE(Error.empty());
}

TEST(EditorDocumentSchema, IdentityAllocationNeverRewindsWithCopiedValues)
{
	CDocumentIdentityAllocator Ids;
	CQuadValues Original;
	Original.m_Id = *Ids.Allocate();
	const auto Snapshot = Original;
	Original.m_Id = *Ids.Allocate();
	Original = Snapshot;
	EXPECT_EQ(Original.m_Id, 1u);
	EXPECT_EQ(*Ids.Allocate(), 3u);
	EXPECT_TRUE(Ids.ReserveThrough(100));
	EXPECT_EQ(*Ids.Allocate(), 101u);
	EXPECT_TRUE(Ids.ReserveThrough(2));
	EXPECT_EQ(*Ids.Allocate(), 102u);
	EXPECT_FALSE(Ids.ReserveThrough(std::numeric_limits<std::uint64_t>::max()));
	EXPECT_EQ(*Ids.Allocate(), 103u);
}

TEST(EditorDocumentExport, FingerprintReusesUnchangedProjectionAndRecomputesOneChunk)
{
	auto Document = MakeCompleteDocument();
	Document.m_Images.Update([](auto &vImages) { vImages[0].m_External = 0; });
	UpdateDocumentLayer(Document, 6, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Resize(65, 33); });
	CEditorDocumentFingerprint Cache;
	const auto BaselineKey = Cache.Key(Document);
	EXPECT_EQ(Cache.Statistics().m_BlobScans, 2u);
	EXPECT_EQ(Cache.Statistics().m_ImagesAnalyzed, 1u);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 12u);
	EXPECT_EQ(BaselineKey, EditorDocumentPersistedKey(Document));
	EXPECT_EQ(Cache.Key(Document), BaselineKey);
	EXPECT_EQ(Cache.Statistics().m_BlobScans, 0u);
	EXPECT_EQ(Cache.Statistics().m_ImagesAnalyzed, 0u);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 0u);

	auto Changed = Document;
	str_copy(Changed.m_MapInfo.m_Author.Buffer(), "another mapper");
	EXPECT_NE(Cache.Key(Changed), BaselineKey);
	EXPECT_EQ(Cache.Statistics().m_BlobScans, 0u);
	EXPECT_EQ(Cache.Statistics().m_ImagesAnalyzed, 0u);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 0u);
	UpdateDocumentLayer(Changed, 6, [](auto &Layer) {
		std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Update(32 * 65 + 64, [](auto &Tile) { Tile.m_Index = 13; });
	});
	EXPECT_EQ(Cache.Key(Changed), EditorDocumentPersistedKey(Changed));
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 1u);
	EXPECT_EQ(Cache.Key(Document), BaselineKey);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 1u);
}

TEST(EditorDocumentExport, FingerprintOpacityChangesOnlyRelevantTileProjection)
{
	auto Document = MakeCompleteDocument();
	Document.m_Images.Update([](auto &vImages) {
		vImages[0].m_Width = vImages[0].m_Height = 16;
		vImages[0].m_Content.emplace(std::vector<std::uint8_t>(16 * 16 * 4, 255));
	});
	UpdateDocumentLayer(Document, 6, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Color.a = 255; });
	CEditorDocumentFingerprint Cache;
	const auto BaselineKey = Cache.Key(Document);
	auto Changed = Document;
	Changed.m_Images.Update([](auto &vImages) {
		std::vector<std::uint8_t> vPixels(vImages[0].m_Content->Bytes().begin(), vImages[0].m_Content->Bytes().end());
		vPixels[17 * 4 + 3] = 0;
		vImages[0].m_Content.emplace(vPixels);
	});
	EXPECT_NE(Cache.Key(Changed), BaselineKey);
	EXPECT_EQ(Cache.Statistics().m_ImagesAnalyzed, 1u);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 1u);
	EXPECT_EQ(Cache.Statistics().m_TileLeafHashes, 0u);
	EXPECT_EQ(Cache.Statistics().m_TileBranchHashes, 0u);
	EXPECT_EQ(Cache.Key(Changed), EditorDocumentPersistedKey(Changed));
	Changed.m_Images.Update([](auto &vImages) { str_copy(vImages[0].m_aName, "renamed"); });
	EXPECT_EQ(Cache.Key(Changed), EditorDocumentPersistedKey(Changed));
	EXPECT_EQ(Cache.Statistics().m_ImagesAnalyzed, 0u);
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 0u);
}

TEST(EditorDocumentExport, FingerprintRebindsPlaneTypesAndDimensionsAfterReorder)
{
	auto Document = MakeCompleteDocument();
	CEditorDocumentFingerprint Cache;
	const auto BaselineKey = Cache.Key(Document);
	Document.m_Groups.Update([](auto &vGroups) {
		vGroups[0].Update([](auto &Group) {
			Group.m_Layers.Update([](auto &vLayers) { std::reverse(vLayers.begin(), vLayers.end()); });
		});
	});
	EXPECT_NE(Cache.Key(Document), BaselineKey);
	EXPECT_EQ(Cache.Key(Document), EditorDocumentPersistedKey(Document));
	UpdateDocumentLayer(Document, 2, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Resize(33, 17); });
	EXPECT_EQ(Cache.Key(Document), EditorDocumentPersistedKey(Document));
	EXPECT_EQ(Cache.Statistics().m_TileChunks, 2u);
}

TEST(EditorHistoryStorage, IncrementalUnionMatchesFreshAccountingAndRollsBack)
{
	auto First = *CTestPlane::Create(32768, 32768);
	First.Set(0, 0, {1, 2});
	auto Second = First;
	Second.Set(32767, 32767, {3, 4});
	CStorageUsage Incremental;
	First.Account(Incremental);
	const auto FirstBytes = Incremental.Bytes();
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Second.Account(Incremental);
		CStorageUsage Fresh;
		First.Account(Fresh);
		Second.Account(Fresh);
		EXPECT_EQ(Incremental.Bytes(), Fresh.Bytes());
		Incremental.Removing(true);
		First.Account(Incremental);
		CStorageUsage Remaining;
		Second.Account(Remaining);
		EXPECT_EQ(Incremental.Bytes(), Remaining.Bytes());
		// Simulates a later allocation failure: destruction rolls back both paths.
	}
	EXPECT_EQ(Incremental.Bytes(), FirstBytes);
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Second.Account(Incremental);
		Incremental.Removing(true);
		First.Account(Incremental);
		Transaction.Commit();
	}
	CStorageUsage Remaining;
	Second.Account(Remaining);
	EXPECT_EQ(Incremental.Bytes(), Remaining.Bytes());
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Incremental.Removing(true);
		Second.Account(Incremental);
		Transaction.Commit();
	}
	EXPECT_EQ(Incremental.Bytes(), 0u);
	EXPECT_EQ(Incremental.Allocations(), 0u);
}

TEST(EditorHistoryStorage, BulkImportNormalizesPartialChunksAndResizeStride)
{
	CEditorTilePlane<CTileValues> Plane(67, 35);
	std::vector<CTile> vCells(Plane.Size());
	for(std::size_t Index = 0; Index < vCells.size(); ++Index)
		vCells[Index] = {static_cast<unsigned char>(Index % 239), static_cast<unsigned char>(Index % 16), 99, 77};
	Plane.Assign(std::span<const CTile>(vCells));
	for(std::size_t Index = 0; Index < vCells.size(); ++Index)
		EXPECT_EQ(Plane[Index], CTileValues(vCells[Index]));
	const auto Original = Plane;
	ASSERT_TRUE(Plane.Resize(34, 33));
	ASSERT_TRUE(Plane.Resize(101, 65));
	for(int Y = 0; Y < Plane.Height(); ++Y)
		for(int X = 0; X < Plane.Width(); ++X)
			EXPECT_EQ(Plane[Y * Plane.Width() + X], X < 34 && Y < 33 ? Original[Y * 67 + X] : CTileValues{});
}

TEST(EditorDocumentExport, MassiveSparseFingerprintHashesOnlyChangedTreePaths)
{
	for(const int Edge : {256, 4096, 32768})
	{
		auto Document = MakeCompleteDocument();
		UpdateDocumentLayer(Document, 6, [&](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles = CEditorTilePlane<CTileValues>(Edge, Edge); });
		CEditorDocumentFingerprint Cache;
		const auto InitialKey = Cache.Key(Document);
		EXPECT_LT(Cache.Statistics().m_TileLeafHashes, 20u);
		EXPECT_EQ(Cache.Key(Document), InitialKey);
		EXPECT_EQ(Cache.Statistics().m_TileLeafHashes, 0u);
		EXPECT_EQ(Cache.Statistics().m_TileBranchHashes, 0u);
		for(int Revision = 1; Revision <= 50; ++Revision)
		{
			UpdateDocumentLayer(Document, 6, [&](auto &Layer) {
				std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Set(static_cast<std::size_t>(Edge) * Edge - 1, CTile{static_cast<unsigned char>(Revision), 0, 0, 0});
			});
			const auto Key = Cache.Key(Document);
			EXPECT_EQ(Cache.Statistics().m_TileLeafHashes, 1u);
			EXPECT_LE(Cache.Statistics().m_TileBranchHashes, 5u);
			EXPECT_NE(Key, InitialKey);
			EXPECT_EQ(Key, EditorDocumentPersistedKey(Document));
		}
		UpdateDocumentLayer(Document, 6, [&](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Set(static_cast<std::size_t>(Edge) * Edge - 1, {}); });
		EXPECT_EQ(Cache.Key(Document), InitialKey);
	}
}

TEST(EditorDocumentExport, ObjectFingerprintChecksGenerationOfMutableStorage)
{
	auto Document = MakeCompleteDocument();
	CEditorDocumentFingerprint Cache;
	const auto Before = Cache.Key(Document);
	// No external revision pins the layer; a weak owner alone would miss this write.
	Document.m_Groups.Update([](auto &vGroups) {
		vGroups[0].Update([](auto &Group) {
			Group.m_Layers.Update([](auto &vLayers) {
				for(auto &Layer : vLayers)
					if(std::holds_alternative<CLayerQuadsValues>(Layer.Read().m_Data))
						Layer.Update([](auto &Value) { std::get<CLayerQuadsValues>(Value.m_Data).m_vQuads[0].m_aPoints[0].x += 7; });
			});
		});
	});
	EXPECT_NE(Cache.Key(Document), Before);
	EXPECT_EQ(Cache.Key(Document), EditorDocumentPersistedKey(Document));
}

TEST(EditorHistoryStorage, EmptyRawRowsPreserveConversionAndIgnoreNonsemanticBytes)
{
	CEditorTilePlane<CSpeedupTileValues> Speedup(65, 33);
	std::vector<CSpeedupTile> vSpeedup(Speedup.Size());
	Speedup.Assign(std::span<const CSpeedupTile>(vSpeedup));
	editor_history::CStorageUsage Empty;
	Speedup.Account(Empty);
	EXPECT_EQ(Empty.Bytes(), 0u);
	for(auto &Cell : vSpeedup)
		Cell.m_MustBe0 = 255;
	Speedup.Assign(std::span<const CSpeedupTile>(vSpeedup));
	Speedup.Account(Empty);
	EXPECT_EQ(Empty.Bytes(), 0u);
	vSpeedup.back() = {7, 19, TILE_SPEED_BOOST, 255, -135};
	Speedup.Assign(std::span<const CSpeedupTile>(vSpeedup));
	for(std::size_t Index = 0; Index < Speedup.Size(); ++Index)
		EXPECT_EQ(Speedup[Index], CSpeedupTileValues(vSpeedup[Index]));
	CEditorTilePlane<CTileValues> Tiles(65, 33);
	std::vector<CTile> vTiles(Tiles.Size(), CTile{0, TILEFLAG_OPAQUE, 255, 255});
	Tiles.Assign(std::span<const CTile>(vTiles));
	Tiles.Account(Empty);
	EXPECT_EQ(Empty.Bytes(), 0u);

	struct CInput
	{
	};
	struct CValue
	{
		int m_Value = 0;
		CValue() = default;
		CValue(CInput) :
			m_Value(1) {}
		bool operator==(const CValue &) const = default;
	};
	CEditorTilePlane<CValue> NonzeroDefault(1, 1);
	const CInput Input{};
	NonzeroDefault.Assign(std::span(&Input, 1));
	EXPECT_EQ(NonzeroDefault[0].m_Value, 1);
}

TEST(EditorHistoryStorage, SparseDerivedPlanesVisitStoredChunksAndNormalizeEdges)
{
	const auto Convert = [](const CSwitchTileValues &Cell) {
		return Cell.m_Type == TILE_SWITCHOPEN ? CTileValues(CTile{Cell.m_Type, Cell.m_Flags, 0, 0}) : CTileValues{};
	};
	for(const int Edge : {67, 32768})
	{
		CEditorTilePlane<CSwitchTileValues> Source(Edge, Edge);
		Source.Set(0, CSwitchTile{7, TILE_SWITCHOPEN, 255, 3});
		Source.Set(Source.Size() - 1, CSwitchTile{8, TILE_SWITCHOPEN, TILEFLAG_ROTATE, 5});
		CEditorTilePlane<CTileValues> Derived(Edge, Edge);
		Derived.Set(Edge + 1, CTile{TILE_SOLID, 0, 0, 0});
		std::size_t Visits = 0;
		Derived.AssignSparse(Source, [&](const auto &Cell) { ++Visits; return Convert(Cell); });
		EXPECT_LE(Visits, 2u * 32u * 32u);
		EXPECT_EQ(Derived[0], CTileValues(CTile{TILE_SWITCHOPEN, TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE, 0, 0}));
		EXPECT_EQ(Derived[Edge + 1], CTileValues{});
		EXPECT_EQ(Derived[Source.Size() - 1], CTileValues(CTile{TILE_SWITCHOPEN, TILEFLAG_ROTATE, 0, 0}));
		editor_history::CStorageUsage Storage;
		Derived.Account(Storage);
		EXPECT_LT(Storage.Bytes(), 16384u);
	}
	CEditorTilePlane<CSwitchTileValues> Source(67, 35);
	std::vector<CSwitchTile> vInput(Source.Size());
	for(std::size_t Index = 0; Index < vInput.size(); ++Index)
		vInput[Index] = {static_cast<unsigned char>(Index), static_cast<unsigned char>(Index % 3 == 0 ? TILE_SWITCHOPEN : 255), static_cast<unsigned char>(Index), 17};
	Source.Assign(std::span<const CSwitchTile>(vInput));
	CEditorTilePlane<CTileValues> Derived(67, 35);
	Derived.AssignSparse(Source, Convert);
	for(std::size_t Index = 0; Index < vInput.size(); ++Index)
		EXPECT_EQ(Derived[Index], Index % 3 == 0 ? CTileValues(CTile{TILE_SWITCHOPEN, vInput[Index].m_Flags, 0, 0}) : CTileValues{});
	ASSERT_TRUE(Derived.Resize(69, 37));
	EXPECT_EQ(Derived[35 * 69 + 66], CTileValues{});
	EXPECT_EQ(Derived[34 * 69 + 67], CTileValues{});
}

TEST(EditorDocumentExport, PlatformDigestMatchesPortableSha256AcrossBlocksAndRepeatedKeys)
{
	auto Document = MakeCompleteDocument();
	CEditorDocumentFingerprint Platform;
	CEditorDocumentFingerprint Portable(CEditorDocumentFingerprint::EHashBackend::PORTABLE);
	for(const std::size_t Size : {0u, 1u, 55u, 64u, 65u, 4096u, 1024u * 1024u})
	{
		std::vector<std::uint8_t> vBytes(Size);
		for(std::size_t Index = 0; Index < Size; ++Index)
			vBytes[Index] = static_cast<std::uint8_t>(Index * 37 + 19);
		Document.m_Sounds.Update([&](auto &vSounds) { vSounds[0].m_Content.emplace(vBytes); });
		UpdateDocumentLayer(Document, 6, [&](auto &Layer) {
			auto &Tiles = std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles;
			Tiles.Resize(65, 33);
			Tiles.Set(32 * 65 + 64, CTile{static_cast<unsigned char>(Size % 251 + 1), TILEFLAG_ROTATE, 0, 0});
		});
		EXPECT_EQ(Platform.Key(Document), Portable.Key(Document));
		EXPECT_EQ(Platform.Key(Document), Portable.Key(Document));
		Portable.Clear();
		EXPECT_EQ(Platform.Key(Document), Portable.Key(Document));
	}
}

TEST(EditorDocumentExport, UnusedOpacityCannotAffectPartialChunkOrTreePadding)
{
	auto Document = MakeCompleteDocument();
	Document.m_Images.Update([](auto &vImages) {
		vImages[0].m_External = 1;
		vImages[0].m_Width = vImages[0].m_Height = 16;
		vImages[0].m_Content.emplace(std::vector<std::uint8_t>(16 * 16 * 4, 255));
	});
	UpdateDocumentLayer(Document, 6, [](auto &Layer) {
		auto &Tiles = std::get<CTileLayerDocumentValues>(Layer.m_Data);
		Tiles.m_Color.a = 255;
		Tiles.m_Tiles.Resize(33, 17);
		const std::vector<CTile> vTiles(Tiles.m_Tiles.Size(), CTile{17, 0, 0, 0});
		Tiles.m_Tiles.Assign(std::span<const CTile>(vTiles));
	});
	CEditorDocumentFingerprint Cache;
	const auto InitialKey = Cache.Key(Document);
	Document.m_Images.Update([](auto &vImages) {
		std::vector<std::uint8_t> vPixels(16 * 16 * 4, 255);
		vPixels[3] = 0; // Only tile zero, which appears solely in padding.
		vImages[0].m_Content.emplace(vPixels);
	});
	EXPECT_EQ(Cache.Key(Document), InitialKey);
	EXPECT_EQ(EditorDocumentPersistedKey(Document), InitialKey);
	UpdateDocumentLayer(Document, 6, [](auto &Layer) { std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Set(0, {}); });
	const auto TransparentZero = Cache.Key(Document);
	Document.m_Images.Update([](auto &vImages) { vImages[0].m_Content.emplace(std::vector<std::uint8_t>(16 * 16 * 4, 255)); });
	EXPECT_NE(Cache.Key(Document), TransparentZero);
}

TEST(EditorDocumentExport, SparseNativePlanesPreserveEveryCellAndRejectOversizedBlocks)
{
	class CNativeOutput : public IEditorDocumentOutput
	{
	public:
		std::vector<unsigned char> m_vBytes;
		int AddItem(int, int, std::size_t, const void *) override { return 0; }
		int AddData(std::size_t Size, const void *pData) override
		{
			if(Size == 0 || pData == nullptr)
				return -1;
			const auto *pBytes = static_cast<const unsigned char *>(pData);
			m_vBytes.assign(pBytes, pBytes + Size);
			return 0;
		}
		int AddDataSwapped(std::size_t Size, const void *pData) override { return AddData(Size, pData); }
	} Output;
	const auto Check = [&]<typename TValue, typename TNative>(TValue Value, TNative) {
		CEditorTilePlane<TValue> Plane(65, 33);
		Plane.Set(0, Value);
		Plane.Set(32 * 65 + 64, Value);
		std::array<unsigned char, 256> aFlags{};
		aFlags[0] = TILEFLAG_OPAQUE;
		aFlags[1] = TILEFLAG_OPAQUE;
		ASSERT_EQ(Output.AddTilePlane(&Plane, aFlags), 0);
		ASSERT_EQ(Output.m_vBytes.size(), Plane.Size() * sizeof(TNative));
		for(std::size_t Index = 0; Index < Plane.Size(); ++Index)
		{
			auto Expected = static_cast<TNative>(Plane[Index]);
			if constexpr(std::is_same_v<TValue, CTileValues>)
				Expected.m_Flags |= aFlags[Expected.m_Index];
			EXPECT_EQ(memcmp(&Expected, Output.m_vBytes.data() + Index * sizeof(TNative), sizeof(TNative)), 0) << Index;
		}
	};
	Check(CTileValues(CTile{1, TILEFLAG_ROTATE, 0, 0}), CTile{});
	Check(CTeleTileValues(CTeleTile{3, TILE_TELEIN}), CTeleTile{});
	Check(CSpeedupTileValues(CSpeedupTile{12, 34, TILE_SPEED_BOOST, 0, 267}), CSpeedupTile{});
	Check(CSwitchTileValues(CSwitchTile{8, TILE_SWITCHOPEN, TILEFLAG_ROTATE, 15}), CSwitchTile{});
	Check(CTuneTileValues(CTuneTile{10, TILE_TUNE}), CTuneTile{});
	ASSERT_EQ(Output.AddEmptyTiles(65 * 33), 0);
	EXPECT_EQ(Output.m_vBytes, std::vector<unsigned char>(65 * 33 * sizeof(CTile)));
	const CEditorTilePlane<CTileValues> Huge(100000, 100000);
	EXPECT_EQ(Output.AddTilePlane(&Huge), -1);
	EXPECT_EQ(Output.AddEmptyTiles(static_cast<std::size_t>(std::numeric_limits<int>::max())), -1);
}

TEST(EditorDocumentExport, CacheObservationPinsImmutableSourcesAndMatchesFreshUnion)
{
	auto Document = MakeCompleteDocument();
	Document.m_vSettings.reserve(Document.m_vSettings.size() + 100);
	CEditorDocumentFingerprint Cache;
	EXPECT_FALSE(Cache.Key(Document).empty());
	editor_history::CStorageUsage Fresh;
	Document.Account(Fresh);
	Cache.Account(Fresh);
	editor_history::CStorageObservation Observation;
	Observation.Observe(Document);
	Cache.Observe(Observation);
	std::size_t ObservedBytes = 0;
	std::promise<void> Mutated;
	auto Ready = Mutated.get_future();
	std::thread Worker([&] {
		Ready.wait();
		editor_history::CStorageUsage Usage;
		Observation.Account(Usage);
		ObservedBytes = Usage.Bytes();
	});
	UpdateDocumentLayer(Document, 6, [](auto &Layer) {
		std::get<CTileLayerDocumentValues>(Layer.m_Data).m_Tiles.Set(0, CTileValues(CTile{TILE_DEATH, 0, 0, 0}));
	});
	EXPECT_FALSE(Cache.Key(Document).empty());
	Document.m_vSettings.reserve(Document.m_vSettings.capacity() + 10000);
	Cache.Clear();
	editor_history::CStorageUsage Changed;
	Document.Account(Changed);
	EXPECT_GT(Changed.Bytes(), Fresh.Bytes());
	Document = {}; // the observation now outlives both source graphs
	Mutated.set_value();
	Worker.join();
	EXPECT_EQ(ObservedBytes, Fresh.Bytes());
}
