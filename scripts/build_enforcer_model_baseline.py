#!/usr/bin/env python3
"""Build the model-only original Enforcer with the identical repaired game driver.

Only dispatcher selection differs. Compile that source plus an accurately
recomputed build fingerprint, then link the verified current game objects and
archive. No full build copy and no stale build-info/profile object selection.
"""
import argparse,hashlib,json,re,shlex,subprocess,difflib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];build=root/'build-enforcer';out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
source=root/'engine/src/character_profile.cpp';current=source.read_text();dispatch='''  if (identity == "NpcEnforcer" && role == CharacterRole::Enforcer)
    return make_enforcer_character_profile(height, role, identity, lod);
''';assert current.count(dispatch)==1
before=current.replace(dispatch,'');override=out/'character_profile.cpp';override.write_text(before)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
paths=(build/'generated/source-files.txt').read_text().splitlines();records=[];after_payload='';before_payload=''
for rel in paths:
 digest=sha(root/rel);prior=sha(override)if rel=='engine/src/character_profile.cpp'else digest
 after_payload+=f'{rel}:{digest}\n';before_payload+=f'{rel}:{prior}\n';records.append({'path':rel,'after_sha256':digest,'before_sha256':prior})
after_fp=hashlib.sha256(after_payload.encode()).hexdigest();before_fp=hashlib.sha256(before_payload.encode()).hexdigest()
assert after_fp in (build/'generated/build_info.cpp').read_text(),'Current binary build info is stale against live source'
info=out/'build_info.cpp';info.write_text('#include "fury/render_settings.hpp"\nnamespace fury { const char* build_source_fingerprint() { return "'+before_fp+'"; } }\n')
commands=json.loads((build/'compile_commands.json').read_text());compiled=[]
for original,replacement,name in [(source,override,'profile'),(build/'generated/build_info.cpp',info,'build-info')]:
 entry=next(x for x in commands if Path(x['file']).resolve()==original.resolve());args=shlex.split(entry['command']);target=out/(name+'.o')
 args[args.index('-o')+1]=str(target);args=[str(replacement)if Path(x).is_absolute()and Path(x).resolve()==original.resolve()else x for x in args]
 subprocess.run(args,cwd=entry['directory'],check=True);compiled.append({'original':str(original),'replacement':str(replacement),'replacement_sha256':sha(replacement),'object':str(target),'object_sha256':sha(target),'command':args})
work=build/'apps/vaultline';link=shlex.split((work/'CMakeFiles/vaultline.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(out/'vaultline-original-model');at=next(i for i,x in enumerate(link)if x.endswith('libfury_engine.a'));link[at:at]=[str(out/'profile.o'),str(out/'build-info.o')];link.append('-Wl,-Map,'+str(out/'link.map'))
subprocess.run(link,cwd=work,check=True)
link_map=(out/'link.map').read_text();assert 'libfury_engine.a(character_profile.cpp.o)'not in link_map and 'libfury_engine.a(build_info.cpp.o)'not in link_map,'Stale dispatcher or fingerprint selected from archive'
(out/'dispatch-only.patch').write_text(''.join(difflib.unified_diff(current.splitlines(True),before.splitlines(True),fromfile='after/engine/src/character_profile.cpp',tofile='before/engine/src/character_profile.cpp')))
(out/'manifest.json').write_text(json.dumps({'purpose':'Model-only art comparison; identical authorized production spawn repair and lifecycle driver on both sides','only_source_difference':'engine/src/character_profile.cpp Enforcer route omitted','after_fingerprint':after_fp,'before_fingerprint':before_fp,'files':records,'compiled_overrides':compiled,'link_command':link,'link_map_sha256':sha(out/'link.map'),'binary_sha256':sha(out/'vaultline-original-model')},indent=2)+'\n')
print('Built model-only original Enforcer',before_fp,'against shared after',after_fp)
