// Shared helpers for driving the DDNet emscripten client with Playwright.
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

// The game consumes relative mouse deltas at 2x (ui_mousesens/cl_mousesens
// default to 200 in this build), so Playwright coordinates are half the
// game-screen coordinates. Positioning is made deterministic by first
// clamping the game cursor to (0,0) with one large negative move.
const CURSOR_SCALE = 2;
const VIEWPORT = { width: 1280, height: 720 };

async function bootClient(opts) {
	const browser = await chromium.launch({
		headless: true,
		// SwiftShader software WebGL, needed for headless rendering
		args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
	});
	const page = await browser.newPage({ viewport: VIEWPORT });
	const logs = [];
	page.on('console', msg => { logs.push(msg.text()); if (opts.onLog) opts.onLog(msg.text()); });
	page.on('pageerror', err => { logs.push(`PAGEERROR: ${err.message}`); if (opts.onLog) opts.onLog(`PAGEERROR: ${err.message}`); });

	await page.goto(opts.url, { waitUntil: 'domcontentloaded' });
	await page.waitForSelector('#launch-button:not([disabled])', { timeout: 180000 });
	if (opts.args) {
		// Each entry of Module.arguments is executed as a console command line
		await page.evaluate(a => Module.arguments.push(a), opts.args);
	}
	await page.click('#launch-button');
	// Log-driven readiness instead of a fixed delay, for slow CI runners
	const start = Date.now();
	while (!logs.some(l => l.includes('on emscripten wasm32'))) {
		if (Date.now() - start > 120000) throw new Error('timeout waiting for client to boot');
		await page.waitForTimeout(250);
	}
	await page.waitForTimeout(3000);
	// Fresh browser profiles get the language selection popup; Enter confirms it.
	await page.keyboard.press('Enter');
	await page.waitForTimeout(2000);
	return { browser, page, logs };
}

async function moveTo(page, x, y) {
	await page.mouse.move(VIEWPORT.width - 1, VIEWPORT.height - 1);
	await page.mouse.move(0, 0); // large negative delta clamps the game cursor to (0,0)
	await page.mouse.move(x / CURSOR_SCALE, y / CURSOR_SCALE);
}

// Executes one step; returns false when the step requests termination.
// Steps use game-screen coordinates (1280x720).
async function executeStep(page, logs, step) {
	switch (step.action) {
		case 'key':
			await page.keyboard.press(step.key);
			break;
		case 'keydown':
			await page.keyboard.down(step.key);
			break;
		case 'keyup':
			await page.keyboard.up(step.key);
			break;
		case 'type':
			await page.keyboard.type(step.text, { delay: 25 });
			break;
		case 'move':
			await moveTo(page, step.x, step.y);
			break;
		case 'click':
			await moveTo(page, step.x, step.y);
			await page.waitForTimeout(150);
			await page.mouse.down({ button: step.button || 'left' });
			await page.waitForTimeout(80);
			await page.mouse.up({ button: step.button || 'left' });
			break;
		case 'down':
			await page.mouse.down({ button: step.button || 'left' });
			break;
		case 'up':
			await page.mouse.up({ button: step.button || 'left' });
			break;
		case 'wheel':
			await page.mouse.wheel(0, step.dy);
			break;
		case 'console':
			// Only reliable for commands that do not trigger a map load; the
			// trailing toggle races with loading otherwise (use 'connect' then).
			await page.keyboard.press('F1');
			await page.waitForTimeout(300);
			await page.keyboard.type(step.cmd, { delay: 20 });
			await page.keyboard.press('Enter');
			await page.waitForTimeout(300);
			await page.keyboard.press('F1');
			break;
		case 'connect': {
			// Connects via the console, leaving the console open during map
			// loading (a close pressed during loading is swallowed; one pressed
			// before it re-opens later). Close only after the first ping
			// round-trip confirms the game is fully up.
			await page.keyboard.press('F1');
			await page.waitForTimeout(300);
			await page.keyboard.type(`connect ${step.address}`, { delay: 20 });
			await page.keyboard.press('Enter');
			const start = Date.now();
			while (!logs.some(l => l.includes('got pong from current server'))) {
				if (Date.now() - start > (step.timeout || 60000)) {
					throw new Error(`timeout connecting to ${step.address}`);
				}
				await page.waitForTimeout(200);
			}
			await page.waitForTimeout(1000);
			await page.keyboard.press('F1');
			await page.waitForTimeout(500);
			break;
		}
		case 'wait':
			await page.waitForTimeout(step.ms);
			break;
		case 'waitlog': {
			const start = Date.now();
			while (!logs.some(l => l.includes(step.pattern))) {
				if (Date.now() - start > (step.timeout || 30000)) {
					throw new Error(`timeout waiting for log: ${step.pattern}`);
				}
				await page.waitForTimeout(200);
			}
			break;
		}
		case 'shot':
			break; // handled by the caller
		case 'quit':
			return false;
		default:
			throw new Error(`unknown action: ${step.action}`);
	}
	return true;
}

// Compares a screenshot against a golden image. Returns {match, reason, diffPct}
// and writes a visual diff next to the actual image on mismatch.
function compareShot(actualPath, goldenPath, maxDiffPct) {
	const { PNG } = require('pngjs');
	const pixelmatch = require('pixelmatch');
	const actual = PNG.sync.read(fs.readFileSync(actualPath));
	const golden = PNG.sync.read(fs.readFileSync(goldenPath));
	if (actual.width !== golden.width || actual.height !== golden.height) {
		return { match: false, reason: 'size mismatch', diffPct: 100 };
	}
	const diff = new PNG({ width: actual.width, height: actual.height });
	const numDiff = pixelmatch(actual.data, golden.data, diff.data, actual.width, actual.height, { threshold: 0.1 });
	const diffPct = (numDiff / (actual.width * actual.height)) * 100;
	if (diffPct > maxDiffPct) {
		const diffPath = actualPath.replace(/\.png$/, '.diff.png');
		fs.writeFileSync(diffPath, PNG.sync.write(diff));
		return { match: false, reason: `${diffPct.toFixed(2)}% pixels differ (max ${maxDiffPct}%)`, diffPct };
	}
	return { match: true, diffPct };
}

module.exports = { bootClient, executeStep, moveTo, compareShot, CURSOR_SCALE, VIEWPORT };
