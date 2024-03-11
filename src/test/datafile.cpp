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

		int Type, Id;
		const CMapItemTest *pTest = (const CMapItemTest *)Reader.GetItem(Index, &Type, &Id);
		EXPECT_EQ(pTest, Reader.FindItem(MAPITEMTYPE_TEST, 0x8000));
		EXPECT_EQ(Type, MAPITEMTYPE_TEST);
		EXPECT_EQ(Id, 0x8000);

		EXPECT_EQ(pTest->m_Version, ItemTest.m_Version);
		EXPECT_EQ(pTest->m_aFields[0], ItemTest.m_aFields[0]);
		EXPECT_EQ(pTest->m_aFields[1], ItemTest.m_aFields[1]);
		EXPECT_EQ(pTest->m_Field3, ItemTest.m_Field3);
		EXPECT_EQ(pTest->m_Field4, ItemTest.m_Field4);

		Reader.Close();
	}

	if(!HasFailure())
	{
		pStorage->RemoveFile(Info.m_aFilename, IStorage::TYPE_SAVE);
	}
}

TEST(Datafile, StringData)
{
	auto pStorage = std::unique_ptr<IStorage>(CreateLocalStorage());
	CTestInfo Info;

	{
		CDataFileWriter Writer;
		Writer.Open(pStorage.get(), Info.m_aFilename);

		EXPECT_EQ(Writer.AddDataString(""), -1); // Empty string is not added
		EXPECT_EQ(Writer.AddDataString("Abc"), 0);
		EXPECT_EQ(Writer.AddDataString("DDNet最好了"), 1);
		EXPECT_EQ(Writer.AddDataString("aβい🐘"), 2);
		EXPECT_EQ(Writer.AddData(3, "Abc"), 3); // Not zero-terminated
		EXPECT_EQ(Writer.AddData(7, "foo\0bar"), 4); // Early zero-terminator
		EXPECT_EQ(Writer.AddData(5, "xyz\xff\0"), 5); // Truncated UTF-8
		EXPECT_EQ(Writer.AddData(4, "XYZ\xff"), 6); // Truncated UTF-8 and not zero-terminated

		Writer.Finish();
	}

	{
		CDataFileReader Reader;
		Reader.Open(pStorage.get(), Info.m_aFilename, IStorage::TYPE_ALL);

		EXPECT_EQ(Reader.GetDataString(-1000), nullptr);
		EXPECT_STREQ(Reader.GetDataString(-1), "");
		EXPECT_STREQ(Reader.GetDataString(0), "Abc");
		EXPECT_STREQ(Reader.GetDataString(1), "DDNet最好了");
		EXPECT_STREQ(Reader.GetDataString(2), "aβい🐘");
		EXPECT_EQ(Reader.GetDataString(3), nullptr);
		EXPECT_EQ(Reader.GetDataString(4), nullptr);
		EXPECT_EQ(Reader.GetDataString(5), nullptr);
		EXPECT_EQ(Reader.GetDataString(6), nullptr);
		EXPECT_EQ(Reader.GetDataString(1000), nullptr);

		Reader.Close();
	}

	if(!HasFailure())
	{
		pStorage->RemoveFile(Info.m_aFilename, IStorage::TYPE_SAVE);
	}
}
