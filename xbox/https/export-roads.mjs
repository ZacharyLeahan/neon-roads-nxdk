import { rolldown } from 'rolldown';
import { mkdir, writeFile } from 'node:fs/promises';
async function loadModule(input) {
  const bundle = await rolldown({input, platform:'node'});
  const {output} = await bundle.generate({format:'esm'});
  await bundle.close();
  return import('data:text/javascript;base64,' + Buffer.from(output[0].code).toString('base64'));
}
const {WORLDS,getRoad} = await loadModule('src/levels/worlds.ts');
const {THEMES} = await loadModule('src/audio/audio.ts');
const arrays = [], roads = [];
for (const [w, world] of WORLDS.entries()) {
  for (const [r] of world.roads.entries()) {
    const road = getRoad(w, r);
    if(w===0 && r===0){
      // Full-width warm-up with optional outside-lane gaps and a clear center route.
      const length=360;
      const practice=[];
      for(let row=0;row<length;row++){
        const cells=[3,1,1,1,1,1,3];
        if(row%24===0)cells.fill(3);
        if(row>=80 && row%90<3){
          const lane=Math.floor(row/90)%2===0?1:5;
          cells[lane]=0;
        }
        practice.push(cells.join(','));
      }
      arrays.push('static const unsigned short practice_cells[] = {\n'+practice.join(',\n')+'\n};');
    }
    const symbol = 'cells_' + w + '_' + r;
    const rows = [];
    for (let i = 0; i < road.cells.length; i += 70) rows.push(Array.from(road.cells.slice(i, i + 70)).join(','));
    arrays.push('static const unsigned short ' + symbol + '[] = {\n' + rows.join(',\n') + '\n};');
    const p = world.palette;
    const colors = [...p.floor,p.block,p.blockAlt,p.tunnel,p.edge,world.sky.top,world.sky.horizon,world.sky.featureColorA].map(s=>s.replace('#','0x'));
    if(w===0 && r===0)
      roads.push('{"Board Practice","Easy Cruise",360,500,2000,180,'+world.music+',practice_cells,{'+colors.join(',')+'},1}');
    roads.push('{' + [JSON.stringify(world.name),JSON.stringify(road.name),road.length,road.gravity,road.fuel,road.oxygen,world.music,symbol,'{'+colors.join(',')+'}'].join(',') + '}');
  }
}
await mkdir('xbox/generated',{recursive:true});
await writeFile('xbox/generated/roads.h', arrays.join('\n') + '\nstatic const Road roads[] = {\n' + roads.join(',\n') + '\n};\n');
console.log('Exported', roads.length, 'campaign roads');
const themes = THEMES.map(t => '{' + [t.bpm,t.root,t.arpStyle,'{'+t.scale+'}','{'+t.prog+'}'].join(',') + '}');
await writeFile('xbox/generated/themes.h', 'static const Theme themes[] = {' + themes.join(',\n') + '};\n');
