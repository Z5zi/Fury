#!/usr/bin/env python3
"""Link a private model-only baseline: identical current app/transport, only Ghost dispatch disabled."""
from pathlib import Path
import hashlib,json,shlex,subprocess,re,argparse
root=Path(__file__).resolve().parents[1];build=root/'build';parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,default=root/'ghost-evidence/model-only-baseline');args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
names=(build/'generated/source-files.txt').read_text().splitlines();hashes={n:sha(root/n) for n in names};payload=lambda table:''.join(f'{n}:{table[n]}\n' for n in names);final_fp=hashlib.sha256(payload(hashes).encode()).hexdigest();assert final_fp in (build/'generated/build_info.cpp').read_text()
source=root/'engine/src/character_profile.cpp';profile=source.read_text();needle='  if (identity == "GhostLoop" && role == CharacterRole::Ghost)\n    return make_ghost_character_profile(height, role, identity, lod);\n';assert profile.count(needle)==1;alternative=profile.replace(needle,'');alt_source=out/'character_profile.cpp';alt_source.write_text(alternative)
alt_hashes=dict(hashes);alt_hashes['engine/src/character_profile.cpp']=sha(alt_source);alt_fp=hashlib.sha256(payload(alt_hashes).encode()).hexdigest();info=out/'build_info.cpp';info.write_text('#include "fury/render_settings.hpp"\nnamespace fury { const char* build_source_fingerprint() { return "'+alt_fp+'"; } }\n')
commands=json.loads((build/'compile_commands.json').read_text());compiles=[]
for original,modified,name in [(source,alt_source,'profile'),(build/'generated/build_info.cpp',info,'build-info')]:
 record=next(c for c in commands if Path(c['file']).resolve()==original.resolve());cmd=shlex.split(record['command']);obj=out/(name+'.o');cmd[cmd.index('-o')+1]=str(obj);cmd[cmd.index('-c')+1]=str(modified);cmd+=['-MMD','-MF',str(out/(name+'.d'))];subprocess.run(cmd,cwd=record['directory'],check=True);compiles.append({'original_command':record['command'],'actual_command':cmd,'object':obj.name,'object_sha256':sha(obj)})
link_text=subprocess.check_output(['/tmp/fury-python-tools/bin/ninja','-C',str(build),'-t','commands','vaultline'],text=True).splitlines()[-1];tokens=shlex.split(link_text);start=next(i for i,t in enumerate(tokens) if t.endswith('/c++'));end=tokens.index('&&',start);link=tokens[start:end];link[link.index('-o')+1]=str(out/'vaultline');link=[('-Wl,--dependency-file='+str(out/'link.d')) if t.startswith('-Wl,--dependency-file=') else t for t in link];index=link.index('engine/libfury_engine.a');link[index:index]=[str(out/'profile.o'),str(out/'build-info.o')];link+=['-Wl,-Map='+str(out/'link.map')];subprocess.run(link,cwd=build,check=True)
# Record each private compiled unit's transitive dependency bytes. These objects
# were freshly built for the current private source; there is no external cache.
deps=subprocess.check_output(['/tmp/fury-python-tools/bin/ninja','-C',str(build),'-t','deps'],text=True);unit=None;dependencies={}
for line in deps.splitlines():
 if line and not line[0].isspace() and ': #' in line:
  unit=line.split(': #',1)[0];dependencies[unit]=[]
 elif line.startswith('    ') and unit:
  p=Path(line.strip());p=p if p.is_absolute() else build/p
  if p.is_file():dependencies[unit].append({'path':str(p),'sha256':sha(p)})
assert all(sha(root/n)==value for n,value in hashes.items()),'Runtime source changed during model-only link'
objects={t:sha(build/t) for t in link if t.endswith(('.o','.a')) and not Path(t).is_absolute()}
manifest={'kind':'current lifecycle/protocol with only GhostLoop profile dispatch disabled','final_source_fingerprint':final_fp,'baseline_source_fingerprint':alt_fp,'runtime_changed_path':'engine/src/character_profile.cpp','exact_removed_lines':needle,'compiler':subprocess.check_output(['/usr/bin/c++','--version'],text=True).splitlines()[0],'source_hashes_final':hashes,'source_hashes_baseline':alt_hashes,'compilations':compiles,'link_command':link,'link_inputs':objects,'private_object_transitive_dependencies':dependencies,'baseline_binary_sha256':sha(out/'vaultline'),'final_binary_sha256':sha(build/'apps/vaultline/vaultline'),'link_map_sha256':sha(out/'link.map'),'notes':'The alternative dispatch/build-info objects appear before the unchanged current engine archive; their matching archive members are not extracted. All other app and engine units remain the current private build. The distinct source fingerprint includes the alternative dispatch bytes, never the main-tree fingerprint.'};(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({k:manifest[k] for k in ['final_source_fingerprint','baseline_source_fingerprint','baseline_binary_sha256','final_binary_sha256']},indent=2))
