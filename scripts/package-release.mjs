// Dirty builds require --allow-dirty and are labeled development artifacts.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

export function packageRelease({root=process.cwd(), output=path.join(root,'build','release'), allowDirty=false}={}) {
    const version=fs.readFileSync(path.join(root,'CMakeLists.txt'),'utf8')
        .match(/project\(sourcesight\s+VERSION\s+(\d+\.\d+\.\d+)/)?.[1];
    if(!version) throw new Error('Cannot read project version from CMakeLists.txt');
    const git=(...args)=>execFileSync('git',args,{cwd:root,encoding:'utf8'}).trim();
    const revision=git('rev-parse','HEAD');
    const dirty=git('status','--porcelain','--untracked-files=normal').length>0;
    if(dirty&&!allowDirty) throw new Error('Working tree is dirty. Review/commit it or use --allow-dirty for a labeled development artifact.');
    const artifactVersion=version+(dirty?'-dev.'+revision.slice(0,12)+'.dirty':'');
    const name='sourcesight-v'+artifactVersion+'-linux-x86_64';
    const binary=fs.readFileSync(path.join(root,'build','sourcesight'));
    if(binary.length<20 || binary.subarray(0,4).toString('hex')!=='7f454c46' ||
       binary[4]!==2 || binary[5]!==1 || binary.readUInt16LE(18)!==62)
        throw new Error('Expected a Linux x86-64 ELF binary in build/sourcesight');
    const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
    // Only public allowlisted files: no profiles, logs, credentials or map data.
    const files=[
        ['build/sourcesight','sourcesight'],
        ...['README.md','LICENSE','THIRD_PARTY_NOTICES.md','CHANGELOG.md','RELEASE_NOTES_v0.6.0.md',
            'docs/RELEASING.md','assets/brand/banner.svg','assets/brand/logo.svg',
            'assets/brand/menu.png'].map(file=>[file,file]),
        ['src/external/imgui/LICENSE.txt','licenses/imgui.txt'],
        ['src/external/json/LICENSE.MIT','licenses/json.txt'],
        ['src/external/AsyncLogger/LICENSE','licenses/AsyncLogger.txt'],
        ['src/external/VisCheckCS2/LICENSE','licenses/VisCheckCS2.txt'],
        ['src/external/curl/COPYING','licenses/curl.txt'],
        ['assets/README.md','licenses/asset-attribution.md']
    ];
    for(const [file] of files) if(!fs.statSync(path.join(root,file)).isFile())
        throw new Error('Missing packaging input: '+file);
    fs.mkdirSync(output,{recursive:true});
    const destination=path.join(output,name);
    fs.mkdirSync(destination); // EEXIST intentionally refuses to overwrite releases.
    const stage=fs.mkdtempSync(path.join(os.tmpdir(),'sourcesight-release-'));
    try {
        const pkg=path.join(stage,name);fs.mkdirSync(pkg);
        for(const [from,to] of files) {
            const dest=path.join(pkg,to);fs.mkdirSync(path.dirname(dest),{recursive:true});
            fs.copyFileSync(path.join(root,from),dest);
        }
        fs.chmodSync(path.join(pkg,'sourcesight'),0o755);
        const metadata={version,artifact_version:artifactVersion,source_revision:revision,
            source_dirty:dirty,binary_sha256:hash(binary),packaged_at:new Date().toISOString(),
            platform:'linux-x86_64',verification:'See test/CI results; packaging does not certify runtime correctness.'};
        fs.writeFileSync(path.join(pkg,'BUILD.json'),JSON.stringify(metadata,null,2)+'\n');
        fs.writeFileSync(path.join(pkg,'BUILD.txt'),
            'SourceSight '+artifactVersion+'\nSource revision: '+revision+'\nDirty source: '+dirty+'\n'+
            'Dynamically linked: requires compatible glibc/libstdc++, GLFW, curl, X11 and OpenGL.\n'+
            'Keep an earlier extracted version and backed-up profiles for rollback. See docs/RELEASING.md.\n');
        const archive=path.join(destination,name+'.tar.gz');
        execFileSync('tar',['-czf',archive,'-C',stage,name]);
        const digest=hash(fs.readFileSync(archive));
        fs.writeFileSync(path.join(destination,'SHA256SUMS'),digest+'  '+path.basename(archive)+'\n',{flag:'wx'});
        return {archive,digest,metadata};
    } finally {
        // Exclusively owned fresh mkdtemp directory, never a user-supplied target.
        fs.rmSync(stage,{recursive:true,force:true});
    }
}

if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
    const flags=process.argv.slice(2);
    if(flags.some(flag=>flag!=='--allow-dirty')) throw new Error('Usage: node scripts/package-release.mjs [--allow-dirty]');
    const result=packageRelease({allowDirty:flags.includes('--allow-dirty')});
    console.log(result.archive);console.log(result.digest);
}
