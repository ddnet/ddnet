#include <gtest/gtest.h>

#include <base/system.h>

#if defined(CONF_FAMILY_UNIX)
TEST(Unix, Create)
{
	UNIXSOCKET Socket = net_unix_create_unnamed();
	ASSERT_GE(Socket, 0);
	net_unix_close(Socket);
}
#endif
