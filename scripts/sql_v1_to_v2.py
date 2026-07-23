#!/usr/bin/env python3
"""Migrate a DDNet schema v1 score database (MariaDB/MySQL or SQLite) into a
PostgreSQL schema v2 database, and replay schema v2 sqlite backup files.

The v2 schema is defined by FormatCreateV2* in
src/engine/server/databases/connection.cpp -- keep them in sync.

Typical production flow (see the migration plan):

  1. Bulk load from a snapshot, days before the freeze:
       sql_v1_to_v2.py create-schema --to "dbname=teeworlds"
       sql_v1_to_v2.py migrate --from mysql://user:pw@host:3306/teeworlds --to "dbname=teeworlds"
       sql_v1_to_v2.py finalize --to "dbname=teeworlds"
       sql_v1_to_v2.py verify --from mysql://... --to "dbname=teeworlds"
  2. Freeze MariaDB (SET GLOBAL read_only=ON), then sync the delta:
       sql_v1_to_v2.py migrate --from mysql://... --to ... --since "2026-08-01 03:00:00"
       sql_v1_to_v2.py verify --from mysql://... --to ...
  3. Flip the fleet to PostgreSQL v2 via rcon.
  4. Replay the per-host sqlite backup files buffered during the freeze:
       sql_v1_to_v2.py replay --from sqlite:///path/ddnet-server-v2.sqlite --to ...

Timestamps are copied as UTC (the MySQL session time zone is forced to
+00:00); this matches game servers running with system time UTC, which write
local timestamps into the naive v2 TIMESTAMP columns.

Requires psycopg2; pymysql or MySQLdb for MySQL sources.
"""

import argparse
import hashlib
import io
import struct
import sqlite3
import sys
import urllib.parse

import psycopg2
import psycopg2.extras

NUM_CHECKPOINTS = 25
BATCH = 50000

V2_SCHEMA = """
CREATE TABLE IF NOT EXISTS {p}_map (
  map_id SERIAL PRIMARY KEY,
  name VARCHAR(128) COLLATE "C" NOT NULL UNIQUE,
  category VARCHAR(32) COLLATE "C" NOT NULL DEFAULT '',
  mapper VARCHAR(128) COLLATE "C" NOT NULL DEFAULT '',
  points INT NOT NULL DEFAULT 0,
  stars INT NOT NULL DEFAULT 0,
  released TIMESTAMP NULL DEFAULT NULL
);
CREATE TABLE IF NOT EXISTS {p}_player (
  player_id SERIAL PRIMARY KEY,
  name VARCHAR(15) COLLATE "C" NOT NULL UNIQUE,
  first_finished TIMESTAMP NULL DEFAULT NULL
);
CREATE TABLE IF NOT EXISTS {p}_player_points (
  player_id INT NOT NULL,
  points INT NOT NULL DEFAULT 0,
  PRIMARY KEY (player_id)
);
-- The finish primary key is added by `finalize` after the bulk load.
CREATE TABLE IF NOT EXISTS {p}_finish (
  map_id INT NOT NULL,
  player_id INT NOT NULL,
  time_cs INT NOT NULL,
  finished_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  server VARCHAR(4) COLLATE "C" NOT NULL DEFAULT '',
  game_uuid VARCHAR(36) COLLATE "C" NULL DEFAULT NULL,
  cp_times BYTEA NULL DEFAULT NULL,
  ddnet7 BOOL NOT NULL DEFAULT FALSE
);
CREATE TABLE IF NOT EXISTS {p}_best (
  map_id INT NOT NULL,
  player_id INT NOT NULL,
  server VARCHAR(4) COLLATE "C" NOT NULL DEFAULT '',
  time_cs INT NOT NULL,
  cp_time_cs INT NULL DEFAULT NULL,
  cp_times BYTEA NULL DEFAULT NULL,
  finish_count INT NOT NULL DEFAULT 0,
  first_finished TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_finished TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (map_id, player_id, server)
);
CREATE TABLE IF NOT EXISTS {p}_team (
  team_id BYTEA NOT NULL,
  map_id INT NOT NULL,
  roster_hash BYTEA NOT NULL,
  time_cs INT NOT NULL,
  finished_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  server VARCHAR(4) COLLATE "C" NOT NULL DEFAULT '',
  game_uuid VARCHAR(36) COLLATE "C" NULL DEFAULT NULL,
  member_count INT NOT NULL DEFAULT 0,
  ddnet7 BOOL NOT NULL DEFAULT FALSE,
  PRIMARY KEY (team_id),
  UNIQUE (map_id, roster_hash)
);
CREATE TABLE IF NOT EXISTS {p}_team_player (
  team_id BYTEA NOT NULL,
  player_id INT NOT NULL,
  PRIMARY KEY (team_id, player_id)
);
CREATE TABLE IF NOT EXISTS {p}_save (
  map_id INT NOT NULL,
  code VARCHAR(128) COLLATE "C" NOT NULL,
  savegame TEXT COLLATE "C" NOT NULL,
  saved_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  server VARCHAR(4) COLLATE "C" NULL DEFAULT NULL,
  save_uuid VARCHAR(36) NULL DEFAULT NULL,
  ddnet7 BOOL NOT NULL DEFAULT FALSE,
  PRIMARY KEY (map_id, code)
);
"""


