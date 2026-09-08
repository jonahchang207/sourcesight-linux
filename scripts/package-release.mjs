// Run after a Release build: node scripts/package-release.mjs
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';

const root=process.cwd();
const version='0.6.0';
const name='sourcesight-v'+version+'-linux-x86_64';
const stage=fs.mkdtempSync(path.join(os.tmpdir(),'sourcesight-release-'));
const pkg=path.join(stage,name);
const output=path.join(root,'build','release');
fs.mkdirSync(pkg);fs.mkdirSync(output,{recursive:true});
const copy=(from,to=from)=>{
    const dest=path.join(pkg,to);
    fs.mkdirSync(path.dirname(dest),{recursive:true});
    fs.copyFileSync(path.join(root,from),dest);
};
copy('build/sourcesight','sourcesight');
fs.chmodSync(path.join(pkg,'sourcesight'),0o755);
for(const file of ['README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE_NOTES_v0.6.0.md',
    'assets/brand/banner.svg','assets/brand/logo.svg','assets/brand/menu.png'])
    copy(file);
for(const [from,to] of [
    ['src/external/imgui/LICENSE.txt','imgui.txt'],
    ['src/external/json/LICENSE.MIT','json.txt'],
    ['src/external/AsyncLogger/LICENSE','AsyncLogger.txt'],
    ['src/external/VisCheckCS2/LICENSE','VisCheckCS2.txt'],
    ['src/external/curl/COPYING','curl.txt'],
    ['assets/README.md','asset-attribution.md']
]) copy(from,'licenses/'+to);
const revision=execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim();
fs.writeFileSync(path.join(pkg,'BUILD.txt'),
    'SourceSight '+version+'\nSource revision: '+revision+'\nPlatform: Arch Linux x86-64\n'+
    'Dynamically linked; requires glibc 2.43+, compatible libstdc++, GLFW, curl, X11 and OpenGL.\n');
const archive=path.join(output,name+'.tar.gz');
execFileSync('tar',['-czf',archive,'-C',stage,name]);
const digest=crypto.createHash('sha256').update(fs.readFileSync(archive)).digest('hex');
fs.writeFileSync(path.join(output,'SHA256SUMS'),digest+'  '+path.basename(archive)+'\n');
console.log(archive);
console.log(digest);
