#!/usr/bin/env python3
from collections import namedtuple
from queue import Queue
from threading import Thread
from time import time
from urllib.request import urlopen
from uuid import uuid4, UUID
import io
import json
import os
import queue
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import traceback

# TODO: less strict default timeouts?

# TODO: what kind of ASAN support did integration_test.sh have?

# TODO: check for valgrind errors

class Log(namedtuple("Log", "timestamp level line")):
	@classmethod
	def parse(cls, line):
		if line.startswith("=="):
			pid, line = line[2:].split("== ", 1)
			return cls(None, "valgrind", f"{pid}: {line}")
		elif not line.startswith("["):
			# DDNet log
			date, time, level, line = line.split(" ", 3)
			return cls(f"{date} {time}", level, line)
		else:
			# Rust log
			datetime, level, line = line.split(" ", 2)
			return cls(datetime.removeprefix("["), level, line.removeprefix(" ").replace("]", ":", 1))
	def raise_on_error(self, timeout_id):
		pass

class LogParseError(namedtuple("LogParseError", "line")):
	def raise_on_error(self, timeout_id): # pylint: disable=unused-argument
		Log.parse(self.line)
		# The above should have raised an error.
		raise RuntimeError("log line shouldn't parse")

class Exit(namedtuple("Exit", "")):
	def raise_on_error(self, timeout_id): # pylint: disable=unused-argument
		pass

class UncleanExit(namedtuple("UncleanExit", "reason")):
	def raise_on_error(self, timeout_id): # pylint: disable=unused-argument
		raise RuntimeError(f"unclean exit: {self.reason}")

class TestTimeout(namedtuple("TestTimeout", "")):
	def raise_on_error(self, timeout_id): # pylint: disable=unused-argument
		raise TimeoutError("test timeout")

class Timeout(namedtuple("Timeout", "id")):
	def raise_on_error(self, timeout_id):
		if timeout_id == self.id:
			raise TimeoutError("timeout")

# This class is used to track that each timeout value is multiplied by
# `timeout_multiplier` exactly once.
class TimeoutParam(namedtuple("Timeout", "start unmultiplied_duration")):
	def __new__(cls, duration):
		return super().__new__(cls, time(), duration)
	def remaining_duration(self, test_env):
		duration = test_env.runner.timeout_multiplier * self.unmultiplied_duration
		return max((self.start + duration) - time(), 0)

def relpath(path, start=os.curdir):
	try:
		return os.path.relpath(path, start)
	except ValueError:
		return os.path.realpath(path)

def popen(args, *, cwd, **kwargs):
	# If cwd is set, we might need to fix up the program path: On Windows, the
	# executed program is relative to the current process's working directory.
	if cwd is not None and os.name == "nt":
		# If relative and contains a path separator.
		if not os.path.isabs(args[0]) and os.path.dirname(args[0]) != "":
			args = [relpath(os.path.join(cwd, args[0]))] + args[1:]
	return subprocess.Popen(args, cwd=cwd, **kwargs)

GREEN="\x1b[32m"
RED="\x1b[31m"
RESET="\x1b[m"
YELLOW="\x1b[33m"