def log(msg):
    print(msg, file=sys.stderr, flush=True)


def connect_source(url):
    """Opens mysql://user:pass@host:port/db or sqlite:///path as a v1 source.

    Returns (kind, connection) with kind in ("mysql", "sqlite")."""
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme == "sqlite":
        conn = sqlite3.connect(f"file:{parsed.path}?mode=ro", uri=True)
        return "sqlite", conn
    if parsed.scheme == "mysql":
        try:
            import pymysql as mysql_driver
        except ImportError:
            import MySQLdb as mysql_driver
        conn = mysql_driver.connect(
            host=parsed.hostname or "localhost",
            port=parsed.port or 3306,
            user=urllib.parse.unquote(parsed.username or ""),
            password=urllib.parse.unquote(parsed.password or ""),
            database=parsed.path.lstrip("/"),
            charset="utf8mb4",
        )
        cur = conn.cursor()
        # dump in UTC and get past the max_statement_time safety net
        cur.execute("SET SESSION time_zone = '+00:00'")
        try:
            cur.execute("SET SESSION max_statement_time = 0")
        except Exception:
            pass  # MySQL spells it max_execution_time and it only limits SELECTs
        cur.close()
        return "mysql", conn
    raise SystemExit(f"unsupported source url: {url}")


def iter_rows(src, query, args=()):
    """Streams rows without loading the whole result into memory."""
    kind, conn = src
    if kind == "sqlite":
        cur = conn.cursor()
        cur.execute(query.replace("%s", "?"), args)
    else:
        # unbuffered cursor, the tables don't fit in memory
        cur = conn.cursor(conn.__class__.cursorclass.__mro__[0]) if False else conn.cursor()
        try:
            import pymysql.cursors
            cur.close()
            cur = conn.cursor(pymysql.cursors.SSCursor)
        except ImportError:
            pass
        cur.execute(query, args)
    while True:
        rows = cur.fetchmany(BATCH)
        if not rows:
            break
        yield from rows
    cur.close()


def time_to_cs(time):
    # clamp garbage values from old v1 data (FLOAT columns can hold times
    # beyond the ~248 days that fit into int32 centiseconds)
    return max(-2**31, min(2**31 - 1, round(float(time) * 100)))


def pack_cp_times(cps):
    cs = [time_to_cs(cp) for cp in cps]
    if not any(cs):
        return None
    return struct.pack("<25i", *cs)


def clean_name(name):
    """MySQL compares VARCHARs with PAD SPACE semantics even in binary
    collations, so v1 treats names differing only in trailing spaces as the
    same player (and servers have trimmed new names for years). Merge such
    legacy variants into one v2 player to keep ranks and points identical."""
    return name.rstrip(" ")


def roster_hash(names):
    h = hashlib.md5()
    for name in sorted(names):
        h.update(clean_name(name).encode() + b"\0")
    return h.digest()


def clean_ts(value, null_ok=False):
    """MySQL zero dates (0000-00-00 00:00:00) don't exist in PostgreSQL. The
    game displays finishes with a zero/epoch timestamp as "don't know when",
    so map them to the epoch (NULL for nullable columns)."""
    if value is None or str(value).startswith("0000-00-00"):
        return None if null_ok else "1970-01-01 00:00:00"
    return str(value)


def clean_uuid(value):
    if value is None:
        return None
    value = str(value)[:36]
    return value if value else None


