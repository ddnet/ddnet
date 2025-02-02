#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <game/server/gameworld.h>

bool IsInterrupted()
{
	return false;
}

std::vector<std::string> FakeQueue;
std::vector<std::string> FetchAndroidServerCommandQueue()
{
	return FakeQueue;
}

TEST(GameWorld, Basic)
{
	CGameWorld *pWorld = new CGameWorld();
	delete pWorld;
}
