#!/usr/bin/env python3
"""Link a model-only baseline with current diagnostics and the frozen old profile.

Original baseline executables stay immutable. This does not copy or rebuild a
full tree: verified current application objects and engine archive are retained,
while only the original dispatcher and exact virtual-source fingerprint differ.
"""
import argparse,difflib,hashlib,json,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
base=json.loads((root/'baseline/source_manifest.json').read_text());name='engine/src/character_profile.cpp'
content=subprocess.check_output(['git','-C',str(root.parent/'fury'),'show',base['tree']+':'+name])
assert hashlib.sha256(content).hexdigest()==base['files'][name]
profile=out/'character_profile_original.cpp';profile.write_bytes(content)
inputs={n:sha(root/n) for n in (root/'build/generated/source-files.txt').read_text().splitlines()}
original_profile_hash=inputs[name];inputs[name]=sha(profile)
fingerprint=hashlib.sha256(''.join(n+':'+inputs[n]+'\n' for n in sorted(inputs)).encode()).hexdigest()
build_info=out/'build_info.cpp';build_info.write_text('#include "fury/render_settings.hpp"\nnamespace fury {const char* build_source_fingerprint(){return "'+fingerprint+'";}}\n')
objects=[];commands=[]
compile_database=json.loads((root/'build/compile_commands.json').read_text())
template=next(q for q in compile_database if q['file']==str(root/'engine/src/character_profile.cpp'))
for source in [profile,build_info]:
 obj=source.with_suffix('.o');cmd=shlex.split(template['command']);cmd=[q.replace('\\\"','\"') for q in cmd];cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj);subprocess.run(cmd,cwd=root,check=True);commands.append(cmd);objects.append(str(obj))
report={'kind':'matched-model-only-baseline','accepted_original_tree':base['tree'],'original_preserved_executables_manifest':str(root/'baseline/executables.sha256'),'source_sha256':fingerprint,'virtual_source_inputs':inputs,'only_production_source_override':name,'after_dispatcher_sha256':original_profile_hash,'original_dispatcher_sha256':sha(profile),'commands':commands,'binaries':[]}
for app,target in [('characterlab','fury_characterlab'),('vaultline','vaultline')]:
 cwd=root/'build/apps'/app;cmd=shlex.split((cwd/'CMakeFiles'/f'{target}.dir/link.txt').read_text());cmd=[q for q in cmd if not q.startswith('-Wl,--dependency-file=')]
 cmd[cmd.index('-o')+1]=str(out/target);archive=next(q for q in cmd if q.endswith('/libfury_engine.a'));index=cmd.index(archive);cmd[index:index]=objects
 linkmap=out/(target+'.map');cmd+=['-Wl,-Map,'+str(linkmap)];subprocess.run(cmd,cwd=cwd,check=True)
 text=linkmap.read_text();assert 'libfury_engine.a(character_profile.cpp.o)' not in text and 'libfury_engine.a(build_info.cpp.o)' not in text
 assert 'libfury_engine.a(character_rook.cpp.o)' not in text,'New actor leaked into model-only baseline'
 report['binaries'].append({'path':str(out/target),'sha256':sha(out/target),'link_command':cmd,'cwd':str(cwd),'link_map_sha256':sha(linkmap)})
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'fingerprint':fingerprint,'binaries':report['binaries']},indent=2))
