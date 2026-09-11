// Build first; requires EGL with a surfaceless OpenGL implementation (e.g. Mesa).
// Real framebuffer regression tests, without attaching to or starting CS2.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
const root = process.cwd();
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'sourcesight-full-map-'));
let args = fs.readFileSync('build/CMakeFiles/sourcesight.dir/link.txt', 'utf8').trim().split(/\s+/);
args = args.filter(x => !x.includes('dependency-file') && !x.endsWith('src/main.cpp.o'));
args[args.indexOf('-o') + 1] = path.join(temp, 'check');
args.splice(1, 0, path.join(root, 'tools/test-full-map-renderer.cpp'), '-std=c++20', '-include', '../src/common.hpp',
    ...['src', 'src/external', 'src/external/imgui', 'src/external/json/include',
        'src/external/AsyncLogger/src', 'src/external/AsyncLogger/include/AsyncLogger'].map(x => '-I../' + x));
args.push('-lEGL');
execFileSync(args[0], args.slice(1), {cwd: path.join(root, 'build'), stdio: 'inherit'});
execFileSync(path.join(temp, 'check'), [], {cwd: temp, stdio: 'inherit'});
