/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
// This helps IDEs properly syntax highlight the uses of the macro below.
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc)
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc)
#endif

MACRO_CONFIG_INT(FoxExampleInt, fox_example_int, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_SAVE, "Example integer config variable")
MACRO_CONFIG_STR(FoxExampleStr, fox_example_str, 100, "FoxNet", CFGFLAG_SERVER | CFGFLAG_SAVE, "Example string config variable")

MACRO_CONFIG_INT(SvVoteMenuFlags, sv_vote_menu_flags, 0, 0, 32768, CFGFLAG_SERVER | CFGFLAG_SAVE, "Flags for what Pages to show in the vote menu")

MACRO_CONFIG_INT(SvAccounts, sv_accounts, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_SAVE, "Enable optional player accounts")

MACRO_CONFIG_STR(SvCurrencyName, sv_currency_name, 16, "FoxCoins", CFGFLAG_SERVER, "Whatever you want your currency name to be")
MACRO_CONFIG_INT(SvLevelUpMoney, fs_levelup_money, 500, 0, 5000, CFGFLAG_SERVER | CFGFLAG_GAME, "How much money a player should get if they level up")
MACRO_CONFIG_INT(SvPlaytimeMoney, fs_playtime_money, 250, 0, 5000, CFGFLAG_SERVER | CFGFLAG_GAME, "How much money a player should everytime their playtime increased by 100 (divisble by 100: 100, 200..)")
