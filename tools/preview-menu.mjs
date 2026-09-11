// Run after building; uses the exact application objects and a temporary cwd.
// Add --headless --width 940 --height 600 --screenshot /tmp/menu.ppm for a framebuffer preview.
import {execFileSync} from 'node:child_process';
import {buildHarness} from './build-harness.mjs';
const {executable,cwd}=buildHarness('preview-menu',['-lEGL']);
execFileSync(executable,process.argv.slice(2), {cwd,stdio:'inherit'});
console.log('Harness artifacts: '+cwd);