class IdCache:
    """name -> id cache for the map/player dimension tables, creating rows on
    demand. Loaded eagerly, the dimensions fit in memory even for production
    data (a few million names). Player names are normalized via clean_name."""

    def __init__(self, pg, table, id_col):
        self.pg = pg
        self.table = table
        self.id_col = id_col
        self.normalize = clean_name if table.endswith("_player") else (lambda n: n)
        cur = pg.cursor()
        cur.execute(f"SELECT name, {id_col} FROM {table}")
        self.ids = dict(cur.fetchall())
        cur.close()

    def get(self, name, insert_cur=None):
        name = self.normalize(name)
        if name in self.ids:
            return self.ids[name]
        cur = insert_cur or self.pg.cursor()
        cur.execute(
            f"INSERT INTO {self.table}(name) VALUES (%s) ON CONFLICT DO NOTHING",
            (name,))
        cur.execute(f"SELECT {self.id_col} FROM {self.table} WHERE name = %s", (name,))
        self.ids[name] = cur.fetchone()[0]
        if insert_cur is None:
            cur.close()
        return self.ids[name]


def migrate_maps(src, pg, prefix):
    cur = pg.cursor()
    n = 0
    for row in iter_rows(src, f"SELECT Map, Server, Mapper, Points, Stars, Timestamp FROM {prefix}_maps"):
        row = row[:5] + (clean_ts(row[5], null_ok=True),)
        cur.execute(
            f"INSERT INTO {prefix}_map(name, category, mapper, points, stars, released) "
            "VALUES (%s, %s, %s, %s, %s, %s) "
            "ON CONFLICT (name) DO UPDATE SET category = EXCLUDED.category, "
            "mapper = EXCLUDED.mapper, points = EXCLUDED.points, "
            "stars = EXCLUDED.stars, released = EXCLUDED.released",
            row)
        n += 1
    pg.commit()
    log(f"maps: {n}")


def migrate_players(src, pg, prefix):
    """Creates the player dimension from every v1 table that contains names."""
    cur = pg.cursor()
    n = 0
    for query in (
            f"SELECT DISTINCT Name FROM {prefix}_race",
            f"SELECT DISTINCT Name FROM {prefix}_teamrace",
            f"SELECT DISTINCT Name FROM {prefix}_points"):
        buf = []
        for (name,) in iter_rows(src, query):
            buf.append((clean_name(name),))
            if len(buf) >= BATCH:
                psycopg2.extras.execute_values(
                    cur, f"INSERT INTO {prefix}_player(name) VALUES %s ON CONFLICT DO NOTHING", buf)
                n += len(buf)
                buf = []
        if buf:
            psycopg2.extras.execute_values(
                cur, f"INSERT INTO {prefix}_player(name) VALUES %s ON CONFLICT DO NOTHING", buf)
            n += len(buf)
        pg.commit()
    log(f"players: {n} candidate names")


def finish_row_to_copy(maps, players, row):
    (map_name, name, timestamp, time, server, *cps_gameid) = row
    cps = cps_gameid[:NUM_CHECKPOINTS]
    game_uuid, ddnet7 = cps_gameid[NUM_CHECKPOINTS], cps_gameid[NUM_CHECKPOINTS + 1]
    map_id = maps.ids.get(map_name)
    player_id = players.ids.get(clean_name(name))
    cp_blob = pack_cp_times(cps)
    fields = [
        str(map_id), str(player_id), str(time_to_cs(time)), clean_ts(timestamp),
        server if server is not None else "",
        clean_uuid(game_uuid) or r"\N",
        # the backslash of the \x bytea prefix must itself be escaped in COPY text format
        r"\\x" + cp_blob.hex() if cp_blob is not None else r"\N",
        "t" if ddnet7 else "f",
    ]
    return "\t".join(fields) + "\n"


def migrate_finishes(src, pg, prefix, since):
    maps = IdCache(pg, f"{prefix}_map", "map_id")
    players = IdCache(pg, f"{prefix}_player", "player_id")
    cp_cols = ", ".join(f"cp{i + 1}" for i in range(NUM_CHECKPOINTS))
    where = "WHERE Timestamp > %s" if since else ""
    query = (f"SELECT Map, Name, Timestamp, Time, Server, {cp_cols}, GameId, DDNet7 "
             f"FROM {prefix}_race {where}")
    args = (since,) if since else ()

    cur = pg.cursor()
    n = 0
    skipped = 0
    batch = []

    def flush(batch):
        nonlocal skipped
        if not batch:
            return
        if since:
            # delta mode runs after `finalize`, the primary key exists
            psycopg2.extras.execute_values(
                cur,
                f"INSERT INTO {prefix}_finish(map_id, player_id, time_cs, finished_at, server, game_uuid, cp_times, ddnet7) "
                "VALUES %s ON CONFLICT DO NOTHING",
                batch)
        else:
            buf = io.StringIO()
            for row in batch:
                buf.write(row)
            buf.seek(0)
            cur.copy_expert(
                f"COPY {prefix}_finish(map_id, player_id, time_cs, finished_at, server, game_uuid, cp_times, ddnet7) "
                "FROM STDIN",
                buf)
        pg.commit()

    for row in iter_rows(src, query, args):
        map_name, name = row[0], clean_name(row[1])
        if map_name not in maps.ids or name not in players.ids:
            # shouldn't happen, migrate_maps/migrate_players ran first
            maps.get(map_name, cur)
            players.get(name, cur)
        if since:
            (map_name, name, timestamp, time, server, *rest) = row
            cps = rest[:NUM_CHECKPOINTS]
            game_uuid, ddnet7 = rest[NUM_CHECKPOINTS], rest[NUM_CHECKPOINTS + 1]
            batch.append((
                maps.ids[map_name], players.ids[name], time_to_cs(time), clean_ts(timestamp),
                server if server is not None else "", clean_uuid(game_uuid),
                pack_cp_times(cps), bool(ddnet7)))
        else:
            batch.append(finish_row_to_copy(maps, players, row))
        n += 1
        if len(batch) >= BATCH:
            flush(batch)
            batch = []
            if n % (BATCH * 20) == 0:
                log(f"finishes: {n}...")
    flush(batch)
    log(f"finishes: {n} done ({skipped} skipped)")


