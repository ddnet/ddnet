#include "test.h"

#include <base/hash.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/editor/history/tile_plane.h>
#include <game/editor/history/tile_projection.h>
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
