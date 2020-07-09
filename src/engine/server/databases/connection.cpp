#include "connection.h"

#include <engine/shared/protocol.h>

void IDbConnection::FormatCreateRace(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
			"CREATE TABLE IF NOT EXISTS %s_race ("
				"Map VARCHAR(128) BINARY NOT NULL, "
				"Name VARCHAR(%d) BINARY NOT NULL, "
				"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"Time FLOAT DEFAULT 0, "
				"Server CHAR(4), "
				"cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, "
				"cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, "
				"cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, "
				"cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, "
				"cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, "
				"cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, "
				"cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, "
				"cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, "
				"cp25 FLOAT DEFAULT 0, "
				"GameID VARCHAR(64), "
				"DDNet7 BOOL DEFAULT FALSE, "
				"KEY (Map, Name)"
			") CHARACTER SET utf8mb4;",
			GetPrefix(), MAX_NAME_LENGTH);
}

void IDbConnection::FormatCreateTeamrace(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
			"CREATE TABLE IF NOT EXISTS %s_teamrace ("
				"Map VARCHAR(128) BINARY NOT NULL, "
				"Name VARCHAR(%d) BINARY NOT NULL, "
				"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"Time FLOAT DEFAULT 0, "
				"ID VARBINARY(16) NOT NULL, "
				"GameID VARCHAR(64), "
				"DDNet7 BOOL DEFAULT FALSE, "
				"KEY Map (Map)"
			") CHARACTER SET utf8mb4;",
			GetPrefix(), MAX_NAME_LENGTH);
}

void IDbConnection::FormatCreateMaps(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
			"CREATE TABLE IF NOT EXISTS %s_maps ("
				"Map VARCHAR(128) BINARY NOT NULL, "
				"Server VARCHAR(32) BINARY NOT NULL, "
				"Mapper VARCHAR(128) BINARY NOT NULL, "
				"Points INT DEFAULT 0, "
				"Stars INT DEFAULT 0, "
				"Timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
				"UNIQUE KEY Map (Map)"
			") CHARACTER SET utf8mb4;",
			GetPrefix());
}

void IDbConnection::FormatCreateSaves(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
			"CREATE TABLE IF NOT EXISTS %s_saves ("
				"Savegame TEXT CHARACTER SET utf8mb4 BINARY NOT NULL, "
				"Map VARCHAR(128) BINARY NOT NULL, "
				"Code VARCHAR(128) BINARY NOT NULL, "
				"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"Server CHAR(4), "
				"DDNet7 BOOL DEFAULT FALSE, "
				"SaveID VARCHAR(36) DEFAULT NULL, "
				"UNIQUE KEY (Map, Code)"
			") CHARACTER SET utf8mb4;",
			GetPrefix());
}

void IDbConnection::FormatCreatePoints(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
			"CREATE TABLE IF NOT EXISTS %s_points ("
				"Name VARCHAR(%d) BINARY NOT NULL, "
				"Points INT DEFAULT 0, "
				"UNIQUE KEY Name (Name)"
			") CHARACTER SET utf8mb4;",
			GetPrefix(), MAX_NAME_LENGTH);
}