def migrate_teams(src, pg, prefix, since):
    maps = IdCache(pg, f"{prefix}_map", "map_id")
    players = IdCache(pg, f"{prefix}_player", "player_id")
    cur = pg.cursor()
    where = ""
    args = ()
    if since:
        # improvements update the row's Timestamp in place, so this catches
        # both new and improved team ranks
        where = "WHERE Id IN (SELECT Id FROM %s_teamrace WHERE Timestamp > %%s)" % prefix
        args = (since,)
    query = (f"SELECT Id, Map, Name, Time, Timestamp, GameId, DDNet7 "
             f"FROM {prefix}_teamrace {where} ORDER BY Id")

    n = 0

    def flush_team(team_id, map_name, names, time, timestamp, game_uuid, ddnet7):
        map_id = maps.get(map_name, cur)
        member_ids = [players.get(name, cur) for name in names]
        h = roster_hash(names)
        cur.execute(
            f"INSERT INTO {prefix}_team(team_id, map_id, roster_hash, time_cs, finished_at, server, game_uuid, member_count, ddnet7) "
            "VALUES (%s, %s, %s, %s, %s, '', %s, %s, %s) "
            "ON CONFLICT (map_id, roster_hash) DO UPDATE SET "
            "  time_cs = LEAST({p}_team.time_cs, EXCLUDED.time_cs), "
            "  finished_at = CASE WHEN EXCLUDED.time_cs < {p}_team.time_cs THEN EXCLUDED.finished_at ELSE {p}_team.finished_at END, "
            "  game_uuid = CASE WHEN EXCLUDED.time_cs < {p}_team.time_cs THEN EXCLUDED.game_uuid ELSE {p}_team.game_uuid END "
            "RETURNING team_id".format(p=prefix),
            (psycopg2.Binary(team_id), map_id, psycopg2.Binary(h), time_to_cs(time),
                clean_ts(timestamp), clean_uuid(game_uuid), len(names), bool(ddnet7)))
        kept_id = bytes(cur.fetchone()[0])
        # same roster -> same members, only insert them for the kept team row
        psycopg2.extras.execute_values(
            cur,
            f"INSERT INTO {prefix}_team_player(team_id, player_id) VALUES %s ON CONFLICT DO NOTHING",
            [(psycopg2.Binary(kept_id), player_id) for player_id in member_ids])

    current = None
    for (team_id, map_name, name, time, timestamp, game_uuid, ddnet7) in iter_rows(src, query, args):
        team_id = bytes(team_id)
        if current is not None and current[0] != team_id:
            flush_team(*current[:2], current[2], *current[3:])
            n += 1
            current = None
        if current is None:
            current = [team_id, map_name, [name], time, timestamp, game_uuid, ddnet7]
        else:
            current[2].append(name)
        if n and n % BATCH == 0:
            pg.commit()
    if current is not None:
        flush_team(*current[:2], current[2], *current[3:])
        n += 1
    pg.commit()
    log(f"teams: {n}")


def migrate_saves(src, pg, prefix):
    maps = IdCache(pg, f"{prefix}_map", "map_id")
    cur = pg.cursor()
    n = 0
    for (map_name, code, savegame, timestamp, server, save_uuid, ddnet7) in iter_rows(
            src, f"SELECT Map, Code, Savegame, Timestamp, Server, SaveId, DDNet7 FROM {prefix}_saves"):
        map_id = maps.get(map_name, cur)
        cur.execute(
            f"INSERT INTO {prefix}_save(map_id, code, savegame, saved_at, server, save_uuid, ddnet7) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s) "
            "ON CONFLICT (map_id, code) DO NOTHING",
            (map_id, code, savegame, clean_ts(timestamp), server, clean_uuid(save_uuid), bool(ddnet7)))
        n += 1
    pg.commit()
    log(f"saves: {n}")


