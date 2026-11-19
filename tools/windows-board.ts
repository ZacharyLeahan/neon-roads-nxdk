import { randomBytes } from 'node:crypto';
import { readFileSync } from 'node:fs';
import type { Connect, Plugin } from 'vite';

// Windows playtest transport. This page retains the interactive Windows calibration experiments.
function boardMiddleware(): Connect.NextHandleFunction {
  type Phase = 'ready' | 'racing' | 'lost' | 'finished';
  type Action = { id: string; seq: number };
  const sessions = new Map<string, {
    host: string; phase: Phase; lastHost: number; lastPhone: number;
    tilt: number; brake: boolean; stage: number; angles: number[]; pose: number[]; peaks: number[]; jumps: number;
    actions: Action[]; lastAction: string; seq: number; exited: boolean;
  }>();
  return (req, res, next) => {
    const path = req.url?.split('?')[0] ?? '';
    if (!path.startsWith('/board/')) return next();
    res.setHeader('Cache-Control', 'no-store');
    function send(code: number, body: unknown) {
      res.statusCode = code;
      res.setHeader('Content-Type', typeof body === 'string' ? 'text/plain' : 'application/json');
      res.end(typeof body === 'string' ? body : JSON.stringify(body));
    }
    if (req.method === 'GET' && path === '/board/controller.html') {
      const page = readFileSync('tools/board-lab-controller.html', 'utf8');
      res.setHeader('Content-Type', 'text/html; charset=utf-8');
      return res.end(page.replaceAll('Connected to Xbox','Connected to Windows').replace('watch the TV','watch the Windows screen').replace('Exit to dashboard', 'Disconnect').replace('Leave this game and return the Xbox to its dashboard?', 'Disconnect from this Windows game?').replace('Returning to dashboard.', 'Disconnected.'));
    }
    if (req.method !== 'POST') return send(405, 'POST required');
    if (path === '/board/session') {
      if(req.headers['x-board-host'] !== 'create') return send(403,'Invalid session request');
      for(const [key,s] of sessions) if(Date.now()-s.lastHost>120000) sessions.delete(key);
      if(sessions.size>=32) return send(429,'Too many board sessions');
      const pair=randomBytes(16).toString('hex'), host=randomBytes(16).toString('hex');
      sessions.set(pair,{host,phase:'ready',lastHost:Date.now(),lastPhone:0,tilt:0,brake:false,stage:0,
        angles:[],pose:[],peaks:[],jumps:0,actions:[],seq:0,lastAction:'',exited:false});
      return send(200,{pair,host});
    }
    const session = sessions.get(String(req.headers['x-pair']));
    if(!session || Date.now()-session.lastHost>120000) return send(403,'Scan the current code');
    if (path === '/board/read') {
      if(req.headers['x-board-host']!==session.host) return send(403,'Invalid host');
      const phase=req.headers['x-phase'];
      if(phase==='racing'||phase==='ready'||phase==='finished'||phase==='lost') session.phase=phase;
      session.lastHost=Date.now();
      const ack=Number(req.headers['x-ack']??0);
      session.actions=session.actions.filter(a=>a.seq>ack);
      const {host: _host,...state}=session;
      return send(200,state);
    }
    if (path === '/board/dashboard') {session.exited=true;return send(202,'Disconnected');}
    const match=/^\/board\/tilt\/(-?\d{1,2})$/.exec(path);
    if(!match || Math.abs(Number(match[1]))>90) return send(404,'Unknown input');
    const values=(key:string,n:number)=>String(req.headers[key]??'').split(',').slice(0,n).map(x=>Math.max(-180,Math.min(180,Number(x)/10||0)));
    session.exited=false;
    session.lastPhone=Date.now();session.tilt=Number(match[1]);session.brake=req.headers['x-brake']==='1';
    session.stage=Math.max(0,Math.min(9,Number(req.headers['x-setup'])||0));
    session.angles=values('x-angles',5);session.pose=values('x-pose',2);
    if(session.stage===1) session.peaks=[];
    if(req.headers['x-peaks']) values('x-peaks',6).forEach((v,i)=>{if(v>0)session.peaks[i]=v;});
    const id=String(req.headers['x-action']??'');
    if(/^[pj]\d{1,12}$/.test(id) && id!==session.lastAction){
      if(id.startsWith('j'))session.jumps++;
      session.lastAction=id;session.actions.push({id,seq:++session.seq});
      if(session.actions.length>16)session.actions.shift();
    }
    send(200,session.phase);
  };
}
export function windowsBoard(): Plugin {
  return { name:'windows-board', configureServer(server){server.middlewares.use(boardMiddleware());},
    configurePreviewServer(server){server.middlewares.use(boardMiddleware());} };
}