class TestRunner:
	def __init__(self, ddnet, ddnet_server, ddnet_mastersrv, repo_dir, dir, valgrind_memcheck, keep_tmpdirs, timeout_multiplier):
		self.ddnet = ddnet
		self.ddnet_server = ddnet_server
		self.ddnet_mastersrv = ddnet_mastersrv
		self.repo_dir = repo_dir
		self.data_dir = os.path.join(repo_dir, "data")
		self.dir = dir
		self.extra_env_vars = {}
		self.keep_tmpdirs = keep_tmpdirs
		self.timeout_multiplier = timeout_multiplier
		self.valgrind_memcheck = valgrind_memcheck
		if self.valgrind_memcheck:
			self.timeout_multiplier *= 10

	def run_test(self, test):
		tmp_dir = tempfile.mkdtemp(prefix=f"integration_{test.name}_", dir=self.dir)
		tmp_dir_cleanup = not self.keep_tmpdirs
		try:
			env = TestEnvironment(self, test.name, tmp_dir, timeout=test.timeout)
			try:
				test(env)
			except Exception as e: # pylint: disable=broad-exception-caught
				env.kill_all()
				error = "".join(traceback.format_exception(type(e), e, e.__traceback__))
				tmp_dir_cleanup = False
			else:
				env.kill_all()
				error = None
				if self.valgrind_memcheck:
					error = env.check_valgrind_memcheck_errors()
		finally:
			if tmp_dir_cleanup:
				shutil.rmtree(tmp_dir)
				tmp_dir = None
		return relpath(tmp_dir) if tmp_dir is not None else None, error

	def run_tests(self, tests):
		tests = list(tests)
		# pylint: disable=consider-using-f-string
		print("running {} test{}".format(len(tests), "s" if len(tests) != 1 else ""))
		start = time()
		failed = []
		num_passed = 0
		num_skipped = 0
		for test in tests:
			if test.requires_mastersrv and self.ddnet_mastersrv is None:
				print(f"{test.name} ... {YELLOW}skipped{RESET}")
				num_skipped += 1
				continue
			print(f"{test.name} ... ", end="", flush=True)
			tmp_dir, error = self.run_test(test)
			tmp_dir_formatted = f" ({tmp_dir})" if tmp_dir is not None else ""
			if error:
				print(f"{RED}FAILED{RESET}{tmp_dir_formatted}")
				failed.append((test.name, error))
			else:
				print(f"{GREEN}ok{RESET}{tmp_dir_formatted}")
				num_passed += 1
		print()
		if len(tests) != len(failed) + num_passed + num_skipped:
			raise AssertionError("invalid counts")
		if failed:
			print("failures:")
			print()
			for test, details in failed:
				print(f"---- {test} ----")
				print(details)
		if failed:
			print("failures:")
			for test, _ in failed:
				print(f"    {test}")
			print()
		result = f"{RED}FAILED{RESET}" if failed else f"{GREEN}ok{RESET}"
		duration = time() - start
		print(f"test result: {result}. {num_passed} passed; {num_skipped} skipped, {len(failed)} failed; finished in {duration:.2f}s")
		print()
		return bool(failed)

class TestEnvironment:
	def __init__(self, runner, name, tmp_dir, timeout):
		self.runner = runner
		self.tmp_dir = tmp_dir
		with open(os.path.join(self.tmp_dir, "storage.cfg"), "w", encoding="utf-8") as f:
			f.write(f"""\
add_path .
add_path {relpath(self.runner.data_dir, tmp_dir)}
""")
		self.ddnet = os.path.relpath(runner.ddnet, self.tmp_dir)
		self.ddnet_server = os.path.relpath(runner.ddnet_server, self.tmp_dir)
		self.ddnet_mastersrv = os.path.relpath(runner.ddnet_mastersrv, self.tmp_dir) if runner.ddnet_mastersrv is not None else None
		self.run_prefix_args = []
		if self.runner.valgrind_memcheck:
			self.run_prefix_args = [
				"valgrind",
				"--tool=memcheck",
				"--gen-suppressions=all",
				# pylint: disable=consider-using-f-string
				"--suppressions={}".format(relpath(os.path.join(runner.repo_dir, "memcheck.supp"), self.tmp_dir)),
				"--track-origins=yes",
			]
		self.name = name
		self.num_clients = 0
		self.num_servers = 0
		self.num_mastersrvs = 0
		self.processes = []
		self.run_id = uuid4()
		self.full_stderrs = []
		self.test_timeout_queue = Queue()
		run_test_timeout_thread(f"{self.name}_timeout", self, self.test_timeout_queue, TimeoutParam(timeout))

	def __del__(self):
		self.kill_all()

	def register_process(self, process, full_stderr):
		self.processes.append(process)
		self.full_stderrs.append(full_stderr)

	def register_events_queue(self, queue):
		self.test_timeout_queue.put(queue)

	def server(self, *args, **kwargs):
		return Server(self, *args, **kwargs)

	def client(self, *args, **kwargs):
		return Client(self, *args, **kwargs)

	def mastersrv(self, *args, **kwargs):
		return Mastersrv(self, *args, **kwargs)

	def kill_all(self):
		for process in self.processes:
			if process.poll() is None:
				#print("warning: process hasn't terminated") # TODO
				process.kill()
		while self.processes:
			self.processes.pop().wait()

	def check_valgrind_memcheck_errors(self):
		if any(any("== ERROR SUMMARY: " in line and not "== ERROR SUMMARY: 0" in line for line in stderr) for stderr in self.full_stderrs):
			return "\n".join(line for stderr in self.full_stderrs for line in stderr if line.startswith("=="))
		return None

