// Run after building. Every suite uses fixtures; none launches/attaches to CS2.
import {execFileSync} from 'node:child_process';

for(const suite of ['test-config', 'test-runtime', 'test-menu', 'test-map-geometry',
                    'test-full-map-renderer', 'test-package-release']) {
    console.log('\nChecking '+suite+'...');
    execFileSync(process.execPath,['tools/'+suite+'.mjs'],{stdio:'inherit',timeout:600000});
}
console.log('\nAll product-quality regression suites passed.');
