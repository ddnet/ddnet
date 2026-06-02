#include <base/dbg.h>

#include <gtest/gtest.h>

TEST(Dbg, AssertTrue)
{
	dbg_assert(true, "Dbg.AssertTrue unintended assertion");
}

TEST(Dbg, AssertFalse)
{
	ASSERT_DEATH({ dbg_assert(false, "Dbg.AssertFalse intended assertion"); }, "");
}

TEST(Dbg, AssertFailed)
{
	ASSERT_DEATH({ dbg_assert_failed("Dbg.AssertFailed intended assertion"); }, "");
}