def run_lines_thread(name, file, output_filename, output_list, output_queue):
	def thread():
		output_file = None
		for line in file:
			if output_file is None:
				# pylint: disable=consider-using-with
				output_file = open(output_filename, "w", buffering=1, encoding="utf-8") # line buffering
			output_file.write(line)
			line = line.rstrip("\r\n")
			output_list.append(line)
			if output_queue is not None:
				try:
					log = Log.parse(line)
				except ValueError:
					output_queue.put(LogParseError(line))
				else:
					output_queue.put(log)
	Thread(name=name, target=thread, daemon=True).start()

def run_exit_thread(name, process, queue, allow_unclean_exit):
	def thread():
		exit_code = process.wait()
		if allow_unclean_exit or exit_code == 0:
			queue.put(Exit())
		else:
			queue.put(UncleanExit(f"exit code {exit_code}"))
	Thread(name=name, target=thread, daemon=True).start()

def run_timeout_thread(name, test_env, input_queue, output_queue):
	def thread():
		param = None
		while True:
			timeout = param.remaining_duration(test_env) if param is not None else None
			try:
				id, param = input_queue.get(timeout=timeout)
			except queue.Empty:
				output_queue.put(Timeout(id))
				param = None
				del id
			# TODO: quit this thread
	Thread(name=name, target=thread, daemon=True).start()

def run_test_timeout_thread(name, test_env, input_queue, param):
	def thread():
		outputs = []
		while True:
			timeout = param.remaining_duration(test_env)
			try:
				new_output = input_queue.get(timeout=timeout)
			except queue.Empty:
				for output in outputs:
					output.put(TestTimeout())
				break
			else:
				outputs.append(new_output)
	Thread(name=name, target=thread, daemon=True).start()

