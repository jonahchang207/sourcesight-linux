// Link an isolated harness to the exact objects from the last application build.
// Do not silently recompile a subset with different flags or version metadata.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';

export function buildHarness(name, libraries=[]) {
    const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
    const cwd=fs.mkdtempSync(path.join(os.tmpdir(),'sourcesight-'+name+'-'));
    let args=fs.readFileSync(path.join(root,'build/CMakeFiles/sourcesight.dir/link.txt'),'utf8').trim().split(/\s+/);
    args=args.filter(arg=>!arg.includes('dependency-file')&&!arg.endsWith('src/main.cpp.o'));
    const executable=path.join(cwd,name);
    args[args.indexOf('-o')+1]=executable;
    args.splice(1,0,path.join(root,'tools',name+'.cpp'),'-std=c++20','-include',path.join(root,'src/common.hpp'),
        ...['src','src/external','src/external/imgui','src/external/json/include',
            'src/external/AsyncLogger/src','src/external/AsyncLogger/include/AsyncLogger'].map(dir=>'-I'+path.join(root,dir)));
    args.push(...libraries);
    execFileSync(args[0],args.slice(1),{cwd:path.join(root,'build'),stdio:'inherit'});
    return {executable,cwd};
}
