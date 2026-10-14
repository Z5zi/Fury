#!/usr/bin/env python3
"""Verify and reuse an immutable ASan/UBSan engine cache for this exact study.

No writes occur in the cache. Every reused repository dependency is checked
against its frozen manifest and the current private tree. Shared system/header
paths and compiler identity are recorded. Mismatching units are rebuilt.
"""
import argparse,concurrent.futures,hashlib,json,os,re,shlex,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--hashes',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--plan-only',action='store_true');p.add_argument('--jobs',type=int,choices=[1,2],default=2);a=p.parse_args()
root=Path(__file__).resolve().parents[1];cache=a.cache.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
frozen=json.loads(a.hashes.read_text());commands=json.loads((cache/'compile_commands.json').read_text())
engine=[q for q in commands if '/engine/CMakeFiles/fury_engine.dir/' in q['output']]
original=Path(next(q['file'] for q in engine if q['file'].endswith('/engine/src/character_mira.cpp'))).parents[2]
compiler=Path(shlex.split(engine[0]['command'])[0]);version=subprocess.check_output([str(compiler),'--version'],text=True)
compiler_info=next((cache/'CMakeFiles').glob('*/CMakeCXXCompiler.cmake')).read_text();v=re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "(.*?)"',compiler_info).group(1)
assert v in version,'Compiler version differs from cache configuration'
ninja=Path('/tmp/fury-python-tools/bin/ninja');text=subprocess.check_output([str(ninja),'-C',str(cache),'-t','deps'],text=True)
deps={};key=None
for line in text.splitlines():
 if ': #deps ' in line:key=line.split(': #deps ',1)[0];deps[key]=[];assert '(VALID)' in line,line
 elif line.startswith('    ') and key:deps[key].append(Path(line.strip()))
inputs={}
for name in (root/'build/generated/source-files.txt').read_text().splitlines():inputs[name]=sha(root/name)
for name in ['tests/character_rook_tests.cpp','tests/character_rook_contact_tests.cpp','tests/character_rook_renderer_tests.cpp']:inputs[name]=sha(root/name)
archive=cache/'engine/libfury_engine.a';report={'cache':str(cache),'cache_manifest_sha256':sha(a.hashes),'cache_archive_sha256':sha(archive),'compiler':str(compiler),'compiler_sha256':sha(compiler),'compiler_version':version,'source_inputs_before':inputs,'reused':[],'recompiled':[],'shared_external_headers':{},'commands':[],'scope':'Entire selected engine archive is ASan/UBSan-instrumented; project dependency hashes matched or rebuilt. Shared external headers use the same absolute files in this executor; hashes recorded at reuse time, not claimed as historical external hashes.'}
by_name={Path(q['file']).name:q for q in engine}
cmake=(root/'engine/CMakeLists.txt').read_text().split('set(FURY_ENGINE_HEADERS',1)[0]
sources=[root/'engine'/n for n in re.findall(r'^\s+(src/[^\s]+\.cpp)\s*$',cmake,re.M)]
sources.insert(0,root/'build/generated/build_info.cpp')
rebuild=[]
for source in sources:
 old=by_name.get(source.name);reasons=[];checked={}
 if not old:reasons=['new translation unit']
 else:
  obj=Path(old['output']);relative=str(obj.relative_to(cache));assert obj.is_file(),obj
  d=deps.get(relative);assert d,relative
  for dependency in d:
   try:rel=str(dependency.relative_to(original))
   except ValueError:
    if dependency.is_relative_to(cache):reasons.append('generated cache input '+str(dependency));continue
    assert dependency.is_file(),dependency
    report['shared_external_headers'][str(dependency)]=sha(dependency);continue
   current=root/rel
   digest=sha(current) if current.is_file() else None
   checked[rel]=digest
   if digest!=frozen.get(rel):reasons.append('changed/unpinned '+rel)
  if source.name=='build_info.cpp':reasons.append('private exact-source fingerprint')
  if not reasons:report['reused'].append({'source':str(source.relative_to(root)),'object':str(obj),'object_sha256':sha(obj),'command':old['command'],'repository_dependencies':checked})
 if reasons:
  template=old or by_name['character_rina.cpp'];args=shlex.split(template['command']);args=[x.replace(str(original),str(root)) for x in args]
  oi=args.index('-o');args[oi+1]=str(out/(source.name+'.o'));ci=args.index('-c');args[ci+1]=str(source)
  # CMake's compilation database uses a shell-escaped version macro. Supply
  # the actual quoted token directly to the compiler via argv.
  args=[x.replace('\\"','"') for x in args]
  args += ['-MD','-MF',str(out/(source.name+'.o.d'))]
  rebuild.append((source,args));report['recompiled'].append({'source':str(source.relative_to(root)),'reasons':sorted(set(reasons)),'command':args})
(out/'reuse-plan.json').write_text(json.dumps(report,indent=2)+'\n')
print('Reuse',len(report['reused']),'units; rebuild',[(str(s.relative_to(root))) for s,_ in rebuild],flush=True)
if a.plan_only:raise SystemExit(0)
started=time.monotonic()
def compile_one(job):
 source,args=job;then=time.monotonic()
 with (out/(source.name+'.compile.log')).open('w') as log:result=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
 assert result.returncode==0,str(source)+' compile failed'
 return {'source':str(source.relative_to(root)),'object_sha256':sha(args[args.index('-o')+1]),'wall_seconds':time.monotonic()-then}
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:report['compile_results']=list(pool.map(compile_one,rebuild))
assert all(sha(root/n)==digest for n,digest in inputs.items()),'Sources changed during sanitizer construction'
# Explicit overrides precede the read-only archive. Link maps establish which
# archive members actually satisfy symbols and reject stale replacements.
overrides=[str(out/(s.name+'.o')) for s,_ in rebuild]
base_args=['/usr/bin/c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'engine/include'),'-isystem','/tmp/SDL2-2.32.10/include']
report['binaries']=[]
for name in ['character_rook_tests','character_rook_contact_tests','character_rook_renderer_tests']:
 obj=out/(name+'.o');cmd=base_args+['-MD','-MF',str(obj)+'.d','-c',str(root/'tests'/(name+'.cpp')),'-o',str(obj)]
 with (out/(name+'.compile.log')).open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0,name+' test compilation failed'
 binary=out/('fury_'+name);linkmap=out/(name+'.map')
 cmd=base_args+[str(obj),*overrides,str(archive),'/lib/x86_64-linux-gnu/libSDL2-2.0.so.0','-Wl,-Map,'+str(linkmap),'-o',str(binary)]
 with (out/(name+'.link.log')).open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0,name+' link failed'
 selected=re.findall(re.escape(str(archive))+r'\(([^)]+)\)',linkmap.read_text());selected=set(selected)
 for s,_ in rebuild:assert s.name+'.o' not in selected,'Stale archive member selected: '+s.name
 allowed={Path(q['object']).name for q in report['reused']}
 assert selected.issubset(allowed),'Unverified archive selection '+str(selected-allowed)
 report['binaries'].append({'path':str(binary),'sha256':sha(binary),'command':cmd,'link_map_sha256':sha(linkmap),'selected_verified_archive_members':sorted(selected)})
report['source_inputs_after']={n:sha(root/n) for n in inputs};assert report['source_inputs_after']==inputs
report['elapsed_seconds']=time.monotonic()-started;report['status']='built and source-verified; execution pending'
(out/'build-provenance.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'],flush=True)