class Runnable:
	# pylint: disable=dangerous-default-value
	def __init__(self, test_env, name, args, *, extra_env_vars={}, log_is_stderr=False, allow_unclean_exit=False):
		self.name = name
		cur_env_vars = dict(os.environ)
		intersection = set(cur_env_vars) & (set(test_env.runner.extra_env_vars) | set(extra_env_vars))
		if intersection:
			# pylint: disable=consider-using-f-string
			raise ValueError("conflicting environment variable(s): {}".format(
				", ".join(sorted(intersection))
			))
		new_env_vars = {**cur_env_vars, **test_env.runner.extra_env_vars, **extra_env_vars}
		self.process = popen(
			test_env.run_prefix_args + args,
			cwd=test_env.tmp_dir,
			env=new_env_vars,
			stdin=subprocess.DEVNULL,
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
		)
		stdout_wrapper = io.TextIOWrapper(self.process.stdout, encoding="utf-8", newline="\n")
		stderr_wrapper = io.TextIOWrapper(self.process.stderr, encoding="utf-8", newline="\n")
		self.full_stdout = []
		self.full_stderr = []
		test_env.register_process(self.process, self.full_stderr)
		self.events = Queue()
		test_env.register_events_queue(self.events)
		self.next_timeout_id = 0
		self.timeout_queue = Queue()
		global_name = f"{test_env.name}_{self.name}"
		stdout_path = os.path.join(test_env.tmp_dir, f"{self.name}.stdout")
		stderr_path = os.path.join(test_env.tmp_dir, f"{self.name}.stderr")
		run_timeout_thread(f"{global_name}_timeout", test_env, self.timeout_queue, self.events)
		run_lines_thread(f"{global_name}_stdout", stdout_wrapper, stdout_path, self.full_stdout, self.events if not log_is_stderr else None)
		run_lines_thread(f"{global_name}_stderr", stderr_wrapper, stderr_path, self.full_stderr, self.events if log_is_stderr else None)
		run_exit_thread(f"{global_name}_exit", self.process, self.events, allow_unclean_exit)
	def register_timeout(self, timeout):
		timeout_id = self.next_timeout_id
		self.next_timeout_id += 1
		self.timeout_queue.put((timeout_id, TimeoutParam(timeout)))
		return timeout_id
	def next_event(self, timeout_id):
		event = self.events.get()
		event.raise_on_error(timeout_id)
		return event
	def clear_events(self):
		while True:
			try:
				event = self.events.get(block=False)
			except queue.Empty:
				break
			else:
				event.raise_on_error(None)
	def wait_for_log(self, fn, timeout=1):
		timeout_id = self.register_timeout(timeout)
		while True:
			event = self.next_event(timeout_id)
			if isinstance(event, Exit):
				raise EOFError("program exited before reaching wanted log line")
			elif isinstance(event, Log):
				if fn(event):
					return event
	def wait_for_log_prefix(self, prefix, timeout=1):
		self.wait_for_log(lambda l: l.line.startswith(prefix), timeout=timeout)
	def wait_for_log_exact(self, line, timeout=1):
		self.wait_for_log(lambda l: l.line == line, timeout=timeout)
	def wait_for_exit(self, timeout=10):
		timeout_id = self.register_timeout(timeout)
		while True:
			event = self.next_event(timeout_id)
			if isinstance(event, Exit):
				return

def fifo_name(test_env, name):
	if os.name != "nt":
		return os.path.join(test_env.tmp_dir, f"{name}.fifo")
	else:
		return f"{test_env.name}_{test_env.run_id}_{name}"

def open_fifo(name):
	if os.name != "nt":
		name_arg = os.open(name, flags=os.O_WRONLY)
	else:
		name_arg = fr"\\.\pipe\{name}"
	return open(name_arg, "w", buffering=1, encoding="utf-8") # line buffering

class Client(Runnable):
	# pylint: disable=dangerous-default-value
	def __init__(self, test_env, extra_args=[]):
		name = f"client{test_env.num_clients}"
		self.fifo_name = fifo_name(test_env, name)
		# Delay opening the FIFO until the client has started, because it will
		# block.
		self.fifo = None
		super().__init__(
			test_env,
			name,
			[
				test_env.ddnet,
				f"cl_input_fifo {self.fifo_name}",
				"gfx_fullscreen 0",
			] + extra_args,
		)
		test_env.num_clients += 1
	def command(self, command):
		if self.fifo is None:
			self.fifo = open_fifo(self.fifo_name)
		self.fifo.write(f"{command}\n")
	def exit(self):
		self.command("quit")
	def wait_for_startup(self, timeout=15):
		self.wait_for_log_prefix("client: version", timeout=timeout)

