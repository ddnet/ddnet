#include "test.h"

#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/storage.h>

#include <gtest/gtest.h>

#include <cstdlib>

TEST(Storage, ReplaceFile)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	const auto Write = [&](const char *pFilename, const char *pContents) {
		IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		EXPECT_EQ(io_write(File, pContents, str_length(pContents)), static_cast<unsigned>(str_length(pContents)));
		EXPECT_EQ(io_close(File), 0);
	};
	const auto ExpectContents = [&](const char *pFilename, const char *pContents) {
		char *pActual = pStorage->ReadFileStr(pFilename, IStorage::TYPE_SAVE);
		ASSERT_NE(pActual, nullptr);
		EXPECT_STREQ(pActual, pContents);
		free(pActual);
	};

	// Both paths must resolve inside the test storage, not the current directory.
	ASSERT_TRUE(pStorage->CreateFolder("nested", IStorage::TYPE_SAVE));
	Write("nested/file", "previous file");
	EXPECT_FALSE(pStorage->ReplaceFile("nested/prepared", "nested/file", IStorage::TYPE_SAVE));
	ExpectContents("nested/file", "previous file");

	// Windows allows fs_rename to replace a file with a directory. The file-only
	// replacement wrapper must reject that input and preserve the destination.
	ASSERT_TRUE(pStorage->CreateFolder("nested/prepared", IStorage::TYPE_SAVE));
	EXPECT_FALSE(pStorage->ReplaceFile("nested/prepared", "nested/file", IStorage::TYPE_SAVE));
	ExpectContents("nested/file", "previous file");
	EXPECT_TRUE(pStorage->FolderExists("nested/prepared", IStorage::TYPE_SAVE));
	ASSERT_TRUE(pStorage->RemoveFolder("nested/prepared", IStorage::TYPE_SAVE));

	Write("nested/prepared", "completed file");
	EXPECT_FALSE(pStorage->ReplaceFile("nested/prepared", "missing/file", IStorage::TYPE_SAVE));
	ExpectContents("nested/prepared", "completed file");
	ExpectContents("nested/file", "previous file");
	EXPECT_TRUE(pStorage->ReplaceFile("nested/prepared", "nested/file", IStorage::TYPE_SAVE));
	ExpectContents("nested/file", "completed file");
	EXPECT_FALSE(pStorage->FileExists("nested/prepared", IStorage::TYPE_SAVE));

	EXPECT_TRUE(pStorage->ReplaceFile("nested/file", "nested/new-file", IStorage::TYPE_SAVE));
	ExpectContents("nested/new-file", "completed file");
	EXPECT_FALSE(pStorage->FileExists("nested/file", IStorage::TYPE_SAVE));
}

TEST(Storage, ReplaceBinaryFile)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	// Test storage uses the current directory for binary paths. Keep all files
	// inside its isolated directory, including the newly created parent folder.
	char aPrepared[IO_MAX_PATH_LENGTH];
	char aDestination[IO_MAX_PATH_LENGTH];
	str_format(aPrepared, sizeof(aPrepared), "%s/prepared", Info.m_aFilename);
	str_format(aDestination, sizeof(aDestination), "%s/new/file", Info.m_aFilename);
	IOHANDLE File = io_open(aPrepared, IOFLAG_WRITE);
	ASSERT_NE(File, nullptr);
	EXPECT_EQ(io_write(File, "completed file", 14), 14U);
	EXPECT_EQ(io_close(File), 0);

	EXPECT_TRUE(pStorage->ReplaceBinaryFile(aPrepared, aDestination));
	EXPECT_FALSE(fs_is_file(aPrepared));
	char *pActual = pStorage->ReadFileStr("new/file", IStorage::TYPE_SAVE);
	ASSERT_NE(pActual, nullptr);
	EXPECT_STREQ(pActual, "completed file");
	free(pActual);

	File = io_open(aPrepared, IOFLAG_WRITE);
	ASSERT_NE(File, nullptr);
	EXPECT_EQ(io_write(File, "updated file", 12), 12U);
	EXPECT_EQ(io_close(File), 0);
	EXPECT_TRUE(pStorage->ReplaceBinaryFile(aPrepared, aDestination));
	EXPECT_FALSE(fs_is_file(aPrepared));
	pActual = pStorage->ReadFileStr("new/file", IStorage::TYPE_SAVE);
	ASSERT_NE(pActual, nullptr);
	EXPECT_STREQ(pActual, "updated file");
	free(pActual);

	// A refused promotion must leave the installed file available.
	EXPECT_FALSE(pStorage->ReplaceBinaryFile(aPrepared, aDestination));
	pActual = pStorage->ReadFileStr("new/file", IStorage::TYPE_SAVE);
	ASSERT_NE(pActual, nullptr);
	EXPECT_STREQ(pActual, "updated file");
	free(pActual);
}
