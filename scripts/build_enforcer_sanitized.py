#!/usr/bin/env python3
"""Verify immutable sanitizer cache dependencies and compile only differing inputs.

The sanitizer configuration exactly matches the approved Metro cache, including
its GL=1/DX12=0 defines. It is recorded separately from release configuration.
No source, dependency, object or build file in the cache is modified.
"""
import argparse,hashlib,json,os,re,shlex,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cache-root',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--audit-only',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[1];release=root/'build-enforcer';cache=a.cache_root.resolve();cache_build=cache/'build-metro-asan';out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
sha_cache={}
def sha(path):
 path=path.resolve()
 if path not in sha_cache:sha_cache[path]=hashlib.sha256(path.read_bytes()).hexdigest()
 return sha_cache[path]
def dependency_records(depfile):
 text=depfile.read_text().replace('\\\n',' ');names=shlex.split(text.split(':',1)[1]);return [{'path':str(Path(name).resolve()),'sha256':sha(Path(name))}for name in names]
def words(file,key):
 line=next(x for x in file.read_text().splitlines()if x.startswith(key+' = '));return shlex.split(line.split(' = ',1)[1])
flagfile=cache_build/'engine/CMakeFiles/fury_engine.dir/flags.make';defines=words(flagfile,'CXX_DEFINES');flags=words(flagfile,'CXX_FLAGS');includes=[s.replace(str(cache),str(root))for s in words(flagfile,'CXX_INCLUDES')]
assert '-fsanitize=address,undefined'in flags and '-O1'in flags and '-DFURY_HAS_OPENGL=1'in defines
compiler='/usr/bin/c++';version=subprocess.check_output([compiler,'--version'],text=True)
cache_commands=json.loads((cache_build/'compile_commands.json').read_text());release_commands=json.loads((release/'compile_commands.json').read_text())
engine=[c for c in release_commands if '/engine/CMakeFiles/fury_engine.dir/'in str(Path(c['directory'])/shlex.split(c['command'])[shlex.split(c['command']).index('-o')+1])]
records=[];objects=[];started=time.monotonic()
for item in engine:
 source=Path(item['file']).resolve();rel=source.relative_to(root)if source.is_relative_to(root)else Path('generated/build_info.cpp')
 generated=source==release/'generated/build_info.cpp';cached_source=cache/rel
 candidate=next((c for c in cache_commands if Path(c['file']).resolve()==cached_source),None)
 reasons=[];deps=[];object_file=None
 if generated:reasons.append('Always compile exact current build fingerprint')
 elif source.name in ['character_profile.cpp','character_enforcer.cpp']:reasons.append('Always compile private dispatcher/owned actor')
 elif candidate is None:reasons.append('Source absent from cache')
 if candidate is not None and not reasons:
  command=shlex.split(candidate['command']);object_file=(Path(candidate['directory'])/command[command.index('-o')+1]).resolve();depfile=Path(str(object_file)+'.d')
  expected=[compiler,*defines,*includes,*flags]
  actual=[x.replace(str(cache),str(root))for x in command[:command.index('-o')]]
  if actual!=expected:reasons.append('Compiler/options differ from exact desired sanitized configuration')
  if not object_file.is_file()or not depfile.is_file():reasons.append('Missing cached object/dependency record')
  else:
   text=depfile.read_text().replace('\\\n',' ');names=shlex.split(text.split(':',1)[1])
   for name in names:
    old=Path(name).resolve();new=root/old.relative_to(cache)if old.is_relative_to(cache)else old
    if not old.is_file()or not new.is_file():reasons.append('Missing dependency '+str(new));continue
    oldhash,newhash=sha(old),sha(new);deps.append({'cache_path':str(old),'target_path':str(new),'cache_sha256':oldhash,'target_sha256':newhash})
    if oldhash!=newhash:reasons.append('Dependency byte mismatch: '+str(new))
    if old.stat().st_mtime_ns>object_file.stat().st_mtime_ns:reasons.append('Cache dependency newer than object: '+str(old))
 if reasons:
  target=out/'objects'/((str(rel).replace('/','__'))+'.o');command=[compiler,*defines,*includes,*flags,'-MD','-MF',str(target)+'.d','-c',str(source),'-o',str(target)]
  if not a.audit_only:
   target.parent.mkdir(exist_ok=True);subprocess.run(command,cwd=root,check=True)
  objects.append(target);action='compiled'
 else:
  target=object_file;objects.append(target);command=shlex.split(candidate['command']);action='reused_read_only'
 records.append({'source':str(source),'source_sha256':sha(source),'action':action,'reasons':reasons,'object':str(target),'object_sha256':sha(target)if target.is_file()else None,'command':command,'dependencies':deps,'compiled_dependencies':dependency_records(Path(str(target)+'.d'))if reasons and not a.audit_only else []})
manifest={'cache_fingerprint':(cache_build/'generated/build_info.cpp').read_text(),'production_fingerprint_source':(release/'generated/build_info.cpp').read_text(),'compiler':compiler,'compiler_version':version,'defines':defines,'flags':flags,'configuration_note':'Exact approved cache ASan/UBSan configuration, GL=1/DX12=0; distinct from release GL=0 configure','objects':records,'audit_only':a.audit_only,'seconds':time.monotonic()-started}
(out/('audit.json'if a.audit_only else'object-manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
print('cache objects',sum(x['action']=='reused_read_only'for x in records),'compile objects',sum(x['action']=='compiled'for x in records),flush=True)
if a.audit_only:raise SystemExit(0)
test=root/'tests/character_enforcer_tests.cpp';test_object=out/'enforcer-tests.o';test_cmd=[compiler,*defines,*includes,*flags,'-I'+str(root/'tests'),'-MD','-MF',str(test_object)+'.d','-c',str(test),'-o',str(test_object)];subprocess.run(test_cmd,cwd=root,check=True)
link=[compiler,*flags,str(test_object),*[str(x)for x in objects],'/lib/x86_64-linux-gnu/libSDL2-2.0.so.0','-lpthread','-o',str(out/'enforcer-asan'),'-Wl,-Map,'+str(out/'link.map')];subprocess.run(link,cwd=root,check=True)
manifest['test']={'source':str(test),'source_sha256':sha(test),'object_sha256':sha(test_object),'command':test_cmd,'dependencies':dependency_records(Path(str(test_object)+'.d'))};manifest['link_command']=link;manifest['link_map_sha256']=sha(out/'link.map');manifest['binary_sha256']=sha(out/'enforcer-asan');manifest['seconds']=time.monotonic()-started
(out/'object-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('built',out/'enforcer-asan',flush=True)