class Server(Runnable):
	# pylint: disable=dangerous-default-value
	def __init__(self, test_env, extra_args=[]):
		name = f"server{test_env.num_servers}"
		self.fifo_name = fifo_name(test_env, name)
		# Delay opening the FIFO until the server has started, because it will
		# block.
		self.fifo = None
		super().__init__(
			test_env,
			name,
			[
				test_env.ddnet_server,
				f"sv_input_fifo {self.fifo_name}",
				"sv_register 0",
			] + extra_args,
		)
		test_env.num_servers += 1
	def command(self, command):
		if self.fifo is None:
			self.fifo = open_fifo(self.fifo_name)
		self.fifo.write(f"{command}\n")
	def next_event(self, timeout_id):
		event = super().next_event(timeout_id)
		if isinstance(event, Log):
			if event.line.startswith("server: using port "):
				self.port = int(event.line[len("server: using port "):]) # pylint: disable=attribute-defined-outside-init
			elif event.line.startswith("server: | rcon password: '"):
				_, self.rcon_password, _ = event.line.split("'") # pylint: disable=attribute-defined-outside-init
			elif event.line.startswith("teehistorian: recording to '"):
				_, self.teehistorian_filename, _ = event.line.split("'") # pylint: disable=attribute-defined-outside-init
		return event
	def exit(self):
		self.command("shutdown")
	def wait_for_startup(self, timeout=5):
		self.wait_for_log_prefix("server: version", timeout=timeout)

class Mastersrv(Runnable):
	# pylint: disable=dangerous-default-value
	def __init__(self, test_env, extra_args=[]):
		name = f"mastersrv{test_env.num_mastersrvs}"
		super().__init__(
			test_env,
			name,
			[
				test_env.ddnet_mastersrv,
				"--listen",
				"[::]:0",
				"--test-servers-route",
			] + extra_args,
			extra_env_vars={"RUST_LOG": "info,mastersrv=debug"},
			log_is_stderr=True,
			allow_unclean_exit=True, # We don't have a way to exit the mastersrv cleanly.
		)
		test_env.num_mastersrvs += 1
	def next_event(self, timeout_id):
		event = super().next_event(timeout_id)
		if isinstance(event, Log):
			if event.line.startswith("warp::server: listening on http://[::]:"):
				self.port = int(event.line[len("warp::server: listening on http://[::]:"):]) # pylint: disable=attribute-defined-outside-init
		return event
	def exit(self):
		self.process.terminate()
	def wait_for_startup(self, timeout=5):
		self.wait_for_log_prefix("warp::server: listening on http://[::]:", timeout=timeout)
	def servers_json(self):
		return json.loads(urlopen(f"http://[::1]:{self.port}/ddnet/15/test-servers.json").read())

ALL_TESTS = []
def test(test=None, *, requires_mastersrv=False, timeout=60):
	def apply(test):
		test.name = test.__name__
		test.requires_mastersrv = requires_mastersrv
		test.timeout = timeout
		ALL_TESTS.append(test)
		return test
	if test is None:
		return apply
	else:
		return apply(test)

def wait_for_startup(l):
	for el in l:
		el.wait_for_startup()

@test(timeout=10)
def meta_timeout(test_env):
	server = test_env.server()
	wait_for_startup([server])
	try:
		server.wait_for_exit(timeout=0.1)
	except TimeoutError as e:
		if str(e) != "timeout":
			raise
	else:
		raise AssertionError("timeout should have triggered")
	server.exit()
	server.wait_for_exit()

@test(timeout=0.1)
def meta_test_timeout(test_env):
	server = test_env.server()
	try:
		server.wait_for_exit(timeout=0.1)
	except TimeoutError as e:
		if str(e) != "test timeout":
			raise
	else:
		raise AssertionError("timeout should have triggered")
	# with the global timeout disabled, better exit the test quickly without waiting

@test
def start_server(test_env):
	server = test_env.server()
	wait_for_startup([server])
	server.exit()
	server.wait_for_exit()

@test
def start_client(test_env):
	client = test_env.client(["gfx_fullscreen 1"])
	wait_for_startup([client])
	client.exit()
	client.wait_for_exit()

