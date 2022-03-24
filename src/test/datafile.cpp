#include "test.h"
#include <gtest/gtest.h>
#include <memory>

#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems_ex.h>

TEST(Datafile, ExtendedType)
{
	auto pStorage = std::unique_ptr<IStorage>(CreateLocalStorage());
	CTestInfo Info;

	CMapItemTest ItemTest;
	ItemTest.m_Version = CMapItemTest::CURRENT_VERSION;
	ItemTest.m_aFields[0] = 1234;
	ItemTest.m_aFields[1] = 5678;
	ItemTest.m_Field3 = 9876;
	ItemTest.m_Field4 = 5432;

	{
		CDataFileWriter Writer;
		Writer.Open(pStorage.get(), Info.m_aFilename);

		Writer.AddItem(MAPITEMTYPE_TEST, 0x8000, sizeof(ItemTest), &ItemTest);

		Writer.Finish();
	}

	{
		CDataFileReader Reader;
		Reader.Open(pStorage.get(), Info.m_aFilename, IStorage::TYPE_ALL);

		int Start, Num;
		Reader.GetType(MAPITEMTYPE_TEST, &Start, &Num);
		EXPECT_EQ(Num, 1);

		int Index = Reader.FindItemIndex(MAPITEMTYPE_TEST, 0x8000);
		EXPECT_EQ(Start, Index);
		ASSERT_GE(Index, 0);
		ASSERT_EQ(Reader.GetItemSize(Index), (int)sizeof(ItemTest));

		int Type, ID;
		const CMapItemTest *pTest = (const CMapItemTest *)Reader.GetItem(Index, &Type, &ID);
		EXPECT_EQ(pTest, Reader.FindItem(MAPITEMTYPE_TEST, 0x8000));
		EXPECT_EQ(Type, MAPITEMTYPE_TEST);
		EXPECT_EQ(ID, 0x8000);

		EXPECT_EQ(pTest->m_Version, ItemTest.m_Version);
		EXPECT_EQ(pTest->m_aFields[0], ItemTest.m_aFields[0]);
		EXPECT_EQ(pTest->m_aFields[1], ItemTest.m_aFields[1]);
		EXPECT_EQ(pTest->m_Field3, ItemTest.m_Field3);
		EXPECT_EQ(pTest->m_Field4, ItemTest.m_Field4);
	}

	if(!HasFailure())
	{
		pStorage->RemoveFile(Info.m_aFilename, IStorage::TYPE_SAVE);
	}
}
