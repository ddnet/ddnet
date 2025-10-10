#include "connection.h"

IDbConnection::IDbConnection(const char *pPrefix)
{
	str_copy(m_aPrefix, pPrefix);
}

void IDbConnection::FormatCreateRace(char *aBuf, unsigned int BufferSize, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_race%s ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time %s DEFAULT 0, "
		"  Server CHAR(4), "
		"  cp1 %s DEFAULT 0, cp2 %s DEFAULT 0, cp3 %s DEFAULT 0, "
		"  cp4 %s DEFAULT 0, cp5 %s DEFAULT 0, cp6 %s DEFAULT 0, "
		"  cp7 %s DEFAULT 0, cp8 %s DEFAULT 0, cp9 %s DEFAULT 0, "
		"  cp10 %s DEFAULT 0, cp11 %s DEFAULT 0, cp12 %s DEFAULT 0, "
		"  cp13 %s DEFAULT 0, cp14 %s DEFAULT 0, cp15 %s DEFAULT 0, "
		"  cp16 %s DEFAULT 0, cp17 %s DEFAULT 0, cp18 %s DEFAULT 0, "
		"  cp19 %s DEFAULT 0, cp20 %s DEFAULT 0, cp21 %s DEFAULT 0, "
		"  cp22 %s DEFAULT 0, cp23 %s DEFAULT 0, cp24 %s DEFAULT 0, "
		"  cp25 %s DEFAULT 0, "
		"  GameId VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (Map, Name, Time, Timestamp, Server)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), MAX_NAME_LENGTH_SQL, BinaryCollate(), Decimal(),
		Decimal(), Decimal(), Decimal(), Decimal(), Decimal(), Decimal(),
		Decimal(), Decimal(), Decimal(), Decimal(), Decimal(), Decimal(),
		Decimal(), Decimal(), Decimal(), Decimal(), Decimal(), Decimal(),
		Decimal(), Decimal(), Decimal(), Decimal(), Decimal(), Decimal(),
		Decimal());
}

void IDbConnection::FormatCreateTeamrace(char *aBuf, unsigned int BufferSize, const char *pIdType, bool Backup) const
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_teamrace%s ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time %s DEFAULT 0, "
		"  ID %s NOT NULL, " // VARBINARY(16) for MySQL and BLOB for SQLite
		"  GameId VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (Id, Name)"
		")",
		GetPrefix(), Backup ? "_backup" : "",
		BinaryCollate(), MAX_NAME_LENGTH_SQL, BinaryCollate(), Decimal(), pIdType);
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
		"  Server CHAR(4), "
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