# TODO: make this less verbose
@test
def client_can_connect(test_env):
	client = test_env.client()
	server = test_env.server()
	wait_for_startup([client, server])
	client.command(f"connect localhost:{server.port}")
	server.wait_for_log_prefix("server: player has entered the game", timeout=10)
	server.exit()
	client.wait_for_log_exact("client: offline error='Server shutdown'")
	client.exit()
	server.wait_for_exit()
	client.wait_for_exit()

@test
def smoke_test(test_env):
	client1 = test_env.client(["logfile client1.log", "player_name client1"])
	server = test_env.server(["logfile server.log", "sv_demo_chat 1", "sv_map coverage", "sv_tee_historian 1"])
	wait_for_startup([client1, server])

	client1.command("debug 1")
	client1.command("stdout_output_level 2; loglevel 2")
	client1.command(f"connect localhost:{server.port}")
	server.wait_for_log_prefix("server: player has entered the game", timeout=10)
	server.command("record server")
	client1.wait_for_log_exact("client: state change. last=2 current=3", timeout=10)
	client1.command("stdout_output_level 0; loglevel 0")
	client1.command("debug 0")
	client1.command("record client1")

	client2 = test_env.client(["logfile client2.log", "player_name client2", f"connect localhost:{server.port}"])
	wait_for_startup([client2])
	server.wait_for_log_prefix("server: player has entered the game", timeout=10)
	for _ in range(5):
		server.wait_for_log(
			lambda l: l.line.startswith("chat: *** client1 finished in:") or
				l.line.startswith("chat: *** client2 finished in:"),
			timeout=20,
		)

	client1.command("say hello world")
	server.wait_for_log_exact("chat: 0:-2:client1: hello world")

	client1.command(f"rcon_auth {server.rcon_password}")
	server.wait_for_log_exact("server: ClientId=0 authed with key=default_admin (admin)")

	client1.command("say \"/mc; {}\"".format("; ".join(l.strip() for l in """
		top5
		rank
		team 512
		emote happy -999
		pause
		points
		mapinfo
		list
		whisper client2 hi
		kill
		settings cheats
		timeout 123
		timer broadcast
		cmdlist
		saytime
	""".strip().split("\n"))))
	client1.command("; ".join(l.strip() for l in """
		rcon say hello from admin
		rcon broadcast test
		rcon status
		rcon echo test
		rcon muteid 1 900 spam
		rcon unban_all
		rcon say the end
	""".strip().split("\n")))
	client1.wait_for_log_exact("chat/server: *** the end")

	server.command("stoprecord")
	client1.command("stoprecord")

	game_uuid = str(UUID(server.teehistorian_filename.removeprefix("teehistorian/").removesuffix(".teehistorian")))

	client1.command("rcon sv_map Tutorial")

	for _ in range(2):
		server.wait_for_log_prefix("server: player has entered the game", timeout=10)

	client1.clear_events()
	client2.clear_events()

	client1.command("play demos/server.demo")
	client2.command("play demos/client1.demo")

	client1.wait_for_log_prefix("chat/server: *** client1 finished in:", timeout=20)
	client2.wait_for_log_prefix("chat/server: *** client1 finished in:", timeout=20)

	client1.exit()
	client2.exit()
	server.exit()
	client1.wait_for_exit()
	client2.wait_for_exit()
	server.wait_for_exit()

	if not all(any(word in line for line in client1.full_stdout) for word in "cmdlist pause rank points".split()):
		raise AssertionError("did not find output of /cmdlist command")
	if not any("hello from admin" in line for line in server.full_stdout):
		raise AssertionError("admin message not found in server output")

	conn = sqlite3.connect(os.path.join(test_env.tmp_dir, "ddnet-server.sqlite"))
	ranks = list(conn.execute("SELECT * FROM record_race"))
	conn.close()

	# strip timestamps
	ranks = sorted(rank[:2] + rank[3:] for rank in ranks)

	expected_ranks = [
		('coverage', 'client1', 6248.56, 'UNK', 0.42, 0.5, 0.0, 0.66, 0.92, 0.02, 300.18, 300.46, 300.76, 300.88, 300.98, 301.02, 301.04, 301.06, 301.08, 301.18, 301.38, 301.66, 307.34, 308.08, 308.1, 308.14, 308.44, 6248.5, 6248.54, game_uuid, 0),
		('coverage', 'client1', 168300.5, 'UNK', 0.02, 0.06, 0.12, 15300.14, 15300.18, 30600.2, 30600.22, 45900.24, 45900.26, 61200.28, 61200.3, 76500.32, 76500.34, 91800.36, 91800.36, 107100.38, 107100.4, 122400.42, 122400.42, 137700.44, 137700.45, 137700.45, 153000.48, 153000.48, 0.0, game_uuid, 0),
		('coverage', 'client2', 302.02, 'UNK', 0.42, 0.5, 0.0, 0.66, 0.92, 0.02, 300.18, 300.46, 300.76, 300.88, 300.98, 301.16, 301.24, 301.28, 301.3, 301.86, 301.96, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, game_uuid, 0),
		('coverage', 'client2', 1020.38, 'UNK', 1021.34, 0.02, 0.04, 0.04, 0.06, 600.08, 600.1, 600.12, 600.12, 1020.14, 1020.16, 1020.18, 1020.2, 1020.2, 1020.22, 1020.24, 1020.26, 1020.26, 1020.28, 1020.3, 1020.3, 1020.32, 1020.34, 1020.34, 1020.36, game_uuid, 0),
		('coverage', 'client2', 1020.98, 'UNK', 0.02, 0.1, 0.2, 0.26, 0.32, 600.36, 600.42, 600.46, 600.5, 1020.54, 1020.58, 1020.6, 1020.64, 1020.66, 1020.7, 1020.72, 1020.76, 1020.78, 1020.8, 1020.84, 1020.86, 1020.88, 1020.9, 1020.94, 1020.96, game_uuid, 0),
	]

	if not ranks:
		raise AssertionError("no ranks found")
	# TODO: why do the results under valgrind differ?
	if not test_env.runner.valgrind_memcheck and ranks != expected_ranks:
		raise AssertionError(f"unexpected ranks:\n{ranks}\n\n{expected_ranks}")

