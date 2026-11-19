import './workbench.css';
import qrCode from 'qrcode-generator';
import type { Input } from '../core/input';

type Phase = 'ready' | 'racing' | 'lost' | 'finished';
type State = {
  stage: number; pose: number[]; angles: number[]; peaks: number[]; jumps: number;
  lastPhone: number; tilt: number; brake: boolean; exited: boolean;
  actions: { seq: number; id: string }[];
};
type Game = { phase(): Phase; play(): void; bind(read: (out: Input['remote']) => void): void };
const titles = ['Ready to ride', 'Rest the board flat', 'Lean left · 1 of 2', 'Lean left · 2 of 2',
  'Lean right · 1 of 2', 'Lean right · 2 of 2', 'Kick up · 1 of 2', 'Kick up · 2 of 2',
  'Teach your ready gesture', 'Try your ready gesture'];
const degree = (n: number) => Number(n.toFixed(1)) + '°';

function meter(name: string) {
  return '<section class="board-meter"><label>' + name + '</label><output>0°</output><div class="board-track"><b></b><i></i><i></i><em></em></div><small></small></section>';
}
function updateMeter(el: HTMLElement, value: number, targets: number[], signed: boolean, caption: string) {
  // Fixed zoom steps keep the ruler still as the rider approaches a target.
  const span = [10, 20, 40, 90, 180].find(v => v >= Math.max(Math.abs(value), ...targets.map(Math.abs))) ?? 180;
  const position = (v: number) => (signed ? 50 + 50 * v / span : 100 * v / span);
  const pos = position(value), center = position(0);
  const track = el.querySelector<HTMLElement>('.board-track')!;
  track.style.setProperty('--origin', center + '%');
  track.style.setProperty('--start', Math.min(center, pos) + '%');
  track.style.setProperty('--width', Math.abs(center - pos) + '%');
  track.style.setProperty('--needle', pos + '%');
  el.querySelector('output')!.textContent = degree(value);
  el.querySelector('small')!.textContent = caption;
  el.querySelectorAll('i').forEach((mark, i) => {
    mark.hidden = !targets[i];
    if (!mark.hidden) mark.style.left = position(targets[i]) + '%';
  });
}

