#include "test.h"
#include <gtest/gtest.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems_ex.h>

TEST(Datafile, ExtendedType)
{
	IStorage *pStorage = CreateLocalStorage();
	CTestInfo Info;

	CMapItemTest Test;
	Test.m_Version = CMapItemTest::CURRENT_VERSION;
	Test.m_aFields[0] = 1234;
	Test.m_aFields[1] = 5678;
	Test.m_Field3 = 9876;
	Test.m_Field4 = 5432;

	{
		CDataFileWriter Writer;
		Writer.Open(pStorage, Info.m_aFilename);

		Writer.AddItem(MAPITEMTYPE_TEST, 0x8000, sizeof(Test), &Test);

		Writer.Finish();
	}

	{
		CDataFileReader Reader;
		Reader.Open(pStorage, Info.m_aFilename, IStorage::TYPE_ALL);

		int Start, Num;
		Reader.GetType(MAPITEMTYPE_TEST, &Start, &Num);
		EXPECT_EQ(Num, 1);

		int Index = Reader.FindItemIndex(MAPITEMTYPE_TEST, 0x8000);
		EXPECT_EQ(Start, Index);
		ASSERT_GE(Index, 0);
		ASSERT_EQ(Reader.GetItemSize(Index), (int)sizeof(Test));

		int Type, ID;
		const CMapItemTest *pTest = (const CMapItemTest *)Reader.GetItem(Index, &Type, &ID);
		EXPECT_EQ(pTest, Reader.FindItem(MAPITEMTYPE_TEST, 0x8000));
		EXPECT_EQ(Type, MAPITEMTYPE_TEST);
		EXPECT_EQ(ID, 0x8000);

		EXPECT_EQ(pTest->m_Version, Test.m_Version);
		EXPECT_EQ(pTest->m_aFields[0], Test.m_aFields[0]);
		EXPECT_EQ(pTest->m_aFields[1], Test.m_aFields[1]);
		EXPECT_EQ(pTest->m_Field3, Test.m_Field3);
		EXPECT_EQ(pTest->m_Field4, Test.m_Field4);
	}

	if(!HasFailure())
	{
		pStorage->RemoveFile(Info.m_aFilename, IStorage::TYPE_SAVE);
	}

	delete pStorage;
}
