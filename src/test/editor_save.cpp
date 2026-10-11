#include "test.h"

#include <base/str.h>

#include <engine/storage.h>

#include <game/editor/history/save_job.h>
#include <game/editor/mapitems/document_export.h>

#include <gtest/gtest.h>

namespace
{
	void CheckPreparedSave(bool BlockDestination)
	{
		CTestInfo Info;
		Info.m_DeleteTestStorageFilesOnSuccess = true;
		auto pStorage = Info.CreateTestStorage();
		ASSERT_NE(pStorage, nullptr);
		if(BlockDestination)
		{
			ASSERT_TRUE(pStorage->CreateFolder("saved.map", IStorage::TYPE_SAVE));
			IOHANDLE File = pStorage->OpenFile("saved.map/keep", IOFLAG_WRITE, IStorage::TYPE_SAVE);
			ASSERT_NE(File, nullptr);
			ASSERT_EQ(io_close(File), 0);
		}

		CEditorDocumentValues Document;
		CGroupDocumentValues Group;
		Group.m_Id = 1;
		Group.m_GameGroup = true;
		CLayerDocumentValues Layer;
		Layer.m_Id = 2;
		Layer.m_Type = LAYERTYPE_TILES;
		auto &Tiles = std::get<CTileLayerDocumentValues>(Layer.m_Data);
		Tiles.m_HasGame = true;
		ASSERT_TRUE(Tiles.m_Tiles.Resize(1, 1));
		Group.m_Layers.Update([&](auto &vLayers) { vLayers.emplace_back(Layer); });
		Document.m_Groups.Update([&](auto &vGroups) { vGroups.emplace_back(Group); });
		CEditorSaveState Saves;
		const auto LoadedKey = EditorDocumentPersistedKey(Document);
		ASSERT_TRUE(Saves.InitializeLoaded(LoadedKey));
		str_copy(Document.m_MapInfo.m_Author.Buffer(), "captured author");
		const auto CapturedKey = EditorDocumentPersistedKey(Document);
		auto Ticket = Saves.Capture(std::make_shared<const CEditorDocumentValues>(Document), CapturedKey, "saved.map", editor_history::ESaveKind::MANUAL);
		ASSERT_TRUE(Ticket);
		CDataFileWriter Writer;
		ASSERT_TRUE(Writer.Open(pStorage.get(), "queued.tmp"));
		std::string Error;
		ASSERT_TRUE(WriteEditorDocument(*Ticket->m_pRoot, Writer, Error)) << Error;
		auto pJob = std::make_shared<CDataFileWriterFinishJob>(pStorage.get(), "queued.tmp", std::move(Writer), std::move(*Ticket));
		EXPECT_EQ(pJob->Ticket().m_pRoot, nullptr);
		str_copy(Document.m_MapInfo.m_Author.Buffer(), "later author");
		EXPECT_TRUE(Saves.Dirty(CapturedKey, editor_history::ESaveKind::MANUAL));

		CJobPool Pool;
		Pool.Init(1);
		Pool.Add(pJob);
		Pool.Shutdown();
		ASSERT_EQ(pJob->State(), IJob::STATE_DONE);
		const bool Success = pJob->ErrorMessage()[0] == '\0';
		EXPECT_EQ(Success, !BlockDestination) << pJob->ErrorMessage();
		EXPECT_EQ(Saves.Complete(pJob->Ticket(), Success), !BlockDestination);
		EXPECT_EQ(Saves.Dirty(LoadedKey, editor_history::ESaveKind::MANUAL), !BlockDestination);
		EXPECT_EQ(Saves.Dirty(CapturedKey, editor_history::ESaveKind::MANUAL), BlockDestination);
		EXPECT_TRUE(Saves.Dirty(EditorDocumentPersistedKey(Document), editor_history::ESaveKind::MANUAL));
		if(BlockDestination)
			EXPECT_TRUE(pStorage->FileExists("saved.map/keep", IStorage::TYPE_SAVE));
		else
			EXPECT_FALSE(pStorage->FileExists("queued.tmp", IStorage::TYPE_SAVE));
		CDataFileReader Reader;
		ASSERT_TRUE(Reader.Open(pStorage.get(), BlockDestination ? "queued.tmp" : "saved.map", IStorage::TYPE_SAVE));
		const auto *pInfo = static_cast<const CMapItemInfo *>(Reader.FindItem(MAPITEMTYPE_INFO, 0));
		ASSERT_NE(pInfo, nullptr);
		EXPECT_STREQ(Reader.GetDataString(pInfo->m_Author), "captured author");
	}
}

TEST(EditorSaveJob, PreparedWriterSavesCapturedContentsAndCompletesMarker)
{
	CheckPreparedSave(false);
}

TEST(EditorSaveJob, FailedRenamePreservesDestinationAndSavedMarker)
{
	CheckPreparedSave(true);
}