@test(requires_mastersrv=True)
def start_mastersrv(test_env):
	mastersrv = test_env.mastersrv()
	wait_for_startup([mastersrv])
	mastersrv.exit()
	mastersrv.wait_for_exit()

@test(requires_mastersrv=True)
def server_can_register(test_env):
	mastersrv = test_env.mastersrv()
	wait_for_startup([mastersrv])
	server = test_env.server([
		"http_allow_insecure 1",
		"sv_register ipv6",
		f"sv_register_url http://[::1]:{mastersrv.port}/ddnet/15/register",
	])
	wait_for_startup([server])
	server.wait_for_log(lambda l: l.line.endswith("successfully registered"), timeout=5)
	server.wait_for_log(lambda l: l.line.endswith("successfully registered"), timeout=5)
	servers_json = mastersrv.servers_json()
	if len(servers_json["servers"]) != 1 or servers_json["servers"][0]["info"]["map"]["name"] != "Tutorial" \
			or len(servers_json["servers"][0]["addresses"]) != 2:
		raise AssertionError(f"unexpected servers.json\n{servers_json}")
	server.exit()
	mastersrv.wait_for_log_prefix("mastersrv: successfully removed", timeout=5)
	mastersrv.wait_for_log_prefix("mastersrv: successfully removed", timeout=5)
	servers_json = mastersrv.servers_json()
	if len(servers_json["servers"]) != 0:
		raise AssertionError(f"unexpected servers.json\n{servers_json}")
	mastersrv.exit()
	mastersrv.wait_for_exit()

