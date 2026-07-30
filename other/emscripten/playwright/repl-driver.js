// Interactive driver / scenario recorder for the DDNet emscripten client.
//
//   node repl-driver.js [--url http://localhost:8000]
//
// Boots the client to the main menu, then executes JSON commands appended to
// cmds.jsonl (one per line, shared step format from lib.js, plus an "id"):
//
//   {"id":"01","action":"click","x":1196,"y":26}
//   {"id":"02","action":"console","cmd":"echo hi"}
//   {"id":"03","action":"shot","name":"settings"}
//   {"id":"99","action":"quit"}
//
// After each command it saves shots/<id>.png and shots/<id>.done.json, so a
// human or agent can look at the result before choosing the next command.
// Every successfully executed command is appended to record.jsonl (without
// the id) — curate that file into scenarios/<name>.json for run-scenario.js.
const fs = require('fs');
const path = require('path');
const { bootClient, executeStep } = require('./lib');

const argv = process.argv.slice(2);
const urlArg = argv.indexOf('--url');
const url = urlArg >= 0 ? argv[urlArg + 1] : 'http://localhost:8000';

const shotsDir = path.join(__dirname, 'shots');
const cmdFile = path.join(__dirname, 'cmds.jsonl');
const recordFile = path.join(__dirname, 'record.jsonl');
const logFile = path.join(__dirname, 'driver-console.log');
fs.mkdirSync(shotsDir, { recursive: true });
fs.writeFileSync(cmdFile, '');
fs.writeFileSync(recordFile, '');
fs.writeFileSync(logFile, '');

(async () => {
	const { browser, page, logs } = await bootClient({
		url,
		args: 'cl_show_welcome 0; player_name Playwright',
		onLog: line => fs.appendFileSync(logFile, line + '\n'),
	});
	await page.screenshot({ path: path.join(shotsDir, 'boot.png') });
	console.log('READY - client at main menu, polling cmds.jsonl');

	let offset = 0;
	let running = true;
	while (running) {
		const content = fs.readFileSync(cmdFile, 'utf8');
		const fresh = content.slice(offset);
		const nl = fresh.lastIndexOf('\n');
		if (nl >= 0) {
			const lines = fresh.slice(0, nl).split('\n').filter(l => l.trim());
			offset += nl + 1;
			for (const line of lines) {
				const cmd = JSON.parse(line);
				let result = { ok: true };
				try {
					running = await executeStep(page, logs, cmd);
					const { id, ...step } = cmd;
					fs.appendFileSync(recordFile, JSON.stringify(step) + '\n');
				} catch (err) {
					result = { ok: false, error: err.message };
				}
				await page.waitForTimeout(250);
				await page.screenshot({ path: path.join(shotsDir, `${cmd.id}.png`) }).catch(() => {});
				fs.writeFileSync(path.join(shotsDir, `${cmd.id}.done.json`), JSON.stringify(result));
				if (!running) break;
			}
		}
		await new Promise(r => setTimeout(r, 200));
	}
	console.log('driver exiting');
	await browser.close();
})().catch(err => {
	console.error('DRIVER FAILED:', err.message);
	process.exit(1);
});
