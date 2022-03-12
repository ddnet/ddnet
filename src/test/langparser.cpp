#include <gtest/gtest.h>

#include <engine/shared/chillerbot/langparser.h>

TEST(Lang, Greetings)
{
    CLangParser Parser;

    // proper greetings
	EXPECT_TRUE(Parser.IsGreeting("hi"));
	EXPECT_TRUE(Parser.IsGreeting("hi!"));

    // possible false positives
	EXPECT_FALSE(Parser.IsGreeting("Do you know the streamer Hallowed1986?"));
	EXPECT_FALSE(Parser.IsGreeting("Ich hoffe das wird ein guter tag"));
}