def server_can_register_protocol(test_env, protocol_config, protocol_log, protocol_scheme):
	mastersrv = test_env.mastersrv()
	wait_for_startup([mastersrv])
	server = test_env.server([
		"http_allow_insecure 1",
		f"sv_register {protocol_config}",
		f"sv_register_url http://[::1]:{mastersrv.port}/ddnet/15/register",
	])
	wait_for_startup([server])
	server.wait_for_log_exact(f"register/{protocol_log}: successfully registered", timeout=5)
	servers_json = mastersrv.servers_json()
	if len(servers_json["servers"]) != 1 or servers_json["servers"][0]["info"]["map"]["name"] != "Tutorial" \
			or len(servers_json["servers"][0]["addresses"]) != 1 \
			or not servers_json["servers"][0]["addresses"][0].startswith(f"{protocol_scheme}://[::1]:"):
		raise AssertionError(f"unexpected servers.json\n{servers_json}")
	server.exit()
	mastersrv.wait_for_log_prefix(f"mastersrv: successfully removed {protocol_scheme}://[::1]:", timeout=5)
	servers_json = mastersrv.servers_json()
	if len(servers_json["servers"]) != 0:
		raise AssertionError(f"unexpected servers.json\n{servers_json}")
	mastersrv.exit()
	mastersrv.wait_for_exit()

@test(requires_mastersrv=True)
def server_can_register_tw_0_6(test_env):
	server_can_register_protocol(test_env, "tw0.6/ipv6", "6/ipv6", "tw-0.6+udp")

@test(requires_mastersrv=True)
def server_can_register_tw_0_7(test_env):
	server_can_register_protocol(test_env, "tw0.7/ipv6", "7/ipv6", "tw-0.7+udp")

EXE_SUFFIX = ""
if os.name == "nt":
	EXE_SUFFIX = ".exe"

def main():
	repo_dir = relpath(os.path.join(os.path.dirname(__file__), ".."))

	import argparse # pylint: disable=import-outside-toplevel
	parser = argparse.ArgumentParser()
	parser.add_argument("--keep-tmpdirs", action="store_true", help="keep temporary directories used for the tests")
	parser.add_argument("--test-mastersrv", action="store_true", help="enforce testing of mastersrv")
	parser.add_argument("--timeout-multiplier", type=float, default=1, help="multiply all timeouts by this value")
	parser.add_argument("--valgrind-memcheck", action="store_true", help="use valgrind's memcheck on client and server")
	parser.add_argument("builddir", metavar="BUILDDIR", help="path to ddnet build directory")
	parser.add_argument("test", metavar="TEST", nargs="?", help="name of test to run")
	args = parser.parse_args()

	ddnet = os.path.join(args.builddir, f"DDNet{EXE_SUFFIX}")
	ddnet_server = os.path.join(args.builddir, f"DDNet-Server{EXE_SUFFIX}")
	ddnet_mastersrv = os.path.join(args.builddir, f"mastersrv{EXE_SUFFIX}")
	if not os.path.exists(ddnet):
		raise RuntimeError(f"client binary {ddnet!r} not found")
	if not os.path.exists(ddnet_server):
		raise RuntimeError(f"server binary {ddnet_server!r} not found")
	if not os.path.exists(ddnet_mastersrv):
		if args.test_mastersrv:
			raise RuntimeError(f"mastersrv binary {ddnet_mastersrv!r} not found, compile it from src/mastersrv")
		else:
			ddnet_mastersrv = None

	tests = ALL_TESTS
	if args.test is not None:
		tests = [test for test in tests if args.test in test.name]

	TestRunner(
		ddnet=ddnet,
		ddnet_server=ddnet_server,
		ddnet_mastersrv=ddnet_mastersrv,
		repo_dir=repo_dir,
		dir=args.builddir,
		valgrind_memcheck=args.valgrind_memcheck,
		keep_tmpdirs=args.keep_tmpdirs,
		timeout_multiplier=args.timeout_multiplier,
	).run_tests(tests)

if __name__ == "__main__":
	sys.exit(main())