def migrate_points(src, pg, prefix):
    """The v1 points table is authoritative (it contains manual adjustments),
    so it's copied as-is instead of being recomputed from the finishes."""
    players = IdCache(pg, f"{prefix}_player", "player_id")
    cur = pg.cursor()
    n = 0
    batch = []

    def flush():
        psycopg2.extras.execute_values(
            cur,
            f"INSERT INTO {prefix}_player_points(player_id, points) VALUES %s "
            "ON CONFLICT (player_id) DO UPDATE SET points = EXCLUDED.points",
            batch)

    for (name, points) in iter_rows(src, f"SELECT Name, Points FROM {prefix}_points"):
        batch.append((players.get(name, cur), points))
        n += 1
        if len(batch) >= BATCH:
            flush()
            batch = []
    if batch:
        flush()
    pg.commit()
    log(f"points: {n}")


def cmd_create_schema(args):
    pg = psycopg2.connect(args.to)
    cur = pg.cursor()
    cur.execute(V2_SCHEMA.format(p=args.prefix))
    pg.commit()
    log("schema created")


def cmd_migrate(args):
    src = connect_source(args.source)
    pg = psycopg2.connect(args.to)
    migrate_maps(src, pg, args.prefix)
    migrate_players(src, pg, args.prefix)
    migrate_finishes(src, pg, args.prefix, args.since)
    migrate_teams(src, pg, args.prefix, args.since)
    migrate_saves(src, pg, args.prefix)
    migrate_points(src, pg, args.prefix)
    if args.since:
        log("delta done, rebuild the derived tables with `finalize`")


def cmd_finalize(args):
    p = args.prefix
    pg = psycopg2.connect(args.to)
    cur = pg.cursor()

    log("adding finish primary key (skipped if it exists)...")
    cur.execute(
        "SELECT COUNT(*) FROM pg_constraint WHERE conname = %s", (f"{p}_finish_pkey",))
    if cur.fetchone()[0] == 0:
        try:
            cur.execute(f"ALTER TABLE {p}_finish ADD PRIMARY KEY (map_id, player_id, time_cs, finished_at, server)")
        except psycopg2.errors.UniqueViolation:
            # v1 rows whose garbage times collapsed onto the same clamped
            # centisecond value; keep one of each
            pg.rollback()
            log("duplicate finish rows after clamping, deduplicating...")
            cur.execute(
                f"DELETE FROM {p}_finish a USING {p}_finish b "
                "WHERE a.ctid > b.ctid AND a.map_id = b.map_id AND a.player_id = b.player_id "
                "AND a.time_cs = b.time_cs AND a.finished_at = b.finished_at AND a.server = b.server")
            log(f"deleted {cur.rowcount} duplicates")
            pg.commit()
            cur.execute(f"ALTER TABLE {p}_finish ADD PRIMARY KEY (map_id, player_id, time_cs, finished_at, server)")
    log("creating indexes...")
    cur.execute(f"CREATE INDEX IF NOT EXISTS {p}_finish_map_time ON {p}_finish(map_id, finished_at)")
    cur.execute(f"CREATE INDEX IF NOT EXISTS {p}_finish_time ON {p}_finish(finished_at)")
    cur.execute(f"CREATE INDEX IF NOT EXISTS {p}_best_player ON {p}_best(player_id)")
    # covering index so rank/top window queries run as index-only scans
    cur.execute(f"CREATE INDEX IF NOT EXISTS {p}_best_cover ON {p}_best(map_id, player_id, server, time_cs)")
    cur.execute(f"CREATE INDEX IF NOT EXISTS {p}_team_player_player ON {p}_team_player(player_id)")
    pg.commit()

    log("rebuilding best table...")
    cur.execute(f"TRUNCATE {p}_best")
    cur.execute(f"""
        INSERT INTO {p}_best(map_id, player_id, server, time_cs, cp_time_cs, cp_times, finish_count, first_finished, last_finished)
        SELECT f.map_id, f.player_id, f.server, MIN(f.time_cs), cp.time_cs, cp.cp_times,
               COUNT(*), MIN(f.finished_at), MAX(f.finished_at)
        FROM {p}_finish f
        LEFT JOIN LATERAL (
            SELECT f2.time_cs, f2.cp_times
            FROM {p}_finish f2
            WHERE f2.map_id = f.map_id AND f2.player_id = f.player_id AND f2.server = f.server
              AND f2.cp_times IS NOT NULL
            ORDER BY f2.time_cs
            LIMIT 1
        ) cp ON true
        GROUP BY f.map_id, f.player_id, f.server, cp.time_cs, cp.cp_times
    """)
    log("rebuilding player.first_finished...")
    cur.execute(f"""
        UPDATE {p}_player p SET first_finished = f.first
        FROM (SELECT player_id, MIN(finished_at) AS first FROM {p}_finish
              WHERE finished_at > '1970-01-01' GROUP BY player_id) f
        WHERE f.player_id = p.player_id
    """)
    if args.team_servers:
        log("deriving team servers from matching finishes...")
        cur.execute(f"""
            UPDATE {p}_team t SET server = COALESCE((
                SELECT f.server FROM {p}_finish f
                JOIN {p}_team_player tp ON tp.team_id = t.team_id AND tp.player_id = f.player_id
                WHERE f.map_id = t.map_id AND f.time_cs = t.time_cs
                LIMIT 1
            ), '')
            WHERE t.server = ''
        """)
    log("vacuuming (needed for index-only scans after the bulk load)...")
    pg.commit()
    old_isolation = pg.isolation_level
    pg.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
    cur.execute(f"VACUUM ANALYZE {p}_finish")
    cur.execute(f"VACUUM ANALYZE {p}_best")
    cur.execute(f"VACUUM ANALYZE {p}_team")
    cur.execute(f"VACUUM ANALYZE {p}_player")
    pg.set_isolation_level(old_isolation)
    log("finalize done")


