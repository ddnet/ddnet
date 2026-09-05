#include <engine/server/authmanager.h>

#include <gtest/gtest.h>

TEST(RconRole, DefaultKickHierarchy)
{
	CAuthManager Manager;
	CRconRole *pAdmin = Manager.FindRole(RoleName::ADMIN);
	CRconRole *pMod = Manager.FindRole(RoleName::MODERATOR);
	CRconRole *pHelper = Manager.FindRole(RoleName::HELPER);
	ASSERT_NE(pAdmin, nullptr);
	ASSERT_NE(pMod, nullptr);
	ASSERT_NE(pHelper, nullptr);

	// admin outranks everyone
	EXPECT_TRUE(pAdmin->CanKick(pMod));
	EXPECT_TRUE(pAdmin->CanKick(pHelper));
	EXPECT_FALSE(pHelper->CanKick(pAdmin));
	EXPECT_FALSE(pMod->CanKick(pAdmin));

	// a moderator must be able to kick a helper
	EXPECT_TRUE(pMod->CanKick(pHelper));
	// a helper must NOT be able to kick a moderator
	EXPECT_FALSE(pHelper->CanKick(pMod));
}

TEST(RconRole, AdminInheritsReservedSlotsAndTeleOthers)
{
	CAuthManager Manager;
	// this is what the sv_reserved_slots_auth_level/sv_tele_others_auth_level
	// conchains do for the documented default value "helper"
	Manager.FindRole(RoleName::HELPER)->SetReservedSlots(true);
	Manager.FindRole(RoleName::HELPER)->SetTeleOthers(true);

	EXPECT_TRUE(Manager.FindRole(RoleName::HELPER)->HasReservedSlots());
	EXPECT_TRUE(Manager.FindRole(RoleName::MODERATOR)->HasReservedSlots());
	EXPECT_TRUE(Manager.FindRole(RoleName::ADMIN)->HasReservedSlots());
	EXPECT_TRUE(Manager.FindRole(RoleName::ADMIN)->CanTeleOthers());
}

TEST(RconRole, DisallowCommandIsCaseInsensitive)
{
	CAuthManager Manager;
	CRconRole *pMod = Manager.FindRole(RoleName::MODERATOR);
	// console command lookup is case insensitive, so this is how a command can end up stored upper case
	EXPECT_TRUE(pMod->AllowCommand("KICK"));
	EXPECT_TRUE(pMod->CanUseRconCommand("kick"));
	EXPECT_TRUE(pMod->DisallowCommand("kick"));
	EXPECT_FALSE(pMod->CanUseRconCommand("kick"));
}

TEST(RconRole, SelfInheritIsRejected)
{
	CAuthManager Manager;
	EXPECT_FALSE(Manager.RoleInherit(RoleName::HELPER, RoleName::HELPER));
	// this recurses forever if the self loop above was accepted
	EXPECT_FALSE(Manager.FindRole(RoleName::HELPER)->CanUseRconCommand("kick"));
}
