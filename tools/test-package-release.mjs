import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {packageRelease} from '../scripts/package-release.mjs';

const temp=fs.mkdtempSync(path.join(os.tmpdir(),'sourcesight-package-test-'));
try {
    const dirty=execFileSync('git',['status','--porcelain','--untracked-files=normal'],{encoding:'utf8'}).trim().length>0;
    if(dirty) assert.throws(()=>packageRelease({output:temp}),/dirty/);
    const result=packageRelease({output:temp,allowDirty:true});
    assert.equal(result.metadata.source_dirty,dirty);
    if(dirty) assert.match(path.basename(result.archive),/-dev\..*\.dirty-/);
    const names=execFileSync('tar',['-tzf',result.archive],{encoding:'utf8'}).trim().split('\n');
    assert(!names.some(name=>/(^|\/)(configs|logs|maps|\.env|\.git)(\/|$)/.test(name)));
    const member=names.find(name=>name.endsWith('/BUILD.json'));
    assert(member);
    const metadata=JSON.parse(execFileSync('tar',['-xOzf',result.archive,member],{encoding:'utf8'}));
    assert.equal(metadata.version,result.metadata.version);
    assert.equal(metadata.binary_sha256,crypto.createHash('sha256').update(fs.readFileSync('build/sourcesight')).digest('hex'));
    execFileSync('sha256sum',['-c','SHA256SUMS'],{cwd:path.dirname(result.archive),stdio:'inherit'});
    assert.throws(()=>packageRelease({output:temp,allowDirty:true}),{code:'EEXIST'});
    console.log('PASS: package identity, checksums, dirty labels, allowlist and overwrite protection');
} finally {
    // Only this test's freshly created temporary artifacts are removed.
    fs.rmSync(temp,{recursive:true,force:true});
}
