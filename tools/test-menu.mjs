// Run after building; uses the exact application objects and a temporary cwd.
import {execFileSync} from 'node:child_process';
import {buildHarness} from './build-harness.mjs';
const {executable,cwd}=buildHarness('test-menu');
execFileSync(executable,[], {cwd,stdio:'inherit'});
console.log('Harness artifacts: '+cwd);
