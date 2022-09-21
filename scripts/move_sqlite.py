#!/usr/bin/env python3

# This script is intended to be called automatically every day (e.g. via cron).
# It only output stuff if new ranks have to be inserted. Therefore the output
# may be redirected to email notifying about manual action to transfer the
# ranks to MySQL.
#
# Configure cron as the user running the DDNet-Server processes
#
#     $ crontab -e
#     30 5 * * * /path/to/this/script/move_sqlite.py --from /path/to/ddnet-server.sqlite
#
# Afterwards configure a MTA (e.g. postfix) and the users email address.

import sqlite3
import argparse
from time import strftime
import os

def sqlite_table_exists(cursor, table):
	cursor.execute(f"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='{table}'")
	return cursor.fetchone()[0] != 0

def sqlite_num_transfer(conn, table):
	c = conn.cursor()
	if not sqlite_table_exists(c, table):
		return 0
	c.execute(f'SELECT COUNT(*) FROM {table}')
	num = c.fetchone()[0]
	return num

def transfer(file_from, file_to):
	conn_to = sqlite3.connect(file_to, isolation_level='EXCLUSIVE')
	cursor_to = conn_to.cursor()

	conn_from = sqlite3.connect(file_from, isolation_level='EXCLUSIVE')
	conn_from.text_factory = lambda b: b.decode(errors = 'ignore').rstrip()
	for line in conn_from.iterdump():
		cursor_to.execute(line)
		print(line.encode('utf-8'))
	cursor_to.close()
	conn_to.commit()
	conn_to.close()

	cursor_from = conn_from.cursor()
	if sqlite_table_exists(cursor_from, 'record_race'):
		cursor_from.execute('DELETE FROM record_race')
	if sqlite_table_exists(cursor_from, 'record_teamrace'):
		cursor_from.execute('DELETE FROM record_teamrace')
	if sqlite_table_exists(cursor_from, 'record_saves'):
		cursor_from.execute('DELETE FROM record_saves')
	cursor_from.close()
	conn_from.commit()
	conn_from.close()

def main():
	default_output = 'ddnet-server-' + strftime('%Y-%m-%d') + '.sqlite'
	parser = argparse.ArgumentParser(
			description='Move DDNet ranks, teamranks and saves from a possible active SQLite3 to a new one',
			formatter_class=argparse.ArgumentDefaultsHelpFormatter)
	parser.add_argument('--from', '-f', dest='f',
			default='ddnet-server.sqlite',
			help='Input file where ranks are deleted from when moved successfully (default: ddnet-server.sqlite)')
	parser.add_argument('--to', '-t',
			default=default_output,
			help='Output file where ranks are saved adds current date by default')
	args = parser.parse_args()

	if not os.path.exists(args.f):
		print(f"Warning: '{args.f}' does not exist (yet). Is the path specified correctly?")
		return

	conn = sqlite3.connect(args.f)
	num_ranks = sqlite_num_transfer(conn, 'record_race')
	num_teamranks = sqlite_num_transfer(conn, 'record_teamrace')
	num_saves = sqlite_num_transfer(conn, 'record_saves')
	num = num_ranks + num_teamranks + num_saves
	conn.close()
	if num == 0:
		return

	print(f'{num} new entries in backup database found ({num_ranks} ranks, {num_teamranks} teamranks, {num_saves} saves)')
	print(f'Moving entries from {os.path.abspath(args.f)} to {os.path.abspath(args.to)}')
	print("You can use the following commands to import the entries to MySQL (using https://github.com/techouse/sqlite3-to-mysql/):")
	print()
	print(f"sqlite3mysql --sqlite-file {os.path.abspath(args.to)} --ignore-duplicate-keys --mysql-insert-method IGNORE --sqlite-tables record_race record_teamrace record_saves --mysql-password 'PW2' --mysql-host 'host' --mysql-database teeworlds --mysql-user teeworlds")
	print(f"When the ranks are transfered successfully to mysql, {os.path.abspath(args.to)} can be removed")
	print()
	print("Log of the transfer:")
	print()

	transfer(args.f, args.to)

if __name__ == '__main__':
	main()
