import { createWriteStream, readFileSync } from 'node:fs';
import { once } from 'node:events';
import { setTimeout as delay } from 'node:timers/promises';

// Observe an existing host session without consuming actions or changing game phase.
const [sessionFile, outputFile] = process.argv.slice(2);
if (!sessionFile || !outputFile) throw Error('Usage: node tools/record-board.mjs session.json readings.ndjson');
const { pair, host } = JSON.parse(readFileSync(sessionFile, 'utf8').replace(/^\uFEFF/, ''));
const log = createWriteStream(outputFile, { flags: 'a' });
let lastTime = 0;
const deadline = Date.now() + 20 * 60_000;
while (Date.now() < deadline) {
  const response = await fetch('http://127.0.0.1:5287/board/read', {
    method: 'POST', headers: { 'X-Pair': pair, 'X-Board-Host': host },
    signal: AbortSignal.timeout(3000),
  }).catch(() => null);
  if (response?.status === 403) break;
  if (response?.ok) {
    const {lastPhone, stage, pose, angles, peaks, jumps, exited} = await response.json();
    if (exited) break;
    if (lastPhone && lastTime !== lastPhone) {
      lastTime = lastPhone;
      const row = {time:lastPhone, stage, roll:pose[0], pitch:pose[1], peak:angles[1], target:angles[4], peaks, jumps};
      if (!log.write(JSON.stringify(row) + '\n')) await once(log, 'drain');
    }
  }
  await delay(Date.now() - lastTime > 3000 ? 250 : 50);
}
log.end();
