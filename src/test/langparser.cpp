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

TEST(Lang, Bye)
{
	CLangParser Parser;

	// proper greetings
	EXPECT_TRUE(Parser.IsBye("bye"));
	EXPECT_TRUE(Parser.IsBye("bye!"));
	EXPECT_TRUE(Parser.IsBye("ChillerDragon: bye"));

	// possible false positives
	EXPECT_FALSE(Parser.IsBye("bye: hello!"));
}

TEST(Lang, Insult)
{
	CLangParser Parser;

	// proper greetings
	EXPECT_TRUE(Parser.IsInsult("fuck your relatives"));
	EXPECT_TRUE(Parser.IsInsult("DELETE THE GAME"));

	// possible false positives
	EXPECT_FALSE(Parser.IsInsult("fuck yeah! that was awesome"));
	EXPECT_FALSE(Parser.IsInsult("dogshit i died"));
}

TEST(Lang, Why)
{
	CLangParser Parser;

	// proper greetings
	// EXPECT_TRUE(Parser.IsQuestionWhy("wai u do dis"));
	EXPECT_TRUE(Parser.IsQuestionWhy("why did you!"));
	EXPECT_TRUE(Parser.IsQuestionWhy("warum ist das passiert?!"));

	// possible false positives
	// EXPECT_FALSE(Parser.IsQuestionWhy("did he tell you why?"));
	EXPECT_FALSE(Parser.IsQuestionWhy("when did what happen?"));
}
