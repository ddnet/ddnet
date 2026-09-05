// Seeds golden screenshots from the ddnet-ui-test-screenshots artifact of a
// CI run, so goldens are rendered by the same environment that compares them.
//
//   node seed-goldens.js <run-id> [repo]      (default repo: ddnet/ddnet)
//
// Only shots that scenarios actually compare are copied (volatile
// "compare": false shots and failure/diff images are skipped). Review and
// commit the resulting goldens/ directory.
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const runId = process.argv[2];
const repo = process.argv[3] || 'ddnet/ddnet';
if (!runId) {
	console.error('usage: node seed-goldens.js <run-id> [repo]');
	process.exit(2);
}

const comparedShots = {};
for (const file of fs.readdirSync(path.join(__dirname, 'scenarios'))) {
	const scenario = JSON.parse(fs.readFileSync(path.join(__dirname, 'scenarios', file), 'utf8'));
	comparedShots[scenario.name] = scenario.steps
		.filter(s => s.action === 'shot' && s.compare !== false)
		.map(s => s.name);
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ddnet-goldens-'));
console.log(`downloading artifact of run ${runId} from ${repo} ...`);
execFileSync('gh', ['run', 'download', runId, '--repo', repo, '--name', 'ddnet-ui-test-screenshots', '--dir', tmp], { stdio: 'inherit' });

let seeded = 0;
for (const [name, shotNames] of Object.entries(comparedShots)) {
	const srcDir = path.join(tmp, name);
	if (!fs.existsSync(srcDir)) {
		console.log(`scenario '${name}': no screenshots in artifact, skipped`);
		continue;
	}
	const goldenDir = path.join(__dirname, 'goldens', name);
	fs.mkdirSync(goldenDir, { recursive: true });
	for (const shotName of shotNames) {
		const src = path.join(srcDir, `${shotName}.png`);
		if (!fs.existsSync(src)) {
			console.log(`scenario '${name}': shot '${shotName}' missing in artifact`);
			continue;
		}
		fs.copyFileSync(src, path.join(goldenDir, `${shotName}.png`));
		seeded++;
	}
}
fs.rmSync(tmp, { recursive: true, force: true });
console.log(`seeded ${seeded} goldens - review and commit the goldens/ directory`);