export async function openWorkbench(game: Game) {
  const panel = document.createElement('aside');
  panel.id = 'board-lab';
  panel.innerHTML = '<article><header>NEON ROADS / BOARD LAB</header><h1>Pair your board</h1><p class="cue">Scan on your iPhone, then tap Enable tilt.</p><a class="board-pair" aria-label="Open phone controller"></a><div class="meters" hidden>' +
    meter('Steering') + meter('Kick height') + '</div><p class="reps"></p><p class="connection" role="status"></p><button class="restart-board">Restart calibration</button></article>';
  document.body.append(panel);
  panel.querySelector<HTMLButtonElement>('.restart-board')!.onclick = () => {
    history.replaceState(null, '', '?board&fresh=1');
    location.reload();
  };
  const link = panel.querySelector<HTMLAnchorElement>('a')!;
  const title = panel.querySelector('h1')!, cue = panel.querySelector('.cue')!;
  const status = panel.querySelector('.connection')!, reps = panel.querySelector('.reps')!;
  const meters = panel.querySelector<HTMLElement>('.meters')!;
  const [steer, kick] = meters.querySelectorAll<HTMLElement>('section');
  let state: State | undefined, ack = 0, jump = false, flashUntil = 0;
  game.bind(out => {
    out.active = !!state && !state.exited && Date.now() - state.lastPhone < 1200 && state.stage === 0;
    out.steer = out.active ? state!.tilt / 28 : 0;
    out.throttle = out.active ? (state!.brake ? -1 : 1) : 0;
    out.jump = out.active && jump;
    jump = false;
  });

  try {
    const fragment = new URLSearchParams(location.hash.slice(1));
    let pair = fragment.get('pair'), host = fragment.get('host');
    if (!pair || !host) {
      const response = await fetch('/board/session', {method:'POST', headers:{'X-Board-Host':'create'}});
      if (!response.ok) throw Error('Could not open a calibration session.');
      ({pair, host} = await response.json());
      history.replaceState(null, '', '#pair=' + pair + '&host=' + host);
    }
    const fresh = new URLSearchParams(location.search).has('fresh');
    if (fresh) {
      title.textContent = 'Start a fresh calibration';
      cue.textContent = 'Scan this code. Rest the phone flat on the board, then tap Start calibration.';
    }
    link.href = new URL('/board/controller.html' + (fresh ? '?fresh=1' : '') + '#' + pair, location.origin).href;
    const qr = qrCode(0,'M');
    qr.addData(link.href); qr.make();
    link.innerHTML = qr.createSvgTag({scalable:true, margin:16});
    async function poll() {
      try {
        const response = await fetch('/board/read', {method:'POST', headers:{
          'X-Pair':pair!, 'X-Board-Host':host!, 'X-Phase':game.phase(), 'X-Ack':String(ack)
        }});
        if (response.status === 403) { status.innerHTML = 'Session expired. <a href="?board">Start a new session</a>'; return; }
        if (!response.ok) throw Error();
        const data: State = await response.json();
        state = data;
        for (const action of data.actions) {
          if (action.seq <= ack) continue;
          ack = action.seq;
          if (action.id.startsWith('p')) game.play();
          else { jump = true; flashUntil = Date.now() + 450; }
        }
        if (data.lastPhone) {
          link.hidden = !data.exited;
          meters.hidden = data.exited;
          const phase = game.phase(), riding = phase === 'racing' && data.stage === 0;
          panel.classList.toggle('riding', riding);
          title.textContent = data.exited ? 'Pair your board' : riding ? 'On the road' : phase === 'lost' ? 'Ready for another run?' : titles[data.stage];
          cue.textContent = data.exited ? 'Scan to reconnect.' : data.stage === 1 ? 'Phone screen-up, top toward the nose. Hold still for three seconds.' :
            data.stage >= 2 && data.stage <= 7 ? 'Use a comfortable motion, then ease off a little to count it.' :
            data.stage === 8 ? 'Pause briefly, then make the movement you want to use to start.' :
            riding ? '' : 'Pause for two seconds, then repeat your ready gesture to play.';
          const [roll = 0, pitch = 0] = data.pose;
          const [,peak = 0,left = 0,right = 0,trigger = 0] = data.angles;
          const axis = Math.sign(right) || -Math.sign(left) || 1;
          const steerTargets = [-Math.abs(left)*.8, Math.abs(right)*.8];
          updateMeter(steer, roll * axis, steerTargets, true, left || right ?
            'Full steer   L ' + degree(Math.abs(left)*.8) + '   /   R ' + degree(Math.abs(right)*.8) : 'Level at center');
          const provisional = Math.max(...data.peaks.slice(4), data.stage === 6 || data.stage === 7 ? peak : 0) * .8;
          const threshold = trigger || provisional;
          updateMeter(kick, Math.abs(pitch), [threshold], false, Date.now() < flashUntil ? 'Jump registered' :
            threshold ? (trigger ? 'Jump at ' : 'Practice target ') + degree(threshold) : 'Lift either end of the board');
          if (data.stage === 6 || data.stage === 7) {
            if (peak > 0 && (data.stage === 6 || Math.abs(pitch) >= peak)) {
              cue.textContent = 'Lift detected. Ease the board down to count this kick.';
            } else {
              cue.textContent = 'Lift one end, then ease it down. Watch the kick-height marker.';
            }
          }
          kick.classList.toggle('fired', Date.now() < flashUntil);
          reps.textContent = ['Left','Right','Kick'].map((label,i) => label + '  ' +
            [0,1].map(j => data.peaks[2*i+j] ? degree(data.peaks[2*i+j]) : '–').join(' / ')).join('     ·     ');
          if (!data.exited && Date.now() - data.lastPhone > 3000 && !riding) {
            title.textContent = 'Phone readings paused';
            cue.textContent = 'Bring the controller tab back to the foreground on your phone. Your measurements are here.';
          }
          status.textContent = data.exited ? 'Phone disconnected' : Date.now()-data.lastPhone>2500 ? 'Waiting for phone motion' :
            data.stage===0 ? data.jumps+' jump inputs' : '';
        }
      } catch { status.textContent = 'Reconnecting to Windows…'; }
      window.setTimeout(poll, 65);
    }
    void poll();
  } catch (err) { status.textContent = String(err); }
}
