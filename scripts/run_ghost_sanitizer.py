#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,os,hashlib,time
root=Path(__file__).resolve().parents[1];out=root/'ghost-evidence/sanitizer';base={k:v for k,v in os.environ.items() if not k.startswith('FURY_')};base.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1');results=[]
for name,binary,args,code in [('model','character_ghost_tests',[],0),('ordinary-contact','character_ghost_contact_tests',['--ordinary'],0),('retained-strict-contact','character_ghost_contact_tests',[],1),('raster-and-path','character_ghost_renderer_tests',[],0),('actual-gl','character_ghost_renderer_tests',['--gl'],0),('real-socket-protocol','ghost_network_protocol_tests',[],0)]:
 env=base.copy()
 if name=='actual-gl':env['SDL_VIDEODRIVER']='offscreen'
 cmd=[str(out/binary),*args];log=out/(name+'.log');resources=out/(name+'.resources');started=time.monotonic()
 with log.open('w') as f:
  r=subprocess.Popen(cmd,env=env,stdout=f,stderr=subprocess.STDOUT);pid,status,usage=os.wait4(r.pid,0);r.returncode=os.waitstatus_to_exitcode(status)
 resources.write_text(json.dumps({'user_seconds':usage.ru_utime,'system_seconds':usage.ru_stime,'max_rss_kib':usage.ru_maxrss}))
 text=log.read_text();assert r.returncode==code,(name,r.returncode);assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text and 'AddressSanitizer:DEADLYSIGNAL' not in text,name
 if code:assert 'mode=5 phase=0' in text,name
 results.append({'name':name,'command':cmd,'exit_code':r.returncode,'expected_exit_code':code,'resource_time_user_system_maxrss':resources.read_text().strip(),'wall_seconds':time.monotonic()-started,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),'sanitizer_diagnostics':False,'strict_failure_retained':bool(code)});(out/'RESULTS.json').write_text(json.dumps({'source_fingerprint':json.loads((out/'BUILD_PROVENANCE.json').read_text())['source_fingerprint'],'fresh_compilation':True,'leak_detection':False,'LSan_note':'Not available under this executor ptrace arrangement; no leak-clean claim.','environment':{k:base[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','SDL_VIDEODRIVER','FURY_CPU_THREADS','LP_NUM_THREADS','LIBGL_ALWAYS_SOFTWARE']},'results':results},indent=2)+'\n');print(name,r.returncode,flush=True)
