// Replays a recorded scenario against the emscripten client and verifies it.
//
//   node run-scenario.js scenarios/menu-settings.json [--url http://localhost:8000] [--update-golden]
//
// Scenario format: { "name": ..., "args": "console cmds", "steps": [...] }.
// Steps are the shared step format from lib.js; "shot" steps save a screenshot
// to out/<name>/ and compare it against goldens/<name>/ when a golden exists.
// A shot step can set "maxDiffPct" (default 1.0) or "compare": false for
// volatile content (e.g. ingame views with timers). Exits non-zero when any
// step fails or any comparison mismatches.
const fs = require('fs');
const path = require('path');
const { bootClient, executeStep, compareShot } = require('./lib');

const argv = process.argv.slice(2);
const scenarioPath = argv.find(a => !a.startsWith('--'));
const urlArg = argv.indexOf('--url');
const url = urlArg >= 0 ? argv[urlArg + 1] : 'http://localhost:8000';
const updateGolden = argv.includes('--update-golden');

if (!scenarioPath) {
	console.error('usage: node run-scenario.js <scenario.json> [--url <url>] [--update-golden]');
	process.exit(2);
}
const scenario = JSON.parse(fs.readFileSync(scenarioPath, 'utf8'));
const outDir = path.join(__dirname, 'out', scenario.name);
const goldenDir = path.join(__dirname, 'goldens', scenario.name);
fs.mkdirSync(outDir, { recursive: true });

(async () => {
	const failures = [];
	const shots = [];
	console.log(`scenario '${scenario.name}': booting client at ${url}`);
	const { browser, page, logs } = await bootClient({ url, args: scenario.args });

	for (const [i, step] of scenario.steps.entries()) {
		const label = `step ${i + 1}/${scenario.steps.length} ${JSON.stringify(step)}`;
		try {
			if (step.action === 'shot') {
				await page.waitForTimeout(250);
				const actualPath = path.join(outDir, `${step.name}.png`);
				await page.screenshot({ path: actualPath });
				const goldenPath = path.join(goldenDir, `${step.name}.png`);
				if (updateGolden) {
					fs.mkdirSync(goldenDir, { recursive: true });
					fs.copyFileSync(actualPath, goldenPath);
					shots.push({ name: step.name, status: 'golden updated' });
					console.log(`${label} -> golden updated`);
				} else if (step.compare !== false && fs.existsSync(goldenPath)) {
					const result = compareShot(actualPath, goldenPath, step.maxDiffPct ?? 1.0);
					if (!result.match) {
						// Copy the golden next to the actual so the uploaded
						// artifact is self-contained for comparison
						fs.copyFileSync(goldenPath, path.join(outDir, `${step.name}.golden.png`));
						shots.push({ name: step.name, status: 'MISMATCH', diffPct: result.diffPct, mismatch: true });
						failures.push(`${step.name}: ${result.reason}`);
						console.log(`${label} -> MISMATCH: ${result.reason}`);
					} else {
						shots.push({ name: step.name, status: 'ok', diffPct: result.diffPct });
						console.log(`${label} -> ok (${result.diffPct.toFixed(2)}% diff)`);
					}
				} else {
					shots.push({ name: step.name, status: 'no golden' });
					console.log(`${label} -> saved (no golden comparison)`);
				}
			} else {
				const cont = await executeStep(page, logs, step);
				console.log(`${label} -> ok`);
				if (!cont) break;
			}
		} catch (err) {
			failures.push(`step ${i + 1} (${step.action}): ${err.message}`);
			console.log(`${label} -> FAILED: ${err.message}`);
			await page.screenshot({ path: path.join(outDir, `failure-step-${i + 1}.png`) }).catch(() => {});
			break; // later steps depend on earlier ones
		}
	}

	fs.writeFileSync(path.join(outDir, 'console.log'), logs.join('\n'));
	writeReport(shots, failures);
	await browser.close();
	if (failures.length > 0) {
		console.error(`\nscenario '${scenario.name}' FAILED:\n- ${failures.join('\n- ')}`);
		process.exit(1);
	}
	console.log(`\nscenario '${scenario.name}' passed`);
})().catch(err => {
	console.error('FAILED:', err.message);
	process.exit(1);
});

// Writes a side-by-side HTML report into the output directory (part of the CI
// artifact) and, when running in GitHub Actions, a summary table for the job.
function writeReport(shots, failures) {
	const esc = s => s.replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
	const rows = shots.map(s => {
		const diff = s.diffPct !== undefined ? `${s.diffPct.toFixed(2)}% diff` : '';
		let imgs = `<figure><figcaption>actual</figcaption><img src="${s.name}.png"></figure>`;
		if (s.mismatch) {
			imgs = `<figure><figcaption>golden</figcaption><img src="${s.name}.golden.png"></figure>` + imgs +
				`<figure><figcaption>diff</figcaption><img src="${s.name}.diff.png"></figure>`;
		}
		return `<h2>${esc(s.name)} — ${esc(s.status)} ${diff}</h2><div class="row">${imgs}</div>`;
	}).join('\n');
	const html = `<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>${esc(scenario.name)} UI test report</title><style>
body { font-family: sans-serif; margin: 20px; }
.row { display: flex; gap: 10px; }
figure { margin: 0; flex: 1; min-width: 0; }
figcaption { font-weight: bold; margin-bottom: 4px; }
img { max-width: 100%; border: 1px solid #ccc; }
h2 { margin: 25px 0 8px 0; }
</style></head><body><h1>${esc(scenario.name)}</h1>
${failures.length > 0 ? `<p><b>${failures.length} failure(s)</b></p>` : '<p>passed</p>'}
${rows}</body></html>`;
	fs.writeFileSync(path.join(outDir, 'report.html'), html);

	if (process.env.GITHUB_STEP_SUMMARY) {
		const lines = [`### UI test: ${scenario.name} — ${failures.length > 0 ? 'FAILED' : 'passed'}`, '', '| shot | status | diff |', '| --- | --- | --- |'];
		for (const s of shots) {
			const icon = s.mismatch ? ':x:' : s.status === 'ok' ? ':white_check_mark:' : '';
			lines.push(`| ${s.name} | ${icon} ${s.status} | ${s.diffPct !== undefined ? s.diffPct.toFixed(2) + '%' : ''} |`);
		}
		for (const f of failures.filter(f => !shots.some(s => f.startsWith(s.name + ':')))) {
			lines.push(`| | :x: ${f} | |`);
		}
		lines.push('', `Download the \`ddnet-ui-test-screenshots\` artifact and open \`${scenario.name}/report.html\` for side-by-side images.`, '');
		fs.appendFileSync(process.env.GITHUB_STEP_SUMMARY, lines.join('\n'));
	}
}
