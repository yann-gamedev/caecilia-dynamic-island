'use strict';
// Requires a local Chrome CDP endpoint; screenshots retain the prototype's original fonts/artwork.
const fs = require('node:fs');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const prototype = process.argv[2];
if (!prototype) {
  console.error('Usage: node tools/capture-m1-reference.cjs <external-prototype.html>');
  process.exit(1);
}
let html = fs.readFileSync(prototype, 'utf8');
const out = path.resolve('artifacts/m1-validation/reference');
fs.mkdirSync(out, {recursive:true});
html = html.replace("useState(true);\n  const [inspect", "useState(false);\n  const [inspect")
  .replace('const [inspect, setInspect] = useState(true)', 'const [inspect, setInspect] = useState(false)')
  .replace('useState(() => (window.innerWidth >= 1180 ? 2 : 1))','useState(1)')
  .replace('  // clocks & simulated data', '  window.reference = (base, tab, hud) => {setBase(base);setTab(tab);setHudMode(hud);setFlash(false);};\n  // clocks & simulated data')
  .replace('<script src="https://cdn.tailwindcss.com/3.4.17">', '<script>window.setInterval=()=>0;</script><script src="https://cdn.tailwindcss.com/3.4.17">');
const local = path.join(out,'reference.html'); fs.writeFileSync(local,html);
(async()=>{
 const target = await (await fetch('http://127.0.0.1:9337/json/new?about:blank',{method:'PUT'})).json();
 const ws = new WebSocket(target.webSocketDebuggerUrl);
 await new Promise(r=>ws.addEventListener('open',r,{once:true}));
 let id=0; const pending=new Map();
 ws.onmessage=e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);m.error?p.reject(m.error):p.resolve(m.result);}};
 const call=(method,params={})=>new Promise((resolve,reject)=>{pending.set(++id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
 const wait=ms=>new Promise(r=>setTimeout(r,ms));
 try {
 await call('Page.enable');
 await call('Emulation.setDeviceMetricsOverride',{width:1400,height:1000,deviceScaleFactor:1,mobile:false});
 await call('Page.navigate',{url:pathToFileURL(local).href});
 for(let i=0;i<60 && !await evaluate('!!window.reference');i++)await wait(500);
 if(!await evaluate('!!window.reference'))throw Error('Prototype CDN scripts did not load');
 await evaluate(`(()=>{let style=document.createElement('style');style.textContent='.screen {background:white!important} .fade{animation:none!important}';document.head.appendChild(style);})()`);
 const cases=[['dormant','idle',0,'volume'],['compact','compact',0,'volume'],['expanded-now','expanded',0,'volume'],['expanded-telemetry','expanded',1,'volume'],['expanded-tray','expanded',2,'volume'],['hud-volume','hud',0,'volume'],['hud-brightness','hud',0,'brightness'],['hud-pomodoro','hud',0,'pomodoro']];
 for(const scale of [1,1.5]){
 await call('Emulation.setDeviceMetricsOverride',{width:1400,height:1000,deviceScaleFactor:scale,mobile:false});
 const folder=path.join(out,`${scale*96}dpi`);fs.mkdirSync(folder,{recursive:true});
 for(const [name,base,tab,hud]of cases){
 await evaluate(`window.reference(${JSON.stringify(base)},${tab},${JSON.stringify(hud)})`);await wait(1100);
 const clip=await evaluate(`(()=>{const r=document.querySelector('.isl').getBoundingClientRect();return {x:r.x+r.width/2-246,y:r.y,width:492,height:193,scale:1}})()`);
 const shot=await call('Page.captureScreenshot',{format:'png',clip,captureBeyondViewport:true});
 fs.writeFileSync(path.join(folder,name+'.png'),Buffer.from(shot.data,'base64'));
 }
 }
 console.log('Captured original HTML prototype (frozen data, white comparison backdrop, bounds off) at 96/144 DPI.');
 }finally{await call('Page.close');ws.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
