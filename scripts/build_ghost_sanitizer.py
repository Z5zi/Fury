#!/usr/bin/env python3
"""Fresh, private ASan/UBSan compilation; no cache member is reused."""
import concurrent.futures,hashlib,json,re,shlex,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1];build=root/'build';out=root/'ghost-evidence/sanitizer';out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
commands=json.loads((build/'compile_commands.json').read_text());engine=[c for c in commands if '/engine/CMakeFiles/fury_engine.dir/' in c['output']]
test_names=['character_ghost_tests','character_ghost_contact_tests','character_ghost_renderer_tests','ghost_network_protocol_tests']
tests=[next(c for c in commands if c['file'].endswith('/tests/'+n+'.cpp')) for n in test_names]
source_inputs={n:sha(root/n) for n in (build/'generated/source-files.txt').read_text().splitlines()};source_inputs.update({str(Path(c['file']).relative_to(root)):sha(c['file']) for c in tests})
report={'source_fingerprint':re.search(r'"([0-9a-f]{64})"',(build/'generated/build_info.cpp').read_text()).group(1),'source_inputs_before':source_inputs,'compiler_path':'/usr/bin/c++','compiler_sha256':sha('/usr/bin/c++'),'compiler_version':subprocess.check_output(['/usr/bin/c++','--version'],text=True),'reused_objects':[],'cache_rejection':'Immutable candidate has repository source hashes but no historical external-system/SDL dependency content hashes. Conservatively all selected units are freshly instrumented.','flags':['-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer'],'cpu_affinity':[3,4],'max_jobs':2,'compile_results':[],'status':'compiling'}
(out/'BUILD_PROVENANCE.json').write_text(json.dumps(report,indent=2)+'\n')
started=time.monotonic()
def compile_one(c):
 source=Path(c['file']);name=source.name;obj=out/(name+'.o');args=shlex.split(c['command']);args=[x.replace('\\"','"') for x in args if x not in ['-O3','-DNDEBUG','-g','-g1']];args[args.index('-o')+1]=str(obj);args+=report['flags']+['-MD','-MF',str(obj)+'.d'];t=time.monotonic()
 with (out/(name+'.compile.log')).open('w') as log:r=subprocess.run(args,cwd=c['directory'],stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0,name+' compile failed'
 text=Path(str(obj)+'.d').read_text().replace('\\\n',' ');paths=shlex.split(text.split(':',1)[1]);deps={}
 for v in paths:
  p=Path(v);p=p if p.is_absolute() else Path(c['directory'])/p;deps[str(p.resolve())]=sha(p)
 print(name,round(time.monotonic()-t,3),flush=True)
 return {'source':str(source),'object':str(obj),'object_sha256':sha(obj),'command':args,'wall_seconds':time.monotonic()-t,'transitive_dependency_hashes':deps}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 for result in pool.map(compile_one,engine+tests):
  report['compile_results'].append(result);(out/'BUILD_PROVENANCE.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(sha(root/n)==v for n,v in source_inputs.items())
for c in report['compile_results']:
 for p,h in c['transitive_dependency_hashes'].items():assert sha(p)==h,'Dependency changed during compilation: '+p
archive=out/'libfury_engine_asan.a';members=[str(out/(Path(c['file']).name+'.o')) for c in engine];subprocess.run(['/usr/bin/ar','rcs',str(archive),*members],check=True);report['archive_sha256']=sha(archive);report['archive_members']=members;report['binaries']=[]
for n in test_names:
 binary=out/n;linkmap=out/(n+'.map');cmd=['/usr/bin/c++',*report['flags'],str(out/(n+'.cpp.o')),str(archive),'/lib/x86_64-linux-gnu/libSDL2-2.0.so.0','-Wl,-Map='+str(linkmap),'-o',str(binary)]
 with (out/(n+'.link.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,n+' link failed'
 report['binaries'].append({'path':str(binary),'sha256':sha(binary),'link_command':cmd,'link_map_sha256':sha(linkmap),'selected_fresh_archive_members':sorted(set(re.findall(re.escape(str(archive))+r'\(([^)]+)\)',linkmap.read_text())))})
report['status']='fresh instrumented build complete; runtime execution pending';report['wall_seconds']=time.monotonic()-started;report['source_inputs_after']={n:sha(root/n) for n in source_inputs};assert report['source_inputs_after']==source_inputs;(out/'BUILD_PROVENANCE.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'],flush=True)