def cmd_replay(args):
    """Replays a schema v2 sqlite backup file (freeze-window buffer) into the
    v2 PostgreSQL database, with the same semantics as the game's SaveScore:
    first finishes award map points (only with --award-points)."""
    p = args.prefix
    parsed = urllib.parse.urlparse(args.source)
    if parsed.scheme != "sqlite":
        raise SystemExit("replay expects a sqlite:///... source (v2 backup file)")
    lite = sqlite3.connect(f"file:{parsed.path}?mode=ro", uri=True)
    pg = psycopg2.connect(args.to)
    cur = pg.cursor()
    maps = IdCache(pg, f"{p}_map", "map_id")
    players = IdCache(pg, f"{p}_player", "player_id")

    finishes = 0
    points_awarded = 0
    for table in (f"{p}_finish", f"{p}_finish_backup"):
        try:
            rows = lite.execute(f"""
                SELECT m.name, pl.name, f.time_cs, f.finished_at, f.server, f.game_uuid, f.cp_times, f.ddnet7
                FROM {table} f
                JOIN {p}_map m ON m.map_id = f.map_id
                JOIN {p}_player pl ON pl.player_id = f.player_id
            """)
        except sqlite3.OperationalError:
            continue  # table doesn't exist in this file
        for (map_name, name, time_cs, finished_at, server, game_uuid, cp_times, ddnet7) in rows:
            map_id = maps.get(map_name, cur)
            player_id = players.get(name, cur)
            first = False
            if args.award_points:
                cur.execute(
                    f"SELECT COUNT(*) FROM {p}_finish WHERE map_id = %s AND player_id = %s",
                    (map_id, player_id))
                first = cur.fetchone()[0] == 0
            cur.execute(
                f"INSERT INTO {p}_finish(map_id, player_id, time_cs, finished_at, server, game_uuid, cp_times, ddnet7) "
                "VALUES (%s, %s, %s, %s, %s, %s, %s, %s) ON CONFLICT DO NOTHING",
                (map_id, player_id, time_cs, finished_at, server or "", clean_uuid(game_uuid),
                    psycopg2.Binary(cp_times) if cp_times is not None else None, bool(ddnet7)))
            if cur.rowcount == 0:
                continue  # already replayed
            finishes += 1
            if first:
                cur.execute(f"SELECT points FROM {p}_map WHERE map_id = %s", (map_id,))
                map_points = cur.fetchone()[0]
                if map_points:
                    cur.execute(
                        f"INSERT INTO {p}_player_points(player_id, points) VALUES (%s, 0) ON CONFLICT DO NOTHING",
                        (player_id,))
                    cur.execute(
                        f"UPDATE {p}_player_points SET points = points + %s WHERE player_id = %s",
                        (map_points, player_id))
                    points_awarded += map_points
            cp_time = time_cs if cp_times is not None else None
            cur.execute(
                f"INSERT INTO {p}_best(map_id, player_id, server, time_cs, cp_time_cs, cp_times, finish_count, first_finished, last_finished) "
                "VALUES (%s, %s, %s, %s, %s, %s, 1, %s, %s) "
                "ON CONFLICT (map_id, player_id, server) DO UPDATE SET "
                "  finish_count = {p}_best.finish_count + 1, "
                "  time_cs = LEAST({p}_best.time_cs, EXCLUDED.time_cs), "
                "  cp_times = CASE WHEN EXCLUDED.cp_time_cs IS NOT NULL AND ({p}_best.cp_time_cs IS NULL OR {p}_best.cp_time_cs > EXCLUDED.cp_time_cs) THEN EXCLUDED.cp_times ELSE {p}_best.cp_times END, "
                "  cp_time_cs = CASE WHEN EXCLUDED.cp_time_cs IS NOT NULL AND ({p}_best.cp_time_cs IS NULL OR {p}_best.cp_time_cs > EXCLUDED.cp_time_cs) THEN EXCLUDED.cp_time_cs ELSE {p}_best.cp_time_cs END, "
                "  first_finished = LEAST({p}_best.first_finished, EXCLUDED.first_finished), "
                "  last_finished = GREATEST({p}_best.last_finished, EXCLUDED.last_finished)".format(p=p),
                (map_id, player_id, server or "", time_cs, cp_time,
                    psycopg2.Binary(cp_times) if cp_times is not None else None,
                    finished_at, finished_at))
            cur.execute(
                f"UPDATE {p}_player SET first_finished = LEAST(COALESCE(first_finished, %s), %s) WHERE player_id = %s",
                (finished_at, finished_at, player_id))

    teams = 0
    for table in (f"{p}_team", f"{p}_team_backup"):
        try:
            rows = lite.execute(f"""
                SELECT t.team_id, m.name, t.time_cs, t.finished_at, t.server, t.game_uuid, t.member_count, t.ddnet7,
                       (SELECT GROUP_CONCAT(pl.name, char(10)) FROM {p}_team_player tp
                        JOIN {p}_player pl ON pl.player_id = tp.player_id WHERE tp.team_id = t.team_id)
                FROM {table} t JOIN {p}_map m ON m.map_id = t.map_id
            """).fetchall()
        except sqlite3.OperationalError:
            continue
        for (team_id, map_name, time_cs, finished_at, server, game_uuid, member_count, ddnet7, names) in rows:
            names = names.split("\n") if names else []
            map_id = maps.get(map_name, cur)
            member_ids = [players.get(n, cur) for n in names]
            h = roster_hash(names)
            cur.execute(
                f"INSERT INTO {p}_team(team_id, map_id, roster_hash, time_cs, finished_at, server, game_uuid, member_count, ddnet7) "
                "VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s) "
                "ON CONFLICT (map_id, roster_hash) DO UPDATE SET "
                "  time_cs = LEAST({p}_team.time_cs, EXCLUDED.time_cs), "
                "  finished_at = CASE WHEN EXCLUDED.time_cs < {p}_team.time_cs THEN EXCLUDED.finished_at ELSE {p}_team.finished_at END "
                "RETURNING team_id".format(p=p),
                (psycopg2.Binary(bytes(team_id)), map_id, psycopg2.Binary(h), time_cs,
                    finished_at, server or "", clean_uuid(game_uuid), member_count, bool(ddnet7)))
            kept_id = bytes(cur.fetchone()[0])
            psycopg2.extras.execute_values(
                cur,
                f"INSERT INTO {p}_team_player(team_id, player_id) VALUES %s ON CONFLICT DO NOTHING",
                [(psycopg2.Binary(kept_id), player_id) for player_id in member_ids])
            teams += 1

    saves = 0
    for table in (f"{p}_save", f"{p}_save_backup"):
        try:
            rows = lite.execute(f"""
                SELECT m.name, s.code, s.savegame, s.saved_at, s.server, s.save_uuid, s.ddnet7
                FROM {table} s JOIN {p}_map m ON m.map_id = s.map_id
            """).fetchall()
        except sqlite3.OperationalError:
            continue
        for (map_name, code, savegame, saved_at, server, save_uuid, ddnet7) in rows:
            map_id = maps.get(map_name, cur)
            cur.execute(
                f"INSERT INTO {p}_save(map_id, code, savegame, saved_at, server, save_uuid, ddnet7) "
                "VALUES (%s, %s, %s, %s, %s, %s, %s) ON CONFLICT (map_id, code) DO NOTHING",
                (map_id, code, savegame, saved_at, server, clean_uuid(save_uuid), bool(ddnet7)))
            saves += cur.rowcount

    pg.commit()
    log(f"replayed {finishes} finishes ({points_awarded} points awarded), {teams} teams, {saves} saves")


