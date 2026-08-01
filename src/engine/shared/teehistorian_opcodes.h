#ifndef ENGINE_SHARED_TEEHISTORIAN_OPCODES_H
#define ENGINE_SHARED_TEEHISTORIAN_OPCODES_H

// Top-level opcodes of the teehistorian binary format, written as `-Value` (negative, to
// distinguish them from the non-negative client ids that introduce a position delta).
// Shared between `CTeeHistorian` (src/game/server/teehistorian.cpp, the writer) and
// `CTeeHistorianReader` (src/game/server/teehistorian_reader.cpp) so the two can not drift
// apart silently.
enum
{
	TEEHISTORIAN_NONE,
	TEEHISTORIAN_FINISH,
	TEEHISTORIAN_TICK_SKIP,
	TEEHISTORIAN_PLAYER_NEW,
	TEEHISTORIAN_PLAYER_OLD,
	TEEHISTORIAN_INPUT_DIFF,
	TEEHISTORIAN_INPUT_NEW,
	TEEHISTORIAN_MESSAGE,
	TEEHISTORIAN_JOIN,
	TEEHISTORIAN_DROP,
	TEEHISTORIAN_CONSOLE_COMMAND,
	TEEHISTORIAN_EX,
};

#endif // ENGINE_SHARED_TEEHISTORIAN_OPCODES_H
