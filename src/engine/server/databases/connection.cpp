#include "connection.h"

#include <base/str.h>

IDbConnection::IDbConnection(const char *pPrefix, int SchemaVersion) :
	m_SchemaVersion(SchemaVersion)
{
	str_copy(m_aPrefix, pPrefix);
}

bool IDbConnection::AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize)
{
	if(SchemaVersion() < 2)
	{
		return AddPointsV1(pPlayer, Points, pError, ErrorSize);
	}
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_player(name) VALUES (%s)%s",
		InsertIgnore(), GetPrefix(), Placeholder(1).c_str(), InsertIgnoreEnd());
	if(!PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	BindString(1, pPlayer);
	int NumUpdated;
	if(!ExecuteUpdate(&NumUpdated, pError, ErrorSize))
	{
		return false;
	}
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_player_points(player_id, points) "
		"SELECT player_id, 0 FROM %s_player WHERE name = %s%s",
		InsertIgnore(), GetPrefix(), GetPrefix(), Placeholder(1).c_str(), InsertIgnoreEnd());
	if(!PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	BindString(1, pPlayer);
	if(!ExecuteUpdate(&NumUpdated, pError, ErrorSize))
	{
		return false;
	}
	str_format(aBuf, sizeof(aBuf),
		"UPDATE %s_player_points SET points = points + %s "
		"WHERE player_id = (SELECT player_id FROM %s_player WHERE name = %s)",
		GetPrefix(), Placeholder(1).c_str(), GetPrefix(), Placeholder(2).c_str());
	if(!PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	BindInt(1, Points);
	BindString(2, pPlayer);
	return ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

std::optional<float> IDbConnection::GetOptionalFloat(int Col)
{
	if(IsNull(Col))
		return std::nullopt;
	return GetInt(Col);
}

std::optional<int> IDbConnection::GetOptionalInt(int Col)
{
	if(IsNull(Col))
		return std::nullopt;
	return GetInt(Col);
}

std::optional<int64_t> IDbConnection::GetOptionalInt64(int Col)
{
	if(IsNull(Col))
		return std::nullopt;
	return GetInt64(Col);
}

void IDbConnection::FormatCreateRace(char *aBuf, unsigned int BufferSize, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_race%s ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time FLOAT DEFAULT 0, "
		"  Server VARCHAR(4), "
		"  cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, "
		"  cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, "
		"  cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, "
		"  cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, "
		"  cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, "
		"  cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, "
		"  cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, "
		"  cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, "
		"  cp25 FLOAT DEFAULT 0, "
		"  GameId VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (Map, Name, Time, Timestamp, Server)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), MAX_NAME_LENGTH_SQL, BinaryCollate());
}

void IDbConnection::FormatCreateTeamrace(char *aBuf, unsigned int BufferSize, const char *pIdType, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_teamrace%s ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time FLOAT DEFAULT 0, "
		"  ID %s NOT NULL, " // VARBINARY(16) for MySQL and BLOB for SQLite
		"  GameId VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (Id, Name)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), MAX_NAME_LENGTH_SQL, BinaryCollate(), pIdType);
}

void IDbConnection::FormatCreateMaps(char *aBuf, unsigned int BufferSize) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_maps ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Server VARCHAR(32) COLLATE %s NOT NULL, "
		"  Mapper VARCHAR(128) COLLATE %s NOT NULL, "
		"  Points INT DEFAULT 0, "
		"  Stars INT DEFAULT 0, "
		"  Timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
		"  PRIMARY KEY (Map)"
		")",
		GetPrefix(), BinaryCollate(), BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreateSaves(char *aBuf, unsigned int BufferSize, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_saves%s ("
		"  Savegame TEXT COLLATE %s NOT NULL, "
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Code VARCHAR(128) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Server VARCHAR(4), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  SaveId VARCHAR(36) DEFAULT NULL, "
		"  PRIMARY KEY (Map, Code)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreatePoints(char *aBuf, unsigned int BufferSize) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_points ("
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Points INT DEFAULT 0, "
		"  PRIMARY KEY (Name)"
		")",
		GetPrefix(), MAX_NAME_LENGTH_SQL, BinaryCollate());
}

void IDbConnection::FormatCreateV2Map(char *aBuf, unsigned int BufferSize, const char *pIdAutoIncrement) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_map ("
		"  map_id %s, "
		"  name VARCHAR(128) COLLATE %s NOT NULL UNIQUE, "
		"  category VARCHAR(32) COLLATE %s NOT NULL DEFAULT '', "
		"  mapper VARCHAR(128) COLLATE %s NOT NULL DEFAULT '', "
		"  points INT NOT NULL DEFAULT 0, "
		"  stars INT NOT NULL DEFAULT 0, "
		"  released TIMESTAMP NULL DEFAULT NULL"
		")",
		GetPrefix(), pIdAutoIncrement, BinaryCollate(), BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreateV2Player(char *aBuf, unsigned int BufferSize, const char *pIdAutoIncrement) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_player ("
		"  player_id %s, "
		"  name VARCHAR(%d) COLLATE %s NOT NULL UNIQUE, "
		"  first_finished TIMESTAMP NULL DEFAULT NULL"
		")",
		GetPrefix(), pIdAutoIncrement, MAX_NAME_LENGTH_SQL, BinaryCollate());
}

void IDbConnection::FormatCreateV2PlayerPoints(char *aBuf, unsigned int BufferSize) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_player_points ("
		"  player_id INT NOT NULL, "
		"  points INT NOT NULL DEFAULT 0, "
		"  PRIMARY KEY (player_id)"
		")",
		GetPrefix());
}

void IDbConnection::FormatCreateV2Finish(char *aBuf, unsigned int BufferSize, const char *pBlobType, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_finish%s ("
		"  map_id INT NOT NULL, "
		"  player_id INT NOT NULL, "
		"  time_cs INT NOT NULL, "
		"  finished_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  server VARCHAR(4) COLLATE %s NOT NULL DEFAULT '', "
		"  game_uuid VARCHAR(36) COLLATE %s NULL DEFAULT NULL, "
		"  cp_times %s NULL DEFAULT NULL, "
		"  ddnet7 BOOL NOT NULL DEFAULT FALSE, "
		"  PRIMARY KEY (map_id, player_id, time_cs, finished_at, server)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), BinaryCollate(), pBlobType);
}

void IDbConnection::FormatCreateV2Best(char *aBuf, unsigned int BufferSize, const char *pBlobType) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_best ("
		"  map_id INT NOT NULL, "
		"  player_id INT NOT NULL, "
		"  server VARCHAR(4) COLLATE %s NOT NULL DEFAULT '', "
		"  time_cs INT NOT NULL, "
		"  cp_time_cs INT NULL DEFAULT NULL, "
		"  cp_times %s NULL DEFAULT NULL, "
		"  finish_count INT NOT NULL DEFAULT 0, "
		"  first_finished TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  last_finished TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  PRIMARY KEY (map_id, player_id, server)"
		")",
		GetPrefix(), BinaryCollate(), pBlobType);
}

void IDbConnection::FormatCreateV2Team(char *aBuf, unsigned int BufferSize, const char *pUuidType, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_team%s ("
		"  team_id %s NOT NULL, "
		"  map_id INT NOT NULL, "
		"  roster_hash %s NOT NULL, "
		"  time_cs INT NOT NULL, "
		"  finished_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  server VARCHAR(4) COLLATE %s NOT NULL DEFAULT '', "
		"  game_uuid VARCHAR(36) COLLATE %s NULL DEFAULT NULL, "
		"  member_count INT NOT NULL DEFAULT 0, "
		"  ddnet7 BOOL NOT NULL DEFAULT FALSE, "
		"  PRIMARY KEY (team_id), "
		"  UNIQUE (map_id, roster_hash)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		pUuidType, pUuidType, BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreateV2TeamPlayer(char *aBuf, unsigned int BufferSize, const char *pUuidType) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_team_player ("
		"  team_id %s NOT NULL, "
		"  player_id INT NOT NULL, "
		"  PRIMARY KEY (team_id, player_id)"
		")",
		GetPrefix(), pUuidType);
}

void IDbConnection::FormatCreateV2Save(char *aBuf, unsigned int BufferSize, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_save%s ("
		"  map_id INT NOT NULL, "
		"  code VARCHAR(128) COLLATE %s NOT NULL, "
		"  savegame TEXT COLLATE %s NOT NULL, "
		"  saved_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  server VARCHAR(4) COLLATE %s NULL DEFAULT NULL, "
		"  save_uuid VARCHAR(36) NULL DEFAULT NULL, "
		"  ddnet7 BOOL NOT NULL DEFAULT FALSE, "
		"  PRIMARY KEY (map_id, code)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), BinaryCollate(), BinaryCollate());
}