def one(src_or_pg, query, args=()):
    if isinstance(src_or_pg, tuple):
        return next(iter_rows(src_or_pg, query, args))
    cur = src_or_pg.cursor()
    cur.execute(query, args)
    row = cur.fetchone()
    cur.close()
    return row


def cmd_verify(args):
    p = args.prefix
    src = connect_source(args.source)
    pg = psycopg2.connect(args.to)
    failed = False

    def check(name, v1, v2):
        nonlocal failed
        status = "OK " if v1 == v2 else "FAIL"
        if v1 != v2:
            failed = True
        log(f"[{status}] {name}: v1={v1} v2={v2}")

    check("finish count", one(src, f"SELECT COUNT(*) FROM {p}_race")[0],
        one(pg, f"SELECT COUNT(*) FROM {p}_finish")[0])
    check("time checksum", one(src, f"SELECT COALESCE(SUM(LEAST(CAST(ROUND(Time * 100) AS SIGNED), 2147483647)), 0) FROM {p}_race"
        if src[0] == "mysql" else f"SELECT COALESCE(SUM(MIN(CAST(ROUND(Time * 100) AS INTEGER), 2147483647)), 0) FROM {p}_race")[0],
        one(pg, f"SELECT COALESCE(SUM(time_cs::bigint), 0) FROM {p}_finish")[0])
    # v1 can contain multiple team ranks for the same roster ("duplicate
    # teamranks"), which v2 collapses into one row, so compare against the
    # number of distinct (map, roster) groups
    rosters = {}
    for (team_id, map_name, name) in iter_rows(src, f"SELECT Id, Map, Name FROM {p}_teamrace"):
        rosters.setdefault(bytes(team_id), (map_name, []))[1].append(name)
    distinct_rosters = len({(map_name, tuple(sorted(names))) for (map_name, names) in rosters.values()})
    check("team count", distinct_rosters, one(pg, f"SELECT COUNT(*) FROM {p}_team")[0])
    check("map count", one(src, f"SELECT COUNT(*) FROM {p}_maps")[0],
        one(pg, f"SELECT COUNT(*) FROM {p}_map")[0])
    check("points sum", one(src, f"SELECT COALESCE(SUM(Points), 0) FROM {p}_points")[0],
        one(pg, f"SELECT COALESCE(SUM(points), 0) FROM {p}_player_points")[0])
    check("save count", one(src, f"SELECT COUNT(*) FROM {p}_saves")[0],
        one(pg, f"SELECT COUNT(*) FROM {p}_save")[0])

    # spot check: best time per map for the 20 maps with the most finishes
    rows = list(iter_rows(src,
        f"SELECT Map, MIN(Time), COUNT(*) FROM {p}_race GROUP BY Map ORDER BY COUNT(*) DESC LIMIT 20"))
    for (map_name, best, count) in rows:
        v2 = one(pg, f"""
            SELECT MIN(b.time_cs), COALESCE(SUM(b.finish_count), 0) FROM {p}_best b
            JOIN {p}_map m ON m.map_id = b.map_id WHERE m.name = %s""", (map_name,))
        check(f"map '{map_name}' best/finishes", (time_to_cs(best), count), (v2[0], v2[1]))

    if failed:
        raise SystemExit(1)
    log("verification passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def add(name, func, source=False):
        cmd = sub.add_parser(name)
        cmd.set_defaults(func=func)
        cmd.add_argument("--to", required=True, help="psycopg2 conninfo of the v2 PostgreSQL database")
        cmd.add_argument("--prefix", default="record")
        if source:
            cmd.add_argument("--from", dest="source", required=True,
                help="mysql://user:pass@host:port/db or sqlite:///path")
        return cmd

    add("create-schema", cmd_create_schema)
    migrate = add("migrate", cmd_migrate, source=True)
    migrate.add_argument("--since", default=None,
        help="delta mode: only copy finishes/teams with Timestamp > SINCE (UTC); requires a prior `finalize`")
    finalize = add("finalize", cmd_finalize)
    finalize.add_argument("--team-servers", action="store_true",
        help="derive team.server from matching finishes (for regional team ranks, slow)")
    replay = add("replay", cmd_replay, source=True)
    replay.add_argument("--award-points", action="store_true",
        help="award map points for first finishes, like the game's SaveScore")
    add("verify", cmd_verify, source=True)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
