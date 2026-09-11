// Run after the coordinator's normal build: node tools/test-config.mjs
// The test runs in a temporary working directory and never touches user profiles.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';

const root = process.cwd();
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'sourcesight-config-'));
const link = fs.readFileSync('build/CMakeFiles/sourcesight.dir/link.txt', 'utf8').trim().split(/\s+/);
const output = link.indexOf('-o');
link[output + 1] = path.join(temp, 'check');
const appObjects = link.filter(value => !value.endsWith('src/main.cpp.o'));
appObjects.splice(1, 0, path.join(root, 'tools/test-config.cpp'), '-std=c++20', '-include', '../src/common.hpp',
    ...['src', 'src/external', 'src/external/imgui', 'src/external/json/include',
        'src/external/AsyncLogger/src', 'src/external/AsyncLogger/include/AsyncLogger'].map(value => '-I../' + value));
execFileSync(appObjects[0], appObjects.slice(1), {cwd: path.join(root, 'build'), stdio: 'inherit'});
execFileSync(path.join(temp, 'check'), [], {cwd: temp, stdio: 'inherit'});
