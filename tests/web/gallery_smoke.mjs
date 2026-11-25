// Smoke test for the web gallery in headless Chrome (WebGPU; CI uses
// SwiftShader). Fails if the page:
//   - requests a dynamic library (.so: a SHARED library became a side module
//     instead of being linked into the .wasm) or any file that fails to load,
//   - throws or aborts,
//   - doesn't play the Video tab's sample through the zero-copy path
//     (Module.campelloGpu.importExternalImage), or doesn't rewind at the end.
//
//   node tests/web/gallery_smoke.mjs <dir with campello_widgets_gallery.html>
//
// Chrome: CHROME_PATH (default: macOS Google Chrome), extra flags in
// CHROME_FLAGS. Needs Node 22+ (global WebSocket). Clicks assume the
// gallery's 1280x800 layout (sidebar "Video" item, then the Play button).
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { existsSync, mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { extname, join, resolve } from 'node:path';

const root = resolve(process.argv[2] ?? '.');
const page = 'campello_widgets_gallery.html';
if (!existsSync(join(root, page))) { console.error(`no ${page} in ${root}`); process.exit(2); }

const failures = [];
const fail = (m) => { console.log(`FAIL: ${m}`); failures.push(m); };

const mime = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.mp4': 'video/mp4' };
const server = createServer((req, res) => {
  const p = join(root, decodeURIComponent(new URL(req.url, 'http://x').pathname));
  if (!p.startsWith(root) || !existsSync(p)) { res.writeHead(404); res.end(); return; }
  const data = readFileSync(p);
  const type = mime[extname(p)] ?? 'application/octet-stream';
  const range = req.headers.range && /bytes=(\d+)-(\d*)/.exec(req.headers.range);
  if (range) {   // <video> seeks with range requests
    const start = +range[1], end = range[2] ? +range[2] : data.length - 1;
    res.writeHead(206, { 'Content-Type': type, 'Accept-Ranges': 'bytes',
      'Content-Range': `bytes ${start}-${end}/${data.length}`, 'Content-Length': end - start + 1 });
    res.end(data.subarray(start, end + 1));
  } else {
    res.writeHead(200, { 'Content-Type': type, 'Accept-Ranges': 'bytes', 'Content-Length': data.length });
    res.end(data);
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const url = `http://127.0.0.1:${server.address().port}/${page}`;

const chromePath = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const extra = (process.env.CHROME_FLAGS ?? '').split(/\s+/).filter(Boolean);
const chrome = spawn(chromePath, [
  '--headless=new', `--user-data-dir=${mkdtempSync(join(tmpdir(), 'cw-smoke-'))}`, '--no-first-run',
  '--enable-unsafe-webgpu', '--autoplay-policy=no-user-gesture-required', '--remote-debugging-port=0',
  ...extra, 'about:blank',
], { stdio: ['ignore', 'ignore', 'pipe'] });
const killAll = () => { chrome.kill(); server.close(); };
const watchdog = setTimeout(() => { fail('timed out'); killAll(); process.exit(1); }, 120_000);

const wsUrl = await new Promise((r) => {
  let b = '';
  chrome.stderr.on('data', (c) => { b += c; const m = b.match(/DevTools listening on (ws:\S+)/); if (m) r(m[1]); });
});
const browser = new WebSocket(wsUrl);
await new Promise((r) => { browser.onopen = r; });
let nextId = 1; const pending = new Map();
const send = (method, params = {}, sessionId) => new Promise((r) => {
  const id = nextId++; pending.set(id, r); browser.send(JSON.stringify({ id, method, params, sessionId }));
});
browser.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.exceptionThrown')
    fail(`exception: ${m.params.exceptionDetails.exception?.description ?? m.params.exceptionDetails.text}`);
  if (m.method === 'Runtime.consoleAPICalled') {
    const text = m.params.args.map((a) => a.value ?? a.description ?? '').join(' ');
    console.log(`[console] ${text}`);
    if (/Aborted\(|RuntimeError/.test(text)) fail(`console: ${text}`);
  }
  if (m.method === 'Network.requestWillBeSent' && /\.so(\?|$)/.test(m.params.request.url))
    fail(`requested a dynamic library: ${m.params.request.url}`);
  if (m.method === 'Network.responseReceived' && m.params.response.status >= 400 &&
      !m.params.response.url.endsWith('/favicon.ico'))   // Chrome asks on its own
    fail(`HTTP ${m.params.response.status}: ${m.params.response.url}`);
  if (m.method === 'Network.loadingFailed' && !m.params.canceled)
    fail(`failed to load (${m.params.errorText}): request ${m.params.requestId}`);
};

const { result: { targetId } } = await send('Target.createTarget', { url: 'about:blank' });
const s = (await send('Target.attachToTarget', { targetId, flatten: true })).result.sessionId;
await send('Runtime.enable', {}, s);
await send('Network.enable', {}, s);
await send('Page.enable', {}, s);
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 800, deviceScaleFactor: 1, mobile: false }, s);
await send('Page.navigate', { url }, s);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const evaluate = async (expression) =>
  (await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }, s)).result?.result?.value;
const click = async (x, y) => {
  for (const type of ['mousePressed', 'mouseReleased'])
    await send('Input.dispatchMouseEvent', { type, x, y, button: 'left', clickCount: 1 }, s);
};
async function until(what, expression, timeoutMs) {
  for (const end = Date.now() + timeoutMs; Date.now() < end; await sleep(250))
    if (await evaluate(expression)) return true;
  fail(`${what} (waited ${timeoutMs} ms)`);
  return false;
}

const video = `document.querySelector('video')`;
if (await until('campello_gpu never registered its JavaScript side',
                `!!(globalThis.Module && Module.campelloGpu && Module.campelloGpu.importExternalImage)`, 30_000)) {
  // Count frames handed to the GPU (the zero-copy path).
  await evaluate(`(() => {
    const gpu = Module.campelloGpu, real = gpu.importExternalImage;
    globalThis.__cwImports = 0;
    gpu.importExternalImage = (...a) => { ++globalThis.__cwImports; return real.apply(gpu, a); };
  })()`);
  await sleep(2000);   // first frames
  await click(60, 627);   // sidebar: Video
  if (await until('the Video tab made no <video>, or it never loaded', `!!${video} && ${video}.readyState >= 2`, 20_000)) {
    await click(260, 731);   // Play
    if (await until('the video did not play', `!${video}.paused && ${video}.currentTime > 1`, 15_000)) {
      const imports = await evaluate('globalThis.__cwImports');
      console.log(`frames imported to the GPU: ${imports}`);
      if (!(imports > 5)) fail(`only ${imports} frames went through importExternalImage`);
      await until('the video did not rewind after it ended',
                  `${video}.paused && ${video}.currentTime === 0 && globalThis.__cwImports > 0`, 15_000);
    }
  }
}

clearTimeout(watchdog);
killAll();
console.log(failures.length ? `${failures.length} failure(s)` : 'gallery smoke test passed');
process.exit(failures.length ? 1 : 0);
